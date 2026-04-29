# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6199/8024 lines (77.26%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits |  Line | Source |
| -------: | ----: | :--- |
|        - |     1 | `/**` |
|        - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |     5 | ` */` |
|        - |     6 | `#include "ph7int.h"` |
|        - |     7 | `#include <stddef.h>` |
|        - |     8 | `#include <stdlib.h>` |
|        - |     9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |    10 | `#include <math.h>` |
|        - |    11 | `#endif` |
|        - |    12 | `/* Signed 64-bit multiplication with overflow detection. GCC/Clang expose` |
|        - |    13 | ` * __builtin_mul_overflow; MSVC does not, so we fall back to a portable` |
|        - |    14 | ` * UB-free bound-check implementation. Sets *pR to the wrapped product and` |
|        - |    15 | ` * returns non-zero on overflow. The fallback never divides a potentially-` |
|        - |    16 | ` * overflowed intermediate: all divisions are of compile-time constants` |
|        - |    17 | ` * (LARGEST_INT64/SMALLEST_INT64) by a factor already proven not to be -1,` |
|        - |    18 | ` * and the product itself is computed via unsigned multiplication to avoid` |
|        - |    19 | ` * signed-overflow UB. */` |
|        - |    20 | `#if defined(__GNUC__) \|\| defined(__clang__)` |
|        - |    21 | `#define VmMulOverflow64(a,b,pR) __builtin_mul_overflow((a),(b),(pR))` |
|        - |    22 | `#else` |
|        - |    23 | `static int VmMulOverflow64(sxi64 a, sxi64 b, sxi64 *pR)` |
|        1 |    24 |  |
|        1 |    25 | `	*pR = (sxi64)((sxu64)a * (sxu64)b);` |
|        - |    26 | `	/* Factors of 0 or ±1 never overflow; handle up front so the divisions` |
|        - |    27 | `	 * below are guaranteed safe (no SMALLEST_INT64 / -1, no /0). */` |
|        1 |    28 | `	if( a == 0 \|\| b == 0 \|\| a == 1 \|\| b == 1 ){` |
|        1 |    29 | `		return 0;` |
|        - |    30 | `	}` |
|        1 |    31 | `	if( a == -1 ){` |
|      ! 0 |    32 | `		return b == SMALLEST_INT64;` |
|        - |    33 | `	}` |
|        1 |    34 | `	if( b == -1 ){` |
|      ! 0 |    35 | `		return a == SMALLEST_INT64;` |
|        - |    36 | `	}` |
|        - |    37 | `	/* \|a\|,\|b\| >= 2 and neither is -1.  Bound check against the MAX/MIN` |
|        - |    38 | `	 * thresholds.  No division by -1 is possible here, and the quotients` |
|        - |    39 | `	 * of compile-time constants by {a,b} always fit in sxi64. */` |
|        1 |    40 | `	if( a > 0 ){` |
|        1 |    41 | `		if( b > 0 ){` |
|        1 |    42 | `			return a > LARGEST_INT64 / b;` |
|      ! 0 |    43 | `		}else{` |
|      ! 0 |    44 | `			return b < SMALLEST_INT64 / a;` |
|        - |    45 | `		}` |
|      ! 0 |    46 | `	}else{` |
|        1 |    47 | `		if( b > 0 ){` |
|        1 |    48 | `			return a < SMALLEST_INT64 / b;` |
|      ! 0 |    49 | `		}else{` |
|        1 |    50 | `			return b < LARGEST_INT64 / a;` |
|        - |    51 | `		}` |
|        - |    52 | `	}` |
|        1 |    53 |  |
|        - |    54 | `#endif` |
|        - |    55 | `/*` |
|        - |    56 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|        - |    57 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|        - |    58 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|        - |    59 | ` * statements.` |
|        - |    60 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|        - |    61 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|        - |    62 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|        - |    63 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|        - |    64 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|        - |    65 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|        - |    66 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|        - |    67 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|        - |    68 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|        - |    69 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|        - |    70 | ` * and so on.` |
|        - |    71 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|        - |    72 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|        - |    73 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|        - |    74 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|        - |    75 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|        - |    76 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|        - |    77 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|        - |    78 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|        - |    79 | ` */` |
|        - |    80 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|        - |    81 | `/*` |
|        - |    82 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|        - |    83 | ` * memory object index is stored in an instance of the following structure and put` |
|        - |    84 | ` * in the free object table so that it can be reused again without allocating` |
|        - |    85 | ` * a new memory object.` |
|        - |    86 | ` */` |
|        - |    87 | `typedef struct VmSlot VmSlot;` |
|        - |    88 | `struct VmSlot` |
|        - |    89 |  |
|        - |    90 | `	sxu32 nIdx;      /* Index in pVm->aMemObj[] */` |
|        - |    91 | `	void *pUserData; /* Upper-layer private data */` |
|        - |    92 | `};` |
|        - |    93 | `/*` |
|        - |    94 | ` * An entry in the reference table is represented by an instance of the` |
|        - |    95 | ` * follwoing table.` |
|        - |    96 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - |    97 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - |    98 | ` * the reference implementation is consistent,solid and it's` |
|        - |    99 | ` * behavior resemble the C++ reference mechanism.` |
|        - |   100 | ` * Refer to the official for more information on this powerful` |
|        - |   101 | ` * extension.` |
|        - |   102 | ` */` |
|        - |   103 | `struct VmRefObj` |
|        - |   104 |  |
|        - |   105 | `	SySet aReference;  /* Table of references to this memory object */` |
|        - |   106 | `	SySet aArrEntries; /* Foreign hashmap entries [i.e: array(&$a) ] */` |
|        - |   107 | `	sxu32 nIdx;        /* Referenced object index */` |
|        - |   108 | `	sxi32 iFlags;      /* Configuration flags */` |
|        - |   109 | `	VmRefObj *pNextCollide,*pPrevCollide; /* Collision link */` |
|        - |   110 | `	VmRefObj *pNext,*pPrev;               /* List of all referenced objects */` |
|        - |   111 | `};` |
|        - |   112 | `#define VM_REF_IDX_KEEP  0x001 /* Do not restore the memory object to the free list */` |
|        - |   113 | `/* VmObEntry struct moved to ph7int.h */` |
|        - |   114 | `/*` |
|        - |   115 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|        - |   116 | ` * is stored in an instance of the following structure.` |
|        - |   117 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|        - |   118 | ` */` |
|        - |   119 | `typedef struct VmShutdownCB VmShutdownCB;` |
|        - |   120 | `struct VmShutdownCB` |
|        - |   121 |  |
|        - |   122 | `	ph7_value sCallback; /* Shutdown callback */` |
|        - |   123 | `	ph7_value aArg[10];   /* Callback arguments (10 maximum arguments) */` |
|        - |   124 | `	int nArg;             /* Total number of given arguments */` |
|        - |   125 | `};` |
|        - |   126 | `/*` |
|        - |   127 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|        - |   128 | ` * is stored in an instance of the following structure.` |
|        - |   129 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|        - |   130 | ` */` |
|        - |   131 | `typedef struct VmAutoloadCB VmAutoloadCB;` |
|        - |   132 | `struct VmAutoloadCB` |
|        - |   133 |  |
|        - |   134 | `	ph7_value sCallback; /* Autoload callback (string or [obj,method] array) */` |
|        - |   135 | `};` |
|        - |   136 | `/* Uncaught exception code value */` |
|        - |   137 | `#define PH7_EXCEPTION -255` |
|        - |   138 |  |
|        - |   139 | `/*` |
|        - |   140 | ` * Return TRUE if either operand is a NaN real value.` |
|        - |   141 | ` */` |
|   897542 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   897544 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   897510 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   897500 |   150 | `	return FALSE;` |
|   448795 |   151 |  |
|        - |   152 | `/*` |
|        - |   153 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   154 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   155 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   156 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   157 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   158 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   159 | ` * still go through the existing numeric coercion.` |
|        - |   160 | ` */` |
|   333172 |   161 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   162 |  |
|        - |   163 | `	SyString sStr;` |
|   333174 |   164 | `	sxu8 bReal = FALSE;` |
|   333174 |   165 | `	const char *zTail = 0;` |
|        - |   166 | `	const char *zEnd;` |
|   333174 |   167 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   333116 |   168 | `		return FALSE;` |
|        - |   169 | `	}` |
|       59 |   170 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       59 |   171 | `	if( sStr.nByte == 0 ){` |
|        5 |   172 | `		return TRUE;` |
|        - |   173 | `	}` |
|       55 |   174 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       43 |   175 | `		return TRUE;` |
|        - |   176 | `	}` |
|        - |   177 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   178 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   179 | `	 * still takes the Perl path. */` |
|       13 |   180 | `	zEnd = sStr.zString + sStr.nByte;` |
|       17 |   181 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   182 | `		zTail++;` |
|        1 |   183 | `	}` |
|       13 |   184 | `	return zTail < zEnd;` |
|   166610 |   185 |  |
|        - |   186 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|        - |   187 | `/*` |
|        - |   188 | ` * Register a constant and it's associated expansion callback so that` |
|        - |   189 | ` * it can be expanded from the target PHP program.` |
|        - |   190 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|        - |   191 | ` * simple and work as follows:` |
|        - |   192 | ` * Each registered constant have a C procedure associated with it.` |
|        - |   193 | ` * This procedure known as the constant expansion callback is responsible` |
|        - |   194 | ` * of expanding the invoked constant to the desired value,for example:` |
|        - |   195 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|        - |   196 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|        - |   197 | ` * (Windows,Linux,...) and so on.` |
|        - |   198 | ` * Please refer to the official documentation for additional information.` |
|        - |   199 | ` */` |
|   581152 |   200 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|        - |   201 | `	ph7_vm *pVm,            /* Target VM */` |
|        - |   202 | `	const SyString *pName,  /* Constant name */` |
|        - |   203 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|        - |   204 | `	void *pUserData         /* Last argument to xExpand() */` |
|        - |   205 | `	)` |
|        2 |   206 |  |
|        - |   207 | `	ph7_constant *pCons;` |
|        - |   208 | `	SyHashEntry *pEntry;` |
|        - |   209 | `	char *zDupName;` |
|        - |   210 | `	sxi32 rc;` |
|   581154 |   211 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   581154 |   212 | `	if( pEntry ){` |
|        - |   213 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   214 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   215 | `		pCons->xExpand = xExpand;` |
|        6 |   216 | `		pCons->pUserData = pUserData;` |
|        6 |   217 | `		return SXRET_OK;` |
|        - |   218 | `	}` |
|        - |   219 | `	/* Allocate a new constant instance */` |
|   581150 |   220 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   581150 |   221 | `	if( pCons == 0 ){` |
|      ! 0 |   222 | `		return 0;` |
|        - |   223 | `	}` |
|        - |   224 | `	/* Duplicate constant name */` |
|   581150 |   225 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   581150 |   226 | `	if( zDupName == 0 ){` |
|      ! 0 |   227 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   228 | `		return 0;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Install the constant */` |
|   581150 |   231 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   581150 |   232 | `	pCons->xExpand = xExpand;` |
|   581150 |   233 | `	pCons->pUserData = pUserData;` |
|   581150 |   234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   581150 |   235 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   236 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return rc;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* All done,constant can be invoked from PHP code */` |
|   581150 |   241 | `	return SXRET_OK;` |
|   290578 |   242 |  |
|        - |   243 | `/*` |
|        - |   244 | ` * Allocate a new foreign function instance.` |
|        - |   245 | ` * This function return SXRET_OK on success. Any other` |
|        - |   246 | ` * return value indicates failure.` |
|        - |   247 | ` * Please refer to the official documentation for an introduction to` |
|        - |   248 | ` * the foreign function mechanism.` |
|        - |   249 | ` */` |
|  1291406 |   250 | `static sxi32 PH7_NewForeignFunction(` |
|        - |   251 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   252 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   253 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   254 | `	void *pUserData,          /* Foreign function private data */` |
|        - |   255 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|        - |   256 | `	)` |
|        2 |   257 |  |
|        - |   258 | `	ph7_user_func *pFunc;` |
|        - |   259 | `	char *zDup;` |
|        - |   260 | `	/* Allocate a new user function */` |
|  1291408 |   261 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1291408 |   262 | `	if( pFunc == 0 ){` |
|      ! 0 |   263 | `		return SXERR_MEM;` |
|        - |   264 | `	}` |
|        - |   265 | `	/* Duplicate function name */` |
|  1291408 |   266 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1291408 |   267 | `	if( zDup == 0 ){` |
|      ! 0 |   268 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   269 | `		return SXERR_MEM;` |
|        - |   270 | `	}` |
|        - |   271 | `	/* Zero the structure */` |
|  1291408 |   272 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   273 | `	/* Initialize structure fields */` |
|  1291408 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1291408 |   275 | `	pFunc->pVm   = pVm;` |
|  1291408 |   276 | `	pFunc->xFunc = xFunc;` |
|  1291408 |   277 | `	pFunc->pUserData = pUserData;` |
|  1291408 |   278 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   279 | `	/* Write a pointer to the new function */` |
|  1291408 |   280 | `	*ppOut = pFunc;` |
|  1291408 |   281 | `	return SXRET_OK;` |
|   645705 |   282 |  |
|        - |   283 | `/*` |
|        - |   284 | ` * Install a foreign function and it's associated callback so that` |
|        - |   285 | ` * it can be invoked from the target PHP code.` |
|        - |   286 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   287 | ` * return value indicates failure.` |
|        - |   288 | ` * Please refer to the official documentation for an introduction to` |
|        - |   289 | ` * the foreign function mechanism.` |
|        - |   290 | ` */` |
|  1294084 |   291 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|        - |   292 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   293 | `	const SyString *pName,    /* Foreign function name */` |
|        - |   294 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|        - |   295 | `	void *pUserData           /* Foreign function private data */` |
|        - |   296 | `	)` |
|        2 |   297 |  |
|        - |   298 | `	ph7_user_func *pFunc;` |
|        - |   299 | `	SyHashEntry *pEntry;` |
|        - |   300 | `	sxi32 rc;` |
|        - |   301 | `	/* Overwrite any previously registered function with the same name */` |
|  1294086 |   302 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1294086 |   303 | `	if( pEntry ){` |
|     2680 |   304 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2680 |   305 | `		pFunc->pUserData = pUserData;` |
|     2680 |   306 | `		pFunc->xFunc = xFunc;` |
|     2680 |   307 | `		SySetReset(&pFunc->aAux);` |
|     2680 |   308 | `		return SXRET_OK;` |
|        - |   309 | `	}` |
|        - |   310 | `	/* Create a new user function */` |
|  1291408 |   311 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1291408 |   312 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   313 | `		return rc;` |
|        - |   314 | `	}` |
|        - |   315 | `	/* Install the function in the corresponding hashtable */` |
|  1291408 |   316 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1291408 |   317 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   318 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   319 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   320 | `		return rc;` |
|        - |   321 | `	}` |
|        - |   322 | `	/* User function successfully installed */` |
|  1291408 |   323 | `	return SXRET_OK;` |
|   647044 |   324 |  |
|        - |   325 | `/*` |
|        - |   326 | ` * Initialize a VM function.` |
|        - |   327 | ` */` |
|   234966 |   328 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   329 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   330 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   331 | `	const char *zName,  /* Function name */` |
|        - |   332 | `	sxu32 nByte,        /* zName length */` |
|        - |   333 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   334 | `	void *pUserData     /* Function private data */` |
|        - |   335 | `	)` |
|        2 |   336 |  |
|        - |   337 | `	/* Zero the structure */` |
|   234968 |   338 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   339 | `	/* Initialize structure fields */` |
|        - |   340 | `	/* Arguments container */` |
|   234968 |   341 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   342 | `	/* Static variable container */` |
|   234968 |   343 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   344 | `	/* Bytecode container */` |
|   234968 |   345 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   346 | `    /* Preallocate some instruction slots */` |
|   234968 |   347 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   348 | `	/* Closure environment */` |
|   234968 |   349 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   350 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   234968 |   351 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   234968 |   352 | `	pFunc->iFlags = iFlags;` |
|   234968 |   353 | `	pFunc->pUserData = pUserData;` |
|        - |   354 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   355 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   234968 |   356 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   234968 |   357 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   234968 |   358 | `	return SXRET_OK;` |
|        2 |   359 |  |
|        - |   360 | `/*` |
|        - |   361 | ` * Namespace-aware function lookup.` |
|        - |   362 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   363 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   364 | ` */` |
|        - |   365 | `/*` |
|        - |   366 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   367 | ` */` |
|   721336 |   368 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   369 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   370 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   371 | `	SyString *pName     /* Function name */` |
|        - |   372 | `	)` |
|        2 |   373 |  |
|        - |   374 | `	SyHashEntry *pEntry;` |
|        - |   375 | `	sxi32 rc;` |
|   721338 |   376 | `	if( pName == 0 ){` |
|        - |   377 | `		/* Use the built-in name */` |
|    39858 |   378 | `		pName = &pFunc->sName;` |
|    19928 |   379 | `	}` |
|        - |   380 | `	/* Check for duplicates (functions with the same name) first */` |
|   721338 |   381 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   721338 |   382 | `	if( pEntry ){` |
|   534532 |   383 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   534532 |   384 | `		if( pLink != pFunc ){` |
|        - |   385 | `			/* Link */` |
|      188 |   386 | `			pFunc->pNextName = pLink;` |
|      188 |   387 | `			pEntry->pUserData = pFunc;` |
|       93 |   388 | `		}` |
|   534532 |   389 | `		return SXRET_OK;` |
|        - |   390 | `	}` |
|        - |   391 | `	/* First time seen */` |
|   186808 |   392 | `	pFunc->pNextName = 0;` |
|   186808 |   393 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   186808 |   394 | `	return rc;` |
|   360670 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   398 | ` */` |
|    54754 |   399 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   400 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   401 | `	ph7_class *pClass /* Target Class */` |
|        - |   402 | `	)` |
|        2 |   403 |  |
|    54756 |   404 | `	SyString *pName = &pClass->sName;` |
|        - |   405 | `	SyHashEntry *pEntry;` |
|        - |   406 | `	sxi32 rc;` |
|        - |   407 | `	/* Check for duplicates */` |
|    54756 |   408 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    54756 |   409 | `	if( pEntry ){` |
|       31 |   410 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   411 | `		/* Link entry with the same name */` |
|       31 |   412 | `		pClass->pNextName = pLink;` |
|       31 |   413 | `		pEntry->pUserData = pClass;` |
|       31 |   414 | `		return SXRET_OK;` |
|        - |   415 | `	}` |
|    54726 |   416 | `	pClass->pNextName = 0;` |
|        - |   417 | `	/* Perform a simple hashtable insertion */` |
|    54726 |   418 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    54726 |   419 | `	return rc;` |
|    27379 |   420 |  |
|        - |   421 | `/*` |
|        - |   422 | ` * Instruction builder interface.` |
|        - |   423 | ` */` |
|  4053278 |   424 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|        - |   425 | `	ph7_vm *pVm,  /* Target VM */` |
|        - |   426 | `	sxi32 iOp,    /* Operation to perform */` |
|        - |   427 | `	sxi32 iP1,    /* First operand */` |
|        - |   428 | `	sxu32 iP2,    /* Second operand */` |
|        - |   429 | `	void *p3,     /* Third operand */` |
|        - |   430 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|        - |   431 | `	)` |
|        2 |   432 |  |
|        - |   433 | `	VmInstr sInstr;` |
|        - |   434 | `	sxi32 rc;` |
|        - |   435 | `	/* Fill the VM instruction */` |
|  4053280 |   436 | `	sInstr.iOp = (sxu8)iOp;` |
|  4053280 |   437 | `	sInstr.iP1 = iP1;` |
|  4053280 |   438 | `	sInstr.iP2 = iP2;` |
|  4053280 |   439 | `	sInstr.p3  = p3;` |
|  4053280 |   440 | `	if( pIndex ){` |
|        - |   441 | `		/* Instruction index in the bytecode array */` |
|   220158 |   442 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   110078 |   443 | `	}` |
|        - |   444 | `	/* Finally,record the instruction */` |
|  4053280 |   445 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4053280 |   446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   447 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   448 | `		/* Fall throw */` |
|      ! 0 |   449 | `	}` |
|  4053280 |   450 | `	return rc;` |
|        2 |   451 |  |
|        - |   452 | `/*` |
|        - |   453 | ` * Swap the current bytecode container with the given one.` |
|        - |   454 | ` */` |
|   525960 |   455 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   456 |  |
|   525962 |   457 | `	if( pContainer == 0 ){` |
|        - |   458 | `		/* Point to the default container */` |
|      ! 0 |   459 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   460 | `	}else{` |
|        - |   461 | `		/* Change container */` |
|   525962 |   462 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   463 | `	}` |
|   525962 |   464 | `	return SXRET_OK;` |
|        2 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Return the current bytecode container.` |
|        - |   468 | ` */` |
|   262980 |   469 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   470 |  |
|   262982 |   471 | `	return pVm->pByteContainer;` |
|        2 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   475 | ` */` |
|   217088 |   476 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *pInstr;` |
|   217090 |   479 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   217090 |   480 | `	return pInstr;` |
|        2 |   481 |  |
|        - |   482 | `/*` |
|        - |   483 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   484 | ` */` |
|  1218076 |   485 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   486 |  |
|  1218078 |   487 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   488 |  |
|        - |   489 | `/*` |
|        - |   490 | ` * Pop the last VM instruction.` |
|        - |   491 | ` */` |
|   200728 |   492 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   493 |  |
|   200730 |   494 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   495 |  |
|        - |   496 | `/*` |
|        - |   497 | ` * Peek the last VM instruction.` |
|        - |   498 | ` */` |
|   798338 |   499 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   500 |  |
|   798340 |   501 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   502 |  |
|    31620 |   503 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   504 |  |
|        - |   505 | `	VmInstr *aInstr;` |
|        - |   506 | `	sxu32 n;` |
|    31622 |   507 | `	n = SySetUsed(pVm->pByteContainer);` |
|    31622 |   508 | `	if( n < 2 ){` |
|      ! 0 |   509 | `		return 0;` |
|        - |   510 | `	}` |
|    31622 |   511 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    31622 |   512 | `	return &aInstr[n - 2];` |
|    15812 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Allocate a new virtual machine frame.` |
|        - |   516 | ` */` |
|    21040 |   517 | `static VmFrame * VmNewFrame(` |
|        - |   518 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   519 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	)` |
|        2 |   522 |  |
|        - |   523 | `	VmFrame *pFrame;` |
|        - |   524 | `	/* Allocate a new vm frame */` |
|    21042 |   525 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    21042 |   526 | `	if( pFrame == 0 ){` |
|      ! 0 |   527 | `		return 0;` |
|        - |   528 | `	}` |
|        - |   529 | `	/* Zero the structure */` |
|    21042 |   530 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   531 | `	/* Initialize frame fields */` |
|    21042 |   532 | `	pFrame->pUserData = pUserData;` |
|    21042 |   533 | `	pFrame->pThis = pThis;` |
|    21042 |   534 | `	pFrame->pVm = pVm;` |
|    21042 |   535 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    21042 |   536 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    21042 |   537 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    21042 |   538 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    21042 |   539 | `	return pFrame;` |
|    10522 |   540 |  |
|        - |   541 | `/* Forward declaration */` |
|        - |   542 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   543 | `/*` |
|        - |   544 | ` * Enter a VM frame.` |
|        - |   545 | ` */` |
|    20994 |   546 | `static sxi32 VmEnterFrame(` |
|        - |   547 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   548 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   549 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   550 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   551 | `	)` |
|        2 |   552 |  |
|        - |   553 | `	VmFrame *pFrame;` |
|        - |   554 | `	/* Allocate a new frame */` |
|    20996 |   555 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    20996 |   556 | `	if( pFrame == 0 ){` |
|      ! 0 |   557 | `		return SXERR_MEM;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the list of active VM frame */` |
|    20996 |   560 | `	pFrame->pParent = pVm->pFrame;` |
|    20996 |   561 | `	pVm->pFrame = pFrame;` |
|    20996 |   562 | `	if( ppFrame ){` |
|        - |   563 | `		/* Write a pointer to the new VM frame */` |
|    18004 |   564 | `		*ppFrame = pFrame;` |
|     9001 |   565 | `	}` |
|    20996 |   566 | `	return SXRET_OK;` |
|    10499 |   567 |  |
|        - |   568 | `/*` |
|        - |   569 | ` * Link a foreign variable with the TOP most active frame.` |
|        - |   570 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|        - |   571 | ` * information.` |
|        - |   572 | ` */` |
|       58 |   573 | `static sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|        2 |   574 |  |
|        - |   575 | `	VmFrame *pTarget,*pFrame;` |
|       60 |   576 | `	SyHashEntry *pEntry = 0;` |
|        - |   577 | `	sxi32 rc;` |
|        - |   578 | `	/* Point to the upper frame */` |
|       60 |   579 | `	pFrame = pVm->pFrame;` |
|       60 |   580 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       60 |   581 | `	pTarget = pFrame;` |
|       60 |   582 | `	pFrame = pTarget->pParent;` |
|       60 |   583 | `	while( pFrame ){` |
|       60 |   584 | `		if( (pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   585 | `			/* Query the current frame */` |
|       60 |   586 | `			pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|       60 |   587 | `			if( pEntry ){` |
|        - |   588 | `				/* Variable found */` |
|       60 |   589 | `				break;` |
|        - |   590 | `			}` |
|      ! 0 |   591 | `		}` |
|        - |   592 | `		/* Point to the upper frame */` |
|      ! 0 |   593 | `		pFrame = pFrame->pParent;` |
|      ! 0 |   594 | `	}` |
|       60 |   595 | `	if( pEntry == 0 ){` |
|        - |   596 | `		/* Inexistant variable */` |
|      ! 0 |   597 | `		return SXERR_NOTFOUND;` |
|        - |   598 | `	}` |
|        - |   599 | `	/* Link to the current frame */` |
|       60 |   600 | `	rc = SyHashInsert(&pTarget->hVar,pEntry->pKey,pEntry->nKeyLen,pEntry->pUserData);` |
|       60 |   601 | `	if( rc == SXRET_OK ){` |
|        - |   602 | `		sxu32 nIdx;` |
|       60 |   603 | `		nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|       60 |   604 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pTarget->hVar),0,0);` |
|       29 |   605 | `	}` |
|       60 |   606 | `	return rc;` |
|       31 |   607 |  |
|        - |   608 | `/*` |
|        - |   609 | ` * Leave the top-most active frame.` |
|        - |   610 | ` */` |
|    17992 |   611 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   612 |  |
|    17994 |   613 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    17994 |   614 | `	if( pCurFrame ){` |
|        - |   615 | `		/* Unlink from the list of active VM frame */` |
|    17994 |   616 | `		pVm->pFrame = pCurFrame->pParent;` |
|    17994 |   617 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   618 | `			VmSlot  *aSlot;` |
|        - |   619 | `			sxu32 n;` |
|        - |   620 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    17694 |   621 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   118568 |   622 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   623 | `				/* Unset the local variable */` |
|   100876 |   624 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    50439 |   625 | `			}` |
|        - |   626 | `			/* Remove local reference */` |
|    17694 |   627 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   118630 |   628 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   100938 |   629 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    50470 |   630 | `			}` |
|     8846 |   631 | `		}` |
|        - |   632 | `		/* Release internal containers */` |
|    17994 |   633 | `		SyHashRelease(&pCurFrame->hVar);` |
|    17994 |   634 | `		SySetRelease(&pCurFrame->sArg);` |
|    17994 |   635 | `		SySetRelease(&pCurFrame->sLocal);` |
|    17994 |   636 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   637 | `		/* Release the whole structure */` |
|    17994 |   638 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     8996 |   639 | `	}` |
|    17994 |   640 |  |
|        - |   641 | `/*` |
|        - |   642 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   643 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   644 | ` * should be skipped when looking for the real execution context.` |
|        - |   645 | ` */` |
|  6962500 |   646 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   647 |  |
|  6964444 |   648 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     1944 |   649 | `		pFrame = pFrame->pParent;` |
|        2 |   650 | `	}` |
|  6962502 |   651 | `	return pFrame;` |
|        2 |   652 |  |
|        - |   653 | `/*` |
|        - |   654 | ` * Compare two functions signature and return the comparison result.` |
|        - |   655 | ` */` |
|      836 |   656 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|        1 |   657 |  |
|      837 |   658 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      837 |   659 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      837 |   660 | `	const char *zSin = pSecond->zString;` |
|      837 |   661 | `	const char *zFin = pFirst->zString;` |
|      837 |   662 | `	const char *zPtr = zFin;` |
|      421 |   663 | `	for(;;){` |
|      843 |   664 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|      413 |   665 | `			break;` |
|        - |   666 | `		}` |
|       19 |   667 | `		if( zFin[0] != zSin[0] ){` |
|        - |   668 | `			/* mismatch */` |
|       13 |   669 | `			break;` |
|        - |   670 | `		}` |
|        7 |   671 | `		zFin++;` |
|        7 |   672 | `		zSin++;` |
|        1 |   673 | `	}` |
|      837 |   674 | `	return (int)(zFin-zPtr);` |
|        1 |   675 |  |
|        - |   676 | `/*` |
|        - |   677 | ` * Select the appropriate VM function for the current call context.` |
|        - |   678 | ` * This is the implementation of the powerful 'function overloading' feature` |
|        - |   679 | ` * introduced by the version 2 of the PH7 engine.` |
|        - |   680 | ` * Refer to the official documentation for more information.` |
|        - |   681 | ` */` |
|      138 |   682 | `static ph7_vm_func * VmOverload(` |
|        - |   683 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |   684 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|        - |   685 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|        - |   686 | `	int nArg             /* Total number of passed arguments  */` |
|        - |   687 | `	)` |
|        2 |   688 |  |
|        - |   689 | `	int iTarget,i,j,iCur,iMax;` |
|        - |   690 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|        - |   691 | `	ph7_vm_func *pLink;` |
|        - |   692 | `	SyString sArgSig;` |
|        - |   693 | `	SyBlob sSig;` |
|        - |   694 |  |
|      140 |   695 | `	pLink = pList;` |
|      140 |   696 | `	i = 0;` |
|        - |   697 | `	/* Put functions expecting the same number of passed arguments */` |
|     1086 |   698 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|     1024 |   699 | `		if( pLink == 0 ){` |
|       78 |   700 | `			break;` |
|        - |   701 | `		}` |
|      948 |   702 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|        - |   703 | `			/* Candidate for overloading */` |
|      902 |   704 | `			apSet[i++] = pLink;` |
|      450 |   705 | `		}` |
|        - |   706 | `		/* Point to the next entry */` |
|      948 |   707 | `		pLink = pLink->pNextName;` |
|        2 |   708 | `	}` |
|      140 |   709 | `	if( i < 1 ){` |
|        - |   710 | `		/* No candidates,return the head of the list */` |
|      ! 0 |   711 | `		return pList;` |
|        - |   712 | `	}` |
|      140 |   713 | `	if( nArg < 1 \|\| i < 2 ){` |
|        - |   714 | `		/* Return the only candidate */` |
|       32 |   715 | `		return apSet[0];` |
|        - |   716 | `	}` |
|        - |   717 | `	/* Calculate function signature */` |
|      109 |   718 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|      367 |   719 | `	for( j = 0 ; j < nArg ; j++ ){` |
|      259 |   720 | `		int c = 'n'; /* null */` |
|      259 |   721 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|        - |   722 | `			/* Hashmap */` |
|       45 |   723 | `			c = 'h';` |
|      237 |   724 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|        - |   725 | `			/* bool */` |
|      ! 0 |   726 | `			c = 'b';` |
|      215 |   727 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|        - |   728 | `			/* int */` |
|        7 |   729 | `			c = 'i';` |
|      212 |   730 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|        - |   731 | `			/* String */` |
|      107 |   732 | `			c = 's';` |
|      156 |   733 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|        - |   734 | `			/* Float */` |
|      ! 0 |   735 | `			c = 'f';` |
|      103 |   736 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|        - |   737 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|        3 |   738 | `			int marker = 'o';` |
|        3 |   739 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|        3 |   740 | `			SyString *pName = &pClass->sName;` |
|        3 |   741 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|        3 |   742 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|        3 |   743 | `			c = -1;` |
|        1 |   744 | `		}` |
|      259 |   745 | `		if( c > 0 ){` |
|      257 |   746 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|      128 |   747 | `		}` |
|      130 |   748 | `	}` |
|      109 |   749 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|      109 |   750 | `	iTarget = 0;` |
|      109 |   751 | `	iMax = -1;` |
|        - |   752 | `	/* Select the appropriate function */` |
|      945 |   753 | `	for( j = 0 ; j < i ; j++ ){` |
|        - |   754 | `		/* Compare the two signatures */` |
|      837 |   755 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      837 |   756 | `		if( iCur > iMax ){` |
|      113 |   757 | `			iMax = iCur;` |
|      113 |   758 | `			iTarget = j;` |
|       56 |   759 | `		}` |
|      419 |   760 | `	}` |
|      109 |   761 | `	SyBlobRelease(&sSig);` |
|        - |   762 | `	/* Appropriate function for the current call context */` |
|      109 |   763 | `	return apSet[iTarget];` |
|       71 |   764 |  |
|        - |   765 | `/* Forward declaration */` |
|        - |   766 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|        - |   767 | `/*` |
|        - |   768 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|        - |   769 | ` * it can be instanciated from the executed PHP script.` |
|        - |   770 | ` */` |
|   159828 |   771 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   772 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   773 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   774 | `	)` |
|        2 |   775 |  |
|        - |   776 | `	ph7_class_method *pMeth;` |
|        - |   777 | `	ph7_class_attr *pAttr;` |
|        - |   778 | `	SyHashEntry *pEntry;` |
|        - |   779 | `	sxi32 rc;` |
|        - |   780 | `	/* Reset the loop cursor */` |
|   159830 |   781 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   782 | `	/* Process only static and constant attribute */` |
|   623180 |   783 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   784 | `		/* Extract the current attribute */` |
|   383438 |   785 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   383438 |   786 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   787 | `			ph7_value *pMemObj;` |
|        - |   788 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1776 |   789 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1776 |   790 | `			if( pMemObj == 0 ){` |
|      ! 0 |   791 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   792 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   793 | `					&pClass->sName,&pAttr->sName` |
|        - |   794 | `					);` |
|      ! 0 |   795 | `				return SXERR_MEM;` |
|        - |   796 | `			}` |
|     1776 |   797 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   798 | `				/* Initialize attribute default value (any complex expression) */` |
|     1772 |   799 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      885 |   800 | `			}` |
|        - |   801 | `			/* Record attribute index */` |
|     1776 |   802 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   803 | `			/* Install static attribute in the reference table */` |
|     1776 |   804 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   805 | `			/* If this is a typed static property, register the slot so the` |
|        - |   806 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   807 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   808 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1776 |   809 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|       10 |   810 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       10 |   811 | `				if( pVmAttrS == 0 ){` |
|      ! 0 |   812 | `					return SXERR_MEM;` |
|        - |   813 | `				}` |
|       10 |   814 | `				pVmAttrS->pAttr = pAttr;` |
|       10 |   815 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       10 |   816 | `				pVmAttrS->iState = 0;` |
|       10 |   817 | `				pVmAttrS->pOwner = pClass;` |
|        - |   818 | `				/* Static typed property with no default starts uninitialized */` |
|        8 |   819 | `				if( SySetUsed(&pAttr->aByteCode) == 0` |
|        8 |   820 | `				 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        6 |   821 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        2 |   822 | `				}` |
|       10 |   823 | `				if( SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttrS->nIdx,sizeof(sxu32),pVmAttrS) != SXRET_OK ){` |
|      ! 0 |   824 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|      ! 0 |   825 | `					return SXERR_MEM;` |
|        - |   826 | `				}` |
|        4 |   827 | `			}` |
|      887 |   828 | `		}` |
|        2 |   829 | `	}` |
|        - |   830 | `	/* Install class methods */` |
|   159830 |   831 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   832 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   833 | `		 */` |
|    79708 |   834 | `		return SXRET_OK;` |
|        - |   835 | `	}` |
|        - |   836 | `	/* Create constructor alias if not yet done */` |
|    80124 |   837 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   838 | `		/* User constructor with the same base class name */` |
|     6276 |   839 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6276 |   840 | `		if( pEntry ){` |
|      ! 0 |   841 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   842 | `			/* Create the alias */` |
|      ! 0 |   843 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   844 | `		}` |
|     3137 |   845 | `	}` |
|        - |   846 | `	/* Install the methods now */` |
|    80124 |   847 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   801673 |   848 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   681490 |   849 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   681490 |   850 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   681482 |   851 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   681482 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				return rc;` |
|        - |   854 | `			}` |
|   340740 |   855 | `		}` |
|        2 |   856 | `	}` |
|        - |   857 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    80124 |   858 | `	pClass->bMounted = TRUE;` |
|    80124 |   859 | `	return SXRET_OK;` |
|    79916 |   860 |  |
|        - |   861 | `/*` |
|        - |   862 | ` * Allocate a private frame for attributes of the given` |
|        - |   863 | ` * class instance (Object in the PHP jargon).` |
|        - |   864 | ` */` |
|     1882 |   865 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   866 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   867 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   868 | `	)` |
|        2 |   869 |  |
|     1884 |   870 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   871 | `	ph7_class_attr *pAttr;` |
|        - |   872 | `	SyHashEntry *pEntry;` |
|        - |   873 | `	sxi32 rc;` |
|        - |   874 | `	/* Install class attribute in the private frame associated with this instance */` |
|     1884 |   875 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     7836 |   876 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   877 | `		VmClassAttr *pVmAttr;` |
|        - |   878 | `		/* Extract the current attribute */` |
|     5954 |   879 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5954 |   880 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     5954 |   881 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   882 | `			return SXERR_MEM;` |
|        - |   883 | `		}` |
|     5954 |   884 | `		pVmAttr->pAttr = pAttr;` |
|     5954 |   885 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   886 | `			ph7_value *pMemObj;` |
|        - |   887 | `			/* Reserve a memory object for this attribute */` |
|     5930 |   888 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     5930 |   889 | `			if( pMemObj == 0 ){` |
|      ! 0 |   890 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   891 | `				return SXERR_MEM;` |
|        - |   892 | `			}` |
|     5930 |   893 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     5930 |   894 | `			pVmAttr->iState = 0;` |
|     5930 |   895 | `			pVmAttr->pOwner = pClass;` |
|     5930 |   896 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   897 | `				/* Initialize attribute default value (any complex expression) */` |
|     2024 |   898 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     4919 |   899 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   900 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   901 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       68 |   902 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       33 |   903 | `			}` |
|     5930 |   904 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     5930 |   905 | `			if( rc != SXRET_OK ){` |
|        - |   906 | `				VmSlot sSlot;` |
|        - |   907 | `				/* Restore memory object */` |
|      ! 0 |   908 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   909 | `				sSlot.pUserData = 0;` |
|      ! 0 |   910 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   911 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   912 | `				return SXERR_MEM;` |
|        - |   913 | `			}` |
|        - |   914 | `			/* Install attribute in the reference table */` |
|     5930 |   915 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   916 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   917 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   918 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     5930 |   919 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      162 |   920 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      162 |   921 | `				if( rc != SXRET_OK ){` |
|        - |   922 | `					VmSlot sSlot;` |
|      ! 0 |   923 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   924 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   925 | `					sSlot.pUserData = 0;` |
|      ! 0 |   926 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   927 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   928 | `					return SXERR_MEM;` |
|        - |   929 | `				}` |
|       80 |   930 | `			}` |
|     2966 |   931 | `		}else{` |
|        - |   932 | `			/* Install static/constant attribute */` |
|       26 |   933 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       26 |   934 | `			pVmAttr->iState = 0;` |
|       26 |   935 | `			pVmAttr->pOwner = pClass;` |
|       26 |   936 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       26 |   937 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   938 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   939 | `				return SXERR_MEM;` |
|        - |   940 | `			}` |
|        - |   941 | `		}` |
|        2 |   942 | `	}` |
|     1884 |   943 | `	return SXRET_OK;` |
|      943 |   944 |  |
|        - |   945 | `/* Forward declaration */` |
|        - |   946 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx);` |
|        - |   947 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef);` |
|        - |   948 | `/*` |
|        - |   949 | ` * Dummy read-only buffer used for slot reservation.` |
|        - |   950 | ` */` |
|        - |   951 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|        - |   952 | `/*` |
|        - |   953 | ` * Reserve a constant memory object.` |
|        - |   954 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   955 | ` */` |
|   433290 |   956 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   957 |  |
|        - |   958 | `	ph7_value *pObj;` |
|        - |   959 | `	sxi32 rc;` |
|   433292 |   960 | `	if( pIndex ){` |
|        - |   961 | `		/* Object index in the object table */` |
|   424316 |   962 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   212157 |   963 | `	}` |
|        - |   964 | `	/* Reserve a slot for the new object */` |
|   433292 |   965 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   433292 |   966 | `	if( rc != SXRET_OK ){` |
|        - |   967 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   968 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   969 | `		 */` |
|      ! 0 |   970 | `		return 0;` |
|        - |   971 | `	}` |
|   433292 |   972 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   433292 |   973 | `	return pObj;` |
|   216647 |   974 |  |
|        - |   975 | `/*` |
|        - |   976 | ` * Reserve a memory object.` |
|        - |   977 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   978 | ` */` |
|  2149316 |   979 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   980 |  |
|        - |   981 | `	ph7_value *pObj;` |
|        - |   982 | `	sxi32 rc;` |
|  2149318 |   983 | `	if( pIndex ){` |
|        - |   984 | `		/* Object index in the object table */` |
|  2149318 |   985 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1074658 |   986 | `	}` |
|        - |   987 | `	/* Reserve a slot for the new object */` |
|  2149318 |   988 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2149318 |   989 | `	if( rc != SXRET_OK ){` |
|        - |   990 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   991 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   992 | `		 */` |
|      ! 0 |   993 | `		return 0;` |
|        - |   994 | `	}` |
|  2149318 |   995 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2149318 |   996 | `	return pObj;` |
|  1074660 |   997 |  |
|        - |   998 | `/* Forward declaration */` |
|        - |   999 | `static sxi32 VmEvalChunk(ph7_vm *pVm,ph7_context *pCtx,SyString *pChunk,int iFlags,int bTrueReturn);` |
|        - |  1000 | `/* Forward declarations for Fiber C functions */` |
|        - |  1001 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1002 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1003 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1004 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1005 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1006 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1007 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1008 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1009 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1010 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1011 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|        - |  1012 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc);` |
|        - |  1013 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1014 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  1015 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg);` |
|        - |  1016 | `static sxi32 VmCallClassMethodWithMap(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1017 | `	ph7_class_method *pMethod, ph7_value *pResult, int nArg,` |
|        - |  1018 | `	ph7_value **apArg, VmCallArgMap *pMap);` |
|        - |  1019 | `static sxi32 VmCallObjectInvoke(ph7_vm *pVm, ph7_class_instance *pThis,` |
|        - |  1020 | `	int nArg, ph7_value **apArg, ph7_value *pResult, VmCallArgMap *pMap);` |
|        - |  1021 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis);` |
|        - |  1022 | `/* Forward declarations for Generator helpers and C functions */` |
|        - |  1023 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx);` |
|        - |  1024 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen);` |
|        - |  1025 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1026 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1027 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1028 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1029 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1030 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1031 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1032 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1033 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg);` |
|        - |  1034 | `/*` |
|        - |  1035 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|        - |  1036 | ` * directly as foreign functions.` |
|        - |  1037 | ` */` |
|        - |  1038 | `#define PH7_BUILTIN_LIB \` |
|        - |  1039 | `	"interface Throwable {"\` |
|        - |  1040 | `	"public function getMessage();"\` |
|        - |  1041 | `	"public function getCode();"\` |
|        - |  1042 | `	"public function getFile();"\` |
|        - |  1043 | `	"public function getLine();"\` |
|        - |  1044 | `	"public function getTrace();"\` |
|        - |  1045 | `	"public function getTraceAsString();"\` |
|        - |  1046 | `	"public function getPrevious();"\` |
|        - |  1047 | `	"public function __toString();"\` |
|        - |  1048 | `	"}"\` |
|        - |  1049 | `	"class Exception implements Throwable { "\` |
|        - |  1050 | `    "protected $message = '';"\` |
|        - |  1051 | `    "protected $code = 0;"\` |
|        - |  1052 | `    "protected $file;"\` |
|        - |  1053 | `    "protected $line;"\` |
|        - |  1054 | `    "protected $trace;"\` |
|        - |  1055 | `    "protected $previous;"\` |
|        - |  1056 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1057 | `	"   if( isset($message) ){"\` |
|        - |  1058 | `	"	  $this->message = $message;"\` |
|        - |  1059 | `	"   }"\` |
|        - |  1060 | `	"   $this->code = $code;"\` |
|        - |  1061 | `	"   $this->file = __FILE__;"\` |
|        - |  1062 | `	"   $this->line = __LINE__;"\` |
|        - |  1063 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1064 | `	"   if( isset($previous) ){"\` |
|        - |  1065 | `	"     $this->previous = $previous;"\` |
|        - |  1066 | `	"   }"\` |
|        - |  1067 | `	"}"\` |
|        - |  1068 | `	"public function getMessage(){"\` |
|        - |  1069 | `	"   return $this->message;"\` |
|        - |  1070 | `	"}"\` |
|        - |  1071 | `	" public function getCode(){"\` |
|        - |  1072 | `	"  return $this->code;"\` |
|        - |  1073 | `	"}"\` |
|        - |  1074 | `	"public function getFile(){"\` |
|        - |  1075 | `	"  return $this->file;"\` |
|        - |  1076 | `	"}"\` |
|        - |  1077 | `	"public function getLine(){"\` |
|        - |  1078 | `	"  return $this->line;"\` |
|        - |  1079 | `	"}"\` |
|        - |  1080 | `	"public function getTrace(){"\` |
|        - |  1081 | `	"   return $this->trace;"\` |
|        - |  1082 | `	"}"\` |
|        - |  1083 | `	"public function getTraceAsString(){"\` |
|        - |  1084 | `	"  return debug_string_backtrace();"\` |
|        - |  1085 | `	"}"\` |
|        - |  1086 | `	"public function getPrevious(){"\` |
|        - |  1087 | `	"    return $this->previous;"\` |
|        - |  1088 | `	"}"\` |
|        - |  1089 | `	"public function __toString(){"\` |
|        - |  1090 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1091 | `    "}"\` |
|        - |  1092 | `	"}"\` |
|        - |  1093 | `	"class Error implements Throwable { "\` |
|        - |  1094 | `    "protected $message = '';"\` |
|        - |  1095 | `    "protected $code = 0;"\` |
|        - |  1096 | `    "protected $file;"\` |
|        - |  1097 | `    "protected $line;"\` |
|        - |  1098 | `    "protected $trace;"\` |
|        - |  1099 | `    "protected $previous;"\` |
|        - |  1100 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1101 | `	"   if( isset($message) ){"\` |
|        - |  1102 | `	"	  $this->message = $message;"\` |
|        - |  1103 | `	"   }"\` |
|        - |  1104 | `	"   $this->code = $code;"\` |
|        - |  1105 | `	"   $this->file = __FILE__;"\` |
|        - |  1106 | `	"   $this->line = __LINE__;"\` |
|        - |  1107 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1108 | `	"   if( isset($previous) ){"\` |
|        - |  1109 | `	"     $this->previous = $previous;"\` |
|        - |  1110 | `	"   }"\` |
|        - |  1111 | `	"}"\` |
|        - |  1112 | `	"public function getMessage(){"\` |
|        - |  1113 | `	"   return $this->message;"\` |
|        - |  1114 | `	"}"\` |
|        - |  1115 | `	"public function getCode(){"\` |
|        - |  1116 | `	"  return $this->code;"\` |
|        - |  1117 | `	"}"\` |
|        - |  1118 | `	"public function getFile(){"\` |
|        - |  1119 | `	"  return $this->file;"\` |
|        - |  1120 | `	"}"\` |
|        - |  1121 | `	"public function getLine(){"\` |
|        - |  1122 | `	"  return $this->line;"\` |
|        - |  1123 | `	"}"\` |
|        - |  1124 | `	"public function getTrace(){"\` |
|        - |  1125 | `	"   return $this->trace;"\` |
|        - |  1126 | `	"}"\` |
|        - |  1127 | `	"public function getTraceAsString(){"\` |
|        - |  1128 | `	"  return debug_string_backtrace();"\` |
|        - |  1129 | `	"}"\` |
|        - |  1130 | `	"public function getPrevious(){"\` |
|        - |  1131 | `	"    return $this->previous;"\` |
|        - |  1132 | `	"}"\` |
|        - |  1133 | `	"public function __toString(){"\` |
|        - |  1134 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1135 | `	"}"\` |
|        - |  1136 | `	"}"\` |
|        - |  1137 | `	"class TypeError extends Error { }"\` |
|        - |  1138 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1139 | `	"class ValueError extends Error { }"\` |
|        - |  1140 | `	"class FiberError extends Error { }"\` |
|        - |  1141 | `	"class AssertionError extends Error { }"\` |
|        - |  1142 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1143 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1144 | `	"class ErrorException extends Exception { "\` |
|        - |  1145 | `	"protected $severity;"\` |
|        - |  1146 | `	"public function __construct(string $message = null,"\` |
|        - |  1147 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1148 | `	"   if( isset($message) ){"\` |
|        - |  1149 | `	"	  $this->message = $message;"\` |
|        - |  1150 | `	"   }"\` |
|        - |  1151 | `	"   $this->severity = $severity;"\` |
|        - |  1152 | `	"   $this->code = $code;"\` |
|        - |  1153 | `	"   $this->file = $filename;"\` |
|        - |  1154 | `	"   $this->line = $lineno;"\` |
|        - |  1155 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1156 | `	"   if( isset($previous) ){"\` |
|        - |  1157 | `	"     $this->previous = $previous;"\` |
|        - |  1158 | `	"   }"\` |
|        - |  1159 | `	"}"\` |
|        - |  1160 | `	"public function getSeverity(){"\` |
|        - |  1161 | `	"   return $this->severity;"\` |
|        - |  1162 | `    "}"\` |
|        - |  1163 | `	"}"\` |
|        - |  1164 | `	"interface Iterator {"\` |
|        - |  1165 | `	"public function current();"\` |
|        - |  1166 | `	"public function key();"\` |
|        - |  1167 | `	"public function next();"\` |
|        - |  1168 | `	"public function rewind();"\` |
|        - |  1169 | `	"public function valid();"\` |
|        - |  1170 | `	"}"\` |
|        - |  1171 | `	"interface IteratorAggregate {"\` |
|        - |  1172 | `	"public function getIterator();"\` |
|        - |  1173 | `	"}"\` |
|        - |  1174 | `	"interface Serializable {"\` |
|        - |  1175 | `	"public function serialize();"\` |
|        - |  1176 | `	"public function unserialize(string $serialized);"\` |
|        - |  1177 | `	"}"\` |
|        - |  1178 | `	"/* Directory releated IO */"\` |
|        - |  1179 | `	"class Directory {"\` |
|        - |  1180 | `	"public $handle = null;"\` |
|        - |  1181 | `	"public $path  = null;"\` |
|        - |  1182 | `	"public function __construct(string $path)"\` |
|        - |  1183 | `	"{"\` |
|        - |  1184 | `	"   $this->handle = opendir($path);"\` |
|        - |  1185 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1186 | `	"      $this->path = $path;"\` |
|        - |  1187 | `	"   }"\` |
|        - |  1188 | `	"}"\` |
|        - |  1189 | `	"public function __destruct()"\` |
|        - |  1190 | `	"{"\` |
|        - |  1191 | `	"  if( $this->handle != null ){"\` |
|        - |  1192 | `	"       closedir($this->handle);"\` |
|        - |  1193 | `	"  }"\` |
|        - |  1194 | `	"}"\` |
|        - |  1195 | `	"public function read()"\` |
|        - |  1196 | `	"{"\` |
|        - |  1197 | `	"    return readdir($this->handle);"\` |
|        - |  1198 | `	"}"\` |
|        - |  1199 | `	"public function rewind()"\` |
|        - |  1200 | `	"{"\` |
|        - |  1201 | `	"    rewinddir($this->handle);"\` |
|        - |  1202 | `	"}"\` |
|        - |  1203 | `	"public function close()"\` |
|        - |  1204 | `	"{"\` |
|        - |  1205 | `	"    closedir($this->handle);"\` |
|        - |  1206 | `	"    $this->handle = null;"\` |
|        - |  1207 | `	"}"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"class Fiber {"\` |
|        - |  1210 | `	"  private $__ctx;"\` |
|        - |  1211 | `	"  private $__callable;"\` |
|        - |  1212 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1213 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1214 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1215 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1216 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1217 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1218 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1219 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1220 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1221 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1222 | `	"}"\` |
|        - |  1223 | `	"class Generator implements Iterator {"\` |
|        - |  1224 | `	"  private $__ctx;"\` |
|        - |  1225 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1226 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1227 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1228 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1229 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1230 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1231 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1232 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1233 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1234 | `	"}"\` |
|        - |  1235 | `	"class stdClass{"\` |
|        - |  1236 | `	"  public $value;"\` |
|        - |  1237 | `	" /* Magic methods */"\` |
|        - |  1238 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1239 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1240 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1241 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1242 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1243 | `	"}"\` |
|        - |  1244 | `	"function dir(string $path){"\` |
|        - |  1245 | `	"   return new Directory($path);"\` |
|        - |  1246 | `	"}"\` |
|        - |  1247 | `	"function Dir(string $path){"\` |
|        - |  1248 | `	"   return new Directory($path);"\` |
|        - |  1249 | `	"}"\` |
|        - |  1250 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1251 | `    "{"\` |
|        - |  1252 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1253 | `	"  $aDir = array();"\` |
|        - |  1254 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1255 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1256 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1257 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1258 | `	"   }"\` |
|        - |  1259 | `	"  closedir($pHandle);"\` |
|        - |  1260 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1261 | `	"      rsort($aDir);"\` |
|        - |  1262 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1263 | `	"      sort($aDir);"\` |
|        - |  1264 | `	"  }"\` |
|        - |  1265 | `	"  return $aDir;"\` |
|        - |  1266 | `	"}"\` |
|        - |  1267 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1268 | `	"/* Open the target directory */"\` |
|        - |  1269 | `	"$zDir = dirname($pattern);"\` |
|        - |  1270 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1271 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1272 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1273 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1274 | `	"	return FALSE;"\` |
|        - |  1275 | `	"}"\` |
|        - |  1276 | `	"$pattern = basename($pattern);"\` |
|        - |  1277 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1278 | `	"/* Loop throw available entries */"\` |
|        - |  1279 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1280 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1281 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1282 | `	"	if( $rc ){"\` |
|        - |  1283 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1284 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1285 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1286 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1287 | `	"		  }"\` |
|        - |  1288 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1289 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1290 | `	"		 continue;"\` |
|        - |  1291 | `	"	   }"\` |
|        - |  1292 | `	"	   /* Add the entry */"\` |
|        - |  1293 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1294 | `	"	}"\` |
|        - |  1295 | `	" }"\` |
|        - |  1296 | `	"/* Close the handle */"\` |
|        - |  1297 | `	"closedir($pHandle);"\` |
|        - |  1298 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1299 | `	"  /* Sort the array */"\` |
|        - |  1300 | `	"  sort($pArray);"\` |
|        - |  1301 | `	"}"\` |
|        - |  1302 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1303 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1304 | `	"  $pArray[] = $pattern;"\` |
|        - |  1305 | `	"}"\` |
|        - |  1306 | `	"/* Return the created array */"\` |
|        - |  1307 | `	"return $pArray;"\` |
|        - |  1308 | `   "}"\` |
|        - |  1309 | `   "/* Creates a temporary file */"\` |
|        - |  1310 | `   "function tmpfile(){"\` |
|        - |  1311 | `   "  /* Extract the temp directory */"\` |
|        - |  1312 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1313 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1314 | `   "    /* Use the current dir */"\` |
|        - |  1315 | `   "    $zTempDir = '.';"\` |
|        - |  1316 | `   "  }"\` |
|        - |  1317 | `   "  /* Create the file */"\` |
|        - |  1318 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1319 | `   "  return $pHandle;"\` |
|        - |  1320 | `   "}"\` |
|        - |  1321 | `   "/* Creates a temporary filename */"\` |
|        - |  1322 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1323 | `   "{"\` |
|        - |  1324 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1325 | `   "}"\` |
|        - |  1326 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1327 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1328 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1329 | `   "/* Copy arguments */"\` |
|        - |  1330 | `   "$nArgs = func_num_args();"\` |
|        - |  1331 | `   "$pNew = array();"\` |
|        - |  1332 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1333 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1334 | `    "}"\` |
|        - |  1335 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1336 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1337 | `	"/* Erase */"\` |
|        - |  1338 | `	"array_erase($pArray);"\` |
|        - |  1339 | `	"/* Unshift */"\` |
|        - |  1340 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1341 | `	"return sizeof($pArray);"\` |
|        - |  1342 | `    "}"\` |
|        - |  1343 | `	"function array_merge_recursive(){"\` |
|        - |  1344 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1345 | `    "$arrays = func_get_args();"\` |
|        - |  1346 | `    "$narrays = count($arrays);"\` |
|        - |  1347 | `    "$ret = array();"\` |
|        - |  1348 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1349 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1350 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1351 | `	 " }"\` |
|        - |  1352 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1353 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1354 | `     "  if( $keyIsInt ) {"\` |
|        - |  1355 | `     "   $ret[] = $value;"\` |
|        - |  1356 | `     "  } else {"\` |
|        - |  1357 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1358 | `     "    $cur = $ret[$key];"\` |
|        - |  1359 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1360 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1361 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1362 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1363 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1364 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1365 | `     "    } else {"\` |
|        - |  1366 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1367 | `     "    }"\` |
|        - |  1368 | `     "   } else {"\` |
|        - |  1369 | `     "    $ret[$key] = $value;"\` |
|        - |  1370 | `     "   }"\` |
|        - |  1371 | `     "  }"\` |
|        - |  1372 | `     " }"\` |
|        - |  1373 | `	 " }"\` |
|        - |  1374 | `	 " return $ret;"\` |
|        - |  1375 | `    "}"\` |
|        - |  1376 | `	"function max(){"\` |
|        - |  1377 | `    "  $pArgs = func_get_args();"\` |
|        - |  1378 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1379 | `	"  return null;"\` |
|        - |  1380 | `    " }"\` |
|        - |  1381 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1382 | `    " $pArg = $pArgs[0];"\` |
|        - |  1383 | `	" if( !is_array($pArg) ){"\` |
|        - |  1384 | `	"   return $pArg; "\` |
|        - |  1385 | `	" }"\` |
|        - |  1386 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1387 | `	"   return null;"\` |
|        - |  1388 | `	" }"\` |
|        - |  1389 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1390 | `	" reset($pArg);"\` |
|        - |  1391 | `	" $max = current($pArg);"\` |
|        - |  1392 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1393 | `	"   if( $val > $max ){"\` |
|        - |  1394 | `	"     $max = $val;"\` |
|        - |  1395 | `    " }"\` |
|        - |  1396 | `	" }"\` |
|        - |  1397 | `	" return $max;"\` |
|        - |  1398 | `    " }"\` |
|        - |  1399 | `    " $max = $pArgs[0];"\` |
|        - |  1400 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1401 | `    " $val = $pArgs[$i];"\` |
|        - |  1402 | `	"if( $val > $max ){"\` |
|        - |  1403 | `	" $max = $val;"\` |
|        - |  1404 | `	"}"\` |
|        - |  1405 | `    " }"\` |
|        - |  1406 | `	" return $max;"\` |
|        - |  1407 | `    "}"\` |
|        - |  1408 | `	"function min(){"\` |
|        - |  1409 | `    "  $pArgs = func_get_args();"\` |
|        - |  1410 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1411 | `	"  return null;"\` |
|        - |  1412 | `    " }"\` |
|        - |  1413 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1414 | `    " $pArg = $pArgs[0];"\` |
|        - |  1415 | `	" if( !is_array($pArg) ){"\` |
|        - |  1416 | `	"   return $pArg; "\` |
|        - |  1417 | `	" }"\` |
|        - |  1418 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1419 | `	"   return null;"\` |
|        - |  1420 | `	" }"\` |
|        - |  1421 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1422 | `	" reset($pArg);"\` |
|        - |  1423 | `	" $min = current($pArg);"\` |
|        - |  1424 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1425 | `	"   if( $val < $min ){"\` |
|        - |  1426 | `	"     $min = $val;"\` |
|        - |  1427 | `    " }"\` |
|        - |  1428 | `	" }"\` |
|        - |  1429 | `	" return $min;"\` |
|        - |  1430 | `    " }"\` |
|        - |  1431 | `    " $min = $pArgs[0];"\` |
|        - |  1432 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1433 | `    " $val = $pArgs[$i];"\` |
|        - |  1434 | `	"if( $val < $min ){"\` |
|        - |  1435 | `	" $min = $val;"\` |
|        - |  1436 | `	" }"\` |
|        - |  1437 | `    " }"\` |
|        - |  1438 | `	" return $min;"\` |
|        - |  1439 | `	"}"\` |
|        - |  1440 | `	"function fileowner(string $file){"\` |
|        - |  1441 | `    " $a = stat($file);"\` |
|        - |  1442 | `	" if( !is_array($a) ){"\` |
|        - |  1443 | `	"	return false;"\` |
|        - |  1444 | `	" }"\` |
|        - |  1445 | `	" return $a['uid'];"\` |
|        - |  1446 | `    "}"\` |
|        - |  1447 | `    "function filegroup(string $file){"\` |
|        - |  1448 | `	" $a = stat($file);"\` |
|        - |  1449 | `	" if( !is_array($a) ){"\` |
|        - |  1450 | `	"	return false;"\` |
|        - |  1451 | `	" }"\` |
|        - |  1452 | `	" return $a['gid'];"\` |
|        - |  1453 | `    "}"\` |
|        - |  1454 | `	 "function fileinode(string $file){"\` |
|        - |  1455 | `	" $a = stat($file);"\` |
|        - |  1456 | `	" if( !is_array($a) ){"\` |
|        - |  1457 | `	"	return false;"\` |
|        - |  1458 | `	" }"\` |
|        - |  1459 | `	" return $a['ino'];"\` |
|        - |  1460 | `    "}"` |
|        - |  1461 |  |
|        - |  1462 | `/*` |
|        - |  1463 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1464 | ` * start compiling the target PHP program.` |
|        - |  1465 | ` */` |
|     2992 |  1466 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1467 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1468 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1469 | `	 )` |
|        2 |  1470 |  |
|        - |  1471 | `	SyString sBuiltin;` |
|        - |  1472 | `	ph7_value *pObj;` |
|        - |  1473 | `	sxi32 rc;` |
|        - |  1474 | `	/* Zero the structure */` |
|     2994 |  1475 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1476 | `	/* Initialize VM fields */` |
|     2994 |  1477 | `	pVm->pEngine = &(*pEngine);` |
|     2994 |  1478 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1479 | `	/* Instructions containers */` |
|     2994 |  1480 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     2994 |  1481 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     2994 |  1482 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1483 | `	/* Object containers */` |
|     2994 |  1484 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2994 |  1485 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1486 | `	/* Virtual machine internal containers */` |
|     2994 |  1487 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     2994 |  1488 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     2994 |  1489 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     2994 |  1490 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     2994 |  1491 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     2994 |  1492 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     2994 |  1493 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     2994 |  1494 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     2994 |  1495 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     2994 |  1496 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     2994 |  1497 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     2994 |  1498 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     2994 |  1499 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     2994 |  1500 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     2994 |  1501 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     2994 |  1502 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     2994 |  1503 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     2994 |  1504 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     2994 |  1505 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     2994 |  1506 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     2994 |  1507 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     2994 |  1508 | `	pVm->pPendingException = 0;` |
|        - |  1509 | `	/* Configuration containers */` |
|     2994 |  1510 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1511 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1512 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     2994 |  1513 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     2994 |  1514 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     2994 |  1515 | `	pVm->iResponseStatus = 200;` |
|     2994 |  1516 | `	pVm->bHeadersSent = 0;` |
|     2994 |  1517 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1518 | `	/* Error callbacks containers */` |
|     2994 |  1519 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     2994 |  1520 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     2994 |  1521 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     2994 |  1522 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     2994 |  1523 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1524 | `	/* Set a default recursion limit */` |
|        - |  1525 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     2994 |  1526 | `	pVm->nMaxDepth = 32;` |
|        - |  1527 | `#else` |
|        - |  1528 | `	pVm->nMaxDepth = 16;` |
|        - |  1529 | `#endif` |
|        - |  1530 | `	/* Default assertion flags */` |
|     2994 |  1531 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1532 | `	/* JSON return status */` |
|     2994 |  1533 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1534 | `	/* PRNG context */` |
|     2994 |  1535 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1536 | `	/* Install the null constant */` |
|     2994 |  1537 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1538 | `	if( pObj == 0 ){` |
|      ! 0 |  1539 | `		rc = SXERR_MEM;` |
|      ! 0 |  1540 | `		goto Err;` |
|        - |  1541 | `	}` |
|     2994 |  1542 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1543 | `	/* Install the boolean TRUE constant */` |
|     2994 |  1544 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1545 | `	if( pObj == 0 ){` |
|      ! 0 |  1546 | `		rc = SXERR_MEM;` |
|      ! 0 |  1547 | `		goto Err;` |
|        - |  1548 | `	}` |
|     2994 |  1549 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1550 | `	/* Install the boolean FALSE constant */` |
|     2994 |  1551 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     2994 |  1552 | `	if( pObj == 0 ){` |
|      ! 0 |  1553 | `		rc = SXERR_MEM;` |
|      ! 0 |  1554 | `		goto Err;` |
|        - |  1555 | `	}` |
|     2994 |  1556 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1557 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1558 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1559 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     2994 |  1560 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     2994 |  1561 | `	if( pObj == 0 ){` |
|      ! 0 |  1562 | `		rc = SXERR_MEM;` |
|      ! 0 |  1563 | `		goto Err;` |
|        - |  1564 | `	}` |
|     2994 |  1565 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1566 | `	/* Create the global frame */` |
|     2994 |  1567 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     2994 |  1568 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1569 | `		goto Err;` |
|        - |  1570 | `	}` |
|        - |  1571 | `	/* Initialize the code generator */` |
|     2994 |  1572 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2994 |  1573 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1574 | `		goto Err;` |
|        - |  1575 | `	}` |
|        - |  1576 | `	/* VM correctly initialized,set the magic number */` |
|     2994 |  1577 | `	pVm->nMagic = PH7_VM_INIT;` |
|     2994 |  1578 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1579 | `	/* Compile the built-in library */` |
|     2994 |  1580 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1581 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     2994 |  1582 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1583 | `	/* Register Fiber internal C functions */` |
|     2994 |  1584 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     2994 |  1585 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     2994 |  1586 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     2994 |  1587 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     2994 |  1588 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     2994 |  1589 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     2994 |  1590 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     2994 |  1591 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     2994 |  1592 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     2994 |  1593 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1594 | `	/* Cache the Generator class pointer and register generator functions */` |
|     2994 |  1595 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     2994 |  1596 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     2994 |  1597 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     2994 |  1598 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     2994 |  1599 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     2994 |  1600 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     2994 |  1601 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     2994 |  1602 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     2994 |  1603 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     2994 |  1604 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1605 | `	/* Reset the code generator */` |
|     2994 |  1606 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     2994 |  1607 | `	return SXRET_OK;` |
|      ! 0 |  1608 | `Err:` |
|      ! 0 |  1609 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1610 | `	return rc;` |
|     1498 |  1611 |  |
|        - |  1612 | `/*` |
|        - |  1613 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1614 | ` * routine which store the output in an internal blob.` |
|        - |  1615 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1616 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1617 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1618 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1619 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1620 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1621 | ` * to finish executing and extracting the output.` |
|        - |  1622 | ` */` |
|       38 |  1623 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1624 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1625 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1626 | `	void *pUserData     /* User private data */` |
|        - |  1627 | `	)` |
|      ! 0 |  1628 |  |
|        - |  1629 | `	 sxi32 rc;` |
|        - |  1630 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1631 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1632 | `	 return rc;` |
|      ! 0 |  1633 |  |
|        - |  1634 | `/*` |
|        - |  1635 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1636 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1637 | ` */` |
|    17956 |  1638 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1639 |  |
|    17958 |  1640 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    17958 |  1641 | `	if( xCons != VmObConsumer ){` |
|     7450 |  1642 | `		pVm->nOutputLen += nLen;` |
|     7450 |  1643 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      950 |  1644 | `			pVm->bHeadersSent = 1;` |
|      474 |  1645 | `		}` |
|     3724 |  1646 | `	}` |
|    17958 |  1647 |  |
|        - |  1648 | `#define VM_STACK_GUARD 16` |
|        - |  1649 | `/*` |
|        - |  1650 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1651 | ` * our compiled PHP program.` |
|        - |  1652 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1653 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1654 | ` */` |
|    42314 |  1655 | `static ph7_value * VmNewOperandStack(` |
|        - |  1656 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1657 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1658 | `	)` |
|        2 |  1659 |  |
|        - |  1660 | `	ph7_value *pStack;` |
|        - |  1661 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1662 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1663 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1664 | `  ** on the maximum stack depth required.` |
|        - |  1665 | `  **` |
|        - |  1666 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1667 | `  */` |
|    42316 |  1668 | `	nInstr += VM_STACK_GUARD;` |
|    42316 |  1669 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    42316 |  1670 | `	if( pStack == 0 ){` |
|      ! 0 |  1671 | `		return 0;` |
|        - |  1672 | `	}` |
|        - |  1673 | `	/* Initialize the operand stack */` |
|  2916462 |  1674 | `	while( nInstr > 0 ){` |
|  2874148 |  1675 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2874148 |  1676 | `		--nInstr;` |
|        2 |  1677 | `	}` |
|        - |  1678 | `	/* Ready for bytecode execution */` |
|    42316 |  1679 | `	return pStack;` |
|    21159 |  1680 |  |
|        - |  1681 | `/* Forward declaration */` |
|        - |  1682 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1683 | `/*` |
|        - |  1684 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1685 | ` * This routine gets called by the PH7 engine after` |
|        - |  1686 | ` * successful compilation of the target PHP program.` |
|        - |  1687 | ` */` |
|     2678 |  1688 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1689 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1690 | `	)` |
|        2 |  1691 |  |
|        - |  1692 | `	SyHashEntry *pEntry;` |
|        - |  1693 | `	sxi32 rc;` |
|     2680 |  1694 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1695 | `		/* Initialize your VM first */` |
|      ! 0 |  1696 | `		return SXERR_CORRUPT;` |
|        - |  1697 | `	}` |
|        - |  1698 | `	/* Mark the VM ready for byte-code execution */` |
|     2680 |  1699 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1700 | `	/* Release the code generator now we have compiled our program */` |
|     2680 |  1701 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1702 | `	/* Emit the DONE instruction */` |
|     2680 |  1703 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2680 |  1704 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1705 | `		return SXERR_MEM;` |
|        - |  1706 | `	}` |
|        - |  1707 | `	/* Script return value */` |
|     2680 |  1708 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1709 | `	/* Allocate a new operand stack */` |
|     2680 |  1710 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2680 |  1711 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1712 | `		return SXERR_MEM;` |
|        - |  1713 | `	}` |
|        - |  1714 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1715 | `	 * private data. */` |
|     2680 |  1716 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2680 |  1717 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1718 | `	/* Allocate the reference table */` |
|     2680 |  1719 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2680 |  1720 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2680 |  1721 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1722 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1723 | `		return SXERR_MEM;` |
|        - |  1724 | `	}` |
|        - |  1725 | `	/* Zero the reference table */` |
|     2680 |  1726 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1727 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2680 |  1728 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2680 |  1729 | `	if( rc != SXRET_OK ){` |
|        - |  1730 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1731 | `		return rc;` |
|        - |  1732 | `	}` |
|        - |  1733 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2680 |  1734 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2680 |  1735 | `	if( rc != SXRET_OK ){` |
|        - |  1736 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1737 | `		return rc;` |
|        - |  1738 | `	}` |
|        - |  1739 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2680 |  1740 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1741 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2680 |  1742 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1743 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2680 |  1744 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1745 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1746 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2680 |  1747 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2680 |  1748 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1749 | `#endif` |
|        - |  1750 | `	/* Initialize and install static and constants class attributes */` |
|     2680 |  1751 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    51160 |  1752 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    48482 |  1753 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    48482 |  1754 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1755 | `			return rc;` |
|        - |  1756 | `		}` |
|        2 |  1757 | `	}` |
|        - |  1758 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2680 |  1759 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1760 | `	/* VM is ready for bytecode execution */` |
|     2680 |  1761 | `	return SXRET_OK;` |
|     1341 |  1762 |  |
|        - |  1763 | `/*` |
|        - |  1764 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1765 | ` */` |
|      ! 0 |  1766 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1767 |  |
|      ! 0 |  1768 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1769 | `		return SXERR_CORRUPT;` |
|        - |  1770 | `	}` |
|        - |  1771 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1772 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1773 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1774 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1775 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1776 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1777 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1778 | `	pVm->bHttpContext = 0;` |
|        - |  1779 | `	/* Set the ready flag */` |
|      ! 0 |  1780 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1781 | `	return SXRET_OK;` |
|      ! 0 |  1782 |  |
|        - |  1783 | `/*` |
|        - |  1784 | ` * Release a Virtual Machine.` |
|        - |  1785 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1786 | ` */` |
|     2670 |  1787 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1788 |  |
|        - |  1789 | `	/* Set the stale magic number */` |
|     2672 |  1790 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1791 | `	/* Release the private memory subsystem */` |
|     2672 |  1792 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2672 |  1793 | `	return SXRET_OK;` |
|        2 |  1794 |  |
|        - |  1795 | `/*` |
|        - |  1796 | ` * Initialize a foreign function call context.` |
|        - |  1797 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1798 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1799 | ` * functions.` |
|        - |  1800 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1801 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1802 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1803 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1804 | ` */` |
|   672116 |  1805 | `static sxi32 VmInitCallContext(` |
|        - |  1806 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1807 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1808 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1809 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1810 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1811 | `	)` |
|        2 |  1812 |  |
|   672118 |  1813 | `	pOut->pFunc = pFunc;` |
|   672118 |  1814 | `	pOut->pVm   = pVm;` |
|   672118 |  1815 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   672118 |  1816 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1817 | `	/* Assume a null return value */` |
|   672118 |  1818 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   672118 |  1819 | `	pOut->pRet = pRet;` |
|   672118 |  1820 | `	pOut->iFlags = iFlags;` |
|   672118 |  1821 | `	return SXRET_OK;` |
|        2 |  1822 |  |
|        - |  1823 | `/*` |
|        - |  1824 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1825 | ` * left behind.` |
|        - |  1826 | ` */` |
|   672116 |  1827 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1828 |  |
|        - |  1829 | `	sxu32 n;` |
|   672118 |  1830 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8230 |  1831 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    23956 |  1832 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    15728 |  1833 | `			if( apObj[n] == 0 ){` |
|        - |  1834 | `				/* Already released */` |
|      318 |  1835 | `				continue;` |
|        - |  1836 | `			}` |
|    15412 |  1837 | `			PH7_MemObjRelease(apObj[n]);` |
|    15412 |  1838 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7707 |  1839 | `		}` |
|     8230 |  1840 | `		SySetRelease(&pCtx->sVar);` |
|     4114 |  1841 | `	}` |
|   672118 |  1842 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1843 | `		ph7_aux_data *aAux;` |
|        - |  1844 | `		void *pChunk;` |
|        - |  1845 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1846 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1847 | `		 */` |
|        9 |  1848 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1849 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1850 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1851 | `			/* Release the chunk */` |
|       25 |  1852 | `			if( pChunk ){` |
|       25 |  1853 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1854 | `			}` |
|       13 |  1855 | `		}` |
|        9 |  1856 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1857 | `	}` |
|   672118 |  1858 |  |
|        - |  1859 | `/*` |
|        - |  1860 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1861 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1862 | ` */` |
|      316 |  1863 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1864 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1865 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1866 | `	)` |
|        2 |  1867 |  |
|      318 |  1868 | `	if( pValue == 0 ){` |
|        - |  1869 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1870 | `		return;` |
|        - |  1871 | `	}` |
|      318 |  1872 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1873 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1874 | `		sxu32 n;` |
|     1116 |  1875 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1876 | `			if( apObj[n] == pValue ){` |
|      318 |  1877 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1878 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1879 | `				/* Mark as released */` |
|      318 |  1880 | `				apObj[n] = 0;` |
|      318 |  1881 | `				break;` |
|        - |  1882 | `			}` |
|      401 |  1883 | `		}` |
|      158 |  1884 | `	}` |
|      160 |  1885 |  |
|        - |  1886 | `/*` |
|        - |  1887 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1888 | ` */` |
|  3837352 |  1889 | `static void VmPopOperand(` |
|        - |  1890 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1891 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1892 | `	)` |
|        2 |  1893 |  |
|  3837354 |  1894 | `	ph7_value *pTos = *ppTos;` |
|  8168370 |  1895 | `	while( nPop > 0 ){` |
|  4331018 |  1896 | `		PH7_MemObjRelease(pTos);` |
|  4331018 |  1897 | `		pTos--;` |
|  4331018 |  1898 | `		nPop--;` |
|        2 |  1899 | `	}` |
|        - |  1900 | `	/* Top of the stack */` |
|  3837354 |  1901 | `	*ppTos = pTos;` |
|  3837354 |  1902 |  |
|        - |  1903 | `/*` |
|        - |  1904 | ` * Reserve a memory object.` |
|        - |  1905 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1906 | ` */` |
|  3161700 |  1907 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1908 |  |
|  3161702 |  1909 | `	ph7_value *pObj = 0;` |
|        - |  1910 | `	VmSlot *pSlot;` |
|        - |  1911 | `	sxu32 nIdx;` |
|        - |  1912 | `	/* Check for a free slot */` |
|  3161702 |  1913 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3161702 |  1914 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3161702 |  1915 | `	if( pSlot ){` |
|  1012386 |  1916 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1012386 |  1917 | `		nIdx = pSlot->nIdx;` |
|   506192 |  1918 | `	}` |
|  3161702 |  1919 | `	if( pObj == 0 ){` |
|        - |  1920 | `		/* Reserve a new memory object */` |
|  2149318 |  1921 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2149318 |  1922 | `		if( pObj == 0 ){` |
|      ! 0 |  1923 | `			return 0;` |
|        - |  1924 | `		}` |
|  1074658 |  1925 | `	}` |
|        - |  1926 | `	/* Set a null default value */` |
|  3161702 |  1927 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3161702 |  1928 | `	pObj->nIdx = nIdx;` |
|  3161702 |  1929 | `	return pObj;` |
|  1580852 |  1930 |  |
|        - |  1931 | `/*` |
|        - |  1932 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1933 | ` */` |
|    34308 |  1934 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1935 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1936 | `	const char *zKey,  /* Entry key */` |
|        - |  1937 | `	sxu32 nByte,       /* Key length */` |
|        - |  1938 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1939 | `	)` |
|        2 |  1940 |  |
|        - |  1941 | `	ph7_value sKey;` |
|        - |  1942 | `	sxi32 rc;` |
|    34310 |  1943 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    34310 |  1944 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1945 | `	/* Perform the insertion */` |
|    34310 |  1946 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    34310 |  1947 | `	PH7_MemObjRelease(&sKey);` |
|    34310 |  1948 | `	return rc;` |
|        2 |  1949 |  |
|        - |  1950 | `/*` |
|        - |  1951 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1952 | ` * Return a pointer to the variable value on success.` |
|        - |  1953 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1954 | ` */` |
|  3570452 |  1955 | `static ph7_value * VmExtractMemObj(` |
|        - |  1956 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1957 | `	const SyString *pName, /* Variable name */` |
|        - |  1958 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1959 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1960 | `	)` |
|        2 |  1961 |  |
|  3570454 |  1962 | `	int bNullify = FALSE;` |
|        - |  1963 | `	SyHashEntry *pEntry;` |
|        - |  1964 | `	VmFrame *pFrame;` |
|        - |  1965 | `	ph7_value *pObj;` |
|        - |  1966 | `	sxu32 nIdx;` |
|        - |  1967 | `	sxi32 rc;` |
|        - |  1968 | `	/* Point to the top active frame */` |
|  3570454 |  1969 | `	pFrame = pVm->pFrame;` |
|  3570454 |  1970 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1971 | `	/* Perform the lookup */` |
|  3570454 |  1972 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  1973 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  1974 | `		pName = &sAnnon;` |
|        - |  1975 | `		/* Always nullify the object */` |
|      ! 0 |  1976 | `		bNullify = TRUE;` |
|      ! 0 |  1977 | `		bDup = FALSE;` |
|      ! 0 |  1978 | `	}` |
|        - |  1979 | `	/* Check the superglobals table first */` |
|  3570454 |  1980 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3570454 |  1981 | `	if( pEntry == 0 ){` |
|        - |  1982 | `		/* Query the top active frame */` |
|  3570414 |  1983 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3570414 |  1984 | `		if( pEntry == 0 ){` |
|   108544 |  1985 | `			char *zName = (char *)pName->zString;` |
|        - |  1986 | `			VmSlot sLocal;` |
|   108544 |  1987 | `			if( !bCreate ){` |
|        - |  1988 | `				/* Do not create the variable,return NULL instead */` |
|      122 |  1989 | `				return 0;` |
|        - |  1990 | `			}` |
|        - |  1991 | `			/* No such variable,automatically create a new one and install` |
|        - |  1992 | `			 * it in the current frame.` |
|        - |  1993 | `			 */` |
|   108424 |  1994 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   108424 |  1995 | `			if( pObj == 0 ){` |
|      ! 0 |  1996 | `				return 0;` |
|        - |  1997 | `			}` |
|   108424 |  1998 | `			nIdx = pObj->nIdx;` |
|   108424 |  1999 | `			if( bDup ){` |
|        - |  2000 | `				/* Duplicate name */` |
|      196 |  2001 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      196 |  2002 | `				if( zName == 0 ){` |
|      ! 0 |  2003 | `					return 0;` |
|        - |  2004 | `				}` |
|       97 |  2005 | `			}` |
|        - |  2006 | `			/* Link to the top active VM frame */` |
|   108424 |  2007 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   108424 |  2008 | `			if( rc != SXRET_OK ){` |
|        - |  2009 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2010 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2011 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2012 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2013 | `				return 0;` |
|        - |  2014 | `			}` |
|   108424 |  2015 | `			if( pFrame->pParent != 0 ){` |
|        - |  2016 | `				/* Local variable */` |
|   100924 |  2017 | `				sLocal.nIdx = nIdx;` |
|   100924 |  2018 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    50463 |  2019 | `			}else{` |
|        - |  2020 | `				/* Register in the $GLOBALS array */` |
|     7502 |  2021 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2022 | `			}` |
|        - |  2023 | `			/* Install in the reference table */` |
|   108424 |  2024 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2025 | `			/* Save object index */` |
|   108424 |  2026 | `			pObj->nIdx = nIdx;` |
|    54213 |  2027 | `		}else{` |
|        - |  2028 | `			/* Extract variable contents */` |
|  3461872 |  2029 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3461872 |  2030 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3461872 |  2031 | `			if( bNullify && pObj ){` |
|      ! 0 |  2032 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2033 | `			}` |
|        - |  2034 | `		}` |
|  1785258 |  2035 | `	}else{` |
|        - |  2036 | `		/* Superglobal */` |
|       42 |  2037 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2038 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2039 | `	}` |
|  3570334 |  2040 | `	return pObj;` |
|  1785338 |  2041 |  |
|        - |  2042 | `/*` |
|        - |  2043 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2044 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2045 | ` */` |
|     2982 |  2046 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2047 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2048 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2049 | `	sxu32 nByte        /* zName length */` |
|        - |  2050 | `	)` |
|        2 |  2051 |  |
|        - |  2052 | `	SyHashEntry *pEntry;` |
|        - |  2053 | `	ph7_value *pValue;` |
|        - |  2054 | `	sxu32 nIdx;` |
|        - |  2055 | `	/* Query the superglobal table */` |
|     2984 |  2056 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     2984 |  2057 | `	if( pEntry == 0 ){` |
|        - |  2058 | `		/* No such entry */` |
|      ! 0 |  2059 | `		return 0;` |
|        - |  2060 | `	}` |
|        - |  2061 | `	/* Extract the superglobal index in the global object pool */` |
|     2984 |  2062 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2063 | `	/* Extract the variable value  */` |
|     2984 |  2064 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     2984 |  2065 | `	return pValue;` |
|     1493 |  2066 |  |
|        - |  2067 | `/*` |
|        - |  2068 | ` * Perform a raw hashmap insertion.` |
|        - |  2069 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2070 | ` */` |
|     3012 |  2071 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2072 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2073 | `	const char *zKey,   /* Entry key */` |
|        - |  2074 | `	int nKeylen,        /* zKey length*/` |
|        - |  2075 | `	const char *zData,  /* Entry data */` |
|        - |  2076 | `	int nLen            /* zData length */` |
|        - |  2077 | `	)` |
|        2 |  2078 |  |
|        - |  2079 | `	ph7_value sKey,sValue;` |
|        - |  2080 | `	sxi32 rc;` |
|     3014 |  2081 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3014 |  2082 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3014 |  2083 | `	if( zKey ){` |
|     2992 |  2084 | `		if( nKeylen < 0 ){` |
|     2940 |  2085 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1469 |  2086 | `		}` |
|     2992 |  2087 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1495 |  2088 | `	}` |
|     3014 |  2089 | `	if( zData ){` |
|     3014 |  2090 | `		if( nLen < 0 ){` |
|        - |  2091 | `			/* Compute length automatically */` |
|      144 |  2092 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2093 | `		}` |
|     3014 |  2094 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1506 |  2095 | `	}` |
|        - |  2096 | `	/* Perform the insertion */` |
|     3014 |  2097 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3014 |  2098 | `	PH7_MemObjRelease(&sKey);` |
|     3014 |  2099 | `	PH7_MemObjRelease(&sValue);` |
|     3014 |  2100 | `	return rc;` |
|        2 |  2101 |  |
|        - |  2102 | `/*` |
|        - |  2103 | ` * Configure a working virtual machine instance.` |
|        - |  2104 | ` *` |
|        - |  2105 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2106 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2107 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2108 | ` * The second argument to this function is an integer configuration option` |
|        - |  2109 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2110 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2111 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2112 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2113 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2114 | ` */` |
|    43178 |  2115 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2116 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2117 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2118 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2119 | `	)` |
|        2 |  2120 |  |
|    43180 |  2121 | `	sxi32 rc = SXRET_OK;` |
|    43180 |  2122 | `	switch(nOp){` |
|     1331 |  2123 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2664 |  2124 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2664 |  2125 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2126 | `		/* VM output consumer callback */` |
|        - |  2127 | `#ifdef UNTRUST` |
|        - |  2128 | `		if( xConsumer == 0 ){` |
|        - |  2129 | `			rc = SXERR_CORRUPT;` |
|        - |  2130 | `			break;` |
|        - |  2131 | `		}` |
|        - |  2132 | `#endif` |
|        - |  2133 | `		/* Install the output consumer */` |
|     2664 |  2134 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2664 |  2135 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2664 |  2136 | `		break;` |
|        - |  2137 | `							   }` |
|     1339 |  2138 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2139 | `		/* Import path */` |
|        - |  2140 | `		  const char *zPath;` |
|        - |  2141 | `		  SyString sPath;` |
|     2680 |  2142 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2143 | `#if defined(UNTRUST)` |
|        - |  2144 | `		  if( zPath == 0 ){` |
|        - |  2145 | `			  rc = SXERR_EMPTY;` |
|        - |  2146 | `			  break;` |
|        - |  2147 | `		  }` |
|        - |  2148 | `#endif` |
|     2680 |  2149 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2150 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2151 | `#ifdef __WINNT__` |
|        2 |  2152 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2153 | `#endif` |
|     5358 |  2154 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2155 | `		  /* Remove leading and trailing white spaces */` |
|     2680 |  2156 | `		  SyStringFullTrim(&sPath);` |
|     2680 |  2157 | `		  if( sPath.nByte > 0 ){` |
|        - |  2158 | `			  /* Store the path in the corresponding conatiner */` |
|     2680 |  2159 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1339 |  2160 | `		  }` |
|     2680 |  2161 | `		  break;` |
|        - |  2162 | `									 }` |
|     1339 |  2163 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2164 | `		/* Run-Time Error report */` |
|     2680 |  2165 | `		pVm->bErrReport = 1;` |
|     2680 |  2166 | `		break;` |
|      ! 0 |  2167 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2168 | `		/* Recursion depth */` |
|      ! 0 |  2169 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2170 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2171 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2172 | `		}` |
|      ! 0 |  2173 | `		break;` |
|        - |  2174 | `									   }` |
|      ! 0 |  2175 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2176 | `		/* VM output length in bytes */` |
|      ! 0 |  2177 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2178 | `#ifdef UNTRUST` |
|        - |  2179 | `		if( pOut == 0 ){` |
|        - |  2180 | `			rc = SXERR_CORRUPT;` |
|        - |  2181 | `			break;` |
|        - |  2182 | `		}` |
|        - |  2183 | `#endif` |
|      ! 0 |  2184 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2185 | `		break;` |
|        - |  2186 | `							   }` |
|        - |  2187 |  |
|    13390 |  2188 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2189 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2190 | `		/* Create a new superglobal/global variable */` |
|    26782 |  2191 | `		const char *zName = va_arg(ap,const char *);` |
|    26782 |  2192 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2193 | `		SyHashEntry *pEntry;` |
|        - |  2194 | `		ph7_value *pObj;` |
|        - |  2195 | `		sxu32 nByte;` |
|        - |  2196 | `		sxu32 nIdx;` |
|        - |  2197 | `#ifdef UNTRUST` |
|        - |  2198 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2199 | `			rc = SXERR_CORRUPT;` |
|        - |  2200 | `			break;` |
|        - |  2201 | `		}` |
|        - |  2202 | `#endif` |
|    26782 |  2203 | `		nByte = SyStrlen(zName);` |
|    26782 |  2204 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2205 | `			/* Check if the superglobal is already installed */` |
|    26782 |  2206 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    13392 |  2207 | `		}else{` |
|        - |  2208 | `			/* Query the top active VM frame */` |
|      ! 0 |  2209 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2210 | `		}` |
|    26782 |  2211 | `		if( pEntry ){` |
|        - |  2212 | `			/* Variable already installed */` |
|      ! 0 |  2213 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2214 | `			/* Extract contents */` |
|      ! 0 |  2215 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2216 | `			if( pObj ){` |
|        - |  2217 | `				/* Overwrite old contents */` |
|      ! 0 |  2218 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2219 | `			}` |
|      ! 0 |  2220 | `		}else{` |
|        - |  2221 | `			/* Install a new variable */` |
|    26782 |  2222 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    26782 |  2223 | `			if( pObj == 0 ){` |
|      ! 0 |  2224 | `				rc = SXERR_MEM;` |
|      ! 0 |  2225 | `				break;` |
|        - |  2226 | `			}` |
|    26782 |  2227 | `			nIdx = pObj->nIdx;` |
|        - |  2228 | `			/* Copy value */` |
|    26782 |  2229 | `			PH7_MemObjStore(pValue,pObj);` |
|    26782 |  2230 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2231 | `				/* Install the superglobal */` |
|    26782 |  2232 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    13392 |  2233 | `			}else{` |
|        - |  2234 | `				/* Install in the current frame */` |
|      ! 0 |  2235 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2236 | `			}` |
|    26782 |  2237 | `			if( rc == SXRET_OK ){` |
|        - |  2238 | `				SyHashEntry *pRef;` |
|    26782 |  2239 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    26782 |  2240 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    13392 |  2241 | `				}else{` |
|      ! 0 |  2242 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2243 | `				}` |
|        - |  2244 | `				/* Install in the reference table */` |
|    26782 |  2245 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    26782 |  2246 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2247 | `					/* Register in the $GLOBALS array */` |
|    26782 |  2248 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    13390 |  2249 | `				}` |
|    13390 |  2250 | `			}` |
|        - |  2251 | `		}` |
|    26782 |  2252 | `		break;` |
|        - |  2253 | `									}` |
|     1469 |  2254 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2255 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2256 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2257 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2258 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2259 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2260 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     2940 |  2261 | `		const char *zKey   = va_arg(ap,const char *);` |
|     2940 |  2262 | `		const char *zValue = va_arg(ap,const char *);` |
|     2940 |  2263 | `		int nLen = va_arg(ap,int);` |
|        - |  2264 | `		ph7_hashmap *pMap;` |
|        - |  2265 | `		ph7_value *pValue;` |
|     2940 |  2266 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2267 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2268 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     2939 |  2269 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2270 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2271 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     2938 |  2272 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2273 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2274 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     2938 |  2275 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2276 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2277 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     2938 |  2278 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2279 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2280 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     2938 |  2281 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2282 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2283 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2284 | `		}else{` |
|        - |  2285 | `			/* Extract the $_SERVER superglobal */` |
|     2938 |  2286 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2287 | `		}` |
|     2940 |  2288 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2289 | `			/* No such entry */` |
|      ! 0 |  2290 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2291 | `			break;` |
|        - |  2292 | `		}` |
|        - |  2293 | `		/* Point to the hashmap */` |
|     2940 |  2294 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2295 | `		/* Perform the insertion */` |
|     2940 |  2296 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     2940 |  2297 | `		break;` |
|        - |  2298 | `								   }` |
|       11 |  2299 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2300 | `		/* Script arguments */` |
|       24 |  2301 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2302 | `		ph7_hashmap *pMap;` |
|        - |  2303 | `		ph7_value *pValue;` |
|        - |  2304 | `		sxu32 n;` |
|       24 |  2305 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2306 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2307 | `			break;` |
|        - |  2308 | `		}` |
|        - |  2309 | `		/* Extract the $argv array */` |
|       24 |  2310 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2311 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2312 | `			/* No such entry */` |
|      ! 0 |  2313 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2314 | `			break;` |
|        - |  2315 | `		}` |
|        - |  2316 | `		/* Point to the hashmap */` |
|       24 |  2317 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2318 | `		/* Perform the insertion */` |
|       24 |  2319 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2320 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2321 | `		if( rc == SXRET_OK ){` |
|       24 |  2322 | `			if( pMap->nEntry > 1 ){` |
|        - |  2323 | `				/* Append space separator first */` |
|       18 |  2324 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2325 | `			}` |
|       24 |  2326 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2327 | `		}` |
|       24 |  2328 | `		break;` |
|        - |  2329 | `								  }` |
|      ! 0 |  2330 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2331 | `		/* error_log() consumer */` |
|      ! 0 |  2332 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2333 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2334 | `		break;` |
|        - |  2335 | `										}` |
|      ! 0 |  2336 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2337 | `		/* Script return value */` |
|      ! 0 |  2338 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2339 | `#ifdef UNTRUST` |
|        - |  2340 | `		if( ppValue == 0 ){` |
|        - |  2341 | `			rc = SXERR_CORRUPT;` |
|        - |  2342 | `			break;` |
|        - |  2343 | `		}` |
|        - |  2344 | `#endif` |
|      ! 0 |  2345 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2346 | `		break;` |
|        - |  2347 | `								   }` |
|     2678 |  2348 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2349 | `		/* Register an IO stream device */` |
|     5358 |  2350 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2351 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8034 |  2352 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5358 |  2353 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2354 | `				/* Invalid stream */` |
|      ! 0 |  2355 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2356 | `				break;` |
|        - |  2357 | `		}` |
|     5358 |  2358 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2359 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2680 |  2360 | `			pVm->pDefStream = pStream;` |
|     1339 |  2361 | `		}` |
|        - |  2362 | `		/* Insert in the appropriate container */` |
|     5358 |  2363 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5358 |  2364 | `		break;` |
|        - |  2365 | `								  }` |
|        8 |  2366 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2367 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2368 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2369 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2370 | `#ifdef UNTRUST` |
|        - |  2371 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2372 | `			rc = SXERR_CORRUPT;` |
|        - |  2373 | `			break;` |
|        - |  2374 | `		}` |
|        - |  2375 | `#endif` |
|       16 |  2376 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2377 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2378 | `		break;` |
|        - |  2379 | `									   }` |
|        8 |  2380 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2381 | `		/* Raw HTTP request*/` |
|       16 |  2382 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2383 | `		int nByte = va_arg(ap,int);` |
|       16 |  2384 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2385 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2386 | `			break;` |
|        - |  2387 | `		}` |
|       16 |  2388 | `		if( nByte < 0 ){` |
|        - |  2389 | `			/* Compute length automatically */` |
|      ! 0 |  2390 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2391 | `		}` |
|        - |  2392 | `		/* Process the request */` |
|       16 |  2393 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2394 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2395 | `		if( rc == SXRET_OK ){` |
|       16 |  2396 | `			pVm->bHttpContext = 1;` |
|        8 |  2397 | `		}` |
|       16 |  2398 | `		break;` |
|        - |  2399 | `									}` |
|        8 |  2400 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2401 | `		/* Extract HTTP response status code */` |
|       16 |  2402 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2403 | `		if( pStatus ){` |
|       16 |  2404 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2405 | `		}` |
|       16 |  2406 | `		break;` |
|        - |  2407 | `										}` |
|        8 |  2408 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2409 | `		/* Iterate response headers via callback */` |
|        - |  2410 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2411 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2412 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2413 | `		if( xCallback ){` |
|       16 |  2414 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2415 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2416 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2417 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2418 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2419 | `							   pUserData);` |
|       12 |  2420 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2421 | `					break;` |
|        - |  2422 | `				}` |
|        6 |  2423 | `			}` |
|        8 |  2424 | `		}` |
|       16 |  2425 | `		break;` |
|        - |  2426 | `										 }` |
|      ! 0 |  2427 | `	default:` |
|        - |  2428 | `		/* Unknown configuration option */` |
|      ! 0 |  2429 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2430 | `		break;` |
|        - |  2431 | `	}` |
|    43180 |  2432 | `	return rc;` |
|        2 |  2433 |  |
|        - |  2434 | `/* Forward declaration */` |
|        - |  2435 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2436 | `/*` |
|        - |  2437 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2438 | ` * format.` |
|        - |  2439 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2440 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2441 | ` * (STDOUT).` |
|        - |  2442 | ` */` |
|        2 |  2443 | `static sxi32 VmByteCodeDump(` |
|        - |  2444 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2445 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2446 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2447 | `	)` |
|        1 |  2448 |  |
|        - |  2449 | `	static const char zDump[] = {` |
|        - |  2450 | `		"====================================================\n"` |
|        - |  2451 | `		"PH7 VM Dump\n"` |
|        - |  2452 | `		"====================================================\n"` |
|        - |  2453 | `	};` |
|        - |  2454 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2455 | `	sxi32 rc = SXRET_OK;` |
|        - |  2456 | `	sxu32 n;` |
|        - |  2457 | `	/* Point to the PH7 instructions */` |
|        3 |  2458 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2459 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2460 | `	n = 0;` |
|        3 |  2461 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2462 | `	/* Dump instructions */` |
|        7 |  2463 | `	for(;;){` |
|       15 |  2464 | `		if( pInstr >= pEnd ){` |
|        - |  2465 | `			/* No more instructions */` |
|        3 |  2466 | `			break;` |
|        - |  2467 | `		}` |
|        - |  2468 | `		/* Format and call the consumer callback */` |
|       19 |  2469 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2470 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2471 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2472 | `		if( rc != SXRET_OK ){` |
|        - |  2473 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2474 | `			return rc;` |
|        - |  2475 | `		}` |
|       13 |  2476 | `		++n;` |
|       13 |  2477 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2478 | `	}` |
|        3 |  2479 | `	return rc;` |
|        2 |  2480 |  |
|        - |  2481 | `/* Forward declaration */` |
|        - |  2482 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2483 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2484 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2485 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2486 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2487 | `/*` |
|        - |  2488 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2489 | ` * consumer callback.` |
|        - |  2490 | ` */` |
|      598 |  2491 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2492 |  |
|      599 |  2493 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      599 |  2494 | `	sxi32 rc = SXRET_OK;` |
|        - |  2495 | `	/* Append a new line */` |
|        - |  2496 | `#ifdef __WINNT__` |
|        1 |  2497 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2498 | `#else` |
|      598 |  2499 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2500 | `#endif` |
|        - |  2501 | `	/* Invoke the output consumer callback */` |
|      599 |  2502 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      599 |  2503 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      599 |  2504 | `	return rc;` |
|        1 |  2505 |  |
|        - |  2506 | `/*` |
|        - |  2507 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2508 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2509 | ` * information.` |
|        - |  2510 | ` */` |
|      148 |  2511 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2512 |  |
|      150 |  2513 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2514 | `		ph7_value apArg[4];` |
|        - |  2515 | `		ph7_value *apArgPtr[4];` |
|        - |  2516 | `		ph7_value sResult;` |
|        - |  2517 | `		SyString sErr;` |
|        - |  2518 | `		/* Prepare arguments */` |
|       76 |  2519 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2520 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2521 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2522 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2523 | `		if( pFile ){` |
|       76 |  2524 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2525 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2526 | `		}else{` |
|      ! 0 |  2527 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2528 | `		}` |
|       76 |  2529 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2530 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2531 | `		/* Set up pointer array */` |
|       76 |  2532 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2533 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2534 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2535 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2536 | `		/* Call the handler */` |
|       76 |  2537 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2538 | `		/* Check return value */` |
|       76 |  2539 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2540 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2541 | `		}` |
|        - |  2542 | `		/* Release */` |
|       76 |  2543 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2544 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2545 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2546 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2547 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2548 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2549 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2550 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2551 | `	}` |
|        - |  2552 | `	/* No handler, always call error handler */` |
|       75 |  2553 | `	return TRUE;` |
|       76 |  2554 |  |
|      110 |  2555 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2556 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2557 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2558 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2559 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2560 | `	)` |
|        2 |  2561 |  |
|      112 |  2562 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2563 | `	SyString *pFile;` |
|        - |  2564 | `	char *zErr;` |
|      112 |  2565 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2566 | `	if( !pVm->bErrReport ){` |
|        - |  2567 | `		/* Don't bother reporting errors */` |
|        3 |  2568 | `		return SXRET_OK;` |
|        - |  2569 | `	}` |
|        - |  2570 | `	/* Reset the working buffer */` |
|      110 |  2571 | `	SyBlobReset(pWorker);` |
|        - |  2572 | `	/* Peek the processed file if available */` |
|      110 |  2573 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2574 | `	if( pFile ){` |
|        - |  2575 | `		/* Append file name */` |
|      110 |  2576 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2577 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2578 | `	}` |
|        - |  2579 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2580 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2581 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2582 | `	 * E_DEPRECATED). */` |
|      110 |  2583 | `	zErr = "Error:  ";` |
|      110 |  2584 | `	switch(iErr){` |
|       19 |  2585 | `	case PH7_CTX_WARNING:` |
|       40 |  2586 | `		zErr = "Warning:  ";` |
|       40 |  2587 | `		break;` |
|        6 |  2588 | `	case PH7_CTX_NOTICE:` |
|       14 |  2589 | `		zErr = "Notice:  ";` |
|       12 |  2590 | `		break;` |
|       29 |  2591 | `	default:` |
|        - |  2592 | `		/* keep iErr unchanged */` |
|       58 |  2593 | `		break;` |
|        - |  2594 | `	}` |
|      110 |  2595 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2596 | `	if( pFuncName ){` |
|        - |  2597 | `		/* Append function name first */` |
|       23 |  2598 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2599 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2600 | `	}` |
|      110 |  2601 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2602 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2603 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2604 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2605 | `	}` |
|      110 |  2606 | `	return rc;` |
|       57 |  2607 |  |
|        - |  2608 | `/*` |
|        - |  2609 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2610 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2611 | ` * information.` |
|        - |  2612 | ` */` |
|       40 |  2613 | `static sxi32 VmThrowErrorAp(` |
|        - |  2614 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2615 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2616 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2617 | `	const char *zFormat, /* Format message */` |
|        - |  2618 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2619 | `	)` |
|        2 |  2620 |  |
|       42 |  2621 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2622 | `	SyBlob sMsg;` |
|        - |  2623 | `	SyString *pFile;` |
|        - |  2624 | `	char *zErr;` |
|       42 |  2625 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2626 | `	if( !pVm->bErrReport ){` |
|        - |  2627 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2628 | `		return SXRET_OK;` |
|        - |  2629 | `	}` |
|        - |  2630 | `	/* Reset the working buffer */` |
|       42 |  2631 | `	SyBlobReset(pWorker);` |
|        - |  2632 | `	/* Peek the processed file if available */` |
|       42 |  2633 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2634 | `	if( pFile ){` |
|        - |  2635 | `		/* Append file name */` |
|       42 |  2636 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2637 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2638 | `	}` |
|        - |  2639 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2640 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2641 | `	 * the correct errno value. */` |
|       42 |  2642 | `	zErr = "Error:  ";` |
|       42 |  2643 | `	switch(iErr){` |
|        4 |  2644 | `	case PH7_CTX_WARNING:` |
|        9 |  2645 | `		zErr = "Warning:  ";` |
|        9 |  2646 | `		break;` |
|        3 |  2647 | `	case PH7_CTX_NOTICE:` |
|        7 |  2648 | `		zErr = "Notice:  ";` |
|        6 |  2649 | `		break;` |
|       13 |  2650 | `	default:` |
|        - |  2651 | `		/* do not change iErr */` |
|       26 |  2652 | `		break;` |
|        - |  2653 | `	}` |
|       42 |  2654 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2655 | `	if( pFuncName ){` |
|        - |  2656 | `		/* Append function name first */` |
|       26 |  2657 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2658 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2659 | `	}` |
|        - |  2660 | `	/* Format the raw message */` |
|       42 |  2661 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2662 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2663 | `	/* Check if a user error handler is installed */` |
|       42 |  2664 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2665 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2666 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2667 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2668 | `	}` |
|       42 |  2669 | `	SyBlobRelease(&sMsg);` |
|       42 |  2670 | `	return rc;` |
|       22 |  2671 |  |
|        - |  2672 | `/*` |
|        - |  2673 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2674 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2675 | ` * possible.` |
|        - |  2676 | ` */` |
|       38 |  2677 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2678 |  |
|        - |  2679 | `	ph7_class *pClass;` |
|       39 |  2680 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2681 | `	ph7_class_instance *pThis;` |
|        - |  2682 | `	ph7_class_method *pCons;` |
|        - |  2683 | `	ph7_value sArg;` |
|        - |  2684 | `	ph7_value *apArg[1];` |
|        - |  2685 | `	SyBlob sMsg;` |
|        - |  2686 | `	SyString sMsgStr;` |
|        - |  2687 | `	VmFrame *pFrame;` |
|        - |  2688 | `	sxi32 rc;` |
|       39 |  2689 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2690 | `	if( pClass == 0 ){` |
|      ! 0 |  2691 | `		return PH7_ABORT;` |
|        - |  2692 | `	}` |
|       39 |  2693 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2694 | `	if( pThis == 0 ){` |
|      ! 0 |  2695 | `		return PH7_ABORT;` |
|        - |  2696 | `	}` |
|       39 |  2697 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2698 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2699 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2700 | `	{` |
|       39 |  2701 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2702 | `		if( pOwner ){` |
|       39 |  2703 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2704 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2705 | `		}else{` |
|      ! 0 |  2706 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2707 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2708 | `		}` |
|        - |  2709 | `	}` |
|       39 |  2710 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2711 | `	if( pCons ){` |
|       39 |  2712 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2713 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2714 | `		apArg[0] = &sArg;` |
|       39 |  2715 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2716 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2717 | `	}` |
|       39 |  2718 | `	SyBlobRelease(&sMsg);` |
|       39 |  2719 | `	pFrame = pVm->pFrame;` |
|       39 |  2720 | `	if( pFrame ){` |
|       39 |  2721 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2722 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2723 | `	}` |
|       39 |  2724 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2725 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2726 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2727 | `		return PH7_ABORT;` |
|        - |  2728 | `	}` |
|       39 |  2729 | `	return PH7_EXCEPTION;` |
|       20 |  2730 |  |
|        - |  2731 |  |
|        - |  2732 | `/*` |
|        - |  2733 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2734 | ` */` |
|        4 |  2735 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2736 |  |
|        - |  2737 | `	ph7_class *pErrClass;` |
|        - |  2738 | `	ph7_class_instance *pThis;` |
|        - |  2739 | `	ph7_class_method *pCons;` |
|        - |  2740 | `	ph7_value sArg;` |
|        - |  2741 | `	ph7_value *apArg[1];` |
|        - |  2742 | `	SyBlob sMsg;` |
|        - |  2743 | `	SyString sMsgStr;` |
|        - |  2744 | `	VmFrame *pFrame;` |
|        - |  2745 | `	sxi32 rc;` |
|        5 |  2746 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2747 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2748 | `		return PH7_ABORT;` |
|        - |  2749 | `	}` |
|        5 |  2750 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2751 | `	if( pThis == 0 ){` |
|      ! 0 |  2752 | `		return PH7_ABORT;` |
|        - |  2753 | `	}` |
|        5 |  2754 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2755 | `	{` |
|        5 |  2756 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2757 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2758 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2759 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2760 | `	}` |
|        5 |  2761 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2762 | `	if( pCons ){` |
|        5 |  2763 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2764 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2765 | `		apArg[0] = &sArg;` |
|        5 |  2766 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2767 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2768 | `	}` |
|        5 |  2769 | `	SyBlobRelease(&sMsg);` |
|        5 |  2770 | `	pFrame = pVm->pFrame;` |
|        5 |  2771 | `	if( pFrame ){` |
|        5 |  2772 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2773 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2774 | `	}` |
|        5 |  2775 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2776 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2777 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2778 | `		return PH7_ABORT;` |
|        - |  2779 | `	}` |
|        5 |  2780 | `	return PH7_EXCEPTION;` |
|        3 |  2781 |  |
|        - |  2782 |  |
|        - |  2783 | `/*` |
|        - |  2784 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2785 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2786 | ` * For class types, instanceof is verified.` |
|        - |  2787 | ` *` |
|        - |  2788 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2789 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2790 | ` */` |
|        - |  2791 | `/*` |
|        - |  2792 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2793 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2794 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2795 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2796 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2797 | ` */` |
|       20 |  2798 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2799 |  |
|        - |  2800 | `	const char *z, *zEnd, *zTail;` |
|        - |  2801 | `	sxu32 n;` |
|        - |  2802 | `	sxu8 bReal;` |
|        - |  2803 | `	sxi32 rc;` |
|       22 |  2804 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2805 | `		return 0;` |
|        - |  2806 | `	}` |
|       22 |  2807 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2808 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2809 | `	zEnd = z + n;` |
|       22 |  2810 | `	if( n == 0 ){` |
|      ! 0 |  2811 | `		return 0;` |
|        - |  2812 | `	}` |
|       22 |  2813 | `	zTail = 0;` |
|       22 |  2814 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2815 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2816 | `		return 0;` |
|        - |  2817 | `	}` |
|        - |  2818 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2819 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2820 | `		zTail++;` |
|      ! 0 |  2821 | `	}` |
|       16 |  2822 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2823 |  |
|        - |  2824 |  |
|        - |  2825 | `/*` |
|        - |  2826 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2827 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2828 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2829 | ` *   0 if it's not strictly numeric.` |
|        - |  2830 | ` */` |
|       16 |  2831 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2832 |  |
|        - |  2833 | `	const char *z, *zEnd, *zTail;` |
|        - |  2834 | `	sxu32 n;` |
|       18 |  2835 | `	sxu8 bReal = 0;` |
|        - |  2836 | `	sxi32 rc;` |
|       18 |  2837 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2838 | `		return 0;` |
|        - |  2839 | `	}` |
|       18 |  2840 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2841 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2842 | `	zEnd = z + n;` |
|       18 |  2843 | `	if( n == 0 ) return 0;` |
|       18 |  2844 | `	zTail = 0;` |
|       18 |  2845 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2846 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2847 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2848 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2849 | `	return bReal ? 2 : 1;` |
|       10 |  2850 |  |
|        - |  2851 |  |
|        - |  2852 | `/*` |
|        - |  2853 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2854 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2855 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2856 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2857 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2858 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2859 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2860 | ` * throw.` |
|        - |  2861 | ` *` |
|        - |  2862 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2863 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2864 | ` */` |
|       98 |  2865 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2866 |  |
|        - |  2867 | `	sxu32 i;` |
|        - |  2868 | `	ph7_type_alt *aAlts;` |
|        - |  2869 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2870 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2871 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2872 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2873 | `	}` |
|       88 |  2874 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2875 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2876 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2877 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2878 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2879 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2880 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2881 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2882 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2883 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2884 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2885 | `	}` |
|        - |  2886 | `	/* Object handling */` |
|       88 |  2887 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2888 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2889 | `		if( bHasClassAlt ){` |
|       14 |  2890 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2891 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2892 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2893 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2894 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2895 | `			}` |
|       26 |  2896 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2897 | `				ph7_class *pExpected;` |
|        - |  2898 | `				SyString *pCN;` |
|       22 |  2899 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2900 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2901 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2902 | `					pExpected = pSelfNow;` |
|       22 |  2903 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2904 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2905 | `				}else{` |
|       22 |  2906 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2907 | `				}` |
|       22 |  2908 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2909 | `					return SXRET_OK;` |
|        - |  2910 | `				}` |
|        8 |  2911 | `			}` |
|        2 |  2912 | `		}` |
|        9 |  2913 | `		return SXERR_INVALID;` |
|        - |  2914 | `	}` |
|        - |  2915 | `	/* Array handling */` |
|       72 |  2916 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2917 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2918 | `	}` |
|        - |  2919 | `	/* Scalar handling — exact match first */` |
|       66 |  2920 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2921 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2922 | `	}` |
|       42 |  2923 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2924 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2925 | `	}` |
|       38 |  2926 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2927 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2928 | `	}` |
|       18 |  2929 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2930 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2931 | `	}` |
|       18 |  2932 | `	if( bStrict ){` |
|        - |  2933 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2934 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2935 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2936 | `			return SXRET_OK;` |
|        - |  2937 | `		}` |
|      ! 0 |  2938 | `		return SXERR_INVALID;` |
|        - |  2939 | `	}` |
|        - |  2940 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2941 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2942 | `	 * to match PHP's union RFC. */` |
|        - |  2943 | `	{` |
|       18 |  2944 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2945 | `		if( bHasInt ){` |
|        - |  2946 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2947 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2948 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2949 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2950 | `				return SXRET_OK;` |
|        - |  2951 | `			}` |
|       18 |  2952 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2953 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2954 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2955 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2956 | `					return SXRET_OK;` |
|        - |  2957 | `				}` |
|      ! 0 |  2958 | `			}` |
|       18 |  2959 | `			if( kind == 1 ){` |
|        9 |  2960 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2961 | `				return SXRET_OK;` |
|        - |  2962 | `			}` |
|        4 |  2963 | `		}` |
|       10 |  2964 | `		if( bHasFloat ){` |
|       10 |  2965 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2966 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2967 | `				return SXRET_OK;` |
|        - |  2968 | `			}` |
|       10 |  2969 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2970 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2971 | `				return SXRET_OK;` |
|        - |  2972 | `			}` |
|        1 |  2973 | `		}` |
|        3 |  2974 | `		if( bHasString ){` |
|      ! 0 |  2975 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  2976 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  2977 | `				return SXRET_OK;` |
|        - |  2978 | `			}` |
|      ! 0 |  2979 | `		}` |
|        3 |  2980 | `		if( bHasBool ){` |
|      ! 0 |  2981 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  2982 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  2983 | `				return SXRET_OK;` |
|        - |  2984 | `			}` |
|      ! 0 |  2985 | `		}` |
|        - |  2986 | `	}` |
|        3 |  2987 | `	return SXERR_INVALID;` |
|       51 |  2988 |  |
|        - |  2989 |  |
|        - |  2990 | `/*` |
|        - |  2991 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  2992 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  2993 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  2994 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  2995 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  2996 | ` */` |
|       34 |  2997 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  2998 |  |
|       36 |  2999 | `	if( bStrict ){` |
|        - |  3000 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  3001 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3002 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3003 | `			return SXRET_OK;` |
|        - |  3004 | `		}` |
|        7 |  3005 | `		return SXERR_INVALID;` |
|        - |  3006 | `	}` |
|        - |  3007 | `	{` |
|       28 |  3008 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3009 | `		if( xCast ) xCast(pVal);` |
|        - |  3010 | `	}` |
|       28 |  3011 | `	return SXRET_OK;` |
|       19 |  3012 |  |
|        - |  3013 |  |
|        - |  3014 | `/*` |
|        - |  3015 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3016 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3017 | ` *` |
|        - |  3018 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3019 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3020 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3021 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3022 | ` */` |
|        8 |  3023 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  3024 |  |
|        9 |  3025 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  3026 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  3027 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  3028 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  3029 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  3030 | `		}` |
|        9 |  3031 | `		zBuf[nCopy] = 0;` |
|        9 |  3032 | `		return zBuf;` |
|        - |  3033 | `	}` |
|      ! 0 |  3034 | `	switch( nType ){` |
|      ! 0 |  3035 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3036 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3037 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3038 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3039 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3040 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3041 | `		default:             return "scalar";` |
|        - |  3042 | `	}` |
|        5 |  3043 |  |
|        - |  3044 |  |
|        - |  3045 | `/*` |
|        - |  3046 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3047 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3048 | ` */` |
|       18 |  3049 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3050 |  |
|       19 |  3051 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3052 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3053 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3054 | `	return zBuf;` |
|        1 |  3055 |  |
|        - |  3056 |  |
|    13702 |  3057 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3058 |  |
|        - |  3059 | `	SyHashEntry *pSlot;` |
|        - |  3060 | `	VmClassAttr *pVmAttr;` |
|        - |  3061 | `	ph7_class_attr *pAttr;` |
|    13704 |  3062 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    13704 |  3063 | `	if( pSlot == 0 ){` |
|    13502 |  3064 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3065 | `	}` |
|      204 |  3066 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      204 |  3067 | `	pAttr = pVmAttr->pAttr;` |
|      204 |  3068 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3069 | `		return SXRET_OK;` |
|        - |  3070 | `	}` |
|        - |  3071 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3072 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3073 | `	 * matching PHP's documented behavior. */` |
|      204 |  3074 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3075 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3076 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3077 |  |
|       16 |  3078 | `		if( rc == SXRET_OK ){` |
|        9 |  3079 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3080 | `			return SXRET_OK;` |
|        - |  3081 | `		}` |
|        7 |  3082 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3083 | `			char zBuf[128];` |
|        4 |  3084 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3085 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3086 | `		}` |
|        5 |  3087 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3088 | `	}` |
|        - |  3089 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      190 |  3090 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3091 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3092 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3093 | `			return SXRET_OK;` |
|        - |  3094 | `		}` |
|        3 |  3095 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3096 | `	}` |
|        - |  3097 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3098 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3099 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      178 |  3100 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3101 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3102 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3103 | `			return SXRET_OK;` |
|        - |  3104 | `		}` |
|        7 |  3105 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3106 | `	}` |
|      168 |  3107 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3108 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3109 | `		 * currently active on the self-stack. */` |
|       26 |  3110 | `		ph7_class *pExpected = 0;` |
|       26 |  3111 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3112 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3113 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3114 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3115 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3116 | `		}` |
|       26 |  3117 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3118 | `			pExpected = pSelfNow;` |
|       24 |  3119 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3120 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3121 | `		}else{` |
|       22 |  3122 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3123 | `		}` |
|       26 |  3124 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3125 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3126 | `		}` |
|       26 |  3127 | `		if( pExpected ){` |
|       22 |  3128 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3129 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3130 | `				char zBuf[128];` |
|        7 |  3131 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3132 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3133 | `			}` |
|        8 |  3134 | `		}` |
|       22 |  3135 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3136 | `		return SXRET_OK;` |
|        - |  3137 | `	}` |
|        - |  3138 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3139 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      144 |  3140 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3141 | `		char zBuf[128];` |
|       10 |  3142 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3143 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3144 | `	}` |
|      138 |  3145 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3146 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3147 | `		if( xCast ){` |
|        - |  3148 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3149 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3150 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3151 | `			}` |
|       24 |  3152 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3153 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3154 | `			}` |
|        - |  3155 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3156 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3157 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3158 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3159 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3160 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3161 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3162 | `			}` |
|       12 |  3163 | `			xCast(pValue);` |
|        5 |  3164 | `		}` |
|        5 |  3165 | `	}` |
|      124 |  3166 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      124 |  3167 | `	return SXRET_OK;` |
|     6853 |  3168 |  |
|        - |  3169 |  |
|        - |  3170 | `/*` |
|        - |  3171 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3172 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3173 | ` * information.` |
|        - |  3174 | ` * ------------------------------------` |
|        - |  3175 | ` * Simple boring wrapper function.` |
|        - |  3176 | ` * ------------------------------------` |
|        - |  3177 | ` */` |
|       16 |  3178 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3179 |  |
|        - |  3180 | `	va_list ap;` |
|        - |  3181 | `	sxi32 rc;` |
|       17 |  3182 | `	va_start(ap,zFormat);` |
|       17 |  3183 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3184 | `	va_end(ap);` |
|       17 |  3185 | `	return rc;` |
|        1 |  3186 |  |
|        - |  3187 | `/*` |
|        - |  3188 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3189 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3190 | ` */` |
|       34 |  3191 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3192 |  |
|        - |  3193 | `	ph7_class *pClass;` |
|        - |  3194 | `	ph7_class_instance *pThis;` |
|        - |  3195 | `	ph7_class_method *pCons;` |
|        - |  3196 | `	ph7_value sArg;` |
|        - |  3197 | `	ph7_value *apArg[1];` |
|        - |  3198 | `	SyBlob sMsg;` |
|        - |  3199 | `	SyString sMsgStr;` |
|        - |  3200 | `	VmFrame *pFrame;` |
|        - |  3201 | `	sxi32 rc;` |
|       35 |  3202 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3203 | `	if( pClass == 0 ){` |
|      ! 0 |  3204 | `		return PH7_ABORT;` |
|        - |  3205 | `	}` |
|       35 |  3206 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3207 | `	if( pThis == 0 ){` |
|      ! 0 |  3208 | `		return PH7_ABORT;` |
|        - |  3209 | `	}` |
|       35 |  3210 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3211 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3212 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3213 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3214 | `	if( pCons ){` |
|       35 |  3215 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3216 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3217 | `		apArg[0] = &sArg;` |
|       35 |  3218 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3219 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3220 | `	}` |
|       35 |  3221 | `	SyBlobRelease(&sMsg);` |
|       35 |  3222 | `	pFrame = pVm->pFrame;` |
|       35 |  3223 | `	if( pFrame ){` |
|       35 |  3224 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3225 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3226 | `	}` |
|       35 |  3227 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3228 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3229 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3230 | `		return PH7_ABORT;` |
|        - |  3231 | `	}` |
|       31 |  3232 | `	return PH7_EXCEPTION;` |
|       18 |  3233 |  |
|        - |  3234 | `/*` |
|        - |  3235 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3236 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3237 | ` */` |
|        6 |  3238 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3239 |  |
|        - |  3240 | `	ph7_class *pClass;` |
|        - |  3241 | `	ph7_class_instance *pThis;` |
|        - |  3242 | `	ph7_class_method *pCons;` |
|        - |  3243 | `	ph7_value sArg;` |
|        - |  3244 | `	ph7_value *apArg[1];` |
|        - |  3245 | `	SyBlob sMsg;` |
|        - |  3246 | `	SyString sMsgStr;` |
|        - |  3247 | `	VmFrame *pFrame;` |
|        - |  3248 | `	sxi32 rc;` |
|        7 |  3249 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3250 | `	if( pClass == 0 ){` |
|      ! 0 |  3251 | `		return PH7_ABORT;` |
|        - |  3252 | `	}` |
|        7 |  3253 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3254 | `	if( pThis == 0 ){` |
|      ! 0 |  3255 | `		return PH7_ABORT;` |
|        - |  3256 | `	}` |
|        7 |  3257 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3258 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3259 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3260 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3261 | `	if( pCons ){` |
|        7 |  3262 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3263 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3264 | `		apArg[0] = &sArg;` |
|        7 |  3265 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3266 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3267 | `	}` |
|        7 |  3268 | `	SyBlobRelease(&sMsg);` |
|        7 |  3269 | `	pFrame = pVm->pFrame;` |
|        7 |  3270 | `	if( pFrame ){` |
|        7 |  3271 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3272 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3273 | `	}` |
|        7 |  3274 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3275 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3276 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3277 | `		return PH7_ABORT;` |
|        - |  3278 | `	}` |
|      ! 0 |  3279 | `	return PH7_EXCEPTION;` |
|        4 |  3280 |  |
|        - |  3281 | `/*` |
|        - |  3282 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3283 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3284 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3285 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3286 | ` */` |
|        - |  3287 | `/*` |
|        - |  3288 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3289 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3290 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3291 | ` */` |
|       24 |  3292 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3293 |  |
|        - |  3294 | `	sxu32 nCopy;` |
|       26 |  3295 | `	if( nBuf == 0 ) return "";` |
|       26 |  3296 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3297 | `		zBuf[0] = 0;` |
|      ! 0 |  3298 | `		return zBuf;` |
|        - |  3299 | `	}` |
|       26 |  3300 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3301 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3302 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3303 | `	zBuf[nCopy] = 0;` |
|       26 |  3304 | `	return zBuf;` |
|       14 |  3305 |  |
|        - |  3306 |  |
|      262 |  3307 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3308 |  |
|      264 |  3309 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3310 | `	const char *zGiven;` |
|        - |  3311 | `	char zBuf[128];` |
|        - |  3312 | `	char zTypeBuf[128];` |
|        - |  3313 | `	/* Untyped function: no enforcement. */` |
|      264 |  3314 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3315 | `		return SXRET_OK;` |
|        - |  3316 | `	}` |
|        - |  3317 | `	/* void return type: the function must not produce a value. */` |
|      264 |  3318 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|       94 |  3319 | `		if( pValue == 0 ){` |
|       92 |  3320 | `			return SXRET_OK;` |
|        - |  3321 | `		}` |
|        - |  3322 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3323 | `		 * still counts as "returned a value" here. */` |
|        3 |  3324 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3325 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3326 | `	}` |
|        - |  3327 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3328 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      172 |  3329 | `	if( pValue == 0 ){` |
|      ! 0 |  3330 | `		const char *zExpected = "value";` |
|      ! 0 |  3331 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3332 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3333 | `		}` |
|      ! 0 |  3334 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3335 | `	}` |
|        - |  3336 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3337 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3338 | `	 * bNullable=0 here. */` |
|      172 |  3339 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3340 | `		sxi32 rcU;` |
|      ! 0 |  3341 | `		int bNullable = 0;` |
|      ! 0 |  3342 | `		const char *zExpected = "union";` |
|        - |  3343 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3344 | `		{` |
|        - |  3345 | `			sxu32 i;` |
|      ! 0 |  3346 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3347 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3348 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3349 | `			}` |
|        - |  3350 | `		}` |
|      ! 0 |  3351 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3352 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3353 | `			return SXRET_OK;` |
|        - |  3354 | `		}` |
|      ! 0 |  3355 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3356 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3357 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3358 | `			zGiven = "null";` |
|      ! 0 |  3359 | `		}else{` |
|      ! 0 |  3360 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3361 | `		}` |
|      ! 0 |  3362 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3363 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3364 | `		}` |
|      ! 0 |  3365 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3366 | `	}` |
|        - |  3367 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3368 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3369 | `	 * it into the TypeError message. */` |
|      172 |  3370 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3371 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3372 | `		const char *zExpected;` |
|        - |  3373 | `		ph7_class *pExpected;` |
|        6 |  3374 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3375 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3376 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3377 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3378 | `		}` |
|        6 |  3379 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3380 | `			pExpected = pSelfNow;` |
|        4 |  3381 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3382 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3383 | `		}else{` |
|        3 |  3384 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3385 | `		}` |
|        6 |  3386 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3387 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3388 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3389 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3390 | `		}` |
|        6 |  3391 | `		if( pExpected ){` |
|        6 |  3392 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3393 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3394 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3395 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3396 | `			}` |
|        2 |  3397 | `		}` |
|        6 |  3398 | `		return SXRET_OK;` |
|        - |  3399 | `	}` |
|        - |  3400 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3401 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3402 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3403 | `	 * via the type-text leading '?'. */` |
|      168 |  3404 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3405 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3406 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3407 | `			return SXRET_OK;` |
|        - |  3408 | `		}` |
|      ! 0 |  3409 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3410 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3411 | `			"null");` |
|        - |  3412 | `	}` |
|        - |  3413 | `	/* Exact match? Done. */` |
|      162 |  3414 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      156 |  3415 | `		return SXRET_OK;` |
|        - |  3416 | `	}` |
|        - |  3417 | `	/* Object->scalar is never compatible. */` |
|        8 |  3418 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3419 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3420 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3421 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3422 | `			zGiven);` |
|        - |  3423 | `	}` |
|        - |  3424 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3425 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3426 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3427 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3428 | `			ph7_type_name(pValue));` |
|        - |  3429 | `	}` |
|        - |  3430 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3431 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3432 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3433 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3434 | `	if( !bStrict` |
|        5 |  3435 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3436 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3437 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3438 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3439 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3440 | `			"string");` |
|        - |  3441 | `	}` |
|        6 |  3442 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3443 | `		return SXRET_OK;` |
|        - |  3444 | `	}` |
|        4 |  3445 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3446 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3447 | `		ph7_type_name(pValue));` |
|      133 |  3448 |  |
|        - |  3449 | `/*` |
|        - |  3450 | ` * Report a fatal named-argument error.` |
|        - |  3451 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3452 | ` */` |
|        6 |  3453 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3454 |  |
|        7 |  3455 | `	const char *zFunc = 0;` |
|        7 |  3456 | `	int nFunc = 0;` |
|        7 |  3457 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3458 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3459 |  |
|        - |  3460 | `/*` |
|        - |  3461 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3462 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3463 | ` * information.` |
|        - |  3464 | ` * ------------------------------------` |
|        - |  3465 | ` * Simple boring wrapper function.` |
|        - |  3466 | ` * ------------------------------------` |
|        - |  3467 | ` */` |
|       24 |  3468 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3469 |  |
|        - |  3470 | `	sxi32 rc;` |
|       26 |  3471 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3472 | `	return rc;` |
|        2 |  3473 |  |
|        - |  3474 | `/*` |
|        - |  3475 | ` * Resolve function context from the current frame.` |
|        - |  3476 | ` */` |
|     1014 |  3477 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3478 |  |
|        - |  3479 | `	VmFrame *pFrame;` |
|        - |  3480 | `	ph7_vm_func *pFunc;` |
|     1015 |  3481 | `	*pzFuncName = 0;` |
|     1015 |  3482 | `	*pnFuncLen = 0;` |
|     1015 |  3483 | `	pFrame = pVm->pFrame;` |
|     1015 |  3484 | `	if( pFrame == 0 ){` |
|      ! 0 |  3485 | `		return;` |
|        - |  3486 | `	}` |
|     1015 |  3487 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1015 |  3488 | `	if( pFrame->pParent == 0 ){` |
|      991 |  3489 | `		return;` |
|        - |  3490 | `	}` |
|       25 |  3491 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3492 | `	if( pFunc == 0 ){` |
|      ! 0 |  3493 | `		return;` |
|        - |  3494 | `	}` |
|       25 |  3495 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3496 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      508 |  3497 |  |
|        - |  3498 | `/*` |
|        - |  3499 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3500 | ` */` |
|      522 |  3501 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3502 |  |
|        - |  3503 | `	SyBlob sOut;` |
|        - |  3504 | `	SyString *pFile;` |
|      523 |  3505 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3506 | `		return PH7_OK;` |
|        - |  3507 | `	}` |
|      523 |  3508 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3509 | `		zClass = "Exception";` |
|      ! 0 |  3510 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3511 | `	}` |
|      523 |  3512 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      501 |  3513 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      250 |  3514 | `	}` |
|      523 |  3515 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      523 |  3516 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      523 |  3517 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      523 |  3518 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      523 |  3519 | `	if( zMsg && nMsg > 0 ){` |
|      523 |  3520 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      523 |  3521 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      261 |  3522 | `	}` |
|      523 |  3523 | `	if( pFile ){` |
|      523 |  3524 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      523 |  3525 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3526 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      261 |  3527 | `	}` |
|      523 |  3528 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      523 |  3529 | `	if( pFile ){` |
|      523 |  3530 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      523 |  3531 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3532 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3533 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3534 | `		}else{` |
|      499 |  3535 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3536 | `		}` |
|      261 |  3537 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3538 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3539 | `	}else{` |
|      ! 0 |  3540 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3541 | `	}` |
|      523 |  3542 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      523 |  3543 | `	if( pFile ){` |
|      523 |  3544 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      523 |  3545 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      523 |  3546 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      523 |  3547 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      261 |  3548 | `	}` |
|      523 |  3549 | `	VmCallErrorHandler(pVm,&sOut);` |
|      523 |  3550 | `	SyBlobRelease(&sOut);` |
|      523 |  3551 | `	return PH7_ABORT;` |
|      262 |  3552 |  |
|        - |  3553 | `/*` |
|        - |  3554 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3555 | ` */` |
|      568 |  3556 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3557 |  |
|        - |  3558 | `	ph7_vm *pVm;` |
|        - |  3559 | `	ph7_class *pClass;` |
|        - |  3560 | `	ph7_class_instance *pThis;` |
|        - |  3561 | `	ph7_class_method *pCons;` |
|        - |  3562 | `	ph7_value sArg;` |
|        - |  3563 | `	ph7_value *apArg[1];` |
|        - |  3564 | `	SyBlob sMsg;` |
|        - |  3565 | `	SyString sMsgStr;` |
|        - |  3566 | `	VmFrame *pFrame;` |
|        - |  3567 | `	va_list ap;` |
|        - |  3568 | `	sxi32 rc;` |
|        - |  3569 |  |
|      570 |  3570 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3571 | `		return PH7_ABORT;` |
|        - |  3572 | `	}` |
|      570 |  3573 | `	pVm = pCtx->pVm;` |
|      570 |  3574 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3575 | `		zClass = "Error";` |
|      ! 0 |  3576 | `	}` |
|      570 |  3577 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      570 |  3578 | `	if( pClass == 0 ){` |
|      ! 0 |  3579 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3580 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3581 | `			zClass` |
|        - |  3582 | `			);` |
|        - |  3583 | `	}` |
|      570 |  3584 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      570 |  3585 | `	if( pThis == 0 ){` |
|      ! 0 |  3586 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3587 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3588 | `			);` |
|        - |  3589 | `	}` |
|        - |  3590 |  |
|      570 |  3591 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      570 |  3592 | `	va_start(ap,zFormat);` |
|      570 |  3593 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      570 |  3594 | `	va_end(ap);` |
|        - |  3595 |  |
|      570 |  3596 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      570 |  3597 | `	if( pCons ){` |
|      570 |  3598 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      570 |  3599 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      570 |  3600 | `		apArg[0] = &sArg;` |
|      570 |  3601 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      570 |  3602 | `		PH7_MemObjRelease(&sArg);` |
|      284 |  3603 | `	}` |
|      570 |  3604 | `	SyBlobRelease(&sMsg);` |
|        - |  3605 |  |
|      570 |  3606 | `	pFrame = pVm->pFrame;` |
|      570 |  3607 | `	if( pFrame ){` |
|      570 |  3608 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      570 |  3609 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      284 |  3610 | `	}` |
|      570 |  3611 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      570 |  3612 | `	PH7_ClassInstanceUnref(pThis);` |
|      570 |  3613 | `	if( rc == SXERR_ABORT ){` |
|      489 |  3614 | `		return PH7_ABORT;` |
|        - |  3615 | `	}` |
|       82 |  3616 | `	return PH7_EXCEPTION;` |
|      286 |  3617 |  |
|        - |  3618 | `/*` |
|        - |  3619 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3620 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3621 | ` */` |
|      ! 0 |  3622 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3623 |  |
|        - |  3624 | `	ph7_vm *pVm;` |
|        - |  3625 | `	SyBlob sMsg;` |
|      ! 0 |  3626 | `	const char *zFuncName = 0;` |
|      ! 0 |  3627 | `	int nFuncLen = 0;` |
|        - |  3628 | `	va_list ap;` |
|        - |  3629 | `	sxi32 rc;` |
|        - |  3630 |  |
|      ! 0 |  3631 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3632 | `		return PH7_OK;` |
|        - |  3633 | `	}` |
|      ! 0 |  3634 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3635 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3636 | `		zClass = "Error";` |
|      ! 0 |  3637 | `	}` |
|        - |  3638 |  |
|      ! 0 |  3639 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3640 |  |
|      ! 0 |  3641 | `	va_start(ap,zFormat);` |
|      ! 0 |  3642 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3643 | `	va_end(ap);` |
|        - |  3644 |  |
|      ! 0 |  3645 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3646 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3647 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3648 | `	}` |
|      ! 0 |  3649 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3650 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3651 | `	}` |
|      ! 0 |  3652 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3653 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3654 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3655 | `	return rc;` |
|      ! 0 |  3656 |  |
|        - |  3657 | `/*` |
|        - |  3658 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3659 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3660 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3661 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3662 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3663 | ` * when VmByteCodeExec returns.` |
|        - |  3664 | ` */` |
|      144 |  3665 | `static sxi32 VmSuspendCtx(` |
|        - |  3666 | `	ph7_vm *pVm,` |
|        - |  3667 | `	ph7_exec_ctx *pCtx,` |
|        - |  3668 | `	sxi32 pc,` |
|        - |  3669 | `	sxi32 nTos` |
|        - |  3670 | `	)` |
|        2 |  3671 |  |
|       72 |  3672 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3673 | `	pCtx->pc = pc;` |
|      146 |  3674 | `	pCtx->nTos = nTos;` |
|      146 |  3675 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3676 | `	return PH7_SUSPEND;` |
|        2 |  3677 |  |
|        - |  3678 | `/*` |
|        - |  3679 | ` * Resolve named-argument mapping.` |
|        - |  3680 | ` *` |
|        - |  3681 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3682 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3683 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3684 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3685 | ` * every formal parameter that received a value.` |
|        - |  3686 | ` *` |
|        - |  3687 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3688 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3689 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3690 | ` */` |
|       98 |  3691 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3692 | `	ph7_vm *pVm,` |
|        - |  3693 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3694 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3695 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3696 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3697 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3698 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3699 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3700 |  |
|        2 |  3701 |  |
|      100 |  3702 | `	sxi32 posIdx = 0;` |
|        - |  3703 | `	sxu32 i;` |
|        - |  3704 | `	char zErrMsg[256];` |
|      100 |  3705 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3706 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3707 | `		aSlot[i] = -2;` |
|      100 |  3708 | `	}` |
|      290 |  3709 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3710 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3711 | `			/* Named argument — find formal by name */` |
|      184 |  3712 | `			int found = 0;` |
|        - |  3713 | `			sxu32 k;` |
|      304 |  3714 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3715 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3716 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3717 | `						pMap->aNames[i].zString,` |
|      402 |  3718 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3719 | `					if( aUsed[k] ){` |
|        7 |  3720 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3721 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3722 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3723 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3724 | `						return PH7_ABORT;` |
|        - |  3725 | `					}` |
|      168 |  3726 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3727 | `					aUsed[k] = 1;` |
|      168 |  3728 | `					found = 1;` |
|      168 |  3729 | `					break;` |
|        - |  3730 | `				}` |
|       62 |  3731 | `			}` |
|      180 |  3732 | `			if( !found ){` |
|       14 |  3733 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3734 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3735 | `				}else{` |
|        4 |  3736 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3737 | `						"Unknown named parameter $%.*s",` |
|        2 |  3738 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3739 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3740 | `					return PH7_ABORT;` |
|        - |  3741 | `				}` |
|        5 |  3742 | `			}` |
|       90 |  3743 | `		}else{` |
|        - |  3744 | `			/* Positional argument */` |
|       16 |  3745 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3746 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3747 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3748 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3749 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3750 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3751 | `					return PH7_ABORT;` |
|        - |  3752 | `				}` |
|       16 |  3753 | `				aSlot[i] = posIdx;` |
|       16 |  3754 | `				aUsed[posIdx] = 1;` |
|        7 |  3755 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3756 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3757 | `			}` |
|       16 |  3758 | `			posIdx++;` |
|        - |  3759 | `		}` |
|       97 |  3760 | `	}` |
|       93 |  3761 | `	return SXRET_OK;` |
|       51 |  3762 |  |
|        - |  3763 | `/*` |
|        - |  3764 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3765 | ` *` |
|        - |  3766 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3767 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3768 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3769 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3770 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3771 | ` * then the program execution is halted.` |
|        - |  3772 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3773 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3774 | ` * or to reset the VM to it's initial state.` |
|        - |  3775 | ` */` |
|    42412 |  3776 | `static sxi32 VmByteCodeExec(` |
|        - |  3777 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3778 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3779 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3780 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3781 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3782 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3783 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3784 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3785 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3786 | `	)` |
|        2 |  3787 |  |
|        - |  3788 | `	VmInstr *pInstr;` |
|        - |  3789 | `	ph7_value *pTos;` |
|        - |  3790 | `	SySet aArg;` |
|        - |  3791 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3792 | `	sxi32 pc;` |
|        - |  3793 | `	sxi32 rc;` |
|        - |  3794 | `	/* Argument container */` |
|    42414 |  3795 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    42414 |  3796 | `	if( nTos < 0 ){` |
|    39676 |  3797 | `		pTos = &pStack[-1];` |
|    19839 |  3798 | `	}else{` |
|     2740 |  3799 | `		pTos = &pStack[nTos];` |
|        - |  3800 | `	}` |
|    42414 |  3801 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    42414 |  3802 | `	pc = nPc;` |
|        - |  3803 | `/*` |
|        - |  3804 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3805 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3806 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3807 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3808 | ` */` |
|        - |  3809 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3810 | `	{ \` |
|        - |  3811 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3812 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3813 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3814 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3815 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3816 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3817 | `				break; \` |
|        - |  3818 | `			} \` |
|        - |  3819 | `			goto Exception; \` |
|        - |  3820 | `		} \` |
|        - |  3821 | `	}` |
|        - |  3822 | `	/* Execute as much as we can */` |
|  5739280 |  3823 | `	for(;;){` |
|        - |  3824 | `		/* Fetch the instruction to execute */` |
| 11477858 |  3825 | `		pInstr = &aInstr[pc];` |
| 11477858 |  3826 | `		rc = SXRET_OK;` |
|        - |  3827 | `/*` |
|        - |  3828 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  3829 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  3830 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  3831 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  3832 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  3833 | ` */` |
| 11477858 |  3834 | `		switch(pInstr->iOp){` |
|        - |  3835 | `/*` |
|        - |  3836 | ` * DONE: P1 * *` |
|        - |  3837 | ` *` |
|        - |  3838 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  3839 | ` * and return immediately.` |
|        - |  3840 | ` */` |
|    20862 |  3841 | `case PH7_OP_DONE:` |
|        - |  3842 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  3843 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  3844 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  3845 | `	 * callback trampolines, and the main script. */` |
|    41726 |  3846 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      264 |  3847 | `		ph7_value *pRetVal = 0;` |
|      264 |  3848 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      174 |  3849 | `			pRetVal = pTos;` |
|       86 |  3850 | `		}` |
|      264 |  3851 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      264 |  3852 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      258 |  3853 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  3854 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  3855 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  3856 | `				pTos--;` |
|      ! 0 |  3857 | `			}` |
|      ! 0 |  3858 | `			goto Exception;` |
|        - |  3859 | `		}` |
|        - |  3860 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  3861 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  3862 | `		 * defensively we clear the pointer after a successful check). */` |
|      258 |  3863 | `		pEnforceRetFunc = 0;` |
|      128 |  3864 | `	}` |
|    41720 |  3865 | `	if( pInstr->iP1 ){` |
|        - |  3866 | `#ifdef UNTRUST` |
|        - |  3867 | `		if( pTos < pStack ){` |
|        - |  3868 | `			goto Abort;` |
|        - |  3869 | `		}` |
|        - |  3870 | `#endif` |
|    25252 |  3871 | `		if( pLastRef ){` |
|    15624 |  3872 | `			*pLastRef = pTos->nIdx;` |
|     7811 |  3873 | `		}` |
|    25252 |  3874 | `		if( pResult ){` |
|        - |  3875 | `			/* Execution result */` |
|    23908 |  3876 | `			PH7_MemObjStore(pTos,pResult);` |
|    11953 |  3877 | `		}` |
|    25252 |  3878 | `		VmPopOperand(&pTos,1);` |
|    29095 |  3879 | `	}else if( pLastRef ){` |
|        - |  3880 | `		/* Nothing referenced */` |
|     1718 |  3881 | `		*pLastRef = SXU32_HIGH;` |
|      858 |  3882 | `	}` |
|        - |  3883 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  3884 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  3885 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  3886 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  3887 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  3888 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  3889 | `	 * block can override it.` |
|        - |  3890 | `	 */` |
|    41722 |  3891 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  3892 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  3893 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  3894 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  3895 | `		pExc->pFrame = 0;` |
|        3 |  3896 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  3897 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  3898 | `			pExc->iFinallyDone = 1;` |
|        - |  3899 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  3900 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  3901 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  3902 | `				goto Abort;` |
|        - |  3903 | `			}` |
|        1 |  3904 | `		}` |
|        1 |  3905 | `	}` |
|    41720 |  3906 | `	goto Done;` |
|        - |  3907 | `/*` |
|        - |  3908 | ` * HALT: P1 * *` |
|        - |  3909 | ` *` |
|        - |  3910 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  3911 | ` * and abort immediately.` |
|        - |  3912 | ` */` |
|        4 |  3913 | `case PH7_OP_HALT:` |
|        9 |  3914 | `	if( pInstr->iP1 ){` |
|        - |  3915 | `#ifdef UNTRUST` |
|        - |  3916 | `		if( pTos < pStack ){` |
|        - |  3917 | `			goto Abort;` |
|        - |  3918 | `		}` |
|        - |  3919 | `#endif` |
|        9 |  3920 | `		if( pLastRef ){` |
|      ! 0 |  3921 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  3922 | `		}` |
|        9 |  3923 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  3924 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  3925 | `				/* Output the exit message */` |
|        7 |  3926 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  3927 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  3928 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  3929 | `			}` |
|        7 |  3930 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  3931 | `			/* Record exit status */` |
|        5 |  3932 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  3933 | `		}` |
|        9 |  3934 | `		VmPopOperand(&pTos,1);` |
|        4 |  3935 | `	}else if( pLastRef ){` |
|        - |  3936 | `		/* Nothing referenced */` |
|      ! 0 |  3937 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  3938 | `	}` |
|        - |  3939 | `	/* Check if we're in an included file context */` |
|        9 |  3940 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  3941 | `		/* Terminate the entire process */` |
|        9 |  3942 | `		exit(pVm->iExitStatus);` |
|        - |  3943 | `	}` |
|      ! 0 |  3944 | `	goto Abort;` |
|        - |  3945 | `/*` |
|        - |  3946 | ` * JMP: * P2 *` |
|        - |  3947 | ` *` |
|        - |  3948 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  3949 | ` * the one at index P2 from the beginning of the program.` |
|        - |  3950 | ` */` |
|   244944 |  3951 | `case PH7_OP_JMP:` |
|   489934 |  3952 | `	pc = pInstr->iP2 - 1;` |
|   489934 |  3953 | `	break;` |
|        - |  3954 | `/*` |
|        - |  3955 | ` * JZ: P1 P2 *` |
|        - |  3956 | ` *` |
|        - |  3957 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  3958 | ` * entry in the stack if P1 is zero.` |
|        - |  3959 | ` */` |
|   580775 |  3960 | `case PH7_OP_JZ:` |
|        - |  3961 | `#ifdef UNTRUST` |
|        - |  3962 | `	if( pTos < pStack ){` |
|        - |  3963 | `		goto Abort;` |
|        - |  3964 | `	}` |
|        - |  3965 | `#endif` |
|        - |  3966 | `	/* Get a boolean value */` |
|  1161640 |  3967 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  3968 | `		PH7_MemObjToBool(pTos);` |
|       85 |  3969 | `	}` |
|  1161640 |  3970 | `	if( !pTos->x.iVal ){` |
|        - |  3971 | `		/* Take the jump */` |
|   595734 |  3972 | `		pc = pInstr->iP2 - 1;` |
|   297866 |  3973 | `	}` |
|  1161640 |  3974 | `	if( !pInstr->iP1 ){` |
|   922308 |  3975 | `		VmPopOperand(&pTos,1);` |
|   461175 |  3976 | `	}` |
|  1161640 |  3977 | `	break;` |
|        - |  3978 | `/*` |
|        - |  3979 | ` * JNZ: P1 P2 *` |
|        - |  3980 | ` *` |
|        - |  3981 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  3982 | ` * entry in the stack if P1 is zero.` |
|        - |  3983 | ` */` |
|    60769 |  3984 | `case PH7_OP_JNZ:` |
|        - |  3985 | `#ifdef UNTRUST` |
|        - |  3986 | `	if( pTos < pStack ){` |
|        - |  3987 | `		goto Abort;` |
|        - |  3988 | `	}` |
|        - |  3989 | `#endif` |
|        - |  3990 | `	/* Get a boolean value */` |
|   121540 |  3991 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  3992 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  3993 | `	}` |
|   121540 |  3994 | `	if( pTos->x.iVal ){` |
|        - |  3995 | `		/* Take the jump */` |
|     5384 |  3996 | `		pc = pInstr->iP2 - 1;` |
|     2691 |  3997 | `	}` |
|   121540 |  3998 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  3999 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4000 | `	}` |
|   121540 |  4001 | `	break;` |
|        - |  4002 | `/*` |
|        - |  4003 | ` * NOOP: * * *` |
|        - |  4004 | ` *` |
|        - |  4005 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4006 | ` * destination.` |
|        - |  4007 | ` */` |
|      ! 0 |  4008 | `case PH7_OP_NOOP:` |
|      ! 0 |  4009 | `	break;` |
|        - |  4010 | `/*` |
|        - |  4011 | ` * POP: P1 * *` |
|        - |  4012 | ` *` |
|        - |  4013 | ` * Pop P1 elements from the operand stack.` |
|        - |  4014 | ` */` |
|   448533 |  4015 | `case PH7_OP_POP: {` |
|   897112 |  4016 | `	sxi32 n = pInstr->iP1;` |
|   897112 |  4017 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4018 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       17 |  4019 | `		n = (sxi32)(pTos - pStack);` |
|        8 |  4020 | `	}` |
|   897112 |  4021 | `	VmPopOperand(&pTos,n);` |
|   897112 |  4022 | `	break;` |
|        - |  4023 | `				 }` |
|        - |  4024 | `/*` |
|        - |  4025 | ` * DUP: * * *` |
|        - |  4026 | ` *` |
|        - |  4027 | ` * Duplicate the top of the stack.` |
|        - |  4028 | ` */` |
|       41 |  4029 | `case PH7_OP_DUP:` |
|        - |  4030 | `#ifdef UNTRUST` |
|        - |  4031 | `	if( pTos < pStack ){` |
|        - |  4032 | `		goto Abort;` |
|        - |  4033 | `	}` |
|        - |  4034 | `#endif` |
|       84 |  4035 | `	pTos++;` |
|       84 |  4036 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4037 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4038 | `	break;` |
|        - |  4039 | `/*` |
|        - |  4040 | ` * NSSWITCH: * * P3` |
|        - |  4041 | ` *` |
|        - |  4042 | ` * Switch the active namespace at runtime.` |
|        - |  4043 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4044 | ` */` |
|     7529 |  4045 | `case PH7_OP_NSSWITCH:` |
|    15060 |  4046 | `	SyBlobReset(&pVm->sNamespace);` |
|    15060 |  4047 | `	if( pInstr->p3 ){` |
|       98 |  4048 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4049 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4050 | `	}` |
|        - |  4051 | `	/* Clear namespace-scoped use-const imports */` |
|    15060 |  4052 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15060 |  4053 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15060 |  4054 | `	break;` |
|        - |  4055 | `/* OP_USECONST P1 * P3` |
|        - |  4056 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4057 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4058 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4059 | ` */` |
|        7 |  4060 | `case PH7_OP_USECONST: {` |
|       16 |  4061 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4062 | `	if( azPair ){` |
|       16 |  4063 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4064 | `	}` |
|       16 |  4065 | `	break;` |
|        - |  4066 | `				}` |
|        - |  4067 | `/*` |
|        - |  4068 | ` * CVT_INT: * * *` |
|        - |  4069 | ` *` |
|        - |  4070 | ` * Force the top of the stack to be an integer.` |
|        - |  4071 | ` */` |
|       78 |  4072 | `case PH7_OP_CVT_INT:` |
|        - |  4073 | `#ifdef UNTRUST` |
|        - |  4074 | `	if( pTos < pStack ){` |
|        - |  4075 | `		goto Abort;` |
|        - |  4076 | `	}` |
|        - |  4077 | `#endif` |
|      158 |  4078 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4079 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4080 | `	}` |
|        - |  4081 | `	/* Invalidate any prior representation */` |
|      158 |  4082 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      158 |  4083 | `	break;` |
|        - |  4084 | `/*` |
|        - |  4085 | ` * CVT_REAL: * * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Force the top of the stack to be a real.` |
|        - |  4088 | ` */` |
|        5 |  4089 | `case PH7_OP_CVT_REAL:` |
|        - |  4090 | `#ifdef UNTRUST` |
|        - |  4091 | `	if( pTos < pStack ){` |
|        - |  4092 | `		goto Abort;` |
|        - |  4093 | `	}` |
|        - |  4094 | `#endif` |
|       11 |  4095 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4096 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4097 | `	}` |
|        - |  4098 | `	/* Invalidate any prior representation */` |
|       11 |  4099 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4100 | `	break;` |
|        - |  4101 | `/*` |
|        - |  4102 | ` * CVT_STR: * * *` |
|        - |  4103 | ` *` |
|        - |  4104 | ` * Force the top of the stack to be a string.` |
|        - |  4105 | ` */` |
|      146 |  4106 | `case PH7_OP_CVT_STR:` |
|        - |  4107 | `#ifdef UNTRUST` |
|        - |  4108 | `	if( pTos < pStack ){` |
|        - |  4109 | `		goto Abort;` |
|        - |  4110 | `	}` |
|        - |  4111 | `#endif` |
|      294 |  4112 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      294 |  4113 | `		PH7_MemObjToString(pTos);` |
|      146 |  4114 | `	}` |
|      294 |  4115 | `	break;` |
|        - |  4116 | `/*` |
|        - |  4117 | ` * CVT_BOOL: * * *` |
|        - |  4118 | ` *` |
|        - |  4119 | ` * Force the top of the stack to be a boolean.` |
|        - |  4120 | ` */` |
|        5 |  4121 | `case PH7_OP_CVT_BOOL:` |
|        - |  4122 | `#ifdef UNTRUST` |
|        - |  4123 | `	if( pTos < pStack ){` |
|        - |  4124 | `		goto Abort;` |
|        - |  4125 | `	}` |
|        - |  4126 | `#endif` |
|       11 |  4127 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4128 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4129 | `	}` |
|       11 |  4130 | `	break;` |
|        - |  4131 | `/*` |
|        - |  4132 | ` * CVT_NULL: * * *` |
|        - |  4133 | ` *` |
|        - |  4134 | ` * Nullify the top of the stack.` |
|        - |  4135 | ` */` |
|        3 |  4136 | `case PH7_OP_CVT_NULL:` |
|        - |  4137 | `#ifdef UNTRUST` |
|        - |  4138 | `	if( pTos < pStack ){` |
|        - |  4139 | `		goto Abort;` |
|        - |  4140 | `	}` |
|        - |  4141 | `#endif` |
|        7 |  4142 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4143 | `	break;` |
|        - |  4144 | `/*` |
|        - |  4145 | ` * CVT_NUMC: * * *` |
|        - |  4146 | ` *` |
|        - |  4147 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4148 | ` */` |
|      ! 0 |  4149 | `case PH7_OP_CVT_NUMC:` |
|        - |  4150 | `#ifdef UNTRUST` |
|        - |  4151 | `	if( pTos < pStack ){` |
|        - |  4152 | `		goto Abort;` |
|        - |  4153 | `	}` |
|        - |  4154 | `#endif` |
|        - |  4155 | `	/* Force a numeric cast */` |
|      ! 0 |  4156 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4157 | `	break;` |
|        - |  4158 | `/*` |
|        - |  4159 | ` * CVT_ARRAY: * * *` |
|        - |  4160 | ` *` |
|        - |  4161 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4162 | ` */` |
|       10 |  4163 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4164 | `#ifdef UNTRUST` |
|        - |  4165 | `	if( pTos < pStack ){` |
|        - |  4166 | `		goto Abort;` |
|        - |  4167 | `	}` |
|        - |  4168 | `#endif` |
|        - |  4169 | `	/* Force a hashmap cast */` |
|       21 |  4170 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4171 | `	if( rc != SXRET_OK ){` |
|        - |  4172 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4173 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4174 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4175 | `	}` |
|       21 |  4176 | `	break;` |
|        - |  4177 | `/*` |
|        - |  4178 | ` * CVT_OBJ: * * *` |
|        - |  4179 | ` *` |
|        - |  4180 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4181 | ` */` |
|        8 |  4182 | `case PH7_OP_CVT_OBJ:` |
|        - |  4183 | `#ifdef UNTRUST` |
|        - |  4184 | `	if( pTos < pStack ){` |
|        - |  4185 | `		goto Abort;` |
|        - |  4186 | `	}` |
|        - |  4187 | `#endif` |
|       17 |  4188 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4189 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4190 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4191 | `	}` |
|       17 |  4192 | `	break;` |
|        - |  4193 | `/*` |
|        - |  4194 | ` * ERR_CTRL * * *` |
|        - |  4195 | ` *` |
|        - |  4196 | ` * Error control operator.` |
|        - |  4197 | ` */` |
|    15462 |  4198 | `case PH7_OP_ERR_CTRL:` |
|        - |  4199 | `	/*` |
|        - |  4200 | `	 * TICKET 1433-038:` |
|        - |  4201 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4202 | `	 * use the public API,to control error output.` |
|        - |  4203 | `	 */` |
|    30924 |  4204 | `	break;` |
|        - |  4205 | `/*` |
|        - |  4206 | ` * IS_A * * *` |
|        - |  4207 | ` *` |
|        - |  4208 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4209 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4210 | ` * holding a class name or an object).` |
|        - |  4211 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4212 | ` */` |
|       42 |  4213 | `case PH7_OP_IS_A:{` |
|       86 |  4214 | `	ph7_value *pNos = &pTos[-1];` |
|       86 |  4215 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4216 | `#ifdef UNTRUST` |
|        - |  4217 | `	if( pNos < pStack ){` |
|        - |  4218 | `		goto Abort;` |
|        - |  4219 | `	}` |
|        - |  4220 | `#endif` |
|       86 |  4221 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|       84 |  4222 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       84 |  4223 | `		ph7_class *pClass = 0;` |
|        - |  4224 | `		/* Extract the target class */` |
|       84 |  4225 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4226 | `			/* Instance already loaded */` |
|      ! 0 |  4227 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       84 |  4228 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       84 |  4229 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|       84 |  4230 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4231 | `			/* Handle self/static/parent keywords */` |
|       84 |  4232 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4233 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       82 |  4234 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4235 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       81 |  4236 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4237 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4238 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4239 | `					pClass = pSelf->pBase;` |
|        2 |  4240 | `				}` |
|        3 |  4241 | `			}else{` |
|       74 |  4242 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4243 | `			}` |
|       41 |  4244 | `		}` |
|       84 |  4245 | `		if( pClass ){` |
|        - |  4246 | `			/* Perform the query */` |
|       84 |  4247 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       41 |  4248 | `		}` |
|       41 |  4249 | `	}` |
|        - |  4250 | `	/* Push result */` |
|       86 |  4251 | `	VmPopOperand(&pTos,1);` |
|       86 |  4252 | `	PH7_MemObjRelease(pTos);` |
|       86 |  4253 | `	pTos->x.iVal = iRes;` |
|       86 |  4254 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       86 |  4255 | `	break;` |
|        - |  4256 | `				 }` |
|        - |  4257 |  |
|        - |  4258 | `/*` |
|        - |  4259 | ` * LOADC P1 P2 *` |
|        - |  4260 | ` *` |
|        - |  4261 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4262 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4263 | ` */` |
|   978837 |  4264 | `case PH7_OP_LOADC: {` |
|        - |  4265 | `	ph7_value *pObj;` |
|        - |  4266 | `	/* Reserve a room */` |
|  1957720 |  4267 | `	pTos++;` |
|  2927126 |  4268 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1957720 |  4269 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4270 | `			SyHashEntry *pEntry;` |
|        - |  4271 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4272 | `			{` |
|        - |  4273 | `				SyHashEntry *pConstImport;` |
|    28430 |  4274 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    18952 |  4275 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18954 |  4276 | `				if( pConstImport ){` |
|       11 |  4277 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4278 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4279 | `					if( pEntry ){` |
|       11 |  4280 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4281 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4282 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4283 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4284 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4285 | `						break;` |
|        - |  4286 | `					}` |
|        - |  4287 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4288 | `				}` |
|        - |  4289 | `			}` |
|        - |  4290 | `			/* Candidate for expansion via user defined callbacks */` |
|    18944 |  4291 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    18944 |  4292 | `			if( pEntry ){` |
|    18940 |  4293 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4294 | `				/* Set a NULL default value */` |
|    18940 |  4295 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    18940 |  4296 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4297 | `				/* Invoke the callback and deal with the expanded value */` |
|    18940 |  4298 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4299 | `				/* Mark as constant */` |
|    18940 |  4300 | `				pTos->nIdx = SXU32_HIGH;` |
|    18940 |  4301 | `				break;` |
|        - |  4302 | `			}` |
|        - |  4303 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4304 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4305 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4306 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4307 | `			{` |
|        6 |  4308 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        6 |  4309 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4310 | `				sxu32 j;` |
|        6 |  4311 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       14 |  4312 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|        9 |  4313 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|        5 |  4314 | `				}` |
|        6 |  4315 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4316 | `					/* Try current_namespace\name */` |
|      ! 0 |  4317 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4318 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4319 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4320 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4321 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4322 | `					if( pEntry ){` |
|      ! 0 |  4323 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4324 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4325 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4326 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4327 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4328 | `						break;` |
|        - |  4329 | `					}` |
|        - |  4330 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4331 | `				}` |
|        6 |  4332 | `				if( isQualified ){` |
|        - |  4333 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4334 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4335 | `					SyBlob sErr;` |
|        3 |  4336 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4337 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4338 | `					if( pErrFile ){` |
|        3 |  4339 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4340 | `					}` |
|        3 |  4341 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4342 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4343 | `					SyBlobRelease(&sErr);` |
|        3 |  4344 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4345 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4346 | `					goto LoadC_Done;` |
|        - |  4347 | `				}` |
|        - |  4348 | `			}` |
|        1 |  4349 | `		}` |
|  1938770 |  4350 | `		PH7_MemObjLoad(pObj,pTos);` |
|   969408 |  4351 | `	}else{` |
|        - |  4352 | `		/* Set a NULL value */` |
|      ! 0 |  4353 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4354 | `	}` |
|   969363 |  4355 | `LoadC_Done:` |
|        - |  4356 | `	/* Mark as constant */` |
|  1938772 |  4357 | `	pTos->nIdx = SXU32_HIGH;` |
|  1938772 |  4358 | `	break;` |
|        - |  4359 | `				  }` |
|        - |  4360 | `/*` |
|        - |  4361 | ` * LOAD: P1 * P3` |
|        - |  4362 | ` *` |
|        - |  4363 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4364 | ` * from the P3 operand.` |
|        - |  4365 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4366 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4367 | ` */` |
|  1540569 |  4368 | `case PH7_OP_LOAD:{` |
|        - |  4369 | `	ph7_value *pObj;` |
|        - |  4370 | `	SyString sName;` |
|  3081360 |  4371 | `	if( pInstr->p3 == 0 ){` |
|        - |  4372 | `		/* Take the variable name from the top of the stack */` |
|        - |  4373 | `#ifdef UNTRUST` |
|        - |  4374 | `		if( pTos < pStack ){` |
|        - |  4375 | `			goto Abort;` |
|        - |  4376 | `		}` |
|        - |  4377 | `#endif` |
|        - |  4378 | `		/* Force a string cast */` |
|       19 |  4379 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4380 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4381 | `		}` |
|       19 |  4382 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4383 | `	}else{` |
|  3081342 |  4384 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4385 | `		/* Reserve a room for the target object */` |
|  3081342 |  4386 | `		pTos++;` |
|        - |  4387 | `	}` |
|        - |  4388 | `	/* Extract the requested memory object */` |
|  3081360 |  4389 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3081360 |  4390 | `	if( pObj == 0 ){` |
|       28 |  4391 | `		if( pInstr->iP1 ){` |
|        - |  4392 | `			/* Variable not found,load NULL */` |
|       28 |  4393 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4394 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4395 | `			}else{` |
|       28 |  4396 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4397 | `			}` |
|       28 |  4398 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1540584 |  4399 | `			break;` |
|      ! 0 |  4400 | `		}else{` |
|        - |  4401 | `			/* Fatal error */` |
|      ! 0 |  4402 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4403 | `			goto Abort;` |
|        - |  4404 | `		}` |
|        - |  4405 | `	}` |
|        - |  4406 | `	/* Load variable contents */` |
|  3081334 |  4407 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3081334 |  4408 | `	pTos->nIdx = pObj->nIdx;` |
|  3081334 |  4409 | `	break;` |
|        - |  4410 | `				   }` |
|        - |  4411 | `/*` |
|        - |  4412 | ` * LOAD_MAP P1 * *` |
|        - |  4413 | ` *` |
|        - |  4414 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4415 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4416 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4417 | ` */` |
|    21806 |  4418 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4419 | `	ph7_hashmap *pMap;` |
|        - |  4420 | `	/* Allocate a new hashmap instance */` |
|    43614 |  4421 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    43614 |  4422 | `	if( pMap == 0 ){` |
|      ! 0 |  4423 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4424 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4425 | `		goto Abort;` |
|        - |  4426 | `	}` |
|    43614 |  4427 | `	if( pInstr->iP1 > 0 ){` |
|     2390 |  4428 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|        - |  4429 | `		/* Perform the insertion */` |
|     7366 |  4430 | `		while( pEntry < pTos ){` |
|     4978 |  4431 | `			if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4432 | `				/* Insertion by reference */` |
|      142 |  4433 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4434 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4435 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4436 | `					);` |
|       48 |  4437 | `			}else{` |
|        - |  4438 | `				/* Standard insertion */` |
|     7325 |  4439 | `				PH7_HashmapInsert(pMap,` |
|     4882 |  4440 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2441 |  4441 | `					&pEntry[1]` |
|        - |  4442 | `				);` |
|        - |  4443 | `			}` |
|        - |  4444 | `			/* Next pair on the stack */` |
|     4978 |  4445 | `			pEntry += 2;` |
|        2 |  4446 | `		}` |
|        - |  4447 | `		/* Pop P1 elements */` |
|     2390 |  4448 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     1194 |  4449 | `	}` |
|        - |  4450 | `	/* Push the hashmap */` |
|    43614 |  4451 | `	pTos++;` |
|    43614 |  4452 | `	pTos->nIdx = SXU32_HIGH;` |
|    43614 |  4453 | `	pTos->x.pOther = pMap;` |
|    43614 |  4454 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    43614 |  4455 | `	break;` |
|        - |  4456 | `					  }` |
|        - |  4457 | `/*` |
|        - |  4458 | ` * LOAD_LIST: P1 * *` |
|        - |  4459 | ` *` |
|        - |  4460 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4461 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4462 | ` * Caveats:` |
|        - |  4463 | ` *  This implementation support only a single nesting level.` |
|        - |  4464 | ` */` |
|       48 |  4465 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4466 | `	ph7_value *pEntry;` |
|       98 |  4467 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4468 | `		/* Empty list,break immediately */` |
|      ! 0 |  4469 | `		break;` |
|        - |  4470 | `	}` |
|       98 |  4471 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4472 | `#ifdef UNTRUST` |
|        - |  4473 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4474 | `		goto Abort;` |
|        - |  4475 | `	}` |
|        - |  4476 | `#endif` |
|       98 |  4477 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4478 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4479 | `		ph7_hashmap_node *pNode;` |
|        - |  4480 | `		ph7_value sKey,*pObj;` |
|        - |  4481 | `		/* Start Copying */` |
|       91 |  4482 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4483 | `		while( pEntry <= pTos ){` |
|      193 |  4484 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4485 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4486 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4487 | `					if( rc == SXRET_OK ){` |
|        - |  4488 | `						/* Store node value */` |
|      165 |  4489 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4490 | `					}else{` |
|        - |  4491 | `						/* Undefined array key */` |
|        - |  4492 | `						char zMsg[128];` |
|      ! 0 |  4493 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4494 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4495 | `						PH7_MemObjRelease(pObj);` |
|        - |  4496 | `					}` |
|       82 |  4497 | `				}` |
|       82 |  4498 | `			}` |
|      193 |  4499 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4500 | `			pEntry++;` |
|        1 |  4501 | `		}` |
|       46 |  4502 | `	}else{` |
|        - |  4503 | `		/* Source is not an array */` |
|        - |  4504 | `		ph7_value *pObj;` |
|       18 |  4505 | `		while( pEntry <= pTos ){` |
|       12 |  4506 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4507 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4508 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4509 | `				}` |
|        5 |  4510 | `			}` |
|       12 |  4511 | `			pEntry++;` |
|        2 |  4512 | `		}` |
|        8 |  4513 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4514 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4515 | `			const char *zType = "unknown";` |
|        3 |  4516 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4517 | `			char zMsg[256];` |
|        3 |  4518 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4519 | `				zType = "string";` |
|        1 |  4520 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4521 | `				zType = "int";` |
|      ! 0 |  4522 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4523 | `				zType = "float";` |
|      ! 0 |  4524 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4525 | `				zType = "object";` |
|      ! 0 |  4526 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4527 | `				zType = "resource";` |
|      ! 0 |  4528 | `			}` |
|        3 |  4529 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4530 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4531 | `		}` |
|        - |  4532 | `	}` |
|       98 |  4533 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4534 | `	break;` |
|        - |  4535 | `					   }` |
|        - |  4536 | `/*` |
|        - |  4537 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4538 | ` *` |
|        - |  4539 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4540 | ` * from the stack.` |
|        - |  4541 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4542 | ` * instead.` |
|        - |  4543 | ` */` |
|   247189 |  4544 | `case PH7_OP_LOAD_IDX: {` |
|   494424 |  4545 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   494424 |  4546 | `	ph7_hashmap *pMap = 0;` |
|        - |  4547 | `	ph7_value *pIdx;` |
|   494424 |  4548 | `	pIdx = 0;` |
|   494424 |  4549 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4550 | `		if( !pInstr->iP2){` |
|        - |  4551 | `			/* No available index,load NULL */` |
|      ! 0 |  4552 | `			if( pTos >= pStack ){` |
|      ! 0 |  4553 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4554 | `			}else{` |
|        - |  4555 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4556 | `				pTos++;` |
|      ! 0 |  4557 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4558 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4559 | `			}` |
|        - |  4560 | `			/* Emit a notice */` |
|      ! 0 |  4561 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4562 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4563 | `			break;` |
|        - |  4564 | `		}` |
|      ! 0 |  4565 | `	}else{` |
|   494424 |  4566 | `		pIdx = pTos;` |
|   494424 |  4567 | `		pTos--;` |
|        - |  4568 | `	}` |
|   494424 |  4569 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4570 | `		/* String access */` |
|   385116 |  4571 | `		if( pIdx ){` |
|        - |  4572 | `			sxu32 nOfft;` |
|   385116 |  4573 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4574 | `				/* Force an int cast */` |
|      ! 0 |  4575 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4576 | `			}` |
|   385116 |  4577 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   385116 |  4578 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4579 | `				/* Invalid offset,load null */` |
|      ! 0 |  4580 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4581 | `			}else{` |
|   385116 |  4582 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   385116 |  4583 | `				int c = zData[nOfft];` |
|   385116 |  4584 | `				PH7_MemObjRelease(pTos);` |
|   385116 |  4585 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   385116 |  4586 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4587 | `			}` |
|   192581 |  4588 | `		}else{` |
|        - |  4589 | `			/* No available index,load NULL */` |
|      ! 0 |  4590 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4591 | `		}` |
|   385116 |  4592 | `		break;` |
|        - |  4593 | `	}` |
|   109310 |  4594 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4595 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4596 | `			ph7_value *pObj;` |
|        3 |  4597 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4598 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4599 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4600 | `			}` |
|        1 |  4601 | `		}` |
|        1 |  4602 | `	}` |
|   109310 |  4603 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   109310 |  4604 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   109310 |  4605 | `		if( pInstr->iP2 == 1 ){` |
|        - |  4606 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4607 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4608 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4609 | `			 * NOT separate — that would defeat COW on every element read. */` |
|      883 |  4610 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      441 |  4611 | `		}` |
|        - |  4612 | `		/* Point to the hashmap */` |
|   109310 |  4613 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   109310 |  4614 | `		if( pIdx ){` |
|        - |  4615 | `			/* Load the desired entry */` |
|   109310 |  4616 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    54654 |  4617 | `		}` |
|   109310 |  4618 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4619 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4620 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4621 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4622 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4623 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  4624 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  4625 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  4626 | `			 * correct for the outermost write. */` |
|       19 |  4627 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  4628 | `			if( !needWrite && pNode ){` |
|       13 |  4629 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  4630 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  4631 | `					needWrite = 1;` |
|        3 |  4632 | `				}` |
|        6 |  4633 | `			}` |
|       19 |  4634 | `			if( needWrite ){` |
|       13 |  4635 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  4636 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  4637 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  4638 | `					 * into the new map's storage. */` |
|        7 |  4639 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  4640 | `					if( pIdx ){` |
|        7 |  4641 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  4642 | `					}` |
|        3 |  4643 | `				}` |
|        6 |  4644 | `			}` |
|        9 |  4645 | `		}` |
|   109310 |  4646 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3) ){` |
|        - |  4647 | `			/* Create a new empty entry */` |
|      273 |  4648 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  4649 | `			if( rc == SXRET_OK ){` |
|        - |  4650 | `				/* Point to the last inserted entry */` |
|      273 |  4651 | `				pNode = pMap->pLast;` |
|      136 |  4652 | `			}` |
|      136 |  4653 | `		}` |
|    54654 |  4654 | `	}` |
|   109310 |  4655 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  4656 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  4657 | `		char zMsg[128];` |
|      ! 0 |  4658 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  4659 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4660 | `		}` |
|      ! 0 |  4661 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  4662 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4663 | `	}` |
|   109310 |  4664 | `	if( pIdx ){` |
|   109310 |  4665 | `		PH7_MemObjRelease(pIdx);` |
|    54654 |  4666 | `	}` |
|   109310 |  4667 | `	if( rc == SXRET_OK ){` |
|        - |  4668 | `		/* Load entry contents */` |
|    48542 |  4669 | `		if( pMap->iRef < 2 ){` |
|        - |  4670 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  4671 | `			 * of the entry value,rather than pointing to it.` |
|        - |  4672 | `			 */` |
|       24 |  4673 | `			pTos->nIdx = SXU32_HIGH;` |
|       24 |  4674 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       13 |  4675 | `		}else{` |
|    48520 |  4676 | `			pTos->nIdx = pNode->nValIdx;` |
|    48520 |  4677 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    48520 |  4678 | `			PH7_HashmapUnref(pMap);` |
|        - |  4679 | `		}` |
|    24272 |  4680 | `	}else{` |
|        - |  4681 | `		/* No such entry,load NULL */` |
|    60770 |  4682 | `		PH7_MemObjRelease(pTos);` |
|    60770 |  4683 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  4684 | `	}` |
|   109310 |  4685 | `	break;` |
|        - |  4686 | `					  }` |
|        - |  4687 | `/*` |
|        - |  4688 | ` * LOAD_CLOSURE * * P3` |
|        - |  4689 | ` *` |
|        - |  4690 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  4691 | ` * name in the stack.` |
|        - |  4692 | ` */` |
|       47 |  4693 | `case PH7_OP_LOAD_CLOSURE:{` |
|       96 |  4694 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       96 |  4695 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  4696 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  4697 | `		ph7_vm_func *pClosure;` |
|        - |  4698 | `		char *zName;` |
|        - |  4699 | `		sxu32 mLen;` |
|        - |  4700 | `		sxu32 n;` |
|        - |  4701 | `		/* Create a new VM function */` |
|       96 |  4702 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  4703 | `		/* Generate an unique closure name */` |
|       96 |  4704 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       96 |  4705 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  4706 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  4707 | `			goto Abort;` |
|        - |  4708 | `		}` |
|       96 |  4709 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       96 |  4710 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  4711 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  4712 | `		}` |
|        - |  4713 | `		/* Zero the stucture */` |
|       96 |  4714 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  4715 | `		/* Perform a structure assignment on read-only items */` |
|       96 |  4716 | `		pClosure->aArgs = pFunc->aArgs;` |
|       96 |  4717 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       96 |  4718 | `		pClosure->aStatic = pFunc->aStatic;` |
|       96 |  4719 | `		pClosure->iFlags = pFunc->iFlags;` |
|       96 |  4720 | `		pClosure->pUserData = pFunc->pUserData;` |
|       96 |  4721 | `		pClosure->sSignature = pFunc->sSignature;` |
|       96 |  4722 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       96 |  4723 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       96 |  4724 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       96 |  4725 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       96 |  4726 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  4727 | `		/* Register the closure */` |
|       96 |  4728 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  4729 | `		/* Set up closure environment */` |
|       96 |  4730 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       96 |  4731 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      256 |  4732 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  4733 | `			ph7_value *pValue;` |
|      162 |  4734 | `			pEnv = &aEnv[n];` |
|      162 |  4735 | `			sEnv.sName  = pEnv->sName;` |
|      162 |  4736 | `			sEnv.iFlags = pEnv->iFlags;` |
|      162 |  4737 | `			sEnv.nIdx = SXU32_HIGH;` |
|      162 |  4738 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      162 |  4739 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  4740 | `				/* Pass by reference */` |
|      ! 0 |  4741 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  4742 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  4743 | `					);` |
|      ! 0 |  4744 | `			}` |
|        - |  4745 | `			/* Standard pass by value */` |
|      162 |  4746 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      162 |  4747 | `			if( pValue ){` |
|        - |  4748 | `				/* Copy imported value */` |
|       72 |  4749 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  4750 | `			}` |
|        - |  4751 | `			/* Insert the imported variable */` |
|      162 |  4752 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       82 |  4753 | `		}` |
|        - |  4754 | `		/* Finally,load the closure name on the stack */` |
|       96 |  4755 | `		pTos++;` |
|       96 |  4756 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       47 |  4757 | `	}` |
|       96 |  4758 | `	break;` |
|        - |  4759 | `						 }` |
|        - |  4760 | `/*` |
|        - |  4761 | ` * STORE * P2 P3` |
|        - |  4762 | ` *` |
|        - |  4763 | ` * Perform a store (Assignment) operation.` |
|        - |  4764 | ` */` |
|   137985 |  4765 | `case PH7_OP_STORE: {` |
|        - |  4766 | `	ph7_value *pObj;` |
|        - |  4767 | `	SyString sName;` |
|        - |  4768 | `#ifdef UNTRUST` |
|        - |  4769 | `	if( pTos < pStack ){` |
|        - |  4770 | `		goto Abort;` |
|        - |  4771 | `	}` |
|        - |  4772 | `#endif` |
|   275972 |  4773 | `	if( pInstr->iP2 ){` |
|        - |  4774 | `		sxu32 nIdx;` |
|        - |  4775 | `		sxi32 rcT;` |
|        - |  4776 | `		/* Member store operation */` |
|     4786 |  4777 | `		nIdx = pTos->nIdx;` |
|     4786 |  4778 | `		VmPopOperand(&pTos,1);` |
|     4786 |  4779 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  4780 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  4781 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  4782 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4783 | `		}else{` |
|        - |  4784 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  4785 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4782 |  4786 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4782 |  4787 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  4788 | `				goto Abort;` |
|        - |  4789 | `			}` |
|     4782 |  4790 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  4791 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  4792 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  4793 | `				 * propagate out of the VM loop. */` |
|       37 |  4794 | `				VmPopOperand(&pTos,1);` |
|        - |  4795 | `				{` |
|       37 |  4796 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  4797 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  4798 | `						pc = pFrm2->iExceptionJump - 1;` |
|   138004 |  4799 | `						break;` |
|        - |  4800 | `					}` |
|        - |  4801 | `				}` |
|      ! 0 |  4802 | `				goto Exception;` |
|        - |  4803 | `			}` |
|        - |  4804 | `			/* Point to the desired memory object */` |
|     4746 |  4805 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4746 |  4806 | `			if( pObj ){` |
|        - |  4807 | `				/* Perform the store operation */` |
|     4746 |  4808 | `				PH7_MemObjStore(pTos,pObj);` |
|     2372 |  4809 | `			}` |
|        - |  4810 | `		}` |
|     4750 |  4811 | `		break;` |
|   271188 |  4812 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  4813 | `		/* Take the variable name from the next on the stack */` |
|        7 |  4814 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  4815 | `			/* Force a string cast */` |
|      ! 0 |  4816 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4817 | `		}` |
|        7 |  4818 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  4819 | `		pTos--;` |
|        - |  4820 | `#ifdef UNTRUST` |
|        - |  4821 | `		if( pTos < pStack  ){` |
|        - |  4822 | `			goto Abort;` |
|        - |  4823 | `		}` |
|        - |  4824 | `#endif` |
|        4 |  4825 | `	}else{` |
|   271182 |  4826 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4827 | `	}` |
|        - |  4828 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   271188 |  4829 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   271188 |  4830 | `	if( pObj == 0 ){` |
|      ! 0 |  4831 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  4832 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4833 | `		goto Abort;` |
|        - |  4834 | `	}` |
|   271188 |  4835 | `	if( !pInstr->p3 ){` |
|        7 |  4836 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  4837 | `	}` |
|        - |  4838 | `	/* Perform the store operation */` |
|   271188 |  4839 | `	PH7_MemObjStore(pTos,pObj);` |
|   271188 |  4840 | `	break;` |
|        - |  4841 | `				   }` |
|        - |  4842 | `/*` |
|        - |  4843 | ` * STORE_IDX:   P1 * P3` |
|        - |  4844 | ` * STORE_IDX_R: P1 * P3` |
|        - |  4845 | ` *` |
|        - |  4846 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  4847 | ` */` |
|    93793 |  4848 | `case PH7_OP_STORE_IDX:` |
|        - |  4849 | `case PH7_OP_STORE_IDX_REF: {` |
|   187588 |  4850 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  4851 | `	ph7_value *pKey;` |
|        - |  4852 | `	sxu32 nIdx;` |
|   187588 |  4853 | `	if( pInstr->iP1 ){` |
|        - |  4854 | `		/* Key is next on stack */` |
|    62172 |  4855 | `		pKey = pTos;` |
|    62172 |  4856 | `		pTos--;` |
|    31087 |  4857 | `	}else{` |
|   125418 |  4858 | `		pKey = 0;` |
|        - |  4859 | `	}` |
|   187588 |  4860 | `	nIdx = pTos->nIdx;` |
|   187588 |  4861 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  4862 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  4863 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  4864 | `		 * checking true sharing count, then re-add after separation. */` |
|   187536 |  4865 | `		if( nIdx != SXU32_HIGH ){` |
|   187536 |  4866 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   281303 |  4867 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   187536 |  4868 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4869 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  4870 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  4871 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  4872 | `				 * refcounts if the backing array was already separated. */` |
|   187536 |  4873 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   187536 |  4874 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   187536 |  4875 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   187536 |  4876 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   187536 |  4877 | `					pTos->x.pOther = pMap;` |
|    93769 |  4878 | `				}else{` |
|        - |  4879 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  4880 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  4881 | `					pMap = pCur;` |
|        - |  4882 | `				}` |
|    93769 |  4883 | `			}else{` |
|      ! 0 |  4884 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4885 | `			}` |
|    93769 |  4886 | `		}else{` |
|      ! 0 |  4887 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  4888 | `		}` |
|   187536 |  4889 | `		if( pMap->iRef < 2 ){` |
|        - |  4890 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  4891 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  4892 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  4893 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  4894 | `			pMap->iRef = 2;` |
|      ! 0 |  4895 | `		}` |
|    93769 |  4896 | `	}else{` |
|        - |  4897 | `		ph7_value *pObj;` |
|       53 |  4898 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  4899 | `		if( pObj == 0 ){` |
|      ! 0 |  4900 | `			if( pKey ){` |
|      ! 0 |  4901 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  4902 | `			}` |
|      ! 0 |  4903 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  4904 | `			break;` |
|        - |  4905 | `		}` |
|        - |  4906 | `		/* Phase#1: Load the array */` |
|       53 |  4907 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  4908 | `			VmPopOperand(&pTos,1);` |
|       53 |  4909 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  4910 | `				/* Force a string cast */` |
|      ! 0 |  4911 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  4912 | `			}` |
|       53 |  4913 | `			if( pKey == 0 ){` |
|        - |  4914 | `				/* Append string */` |
|        3 |  4915 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  4916 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  4917 | `				}` |
|        2 |  4918 | `			}else{` |
|        - |  4919 | `				sxu32 nOfft;` |
|       51 |  4920 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  4921 | `					/* Force an int cast */` |
|       51 |  4922 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  4923 | `				}` |
|       51 |  4924 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  4925 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  4926 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  4927 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  4928 | `					zData[nOfft] = zBlob[0];` |
|       26 |  4929 | `				}else{` |
|      ! 0 |  4930 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  4931 | `						/* Perform an append operation */` |
|      ! 0 |  4932 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  4933 | `					}` |
|        - |  4934 | `				}` |
|        - |  4935 | `			}` |
|       53 |  4936 | `			if( pKey ){` |
|       51 |  4937 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  4938 | `			}` |
|       53 |  4939 | `			break;` |
|      ! 0 |  4940 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  4941 | `			/* Force a hashmap cast  */` |
|      ! 0 |  4942 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  4943 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  4944 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  4945 | `				goto Abort;` |
|        - |  4946 | `			}` |
|      ! 0 |  4947 | `		}` |
|        - |  4948 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  4949 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  4950 | `	}` |
|   187536 |  4951 | `	VmPopOperand(&pTos,1);` |
|        - |  4952 | `	/* Phase#2: Perform the insertion */` |
|   187536 |  4953 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  4954 | `		/* Insertion by reference */` |
|       15 |  4955 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  4956 | `	}else{` |
|   187522 |  4957 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  4958 | `	}` |
|   187536 |  4959 | `	if( pKey ){` |
|    62122 |  4960 | `		PH7_MemObjRelease(pKey);` |
|    31060 |  4961 | `	}` |
|   187536 |  4962 | `	break;` |
|        - |  4963 | `					   }` |
|        - |  4964 | `/*` |
|        - |  4965 | ` * INCR: P1 * *` |
|        - |  4966 | ` *` |
|        - |  4967 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  4968 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  4969 | ` * the stack and increment after that.` |
|        - |  4970 | ` */` |
|   166564 |  4971 | `case PH7_OP_INCR:` |
|        - |  4972 | `#ifdef UNTRUST` |
|        - |  4973 | `	if( pTos < pStack ){` |
|        - |  4974 | `		goto Abort;` |
|        - |  4975 | `	}` |
|        - |  4976 | `#endif` |
|   333174 |  4977 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   333174 |  4978 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4979 | `			ph7_value *pObj;` |
|   333174 |  4980 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   333174 |  4981 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  4982 | `					/* Perl-style string increment.` |
|        - |  4983 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  4984 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  4985 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  4986 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  4987 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  4988 | `					}` |
|       49 |  4989 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  4990 | `					if( pInstr->iP1 ){` |
|        - |  4991 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  4992 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  4993 | `					}` |
|       25 |  4994 | `				}else{` |
|        - |  4995 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  4996 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  4997 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  4998 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  4999 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5000 | `					 * so its old-value view survives the coercion. */` |
|   333126 |  5001 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       11 |  5002 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        5 |  5003 | `					}` |
|        - |  5004 | `					/* Force a numeric cast on the variable */` |
|   333126 |  5005 | `					PH7_MemObjToNumeric(pObj);` |
|   333126 |  5006 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        5 |  5007 | `						pObj->rVal++;` |
|        3 |  5008 | `					}else{` |
|   333122 |  5009 | `						pObj->x.iVal++;` |
|        - |  5010 | `					}` |
|   333126 |  5011 | `					if( pInstr->iP1 ){` |
|        - |  5012 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5013 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5014 | `					}` |
|        - |  5015 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5016 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5017 | `				}` |
|   166608 |  5018 | `			}` |
|   166610 |  5019 | `		}else{` |
|      ! 0 |  5020 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5021 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5022 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5023 | `				}else{` |
|        - |  5024 | `					/* Force a numeric cast */` |
|      ! 0 |  5025 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5026 | `					/* Pre-increment */` |
|      ! 0 |  5027 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5028 | `						pTos->rVal++;` |
|        - |  5029 | `						/* Try to get an integer representation */` |
|      ! 0 |  5030 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5031 | `					}else{` |
|      ! 0 |  5032 | `						pTos->x.iVal++;` |
|      ! 0 |  5033 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5034 | `					}` |
|        - |  5035 | `				}` |
|      ! 0 |  5036 | `			}` |
|        - |  5037 | `		}` |
|   166608 |  5038 | `	}` |
|   333174 |  5039 | `	break;` |
|        - |  5040 | `/*` |
|        - |  5041 | ` * DECR: P1 * *` |
|        - |  5042 | ` *` |
|        - |  5043 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5044 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5045 | ` * and decrement after that.` |
|        - |  5046 | ` */` |
|        2 |  5047 | `case PH7_OP_DECR:` |
|        - |  5048 | `#ifdef UNTRUST` |
|        - |  5049 | `	if( pTos < pStack ){` |
|        - |  5050 | `		goto Abort;` |
|        - |  5051 | `	}` |
|        - |  5052 | `#endif` |
|        5 |  5053 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|        - |  5054 | `		/* Force a numeric cast */` |
|        5 |  5055 | `		PH7_MemObjToNumeric(pTos);` |
|        5 |  5056 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5057 | `			ph7_value *pObj;` |
|        5 |  5058 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        - |  5059 | `				/* Force a numeric cast */` |
|        5 |  5060 | `				PH7_MemObjToNumeric(pObj);` |
|        5 |  5061 | `				if( pObj->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5062 | `					pObj->rVal--;` |
|        - |  5063 | `					/* Try to get an integer representation */` |
|      ! 0 |  5064 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5065 | `				}else{` |
|        5 |  5066 | `					pObj->x.iVal--;` |
|        5 |  5067 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5068 | `				}` |
|        5 |  5069 | `				if( pInstr->iP1 ){` |
|        - |  5070 | `					/* Pre-icrement */` |
|      ! 0 |  5071 | `					PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5072 | `				}` |
|        2 |  5073 | `			}` |
|        3 |  5074 | `		}else{` |
|      ! 0 |  5075 | `			if( pInstr->iP1 ){` |
|        - |  5076 | `				/* Pre-increment */` |
|      ! 0 |  5077 | `				if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5078 | `					pTos->rVal--;` |
|        - |  5079 | `					/* Try to get an integer representation */` |
|      ! 0 |  5080 | `					PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5081 | `				}else{` |
|      ! 0 |  5082 | `					pTos->x.iVal--;` |
|      ! 0 |  5083 | `					MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5084 | `				}` |
|      ! 0 |  5085 | `			}` |
|        - |  5086 | `		}` |
|        2 |  5087 | `	}` |
|        5 |  5088 | `	break;` |
|        - |  5089 | `/*` |
|        - |  5090 | ` * UMINUS: * * *` |
|        - |  5091 | ` *` |
|        - |  5092 | ` * Perform a unary minus operation.` |
|        - |  5093 | ` */` |
|    28624 |  5094 | `case PH7_OP_UMINUS:` |
|        - |  5095 | `#ifdef UNTRUST` |
|        - |  5096 | `	if( pTos < pStack ){` |
|        - |  5097 | `		goto Abort;` |
|        - |  5098 | `	}` |
|        - |  5099 | `#endif` |
|        - |  5100 | `	/* Force a numeric (integer,real or both) cast */` |
|    57250 |  5101 | `	PH7_MemObjToNumeric(pTos);` |
|    57250 |  5102 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5103 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5104 | `	}` |
|    57250 |  5105 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    57220 |  5106 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    28609 |  5107 | `	}` |
|    57250 |  5108 | `	break;` |
|        - |  5109 | `/*` |
|        - |  5110 | ` * UPLUS: * * *` |
|        - |  5111 | ` *` |
|        - |  5112 | ` * Perform a unary plus operation.` |
|        - |  5113 | ` */` |
|       18 |  5114 | `case PH7_OP_UPLUS:` |
|        - |  5115 | `#ifdef UNTRUST` |
|        - |  5116 | `	if( pTos < pStack ){` |
|        - |  5117 | `		goto Abort;` |
|        - |  5118 | `	}` |
|        - |  5119 | `#endif` |
|        - |  5120 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5121 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5122 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5123 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5124 | `	}` |
|       37 |  5125 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5126 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5127 | `	}` |
|       37 |  5128 | `	break;` |
|        - |  5129 | `/*` |
|        - |  5130 | ` * OP_LNOT: * * *` |
|        - |  5131 | ` *` |
|        - |  5132 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5133 | ` * with its complement.` |
|        - |  5134 | ` */` |
|    44093 |  5135 | `case PH7_OP_LNOT:` |
|        - |  5136 | `#ifdef UNTRUST` |
|        - |  5137 | `	if( pTos < pStack ){` |
|        - |  5138 | `		goto Abort;` |
|        - |  5139 | `	}` |
|        - |  5140 | `#endif` |
|        - |  5141 | `	/* Force a boolean cast */` |
|    88232 |  5142 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5143 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5144 | `	}` |
|    88232 |  5145 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    88232 |  5146 | `	break;` |
|        - |  5147 | `/*` |
|        - |  5148 | ` * OP_BITNOT: * * *` |
|        - |  5149 | ` *` |
|        - |  5150 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5151 | ` * with its ones-complement.` |
|        - |  5152 | ` */` |
|       15 |  5153 | `case PH7_OP_BITNOT:` |
|        - |  5154 | `#ifdef UNTRUST` |
|        - |  5155 | `	if( pTos < pStack ){` |
|        - |  5156 | `		goto Abort;` |
|        - |  5157 | `	}` |
|        - |  5158 | `#endif` |
|        - |  5159 | `	/* Force an integer cast */` |
|       32 |  5160 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5161 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5162 | `	}` |
|       32 |  5163 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5164 | `	break;` |
|        - |  5165 | `/* OP_MUL * * *` |
|        - |  5166 | ` * OP_MUL_STORE * * *` |
|        - |  5167 | ` *` |
|        - |  5168 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5169 | ` * and push the result back onto the stack.` |
|        - |  5170 | ` */` |
|     1287 |  5171 | `case PH7_OP_MUL:` |
|        - |  5172 | `case PH7_OP_MUL_STORE: {` |
|     2576 |  5173 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5174 | `	/* Force the operand to be numeric */` |
|        - |  5175 | `#ifdef UNTRUST` |
|        - |  5176 | `	if( pNos < pStack ){` |
|        - |  5177 | `		goto Abort;` |
|        - |  5178 | `	}` |
|        - |  5179 | `#endif` |
|     2576 |  5180 | `	PH7_MemObjToNumeric(pTos);` |
|     2576 |  5181 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5182 | `	/* Perform the requested operation */` |
|     2576 |  5183 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5184 | `		/* Floating point arithemic */` |
|        - |  5185 | `		ph7_real a,b,r;` |
|       19 |  5186 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5187 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5188 | `		}` |
|       19 |  5189 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5190 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5191 | `		}` |
|       19 |  5192 | `		a = pNos->rVal;` |
|       19 |  5193 | `		b = pTos->rVal;` |
|       19 |  5194 | `		r = a * b;` |
|        - |  5195 | `		/* Push the result */` |
|       19 |  5196 | `		pNos->rVal = r;` |
|       19 |  5197 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5198 | `		/* Try to get an integer representation */` |
|       19 |  5199 | `		PH7_MemObjTryInteger(pNos);` |
|       10 |  5200 | `	}else{` |
|        - |  5201 | `		/* Integer arithmetic */` |
|        - |  5202 | `		sxi64 a,b,r;` |
|     2558 |  5203 | `		a = pNos->x.iVal;` |
|     2558 |  5204 | `		b = pTos->x.iVal;` |
|     2558 |  5205 | `		r = a * b;` |
|        - |  5206 | `		/* Push the result */` |
|     2558 |  5207 | `		pNos->x.iVal = r;` |
|     2558 |  5208 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5209 | `	}` |
|     2576 |  5210 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5211 | `		ph7_value *pObj;` |
|       32 |  5212 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5213 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5214 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5215 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5216 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5217 | `		}` |
|       15 |  5218 | `	}` |
|     2576 |  5219 | `	VmPopOperand(&pTos,1);` |
|     2576 |  5220 | `	break;` |
|        - |  5221 | `				 }` |
|        - |  5222 | `/* OP_POW * * *` |
|        - |  5223 | ` * OP_POW_STORE * * *` |
|        - |  5224 | ` *` |
|        - |  5225 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5226 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5227 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5228 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5229 | ` */` |
|       66 |  5230 | `case PH7_OP_POW:` |
|        - |  5231 | `case PH7_OP_POW_STORE: {` |
|      133 |  5232 | `	ph7_value *pNos = &pTos[-1];` |
|      133 |  5233 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5234 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5235 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5236 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5237 | `	 */` |
|      133 |  5238 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      133 |  5239 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5240 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5241 | `	int bBothInt;` |
|      133 |  5242 | `	int usedInt = 0;` |
|        - |  5243 | `	ph7_real a, b, r;` |
|        - |  5244 | `#endif` |
|      133 |  5245 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5246 | `#ifdef UNTRUST` |
|        - |  5247 | `	if( pNos < pStack ){` |
|        - |  5248 | `		goto Abort;` |
|        - |  5249 | `	}` |
|        - |  5250 | `#endif` |
|      133 |  5251 | `	PH7_MemObjToNumeric(pTos);` |
|      133 |  5252 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5253 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      261 |  5254 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      128 |  5255 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      133 |  5256 | `	if( bBothInt ){` |
|      123 |  5257 | `		base_i = pBase->x.iVal;` |
|      123 |  5258 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5259 | `	}` |
|      133 |  5260 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5261 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5262 | `	}` |
|      133 |  5263 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      131 |  5264 | `		PH7_MemObjToReal(pExp);` |
|       65 |  5265 | `	}` |
|      133 |  5266 | `	a = pBase->rVal;` |
|      133 |  5267 | `	b = pExp->rVal;` |
|      133 |  5268 | `	r = pow(a, b);` |
|        - |  5269 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5270 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5271 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5272 | `	 * representable as double but not as signed int64. */` |
|      133 |  5273 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5274 | `		sxi64 result_i = 1;` |
|      117 |  5275 | `		sxi64 cur_base = base_i;` |
|      117 |  5276 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5277 | `		int overflow = 0;` |
|      401 |  5278 | `		while( cur_exp > 0 ){` |
|      289 |  5279 | `			if( cur_exp & 1 ){` |
|      189 |  5280 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5281 | `					overflow = 1;` |
|        3 |  5282 | `					break;` |
|        - |  5283 | `				}` |
|       93 |  5284 | `			}` |
|      287 |  5285 | `			cur_exp >>= 1;` |
|      287 |  5286 | `			if( cur_exp > 0 ){` |
|      181 |  5287 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5288 | `					overflow = 1;` |
|        3 |  5289 | `					break;` |
|        - |  5290 | `				}` |
|       89 |  5291 | `			}` |
|        1 |  5292 | `		}` |
|      117 |  5293 | `		if( !overflow ){` |
|      113 |  5294 | `			pNos->x.iVal = result_i;` |
|      113 |  5295 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5296 | `			usedInt = 1;` |
|       56 |  5297 | `		}` |
|       58 |  5298 | `	}` |
|      133 |  5299 | `	if( !usedInt ){` |
|       21 |  5300 | `		pNos->rVal = r;` |
|       21 |  5301 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       10 |  5302 | `	}` |
|        - |  5303 | `#else` |
|        - |  5304 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5305 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5306 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5307 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5308 | `	 * represented. */` |
|        - |  5309 | `	base_i = pBase->x.iVal;` |
|        - |  5310 | `	exp_i  = pExp->x.iVal;` |
|        - |  5311 | `	{` |
|        - |  5312 | `		sxi64 result_i = 1;` |
|        - |  5313 | `		sxi64 cur_base = base_i;` |
|        - |  5314 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5315 | `		if( cur_exp < 0 ){` |
|        - |  5316 | `			result_i = 0;` |
|        - |  5317 | `		}else{` |
|        - |  5318 | `			while( cur_exp > 0 ){` |
|        - |  5319 | `				if( cur_exp & 1 ){` |
|        - |  5320 | `					result_i *= cur_base;` |
|        - |  5321 | `				}` |
|        - |  5322 | `				cur_exp >>= 1;` |
|        - |  5323 | `				if( cur_exp > 0 ){` |
|        - |  5324 | `					cur_base *= cur_base;` |
|        - |  5325 | `				}` |
|        - |  5326 | `			}` |
|        - |  5327 | `		}` |
|        - |  5328 | `		pNos->x.iVal = result_i;` |
|        - |  5329 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5330 | `	}` |
|        - |  5331 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      133 |  5332 | `	if( bStore ){` |
|        - |  5333 | `		ph7_value *pObj;` |
|       23 |  5334 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5335 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5336 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5337 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5338 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5339 | `		}` |
|       11 |  5340 | `	}` |
|      133 |  5341 | `	VmPopOperand(&pTos,1);` |
|      133 |  5342 | `	break;` |
|        - |  5343 | `				 }` |
|        - |  5344 | `/* OP_ADD * * *` |
|        - |  5345 | ` *` |
|        - |  5346 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5347 | ` * and push the result back onto the stack.` |
|        - |  5348 | ` */` |
|      513 |  5349 | `case PH7_OP_ADD:{` |
|     1028 |  5350 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5351 | `#ifdef UNTRUST` |
|        - |  5352 | `	if( pNos < pStack ){` |
|        - |  5353 | `		goto Abort;` |
|        - |  5354 | `	}` |
|        - |  5355 | `#endif` |
|        - |  5356 | `	/* Perform the addition */` |
|     1028 |  5357 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1028 |  5358 | `	VmPopOperand(&pTos,1);` |
|     1028 |  5359 | `	break;` |
|        - |  5360 | `				}` |
|        - |  5361 | `/*` |
|        - |  5362 | ` * OP_ADD_STORE * * *` |
|        - |  5363 | ` *` |
|        - |  5364 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5365 | ` * and push the result back onto the stack.` |
|        - |  5366 | ` */` |
|      502 |  5367 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5368 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5369 | `	ph7_value *pObj;` |
|        - |  5370 | `	sxu32 nIdx;` |
|        - |  5371 | `#ifdef UNTRUST` |
|        - |  5372 | `	if( pNos < pStack ){` |
|        - |  5373 | `		goto Abort;` |
|        - |  5374 | `	}` |
|        - |  5375 | `#endif` |
|        - |  5376 | `	/* Perform the addition */` |
|     1006 |  5377 | `	nIdx = pTos->nIdx;` |
|     1006 |  5378 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5379 | `	/* Peform the store operation */` |
|     1006 |  5380 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5381 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5382 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5383 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5384 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5385 | `	}` |
|        - |  5386 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5387 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5388 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5389 | `	break;` |
|        - |  5390 | `				}` |
|        - |  5391 | `/* OP_SUB * * *` |
|        - |  5392 | ` *` |
|        - |  5393 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5394 | ` * first (what was next on the stack) from the second (the` |
|        - |  5395 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5396 | ` */` |
|      348 |  5397 | `case PH7_OP_SUB: {` |
|      698 |  5398 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5399 | `#ifdef UNTRUST` |
|        - |  5400 | `	if( pNos < pStack ){` |
|        - |  5401 | `		goto Abort;` |
|        - |  5402 | `	}` |
|        - |  5403 | `#endif` |
|      698 |  5404 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5405 | `		/* Floating point arithemic */` |
|        - |  5406 | `		ph7_real a,b,r;` |
|       95 |  5407 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5408 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5409 | `		}` |
|       95 |  5410 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5411 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5412 | `		}` |
|       95 |  5413 | `		a = pNos->rVal;` |
|       95 |  5414 | `		b = pTos->rVal;` |
|       95 |  5415 | `		r = a - b;` |
|        - |  5416 | `		/* Push the result */` |
|       95 |  5417 | `		pNos->rVal = r;` |
|       95 |  5418 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5419 | `		/* Try to get an integer representation */` |
|       95 |  5420 | `		PH7_MemObjTryInteger(pNos);` |
|       48 |  5421 | `	}else{` |
|        - |  5422 | `		/* Integer arithmetic */` |
|        - |  5423 | `		sxi64 a,b,r;` |
|      604 |  5424 | `		a = pNos->x.iVal;` |
|      604 |  5425 | `		b = pTos->x.iVal;` |
|      604 |  5426 | `		r = a - b;` |
|        - |  5427 | `		/* Push the result */` |
|      604 |  5428 | `		pNos->x.iVal = r;` |
|      604 |  5429 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5430 | `	}` |
|      698 |  5431 | `	VmPopOperand(&pTos,1);` |
|      698 |  5432 | `	break;` |
|        - |  5433 | `				 }` |
|        - |  5434 | `/* OP_SUB_STORE * * *` |
|        - |  5435 | ` *` |
|        - |  5436 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5437 | ` * first (what was next on the stack) from the second (the` |
|        - |  5438 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5439 | ` */` |
|        4 |  5440 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5441 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5442 | `	ph7_value *pObj;` |
|        - |  5443 | `#ifdef UNTRUST` |
|        - |  5444 | `	if( pNos < pStack ){` |
|        - |  5445 | `		goto Abort;` |
|        - |  5446 | `	}` |
|        - |  5447 | `#endif` |
|       10 |  5448 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5449 | `		/* Floating point arithemic */` |
|        - |  5450 | `		ph7_real a,b,r;` |
|      ! 0 |  5451 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5452 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5453 | `		}` |
|      ! 0 |  5454 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5455 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5456 | `		}` |
|      ! 0 |  5457 | `		a = pTos->rVal;` |
|      ! 0 |  5458 | `		b = pNos->rVal;` |
|      ! 0 |  5459 | `		r = a - b;` |
|        - |  5460 | `		/* Push the result */` |
|      ! 0 |  5461 | `		pNos->rVal = r;` |
|      ! 0 |  5462 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5463 | `		/* Try to get an integer representation */` |
|      ! 0 |  5464 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5465 | `	}else{` |
|        - |  5466 | `		/* Integer arithmetic */` |
|        - |  5467 | `		sxi64 a,b,r;` |
|       10 |  5468 | `		a = pTos->x.iVal;` |
|       10 |  5469 | `		b = pNos->x.iVal;` |
|       10 |  5470 | `		r = a - b;` |
|        - |  5471 | `		/* Push the result */` |
|       10 |  5472 | `		pNos->x.iVal = r;` |
|       10 |  5473 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5474 | `	}` |
|       10 |  5475 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5476 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5477 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5478 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5479 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5480 | `	}` |
|       10 |  5481 | `	VmPopOperand(&pTos,1);` |
|       10 |  5482 | `	break;` |
|        - |  5483 | `				 }` |
|        - |  5484 |  |
|        - |  5485 | `/*` |
|        - |  5486 | ` * OP_MOD * * *` |
|        - |  5487 | ` *` |
|        - |  5488 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5489 | ` * first (what was next on the stack) from the second (the` |
|        - |  5490 | ` * top of the stack) and push the remainder after division` |
|        - |  5491 | ` * onto the stack.` |
|        - |  5492 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5493 | ` */` |
|      308 |  5494 | `case PH7_OP_MOD:{` |
|      618 |  5495 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5496 | `	sxi64 a,b,r;` |
|        - |  5497 | `#ifdef UNTRUST` |
|        - |  5498 | `	if( pNos < pStack ){` |
|        - |  5499 | `		goto Abort;` |
|        - |  5500 | `	}` |
|        - |  5501 | `#endif` |
|        - |  5502 | `	/* Force the operands to be integer */` |
|      618 |  5503 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5504 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5505 | `	}` |
|      618 |  5506 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5507 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5508 | `	}` |
|        - |  5509 | `	/* Perform the requested operation */` |
|      618 |  5510 | `	a = pNos->x.iVal;` |
|      618 |  5511 | `	b = pTos->x.iVal;` |
|      618 |  5512 | `	if( b == 0 ){` |
|        3 |  5513 | `		r = 0;` |
|        3 |  5514 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5515 | `		/* goto Abort; */` |
|        2 |  5516 | `	}else{` |
|      615 |  5517 | `		r = a%b;` |
|        - |  5518 | `	}` |
|        - |  5519 | `	/* Push the result */` |
|      618 |  5520 | `	pNos->x.iVal = r;` |
|      618 |  5521 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5522 | `	VmPopOperand(&pTos,1);` |
|      618 |  5523 | `	break;` |
|        - |  5524 | `				}` |
|        - |  5525 | `/*` |
|        - |  5526 | ` * OP_MOD_STORE * * *` |
|        - |  5527 | ` *` |
|        - |  5528 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5529 | ` * first (what was next on the stack) from the second (the` |
|        - |  5530 | ` * top of the stack) and push the remainder after division` |
|        - |  5531 | ` * onto the stack.` |
|        - |  5532 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5533 | ` */` |
|        1 |  5534 | `case PH7_OP_MOD_STORE: {` |
|        3 |  5535 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5536 | `	ph7_value *pObj;` |
|        - |  5537 | `	sxi64 a,b,r;` |
|        - |  5538 | `#ifdef UNTRUST` |
|        - |  5539 | `	if( pNos < pStack ){` |
|        - |  5540 | `		goto Abort;` |
|        - |  5541 | `	}` |
|        - |  5542 | `#endif` |
|        - |  5543 | `	/* Force the operands to be integer */` |
|        3 |  5544 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5545 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5546 | `	}` |
|        3 |  5547 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5548 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5549 | `	}` |
|        - |  5550 | `	/* Perform the requested operation */` |
|        3 |  5551 | `	a = pTos->x.iVal;` |
|        3 |  5552 | `	b = pNos->x.iVal;` |
|        3 |  5553 | `	if( b == 0 ){` |
|      ! 0 |  5554 | `		r = 0;` |
|      ! 0 |  5555 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5556 | `		/* goto Abort; */` |
|      ! 0 |  5557 | `	}else{` |
|        3 |  5558 | `		r = a%b;` |
|        - |  5559 | `	}` |
|        - |  5560 | `	/* Push the result */` |
|        3 |  5561 | `	pNos->x.iVal = r;` |
|        3 |  5562 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  5563 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5564 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  5565 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  5566 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  5567 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  5568 | `	}` |
|        3 |  5569 | `	VmPopOperand(&pTos,1);` |
|        3 |  5570 | `	break;` |
|        - |  5571 | `				}` |
|        - |  5572 | `/*` |
|        - |  5573 | ` * OP_DIV * * *` |
|        - |  5574 | ` *` |
|        - |  5575 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5576 | ` * first (what was next on the stack) from the second (the` |
|        - |  5577 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5578 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5579 | ` */` |
|       31 |  5580 | `case PH7_OP_DIV:{` |
|       64 |  5581 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5582 | `	ph7_real a,b,r;` |
|        - |  5583 | `#ifdef UNTRUST` |
|        - |  5584 | `	if( pNos < pStack ){` |
|        - |  5585 | `		goto Abort;` |
|        - |  5586 | `	}` |
|        - |  5587 | `#endif` |
|        - |  5588 | `	/* Force the operands to be real */` |
|       64 |  5589 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       60 |  5590 | `		PH7_MemObjToReal(pTos);` |
|       29 |  5591 | `	}` |
|       64 |  5592 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       26 |  5593 | `		PH7_MemObjToReal(pNos);` |
|       12 |  5594 | `	}` |
|        - |  5595 | `	/* Perform the requested operation */` |
|       64 |  5596 | `	a = pNos->rVal;` |
|       64 |  5597 | `	b = pTos->rVal;` |
|       64 |  5598 | `	if( b == 0 ){` |
|        - |  5599 | `		/* Division by zero */` |
|        3 |  5600 | `		pNos->rVal = 0;` |
|        3 |  5601 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  5602 | `		/* goto Abort; */` |
|        2 |  5603 | `	}else{` |
|       61 |  5604 | `		r = a/b;` |
|        - |  5605 | `		/* Push the result */` |
|       61 |  5606 | `		pNos->rVal = r;` |
|       61 |  5607 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5608 | `		/* Try to get an integer representation */` |
|       61 |  5609 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5610 | `	}` |
|       64 |  5611 | `	VmPopOperand(&pTos,1);` |
|       64 |  5612 | `	break;` |
|        - |  5613 | `				}` |
|        - |  5614 | `/*` |
|        - |  5615 | ` * OP_DIV_STORE * * *` |
|        - |  5616 | ` *` |
|        - |  5617 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5618 | ` * first (what was next on the stack) from the second (the` |
|        - |  5619 | ` * top of the stack) and push the result onto the stack.` |
|        - |  5620 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  5621 | ` */` |
|        2 |  5622 | `case PH7_OP_DIV_STORE:{` |
|        5 |  5623 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5624 | `	ph7_value *pObj;` |
|        - |  5625 | `	ph7_real a,b,r;` |
|        - |  5626 | `#ifdef UNTRUST` |
|        - |  5627 | `	if( pNos < pStack ){` |
|        - |  5628 | `		goto Abort;` |
|        - |  5629 | `	}` |
|        - |  5630 | `#endif` |
|        - |  5631 | `	/* Force the operands to be real */` |
|        5 |  5632 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5633 | `		PH7_MemObjToReal(pTos);` |
|        2 |  5634 | `	}` |
|        5 |  5635 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5636 | `		PH7_MemObjToReal(pNos);` |
|        2 |  5637 | `	}` |
|        - |  5638 | `	/* Perform the requested operation */` |
|        5 |  5639 | `	a = pTos->rVal;` |
|        5 |  5640 | `	b = pNos->rVal;` |
|        5 |  5641 | `	if( b == 0 ){` |
|        - |  5642 | `		/* Division by zero */` |
|      ! 0 |  5643 | `		r = 0;` |
|      ! 0 |  5644 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  5645 | `		/* goto Abort; */` |
|      ! 0 |  5646 | `	}else{` |
|        5 |  5647 | `		r = a/b;` |
|        - |  5648 | `		/* Push the result */` |
|        5 |  5649 | `		pNos->rVal = r;` |
|        5 |  5650 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5651 | `		/* Try to get an integer representation */` |
|        5 |  5652 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  5653 | `	}` |
|        5 |  5654 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5655 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  5656 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  5657 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  5658 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  5659 | `	}` |
|        5 |  5660 | `	VmPopOperand(&pTos,1);` |
|        5 |  5661 | `	break;` |
|        - |  5662 | `				}` |
|        - |  5663 | `/* OP_BAND * * *` |
|        - |  5664 | ` *` |
|        - |  5665 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5666 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5667 | ` * two elements.` |
|        - |  5668 | `*/` |
|        - |  5669 | `/* OP_BOR * * *` |
|        - |  5670 | ` *` |
|        - |  5671 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5672 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5673 | ` * two elements.` |
|        - |  5674 | ` */` |
|        - |  5675 | `/* OP_BXOR * * *` |
|        - |  5676 | ` *` |
|        - |  5677 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5678 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5679 | ` * two elements.` |
|        - |  5680 | ` */` |
|       44 |  5681 | `case PH7_OP_BAND:` |
|        - |  5682 | `case PH7_OP_BOR:` |
|        - |  5683 | `case PH7_OP_BXOR:{` |
|       90 |  5684 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5685 | `	sxi64 a,b,r;` |
|        - |  5686 | `#ifdef UNTRUST` |
|        - |  5687 | `	if( pNos < pStack ){` |
|        - |  5688 | `		goto Abort;` |
|        - |  5689 | `	}` |
|        - |  5690 | `#endif` |
|        - |  5691 | `	/* Force the operands to be integer */` |
|       90 |  5692 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5693 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5694 | `	}` |
|       90 |  5695 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5696 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5697 | `	}` |
|        - |  5698 | `	/* Perform the requested operation */` |
|       90 |  5699 | `	a = pNos->x.iVal;` |
|       90 |  5700 | `	b = pTos->x.iVal;` |
|       90 |  5701 | `	switch(pInstr->iOp){` |
|        7 |  5702 | `	case PH7_OP_BOR_STORE:` |
|       15 |  5703 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  5704 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  5705 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  5706 | `	case PH7_OP_BAND_STORE:` |
|       30 |  5707 | `	case PH7_OP_BAND:` |
|       62 |  5708 | `	default:          r = a&b; break;` |
|        - |  5709 | `	}` |
|        - |  5710 | `	/* Push the result */` |
|       90 |  5711 | `	pNos->x.iVal = r;` |
|       90 |  5712 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  5713 | `	VmPopOperand(&pTos,1);` |
|       90 |  5714 | `	break;` |
|        - |  5715 | `				 }` |
|        - |  5716 | `/* OP_BAND_STORE * * *` |
|        - |  5717 | ` *` |
|        - |  5718 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5719 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  5720 | ` * two elements.` |
|        - |  5721 | `*/` |
|        - |  5722 | `/* OP_BOR_STORE * * *` |
|        - |  5723 | ` *` |
|        - |  5724 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5725 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  5726 | ` * two elements.` |
|        - |  5727 | ` */` |
|        - |  5728 | `/* OP_BXOR_STORE * * *` |
|        - |  5729 | ` *` |
|        - |  5730 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5731 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  5732 | ` * two elements.` |
|        - |  5733 | ` */` |
|       10 |  5734 | `case PH7_OP_BAND_STORE:` |
|        - |  5735 | `case PH7_OP_BOR_STORE:` |
|        - |  5736 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  5737 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5738 | `	ph7_value *pObj;` |
|        - |  5739 | `	sxi64 a,b,r;` |
|        - |  5740 | `#ifdef UNTRUST` |
|        - |  5741 | `	if( pNos < pStack ){` |
|        - |  5742 | `		goto Abort;` |
|        - |  5743 | `	}` |
|        - |  5744 | `#endif` |
|        - |  5745 | `	/* Force the operands to be integer */` |
|       21 |  5746 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5747 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5748 | `	}` |
|       21 |  5749 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5750 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5751 | `	}` |
|        - |  5752 | `	/* Perform the requested operation */` |
|       21 |  5753 | `	a = pTos->x.iVal;` |
|       21 |  5754 | `	b = pNos->x.iVal;` |
|       21 |  5755 | `	switch(pInstr->iOp){` |
|        3 |  5756 | `	case PH7_OP_BOR_STORE:` |
|        7 |  5757 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  5758 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  5759 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  5760 | `	case PH7_OP_BAND_STORE:` |
|        3 |  5761 | `	case PH7_OP_BAND:` |
|        7 |  5762 | `	default:          r = a&b; break;` |
|        - |  5763 | `	}` |
|        - |  5764 | `	/* Push the result */` |
|       21 |  5765 | `	pNos->x.iVal = r;` |
|       21 |  5766 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  5767 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5768 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  5769 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  5770 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  5771 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  5772 | `	}` |
|       21 |  5773 | `	VmPopOperand(&pTos,1);` |
|       21 |  5774 | `	break;` |
|        - |  5775 | `				 }` |
|        - |  5776 | `/* OP_SHL * * *` |
|        - |  5777 | ` *` |
|        - |  5778 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5779 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5780 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5781 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5782 | ` */` |
|        - |  5783 | `/* OP_SHR * * *` |
|        - |  5784 | ` *` |
|        - |  5785 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5786 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5787 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5788 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5789 | ` */` |
|       12 |  5790 | `case PH7_OP_SHL:` |
|        - |  5791 | `case PH7_OP_SHR: {` |
|       25 |  5792 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5793 | `	sxi64 a,r;` |
|        - |  5794 | `	sxi32 b;` |
|        - |  5795 | `#ifdef UNTRUST` |
|        - |  5796 | `	if( pNos < pStack ){` |
|        - |  5797 | `		goto Abort;` |
|        - |  5798 | `	}` |
|        - |  5799 | `#endif` |
|        - |  5800 | `	/* Force the operands to be integer */` |
|       25 |  5801 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5802 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5803 | `	}` |
|       25 |  5804 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5805 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5806 | `	}` |
|        - |  5807 | `	/* Perform the requested operation */` |
|       25 |  5808 | `	a = pNos->x.iVal;` |
|       25 |  5809 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  5810 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  5811 | `		r = a << b;` |
|        8 |  5812 | `	}else{` |
|       11 |  5813 | `		r = a >> b;` |
|        - |  5814 | `	}` |
|        - |  5815 | `	/* Push the result */` |
|       25 |  5816 | `	pNos->x.iVal = r;` |
|       25 |  5817 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  5818 | `	VmPopOperand(&pTos,1);` |
|       25 |  5819 | `	break;` |
|        - |  5820 | `				 }` |
|        - |  5821 | `/*  OP_SHL_STORE * * *` |
|        - |  5822 | ` *` |
|        - |  5823 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5824 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5825 | ` * left by N bits where N is the top element on the stack.` |
|        - |  5826 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5827 | ` */` |
|        - |  5828 | `/* OP_SHR_STORE * * *` |
|        - |  5829 | ` *` |
|        - |  5830 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  5831 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  5832 | ` * right by N bits where N is the top element on the stack.` |
|        - |  5833 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  5834 | ` */` |
|        9 |  5835 | `case PH7_OP_SHL_STORE:` |
|        - |  5836 | `case PH7_OP_SHR_STORE: {` |
|       19 |  5837 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5838 | `	ph7_value *pObj;` |
|        - |  5839 | `	sxi64 a,r;` |
|        - |  5840 | `	sxi32 b;` |
|        - |  5841 | `#ifdef UNTRUST` |
|        - |  5842 | `	if( pNos < pStack ){` |
|        - |  5843 | `		goto Abort;` |
|        - |  5844 | `	}` |
|        - |  5845 | `#endif` |
|        - |  5846 | `	/* Force the operands to be integer */` |
|       19 |  5847 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5848 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5849 | `	}` |
|       19 |  5850 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5851 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  5852 | `	}` |
|        - |  5853 | `	/* Perform the requested operation */` |
|       19 |  5854 | `	a = pTos->x.iVal;` |
|       19 |  5855 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  5856 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  5857 | `		r = a << b;` |
|        5 |  5858 | `	}else{` |
|       11 |  5859 | `		r = a >> b;` |
|        - |  5860 | `	}` |
|        - |  5861 | `	/* Push the result */` |
|       19 |  5862 | `	pNos->x.iVal = r;` |
|       19 |  5863 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  5864 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5865 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  5866 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  5867 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  5868 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  5869 | `	}` |
|       19 |  5870 | `	VmPopOperand(&pTos,1);` |
|       19 |  5871 | `	break;` |
|        - |  5872 | `				 }` |
|        - |  5873 | `/* CAT:  P1 * *` |
|        - |  5874 | ` *` |
|        - |  5875 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  5876 | ` * back.` |
|        - |  5877 | ` */` |
|    70042 |  5878 | `case PH7_OP_CAT:{` |
|        - |  5879 | `	ph7_value *pNos,*pCur;` |
|   140086 |  5880 | `	if( pInstr->iP1 < 1 ){` |
|   112718 |  5881 | `		pNos = &pTos[-1];` |
|    56360 |  5882 | `	}else{` |
|    27370 |  5883 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  5884 | `	}` |
|        - |  5885 | `#ifdef UNTRUST` |
|        - |  5886 | `	if( pNos < pStack ){` |
|        - |  5887 | `		goto Abort;` |
|        - |  5888 | `	}` |
|        - |  5889 | `#endif` |
|        - |  5890 | `	/* Force a string cast */` |
|   140086 |  5891 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1640 |  5892 | `		PH7_MemObjToString(pNos);` |
|      819 |  5893 | `	}` |
|   140086 |  5894 | `	pCur = &pNos[1];` |
|   282752 |  5895 | `	while( pCur <= pTos ){` |
|   142668 |  5896 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50900 |  5897 | `			PH7_MemObjToString(pCur);` |
|    25449 |  5898 | `		}` |
|        - |  5899 | `		/* Perform the concatenation */` |
|   142668 |  5900 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   142626 |  5901 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    71312 |  5902 | `		}` |
|   142668 |  5903 | `		SyBlobRelease(&pCur->sBlob);` |
|   142668 |  5904 | `		pCur++;` |
|        2 |  5905 | `	}` |
|   140086 |  5906 | `	pTos = pNos;` |
|   140086 |  5907 | `	break;` |
|        - |  5908 | `				}` |
|        - |  5909 | `/*  CAT_STORE: * * *` |
|        - |  5910 | ` *` |
|        - |  5911 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  5912 | ` * back.` |
|        - |  5913 | ` */` |
|     3893 |  5914 | `case PH7_OP_CAT_STORE:{` |
|     7788 |  5915 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5916 | `	ph7_value *pObj;` |
|        - |  5917 | `#ifdef UNTRUST` |
|        - |  5918 | `	if( pNos < pStack ){` |
|        - |  5919 | `		goto Abort;` |
|        - |  5920 | `	}` |
|        - |  5921 | `#endif` |
|     7788 |  5922 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5923 | `		/* Force a string cast */` |
|        3 |  5924 | `		PH7_MemObjToString(pTos);` |
|        1 |  5925 | `	}` |
|     7788 |  5926 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5927 | `		/* Force a string cast */` |
|      ! 0 |  5928 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  5929 | `	}` |
|        - |  5930 | `	/* Perform the concatenation (Reverse order) */` |
|     7788 |  5931 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     7788 |  5932 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     3893 |  5933 | `	}` |
|        - |  5934 | `	/* Perform the store operation */` |
|     7788 |  5935 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5936 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     7788 |  5937 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     7788 |  5938 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     7786 |  5939 | `		PH7_MemObjStore(pTos,pObj);` |
|     3892 |  5940 | `	}` |
|     7786 |  5941 | `	PH7_MemObjStore(pTos,pNos);` |
|     7786 |  5942 | `	VmPopOperand(&pTos,1);` |
|     7786 |  5943 | `	break;` |
|        - |  5944 | `				}` |
|        - |  5945 | `/* OP_AND: * * *` |
|        - |  5946 | ` *` |
|        - |  5947 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  5948 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5949 | ` * stack.` |
|        - |  5950 | ` */` |
|        - |  5951 | `/* OP_OR: * * *` |
|        - |  5952 | ` *` |
|        - |  5953 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  5954 | ` * two values and push the resulting boolean value back onto the` |
|        - |  5955 | ` * stack.` |
|        - |  5956 | ` */` |
|   106822 |  5957 | `case PH7_OP_LAND:` |
|        - |  5958 | `case PH7_OP_LOR: {` |
|   213690 |  5959 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5960 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  5961 | `#ifdef UNTRUST` |
|        - |  5962 | `	if( pNos < pStack ){` |
|        - |  5963 | `		goto Abort;` |
|        - |  5964 | `	}` |
|        - |  5965 | `#endif` |
|        - |  5966 | `	/* Force a boolean cast */` |
|   213690 |  5967 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  5968 | `		PH7_MemObjToBool(pTos);` |
|        1 |  5969 | `	}` |
|   213690 |  5970 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  5971 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  5972 | `	}` |
|   213690 |  5973 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   213690 |  5974 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   213690 |  5975 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  5976 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    97536 |  5977 | `		v1 = and_logic[v1*3+v2];` |
|    48791 |  5978 | `	}else{` |
|        - |  5979 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   116156 |  5980 | `		v1 = or_logic[v1*3+v2];` |
|        - |  5981 | `	}` |
|   213690 |  5982 | `	if( v1 == 2 ){` |
|      ! 0 |  5983 | `		v1 = 1;` |
|      ! 0 |  5984 | `	}` |
|   213690 |  5985 | `	VmPopOperand(&pTos,1);` |
|   213690 |  5986 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   213690 |  5987 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   213690 |  5988 | `	break;` |
|        - |  5989 | `				 }` |
|        - |  5990 | `/*` |
|        - |  5991 | ` * OP_NULLC: * * *` |
|        - |  5992 | ` * Null coalescing operator '??'.` |
|        - |  5993 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  5994 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  5995 | ` */` |
|        - |  5996 | `/*` |
|        - |  5997 | ` * OP_NULLC: * P2 *` |
|        - |  5998 | ` * Short-circuit null coalescing '??'.` |
|        - |  5999 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6000 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6001 | ` */` |
|       52 |  6002 | `case PH7_OP_NULLC: {` |
|        - |  6003 | `#ifdef UNTRUST` |
|        - |  6004 | `	if( pTos < pStack ){` |
|        - |  6005 | `		goto Abort;` |
|        - |  6006 | `	}` |
|        - |  6007 | `#endif` |
|      106 |  6008 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6009 | `		/* Left is not null — keep it and skip the RHS */` |
|       42 |  6010 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       22 |  6011 | `	}else{` |
|        - |  6012 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       66 |  6013 | `		VmPopOperand(&pTos, 1);` |
|        - |  6014 | `	}` |
|      106 |  6015 | `	break;` |
|        - |  6016 |  |
|        - |  6017 | `/*` |
|        - |  6018 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6019 | ` * Null coalescing assignment short-circuit.` |
|        - |  6020 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6021 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6022 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6023 | ` */` |
|       23 |  6024 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6025 | `#ifdef UNTRUST` |
|        - |  6026 | `	if( pTos < pStack ){` |
|        - |  6027 | `		goto Abort;` |
|        - |  6028 | `	}` |
|        - |  6029 | `#endif` |
|       47 |  6030 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       19 |  6031 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|        9 |  6032 | `	}` |
|       47 |  6033 | `	break;` |
|        - |  6034 |  |
|        - |  6035 | `/*` |
|        - |  6036 | ` * OP_NULLC_STORE: * * *` |
|        - |  6037 | ` * Null coalescing assignment store.` |
|        - |  6038 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6039 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6040 | ` * expression result.` |
|        - |  6041 | ` */` |
|        - |  6042 | `/*` |
|        - |  6043 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6044 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6045 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6046 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6047 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6048 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6049 | ` */` |
|       51 |  6050 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6051 | `#ifdef UNTRUST` |
|        - |  6052 | `	if( pTos < pStack ){` |
|        - |  6053 | `		goto Abort;` |
|        - |  6054 | `	}` |
|        - |  6055 | `#endif` |
|      104 |  6056 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6057 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6058 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6059 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6060 | `	}` |
|      104 |  6061 | `	break;` |
|        - |  6062 |  |
|       14 |  6063 | `case PH7_OP_NULLC_STORE: {` |
|       29 |  6064 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6065 | `	ph7_value *pObj;` |
|        - |  6066 | `	sxu32 nIdx;` |
|        - |  6067 | `#ifdef UNTRUST` |
|        - |  6068 | `	if( pNos < pStack ){` |
|        - |  6069 | `		goto Abort;` |
|        - |  6070 | `	}` |
|        - |  6071 | `#endif` |
|       29 |  6072 | `	nIdx = pNos->nIdx;` |
|       29 |  6073 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6074 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6075 | `			"Cannot perform assignment on a constant class attribute");` |
|       29 |  6076 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       29 |  6077 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       29 |  6078 | `		PH7_MemObjStore(pTos,pObj);` |
|       14 |  6079 | `	}` |
|       29 |  6080 | `	PH7_MemObjStore(pTos,pNos);` |
|       29 |  6081 | `	VmPopOperand(&pTos,1);` |
|       29 |  6082 | `	break;` |
|        - |  6083 |  |
|        - |  6084 | `/*` |
|        - |  6085 | ` * OP_SPREAD: * * *` |
|        - |  6086 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6087 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6088 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6089 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6090 | ` */` |
|        9 |  6091 | `case PH7_OP_SPREAD: {` |
|        - |  6092 | `#ifdef UNTRUST` |
|        - |  6093 | `	if( pTos < pStack ){` |
|        - |  6094 | `		goto Abort;` |
|        - |  6095 | `	}` |
|        - |  6096 | `#endif` |
|       20 |  6097 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6098 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6099 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6100 | `		if( nEntry == 0 ){` |
|        - |  6101 | `			/* Empty array — remove from stack */` |
|        3 |  6102 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6103 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6104 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6105 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6106 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6107 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6108 | `				VM_STACK_GUARD);` |
|      ! 0 |  6109 | `		}else{` |
|        - |  6110 | `			ph7_hashmap_node *pNode2;` |
|        - |  6111 | `			ph7_value *pElem;` |
|        - |  6112 | `			sxu32 i;` |
|        - |  6113 | `			/* Overwrite TOS with first element */` |
|       18 |  6114 | `			pNode2 = pMap->pFirst;` |
|       18 |  6115 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6116 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6117 | `			if( pElem ){` |
|       18 |  6118 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6119 | `			}` |
|       18 |  6120 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6121 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6122 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6123 | `			pNode2 = pNode2->pPrev;` |
|        - |  6124 | `			/* Push remaining elements */` |
|       44 |  6125 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6126 | `				pTos++;` |
|       28 |  6127 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6128 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6129 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6130 | `				if( pElem ){` |
|       28 |  6131 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6132 | `				}` |
|       28 |  6133 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6134 | `			}` |
|       18 |  6135 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6136 | `		}` |
|        9 |  6137 | `	}` |
|        - |  6138 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6139 | `	break;` |
|        - |  6140 |  |
|        - |  6141 | `/* OP_LXOR: * * *` |
|        - |  6142 | ` *` |
|        - |  6143 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6144 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6145 | ` * stack.` |
|        - |  6146 | ` * According to the PHP language reference manual:` |
|        - |  6147 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6148 | ` *  TRUE,but not both.` |
|        - |  6149 | ` */` |
|        5 |  6150 | `case PH7_OP_LXOR:{` |
|       11 |  6151 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6152 | `	sxi32 v = 0;` |
|        - |  6153 | `#ifdef UNTRUST` |
|        - |  6154 | `	if( pNos < pStack ){` |
|        - |  6155 | `		goto Abort;` |
|        - |  6156 | `	}` |
|        - |  6157 | `#endif` |
|        - |  6158 | `	/* Force a boolean cast */` |
|       11 |  6159 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6160 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6161 | `	}` |
|       11 |  6162 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6163 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6164 | `	}` |
|       11 |  6165 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6166 | `		v = 1;` |
|        3 |  6167 | `	}` |
|       11 |  6168 | `	VmPopOperand(&pTos,1);` |
|       11 |  6169 | `	pTos->x.iVal = v;` |
|       11 |  6170 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6171 | `	break;` |
|        - |  6172 | `				 }` |
|        - |  6173 | `/* OP_EQ P1 P2 P3` |
|        - |  6174 | ` *` |
|        - |  6175 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6176 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6177 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6178 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6179 | ` */` |
|        - |  6180 | `/* OP_NEQ P1 P2 P3` |
|        - |  6181 | ` *` |
|        - |  6182 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6183 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6184 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6185 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6186 | ` */` |
|     4445 |  6187 | `case PH7_OP_EQ:` |
|        - |  6188 | `case PH7_OP_NEQ: {` |
|     8892 |  6189 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6190 | `	/* Perform the comparison and act accordingly */` |
|        - |  6191 | `#ifdef UNTRUST` |
|        - |  6192 | `	if( pNos < pStack ){` |
|        - |  6193 | `		goto Abort;` |
|        - |  6194 | `	}` |
|        - |  6195 | `#endif` |
|     8892 |  6196 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8892 |  6197 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6198 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8883 |  6199 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8848 |  6200 | `		rc = rc == 0;` |
|     4425 |  6201 | `	}else{` |
|       28 |  6202 | `		rc = rc != 0;` |
|        - |  6203 | `	}` |
|     8892 |  6204 | `	VmPopOperand(&pTos,1);` |
|     8892 |  6205 | `	if( !pInstr->iP2 ){` |
|        - |  6206 | `		/* Push comparison result without taking the jump */` |
|     8892 |  6207 | `		PH7_MemObjRelease(pTos);` |
|     8892 |  6208 | `		pTos->x.iVal = rc;` |
|        - |  6209 | `		/* Invalidate any prior representation */` |
|     8892 |  6210 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4447 |  6211 | `	}else{` |
|      ! 0 |  6212 | `		if( rc ){` |
|        - |  6213 | `			/* Jump to the desired location */` |
|      ! 0 |  6214 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6215 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6216 | `		}` |
|        - |  6217 | `	}` |
|     8892 |  6218 | `	break;` |
|        - |  6219 | `				 }` |
|        - |  6220 | `/* OP_TEQ P1 P2 *` |
|        - |  6221 | ` *` |
|        - |  6222 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6223 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6224 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6225 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6226 | ` */` |
|   156486 |  6227 | `case PH7_OP_TEQ: {` |
|   312974 |  6228 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6229 | `	/* Perform the comparison and act accordingly */` |
|        - |  6230 | `#ifdef UNTRUST` |
|        - |  6231 | `	if( pNos < pStack ){` |
|        - |  6232 | `		goto Abort;` |
|        - |  6233 | `	}` |
|        - |  6234 | `#endif` |
|   312974 |  6235 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   312974 |  6236 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6237 | `		rc = 0;` |
|        2 |  6238 | `	}else{` |
|   312972 |  6239 | `		rc = rc == 0;` |
|        - |  6240 | `	}` |
|   312974 |  6241 | `	VmPopOperand(&pTos,1);` |
|   312974 |  6242 | `	if( !pInstr->iP2 ){` |
|        - |  6243 | `		/* Push comparison result without taking the jump */` |
|   312974 |  6244 | `		PH7_MemObjRelease(pTos);` |
|   312974 |  6245 | `		pTos->x.iVal = rc;` |
|        - |  6246 | `		/* Invalidate any prior representation */` |
|   312974 |  6247 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   156488 |  6248 | `	}else{` |
|      ! 0 |  6249 | `		if( rc ){` |
|        - |  6250 | `			/* Jump to the desired location */` |
|      ! 0 |  6251 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6252 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6253 | `		}` |
|        - |  6254 | `	}` |
|   312974 |  6255 | `	break;` |
|        - |  6256 | `				 }` |
|        - |  6257 | `/* OP_TNE P1 P2 *` |
|        - |  6258 | ` *` |
|        - |  6259 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6260 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6261 | ` * instruction.` |
|        - |  6262 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6263 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6264 | ` *` |
|        - |  6265 | ` */` |
|   120678 |  6266 | `case PH7_OP_TNE: {` |
|   241358 |  6267 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6268 | `	/* Perform the comparison and act accordingly */` |
|        - |  6269 | `#ifdef UNTRUST` |
|        - |  6270 | `	if( pNos < pStack ){` |
|        - |  6271 | `		goto Abort;` |
|        - |  6272 | `	}` |
|        - |  6273 | `#endif` |
|   241358 |  6274 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   241358 |  6275 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6276 | `		rc = 1;` |
|        2 |  6277 | `	}else{` |
|   241356 |  6278 | `		rc = rc != 0;` |
|        - |  6279 | `	}` |
|   241358 |  6280 | `	VmPopOperand(&pTos,1);` |
|   241358 |  6281 | `	if( !pInstr->iP2 ){` |
|        - |  6282 | `		/* Push comparison result without taking the jump */` |
|   241358 |  6283 | `		PH7_MemObjRelease(pTos);` |
|   241358 |  6284 | `		pTos->x.iVal = rc;` |
|        - |  6285 | `		/* Invalidate any prior representation */` |
|   241358 |  6286 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   120680 |  6287 | `	}else{` |
|      ! 0 |  6288 | `		if( rc ){` |
|        - |  6289 | `			/* Jump to the desired location */` |
|      ! 0 |  6290 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6291 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6292 | `		}` |
|        - |  6293 | `	}` |
|   241358 |  6294 | `	break;` |
|        - |  6295 | `				 }` |
|        - |  6296 | `/* OP_LT P1 P2 P3` |
|        - |  6297 | ` *` |
|        - |  6298 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6299 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6300 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6301 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6302 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6303 | ` *` |
|        - |  6304 | ` */` |
|        - |  6305 | `/* OP_LE P1 P2 P3` |
|        - |  6306 | ` *` |
|        - |  6307 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6308 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6309 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6310 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6311 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6312 | ` *` |
|        - |  6313 | ` */` |
|   111863 |  6314 | `case PH7_OP_LT:` |
|        - |  6315 | `case PH7_OP_LE: {` |
|   223772 |  6316 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6317 | `	/* Perform the comparison and act accordingly */` |
|        - |  6318 | `#ifdef UNTRUST` |
|        - |  6319 | `	if( pNos < pStack ){` |
|        - |  6320 | `		goto Abort;` |
|        - |  6321 | `	}` |
|        - |  6322 | `#endif` |
|   223772 |  6323 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   223772 |  6324 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6325 | `		rc = 0;` |
|   223768 |  6326 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1602 |  6327 | `		rc = rc < 1;` |
|      802 |  6328 | `	}else{` |
|   222164 |  6329 | `		rc = rc < 0;` |
|        - |  6330 | `	}` |
|   223772 |  6331 | `	VmPopOperand(&pTos,1);` |
|   223772 |  6332 | `	if( !pInstr->iP2 ){` |
|        - |  6333 | `		/* Push comparison result without taking the jump */` |
|   223772 |  6334 | `		PH7_MemObjRelease(pTos);` |
|   223772 |  6335 | `		pTos->x.iVal = rc;` |
|        - |  6336 | `		/* Invalidate any prior representation */` |
|   223772 |  6337 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   111909 |  6338 | `	}else{` |
|      ! 0 |  6339 | `		if( rc ){` |
|        - |  6340 | `			/* Jump to the desired location */` |
|      ! 0 |  6341 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6342 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6343 | `		}` |
|        - |  6344 | `	}` |
|   223772 |  6345 | `	break;` |
|        - |  6346 | `				}` |
|        - |  6347 | `/* OP_GT P1 P2 P3` |
|        - |  6348 | ` *` |
|        - |  6349 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6350 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6351 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6352 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6353 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6354 | ` *` |
|        - |  6355 | ` */` |
|        - |  6356 | `/* OP_GE P1 P2 P3` |
|        - |  6357 | ` *` |
|        - |  6358 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6359 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6360 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6361 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6362 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6363 | ` *` |
|        - |  6364 | ` */` |
|    55252 |  6365 | `case PH7_OP_GT:` |
|        - |  6366 | `case PH7_OP_GE: {` |
|   110506 |  6367 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6368 | `	/* Perform the comparison and act accordingly */` |
|        - |  6369 | `#ifdef UNTRUST` |
|        - |  6370 | `	if( pNos < pStack ){` |
|        - |  6371 | `		goto Abort;` |
|        - |  6372 | `	}` |
|        - |  6373 | `#endif` |
|   110506 |  6374 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   110506 |  6375 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6376 | `		rc = 0;` |
|   110502 |  6377 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110114 |  6378 | `		rc = rc >= 0;` |
|    55058 |  6379 | `	}else{` |
|      386 |  6380 | `		rc = rc > 0;` |
|        - |  6381 | `	}` |
|   110506 |  6382 | `	VmPopOperand(&pTos,1);` |
|   110506 |  6383 | `	if( !pInstr->iP2 ){` |
|        - |  6384 | `		/* Push comparison result without taking the jump */` |
|   110506 |  6385 | `		PH7_MemObjRelease(pTos);` |
|   110506 |  6386 | `		pTos->x.iVal = rc;` |
|        - |  6387 | `		/* Invalidate any prior representation */` |
|   110506 |  6388 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55254 |  6389 | `	}else{` |
|      ! 0 |  6390 | `		if( rc ){` |
|        - |  6391 | `			/* Jump to the desired location */` |
|      ! 0 |  6392 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6393 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6394 | `		}` |
|        - |  6395 | `	}` |
|   110506 |  6396 | `	break;` |
|        - |  6397 | `				}` |
|        - |  6398 | `/* OP_SPACESHIP * * *` |
|        - |  6399 | ` *` |
|        - |  6400 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6401 | ` *   -1 if left < right` |
|        - |  6402 | ` *    0 if left == right` |
|        - |  6403 | ` *    1 if left > right` |
|        - |  6404 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6405 | ` */` |
|       25 |  6406 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6407 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6408 | `#ifdef UNTRUST` |
|        - |  6409 | `	if( pNos < pStack ){` |
|        - |  6410 | `		goto Abort;` |
|        - |  6411 | `	}` |
|        - |  6412 | `#endif` |
|       51 |  6413 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6414 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6415 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6416 | `		rc = 1;` |
|        4 |  6417 | `	}else{` |
|        - |  6418 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6419 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6420 | `	}` |
|       51 |  6421 | `	VmPopOperand(&pTos,1);` |
|       51 |  6422 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6423 | `	pTos->x.iVal = rc;` |
|       51 |  6424 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6425 | `	break;` |
|        - |  6426 | `				}` |
|        - |  6427 | `/* OP_SEQ P1 P2 *` |
|        - |  6428 | ` * Strict string comparison.` |
|        - |  6429 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6430 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6431 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6432 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6433 | ` * use PH7_OP_EQ.` |
|        - |  6434 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6435 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6436 | ` */` |
|        - |  6437 | `/* OP_SNE P1 P2 *` |
|        - |  6438 | ` * Strict string comparison.` |
|        - |  6439 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6440 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6441 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6442 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6443 | ` * use PH7_OP_EQ.` |
|        - |  6444 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6445 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6446 | ` */` |
|       18 |  6447 | `case PH7_OP_SEQ:` |
|        - |  6448 | `case PH7_OP_SNE: {` |
|       38 |  6449 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6450 | `	SyString s1,s2;` |
|        - |  6451 | `	/* Perform the comparison and act accordingly */` |
|        - |  6452 | `#ifdef UNTRUST` |
|        - |  6453 | `	if( pNos < pStack ){` |
|        - |  6454 | `		goto Abort;` |
|        - |  6455 | `	}` |
|        - |  6456 | `#endif` |
|        - |  6457 | `	/* Force a string cast */` |
|       38 |  6458 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6459 | `		PH7_MemObjToString(pTos);` |
|        2 |  6460 | `	}` |
|       38 |  6461 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6462 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6463 | `	}` |
|       38 |  6464 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6465 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6466 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6467 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6468 | `		rc = rc != 0;` |
|      ! 0 |  6469 | `	}else{` |
|       38 |  6470 | `		rc = rc == 0;` |
|        - |  6471 | `	}` |
|       38 |  6472 | `	VmPopOperand(&pTos,1);` |
|       38 |  6473 | `	if( !pInstr->iP2 ){` |
|        - |  6474 | `		/* Push comparison result without taking the jump */` |
|       38 |  6475 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6476 | `		pTos->x.iVal = rc;` |
|        - |  6477 | `		/* Invalidate any prior representation */` |
|       38 |  6478 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6479 | `	}else{` |
|      ! 0 |  6480 | `		if( rc ){` |
|        - |  6481 | `			/* Jump to the desired location */` |
|      ! 0 |  6482 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6483 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6484 | `		}` |
|        - |  6485 | `	}` |
|       38 |  6486 | `	break;` |
|        - |  6487 | `				 }` |
|        - |  6488 | `/*` |
|        - |  6489 | ` * OP_LOAD_REF * * *` |
|        - |  6490 | ` * Push the index of a referenced object on the stack.` |
|        - |  6491 | ` */` |
|       57 |  6492 | `case PH7_OP_LOAD_REF: {` |
|        - |  6493 | `	sxu32 nIdx;` |
|        - |  6494 | `#ifdef UNTRUST` |
|        - |  6495 | `	if( pTos < pStack ){` |
|        - |  6496 | `		goto Abort;` |
|        - |  6497 | `	}` |
|        - |  6498 | `#endif` |
|        - |  6499 | `	/* Extract memory object index */` |
|      115 |  6500 | `	nIdx = pTos->nIdx;` |
|      115 |  6501 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  6502 | `		/* Nullify the object */` |
|       95 |  6503 | `		PH7_MemObjRelease(pTos);` |
|        - |  6504 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  6505 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  6506 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  6507 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  6508 | `	}` |
|      115 |  6509 | `	break;` |
|        - |  6510 | `					  }` |
|        - |  6511 | `/*` |
|        - |  6512 | ` * OP_STORE_REF * * P3` |
|        - |  6513 | ` * Perform an assignment operation by reference.` |
|        - |  6514 | ` */` |
|       16 |  6515 | ` case PH7_OP_STORE_REF: {` |
|       34 |  6516 | `	 SyString sName = { 0 , 0 };` |
|        - |  6517 | `	 VmFrame *pFrameLocal;` |
|        - |  6518 | `	SyHashEntry *pEntry;` |
|        - |  6519 | `	sxu32 nIdx;` |
|        - |  6520 | `#ifdef UNTRUST` |
|        - |  6521 | `	if( pTos < pStack ){` |
|        - |  6522 | `		goto Abort;` |
|        - |  6523 | `	}` |
|        - |  6524 | `#endif` |
|       34 |  6525 | `	if( pInstr->p3 == 0 ){` |
|        - |  6526 | `		char *zName;` |
|        - |  6527 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  6528 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6529 | `			/* Force a string cast */` |
|      ! 0 |  6530 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6531 | `		}` |
|      ! 0 |  6532 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6533 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  6534 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6535 | `			if( zName ){` |
|      ! 0 |  6536 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6537 | `			}` |
|      ! 0 |  6538 | `		}` |
|      ! 0 |  6539 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  6540 | `		pTos--;` |
|      ! 0 |  6541 | `	}else{` |
|       34 |  6542 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  6543 | `	}` |
|       34 |  6544 | `	nIdx = pTos->nIdx;` |
|       34 |  6545 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  6546 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  6547 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6548 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  6549 | `		}else{` |
|        - |  6550 | `			ph7_value *pObj;` |
|        - |  6551 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  6552 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  6553 | `			if( pObj == 0 ){` |
|      ! 0 |  6554 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  6555 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  6556 | `				goto Abort;` |
|        - |  6557 | `			}` |
|        - |  6558 | `			/* Perform the store operation */` |
|      ! 0 |  6559 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  6560 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  6561 | `		}` |
|       34 |  6562 | `	}else if( sName.nByte > 0){` |
|       34 |  6563 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  6564 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  6565 | `		}else{` |
|       34 |  6566 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  6567 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6568 | `			/* Query the local frame */` |
|       34 |  6569 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  6570 | `			if( pEntry ){` |
|      ! 0 |  6571 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  6572 | `			}else{` |
|       34 |  6573 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  6574 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  6575 | `					/* Insert in the $GLOBALS array */` |
|       30 |  6576 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  6577 | `				}` |
|       34 |  6578 | `				if( rc == SXRET_OK ){` |
|       34 |  6579 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  6580 | `				}` |
|        - |  6581 | `			}` |
|        - |  6582 | `		}` |
|       16 |  6583 | `	}` |
|       34 |  6584 | `	break;` |
|        - |  6585 | `				 }` |
|        - |  6586 | `/*` |
|        - |  6587 | ` * OP_UPLINK P1 * *` |
|        - |  6588 | ` * Link a variable to the top active VM frame.` |
|        - |  6589 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  6590 | ` */` |
|       28 |  6591 | `case PH7_OP_UPLINK: {` |
|       58 |  6592 | `	if( pVm->pFrame->pParent ){` |
|       58 |  6593 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  6594 | `		SyString sName;` |
|        - |  6595 | `		/* Perform the link */` |
|      116 |  6596 | `		while( pLink <= pTos ){` |
|       60 |  6597 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6598 | `				/* Force a string cast */` |
|      ! 0 |  6599 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  6600 | `			}` |
|       60 |  6601 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  6602 | `			if( sName.nByte > 0 ){` |
|       60 |  6603 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  6604 | `			}` |
|       60 |  6605 | `			pLink++;` |
|        2 |  6606 | `		}` |
|       28 |  6607 | `	}` |
|       58 |  6608 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  6609 | `	break;` |
|        - |  6610 | `					}` |
|        - |  6611 | `/*` |
|        - |  6612 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  6613 | ` * Push an exception in the corresponding container so that` |
|        - |  6614 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  6615 | ` */` |
|      150 |  6616 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      302 |  6617 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  6618 | `	VmFrame *pFrameLocal;` |
|        - |  6619 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      302 |  6620 | `	pException->iFinallyDone = 0;` |
|      302 |  6621 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  6622 | `	/* Create the exception frame */` |
|      302 |  6623 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      302 |  6624 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  6625 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  6626 | `		goto Abort;` |
|        - |  6627 | `	}` |
|        - |  6628 | `	/* Mark the special frame */` |
|      302 |  6629 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      302 |  6630 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  6631 | `	/* Point to the frame that trigger the exception */` |
|      302 |  6632 | `	pFrameLocal = pFrameLocal->pParent;` |
|      302 |  6633 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      302 |  6634 | `	pException->pFrame = pFrameLocal;` |
|      302 |  6635 | `	break;` |
|        - |  6636 | `							}` |
|        - |  6637 | `/*` |
|        - |  6638 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  6639 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  6640 | ` */` |
|      149 |  6641 | `case PH7_OP_POP_EXCEPTION: {` |
|      300 |  6642 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      300 |  6643 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  6644 | `		ph7_exception **apException;` |
|        - |  6645 | `		/* Pop the loaded exception */` |
|       32 |  6646 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  6647 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  6648 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  6649 | `		}` |
|       15 |  6650 | `	}` |
|      300 |  6651 | `	pException->pFrame = 0;` |
|        - |  6652 | `	/* Leave the exception frame */` |
|      300 |  6653 | `	VmLeaveFrame(&(*pVm));` |
|        - |  6654 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      300 |  6655 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  6656 | `		sxi32 rcFinally;` |
|       20 |  6657 | `		pException->iFinallyDone = 1;` |
|       20 |  6658 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  6659 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  6660 | `			goto Abort;` |
|        - |  6661 | `		}` |
|        9 |  6662 | `	}` |
|      300 |  6663 | `	break;` |
|        - |  6664 | `							}` |
|        - |  6665 |  |
|        - |  6666 | `/*` |
|        - |  6667 | ` * OP_THROW * P2 *` |
|        - |  6668 | ` * Throw an user exception.` |
|        - |  6669 | ` */` |
|       58 |  6670 | `case PH7_OP_THROW: {` |
|      118 |  6671 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      118 |  6672 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  6673 | `#ifdef UNTRUST` |
|        - |  6674 | `	if( pTos < pStack ){` |
|        - |  6675 | `		goto Abort;` |
|        - |  6676 | `	}` |
|        - |  6677 | `#endif` |
|      118 |  6678 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  6679 | `	/* Tell the upper layer that an exception was thrown */` |
|      118 |  6680 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      118 |  6681 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      118 |  6682 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6683 | `		ph7_class *pThrowable;` |
|        - |  6684 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      118 |  6685 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      119 |  6686 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  6687 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  6688 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  6689 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  6690 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  6691 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  6692 | `			if( pErrorClass ){` |
|        3 |  6693 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  6694 | `			}` |
|        3 |  6695 | `			if( pErrInst ){` |
|        - |  6696 | `				ph7_class_method *pCons;` |
|        3 |  6697 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  6698 | `				if( pCons ){` |
|        - |  6699 | `					ph7_value sArg;` |
|        - |  6700 | `					ph7_value *apArg[1];` |
|        - |  6701 | `					SyString sMsgStr;` |
|        - |  6702 | `					static const char zErrMsg[] =` |
|        - |  6703 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  6704 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  6705 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  6706 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  6707 | `					apArg[0] = &sArg;` |
|        3 |  6708 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  6709 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  6710 | `				}` |
|        3 |  6711 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  6712 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  6713 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6714 | `					goto Abort;` |
|        - |  6715 | `				}` |
|        2 |  6716 | `			}else{` |
|        - |  6717 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  6718 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  6719 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  6720 | `					goto Abort;` |
|        - |  6721 | `				}` |
|        - |  6722 | `			}` |
|        2 |  6723 | `		}else{` |
|        - |  6724 | `			/* Throw the exception */` |
|      116 |  6725 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      116 |  6726 | `			if( rc == SXERR_ABORT ){` |
|        - |  6727 | `				/* Abort processing immediately */` |
|       11 |  6728 | `				goto Abort;` |
|        - |  6729 | `			}` |
|        - |  6730 | `		}` |
|       55 |  6731 | `	}else{` |
|        - |  6732 | `		/* Expecting a class instance */` |
|      ! 0 |  6733 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  6734 | `		if( rc == SXERR_ABORT ){` |
|        - |  6735 | `			/* Abort processing immediately */` |
|      ! 0 |  6736 | `			goto Abort;` |
|        - |  6737 | `		}` |
|        - |  6738 | `	}` |
|        - |  6739 | `	/* Pop the top entry */` |
|      108 |  6740 | `	VmPopOperand(&pTos,1);` |
|        - |  6741 | `	/* Perform an unconditional jump */` |
|      108 |  6742 | `	pc = nJump - 1;` |
|      108 |  6743 | `	break;` |
|        - |  6744 | `				   }` |
|        - |  6745 | `/*` |
|        - |  6746 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  6747 | ` * Prepare a foreach step.` |
|        - |  6748 | ` */` |
|     5930 |  6749 | `case PH7_OP_FOREACH_INIT: {` |
|    11862 |  6750 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6751 | `	void *pName;` |
|        - |  6752 | `#ifdef UNTRUST` |
|        - |  6753 | `	if( pTos < pStack ){` |
|        - |  6754 | `		goto Abort;` |
|        - |  6755 | `	}` |
|        - |  6756 | `#endif` |
|    11862 |  6757 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6758 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  6759 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6760 | `			/* Force a string cast */` |
|      ! 0 |  6761 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6762 | `		}` |
|        - |  6763 | `		/* Duplicate name */` |
|      ! 0 |  6764 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6765 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6766 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6767 | `		}` |
|      ! 0 |  6768 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6769 | `	}` |
|    11862 |  6770 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  6771 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6772 | `			/* Force a string cast */` |
|      ! 0 |  6773 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  6774 | `		}` |
|        - |  6775 | `		/* Duplicate name */` |
|      ! 0 |  6776 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  6777 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6778 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  6779 | `		}` |
|      ! 0 |  6780 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  6781 | `	}` |
|        - |  6782 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    11862 |  6783 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  6784 | `		/* Jump out of the loop */` |
|      ! 0 |  6785 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  6786 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  6787 | `		}` |
|      ! 0 |  6788 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  6789 | `	}else{` |
|        - |  6790 | `		ph7_foreach_step *pStep;` |
|    11862 |  6791 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    11862 |  6792 | `		if( pStep == 0 ){` |
|      ! 0 |  6793 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  6794 | `			/* Jump out of the loop */` |
|      ! 0 |  6795 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6796 | `		}else{` |
|        - |  6797 | `			/* Zero the structure */` |
|    11862 |  6798 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  6799 | `			/* Prepare the step */` |
|    11862 |  6800 | `			pStep->iFlags = pInfo->iFlags;` |
|    11862 |  6801 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  6802 | `				ph7_hashmap *pMap;` |
|        - |  6803 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  6804 | `				 * source array so mutations don't affect other sharers. */` |
|    11830 |  6805 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  6806 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  6807 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  6808 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6809 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  6810 | `						 * variable still points at the same hashmap as` |
|        - |  6811 | `						 * the stack value. */` |
|        9 |  6812 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  6813 | `							pCur->iRef--;` |
|        9 |  6814 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  6815 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  6816 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  6817 | `						}` |
|        4 |  6818 | `					}` |
|        4 |  6819 | `				}` |
|    11830 |  6820 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  6821 | `				/* Reset the internal loop cursor */` |
|    11830 |  6822 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6823 | `				/* Mark the step */` |
|    11830 |  6824 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    11830 |  6825 | `				pStep->xIter.pMap = pMap;` |
|    11830 |  6826 | `				pMap->iRef++;` |
|     5916 |  6827 | `			}else{` |
|       34 |  6828 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  6829 | `				ph7_class *pIteratorClass;` |
|        - |  6830 | `				/* Check if the object implements Iterator */` |
|       34 |  6831 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       45 |  6832 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  6833 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  6834 | `					ph7_class_method *pRewind;` |
|       24 |  6835 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  6836 | `					pStep->xIter.pThis = pThis;` |
|       24 |  6837 | `					pThis->iRef++;` |
|       24 |  6838 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  6839 | `					if( pRewind ){` |
|       24 |  6840 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  6841 | `					}` |
|       13 |  6842 | `				}else{` |
|        - |  6843 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  6844 | `					ph7_class *pIterAggClass;` |
|       12 |  6845 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  6846 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       13 |  6847 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  6848 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  6849 | `						ph7_class_method *pGetIter;` |
|        3 |  6850 | `						int iterAggOk = 0;` |
|        3 |  6851 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  6852 | `						if( pGetIter ){` |
|        - |  6853 | `							ph7_value sResult;` |
|        3 |  6854 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  6855 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  6856 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  6857 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  6858 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  6859 | `									ph7_class_method *pRewind;` |
|        3 |  6860 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  6861 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  6862 | `									pIterObj->iRef++;` |
|        - |  6863 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  6864 | `									pStep->pOwner = pThis;` |
|        3 |  6865 | `									pThis->iRef++;` |
|        3 |  6866 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  6867 | `									if( pRewind ){` |
|        3 |  6868 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  6869 | `									}` |
|        3 |  6870 | `									iterAggOk = 1;` |
|        1 |  6871 | `								}` |
|        1 |  6872 | `							}` |
|        3 |  6873 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  6874 | `						}` |
|        3 |  6875 | `						if( !iterAggOk ){` |
|        - |  6876 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  6877 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6878 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  6879 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  6880 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  6881 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  6882 | `						}` |
|        2 |  6883 | `					}else{` |
|        - |  6884 | `						/* Plain object iteration via hAttr */` |
|        9 |  6885 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|        9 |  6886 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|        9 |  6887 | `						pStep->xIter.pThis = pThis;` |
|        9 |  6888 | `						pThis->iRef++;` |
|        - |  6889 | `					}` |
|        - |  6890 | `				}` |
|        - |  6891 | `			}` |
|        - |  6892 | `		}` |
|    11862 |  6893 | `		if( pStep ){` |
|    11862 |  6894 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  6895 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  6896 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  6897 | `				/* Jump out of the loop */` |
|      ! 0 |  6898 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  6899 | `			}` |
|     5930 |  6900 | `		}` |
|        - |  6901 | `	}` |
|    11862 |  6902 | `	VmPopOperand(&pTos,1);` |
|    11862 |  6903 | `	break;` |
|        - |  6904 | `						  }` |
|        - |  6905 | `/*` |
|        - |  6906 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  6907 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  6908 | ` */` |
|    96946 |  6909 | `case PH7_OP_FOREACH_STEP: {` |
|   193894 |  6910 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  6911 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  6912 | `	ph7_value *pValue;` |
|        - |  6913 | `	VmFrame *pFrameLocal;` |
|        - |  6914 | `	/* Peek the last step */` |
|   193894 |  6915 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   193894 |  6916 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   193894 |  6917 | `	pFrameLocal = pVm->pFrame;` |
|   193894 |  6918 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   193894 |  6919 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   193766 |  6920 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  6921 | `		ph7_hashmap_node *pNode;` |
|        - |  6922 | `		/* Extract the current node value */` |
|   193766 |  6923 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   193766 |  6924 | `		if( pNode == 0 ){` |
|        - |  6925 | `			/* No more entry to process */` |
|    11828 |  6926 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    11828 |  6927 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6928 | `				/* Break the reference with the last element */` |
|        7 |  6929 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  6930 | `			}` |
|        - |  6931 | `			/* Automatically reset the loop cursor */` |
|    11828 |  6932 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  6933 | `			/* Cleanup the mess left behind */` |
|    11828 |  6934 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    11828 |  6935 | `			SySetPop(&pInfo->aStep);` |
|    11828 |  6936 | `			PH7_HashmapUnref(pMap);` |
|     5915 |  6937 | `		}else{` |
|   181940 |  6938 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      426 |  6939 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      426 |  6940 | `				if( pKey ){` |
|      426 |  6941 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      212 |  6942 | `				}` |
|      212 |  6943 | `			}` |
|   181940 |  6944 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  6945 | `				SyHashEntry *pEntry;` |
|        - |  6946 | `				/* Pass by reference */` |
|       23 |  6947 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  6948 | `				if( pEntry ){` |
|       21 |  6949 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  6950 | `				}else{` |
|        4 |  6951 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  6952 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  6953 | `				}` |
|       12 |  6954 | `			}else{` |
|        - |  6955 | `				/* Make a copy of the entry value */` |
|   181918 |  6956 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   181918 |  6957 | `				if( pValue ){` |
|   181918 |  6958 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    90958 |  6959 | `				}` |
|        - |  6960 | `			}` |
|        2 |  6961 | `		}` |
|    97012 |  6962 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  6963 | `		/* Iterator-based iteration.` |
|        - |  6964 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  6965 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  6966 | `		 */` |
|      106 |  6967 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  6968 | `		ph7_class_method *pMethod;` |
|        - |  6969 | `		ph7_value sResult;` |
|      106 |  6970 | `		int isValid = 0;` |
|        - |  6971 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  6972 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  6973 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  6974 | `		}else{` |
|       82 |  6975 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  6976 | `			if( pMethod ){` |
|       82 |  6977 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  6978 | `			}` |
|        - |  6979 | `		}` |
|        - |  6980 | `		/* Call valid() */` |
|      106 |  6981 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  6982 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  6983 | `		if( pMethod ){` |
|      106 |  6984 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  6985 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  6986 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  6987 | `		}` |
|      106 |  6988 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  6989 | `		if( !isValid ){` |
|        - |  6990 | `			/* Iterator exhausted */` |
|       24 |  6991 | `			pc = pInstr->iP2 - 1;` |
|        - |  6992 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  6993 | `			if( pStep->pOwner ){` |
|        3 |  6994 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  6995 | `			}` |
|       24 |  6996 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  6997 | `			SySetPop(&pInfo->aStep);` |
|       24 |  6998 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  6999 | `		}else{` |
|        - |  7000 | `			/* Call current() to get value */` |
|       84 |  7001 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7002 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7003 | `			if( pMethod ){` |
|       84 |  7004 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7005 | `			}` |
|       84 |  7006 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7007 | `			if( pValue ){` |
|       84 |  7008 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7009 | `			}` |
|       84 |  7010 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7011 | `			/* Call key() if needed */` |
|       84 |  7012 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7013 | `				ph7_value sKey;` |
|       35 |  7014 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7015 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7016 | `				if( pMethod ){` |
|       35 |  7017 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7018 | `				}` |
|       35 |  7019 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7020 | `				if( pValue ){` |
|       35 |  7021 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7022 | `				}` |
|       35 |  7023 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7024 | `			}` |
|        - |  7025 | `		}` |
|       54 |  7026 | `	}else{` |
|       25 |  7027 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       25 |  7028 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7029 | `		SyHashEntry *pEntry;` |
|        - |  7030 | `		/* Point to the next attribute */` |
|       29 |  7031 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       21 |  7032 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7033 | `			/* Check access permission */` |
|       31 |  7034 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       20 |  7035 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       17 |  7036 | `					break; /* Access is granted */` |
|        - |  7037 | `			}` |
|        1 |  7038 | `		}` |
|       25 |  7039 | `		if( pEntry == 0 ){` |
|        - |  7040 | `			/* Clean up the mess left behind */` |
|        9 |  7041 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|        9 |  7042 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7043 | `				/* Break the reference with the last element */` |
|        3 |  7044 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7045 | `			}` |
|        9 |  7046 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        9 |  7047 | `			SySetPop(&pInfo->aStep);` |
|        9 |  7048 | `			PH7_ClassInstanceUnref(pThis);` |
|        5 |  7049 | `		}else{` |
|       17 |  7050 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7051 | `			ph7_value *pAttrValue;` |
|       17 |  7052 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7053 | `				/* Fill with the current attribute name */` |
|       17 |  7054 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       17 |  7055 | `				if( pKey ){` |
|       17 |  7056 | `					SyBlobReset(&pKey->sBlob);` |
|       17 |  7057 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       17 |  7058 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|        8 |  7059 | `				}` |
|        8 |  7060 | `			}` |
|        - |  7061 | `			/* Extract attribute value */` |
|       17 |  7062 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       17 |  7063 | `			if( pAttrValue ){` |
|       17 |  7064 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7065 | `					/* Pass by reference */` |
|        3 |  7066 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7067 | `					if( pEntry ){` |
|        3 |  7068 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7069 | `					}else{` |
|      ! 0 |  7070 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7071 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7072 | `					}` |
|        2 |  7073 | `				}else{` |
|        - |  7074 | `					/* Make a copy of the attribute value */` |
|       15 |  7075 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       15 |  7076 | `					if( pValue ){` |
|       15 |  7077 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        7 |  7078 | `					}` |
|        - |  7079 | `				}` |
|        8 |  7080 | `			}` |
|        - |  7081 | `		}` |
|        - |  7082 | `	}` |
|   193894 |  7083 | `	break;` |
|        - |  7084 | `						  }` |
|        - |  7085 | `/*` |
|        - |  7086 | ` * OP_MEMBER P1 P2` |
|        - |  7087 | ` * Load class attribute/method on the stack.` |
|        - |  7088 | ` */` |
|     3629 |  7089 | `case PH7_OP_MEMBER: {` |
|        - |  7090 | `	ph7_class_instance *pThis;` |
|        - |  7091 | `	ph7_value *pNos;` |
|        - |  7092 | `	SyString sName;` |
|     7260 |  7093 | `	if( !pInstr->iP1 ){` |
|     7034 |  7094 | `		pNos = &pTos[-1];` |
|        - |  7095 | `#ifdef UNTRUST` |
|        - |  7096 | `		if( pNos < pStack ){` |
|        - |  7097 | `			goto Abort;` |
|        - |  7098 | `		}` |
|        - |  7099 | `#endif` |
|     7034 |  7100 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7101 | `			ph7_class *pClass;` |
|        - |  7102 | `			/* Class already instantiated */` |
|     7032 |  7103 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7104 | `			/* Point to the instantiated class */` |
|     7032 |  7105 | `			pClass = pThis->pClass;` |
|        - |  7106 | `			/* Extract attribute name first */` |
|     7032 |  7107 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7032 |  7108 | `			if( pInstr->iP2 ){` |
|        - |  7109 | `				/* Method call */` |
|      720 |  7110 | `				ph7_class_method *pMeth = 0;` |
|      720 |  7111 | `				if( sName.nByte > 0 ){` |
|        - |  7112 | `					/* Extract the target method */` |
|      720 |  7113 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      359 |  7114 | `				}` |
|      720 |  7115 | `				if( pMeth == 0 ){` |
|      ! 0 |  7116 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7117 | `						&pClass->sName,&sName` |
|        - |  7118 | `						);` |
|        - |  7119 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7120 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7121 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7122 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7123 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7124 | `				}else{` |
|        - |  7125 | `					/* Push method name on the stack */` |
|      720 |  7126 | `					PH7_MemObjRelease(pTos);` |
|      720 |  7127 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      720 |  7128 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7129 | `				}` |
|      720 |  7130 | `				pTos->nIdx = SXU32_HIGH;` |
|      361 |  7131 | `			}else{` |
|        - |  7132 | `				/* Attribute access */` |
|     6314 |  7133 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7134 | `				SyHashEntry *pEntry;` |
|        - |  7135 | `				/* Extract the target attribute */` |
|     6314 |  7136 | `				if( sName.nByte > 0 ){` |
|     6314 |  7137 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6314 |  7138 | `					if( pEntry ){` |
|        - |  7139 | `						/* Point to the attribute value */` |
|     6312 |  7140 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3155 |  7141 | `					}` |
|     3156 |  7142 | `				}` |
|     6314 |  7143 | `				if( pObjAttr == 0 ){` |
|        - |  7144 | `					/* No such attribute,load null */` |
|        4 |  7145 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7146 | `						&pClass->sName,&sName);` |
|        - |  7147 | `					/* Call the __get magic method if available */` |
|        3 |  7148 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7149 | `				}` |
|     6314 |  7150 | `				VmPopOperand(&pTos,1);` |
|        - |  7151 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7152 | `				 * This is due to the following case:` |
|        - |  7153 | `				 *     (new TestClass())->foo;` |
|        - |  7154 | `				 */` |
|     6314 |  7155 | `				pThis->iRef++;` |
|     6314 |  7156 | `				PH7_MemObjRelease(pTos);` |
|     6314 |  7157 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6314 |  7158 | `				if( pObjAttr ){` |
|     6312 |  7159 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7160 | `					/* Check attribute access */` |
|     6312 |  7161 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7162 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7163 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7164 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7165 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7166 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6310 |  7167 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3194 |  7168 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       76 |  7169 | `							VmInstr *pNext = pInstr + 1;` |
|       76 |  7170 | `							int bIsLhs = 0;` |
|       76 |  7171 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       74 |  7172 | `								bIsLhs = 1;` |
|       36 |  7173 | `							}` |
|       76 |  7174 | `							if( !bIsLhs ){` |
|        3 |  7175 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7176 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7177 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7178 | `									goto Abort;` |
|        - |  7179 | `								}` |
|        - |  7180 | `								{` |
|        3 |  7181 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7182 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7183 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3629 |  7184 | `										break;` |
|        - |  7185 | `									}` |
|        - |  7186 | `								}` |
|      ! 0 |  7187 | `								goto Exception;` |
|        - |  7188 | `							}` |
|       36 |  7189 | `						}` |
|        - |  7190 | `						/* Load attribute */` |
|     6310 |  7191 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6310 |  7192 | `						if( pValue ){` |
|     6310 |  7193 | `							if( pThis->iRef < 2 ){` |
|        - |  7194 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7195 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7196 | `								 */` |
|        7 |  7197 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7198 | `							}else{` |
|        - |  7199 | `								/* Simple load */` |
|     6304 |  7200 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7201 | `							}` |
|     6310 |  7202 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6308 |  7203 | `								if( pThis->iRef > 1 ){` |
|        - |  7204 | `									/* Load attribute index */` |
|     6302 |  7205 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3150 |  7206 | `								}` |
|     3153 |  7207 | `							}` |
|     3154 |  7208 | `						}` |
|     3156 |  7209 | `					}else{` |
|        - |  7210 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7211 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7212 | `						char zMsg[256];` |
|      ! 0 |  7213 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7214 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7215 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7216 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7217 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7218 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7219 | `						goto Abort;` |
|        - |  7220 | `					}` |
|     3154 |  7221 | `				}` |
|        - |  7222 | `				/* Safely unreference the object */` |
|     6312 |  7223 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7224 | `			}` |
|     3516 |  7225 | `		}else{` |
|        3 |  7226 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7227 | `			VmPopOperand(&pTos,1);` |
|        3 |  7228 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7229 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7230 | `		}` |
|     3517 |  7231 | `	}else{` |
|        - |  7232 | `		/* Static member access using class name */` |
|      228 |  7233 | `		pNos = pTos;` |
|      228 |  7234 | `		pThis = 0;` |
|      228 |  7235 | `		if( !pInstr->p3 ){` |
|      190 |  7236 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7237 | `			pNos--;` |
|        - |  7238 | `#ifdef UNTRUST` |
|        - |  7239 | `			if( pNos < pStack ){` |
|        - |  7240 | `				goto Abort;` |
|        - |  7241 | `			}` |
|        - |  7242 | `#endif` |
|       96 |  7243 | `		}else{` |
|        - |  7244 | `			/* Attribute name already computed */` |
|       40 |  7245 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7246 | `		}` |
|      228 |  7247 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7248 | `			ph7_class *pClass = 0;` |
|      228 |  7249 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7250 | `				/* Class already instantiated */` |
|        5 |  7251 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7252 | `				pClass = pThis->pClass;` |
|        5 |  7253 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7254 | `			}else{` |
|        - |  7255 | `				/* Try to extract the target class */` |
|      224 |  7256 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7257 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7258 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7259 | `					/* Handle self/static/parent keywords */` |
|      224 |  7260 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7261 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7262 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7263 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7264 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7265 | `						}` |
|      194 |  7266 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7267 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7268 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7269 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7270 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7271 | `							pClass = pSelf->pBase;` |
|       13 |  7272 | `						}` |
|       15 |  7273 | `					}else{` |
|      112 |  7274 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7275 | `					}` |
|      111 |  7276 | `				}` |
|        - |  7277 | `			}` |
|      228 |  7278 | `			if( pClass == 0 ){` |
|        - |  7279 | `				/* Undefined class */` |
|      ! 0 |  7280 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7281 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7282 | `					);` |
|      ! 0 |  7283 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7284 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7285 | `				}` |
|      ! 0 |  7286 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7287 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7288 | `			}else{` |
|      228 |  7289 | `				if( pInstr->iP2 ){` |
|        - |  7290 | `					/* Method call */` |
|       86 |  7291 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7292 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7293 | `						/* Extract the target method */` |
|       86 |  7294 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7295 | `					}` |
|       86 |  7296 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7297 | `						if( pMeth ){` |
|      ! 0 |  7298 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7299 | `								&pClass->sName,&sName` |
|        - |  7300 | `								);` |
|      ! 0 |  7301 | `						}else{` |
|      ! 0 |  7302 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7303 | `								&pClass->sName,&sName` |
|        - |  7304 | `								);` |
|        - |  7305 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7306 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7307 | `						}` |
|        - |  7308 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7309 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7310 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7311 | `						}` |
|      ! 0 |  7312 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7313 | `					}else{` |
|        - |  7314 | `						/* Push method name on the stack */` |
|       86 |  7315 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7316 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7317 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7318 | `					}` |
|       86 |  7319 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7320 | `				}else{` |
|        - |  7321 | `					/* Attribute access */` |
|      144 |  7322 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7323 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7324 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7325 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7326 | `						/* ::class returns the fully qualified class name */` |
|        - |  7327 | `						/* Pop the attribute name from the stack */` |
|       60 |  7328 | `						if( !pInstr->p3 ){` |
|       60 |  7329 | `							VmPopOperand(&pTos,1);` |
|       29 |  7330 | `						}` |
|       60 |  7331 | `						PH7_MemObjRelease(pTos);` |
|        - |  7332 | `						/* Load the class name */` |
|       60 |  7333 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7334 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7335 | `					}else{` |
|        - |  7336 | `						/* Extract the target attribute */` |
|       86 |  7337 | `						if( sName.nByte > 0 ){` |
|       86 |  7338 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7339 | `						}` |
|       86 |  7340 | `						if( pAttr == 0 ){` |
|        - |  7341 | `							/* No such attribute,load null */` |
|      ! 0 |  7342 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7343 | `								&pClass->sName,&sName);` |
|        - |  7344 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7345 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7346 | `						}` |
|        - |  7347 | `						/* Pop the attribute name from the stack */` |
|       86 |  7348 | `						if( !pInstr->p3 ){` |
|       48 |  7349 | `							VmPopOperand(&pTos,1);` |
|       23 |  7350 | `						}` |
|       86 |  7351 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7352 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7353 | `						if( pAttr ){` |
|       86 |  7354 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7355 | `								/* Access to a non static attribute */` |
|      ! 0 |  7356 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7357 | `									&pClass->sName,&pAttr->sName` |
|        - |  7358 | `									);` |
|      ! 0 |  7359 | `							}else{` |
|        - |  7360 | `								ph7_value *pValue;` |
|        - |  7361 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7362 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7363 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7364 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7365 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7366 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7367 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7368 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7369 | `										if( pS ){` |
|       28 |  7370 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7371 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7372 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7373 | `												int bIsLhs = 0;` |
|        8 |  7374 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7375 | `													bIsLhs = 1;` |
|        2 |  7376 | `												}` |
|        8 |  7377 | `												if( !bIsLhs ){` |
|        3 |  7378 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7379 | `													if( pThis ){` |
|      ! 0 |  7380 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7381 | `													}` |
|        3 |  7382 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7383 | `														goto Abort;` |
|        - |  7384 | `													}` |
|        - |  7385 | `													{` |
|        3 |  7386 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7387 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7388 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7389 | `															break;` |
|        - |  7390 | `														}` |
|        - |  7391 | `													}` |
|      ! 0 |  7392 | `													goto Exception;` |
|        - |  7393 | `												}` |
|        2 |  7394 | `											}` |
|       12 |  7395 | `										}` |
|       12 |  7396 | `									}` |
|        - |  7397 | `									/* Load the desired attribute */` |
|       80 |  7398 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7399 | `									if( pValue ){` |
|       80 |  7400 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7401 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7402 | `											/* Load index number */` |
|       38 |  7403 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7404 | `										}` |
|       39 |  7405 | `									}` |
|       41 |  7406 | `								}else{` |
|        - |  7407 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7408 | `									char zMsg[256];` |
|        5 |  7409 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7410 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7411 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7412 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7413 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7414 | `									}else{` |
|      ! 0 |  7415 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7416 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7417 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7418 | `									}` |
|        5 |  7419 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7420 | `									goto Abort;` |
|        - |  7421 | `								}` |
|        - |  7422 | `							}` |
|       39 |  7423 | `						}` |
|        - |  7424 | `					}` |
|        - |  7425 | `				}` |
|      222 |  7426 | `				if( pThis ){` |
|        - |  7427 | `					/* Safely unreference the object */` |
|        5 |  7428 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7429 | `				}` |
|        - |  7430 | `			}` |
|      112 |  7431 | `		}else{` |
|        - |  7432 | `			/* Pop operands */` |
|      ! 0 |  7433 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7434 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7435 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7436 | `			}` |
|      ! 0 |  7437 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7438 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7439 | `		}` |
|        - |  7440 | `	}` |
|     7252 |  7441 | `	break;` |
|        - |  7442 | `					}` |
|        - |  7443 | `/*` |
|        - |  7444 | ` * OP_NEW P1 * * *` |
|        - |  7445 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7446 | ` */` |
|      568 |  7447 | `case PH7_OP_NEW: {` |
|     1138 |  7448 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1138 |  7449 | `	ph7_class *pClass = 0;` |
|        - |  7450 | `	ph7_class_instance *pNew;` |
|     1138 |  7451 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7452 | `		/* Try to extract the desired class */` |
|     1706 |  7453 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1136 |  7454 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      568 |  7455 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7456 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7457 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7458 | `	}` |
|     1138 |  7459 | `	if( pClass == 0 ){` |
|        - |  7460 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7461 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7462 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7463 | `			);` |
|        - |  7464 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7465 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7466 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7467 | `			/* Pop given arguments */` |
|      ! 0 |  7468 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7469 | `		}` |
|      ! 0 |  7470 | `		goto Abort;` |
|      ! 0 |  7471 | `	}else{` |
|        - |  7472 | `		ph7_class_method *pCons;` |
|        - |  7473 | `		/* Create a new class instance */` |
|     1138 |  7474 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1138 |  7475 | `		if( pNew == 0 ){` |
|      ! 0 |  7476 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7477 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7478 | `				&pClass->sName` |
|        - |  7479 | `			);` |
|      ! 0 |  7480 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7481 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7482 | `				/* Pop given arguments */` |
|      ! 0 |  7483 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7484 | `			}` |
|      ! 0 |  7485 | `			break;` |
|        - |  7486 | `		}` |
|        - |  7487 | `		/* Check if a constructor is available */` |
|     1138 |  7488 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1138 |  7489 | `		if( pCons == 0 ){` |
|      830 |  7490 | `			SyString *pName = &pClass->sName;` |
|        - |  7491 | `			/* Check for a constructor with the same base class name */` |
|      830 |  7492 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      414 |  7493 | `		}` |
|     1138 |  7494 | `		if( pCons ){` |
|        - |  7495 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  7496 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  7497 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  7498 | `			 * (including variadic string-key packing). */` |
|      310 |  7499 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      310 |  7500 | `			SySetReset(&aArg);` |
|      608 |  7501 | `			while( pArg < pTos ){` |
|      300 |  7502 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      300 |  7503 | `				pArg++;` |
|        2 |  7504 | `			}` |
|      310 |  7505 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  7506 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  7507 | `				sxu32 n;` |
|       65 |  7508 | `				n = SySetUsed(&aArg);` |
|        - |  7509 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  7510 | `				 * for named args the missing-arg check happens downstream` |
|        - |  7511 | `				 * after resolution). */` |
|      113 |  7512 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       49 |  7513 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       49 |  7514 | `					if( pFuncArg ){` |
|       49 |  7515 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  7516 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  7517 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  7518 | `						}` |
|       24 |  7519 | `					}` |
|       49 |  7520 | `					n++;` |
|        1 |  7521 | `				}` |
|       32 |  7522 | `			}` |
|      310 |  7523 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  7524 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      310 |  7525 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  7526 | `				pNew->iRef = 1;` |
|      ! 0 |  7527 | `			}` |
|      154 |  7528 | `		}` |
|     1138 |  7529 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7530 | `			/* Pop given arguments */` |
|      246 |  7531 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      122 |  7532 | `		}` |
|     1138 |  7533 | `		PH7_MemObjRelease(pTos);` |
|     1138 |  7534 | `		pTos->x.pOther = pNew;` |
|     1138 |  7535 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7536 | `	}` |
|     1138 |  7537 | `	break;` |
|        - |  7538 | `				 }` |
|        - |  7539 | `/*` |
|        - |  7540 | ` * OP_CLONE * * *` |
|        - |  7541 | ` * Perfome a clone operation.` |
|        - |  7542 | ` */` |
|       24 |  7543 | `case PH7_OP_CLONE: {` |
|        - |  7544 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  7545 | `#ifdef UNTRUST` |
|        - |  7546 | `	if( pTos < pStack ){` |
|        - |  7547 | `		goto Abort;` |
|        - |  7548 | `	}` |
|        - |  7549 | `#endif` |
|        - |  7550 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  7551 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  7552 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7553 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  7554 | `		PH7_MemObjRelease(pTos);` |
|        5 |  7555 | `		break;` |
|        - |  7556 | `	}` |
|        - |  7557 | `	/* Point to the source */` |
|       46 |  7558 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7559 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  7560 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  7561 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7562 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  7563 | `			&pSrc->pClass->sName);` |
|      ! 0 |  7564 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7565 | `		break;` |
|        - |  7566 | `	}` |
|        - |  7567 | `	/* Perform the clone operation */` |
|       46 |  7568 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  7569 | `	PH7_MemObjRelease(pTos);` |
|       46 |  7570 | `	if( pClone == 0 ){` |
|      ! 0 |  7571 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7572 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  7573 | `	}else{` |
|        - |  7574 | `		/* Load the cloned object */` |
|       46 |  7575 | `		pTos->x.pOther = pClone;` |
|       46 |  7576 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  7577 | `	}` |
|       46 |  7578 | `	break;` |
|        - |  7579 | `				   }` |
|        - |  7580 | `/*` |
|        - |  7581 | ` * OP_SWITCH * * P3` |
|        - |  7582 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  7583 | ` */` |
|       26 |  7584 | `case PH7_OP_SWITCH: {` |
|       54 |  7585 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  7586 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  7587 | `	ph7_value sValue,sCaseValue;` |
|        - |  7588 | `	sxu32 n,nEntry;` |
|        - |  7589 | `#ifdef UNTRUST` |
|        - |  7590 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  7591 | `		goto Abort;` |
|        - |  7592 | `	}` |
|        - |  7593 | `#endif` |
|        - |  7594 | `	/* Point to the case table  */` |
|       54 |  7595 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  7596 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  7597 | `	/* Select the appropriate case block to execute */` |
|       54 |  7598 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  7599 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  7600 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  7601 | `		pCase = &aCase[n];` |
|      130 |  7602 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  7603 | `		/* Execute the case expression first */` |
|      130 |  7604 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  7605 | `		/* Compare the two expression */` |
|      130 |  7606 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  7607 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  7608 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  7609 | `		if( rc == 0 ){` |
|        - |  7610 | `			/* Value match,jump to this block */` |
|       52 |  7611 | `			pc = pCase->nStart - 1;` |
|       52 |  7612 | `			break;` |
|        - |  7613 | `		}` |
|       41 |  7614 | `	}` |
|       54 |  7615 | `	VmPopOperand(&pTos,1);` |
|       54 |  7616 | `	if( n >= nEntry ){` |
|        - |  7617 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  7618 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  7619 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  7620 | `		}else{` |
|        - |  7621 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  7622 | `			pc = pSwitch->nOut - 1;` |
|        - |  7623 | `		}` |
|        1 |  7624 | `	}` |
|       54 |  7625 | `	break;` |
|        - |  7626 | `					}` |
|        - |  7627 | `/*` |
|        - |  7628 | ` * OP_MATCH * * P3` |
|        - |  7629 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  7630 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  7631 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  7632 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  7633 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  7634 | ` */` |
|       54 |  7635 | `case PH7_OP_MATCH: {` |
|      110 |  7636 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  7637 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  7638 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  7639 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  7640 | `	int matched = 0;` |
|        - |  7641 | `#ifdef UNTRUST` |
|        - |  7642 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  7643 | `		goto Abort;` |
|        - |  7644 | `	}` |
|        - |  7645 | `#endif` |
|      110 |  7646 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  7647 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  7648 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  7649 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  7650 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  7651 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  7652 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  7653 | `		pArm = &aArm[i];` |
|      240 |  7654 | `		if( pArm->bDefault ){` |
|       13 |  7655 | `			pDefault = pArm;` |
|       13 |  7656 | `			continue;` |
|        - |  7657 | `		}` |
|      228 |  7658 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  7659 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  7660 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  7661 | `			if( pCondBc == 0 ){` |
|      ! 0 |  7662 | `				continue;` |
|        - |  7663 | `			}` |
|      260 |  7664 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  7665 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  7666 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  7667 | `			if( rc == 0 ){` |
|       93 |  7668 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  7669 | `				matched = 1;` |
|       93 |  7670 | `				break;` |
|        - |  7671 | `			}` |
|       85 |  7672 | `		}` |
|      115 |  7673 | `	}` |
|      110 |  7674 | `	if( !matched && pDefault ){` |
|       13 |  7675 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  7676 | `		matched = 1;` |
|        6 |  7677 | `	}` |
|      110 |  7678 | `	if( !matched ){` |
|        5 |  7679 | `		const char *zType = "unknown";` |
|        - |  7680 | `		char zMsg[128];` |
|        - |  7681 | `		sxu32 nMsg;` |
|        5 |  7682 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  7683 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  7684 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  7685 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  7686 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  7687 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  7688 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  7689 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  7690 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  7691 | `		default: break;` |
|        - |  7692 | `		}` |
|        7 |  7693 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  7694 | `			"Unhandled match case of type %s",zType);` |
|        7 |  7695 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  7696 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  7697 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  7698 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  7699 | `		goto Abort;` |
|        - |  7700 | `	}` |
|      105 |  7701 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  7702 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  7703 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  7704 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  7705 | `	break;` |
|        - |  7706 | `					}` |
|        - |  7707 | `/*` |
|        - |  7708 | ` * OP_YIELD P1 P2 *` |
|        - |  7709 | ` *  Yield a value from a generator function.` |
|        - |  7710 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  7711 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  7712 | ` */` |
|       34 |  7713 | `case PH7_OP_YIELD: {` |
|        - |  7714 | `	ph7_generator *pGen;` |
|       70 |  7715 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  7716 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  7717 | `		goto Abort;` |
|        - |  7718 | `	}` |
|       70 |  7719 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  7720 | `	if( pInstr->iP2 ){` |
|        - |  7721 | `		/* yield $key => $value: value on top, key below */` |
|        - |  7722 | `#ifdef UNTRUST` |
|        - |  7723 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  7724 | `#endif` |
|        7 |  7725 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  7726 | `		VmPopOperand(&pTos, 1);` |
|        7 |  7727 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  7728 | `		VmPopOperand(&pTos, 1);` |
|        - |  7729 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  7730 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  7731 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  7732 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  7733 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  7734 | `			}` |
|        1 |  7735 | `		}` |
|       67 |  7736 | `	}else if( pInstr->iP1 ){` |
|        - |  7737 | `		/* yield $value */` |
|        - |  7738 | `#ifdef UNTRUST` |
|        - |  7739 | `		if( pTos < pStack ) goto Abort;` |
|        - |  7740 | `#endif` |
|       64 |  7741 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  7742 | `		VmPopOperand(&pTos, 1);` |
|        - |  7743 | `		/* Auto-increment key */` |
|       64 |  7744 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  7745 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  7746 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  7747 | `	}else{` |
|        - |  7748 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  7749 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  7750 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  7751 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  7752 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  7753 | `	}` |
|        - |  7754 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  7755 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  7756 | `	goto Suspend;` |
|        - |  7757 |  |
|        - |  7758 | `/*` |
|        - |  7759 | ` * OP_CALL P1 * *` |
|        - |  7760 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  7761 | ` *  function on the stack.` |
|        - |  7762 | ` */` |
|   344805 |  7763 | `case PH7_OP_CALL: {` |
|   689656 |  7764 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  7765 | `	ph7_value *pArg;` |
|   689656 |  7766 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   689656 |  7767 | `	pArg = &pTos[-nCallArgs];` |
|        - |  7768 | `	SyHashEntry *pEntry;` |
|        - |  7769 | `	SyString sName;` |
|        - |  7770 | `	/* Extract function name */` |
|   689656 |  7771 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       78 |  7772 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7773 | `			ph7_value sResult;` |
|      ! 0 |  7774 | `			SySetReset(&aArg);` |
|      ! 0 |  7775 | `			while( pArg < pTos ){` |
|      ! 0 |  7776 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  7777 | `				pArg++;` |
|      ! 0 |  7778 | `			}` |
|      ! 0 |  7779 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  7780 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  7781 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  7782 | `			SySetReset(&aArg);` |
|        - |  7783 | `			/* Pop given arguments */` |
|      ! 0 |  7784 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7785 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7786 | `			}` |
|        - |  7787 | `			/* Copy result */` |
|      ! 0 |  7788 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  7789 | `			PH7_MemObjRelease(&sResult);` |
|       78 |  7790 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       78 |  7791 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7792 | `			ph7_value sResult;` |
|        - |  7793 | `			sxi32 rcInv;` |
|       78 |  7794 | `			SySetReset(&aArg);` |
|      192 |  7795 | `			while( pArg < pTos ){` |
|      116 |  7796 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      116 |  7797 | `				pArg++;` |
|        2 |  7798 | `			}` |
|       78 |  7799 | `			PH7_MemObjInit(pVm,&sResult);` |
|      116 |  7800 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       76 |  7801 | `				(int)SySetUsed(&aArg),` |
|       76 |  7802 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  7803 | `				&sResult,` |
|       76 |  7804 | `				(VmCallArgMap *)pInstr->p3);` |
|       78 |  7805 | `			SySetReset(&aArg);` |
|       78 |  7806 | `			if( nCallArgs > 0 ){` |
|       74 |  7807 | `				VmPopOperand(&pTos,nCallArgs);` |
|       36 |  7808 | `			}` |
|       78 |  7809 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  7810 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  7811 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  7812 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  7813 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  7814 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  7815 | `				pThis->iRef++;` |
|       13 |  7816 | `				PH7_MemObjRelease(pTos);` |
|       13 |  7817 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  7818 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  7819 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7820 | `					goto Abort;` |
|        - |  7821 | `				}` |
|        - |  7822 | `				{` |
|       13 |  7823 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  7824 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  7825 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  7826 | `						pc = pFrm2->iExceptionJump - 1;` |
|       13 |  7827 | `						break;` |
|        - |  7828 | `					}` |
|        - |  7829 | `				}` |
|      ! 0 |  7830 | `				goto Exception;` |
|        - |  7831 | `			}` |
|       66 |  7832 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  7833 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  7834 | `		}else{` |
|        - |  7835 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  7836 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  7837 | `			/* Pop given arguments */` |
|      ! 0 |  7838 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7839 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7840 | `			}` |
|        - |  7841 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  7842 | `			PH7_MemObjRelease(pTos);` |
|        - |  7843 | `		}` |
|       66 |  7844 | `		break;` |
|        - |  7845 | `	}` |
|   689580 |  7846 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  7847 | `	/* Check for a compiled function first.` |
|        - |  7848 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  7849 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   689580 |  7850 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  7851 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  7852 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  7853 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  7854 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  7855 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  7856 | `	{` |
|   689580 |  7857 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   689580 |  7858 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  7859 | `		const char *zFunc;` |
|        - |  7860 | `		const char *zEnd;` |
|        - |  7861 | `		const char *z;` |
|        - |  7862 | `		SyString sGlobal;` |
|       22 |  7863 | `		zFunc = sName.zString;` |
|       22 |  7864 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  7865 | `		z = zEnd;` |
|        - |  7866 | `		/* Find last namespace separator */` |
|      194 |  7867 | `		while( z > zFunc ){` |
|      194 |  7868 | `			if( z[-1] == '\\' ){` |
|       22 |  7869 | `				break;` |
|        - |  7870 | `			}` |
|      174 |  7871 | `			z--;` |
|        2 |  7872 | `		}` |
|       22 |  7873 | `		if( z > zFunc && z < zEnd ){` |
|        - |  7874 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  7875 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  7876 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  7877 | `		}` |
|       10 |  7878 | `	}` |
|        - |  7879 | `	} /* end VmCallArgMap namespace scope */` |
|   689580 |  7880 | `	if( pEntry ){` |
|        - |  7881 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  7882 | `		ph7_class_instance *pThis;` |
|        - |  7883 | `		ph7_value *pFrameStack;` |
|        - |  7884 | `		ph7_vm_func *pVmFunc;` |
|        - |  7885 | `		ph7_class *pSelf;` |
|        - |  7886 | `		VmFrame *pFrame;` |
|        - |  7887 | `		ph7_value *pObj;` |
|        - |  7888 | `		VmSlot sArg;` |
|        - |  7889 | `		sxu32 n;` |
|        - |  7890 | `		/* initialize fields */` |
|    17460 |  7891 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    17460 |  7892 | `		pThis = 0;` |
|    17460 |  7893 | `		pSelf = 0;` |
|    17460 |  7894 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  7895 | `			ph7_class_method *pMeth;` |
|        - |  7896 | `			/* Class method call */` |
|     2952 |  7897 | `			ph7_value *pTarget = &pTos[-1];` |
|     2952 |  7898 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  7899 | `				/* Extract the 'this' pointer */` |
|     2952 |  7900 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  7901 | `					/* Instance already loaded */` |
|     2862 |  7902 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     2862 |  7903 | `					pThis->iRef++;` |
|     2862 |  7904 | `					pSelf = pThis->pClass;` |
|     1430 |  7905 | `				}` |
|     2952 |  7906 | `				if( pSelf == 0 ){` |
|       92 |  7907 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  7908 | `						/* "Late Static Binding" class name */` |
|      128 |  7909 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  7910 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  7911 | `					}` |
|       92 |  7912 | `					if( pSelf == 0 ){` |
|       21 |  7913 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  7914 | `					}` |
|       45 |  7915 | `				}` |
|     2952 |  7916 | `				if( pThis == 0  ){` |
|       92 |  7917 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  7918 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  7919 | `					if( pFrameLocal->pParent ){` |
|        - |  7920 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  7921 | `						pThis = pFrameLocal->pThis;` |
|       66 |  7922 | `						if( pThis ){` |
|       21 |  7923 | `							pThis->iRef++;` |
|       10 |  7924 | `						}` |
|       32 |  7925 | `					}` |
|       45 |  7926 | `				}` |
|     2952 |  7927 | `				VmPopOperand(&pTos,1);` |
|     2952 |  7928 | `				PH7_MemObjRelease(pTos);` |
|        - |  7929 | `				/* Synchronize pointers */` |
|     2952 |  7930 | `				pArg = &pTos[-nCallArgs];` |
|        - |  7931 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  7932 | `				 * user have already computed the random generated unique class method name` |
|        - |  7933 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  7934 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  7935 | `				 */` |
|     2952 |  7936 | `				while( pArg < pStack ){` |
|      ! 0 |  7937 | `					pArg++;` |
|      ! 0 |  7938 | `				}` |
|     2952 |  7939 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  7940 | `					/* Check if the call is allowed */` |
|     2952 |  7941 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     2952 |  7942 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  7943 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  7944 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  7945 | `							char zMsg[256];` |
|      ! 0 |  7946 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7947 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  7948 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  7949 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  7950 | `							/* Pop given arguments */` |
|      ! 0 |  7951 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  7952 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7953 | `							}` |
|      ! 0 |  7954 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7955 | `							goto Abort;` |
|        - |  7956 | `						}` |
|        6 |  7957 | `					}` |
|     1475 |  7958 | `				}` |
|     1475 |  7959 | `			}` |
|     1475 |  7960 | `		}` |
|        - |  7961 | `		/* Check The recursion limit */` |
|    17460 |  7962 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  7963 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7964 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  7965 | `				&pVmFunc->sName);` |
|        - |  7966 | `			/* Pop given arguments */` |
|        3 |  7967 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  7968 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  7969 | `			}` |
|        - |  7970 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  7971 | `			PH7_MemObjRelease(pTos);` |
|       14 |  7972 | `			break;` |
|        - |  7973 | `		}` |
|    17458 |  7974 | `		if( pVmFunc->pNextName ){` |
|        - |  7975 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  7976 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  7977 | `		}` |
|    17458 |  7978 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  7979 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  7980 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  7981 | `			ph7_generator *pGenerator;` |
|        - |  7982 | `			ph7_class_instance *pGenObj;` |
|        - |  7983 | `			ph7_value *pCtxAttr;` |
|        - |  7984 | `			SyString sAttrName;` |
|        - |  7985 | `			ph7_value **apCallArgs;` |
|        - |  7986 | `			int nGenArgs, iArg;` |
|        - |  7987 | `			/* Collect arguments from the operand stack */` |
|       24 |  7988 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  7989 | `			apCallArgs = 0;` |
|       24 |  7990 | `			if( nGenArgs > 0 ){` |
|       14 |  7991 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  7992 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  7993 | `				if( apCallArgs == 0 ){` |
|        - |  7994 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  7995 | `					nGenArgs = 0;` |
|      ! 0 |  7996 | `				}else{` |
|       10 |  7997 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  7998 | `					int didReorder = 0;` |
|       10 |  7999 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8000 | `						/* Named-argument reordering for generator */` |
|        5 |  8001 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8002 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8003 | `						sxu32 nNV = nF;` |
|        5 |  8004 | `						sxi32 iVIdx = -1;` |
|        - |  8005 | `						sxi32 *aGSlot;` |
|        - |  8006 | `						sxu8 *aGUsed;` |
|        - |  8007 | `						sxu32 gi;` |
|       13 |  8008 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8009 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8010 | `						}` |
|        7 |  8011 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8012 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8013 | `						if( aGSlot ){` |
|        5 |  8014 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8015 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8016 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8017 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8018 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8019 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8020 | `								goto Abort;` |
|        - |  8021 | `							}` |
|        - |  8022 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8023 | `							 * append overflow (variadic / positional beyond` |
|        - |  8024 | `							 * formals) so downstream sees every argument. */` |
|        - |  8025 | `							{` |
|        5 |  8026 | `								int nOut = 0;` |
|       13 |  8027 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8028 | `									sxu32 gj;` |
|       13 |  8029 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8030 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8031 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8032 | `											break;` |
|        - |  8033 | `										}` |
|        3 |  8034 | `									}` |
|        5 |  8035 | `								}` |
|       13 |  8036 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8037 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8038 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8039 | `									}` |
|        5 |  8040 | `								}` |
|        5 |  8041 | `								nGenArgs = nOut;` |
|        - |  8042 | `							}` |
|        5 |  8043 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8044 | `							didReorder = 1;` |
|        2 |  8045 | `						}` |
|        - |  8046 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8047 | `						 * positional fill below — preserves arg order rather` |
|        - |  8048 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8049 | `					}` |
|       10 |  8050 | `					if( !didReorder ){` |
|       12 |  8051 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8052 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8053 | `						}` |
|        2 |  8054 | `					}` |
|        - |  8055 | `				}` |
|        4 |  8056 | `			}` |
|        - |  8057 | `			/* Create execution context and generator wrapper */` |
|       24 |  8058 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8059 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8060 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8061 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8062 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8063 | `				break;` |
|        - |  8064 | `			}` |
|       24 |  8065 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8066 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8067 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8068 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8069 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8070 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8071 | `				break;` |
|        - |  8072 | `			}` |
|        - |  8073 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8074 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8075 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8076 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8077 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8078 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8079 | `			if( apCallArgs ){` |
|       10 |  8080 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8081 | `			}` |
|       24 |  8082 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8083 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8084 | `				if( pThis ){` |
|      ! 0 |  8085 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8086 | `				}` |
|      ! 0 |  8087 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8088 | `					goto Abort;` |
|        - |  8089 | `				}` |
|      ! 0 |  8090 | `				break;` |
|        - |  8091 | `			}` |
|        - |  8092 | `			/* Create Generator class instance */` |
|       24 |  8093 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8094 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8095 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8096 | `				break;` |
|        - |  8097 | `			}` |
|        - |  8098 | `			/* Store generator in __ctx attribute */` |
|       24 |  8099 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8100 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8101 | `			if( pCtxAttr ){` |
|       24 |  8102 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8103 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8104 | `			}` |
|        - |  8105 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8106 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8107 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8108 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8109 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8110 | `			pGenObj->iRef++;` |
|       24 |  8111 | `			if( pThis ){` |
|      ! 0 |  8112 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8113 | `			}` |
|       24 |  8114 | `			break;` |
|        - |  8115 | `		}` |
|        - |  8116 | `		/* Extract the formal argument set */` |
|    17436 |  8117 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8118 | `		/* Create a new VM frame  */` |
|    17436 |  8119 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    17436 |  8120 | `		if( rc != SXRET_OK ){` |
|        - |  8121 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8122 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8123 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8124 | `				&pVmFunc->sName);` |
|        - |  8125 | `			/* Pop given arguments */` |
|      ! 0 |  8126 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8127 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8128 | `			}` |
|        - |  8129 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8130 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8131 | `			break;` |
|        - |  8132 | `		}` |
|    17436 |  8133 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8134 | `			/* Install the '$this' variable */` |
|        - |  8135 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     2880 |  8136 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     2880 |  8137 | `			if( pObj ){` |
|        - |  8138 | `				/* Reflect the change */` |
|     2880 |  8139 | `				pObj->x.pOther = pThis;` |
|     2880 |  8140 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1439 |  8141 | `			}` |
|     1439 |  8142 | `		}` |
|    17436 |  8143 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8144 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8145 | `			/* Install static variables */` |
|      ! 0 |  8146 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8147 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8148 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8149 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8150 | `					/* Initialize the static variables */` |
|      ! 0 |  8151 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8152 | `					if( pObj ){` |
|        - |  8153 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8154 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8155 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8156 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8157 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8158 | `						}` |
|      ! 0 |  8159 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8160 | `					}else{` |
|      ! 0 |  8161 | `						continue;` |
|        - |  8162 | `					}` |
|      ! 0 |  8163 | `				}` |
|        - |  8164 | `				/* Install in the current frame */` |
|      ! 0 |  8165 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8166 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8167 | `			}` |
|      ! 0 |  8168 | `		}` |
|        - |  8169 | `		/* Push arguments in the local frame */` |
|        - |  8170 | `		{` |
|    17436 |  8171 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8172 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8173 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    17436 |  8174 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    17436 |  8175 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8176 | `			/* ============================================================` |
|        - |  8177 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8178 | `			 *` |
|        - |  8179 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8180 | `			 * or position, then install them in the frame.` |
|        - |  8181 | `			 * ============================================================ */` |
|       96 |  8182 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8183 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8184 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8185 | `			sxu32 nNonVariadic;` |
|        - |  8186 | `			sxi32 *aSlot;` |
|        - |  8187 | `			sxu8  *aUsed;` |
|        - |  8188 | `			sxu32 i;` |
|        - |  8189 | `			/* Find variadic parameter index */` |
|      292 |  8190 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8191 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8192 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8193 | `					break;` |
|        - |  8194 | `				}` |
|      100 |  8195 | `			}` |
|       96 |  8196 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8197 | `			/* Allocate mapping arrays */` |
|      143 |  8198 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8199 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8200 | `			if( aSlot == 0 ){` |
|      ! 0 |  8201 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8202 | `				goto Abort;` |
|        - |  8203 | `			}` |
|       96 |  8204 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8205 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8206 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8207 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8208 | `			if( rc == PH7_ABORT ){` |
|        7 |  8209 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8210 | `				goto Abort;` |
|        - |  8211 | `			}` |
|        - |  8212 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8213 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8214 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8215 | `				sxi32 iSrc = -1;` |
|      309 |  8216 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8217 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8218 | `						iSrc = (sxi32)i;` |
|      169 |  8219 | `						break;` |
|        - |  8220 | `					}` |
|       62 |  8221 | `				}` |
|      187 |  8222 | `				if( iSrc >= 0 ){` |
|        - |  8223 | `					/* Argument was provided — install with type checking */` |
|      169 |  8224 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8225 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8226 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8227 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8228 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8229 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8230 | `					}` |
|        - |  8231 | `					/* Type checking: union types */` |
|      169 |  8232 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8233 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8234 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8235 | `							bCallIsStrict);` |
|       13 |  8236 | `						if( rcU != SXRET_OK ){` |
|        - |  8237 | `							const char *zGiven;` |
|      ! 0 |  8238 | `							const char *zExpected = "union";` |
|        - |  8239 | `							char zBuf[128];` |
|        - |  8240 | `							char zTypeBuf[128];` |
|      ! 0 |  8241 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8242 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8243 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8244 | `								zGiven = "null";` |
|      ! 0 |  8245 | `							}else{` |
|      ! 0 |  8246 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8247 | `							}` |
|      ! 0 |  8248 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8249 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8250 | `							}` |
|      ! 0 |  8251 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8252 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8253 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8254 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8255 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8256 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8257 | `							pFrameStack = 0;` |
|      ! 0 |  8258 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8259 | `							goto SkipFuncBody;` |
|        - |  8260 | `						}` |
|      171 |  8261 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8262 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8263 | `						/* Scalar/class type checking */` |
|       17 |  8264 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8265 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8266 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8267 | `							if( pClass ){` |
|      ! 0 |  8268 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8269 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8270 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8271 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8272 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8273 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8274 | `									}` |
|      ! 0 |  8275 | `								}else{` |
|      ! 0 |  8276 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8277 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8278 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8279 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8280 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8281 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8282 | `									}` |
|        - |  8283 | `								}` |
|      ! 0 |  8284 | `							}` |
|       17 |  8285 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8286 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8287 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8288 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8289 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8290 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8291 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8292 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8293 | `								pFrameStack = 0;` |
|      ! 0 |  8294 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8295 | `								goto SkipFuncBody;` |
|        7 |  8296 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8297 | `								char zTypeBuf[128];` |
|      ! 0 |  8298 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8299 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8300 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8301 | `									ph7_type_name(pVal));` |
|      ! 0 |  8302 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8303 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8304 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8305 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8306 | `								pFrameStack = 0;` |
|      ! 0 |  8307 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8308 | `								goto SkipFuncBody;` |
|        - |  8309 | `							}` |
|        3 |  8310 | `						}` |
|        8 |  8311 | `					}` |
|        - |  8312 | `					/* Install: by reference or by value */` |
|      169 |  8313 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8314 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8315 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8316 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8317 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8318 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8319 | `							}` |
|      ! 0 |  8320 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8321 | `						}else{` |
|        7 |  8322 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8323 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8324 | `							if( pRefEntry == 0 ){` |
|        7 |  8325 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8326 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8327 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8328 | `								sArg.pUserData = 0;` |
|        5 |  8329 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8330 | `							}` |
|        5 |  8331 | `							pObj = 0;` |
|        - |  8332 | `						}` |
|        3 |  8333 | `					}else{` |
|      165 |  8334 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8335 | `					}` |
|      169 |  8336 | `					if( pObj ){` |
|      165 |  8337 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8338 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8339 | `						sArg.pUserData = 0;` |
|      165 |  8340 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8341 | `					}` |
|       85 |  8342 | `				}else{` |
|        - |  8343 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8344 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8345 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8346 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8347 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8348 | `						if( pObj ){` |
|       19 |  8349 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8350 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8351 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8352 | `							sArg.pUserData = 0;` |
|       19 |  8353 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8354 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8355 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8356 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8357 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8358 | `							}` |
|        9 |  8359 | `						}` |
|        9 |  8360 | `					}` |
|        - |  8361 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8362 | `				}` |
|       94 |  8363 | `			}` |
|        - |  8364 | `			/* Handle variadic parameter */` |
|       89 |  8365 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8366 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8367 | `				if( pObj ){` |
|        9 |  8368 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8369 | `					{` |
|        9 |  8370 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8371 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8372 | `							if( aSlot[i] == -1 ){` |
|       16 |  8373 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8374 | `									/* Named variadic entry: insert with string key */` |
|        - |  8375 | `									ph7_value sKey;` |
|       11 |  8376 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8377 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8378 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8379 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8380 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8381 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8382 | `								}else{` |
|        - |  8383 | `									/* Positional variadic entry */` |
|      ! 0 |  8384 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8385 | `								}` |
|        5 |  8386 | `							}` |
|       12 |  8387 | `						}` |
|        - |  8388 | `					}` |
|        9 |  8389 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8390 | `					sArg.pUserData = 0;` |
|        9 |  8391 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8392 | `				}` |
|        5 |  8393 | `			}else{` |
|        - |  8394 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8395 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8396 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8397 | `				 * the positional-only path's behavior. */` |
|       81 |  8398 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8399 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8400 | `					if( aSlot[i] == -2 ){` |
|        - |  8401 | `						char zAnonBuf[32];` |
|        - |  8402 | `						SyString sAnonName;` |
|      ! 0 |  8403 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8404 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8405 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8406 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8407 | `						if( pObj ){` |
|      ! 0 |  8408 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8409 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8410 | `							sArg.pUserData = 0;` |
|      ! 0 |  8411 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8412 | `						}` |
|      ! 0 |  8413 | `						nAnon++;` |
|      ! 0 |  8414 | `					}` |
|       79 |  8415 | `				}` |
|        - |  8416 | `			}` |
|        - |  8417 | `			/* Release all stack arguments */` |
|      267 |  8418 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  8419 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  8420 | `			}` |
|       89 |  8421 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8422 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  8423 | `			n = nFormal;` |
|       45 |  8424 | `		}else{` |
|        - |  8425 | `		/* ============================================================` |
|        - |  8426 | `		 * Positional-only matching path (original)` |
|        - |  8427 | `		 * ============================================================ */` |
|    17342 |  8428 | `		n = 0;` |
|    46470 |  8429 | `		while( pArg < pTos ){` |
|    29200 |  8430 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8431 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  8432 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  8433 | `				if( pObj ){` |
|        - |  8434 | `					/* Initialize as empty array */` |
|       40 |  8435 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8436 | `					{` |
|       40 |  8437 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  8438 | `						while( pArg < pTos ){` |
|        - |  8439 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8440 | `							 *` |
|        - |  8441 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8442 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8443 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8444 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8445 | `							 * fixing both wants a separate counter for elements` |
|        - |  8446 | `							 * already packed into the variadic array. */` |
|      114 |  8447 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8448 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8449 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8450 | `									bCallIsStrict);` |
|       16 |  8451 | `								if( rcU != SXRET_OK ){` |
|        - |  8452 | `									const char *zGiven;` |
|        3 |  8453 | `									const char *zExpected = "union";` |
|        - |  8454 | `									char zBuf[128];` |
|        - |  8455 | `									char zTypeBuf[128];` |
|        3 |  8456 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8457 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8458 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8459 | `										zGiven = "null";` |
|      ! 0 |  8460 | `									}else{` |
|        3 |  8461 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8462 | `									}` |
|        3 |  8463 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8464 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8465 | `									}` |
|        4 |  8466 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8467 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8468 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8469 | `										goto Abort;` |
|        - |  8470 | `									}` |
|        3 |  8471 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8472 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8473 | `									pFrameStack = 0;` |
|        3 |  8474 | `									rc = PH7_EXCEPTION;` |
|        3 |  8475 | `									goto SkipFuncBody;` |
|        - |  8476 | `								}` |
|       14 |  8477 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8478 | `								pArg++;` |
|       14 |  8479 | `								continue;` |
|        - |  8480 | `							}` |
|        - |  8481 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8482 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  8483 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8484 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8485 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8486 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8487 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8488 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8489 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8490 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8491 | `										goto Abort;` |
|        - |  8492 | `									}` |
|        - |  8493 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  8494 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8495 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8496 | `									pFrameStack = 0;` |
|      ! 0 |  8497 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8498 | `									goto SkipFuncBody;` |
|       13 |  8499 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8500 | `									char zTypeBuf[128];` |
|      ! 0 |  8501 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8502 | `										&aFormalArg[n].sName,` |
|      ! 0 |  8503 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8504 | `										ph7_type_name(pArg));` |
|      ! 0 |  8505 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8506 | `										goto Abort;` |
|        - |  8507 | `									}` |
|      ! 0 |  8508 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  8509 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8510 | `									pFrameStack = 0;` |
|      ! 0 |  8511 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  8512 | `									goto SkipFuncBody;` |
|        - |  8513 | `								}` |
|        6 |  8514 | `							}` |
|      100 |  8515 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  8516 | `							pArg++;` |
|        2 |  8517 | `						}` |
|        - |  8518 | `					}` |
|       38 |  8519 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  8520 | `					sArg.pUserData = 0;` |
|       38 |  8521 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8522 | `				}` |
|       38 |  8523 | `				break; /* All remaining args consumed */` |
|        - |  8524 | `			}` |
|    29162 |  8525 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    28978 |  8526 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       33 |  8527 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  8528 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  8529 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  8530 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8531 | `						goto Abort;` |
|        - |  8532 | `					}` |
|      ! 0 |  8533 | `				}` |
|        - |  8534 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    28980 |  8535 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  8536 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  8537 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  8538 | `						bCallIsStrict);` |
|       60 |  8539 | `					if( rcU != SXRET_OK ){` |
|        - |  8540 | `						const char *zGiven;` |
|       19 |  8541 | `						const char *zExpected = "union";` |
|        - |  8542 | `						char zBuf[128];` |
|        - |  8543 | `						char zTypeBuf[128];` |
|       19 |  8544 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  8545 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  8546 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  8547 | `							zGiven = "null";` |
|        5 |  8548 | `						}else{` |
|        5 |  8549 | `							zGiven = ph7_type_name(pArg);` |
|        - |  8550 | `						}` |
|       19 |  8551 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  8552 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  8553 | `						}` |
|       28 |  8554 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  8555 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  8556 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  8557 | `							goto Abort;` |
|        - |  8558 | `						}` |
|       19 |  8559 | `						PH7_MemObjRelease(pTos);` |
|       19 |  8560 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  8561 | `						pFrameStack = 0;` |
|       19 |  8562 | `						rc = PH7_EXCEPTION;` |
|       19 |  8563 | `						goto SkipFuncBody;` |
|        - |  8564 | `					}` |
|       21 |  8565 | `				}else` |
|        - |  8566 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  8567 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    28946 |  8568 | `				if( aFormalArg[n].nType > 0` |
|    15163 |  8569 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1378 |  8570 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  8571 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  8572 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  8573 | `						ph7_class *pClass;` |
|        - |  8574 | `						/* Try to extract the desired class */` |
|       26 |  8575 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  8576 | `						if( pClass ){` |
|       22 |  8577 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8578 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8579 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8580 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8581 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8582 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8583 | `								}` |
|      ! 0 |  8584 | `							}else{` |
|        - |  8585 | `								/* reuse pThis declared in outer scope */` |
|       22 |  8586 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  8587 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  8588 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  8589 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8590 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8591 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8592 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  8593 | `								}` |
|        - |  8594 | `							}` |
|       12 |  8595 | `						}` |
|     1366 |  8596 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  8597 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8598 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  8599 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  8600 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  8601 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8602 | `								goto Abort;` |
|        - |  8603 | `							}` |
|        - |  8604 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  8605 | `							PH7_MemObjRelease(pTos);` |
|       11 |  8606 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  8607 | `							pFrameStack = 0;` |
|       11 |  8608 | `							rc = PH7_EXCEPTION;` |
|       11 |  8609 | `							goto SkipFuncBody;` |
|       14 |  8610 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8611 | `							char zTypeBuf[128];` |
|        7 |  8612 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  8613 | `								&aFormalArg[n].sName,` |
|        4 |  8614 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  8615 | `								ph7_type_name(pArg));` |
|        5 |  8616 | `							if( rc == PH7_ABORT ){` |
|        5 |  8617 | `								goto Abort;` |
|        - |  8618 | `							}` |
|      ! 0 |  8619 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8620 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8621 | `							pFrameStack = 0;` |
|      ! 0 |  8622 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8623 | `							goto SkipFuncBody;` |
|        - |  8624 | `						}` |
|        4 |  8625 | `					}` |
|      681 |  8626 | `				}` |
|    28948 |  8627 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  8628 | `					/* Pass by reference */` |
|       58 |  8629 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  8630 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  8631 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  8632 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8633 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8634 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8635 | `						}` |
|        - |  8636 | `						/* Switch to pass by value */` |
|      ! 0 |  8637 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8638 | `					}else{` |
|        - |  8639 | `						SyHashEntry *pRefEntry;` |
|        - |  8640 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  8641 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  8642 | `						if( pRefEntry == 0 ){` |
|       86 |  8643 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  8644 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  8645 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  8646 | `							sArg.pUserData = 0;` |
|       58 |  8647 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  8648 | `						}` |
|       58 |  8649 | `						pObj = 0;` |
|        - |  8650 | `					}` |
|       30 |  8651 | `				}else{` |
|        - |  8652 | `					/* Pass by value,make a copy of the given argument */` |
|    28892 |  8653 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8654 | `				}` |
|    14475 |  8655 | `			}else{` |
|        - |  8656 | `				char zName[32];` |
|        - |  8657 | `				SyString sArgName;` |
|        - |  8658 | `				/* Set a dummy name */` |
|      184 |  8659 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      184 |  8660 | `				sArgName.zString = zName;` |
|        - |  8661 | `				/* Annonymous argument */` |
|      184 |  8662 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  8663 | `			}` |
|    29130 |  8664 | `			if( pObj ){` |
|    29074 |  8665 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  8666 | `				/* Insert argument index  */` |
|    29074 |  8667 | `				sArg.nIdx = pObj->nIdx;` |
|    29074 |  8668 | `				sArg.pUserData = 0;` |
|    29074 |  8669 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    14536 |  8670 | `			}` |
|    29130 |  8671 | `			PH7_MemObjRelease(pArg);` |
|    29130 |  8672 | `			pArg++;` |
|    29130 |  8673 | `			++n;` |
|        2 |  8674 | `		}` |
|        - |  8675 | `		} /* end named vs positional branch */` |
|        - |  8676 | `		/* Set up closure environment */` |
|    17396 |  8677 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  8678 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  8679 | `			ph7_value *pValue;` |
|        - |  8680 | `			sxu32 iEnv;` |
|      120 |  8681 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      306 |  8682 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      188 |  8683 | `				pEnv = &aEnv[iEnv];` |
|      188 |  8684 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  8685 | `					/* Do not install null value */` |
|      114 |  8686 | `					continue;` |
|        - |  8687 | `				}` |
|       76 |  8688 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  8689 | `				if( pValue == 0 ){` |
|      ! 0 |  8690 | `					continue;` |
|        - |  8691 | `				}` |
|        - |  8692 | `				/* Invalidate any prior representation */` |
|       76 |  8693 | `				PH7_MemObjRelease(pValue);` |
|        - |  8694 | `				/* Duplicate bound variable value */` |
|       76 |  8695 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  8696 | `			}` |
|       59 |  8697 | `		}` |
|        - |  8698 | `		/* Process default values for remaining formal parameters */` |
|    20058 |  8699 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2710 |  8700 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8701 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  8702 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  8703 | `				if( pObj ){` |
|       48 |  8704 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  8705 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  8706 | `					sArg.pUserData = 0;` |
|       48 |  8707 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  8708 | `				}` |
|       48 |  8709 | `				n++;` |
|       48 |  8710 | `				break; /* Variadic is always last */` |
|        - |  8711 | `			}` |
|     2664 |  8712 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2658 |  8713 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2658 |  8714 | `				if( pObj ){` |
|        - |  8715 | `					/* Evaluate the default value and extract it's result */` |
|     2658 |  8716 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2658 |  8717 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  8718 | `						goto Abort;` |
|        - |  8719 | `					}` |
|        - |  8720 | `					/* Insert argument index */` |
|     2658 |  8721 | `					sArg.nIdx = pObj->nIdx;` |
|     2658 |  8722 | `					sArg.pUserData = 0;` |
|     2658 |  8723 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  8724 | `					/* Make sure the default argument is of the correct type */` |
|     2656 |  8725 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1750 |  8726 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  8727 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  8728 | `						/* Cast to the desired type */` |
|        3 |  8729 | `						xCast(pObj);` |
|        1 |  8730 | `					}` |
|     1328 |  8731 | `				}` |
|     1328 |  8732 | `			}` |
|     2664 |  8733 | `			++n;` |
|        2 |  8734 | `		}` |
|        - |  8735 | `		} /* end VmCallArgMap scope */` |
|        - |  8736 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  8737 | `		 * does not return anything.` |
|        - |  8738 | `		 */` |
|    17396 |  8739 | `		PH7_MemObjRelease(pTos);` |
|    17396 |  8740 | `		pTos = &pTos[-nCallArgs];` |
|        - |  8741 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    17396 |  8742 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    17396 |  8743 | `		if( pFrameStack == 0 ){` |
|        - |  8744 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8745 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8746 | `				&pVmFunc->sName);` |
|      ! 0 |  8747 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8748 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8749 | `			}` |
|      ! 0 |  8750 | `			break;` |
|        - |  8751 | `		}` |
|     8697 |  8752 | `SkipFuncBody:` |
|    17426 |  8753 | `		if( pSelf ){` |
|        - |  8754 | `			/* Push class name */` |
|     2950 |  8755 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1474 |  8756 | `		}` |
|        - |  8757 | `		/* Increment nesting level */` |
|    17426 |  8758 | `		pVm->nRecursionDepth++;` |
|    17426 |  8759 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  8760 | `			/* Execute function body */` |
|    26093 |  8761 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    17394 |  8762 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8697 |  8763 | `		}` |
|        - |  8764 | `		/* Decrement nesting level */` |
|    17426 |  8765 | `		pVm->nRecursionDepth--;` |
|    17426 |  8766 | `		if( pSelf ){` |
|        - |  8767 | `			/* Pop class name */` |
|     2950 |  8768 | `			(void)SySetPop(&pVm->aSelf);` |
|     1474 |  8769 | `		}` |
|        - |  8770 | `		/* Cleanup the mess left behind */` |
|    17426 |  8771 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  8772 | `			/* Return by reference,reflect that */` |
|        9 |  8773 | `			if( n != SXU32_HIGH ){` |
|        9 |  8774 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  8775 | `				sxu32 i;` |
|        - |  8776 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  8777 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  8778 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  8779 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  8780 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8781 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8782 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  8783 | `								&pVmFunc->sName);` |
|      ! 0 |  8784 | `						}` |
|      ! 0 |  8785 | `						n = SXU32_HIGH;` |
|      ! 0 |  8786 | `						break;` |
|        - |  8787 | `					}` |
|        3 |  8788 | `				}` |
|        5 |  8789 | `			}else{` |
|      ! 0 |  8790 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  8791 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  8792 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  8793 | `						&pVmFunc->sName);` |
|      ! 0 |  8794 | `				}` |
|        - |  8795 | `			}` |
|        9 |  8796 | `			pTos->nIdx = n;` |
|        4 |  8797 | `		}` |
|        - |  8798 | `		/* Cleanup the mess left behind */` |
|    17426 |  8799 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  8800 | `			/* An exception was throw in this frame */` |
|       48 |  8801 | `			pFrame = pFrame->pParent;` |
|       48 |  8802 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  8803 | `				/* Pop the resutlt */` |
|       46 |  8804 | `				VmPopOperand(&pTos,1);` |
|        - |  8805 | `				/* Jump to this destination */` |
|       46 |  8806 | `				pc = pFrame->iExceptionJump - 1;` |
|       46 |  8807 | `				rc = PH7_OK;` |
|       24 |  8808 | `			}else{` |
|        3 |  8809 | `				if( pFrame->pParent ){` |
|        3 |  8810 | `					rc = PH7_EXCEPTION;` |
|        2 |  8811 | `				}else{` |
|        - |  8812 | `					/* Continue normal execution */` |
|      ! 0 |  8813 | `					rc = PH7_OK;` |
|        - |  8814 | `				}` |
|        - |  8815 | `			}` |
|       23 |  8816 | `		}` |
|        - |  8817 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    17426 |  8818 | `		if( pFrameStack ){` |
|    17396 |  8819 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8697 |  8820 | `		}` |
|        - |  8821 | `		/* Leave the frame */` |
|    17426 |  8822 | `		VmLeaveFrame(&(*pVm));` |
|    17426 |  8823 | `		if( rc == PH7_ABORT ){` |
|        - |  8824 | `			/* Abort processing immeditaley */` |
|       15 |  8825 | `			goto Abort;` |
|    17412 |  8826 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8827 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  8828 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  8829 | `			 * overwriting the state saved by the inner level.` |
|        - |  8830 | `			 * pTos points to the result slot (not yet written).` |
|        - |  8831 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  8832 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  8833 | `			goto Suspend;` |
|    17374 |  8834 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  8835 | `			goto Exception;` |
|        - |  8836 | `		}` |
|     8687 |  8837 | `	}else{` |
|        - |  8838 | `		ph7_user_func *pFunc;` |
|        - |  8839 | `		ph7_context sCtx;` |
|        - |  8840 | `		ph7_value sRet;` |
|        - |  8841 | `		/* Look for an installed foreign function.` |
|        - |  8842 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  8843 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  8844 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  8845 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   672122 |  8846 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8847 | `		{` |
|   672122 |  8848 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   672122 |  8849 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  8850 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  8851 | `			const char *zShort = sName.zString;` |
|        - |  8852 | `			sxu32 i;` |
|      334 |  8853 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  8854 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  8855 | `					zShort = &sName.zString[i + 1];` |
|       13 |  8856 | `				}` |
|      158 |  8857 | `			}` |
|       22 |  8858 | `			if( zShort != sName.zString ){` |
|       22 |  8859 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  8860 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  8861 | `			}` |
|       10 |  8862 | `		}` |
|        - |  8863 | `		} /* end VmCallArgMap namespace scope */` |
|   672122 |  8864 | `		if( pEntry == 0 ){` |
|        - |  8865 | `			/* Call to undefined function */` |
|        5 |  8866 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  8867 | `			/* Pop given arguments */` |
|        5 |  8868 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  8869 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  8870 | `			}` |
|        - |  8871 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  8872 | `			PH7_MemObjRelease(pTos);` |
|       43 |  8873 | `			break;` |
|        - |  8874 | `		}` |
|   672118 |  8875 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  8876 | `		/* Start collecting function arguments */` |
|   672118 |  8877 | `		SySetReset(&aArg);` |
|  1809108 |  8878 | `		while( pArg < pTos ){` |
|  1136992 |  8879 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1136992 |  8880 | `			pArg++;` |
|        2 |  8881 | `		}` |
|        - |  8882 | `		/* Assume a null return value */` |
|   672118 |  8883 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  8884 | `		/* Init the call context */` |
|   672118 |  8885 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  8886 | `		/* Call the foreign function */` |
|   672118 |  8887 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  8888 | `		/* Release the call context */` |
|   672118 |  8889 | `		VmReleaseCallContext(&sCtx);` |
|   672118 |  8890 | `		if( rc == PH7_ABORT ){` |
|      489 |  8891 | `			goto Abort;` |
|   671630 |  8892 | `		}else if( rc == PH7_EXCEPTION ){` |
|       82 |  8893 | `			VmFrame *pFrm = pVm->pFrame;` |
|       82 |  8894 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       82 |  8895 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  8896 | `				/* Exception was NOT caught, propagate */` |
|        5 |  8897 | `				goto Exception;` |
|        - |  8898 | `			}` |
|        - |  8899 | `			/* Exception was caught: pop args and the result slot */` |
|       77 |  8900 | `			PH7_MemObjRelease(&sRet);` |
|       77 |  8901 | `			if( pInstr->iP1 > 0 ){` |
|       61 |  8902 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       30 |  8903 | `			}` |
|        - |  8904 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|       77 |  8905 | `			VmPopOperand(&pTos,1);` |
|        - |  8906 | `			/* Jump past the try/catch block via the exception frame */` |
|       77 |  8907 | `			pFrm = pVm->pFrame;` |
|       77 |  8908 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|       77 |  8909 | `				pc = pFrm->iExceptionJump - 1;` |
|       38 |  8910 | `			}` |
|       77 |  8911 | `			break;` |
|        - |  8912 | `		}` |
|   671550 |  8913 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  8914 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  8915 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  8916 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  8917 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  8918 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  8919 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  8920 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  8921 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  8922 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  8923 | `			}` |
|        - |  8924 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  8925 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  8926 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  8927 | `			goto Suspend;` |
|        - |  8928 | `		}` |
|   671512 |  8929 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8930 | `			/* Pop function name and arguments */` |
|   650228 |  8931 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   325135 |  8932 | `		}` |
|        - |  8933 | `		/* Save foreign function return value */` |
|   671512 |  8934 | `		PH7_MemObjStore(&sRet,pTos);` |
|   671512 |  8935 | `		PH7_MemObjRelease(&sRet);` |
|        - |  8936 | `	}` |
|   688882 |  8937 | `	break;` |
|        - |  8938 | `				  }` |
|        - |  8939 | `/*` |
|        - |  8940 | ` * OP_CONSUME: P1 * *` |
|        - |  8941 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  8942 | ` */` |
|    14474 |  8943 | `case PH7_OP_CONSUME: {` |
|    28950 |  8944 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    28950 |  8945 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  8946 |  |
|    28950 |  8947 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    28950 |  8948 | `	pCur = pOut;` |
|        - |  8949 | `	/* Start the consume process  */` |
|    57898 |  8950 | `	while( pOut <= pTos ){` |
|        - |  8951 | `		/* Force a string cast */` |
|    28950 |  8952 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      684 |  8953 | `			PH7_MemObjToString(pOut);` |
|      341 |  8954 | `		}` |
|    28950 |  8955 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  8956 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  8957 | `			/* Invoke the output consumer callback */` |
|    16982 |  8958 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    16982 |  8959 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    16982 |  8960 | `			SyBlobRelease(&pOut->sBlob);` |
|    16982 |  8961 | `			if( rc == SXERR_ABORT ){` |
|        - |  8962 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  8963 | `				goto Abort;` |
|        - |  8964 | `			}` |
|     8490 |  8965 | `		}` |
|    28950 |  8966 | `		pOut++;` |
|        2 |  8967 | `	}` |
|    28950 |  8968 | `	pTos = &pCur[-1];` |
|    28948 |  8969 | `	break;` |
|        - |  8970 | `					 }` |
|        - |  8971 |  |
|        - |  8972 | `		} /* Switch() */` |
| 11435446 |  8973 | `		pc++; /* Next instruction in the stream */` |
|        2 |  8974 | `	} /* For(;;) */` |
|    20859 |  8975 | `Done:` |
|    41720 |  8976 | `	SySetRelease(&aArg);` |
|    41720 |  8977 | `	return SXRET_OK;` |
|       72 |  8978 | `Suspend:` |
|      146 |  8979 | `	SySetRelease(&aArg);` |
|      146 |  8980 | `	return PH7_SUSPEND;` |
|      268 |  8981 | `Abort:` |
|      537 |  8982 | `	SySetRelease(&aArg);` |
|     1833 |  8983 | `	while( pTos >= pStack ){` |
|     1297 |  8984 | `		PH7_MemObjRelease(pTos);` |
|     1297 |  8985 | `		pTos--;` |
|        1 |  8986 | `	}` |
|      537 |  8987 | `	return PH7_ABORT;` |
|        3 |  8988 | `Exception:` |
|        8 |  8989 | `	SySetRelease(&aArg);` |
|       22 |  8990 | `	while( pTos >= pStack ){` |
|       16 |  8991 | `		PH7_MemObjRelease(pTos);` |
|       16 |  8992 | `		pTos--;` |
|        2 |  8993 | `	}` |
|        8 |  8994 | `	return PH7_EXCEPTION;` |
|    21204 |  8995 |  |
|        - |  8996 | `/*` |
|        - |  8997 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  8998 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  8999 | ` * See block-comment on that function for additional information.` |
|        - |  9000 | ` */` |
|    19556 |  9001 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9002 |  |
|        - |  9003 | `	ph7_value *pStack;` |
|        - |  9004 | `	sxi32 rc;` |
|        - |  9005 | `	/* Allocate a new operand stack */` |
|    19558 |  9006 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    19558 |  9007 | `	if( pStack == 0 ){` |
|      ! 0 |  9008 | `		return SXERR_MEM;` |
|        - |  9009 | `	}` |
|        - |  9010 | `	/* Execute the program */` |
|    19558 |  9011 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9012 | `	/* Free the operand stack */` |
|    19558 |  9013 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9014 | `	/* Execution result */` |
|    19558 |  9015 | `	return rc;` |
|     9780 |  9016 |  |
|        - |  9017 | `/*` |
|        - |  9018 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9019 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9020 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9021 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9022 | ` * execution ends.` |
|        - |  9023 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9024 | ` * additional information.` |
|        - |  9025 | ` */` |
|     2670 |  9026 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9027 |  |
|        - |  9028 | `	VmShutdownCB *pEntry;` |
|        - |  9029 | `	ph7_value *apArg[10];` |
|        - |  9030 | `	sxu32 n,nEntry;` |
|        - |  9031 | `	int i;` |
|        - |  9032 | `	/* Point to the stack of registered callbacks */` |
|     2672 |  9033 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    29372 |  9034 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    26702 |  9035 | `		apArg[i] = 0;` |
|    13352 |  9036 | `	}` |
|     2674 |  9037 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  9038 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9039 | `		if( pEntry ){` |
|        - |  9040 | `			/* Prepare callback arguments if any */` |
|        3 |  9041 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9042 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9043 | `					break;` |
|        - |  9044 | `				}` |
|      ! 0 |  9045 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9046 | `			}` |
|        - |  9047 | `			/* Invoke the callback */` |
|        3 |  9048 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9049 | `			/*` |
|        - |  9050 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9051 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9052 | `			 */` |
|        3 |  9053 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9054 | `			if( pEntry ){` |
|        3 |  9055 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  9056 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9057 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9058 | `				}` |
|        1 |  9059 | `			}` |
|        1 |  9060 | `		}` |
|        2 |  9061 | `	}` |
|     2672 |  9062 | `	SySetReset(&pVm->aShutdown);` |
|     2672 |  9063 |  |
|        - |  9064 | `/*` |
|        - |  9065 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9066 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9067 | ` * See block-comment on that function for additional information.` |
|        - |  9068 | ` */` |
|     2678 |  9069 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9070 |  |
|        - |  9071 | `	/* Make sure we are ready to execute this program */` |
|     2680 |  9072 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9073 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9074 | `	}` |
|        - |  9075 | `	/* Set the execution magic number  */` |
|     2680 |  9076 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9077 | `	/* Execute the program */` |
|     2680 |  9078 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9079 | `	/* Invoke any shutdown callbacks */` |
|     2676 |  9080 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9081 | `	/*` |
|        - |  9082 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9083 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9084 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9085 | `	 */` |
|     2676 |  9086 | `	return SXRET_OK;` |
|     1341 |  9087 |  |
|        - |  9088 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9089 | `/*` |
|        - |  9090 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9091 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9092 | ` */` |
|       46 |  9093 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9094 |  |
|        - |  9095 | `	ph7_exec_ctx *pCtx;` |
|        - |  9096 | `	ph7_value *pStack;` |
|        - |  9097 | `	VmFrame *pFrame;` |
|       48 |  9098 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9099 | `	if( pCtx == 0 ){` |
|      ! 0 |  9100 | `		return 0;` |
|        - |  9101 | `	}` |
|       48 |  9102 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9103 | `	pCtx->pVm = pVm;` |
|       48 |  9104 | `	pCtx->pFunc = pFunc;` |
|       48 |  9105 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9106 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9107 | `	pCtx->pc = 0;` |
|       48 |  9108 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9109 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9110 | `	/* Allocate a private operand stack */` |
|       48 |  9111 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9112 | `	if( pStack == 0 ){` |
|      ! 0 |  9113 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9114 | `		return 0;` |
|        - |  9115 | `	}` |
|       48 |  9116 | `	pCtx->pStack = pStack;` |
|        - |  9117 | `	/* Create a detached frame for the fiber */` |
|       48 |  9118 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9119 | `	if( pFrame == 0 ){` |
|      ! 0 |  9120 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9121 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9122 | `		return 0;` |
|        - |  9123 | `	}` |
|       48 |  9124 | `	pCtx->pFrame = pFrame;` |
|       48 |  9125 | `	return pCtx;` |
|       25 |  9126 |  |
|        - |  9127 | `/*` |
|        - |  9128 | ` * Start executing a fiber context for the first time.` |
|        - |  9129 | ` */` |
|       46 |  9130 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9131 |  |
|        - |  9132 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9133 | `	sxi32 rc;` |
|       48 |  9134 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9135 | `		return SXERR_INVALID;` |
|        - |  9136 | `	}` |
|        - |  9137 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9138 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9139 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9140 | `	/* Save and set the active context */` |
|       48 |  9141 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9142 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9143 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9144 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9145 | `	pVm->nRecursionDepth++;` |
|        - |  9146 | `	/* Execute from the beginning */` |
|       48 |  9147 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9148 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9149 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9150 | `	pVm->nRecursionDepth--;` |
|        - |  9151 | `	/* Restore the previous context */` |
|       48 |  9152 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9153 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9154 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9155 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9156 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9157 | `		if( pResult ){` |
|       24 |  9158 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9159 | `		}` |
|       46 |  9160 | `		return SXRET_OK;` |
|        - |  9161 | `	}` |
|        - |  9162 | `	/* Detach frame */` |
|        3 |  9163 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9164 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9165 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9166 | `	}` |
|        3 |  9167 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9168 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9169 | `		return PH7_ABORT;` |
|        - |  9170 | `	}` |
|        3 |  9171 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9172 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9173 | `		return PH7_EXCEPTION;` |
|        - |  9174 | `	}` |
|        - |  9175 | `	/* Normal completion */` |
|        3 |  9176 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9177 | `	if( pResult ){` |
|        3 |  9178 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9179 | `	}` |
|        3 |  9180 | `	return SXRET_OK;` |
|       25 |  9181 |  |
|        - |  9182 | `/*` |
|        - |  9183 | ` * Resume a suspended fiber context.` |
|        - |  9184 | ` */` |
|       98 |  9185 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9186 |  |
|        - |  9187 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9188 | `	sxi32 rc;` |
|      100 |  9189 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9190 | `		return SXERR_INVALID;` |
|        - |  9191 | `	}` |
|        - |  9192 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9193 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9194 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9195 | `	if( pResumeValue ){` |
|       40 |  9196 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9197 | `	}else{` |
|       62 |  9198 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9199 | `	}` |
|      100 |  9200 | `	pCtx->nTos++;` |
|        - |  9201 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9202 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9203 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9204 | `	/* Save and set the active context */` |
|      100 |  9205 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9206 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9207 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9208 | `	pVm->nRecursionDepth++;` |
|        - |  9209 | `	/* Resume execution from saved PC */` |
|      100 |  9210 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9211 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9212 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9213 | `	pVm->nRecursionDepth--;` |
|        - |  9214 | `	/* Restore the previous context */` |
|      100 |  9215 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9216 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9217 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9218 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9219 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9220 | `		if( pResult ){` |
|       18 |  9221 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9222 | `		}` |
|       64 |  9223 | `		return SXRET_OK;` |
|        - |  9224 | `	}` |
|        - |  9225 | `	/* Detach frame */` |
|       38 |  9226 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9227 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9228 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9229 | `	}` |
|       38 |  9230 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9231 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9232 | `		return PH7_ABORT;` |
|        - |  9233 | `	}` |
|       38 |  9234 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9235 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9236 | `		return PH7_EXCEPTION;` |
|        - |  9237 | `	}` |
|        - |  9238 | `	/* Normal completion */` |
|       38 |  9239 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9240 | `	if( pResult ){` |
|       20 |  9241 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9242 | `	}` |
|       38 |  9243 | `	return SXRET_OK;` |
|       51 |  9244 |  |
|        - |  9245 | `/*` |
|        - |  9246 | ` * Release an execution context and all its resources.` |
|        - |  9247 | ` */` |
|        4 |  9248 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9249 |  |
|        5 |  9250 | `	if( pCtx == 0 ){` |
|      ! 0 |  9251 | `		return;` |
|        - |  9252 | `	}` |
|        5 |  9253 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9254 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9255 | `		return;` |
|        - |  9256 | `	}` |
|        5 |  9257 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9258 | `	/* Release values */` |
|        5 |  9259 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9260 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9261 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9262 | `	if( pCtx->pFrame ){` |
|        - |  9263 | `		VmSlot *aSlot;` |
|        - |  9264 | `		sxu32 n;` |
|        - |  9265 | `		/* Free local variables */` |
|        5 |  9266 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9267 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9268 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9269 | `		}` |
|        - |  9270 | `		/* Remove local references */` |
|        5 |  9271 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9272 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9273 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9274 | `		}` |
|        5 |  9275 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9276 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9277 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9278 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9279 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9280 | `		pCtx->pFrame = 0;` |
|        2 |  9281 | `	}` |
|        - |  9282 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9283 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9284 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9285 | `	if( pCtx->pStack ){` |
|        5 |  9286 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9287 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9288 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9289 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9290 | `				pTos--;` |
|        1 |  9291 | `			}` |
|        2 |  9292 | `		}` |
|        5 |  9293 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9294 | `		pCtx->pStack = 0;` |
|        2 |  9295 | `	}` |
|        - |  9296 | `	/* Free the context itself */` |
|        5 |  9297 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9298 |  |
|        - |  9299 | `/*` |
|        - |  9300 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9301 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9302 | ` */` |
|       90 |  9303 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9304 |  |
|        - |  9305 | `	ph7_class_instance *pThis;` |
|        - |  9306 | `	SyString sAttr;` |
|        - |  9307 | `	ph7_value *pAttr;` |
|       92 |  9308 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9309 | `		return 0;` |
|        - |  9310 | `	}` |
|       92 |  9311 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9312 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9313 | `		return 0;` |
|        - |  9314 | `	}` |
|       92 |  9315 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9316 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9317 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9318 | `		return 0;` |
|        - |  9319 | `	}` |
|       62 |  9320 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9321 |  |
|        - |  9322 | `/*` |
|        - |  9323 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9324 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9325 | ` */` |
|       38 |  9326 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9327 |  |
|       40 |  9328 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9329 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9330 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9331 | `			"Cannot suspend outside of a fiber");` |
|        - |  9332 | `	}` |
|       40 |  9333 | `	if( nArg > 0 ){` |
|       40 |  9334 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9335 | `	}else{` |
|      ! 0 |  9336 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9337 | `	}` |
|       40 |  9338 | `	return PH7_SUSPEND;` |
|       21 |  9339 |  |
|        - |  9340 | `/*` |
|        - |  9341 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9342 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9343 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9344 | ` */` |
|       24 |  9345 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9346 |  |
|        - |  9347 | `	ph7_class_instance *pThis;` |
|        - |  9348 | `	ph7_value *pAttr;` |
|        - |  9349 | `	SyString sAttrName;` |
|       26 |  9350 | `	if( nArg < 2 ){` |
|      ! 0 |  9351 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9352 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9353 | `	}` |
|       26 |  9354 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9355 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9356 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9357 | `	}` |
|       26 |  9358 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9359 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9360 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9361 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9362 | `	}` |
|        - |  9363 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9364 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9365 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9366 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9367 | `	}` |
|        - |  9368 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9369 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9370 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9371 | `	if( pAttr ){` |
|       26 |  9372 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9373 | `	}` |
|       26 |  9374 | `	return PH7_OK;` |
|       14 |  9375 |  |
|        - |  9376 | `/*` |
|        - |  9377 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9378 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9379 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9380 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9381 | ` */` |
|       24 |  9382 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9383 | `	ph7_class_instance **ppThis)` |
|        2 |  9384 |  |
|       26 |  9385 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9386 | `	ph7_value *pCallable;` |
|        - |  9387 | `	SyString sAttrName;` |
|       26 |  9388 | `	*ppThis = 0;` |
|       26 |  9389 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9390 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9391 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9392 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9393 | `		return 0;` |
|        - |  9394 | `	}` |
|       26 |  9395 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9396 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9397 | `		SyString sName;` |
|        - |  9398 | `		SyHashEntry *pEntry;` |
|        - |  9399 | `		ph7_vm_func *pFunc;` |
|       26 |  9400 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9401 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9402 | `		if( pEntry == 0 ){` |
|      ! 0 |  9403 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9404 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9405 | `			return 0;` |
|        - |  9406 | `		}` |
|       26 |  9407 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9408 | `		return pFunc;` |
|      ! 0 |  9409 | `	}else{` |
|        - |  9410 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9411 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9412 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9413 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9414 | `		if( pMethod == 0 ){` |
|      ! 0 |  9415 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9416 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9417 | `			return 0;` |
|        - |  9418 | `		}` |
|      ! 0 |  9419 | `		*ppThis = pClosure;` |
|      ! 0 |  9420 | `		return &pMethod->sFunc;` |
|        - |  9421 | `	}` |
|       14 |  9422 |  |
|        - |  9423 | `/*` |
|        - |  9424 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9425 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9426 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9427 | ` */` |
|       46 |  9428 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9429 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9430 |  |
|       48 |  9431 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9432 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9433 | `	sxu32 nFormal, n;` |
|        - |  9434 | `	VmSlot sSlot;` |
|        - |  9435 | `	sxi32 rc;` |
|        - |  9436 | `	/* Install $this for closure/method callables */` |
|       48 |  9437 | `	if( pClosureThis ){` |
|        - |  9438 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9439 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9440 | `		if( pObj ){` |
|      ! 0 |  9441 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9442 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9443 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9444 | `		}` |
|      ! 0 |  9445 | `	}` |
|        - |  9446 | `	/* Install static variables */` |
|       48 |  9447 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9448 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9449 | `		ph7_value *pVal;` |
|      ! 0 |  9450 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9451 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9452 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9453 | `			if( pVal ){` |
|      ! 0 |  9454 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9455 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9456 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9457 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9458 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9459 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9460 | `				}` |
|      ! 0 |  9461 | `			}` |
|      ! 0 |  9462 | `		}` |
|      ! 0 |  9463 | `	}` |
|        - |  9464 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9465 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9466 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9467 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9468 | `		ph7_value *pObj;` |
|       20 |  9469 | `		if( n < (sxu32)nArg ){` |
|        - |  9470 | `			/* Argument provided — install with type casting */` |
|       20 |  9471 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9472 | `			if( pObj ){` |
|       20 |  9473 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9474 | `				/* Type casting */` |
|       20 |  9475 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9476 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9477 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9478 | `						if( xCast ){` |
|      ! 0 |  9479 | `							xCast(pObj);` |
|      ! 0 |  9480 | `						}` |
|      ! 0 |  9481 | `					}` |
|      ! 0 |  9482 | `				}` |
|       20 |  9483 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9484 | `				sSlot.pUserData = 0;` |
|       20 |  9485 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9486 | `			}` |
|        9 |  9487 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9488 | `			/* Default value */` |
|      ! 0 |  9489 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9490 | `			if( pObj ){` |
|      ! 0 |  9491 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9492 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  9493 | `					return rc;` |
|        - |  9494 | `				}` |
|      ! 0 |  9495 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9496 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9497 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9498 | `						if( xCast ){` |
|      ! 0 |  9499 | `							xCast(pObj);` |
|      ! 0 |  9500 | `						}` |
|      ! 0 |  9501 | `					}` |
|      ! 0 |  9502 | `				}` |
|      ! 0 |  9503 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 |  9504 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9505 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 |  9506 | `			}` |
|      ! 0 |  9507 | `		}` |
|       11 |  9508 | `	}` |
|        - |  9509 | `	/* Install closure environment (captured variables) */` |
|       48 |  9510 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9511 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - |  9512 | `		ph7_value *pValue;` |
|        - |  9513 | `		sxu32 iEnv;` |
|        3 |  9514 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 |  9515 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 |  9516 | `			pEnv = &aEnv[iEnv];` |
|        7 |  9517 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 |  9518 | `				continue;` |
|        - |  9519 | `			}` |
|        5 |  9520 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 |  9521 | `			if( pValue == 0 ){` |
|      ! 0 |  9522 | `				continue;` |
|        - |  9523 | `			}` |
|        5 |  9524 | `			PH7_MemObjRelease(pValue);` |
|        5 |  9525 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 |  9526 | `		}` |
|        1 |  9527 | `	}` |
|       48 |  9528 | `	return SXRET_OK;` |
|       25 |  9529 |  |
|        - |  9530 | `/*` |
|        - |  9531 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - |  9532 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - |  9533 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - |  9534 | ` */` |
|       26 |  9535 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9536 |  |
|       28 |  9537 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9538 | `	ph7_class_instance *pThis;` |
|        - |  9539 | `	ph7_class_instance *pClosureThis;` |
|        - |  9540 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9541 | `	ph7_vm_func *pFunc;` |
|        - |  9542 | `	ph7_value sResult;` |
|        - |  9543 | `	ph7_value *pCtxAttr;` |
|        - |  9544 | `	SyString sAttrName;` |
|        - |  9545 | `	sxi32 rc;` |
|       28 |  9546 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9547 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - |  9548 | `	}` |
|       28 |  9549 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9550 | `	/* Check if already started (has a __ctx) */` |
|       28 |  9551 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 |  9552 | `	if( pExecCtx != 0 ){` |
|        3 |  9553 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9554 | `			"Cannot start a fiber that has already been started");` |
|        - |  9555 | `	}` |
|        - |  9556 | `	/* Resolve callable */` |
|       26 |  9557 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 |  9558 | `	if( pFunc == 0 ){` |
|      ! 0 |  9559 | `		return PH7_EXCEPTION;` |
|        - |  9560 | `	}` |
|        - |  9561 | `	/* Create execution context now that we know the function */` |
|       26 |  9562 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 |  9563 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9564 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9565 | `			"Fiber::start(): out of memory");` |
|        - |  9566 | `	}` |
|        - |  9567 | `	/* Store context in $this->__ctx */` |
|       26 |  9568 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 |  9569 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9570 | `	if( pCtxAttr ){` |
|       26 |  9571 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 |  9572 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 |  9573 | `	}` |
|        - |  9574 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - |  9575 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - |  9576 | `	 * into the fiber's frame, not the caller's. */` |
|       26 |  9577 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 |  9578 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - |  9579 | `	/* Unpack the args array and install into the frame */` |
|        - |  9580 | `	{` |
|       26 |  9581 | `		ph7_value **apValues = 0;` |
|       26 |  9582 | `		int nActual = 0;` |
|       26 |  9583 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 |  9584 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - |  9585 | `			ph7_hashmap_node *pNode;` |
|       26 |  9586 | `			sxu32 nCount = pMap->nEntry;` |
|       26 |  9587 | `			if( nCount > 0 ){` |
|        3 |  9588 | `				sxu32 idx = 0;` |
|        4 |  9589 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 |  9590 | `					nCount * sizeof(ph7_value *));` |
|        3 |  9591 | `				if( apValues ){` |
|        3 |  9592 | `					pNode = pMap->pFirst;` |
|        7 |  9593 | `					while( pNode && idx < nCount ){` |
|        5 |  9594 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 |  9595 | `						idx++;` |
|        5 |  9596 | `						pNode = pNode->pPrev;` |
|        1 |  9597 | `					}` |
|        3 |  9598 | `					nActual = (int)idx;` |
|        1 |  9599 | `				}` |
|        1 |  9600 | `			}` |
|       12 |  9601 | `		}` |
|       26 |  9602 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 |  9603 | `		if( apValues ){` |
|        3 |  9604 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 |  9605 | `		}` |
|        - |  9606 | `	}` |
|        - |  9607 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 |  9608 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 |  9609 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 |  9610 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  9611 | `		return PH7_ABORT;` |
|        - |  9612 | `	}` |
|       26 |  9613 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 |  9614 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 |  9615 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9616 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9617 | `		return PH7_ABORT;` |
|        - |  9618 | `	}` |
|       26 |  9619 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9620 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9621 | `		return PH7_EXCEPTION;` |
|        - |  9622 | `	}` |
|       26 |  9623 | `	ph7_result_value(pCtx, &sResult);` |
|       26 |  9624 | `	PH7_MemObjRelease(&sResult);` |
|       26 |  9625 | `	return PH7_OK;` |
|       15 |  9626 |  |
|        - |  9627 | `/*` |
|        - |  9628 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - |  9629 | ` */` |
|       36 |  9630 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9631 |  |
|       38 |  9632 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9633 | `	ph7_exec_ctx *pExecCtx;` |
|        - |  9634 | `	ph7_value sResult;` |
|        - |  9635 | `	ph7_value *pResumeVal;` |
|        - |  9636 | `	sxi32 rc;` |
|       38 |  9637 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9638 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 |  9639 | `		return PH7_OK;` |
|        - |  9640 | `	}` |
|       38 |  9641 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 |  9642 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9643 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 |  9644 | `		return PH7_OK;` |
|        - |  9645 | `	}` |
|       38 |  9646 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 |  9647 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9648 | `			"Cannot resume a fiber that is not suspended");` |
|        - |  9649 | `	}` |
|       36 |  9650 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 |  9651 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 |  9652 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 |  9653 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9654 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9655 | `		return PH7_ABORT;` |
|        - |  9656 | `	}` |
|       36 |  9657 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9658 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 |  9659 | `		return PH7_EXCEPTION;` |
|        - |  9660 | `	}` |
|       36 |  9661 | `	ph7_result_value(pCtx, &sResult);` |
|       36 |  9662 | `	PH7_MemObjRelease(&sResult);` |
|       36 |  9663 | `	return PH7_OK;` |
|       20 |  9664 |  |
|        - |  9665 | `/*` |
|        - |  9666 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - |  9667 | ` */` |
|        6 |  9668 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9669 |  |
|        8 |  9670 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9671 | `	ph7_exec_ctx *pExecCtx;` |
|        8 |  9672 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9673 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9674 | `		return PH7_OK;` |
|        - |  9675 | `	}` |
|        8 |  9676 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 |  9677 | `	if( pExecCtx == 0 ){` |
|      ! 0 |  9678 | `		ph7_result_null(pCtx);` |
|      ! 0 |  9679 | `		return PH7_OK;` |
|        - |  9680 | `	}` |
|        8 |  9681 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 |  9682 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9683 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9684 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - |  9685 | `		}` |
|      ! 0 |  9686 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9687 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - |  9688 | `	}` |
|        8 |  9689 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 |  9690 | `	return PH7_OK;` |
|        5 |  9691 |  |
|        - |  9692 | `/*` |
|        - |  9693 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - |  9694 | ` */` |
|        6 |  9695 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9696 |  |
|        - |  9697 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9698 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9699 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9700 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 |  9701 | `	return PH7_OK;` |
|        4 |  9702 |  |
|      ! 0 |  9703 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 |  9704 |  |
|        - |  9705 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 |  9706 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 |  9707 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 |  9708 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 |  9709 | `	return PH7_OK;` |
|      ! 0 |  9710 |  |
|        6 |  9711 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9712 |  |
|        - |  9713 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9714 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9715 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9716 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 |  9717 | `	return PH7_OK;` |
|        4 |  9718 |  |
|        6 |  9719 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9720 |  |
|        - |  9721 | `	ph7_exec_ctx *pExecCtx;` |
|        7 |  9722 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 |  9723 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 |  9724 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 |  9725 | `	return PH7_OK;` |
|        4 |  9726 |  |
|        - |  9727 | `/*` |
|        - |  9728 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - |  9729 | ` */` |
|        4 |  9730 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9731 |  |
|        5 |  9732 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9733 | `	ph7_exec_ctx *pExecCtx;` |
|        5 |  9734 | `	if( nArg < 1 ){` |
|      ! 0 |  9735 | `		return PH7_OK;` |
|        - |  9736 | `	}` |
|        5 |  9737 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 |  9738 | `	if( pExecCtx ){` |
|        5 |  9739 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - |  9740 | `		/* Clear the attribute so double-free is prevented */` |
|        5 |  9741 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 |  9742 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - |  9743 | `			SyString sAttrName;` |
|        - |  9744 | `			ph7_value *pAttr;` |
|        5 |  9745 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 |  9746 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 |  9747 | `			if( pAttr ){` |
|        5 |  9748 | `				PH7_MemObjRelease(pAttr);` |
|        2 |  9749 | `			}` |
|        2 |  9750 | `		}` |
|        2 |  9751 | `	}` |
|        5 |  9752 | `	return PH7_OK;` |
|        3 |  9753 |  |
|        - |  9754 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 |  9755 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 |  9756 |  |
|        - |  9757 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9758 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 |  9759 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  9760 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 |  9761 |  |
|      ! 0 |  9762 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 |  9763 |  |
|        - |  9764 | `	ph7_class_instance *pThis;` |
|      ! 0 |  9765 | `	ph7_class_instance *pClosureThis = 0;` |
|        - |  9766 | `	ph7_exec_ctx *pCtx;` |
|        - |  9767 | `	ph7_vm_func *pFunc;` |
|        - |  9768 | `	ph7_value *pCallable;` |
|        - |  9769 | `	ph7_value *pCtxAttr;` |
|        - |  9770 | `	SyString sAttrName;` |
|        - |  9771 | `	/* Must not already be started */` |
|      ! 0 |  9772 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9773 | `	if( pCtx != 0 ){` |
|      ! 0 |  9774 | `		return SXERR_INVALID;` |
|        - |  9775 | `	}` |
|      ! 0 |  9776 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9777 | `		return SXERR_INVALID;` |
|        - |  9778 | `	}` |
|      ! 0 |  9779 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - |  9780 | `	/* Get the callable */` |
|      ! 0 |  9781 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 |  9782 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9783 | `	if( pCallable == 0 ){` |
|      ! 0 |  9784 | `		return SXERR_INVALID;` |
|        - |  9785 | `	}` |
|        - |  9786 | `	/* Resolve callable */` |
|      ! 0 |  9787 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9788 | `		SyString sName;` |
|        - |  9789 | `		SyHashEntry *pEntry;` |
|      ! 0 |  9790 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 |  9791 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 |  9792 | `		if( pEntry == 0 ){` |
|      ! 0 |  9793 | `			return SXERR_NOTFOUND;` |
|        - |  9794 | `		}` |
|      ! 0 |  9795 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 |  9796 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  9797 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9798 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9799 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9800 | `		if( pMethod == 0 ){` |
|      ! 0 |  9801 | `			return SXERR_INVALID;` |
|        - |  9802 | `		}` |
|      ! 0 |  9803 | `		pClosureThis = pClosure;` |
|      ! 0 |  9804 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 |  9805 | `	}else{` |
|      ! 0 |  9806 | `		return SXERR_INVALID;` |
|        - |  9807 | `	}` |
|        - |  9808 | `	/* Create context */` |
|      ! 0 |  9809 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 |  9810 | `	if( pCtx == 0 ){` |
|      ! 0 |  9811 | `		return SXERR_MEM;` |
|        - |  9812 | `	}` |
|        - |  9813 | `	/* Store in __ctx */` |
|      ! 0 |  9814 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 |  9815 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 |  9816 | `	if( pCtxAttr ){` |
|      ! 0 |  9817 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 |  9818 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 |  9819 | `	}` |
|        - |  9820 | `	/* Set up frame with args */` |
|      ! 0 |  9821 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 |  9822 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 |  9823 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 |  9824 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 |  9825 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 |  9826 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 |  9827 |  |
|      ! 0 |  9828 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 |  9829 |  |
|      ! 0 |  9830 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9831 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 |  9832 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 |  9833 |  |
|      ! 0 |  9834 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9835 |  |
|      ! 0 |  9836 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9837 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 |  9838 |  |
|      ! 0 |  9839 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9840 |  |
|      ! 0 |  9841 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9842 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 |  9843 |  |
|      ! 0 |  9844 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 |  9845 |  |
|      ! 0 |  9846 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 |  9847 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 |  9848 | `	return &pCtx->sRetValue;` |
|      ! 0 |  9849 |  |
|        - |  9850 | `/* ======================== Generator Infrastructure ======================== */` |
|        - |  9851 | `/*` |
|        - |  9852 | ` * Allocate a new generator wrapper around an execution context.` |
|        - |  9853 | ` */` |
|       22 |  9854 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 |  9855 |  |
|        - |  9856 | `	ph7_generator *pGen;` |
|       24 |  9857 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 |  9858 | `	if( pGen == 0 ){` |
|      ! 0 |  9859 | `		return 0;` |
|        - |  9860 | `	}` |
|       24 |  9861 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 |  9862 | `	pGen->pCtx = pCtx;` |
|       24 |  9863 | `	pGen->iImplicitKey = 0;` |
|       24 |  9864 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 |  9865 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - |  9866 | `	/* Link the generator back to the exec context */` |
|       24 |  9867 | `	pCtx->pPrivate = pGen;` |
|       24 |  9868 | `	return pGen;` |
|       13 |  9869 |  |
|        - |  9870 | `/*` |
|        - |  9871 | ` * Release a generator and its execution context.` |
|        - |  9872 | ` */` |
|      ! 0 |  9873 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 |  9874 |  |
|      ! 0 |  9875 | `	if( pGen == 0 ){` |
|      ! 0 |  9876 | `		return;` |
|        - |  9877 | `	}` |
|      ! 0 |  9878 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  9879 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  9880 | `	if( pGen->pCtx ){` |
|      ! 0 |  9881 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 |  9882 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 |  9883 | `		pGen->pCtx = 0;` |
|      ! 0 |  9884 | `	}` |
|      ! 0 |  9885 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 |  9886 |  |
|        - |  9887 | `/*` |
|        - |  9888 | ` * Extract ph7_generator from a Generator class instance.` |
|        - |  9889 | ` */` |
|      236 |  9890 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 |  9891 |  |
|        - |  9892 | `	ph7_class_instance *pThis;` |
|        - |  9893 | `	SyString sAttr;` |
|        - |  9894 | `	ph7_value *pAttr;` |
|      238 |  9895 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9896 | `		return 0;` |
|        - |  9897 | `	}` |
|      238 |  9898 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 |  9899 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 |  9900 | `		return 0;` |
|        - |  9901 | `	}` |
|      238 |  9902 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 |  9903 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 |  9904 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 |  9905 | `		return 0;` |
|        - |  9906 | `	}` |
|      238 |  9907 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 |  9908 |  |
|        - |  9909 | `/*` |
|        - |  9910 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - |  9911 | ` */` |
|       22 |  9912 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9913 |  |
|        - |  9914 | `	ph7_generator *pGen;` |
|        - |  9915 | `	sxi32 rc;` |
|       24 |  9916 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 |  9917 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 |  9918 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 |  9919 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 |  9920 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 |  9921 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 |  9922 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 |  9923 | `	}` |
|       24 |  9924 | `	return PH7_OK;` |
|       13 |  9925 |  |
|        - |  9926 | `/*` |
|        - |  9927 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - |  9928 | ` */` |
|       68 |  9929 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9930 |  |
|        - |  9931 | `	ph7_generator *pGen;` |
|       70 |  9932 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 |  9933 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9934 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 |  9935 | `	return PH7_OK;` |
|       36 |  9936 |  |
|        - |  9937 | `/*` |
|        - |  9938 | ` * Generator::current() — return the last yielded value.` |
|        - |  9939 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9940 | ` */` |
|       68 |  9941 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9942 |  |
|        - |  9943 | `	ph7_generator *pGen;` |
|        - |  9944 | `	sxi32 rc;` |
|       70 |  9945 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9946 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 |  9947 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 |  9948 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9949 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9950 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9951 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9952 | `	}` |
|       70 |  9953 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 |  9954 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 |  9955 | `	}else{` |
|      ! 0 |  9956 | `		ph7_result_null(pCtx);` |
|        - |  9957 | `	}` |
|       70 |  9958 | `	return PH7_OK;` |
|       36 |  9959 |  |
|        - |  9960 | `/*` |
|        - |  9961 | ` * Generator::key() — return the last yielded key.` |
|        - |  9962 | ` * Auto-starts the generator on first access (like PHP).` |
|        - |  9963 | ` */` |
|       12 |  9964 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 |  9965 |  |
|        - |  9966 | `	ph7_generator *pGen;` |
|        - |  9967 | `	sxi32 rc;` |
|       13 |  9968 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9969 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 |  9970 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 |  9971 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9972 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 |  9973 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 |  9974 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 |  9975 | `	}` |
|       13 |  9976 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 |  9977 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 |  9978 | `	}else{` |
|      ! 0 |  9979 | `		ph7_result_null(pCtx);` |
|        - |  9980 | `	}` |
|       13 |  9981 | `	return PH7_OK;` |
|        7 |  9982 |  |
|        - |  9983 | `/*` |
|        - |  9984 | ` * Generator::next() — advance to the next yield point.` |
|        - |  9985 | ` */` |
|       60 |  9986 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9987 |  |
|        - |  9988 | `	ph7_generator *pGen;` |
|        - |  9989 | `	sxi32 rc;` |
|       62 |  9990 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 |  9991 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 |  9992 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 |  9993 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9994 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 |  9995 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 |  9996 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 |  9997 | `	}else{` |
|      ! 0 |  9998 | `		return PH7_OK;` |
|        - |  9999 | `	}` |
|       62 | 10000 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10001 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10002 | `	return PH7_OK;` |
|       32 | 10003 |  |
|        - | 10004 | `/*` |
|        - | 10005 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10006 | ` */` |
|        4 | 10007 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10008 |  |
|        - | 10009 | `	ph7_generator *pGen;` |
|        - | 10010 | `	ph7_value *pSendVal;` |
|        - | 10011 | `	sxi32 rc;` |
|        5 | 10012 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10013 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10014 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10015 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10016 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10017 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10018 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10019 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10020 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10021 | `	}else{` |
|      ! 0 | 10022 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10023 | `		return PH7_OK;` |
|        - | 10024 | `	}` |
|        5 | 10025 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10026 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10027 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10028 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10029 | `	}else{` |
|        3 | 10030 | `		ph7_result_null(pCtx);` |
|        - | 10031 | `	}` |
|        5 | 10032 | `	return PH7_OK;` |
|        3 | 10033 |  |
|        - | 10034 | `/*` |
|        - | 10035 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10036 | ` *` |
|        - | 10037 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10038 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10039 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10040 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10041 | ` * the exception to the caller.` |
|        - | 10042 | ` */` |
|      ! 0 | 10043 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10044 |  |
|        - | 10045 | `	ph7_generator *pGen;` |
|        - | 10046 | `	const char *zMsg;` |
|        - | 10047 | `	int nLen;` |
|      ! 0 | 10048 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10049 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10050 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10051 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10052 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10053 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10054 | `			"Cannot throw into a closed generator");` |
|        - | 10055 | `	}` |
|        - | 10056 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10057 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10058 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10059 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10060 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10061 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10062 | `	nLen = 0;` |
|      ! 0 | 10063 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10064 | `		/* Try to get the exception's message */` |
|        - | 10065 | `		SyString sAttr;` |
|        - | 10066 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10067 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10068 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10069 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10070 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10071 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10072 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10073 | `		}` |
|      ! 0 | 10074 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10075 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10076 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10077 | `	}` |
|      ! 0 | 10078 | `	(void)nLen;` |
|      ! 0 | 10079 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10080 |  |
|        - | 10081 | `/*` |
|        - | 10082 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10083 | ` */` |
|        2 | 10084 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10085 |  |
|        - | 10086 | `	ph7_generator *pGen;` |
|        3 | 10087 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10088 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10089 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10090 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10091 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10092 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10093 | `	}` |
|        3 | 10094 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10095 | `	return PH7_OK;` |
|        2 | 10096 |  |
|        - | 10097 | `/*` |
|        - | 10098 | ` * Generator::__destruct() — clean up.` |
|        - | 10099 | ` */` |
|      ! 0 | 10100 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10101 |  |
|        - | 10102 | `	ph7_generator *pGen;` |
|      ! 0 | 10103 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10104 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10105 | `	if( pGen ){` |
|      ! 0 | 10106 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10107 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10108 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10109 | `			SyString sAttrName;` |
|        - | 10110 | `			ph7_value *pAttr;` |
|      ! 0 | 10111 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10112 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10113 | `			if( pAttr ){` |
|      ! 0 | 10114 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10115 | `			}` |
|      ! 0 | 10116 | `		}` |
|      ! 0 | 10117 | `	}` |
|      ! 0 | 10118 | `	return PH7_OK;` |
|      ! 0 | 10119 |  |
|        - | 10120 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10121 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10122 | `/*` |
|        - | 10123 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10124 | ` * the desired message.` |
|        - | 10125 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10126 | ` * in 'api.c' for additional information.` |
|        - | 10127 | ` */` |
|      370 | 10128 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10129 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10130 | `	SyString *pString /* Message to output */` |
|        - | 10131 | `	)` |
|        2 | 10132 |  |
|      372 | 10133 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10134 | `	sxi32 rc = SXRET_OK;` |
|        - | 10135 | `	/* Call the output consumer */` |
|      372 | 10136 | `	if( pString->nByte > 0 ){` |
|      372 | 10137 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10138 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10139 | `	}` |
|      372 | 10140 | `	return rc;` |
|        2 | 10141 |  |
|        - | 10142 | `/*` |
|        - | 10143 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10144 | ` * callback to consume the formatted message.` |
|        - | 10145 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10146 | ` * in 'api.c' for additional information.` |
|        - | 10147 | ` */` |
|        2 | 10148 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10149 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10150 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10151 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10152 | `	)` |
|        1 | 10153 |  |
|        3 | 10154 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10155 | `	sxi32 rc = SXRET_OK;` |
|        - | 10156 | `	SyBlob sWorker;` |
|        - | 10157 | `	/* Format the message and call the output consumer */` |
|        3 | 10158 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10159 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10160 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10161 | `		/* Consume the formatted message */` |
|        3 | 10162 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10163 | `	}` |
|        3 | 10164 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10165 | `	/* Release the working buffer */` |
|        3 | 10166 | `	SyBlobRelease(&sWorker);` |
|        3 | 10167 | `	return rc;` |
|        1 | 10168 |  |
|        - | 10169 | `/*` |
|        - | 10170 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10171 | ` * This function never fail and always return a pointer` |
|        - | 10172 | ` * to a null terminated string.` |
|        - | 10173 | ` */` |
|       12 | 10174 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10175 |  |
|       13 | 10176 | `	const char *zOp = "Unknown     ";` |
|       13 | 10177 | `	switch(nOp){` |
|        3 | 10178 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10179 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10180 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10181 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10182 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10183 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10184 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10185 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10186 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10187 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10188 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10189 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10190 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10191 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10192 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10193 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10194 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10195 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10196 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10197 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10198 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10199 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10200 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10201 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10202 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10203 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10204 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10205 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10206 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10207 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10208 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10209 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10210 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10211 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10212 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10213 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10214 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10215 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10216 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10217 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10218 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10219 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10220 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10221 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10222 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10223 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10224 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10225 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10226 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10227 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10228 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10229 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10230 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10231 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10232 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10233 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10234 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10235 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10236 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10237 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10238 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10239 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10240 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10241 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10242 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10243 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10244 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10245 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10246 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10247 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10248 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10249 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10250 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10251 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10252 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10253 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10254 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10255 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10256 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10257 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10258 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10259 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10260 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10261 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10262 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10263 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10264 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10265 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10266 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10267 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10268 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10269 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10270 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10271 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10272 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10273 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10274 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10275 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10276 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10277 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10278 | `	default:` |
|      ! 0 | 10279 | `		break;` |
|        - | 10280 | `	}` |
|       13 | 10281 | `	return zOp;` |
|        1 | 10282 |  |
|        - | 10283 | `/*` |
|        - | 10284 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10285 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10286 | ` * is responsible of consuming the generated dump.` |
|        - | 10287 | ` */` |
|        2 | 10288 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10289 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10290 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10291 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10292 | `	)` |
|        1 | 10293 |  |
|        - | 10294 | `	sxi32 rc;` |
|        3 | 10295 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10296 | `	return rc;` |
|        1 | 10297 |  |
|        - | 10298 | `/*` |
|        - | 10299 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10300 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10301 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10302 | ` * in 'compile.c' for additional information.` |
|        - | 10303 | ` */` |
|       14 | 10304 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10305 |  |
|       15 | 10306 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10307 | `	/* Evaluate and expand constant value */` |
|       15 | 10308 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10309 |  |
|        - | 10310 | `/*` |
|        - | 10311 | ` * Section:` |
|        - | 10312 | ` *  Function handling functions.` |
|        - | 10313 | ` * Status:` |
|        - | 10314 | ` *    Stable.` |
|        - | 10315 | ` */` |
|        - | 10316 | `/*` |
|        - | 10317 | ` * int func_num_args(void)` |
|        - | 10318 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10319 | ` * Parameters` |
|        - | 10320 | ` *   None.` |
|        - | 10321 | ` * Return` |
|        - | 10322 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10323 | ` *  or -1 if called from the globe scope.` |
|        - | 10324 | ` */` |
|      960 | 10325 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10326 |  |
|        - | 10327 | `	VmFrame *pFrame;` |
|        - | 10328 | `	ph7_vm *pVm;` |
|        - | 10329 | `	/* Point to the target VM */` |
|      962 | 10330 | `	pVm = pCtx->pVm;` |
|        - | 10331 | `	/* Current frame */` |
|      962 | 10332 | `	pFrame = pVm->pFrame;` |
|      962 | 10333 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      962 | 10334 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10335 | `		SXUNUSED(nArg);` |
|      ! 0 | 10336 | `		SXUNUSED(apArg);` |
|        - | 10337 | `		/* Global frame,return -1 */` |
|      ! 0 | 10338 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10339 | `		return SXRET_OK;` |
|        - | 10340 | `	}` |
|        - | 10341 | `	/* Total number of arguments passed to the enclosing function */` |
|      962 | 10342 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      962 | 10343 | `	ph7_result_int(pCtx,nArg);` |
|      962 | 10344 | `	return SXRET_OK;` |
|      482 | 10345 |  |
|        - | 10346 | `/*` |
|        - | 10347 | ` * value func_get_arg(int $arg_num)` |
|        - | 10348 | ` *   Return an item from the argument list.` |
|        - | 10349 | ` * Parameters` |
|        - | 10350 | ` *  Argument number(index start from zero).` |
|        - | 10351 | ` * Return` |
|        - | 10352 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10353 | ` */` |
|       22 | 10354 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10355 |  |
|       24 | 10356 | `	ph7_value *pObj = 0;` |
|       24 | 10357 | `	VmSlot *pSlot = 0;` |
|        - | 10358 | `	VmFrame *pFrame;` |
|        - | 10359 | `	ph7_vm *pVm;` |
|        - | 10360 | `	/* Point to the target VM */` |
|       24 | 10361 | `	pVm = pCtx->pVm;` |
|        - | 10362 | `	/* Current frame */` |
|       24 | 10363 | `	pFrame = pVm->pFrame;` |
|       24 | 10364 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10365 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10366 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10367 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10368 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10369 | `		return SXRET_OK;` |
|        - | 10370 | `	}` |
|        - | 10371 | `	/* Extract the desired index */` |
|       21 | 10372 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10373 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10374 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10375 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10376 | `		return SXRET_OK;` |
|        - | 10377 | `	}` |
|        - | 10378 | `	/* Extract the desired argument */` |
|       21 | 10379 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10380 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10381 | `			/* Return the desired argument */` |
|       21 | 10382 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10383 | `		}else{` |
|        - | 10384 | `			/* No such argument,return false */` |
|      ! 0 | 10385 | `			ph7_result_bool(pCtx,0);` |
|        - | 10386 | `		}` |
|       11 | 10387 | `	}else{` |
|        - | 10388 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10389 | `		ph7_result_bool(pCtx,0);` |
|        - | 10390 | `	}` |
|       21 | 10391 | `	return SXRET_OK;` |
|       13 | 10392 |  |
|        - | 10393 | `/*` |
|        - | 10394 | ` * array func_get_args_byref(void)` |
|        - | 10395 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10396 | ` * Parameters` |
|        - | 10397 | ` *  None.` |
|        - | 10398 | ` * Return` |
|        - | 10399 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10400 | ` *  member of the current user-defined function's argument list.` |
|        - | 10401 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10402 | ` * NOTE:` |
|        - | 10403 | ` *  Arguments are returned to the array by reference.` |
|        - | 10404 | ` */` |
|        2 | 10405 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10406 |  |
|        - | 10407 | `	ph7_value *pArray;` |
|        - | 10408 | `	VmFrame *pFrame;` |
|        - | 10409 | `	VmSlot *aSlot;` |
|        - | 10410 | `	sxu32 n;` |
|        - | 10411 | `	/* Point to the current frame */` |
|        3 | 10412 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10413 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10414 | `	if( pFrame->pParent == 0 ){` |
|        - | 10415 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10416 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10417 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10418 | `		return SXRET_OK;` |
|        - | 10419 | `	}` |
|        - | 10420 | `	/* Create a new array */` |
|        3 | 10421 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10422 | `	if( pArray == 0 ){` |
|      ! 0 | 10423 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10424 | `		SXUNUSED(apArg);` |
|      ! 0 | 10425 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10426 | `		return SXRET_OK;` |
|        - | 10427 | `	}` |
|        - | 10428 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10429 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10430 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10431 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10432 | `	}` |
|        - | 10433 | `	/* Return the freshly created array */` |
|        3 | 10434 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10435 | `	return SXRET_OK;` |
|        2 | 10436 |  |
|        - | 10437 | `/*` |
|        - | 10438 | ` * array func_get_args(void)` |
|        - | 10439 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10440 | ` * Parameters` |
|        - | 10441 | ` *  None.` |
|        - | 10442 | ` * Return` |
|        - | 10443 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10444 | ` *  member of the current user-defined function's argument list.` |
|        - | 10445 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10446 | ` */` |
|       88 | 10447 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10448 |  |
|       90 | 10449 | `	ph7_value *pObj = 0;` |
|        - | 10450 | `	ph7_value *pArray;` |
|        - | 10451 | `	VmFrame *pFrame;` |
|        - | 10452 | `	VmSlot *aSlot;` |
|        - | 10453 | `	sxu32 n;` |
|        - | 10454 | `	/* Point to the current frame */` |
|       90 | 10455 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10456 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10457 | `	if( pFrame->pParent == 0 ){` |
|        - | 10458 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10459 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10460 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10461 | `		return SXRET_OK;` |
|        - | 10462 | `	}` |
|        - | 10463 | `	/* Create a new array */` |
|       90 | 10464 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10465 | `	if( pArray == 0 ){` |
|      ! 0 | 10466 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10467 | `		SXUNUSED(apArg);` |
|      ! 0 | 10468 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10469 | `		return SXRET_OK;` |
|        - | 10470 | `	}` |
|        - | 10471 | `	/* Start filling the array with the given arguments */` |
|       90 | 10472 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10473 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10474 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10475 | `		if( pObj ){` |
|      134 | 10476 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10477 | `		}` |
|       68 | 10478 | `	}` |
|        - | 10479 | `	/* Return the freshly created array */` |
|       90 | 10480 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10481 | `	return SXRET_OK;` |
|       46 | 10482 |  |
|        - | 10483 | `/*` |
|        - | 10484 | ` * bool function_exists(string $name)` |
|        - | 10485 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10486 | ` * Parameters` |
|        - | 10487 | ` *  The name of the desired function.` |
|        - | 10488 | ` * Return` |
|        - | 10489 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10490 | ` */` |
|     1712 | 10491 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10492 |  |
|        - | 10493 | `	const char *zName;` |
|        - | 10494 | `	ph7_vm *pVm;` |
|        - | 10495 | `	int nLen;` |
|        - | 10496 | `	int res;` |
|     1714 | 10497 | `	if( nArg < 1 ){` |
|        - | 10498 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 10499 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10500 | `		return SXRET_OK;` |
|        - | 10501 | `	}` |
|        - | 10502 | `	/* Point to the target VM */` |
|     1714 | 10503 | `	pVm = pCtx->pVm;` |
|        - | 10504 | `	/* Extract the function name */` |
|     1714 | 10505 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 10506 | `	/* Assume the function is not defined */` |
|     1714 | 10507 | `	res = 0;` |
|        - | 10508 | `	/* Perform the lookup */` |
|     2568 | 10509 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1708 | 10510 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10511 | `			/* Function is defined */` |
|      238 | 10512 | `			res = 1;` |
|      118 | 10513 | `	}` |
|     1714 | 10514 | `	ph7_result_bool(pCtx,res);` |
|     1714 | 10515 | `	return SXRET_OK;` |
|      858 | 10516 |  |
|        - | 10517 | `/*` |
|        - | 10518 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10519 | ` * [i.e: Whether it is callable or not].` |
|        - | 10520 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 10521 | ` */` |
|    21604 | 10522 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 10523 |  |
|    21606 | 10524 | `	int res = 0;` |
|    21606 | 10525 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 10526 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 10527 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 10528 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 10529 | `		 * standard PHP behavior. */` |
|       20 | 10530 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 10531 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 10532 | `			res = 1;` |
|       10 | 10533 | `		}` |
|        9 | 10534 | `		(void)CallInvoke;` |
|    21597 | 10535 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 10536 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 10537 | `		if( pMap->nEntry == 2 ){` |
|        - | 10538 | `			ph7_class *pClass;` |
|        - | 10539 | `			ph7_value *pV;` |
|        - | 10540 | `			/* Extract the target class */` |
|       12 | 10541 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 10542 | `			if( pV ){` |
|       12 | 10543 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 10544 | `				if( pClass ){` |
|        - | 10545 | `					ph7_class_method *pMethod;` |
|        - | 10546 | `					/* Extract the target method */` |
|       10 | 10547 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 10548 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 10549 | `						/* Perform the lookup */` |
|       10 | 10550 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 10551 | `						if( pMethod ){` |
|        - | 10552 | `							/* Method is callable */` |
|        5 | 10553 | `							res = 1;` |
|        2 | 10554 | `						}` |
|        4 | 10555 | `					}` |
|        4 | 10556 | `				}` |
|        5 | 10557 | `			}` |
|        7 | 10558 | `		}` |
|    21575 | 10559 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 10560 | `		const char *zName;` |
|        - | 10561 | `		int nLen;` |
|        - | 10562 | `		/* Extract the name */` |
|     5654 | 10563 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 10564 | `		/* Perform the lookup */` |
|     5669 | 10565 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 10566 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 10567 | `				/* Function is callable */` |
|     5636 | 10568 | `				res = 1;` |
|     2817 | 10569 | `		}` |
|     2826 | 10570 | `	}` |
|    21606 | 10571 | `	return res;` |
|        2 | 10572 |  |
|        - | 10573 | `/*` |
|        - | 10574 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 10575 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 10576 | ` * Parameters` |
|        - | 10577 | ` * $name` |
|        - | 10578 | ` *    The callback function to check` |
|        - | 10579 | ` * $syntax_only` |
|        - | 10580 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 10581 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 10582 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 10583 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 10584 | ` *    a string.` |
|        - | 10585 | ` * Return` |
|        - | 10586 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 10587 | ` */` |
|       20 | 10588 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10589 |  |
|        - | 10590 | `	ph7_vm *pVm;` |
|        - | 10591 | `	int res;` |
|       21 | 10592 | `	if( nArg < 1 ){` |
|        - | 10593 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 10594 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10595 | `		return SXRET_OK;` |
|        - | 10596 | `	}` |
|        - | 10597 | `	/* Point to the target VM */` |
|       21 | 10598 | `	pVm = pCtx->pVm;` |
|        - | 10599 | `	/* Perform the requested operation */` |
|       21 | 10600 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 10601 | `	ph7_result_bool(pCtx,res);` |
|       21 | 10602 | `	return SXRET_OK;` |
|       11 | 10603 |  |
|        - | 10604 | `/*` |
|        - | 10605 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 10606 | ` * defined below.` |
|        - | 10607 | ` */` |
|     1228 | 10608 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 10609 |  |
|     1229 | 10610 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 10611 | `	ph7_value sName;` |
|        - | 10612 | `	sxi32 rc;` |
|        - | 10613 | `	/* Prepare the function name for insertion */` |
|     1229 | 10614 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1229 | 10615 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 10616 | `	/* Perform the insertion */` |
|     1229 | 10617 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1229 | 10618 | `	PH7_MemObjRelease(&sName);` |
|     1229 | 10619 | `	return rc;` |
|        1 | 10620 |  |
|        - | 10621 | `/*` |
|        - | 10622 | ` * array get_defined_functions(void)` |
|        - | 10623 | ` *  Returns an array of all defined functions.` |
|        - | 10624 | ` * Parameter` |
|        - | 10625 | ` *  None.` |
|        - | 10626 | ` * Return` |
|        - | 10627 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 10628 | ` *  both built-in (internal) and user-defined.` |
|        - | 10629 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 10630 | ` *  defined ones using $arr["user"].` |
|        - | 10631 | ` * Note:` |
|        - | 10632 | ` *  NULL is returned on failure.` |
|        - | 10633 | ` */` |
|        2 | 10634 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10635 |  |
|        - | 10636 | `	ph7_value *pArray,*pEntry;` |
|        - | 10637 | `	/* NOTE:` |
|        - | 10638 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 10639 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 10640 | `	 */` |
|        3 | 10641 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10642 | ` 	if( pArray == 0 ){` |
|      ! 0 | 10643 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10644 | `		SXUNUSED(apArg);` |
|        - | 10645 | `		/* Return NULL */` |
|      ! 0 | 10646 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10647 | `		return SXRET_OK;` |
|        - | 10648 | `	}` |
|        3 | 10649 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10650 | `	if( pEntry == 0 ){` |
|        - | 10651 | `		/* Return NULL */` |
|      ! 0 | 10652 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10653 | `		return SXRET_OK;` |
|        - | 10654 | `	}` |
|        - | 10655 | `	/* Fill with the appropriate information */` |
|        3 | 10656 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 10657 | `	/* Create the 'internal' index */` |
|        3 | 10658 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 10659 | `	/* Create the user-func array */` |
|        3 | 10660 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 10661 | `	if( pEntry == 0 ){` |
|        - | 10662 | `		/* Return NULL */` |
|      ! 0 | 10663 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10664 | `		return SXRET_OK;` |
|        - | 10665 | `	}` |
|        - | 10666 | `	/* Fill with the appropriate information */` |
|        3 | 10667 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 10668 | `	/* Create the 'user' index */` |
|        3 | 10669 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 10670 | `	/* Return the multi-dimensional array */` |
|        3 | 10671 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10672 | `	return SXRET_OK;` |
|        2 | 10673 |  |
|        - | 10674 | `/*` |
|        - | 10675 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 10676 | ` *  Register a function for execution on shutdown.` |
|        - | 10677 | ` * Note` |
|        - | 10678 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 10679 | ` *  be called in the same order as they were registered.` |
|        - | 10680 | ` * Parameters` |
|        - | 10681 | ` *  $callback` |
|        - | 10682 | ` *   The shutdown callback to register.` |
|        - | 10683 | ` * $param` |
|        - | 10684 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 10685 | ` * Return` |
|        - | 10686 | ` *  Nothing.` |
|        - | 10687 | ` */` |
|        2 | 10688 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10689 |  |
|        - | 10690 | `	VmShutdownCB sEntry;` |
|        - | 10691 | `	int i,j;` |
|        3 | 10692 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10693 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 10694 | `		return PH7_OK;` |
|        - | 10695 | `	}` |
|        - | 10696 | `	/* Zero the Entry */` |
|        3 | 10697 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 10698 | `	/* Initialize fields */` |
|        3 | 10699 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 10700 | `	/* Save the callback name for later invocation name */` |
|        3 | 10701 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 10702 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 10703 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 10704 | `	}` |
|        - | 10705 | `	/* Copy arguments */` |
|        3 | 10706 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 10707 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 10708 | `			/* Limit reached */` |
|      ! 0 | 10709 | `			break;` |
|        - | 10710 | `		}` |
|      ! 0 | 10711 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 10712 | `	}` |
|        3 | 10713 | `	sEntry.nArg = j;` |
|        - | 10714 | `	/* Install the callback */` |
|        3 | 10715 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 10716 | `	return PH7_OK;` |
|        2 | 10717 |  |
|        - | 10718 | `/*` |
|        - | 10719 | ` * Section:` |
|        - | 10720 | ` *  Class handling functions.` |
|        - | 10721 | ` * Status:` |
|        - | 10722 | ` *    Stable.` |
|        - | 10723 | ` */` |
|        - | 10724 | `/*` |
|        - | 10725 | ` * Extract the top active class. NULL is returned` |
|        - | 10726 | ` * if the class stack is empty.` |
|        - | 10727 | ` */` |
|      890 | 10728 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 10729 |  |
|      892 | 10730 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 10731 | `	ph7_class **apClass;` |
|      892 | 10732 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 10733 | `		/* Empty stack,return NULL */` |
|       15 | 10734 | `		return 0;` |
|        - | 10735 | `	}` |
|        - | 10736 | `	/* Peek the last entry */` |
|      878 | 10737 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      878 | 10738 | `	return apClass[pSet->nUsed - 1];` |
|      447 | 10739 |  |
|        - | 10740 | `/*` |
|        - | 10741 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 10742 | ` *   Get the class that declared the currently executing method.` |
|        - | 10743 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 10744 | ` *` |
|        - | 10745 | ` * Parameters` |
|        - | 10746 | ` *   pVm: Target VM` |
|        - | 10747 | ` *` |
|        - | 10748 | ` * Return` |
|        - | 10749 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 10750 | ` *   - Not executing within a class method` |
|        - | 10751 | ` *` |
|        - | 10752 | ` * Note` |
|        - | 10753 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 10754 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 10755 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 10756 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 10757 | ` *   declaring class.` |
|        - | 10758 | ` */` |
|       98 | 10759 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 10760 |  |
|      100 | 10761 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 10762 | `	ph7_vm_func *pVmFunc;` |
|        - | 10763 |  |
|        - | 10764 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 10765 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 10766 |  |
|        - | 10767 | `	/* Check if we're in a method context */` |
|      100 | 10768 | `	if( pFrame->pParent ){` |
|       96 | 10769 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 10770 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 10771 | `			/* Return the declaring class */` |
|       96 | 10772 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 10773 | `		}` |
|      ! 0 | 10774 | `	}` |
|        - | 10775 |  |
|        5 | 10776 | `	return 0;` |
|       51 | 10777 |  |
|        - | 10778 |  |
|        - | 10779 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 10780 | `/*` |
|        - | 10781 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 10782 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 10783 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 10784 | ` * return value indicates failure.` |
|        - | 10785 | ` */` |
|        - | 10786 | `/*` |
|        - | 10787 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 10788 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 10789 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 10790 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 10791 | ` */` |
|     2148 | 10792 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 10793 | `	ph7_vm *pVm,` |
|        - | 10794 | `	ph7_class_instance *pThis,` |
|        - | 10795 | `	ph7_class_method *pMethod,` |
|        - | 10796 | `	ph7_value *pResult,` |
|        - | 10797 | `	int nArg,` |
|        - | 10798 | `	ph7_value **apArg,` |
|        - | 10799 | `	VmCallArgMap *pMap` |
|        - | 10800 | `	)` |
|        2 | 10801 |  |
|        - | 10802 | `	ph7_value *aStack;` |
|        - | 10803 | `	VmInstr aInstr[2];` |
|        - | 10804 | `	int iCursor;` |
|        - | 10805 | `	int i;` |
|     2150 | 10806 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2150 | 10807 | `	if( aStack == 0 ){` |
|      ! 0 | 10808 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 10809 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 10810 | `		return SXERR_MEM;` |
|        - | 10811 | `	}` |
|     3378 | 10812 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1230 | 10813 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1230 | 10814 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      616 | 10815 | `	}` |
|     2150 | 10816 | `	iCursor = nArg + 1;` |
|     2150 | 10817 | `	if( pThis ){` |
|     2144 | 10818 | `		pThis->iRef++;` |
|     2144 | 10819 | `		aStack[i].x.pOther = pThis;` |
|     2144 | 10820 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1071 | 10821 | `	}` |
|     2150 | 10822 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2150 | 10823 | `	i++;` |
|     2150 | 10824 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2150 | 10825 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2150 | 10826 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2150 | 10827 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2150 | 10828 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2150 | 10829 | `	aInstr[0].iP1 = nArg;` |
|     2150 | 10830 | `	aInstr[0].iP2 = 0;` |
|     2150 | 10831 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2150 | 10832 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2150 | 10833 | `	aInstr[1].iP1 = 1;` |
|     2150 | 10834 | `	aInstr[1].iP2 = 0;` |
|     2150 | 10835 | `	aInstr[1].p3  = 0;` |
|     2150 | 10836 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2150 | 10837 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     2150 | 10838 | `	return PH7_OK;` |
|     1076 | 10839 |  |
|     1686 | 10840 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 10841 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 10842 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 10843 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 10844 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 10845 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 10846 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 10847 | `	)` |
|        2 | 10848 |  |
|     1688 | 10849 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 10850 |  |
|        - | 10851 | `/*` |
|        - | 10852 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 10853 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 10854 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 10855 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 10856 | ` *` |
|        - | 10857 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 10858 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 10859 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 10860 | ` *` |
|        - | 10861 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 10862 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 10863 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 10864 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 10865 | ` *` |
|        - | 10866 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 10867 | ` */` |
|      166 | 10868 | `static sxi32 VmCallObjectInvoke(` |
|        - | 10869 | `	ph7_vm *pVm,` |
|        - | 10870 | `	ph7_class_instance *pThis,` |
|        - | 10871 | `	int nArg,` |
|        - | 10872 | `	ph7_value **apArg,` |
|        - | 10873 | `	ph7_value *pResult,` |
|        - | 10874 | `	VmCallArgMap *pMap` |
|        - | 10875 | `	)` |
|        2 | 10876 |  |
|        - | 10877 | `	ph7_class_method *pMethod;` |
|      168 | 10878 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      168 | 10879 | `	if( pMethod == 0 ){` |
|       13 | 10880 | `		if( pResult ){` |
|       13 | 10881 | `			PH7_MemObjRelease(pResult);` |
|        6 | 10882 | `		}` |
|       13 | 10883 | `		return SXERR_INVALID;` |
|        - | 10884 | `	}` |
|      156 | 10885 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       85 | 10886 |  |
|        - | 10887 | `/*` |
|        - | 10888 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 10889 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 10890 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 10891 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 10892 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 10893 | ` * lookup or 'goto Exception').` |
|        - | 10894 | ` *` |
|        - | 10895 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 10896 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 10897 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 10898 | ` * reported.` |
|        - | 10899 | ` */` |
|       12 | 10900 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 10901 |  |
|        - | 10902 | `	ph7_class *pErrorClass;` |
|       13 | 10903 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 10904 | `	ph7_class_method *pCons;` |
|        - | 10905 | `	VmFrame *pThrowFrame;` |
|        - | 10906 | `	char zMsg[256];` |
|        - | 10907 | `	int nMsg;` |
|        - | 10908 | `	sxi32 rc;` |
|       25 | 10909 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 10910 | `		"Object of type %.*s is not callable",` |
|       12 | 10911 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 10912 | `		pThis->pClass->sName.zString);` |
|       13 | 10913 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 10914 | `	if( pErrorClass ){` |
|       13 | 10915 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 10916 | `	}` |
|       13 | 10917 | `	if( pErrInst == 0 ){` |
|        - | 10918 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 10919 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 10920 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 10921 | `		 * visible to the user. */` |
|      ! 0 | 10922 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 10923 | `		return SXERR_ABORT;` |
|        - | 10924 | `	}` |
|       13 | 10925 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 10926 | `	if( pCons ){` |
|        - | 10927 | `		ph7_value sArg;` |
|        - | 10928 | `		ph7_value *apMsg[1];` |
|        - | 10929 | `		SyString sMsgStr;` |
|       13 | 10930 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 10931 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 10932 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 10933 | `		apMsg[0] = &sArg;` |
|       13 | 10934 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 10935 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 10936 | `	}` |
|        - | 10937 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 10938 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 10939 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 10940 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 10941 | `	if( pThrowFrame ){` |
|       13 | 10942 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 10943 | `	}` |
|       13 | 10944 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 10945 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 10946 | `	return rc;` |
|        7 | 10947 |  |
|        - | 10948 | `/*` |
|        - | 10949 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 10950 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 10951 | ` * in the apArg[] array.` |
|        - | 10952 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 10953 | ` * return value indicates failure.` |
|        - | 10954 | ` */` |
|     1100 | 10955 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 10956 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 10957 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 10958 | `	int nArg,          /* Total number of given arguments */` |
|        - | 10959 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 10960 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 10961 | `	)` |
|        2 | 10962 |  |
|        - | 10963 | `	ph7_value *aStack;` |
|        - | 10964 | `	VmInstr aInstr[2];` |
|        - | 10965 | `	int i;` |
|     1102 | 10966 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 10967 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 10968 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 10969 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      137 | 10970 | `		return VmCallObjectInvoke(&(*pVm),` |
|       90 | 10971 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       45 | 10972 | `			nArg,apArg,pResult,0);` |
|        - | 10973 | `	}` |
|     1012 | 10974 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 10975 | `		/* Don't bother processing,it's invalid anyway */` |
|      509 | 10976 | `		if( pResult ){` |
|        - | 10977 | `			/* Assume a null return value */` |
|      ! 0 | 10978 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 10979 | `		}` |
|      509 | 10980 | `		return SXERR_INVALID;` |
|        - | 10981 | `	}` |
|      504 | 10982 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 10983 | `		/* Class method */` |
|       11 | 10984 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 10985 | `		ph7_class_method *pMethod = 0;` |
|       11 | 10986 | `		ph7_class_instance *pThis = 0;` |
|       11 | 10987 | `		ph7_class *pClass = 0;` |
|        - | 10988 | `		ph7_value *pValue;` |
|        - | 10989 | `		sxi32 rc;` |
|       11 | 10990 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 10991 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 10992 | `			if( pResult ){` |
|        - | 10993 | `				/* Assume a null return value */` |
|      ! 0 | 10994 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 10995 | `			}` |
|      ! 0 | 10996 | `			return SXRET_OK;` |
|        - | 10997 | `		}` |
|        - | 10998 | `		/* Extract the class name or an instance of it */` |
|       11 | 10999 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 11000 | `		if( pValue ){` |
|       11 | 11001 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 11002 | `		}` |
|       11 | 11003 | `		if( pClass == 0 ){` |
|        - | 11004 | `			/* No such class,return NULL */` |
|      ! 0 | 11005 | `			if( pResult ){` |
|      ! 0 | 11006 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11007 | `			}` |
|      ! 0 | 11008 | `			return SXRET_OK;` |
|        - | 11009 | `		}` |
|       11 | 11010 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11011 | `			/* Point to the class instance */` |
|        5 | 11012 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 11013 | `		}` |
|        - | 11014 | `		/* Try to extract the method */` |
|       11 | 11015 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 11016 | `		if( pValue ){` |
|       11 | 11017 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 11018 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 11019 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 11020 | `			}` |
|        5 | 11021 | `		}` |
|       11 | 11022 | `		if( pMethod == 0 ){` |
|        - | 11023 | `			/* No such method,return NULL */` |
|      ! 0 | 11024 | `			if( pResult ){` |
|      ! 0 | 11025 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11026 | `			}` |
|      ! 0 | 11027 | `			return SXRET_OK;` |
|        - | 11028 | `		}` |
|        - | 11029 | `		/* Call the class method */` |
|       11 | 11030 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 11031 | `		return rc;` |
|        - | 11032 | `	}` |
|        - | 11033 | `	/* Create a new operand stack */` |
|      494 | 11034 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      494 | 11035 | `	if( aStack == 0 ){` |
|      ! 0 | 11036 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11037 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11038 | `		if( pResult ){` |
|        - | 11039 | `			/* Assume a null return value */` |
|      ! 0 | 11040 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11041 | `		}` |
|      ! 0 | 11042 | `		return SXERR_MEM;` |
|        - | 11043 | `	}` |
|        - | 11044 | `	/* Fill the operand stack with the given arguments */` |
|     1604 | 11045 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1112 | 11046 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11047 | `		/*` |
|        - | 11048 | `		 * Symisc eXtension:` |
|        - | 11049 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11050 | `		 */` |
|     1112 | 11051 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      557 | 11052 | `	}` |
|        - | 11053 | `	/* Push the function name */` |
|      494 | 11054 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      494 | 11055 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11056 | `	/* Emit the CALL istruction */` |
|      494 | 11057 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      494 | 11058 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      494 | 11059 | `	aInstr[0].iP2 = 0;` |
|      494 | 11060 | `	aInstr[0].p3  = 0;` |
|        - | 11061 | `	/* Emit the DONE instruction */` |
|      494 | 11062 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      494 | 11063 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      494 | 11064 | `	aInstr[1].iP2 = 0;` |
|      494 | 11065 | `	aInstr[1].p3  = 0;` |
|        - | 11066 | `	/* Execute the function body (if available) */` |
|      494 | 11067 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11068 | `	/* Clean up the mess left behind */` |
|      494 | 11069 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      494 | 11070 | `	return PH7_OK;` |
|      552 | 11071 |  |
|        - | 11072 | `/*` |
|        - | 11073 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11074 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11075 | ` * parameter.` |
|        - | 11076 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11077 | ` * return value indicates failure.` |
|        - | 11078 | ` */` |
|      236 | 11079 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11080 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11081 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11082 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11083 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11084 | `	)` |
|        1 | 11085 |  |
|        - | 11086 | `	ph7_value *pArg;` |
|        - | 11087 | `	SySet aArg;` |
|        - | 11088 | `	va_list ap;` |
|        - | 11089 | `	sxi32 rc;` |
|      237 | 11090 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11091 | `	/* Copy arguments one after one */` |
|      237 | 11092 | `	va_start(ap,pResult);` |
|      393 | 11093 | `	for(;;){` |
|      787 | 11094 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 11095 | `		if( pArg == 0 ){` |
|      237 | 11096 | `			break;` |
|        - | 11097 | `		}` |
|      551 | 11098 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11099 | `	}` |
|        - | 11100 | `	/* Call the core routine */` |
|      237 | 11101 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11102 | `	/* Cleanup */` |
|      237 | 11103 | `	SySetRelease(&aArg);` |
|      237 | 11104 | `	return rc;` |
|        1 | 11105 |  |
|        - | 11106 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11107 | `/*` |
|        - | 11108 | ` * bool defined(string $name)` |
|        - | 11109 | ` *  Checks whether a given named constant exists.` |
|        - | 11110 | ` * Parameter:` |
|        - | 11111 | ` *  Name of the desired constant.` |
|        - | 11112 | ` * Return` |
|        - | 11113 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11114 | ` */` |
|       16 | 11115 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11116 |  |
|        - | 11117 | `	const char *zName;` |
|       18 | 11118 | `	int nLen = 0;` |
|       18 | 11119 | `	int res = 0;` |
|       18 | 11120 | `	if( nArg < 1 ){` |
|        - | 11121 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11122 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11123 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11124 | `		return SXRET_OK;` |
|        - | 11125 | `	}` |
|        - | 11126 | `	/* Extract constant name */` |
|       18 | 11127 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11128 | `	/* Perform the lookup */` |
|       18 | 11129 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11130 | `		/* Already defined */` |
|       12 | 11131 | `		res = 1;` |
|        5 | 11132 | `	}` |
|       18 | 11133 | `	ph7_result_bool(pCtx,res);` |
|       18 | 11134 | `	return SXRET_OK;` |
|       10 | 11135 |  |
|        - | 11136 | `/*` |
|        - | 11137 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11138 | ` * below.` |
|        - | 11139 | ` */` |
|       10 | 11140 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11141 |  |
|       12 | 11142 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11143 | `	/* Expand constant value */` |
|       12 | 11144 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11145 |  |
|        - | 11146 | `/*` |
|        - | 11147 | ` * bool define(string $constant_name,expression value)` |
|        - | 11148 | ` *  Defines a named constant at runtime.` |
|        - | 11149 | ` * Parameter:` |
|        - | 11150 | ` *  $constant_name` |
|        - | 11151 | ` *   The name of the constant` |
|        - | 11152 | ` *  $value` |
|        - | 11153 | ` *   Constant value` |
|        - | 11154 | ` * Return:` |
|        - | 11155 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11156 | ` */` |
|       12 | 11157 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11158 |  |
|        - | 11159 | `	const char *zName;  /* Constant name */` |
|        - | 11160 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11161 | `	int nLen = 0;       /* Name length */` |
|        - | 11162 | `	sxi32 rc;` |
|       14 | 11163 | `	if( nArg < 2 ){` |
|        - | 11164 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11165 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11166 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11167 | `		return SXRET_OK;` |
|        - | 11168 | `	}` |
|       14 | 11169 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11170 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11171 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11172 | `		return SXRET_OK;` |
|        - | 11173 | `	}` |
|        - | 11174 | `	/* Extract constant name */` |
|       14 | 11175 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11176 | `	if( nLen < 1 ){` |
|      ! 0 | 11177 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11178 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11179 | `		return SXRET_OK;` |
|        - | 11180 | `	}` |
|        - | 11181 | `	/* Duplicate constant value */` |
|       14 | 11182 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11183 | `	if( pValue == 0 ){` |
|      ! 0 | 11184 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11185 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11186 | `		return SXRET_OK;` |
|        - | 11187 | `	}` |
|        - | 11188 | `	/* Initialize the memory object */` |
|       14 | 11189 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11190 | `	/* Register the constant */` |
|       14 | 11191 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11192 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11193 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11194 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11195 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11196 | `		return SXRET_OK;` |
|        - | 11197 | `	}` |
|        - | 11198 | `	/* Duplicate constant value */` |
|       14 | 11199 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11200 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11201 | `		/* Lower case the constant name */` |
|      ! 0 | 11202 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11203 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11204 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11205 | `				/* UTF-8 stream */` |
|      ! 0 | 11206 | `				zCur++;` |
|      ! 0 | 11207 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11208 | `					zCur++;` |
|      ! 0 | 11209 | `				}` |
|      ! 0 | 11210 | `				continue;` |
|        - | 11211 | `			}` |
|      ! 0 | 11212 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11213 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11214 | `				zCur[0] = (char)c;` |
|      ! 0 | 11215 | `			}` |
|      ! 0 | 11216 | `			zCur++;` |
|      ! 0 | 11217 | `		}` |
|        - | 11218 | `		/* Finally,register the constant */` |
|      ! 0 | 11219 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11220 | `	}` |
|        - | 11221 | `	/* All done,return TRUE */` |
|       14 | 11222 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11223 | `	return SXRET_OK;` |
|        8 | 11224 |  |
|        - | 11225 | `/*` |
|        - | 11226 | ` * value constant(string $name)` |
|        - | 11227 | ` *  Returns the value of a constant` |
|        - | 11228 | ` * Parameter` |
|        - | 11229 | ` *  $name` |
|        - | 11230 | ` *    Name of the constant.` |
|        - | 11231 | ` * Return` |
|        - | 11232 | ` *  Constant value or NULL if not defined.` |
|        - | 11233 | ` */` |
|        8 | 11234 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11235 |  |
|        - | 11236 | `	SyHashEntry *pEntry;` |
|        - | 11237 | `	ph7_constant *pCons;` |
|        - | 11238 | `	const char *zName; /* Constant name */` |
|        - | 11239 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11240 | `	int nLen;` |
|       10 | 11241 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11242 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11243 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11244 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11245 | `		return SXRET_OK;` |
|        - | 11246 | `	}` |
|        - | 11247 | `	/* Extract the constant name */` |
|       10 | 11248 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11249 | `	/* Perform the query */` |
|       10 | 11250 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11251 | `	if( pEntry == 0 ){` |
|        3 | 11252 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11253 | `		ph7_result_null(pCtx);` |
|        3 | 11254 | `		return SXRET_OK;` |
|        - | 11255 | `	}` |
|        8 | 11256 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11257 | `	/* Point to the structure that describe the constant */` |
|        8 | 11258 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11259 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11260 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11261 | `	/* Return that value */` |
|        8 | 11262 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11263 | `	/* Cleanup */` |
|        8 | 11264 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11265 | `	return SXRET_OK;` |
|        6 | 11266 |  |
|        - | 11267 | `/*` |
|        - | 11268 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11269 | ` * defined below.` |
|        - | 11270 | ` */` |
|      452 | 11271 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11272 |  |
|      453 | 11273 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11274 | `	ph7_value sName;` |
|        - | 11275 | `	sxi32 rc;` |
|        - | 11276 | `	/* Prepare the constant name for insertion */` |
|      453 | 11277 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11278 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11279 | `	/* Perform the insertion */` |
|      453 | 11280 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11281 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11282 | `	return rc;` |
|        1 | 11283 |  |
|        - | 11284 | `/*` |
|        - | 11285 | ` * array get_defined_constants(void)` |
|        - | 11286 | ` *  Returns an associative array with the names of all defined` |
|        - | 11287 | ` *  constants.` |
|        - | 11288 | ` * Parameters` |
|        - | 11289 | ` *  NONE.` |
|        - | 11290 | ` * Returns` |
|        - | 11291 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11292 | ` */` |
|        2 | 11293 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11294 |  |
|        - | 11295 | `	ph7_value *pArray;` |
|        - | 11296 | `	/* Create the array first*/` |
|        3 | 11297 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11298 | `	if( pArray == 0 ){` |
|      ! 0 | 11299 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11300 | `		SXUNUSED(apArg);` |
|        - | 11301 | `		/* Return NULL */` |
|      ! 0 | 11302 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11303 | `		return SXRET_OK;` |
|        - | 11304 | `	}` |
|        - | 11305 | `	/* Fill the array with the defined constants */` |
|        3 | 11306 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11307 | `	/* Return the created array */` |
|        3 | 11308 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11309 | `	return SXRET_OK;` |
|        2 | 11310 |  |
|        - | 11311 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11312 | `/*` |
|        - | 11313 | ` * Section:` |
|        - | 11314 | ` *  Random numbers/string generators.` |
|        - | 11315 | ` * Status:` |
|        - | 11316 | ` *    Stable.` |
|        - | 11317 | ` */` |
|        - | 11318 | `/*` |
|        - | 11319 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11320 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11321 | ` * used by te SQLite3 library.` |
|        - | 11322 | ` */` |
|     2753 | 11323 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11324 |  |
|        - | 11325 | `	sxu32 iNum;` |
|     2755 | 11326 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2755 | 11327 | `	return iNum;` |
|        2 | 11328 |  |
|        - | 11329 | `/*` |
|        - | 11330 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11331 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11332 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11333 | ` * by te SQLite3 library.` |
|        - | 11334 | ` */` |
|   195242 | 11335 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11336 |  |
|        - | 11337 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11338 | `	int i;` |
|        - | 11339 | `	/* Generate a binary string first */` |
|   195244 | 11340 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11341 | `	/* Turn the binary string into english based alphabet */` |
|  2147832 | 11342 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  1952590 | 11343 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|   976296 | 11344 | `	 }` |
|   195244 | 11345 |  |
|        - | 11346 | `/*` |
|        - | 11347 | ` * int rand()` |
|        - | 11348 | ` * int mt_rand()` |
|        - | 11349 | ` * int rand(int $min,int $max)` |
|        - | 11350 | ` * int mt_rand(int $min,int $max)` |
|        - | 11351 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11352 | ` * Parameter` |
|        - | 11353 | ` *  $min` |
|        - | 11354 | ` *    The lowest value to return (default: 0)` |
|        - | 11355 | ` *  $max` |
|        - | 11356 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11357 | ` * Return` |
|        - | 11358 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11359 | ` * Note:` |
|        - | 11360 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11361 | ` *  by te SQLite3 library.` |
|        - | 11362 | ` */` |
|       20 | 11363 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11364 |  |
|        - | 11365 | `	sxu32 iNum;` |
|        - | 11366 | `	/* Generate the random number */` |
|       21 | 11367 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11368 | `	if( nArg > 1 ){` |
|        - | 11369 | `		sxu32 iMin,iMax;` |
|        3 | 11370 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11371 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11372 | `		if( iMin < iMax ){` |
|        3 | 11373 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11374 | `			if( iDiv > 0 ){` |
|        3 | 11375 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11376 | `			}` |
|        1 | 11377 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11378 | `			iNum %= iMax;` |
|      ! 0 | 11379 | `		}` |
|        1 | 11380 | `	}` |
|        - | 11381 | `	/* Return the number */` |
|       21 | 11382 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11383 | `	return SXRET_OK;` |
|        1 | 11384 |  |
|        - | 11385 | `/*` |
|        - | 11386 | ` * int getrandmax(void)` |
|        - | 11387 | ` * int mt_getrandmax(void)` |
|        - | 11388 | ` * int rc4_getrandmax(void)` |
|        - | 11389 | ` *   Show largest possible random value` |
|        - | 11390 | ` * Return` |
|        - | 11391 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11392 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11393 | ` * Note:` |
|        - | 11394 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11395 | ` *  by te SQLite3 library.` |
|        - | 11396 | ` */` |
|        4 | 11397 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11398 |  |
|        2 | 11399 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11400 | `	SXUNUSED(apArg);` |
|        5 | 11401 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11402 | `	return SXRET_OK;` |
|        1 | 11403 |  |
|        - | 11404 | `/*` |
|        - | 11405 | ` * string rand_str()` |
|        - | 11406 | ` * string rand_str(int $len)` |
|        - | 11407 | ` *  Generate a random string (English alphabet).` |
|        - | 11408 | ` * Parameter` |
|        - | 11409 | ` *  $len` |
|        - | 11410 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11411 | ` * Return` |
|        - | 11412 | ` *   A pseudo random string.` |
|        - | 11413 | ` * Note:` |
|        - | 11414 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11415 | ` *  by te SQLite3 library.` |
|        - | 11416 | ` *  This function is a symisc extension.` |
|        - | 11417 | ` */` |
|      120 | 11418 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11419 |  |
|        - | 11420 | `	char zString[1024];` |
|      122 | 11421 | `	int iLen = 0x10;` |
|      122 | 11422 | `	if( nArg > 0 ){` |
|        - | 11423 | `		/* Get the desired length */` |
|      122 | 11424 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11425 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11426 | `			/* Default length */` |
|        3 | 11427 | `			iLen = 0x10;` |
|        1 | 11428 | `		}` |
|       60 | 11429 | `	}` |
|        - | 11430 | `	/* Generate the random string */` |
|      122 | 11431 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11432 | `	/* Return the generated string */` |
|      122 | 11433 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11434 | `	return SXRET_OK;` |
|        2 | 11435 |  |
|        - | 11436 | `/*` |
|        - | 11437 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 11438 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 11439 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 11440 | ` */` |
|      488 | 11441 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 11442 |  |
|      488 | 11443 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 11444 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 11445 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11446 | `			"TypeError",` |
|        - | 11447 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 11448 | `			zFunc,iArgPos,zParamName,` |
|        3 | 11449 | `			ph7_type_name(pArg)` |
|        - | 11450 | `			);` |
|        - | 11451 | `	}` |
|      483 | 11452 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 11453 | `		int len;` |
|        9 | 11454 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 11455 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 11456 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11457 | `				"TypeError",` |
|        - | 11458 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 11459 | `				zFunc,iArgPos,zParamName` |
|        - | 11460 | `				);` |
|        - | 11461 | `		}` |
|        2 | 11462 | `	}` |
|      479 | 11463 | `	return SXRET_OK;` |
|      245 | 11464 |  |
|        - | 11465 | `/*` |
|        - | 11466 | ` * int random_int(int $min, int $max)` |
|        - | 11467 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 11468 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 11469 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 11470 | ` *  power-of-two mask covering the range.` |
|        - | 11471 | ` */` |
|      242 | 11472 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11473 |  |
|        - | 11474 | `	sxi64 iMin,iMax;` |
|        - | 11475 | `	sxu64 uRange,uMask,uResult;` |
|        - | 11476 | `	unsigned int nAttempt;` |
|        - | 11477 | `	int rc;` |
|      243 | 11478 | `	if( nArg != 2 ){` |
|       10 | 11479 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11480 | `			"ArgumentCountError",` |
|        - | 11481 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 11482 | `			nArg` |
|        - | 11483 | `			);` |
|        - | 11484 | `	}` |
|      237 | 11485 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 11486 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 11487 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 11488 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 11489 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 11490 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 11491 | `	if( iMin > iMax ){` |
|        3 | 11492 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11493 | `			"ValueError",` |
|        - | 11494 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 11495 | `			);` |
|        - | 11496 | `	}` |
|      229 | 11497 | `	if( iMin == iMax ){` |
|        5 | 11498 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 11499 | `		return SXRET_OK;` |
|        - | 11500 | `	}` |
|      225 | 11501 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 11502 | `	uMask = uRange;` |
|      225 | 11503 | `	uMask \|= uMask >> 1;` |
|      225 | 11504 | `	uMask \|= uMask >> 2;` |
|      225 | 11505 | `	uMask \|= uMask >> 4;` |
|      225 | 11506 | `	uMask \|= uMask >> 8;` |
|      225 | 11507 | `	uMask \|= uMask >> 16;` |
|      225 | 11508 | `	uMask \|= uMask >> 32;` |
|      225 | 11509 | `	uResult = 0;` |
|      371 | 11510 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 11511 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 11512 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 11513 | `		 * and the low-half mask would always read 0). */` |
|        - | 11514 | `		sxu64 uDraw;` |
|      371 | 11515 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 11516 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 11517 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 11518 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11519 | `				"Exception",` |
|        - | 11520 | `				"Cannot gather sufficient random data"` |
|        - | 11521 | `				);` |
|        - | 11522 | `		}` |
|      371 | 11523 | `		uDraw &= uMask;` |
|      371 | 11524 | `		if( uDraw <= uRange ){` |
|      225 | 11525 | `			uResult = uDraw;` |
|      225 | 11526 | `			break;` |
|        - | 11527 | `		}` |
|       83 | 11528 | `	}` |
|      225 | 11529 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 11530 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11531 | `			"Exception",` |
|        - | 11532 | `			"Cannot gather sufficient random data"` |
|        - | 11533 | `			);` |
|        - | 11534 | `	}` |
|      225 | 11535 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 11536 | `	return SXRET_OK;` |
|      122 | 11537 |  |
|        - | 11538 | `/*` |
|        - | 11539 | ` * string random_bytes(int $length)` |
|        - | 11540 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 11541 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 11542 | ` */` |
|       24 | 11543 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11544 |  |
|        - | 11545 | `	sxi64 iLen;` |
|        - | 11546 | `	unsigned char zStack[256];` |
|        - | 11547 | `	void *pBuf;` |
|        - | 11548 | `	int rc;` |
|       25 | 11549 | `	int bHeap = 0;` |
|       25 | 11550 | `	if( nArg != 1 ){` |
|        7 | 11551 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11552 | `			"ArgumentCountError",` |
|        - | 11553 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 11554 | `			nArg` |
|        - | 11555 | `			);` |
|        - | 11556 | `	}` |
|       21 | 11557 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 11558 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 11559 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 11560 | `	if( iLen < 1 ){` |
|        5 | 11561 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11562 | `			"ValueError",` |
|        - | 11563 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 11564 | `			);` |
|        - | 11565 | `	}` |
|        - | 11566 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 11567 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 11568 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 11569 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 11570 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11571 | `			"ValueError",` |
|        - | 11572 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 11573 | `			);` |
|        - | 11574 | `	}` |
|       13 | 11575 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 11576 | `		pBuf = zStack;` |
|        7 | 11577 | `	}else{` |
|      ! 0 | 11578 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 11579 | `		if( pBuf == 0 ){` |
|      ! 0 | 11580 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11581 | `				"Exception",` |
|        - | 11582 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 11583 | `				iLen` |
|        - | 11584 | `				);` |
|        - | 11585 | `		}` |
|      ! 0 | 11586 | `		bHeap = 1;` |
|        - | 11587 | `	}` |
|       13 | 11588 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 11589 | `		if( bHeap ){` |
|      ! 0 | 11590 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11591 | `		}` |
|      ! 0 | 11592 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11593 | `			"Exception",` |
|        - | 11594 | `			"Cannot gather sufficient random data"` |
|        - | 11595 | `			);` |
|        - | 11596 | `	}` |
|       13 | 11597 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 11598 | `	if( bHeap ){` |
|      ! 0 | 11599 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 11600 | `	}` |
|       13 | 11601 | `	return SXRET_OK;` |
|       13 | 11602 |  |
|        - | 11603 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 11604 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 11605 | `/* Unique ID private data */` |
|        - | 11606 | `struct unique_id_data` |
|        - | 11607 |  |
|        - | 11608 | `	ph7_context *pCtx; /* Call context */` |
|        - | 11609 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 11610 | `};` |
|        - | 11611 | `/*` |
|        - | 11612 | ` * Binary to hex consumer callback.` |
|        - | 11613 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 11614 | ` * defined below.` |
|        - | 11615 | ` */` |
|      192 | 11616 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 11617 |  |
|      193 | 11618 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 11619 | `	sxu32 nBuflen;` |
|        - | 11620 | `	/* Extract result buffer length */` |
|      193 | 11621 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 11622 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 11623 | `			/*` |
|        - | 11624 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 11625 | `			 * string will be 13 characters long` |
|        - | 11626 | `			 */` |
|       25 | 11627 | `		return SXERR_ABORT;` |
|        - | 11628 | `	}` |
|      169 | 11629 | `	if( nBuflen > 22 ){` |
|      ! 0 | 11630 | `		return SXERR_ABORT;` |
|        - | 11631 | `	}` |
|        - | 11632 | `	/* Safely Consume the hex stream */` |
|      169 | 11633 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 11634 | `	return SXRET_OK;` |
|       97 | 11635 |  |
|        - | 11636 | `/*` |
|        - | 11637 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 11638 | ` *  Generate a unique ID` |
|        - | 11639 | ` * Parameter` |
|        - | 11640 | ` * $prefix` |
|        - | 11641 | ` *  Append this prefix to the generated unique ID.` |
|        - | 11642 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 11643 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 11644 | ` * $more_entropy` |
|        - | 11645 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 11646 | ` *  that the result will be unique.` |
|        - | 11647 | ` * Return` |
|        - | 11648 | ` *  Returns the unique identifier, as a string.` |
|        - | 11649 | ` */` |
|       24 | 11650 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11651 |  |
|        - | 11652 | `	struct unique_id_data sUniq;` |
|        - | 11653 | `	unsigned char zDigest[20];` |
|       25 | 11654 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11655 | `	const char *zPrefix;` |
|        - | 11656 | `	SHA1Context sCtx;` |
|        - | 11657 | `	char zRandom[7];` |
|        - | 11658 | `	int nPrefix;` |
|        - | 11659 | `	int entropy;` |
|        - | 11660 | `	/* Generate a random string first */` |
|       25 | 11661 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 11662 | `	/* Initialize fields */` |
|       25 | 11663 | `	zPrefix = 0;` |
|       25 | 11664 | `	nPrefix = 0;` |
|       25 | 11665 | `	entropy = 0;` |
|       25 | 11666 | `	if( nArg > 0 ){` |
|        - | 11667 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 11668 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 11669 | `		if( nArg > 1 ){` |
|      ! 0 | 11670 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 11671 | `		}` |
|      ! 0 | 11672 | `	}` |
|       25 | 11673 | `	SHA1Init(&sCtx);` |
|        - | 11674 | `	/* Generate the random ID */` |
|       25 | 11675 | `	if( nPrefix > 0 ){` |
|      ! 0 | 11676 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 11677 | `	}` |
|        - | 11678 | `	/* Append the random ID */` |
|       25 | 11679 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 11680 | `	/* Append the random string */` |
|       25 | 11681 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 11682 | `	/* Increment the number */` |
|       25 | 11683 | `	pVm->unique_id++;` |
|       25 | 11684 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 11685 | `	/* Hexify the digest */` |
|       25 | 11686 | `	sUniq.pCtx = pCtx;` |
|       25 | 11687 | `	sUniq.entropy = entropy;` |
|       25 | 11688 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 11689 | `	/* All done */` |
|       25 | 11690 | `	return PH7_OK;` |
|        1 | 11691 |  |
|        - | 11692 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 11693 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 11694 | `/*` |
|        - | 11695 | ` * Section:` |
|        - | 11696 | ` *  Language construct implementation as foreign functions.` |
|        - | 11697 | ` * Status:` |
|        - | 11698 | ` *    Stable.` |
|        - | 11699 | ` */` |
|        - | 11700 | `/*` |
|        - | 11701 | ` * void echo($string...)` |
|        - | 11702 | ` *  Output one or more messages.` |
|        - | 11703 | ` * Parameters` |
|        - | 11704 | ` *  $string` |
|        - | 11705 | ` *   Message to output.` |
|        - | 11706 | ` * Return` |
|        - | 11707 | ` *  NULL.` |
|        - | 11708 | ` */` |
|      ! 0 | 11709 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11710 |  |
|        - | 11711 | `	const char *zData;` |
|      ! 0 | 11712 | `	int nDataLen = 0;` |
|        - | 11713 | `	ph7_vm *pVm;` |
|        - | 11714 | `	int i,rc;` |
|        - | 11715 | `	/* Point to the target VM */` |
|      ! 0 | 11716 | `	pVm = pCtx->pVm;` |
|        - | 11717 | `	/* Output */` |
|      ! 0 | 11718 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 11719 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 11720 | `		if( nDataLen > 0 ){` |
|      ! 0 | 11721 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 11722 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 11723 | `			if( rc == SXERR_ABORT ){` |
|        - | 11724 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11725 | `				return PH7_ABORT;` |
|        - | 11726 | `			}` |
|      ! 0 | 11727 | `		}` |
|      ! 0 | 11728 | `	}` |
|      ! 0 | 11729 | `	return SXRET_OK;` |
|      ! 0 | 11730 |  |
|        - | 11731 | `/*` |
|        - | 11732 | ` * int print($string...)` |
|        - | 11733 | ` *  Output one or more messages.` |
|        - | 11734 | ` * Parameters` |
|        - | 11735 | ` *  $string` |
|        - | 11736 | ` *   Message to output.` |
|        - | 11737 | ` * Return` |
|        - | 11738 | ` *  1 always.` |
|        - | 11739 | ` */` |
|        2 | 11740 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11741 |  |
|        - | 11742 | `	const char *zData;` |
|        3 | 11743 | `	int nDataLen = 0;` |
|        - | 11744 | `	ph7_vm *pVm;` |
|        - | 11745 | `	int i,rc;` |
|        - | 11746 | `	/* Point to the target VM */` |
|        3 | 11747 | `	pVm = pCtx->pVm;` |
|        - | 11748 | `	/* Output */` |
|        5 | 11749 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 11750 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 11751 | `		if( nDataLen > 0 ){` |
|        3 | 11752 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 11753 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 11754 | `			if( rc == SXERR_ABORT ){` |
|        - | 11755 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 11756 | `				return PH7_ABORT;` |
|        - | 11757 | `			}` |
|        1 | 11758 | `		}` |
|        2 | 11759 | `	}` |
|        - | 11760 | `	/* Return 1 */` |
|        3 | 11761 | `	ph7_result_int(pCtx,1);` |
|        3 | 11762 | `	return SXRET_OK;` |
|        2 | 11763 |  |
|        - | 11764 | `/*` |
|        - | 11765 | ` * void exit(string $msg)` |
|        - | 11766 | ` * void exit(int $status)` |
|        - | 11767 | ` * void die(string $ms)` |
|        - | 11768 | ` * void die(int $status)` |
|        - | 11769 | ` *   Output a message and terminate program execution.` |
|        - | 11770 | ` * Parameter` |
|        - | 11771 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 11772 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 11773 | ` *  and not printed` |
|        - | 11774 | ` * Return` |
|        - | 11775 | ` *  NULL` |
|        - | 11776 | ` */` |
|      ! 0 | 11777 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 11778 |  |
|      ! 0 | 11779 | `	if( nArg > 0 ){` |
|      ! 0 | 11780 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 11781 | `			const char *zData;` |
|      ! 0 | 11782 | `			int iLen = 0;` |
|        - | 11783 | `			/* Print exit message */` |
|      ! 0 | 11784 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 11785 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 11786 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 11787 | `			sxi32 iExitStatus;` |
|        - | 11788 | `			/* Record exit status code */` |
|      ! 0 | 11789 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 11790 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 11791 | `		}` |
|      ! 0 | 11792 | `	}` |
|        - | 11793 | `	/* Check if we are in an included file */` |
|      ! 0 | 11794 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 11795 | `		/* Exit the entire process */` |
|      ! 0 | 11796 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 11797 | `	}` |
|        - | 11798 | `	/* Abort processing immediately */` |
|      ! 0 | 11799 | `	return PH7_ABORT;` |
|      ! 0 | 11800 |  |
|        - | 11801 | `/*` |
|        - | 11802 | ` * bool isset($var,...)` |
|        - | 11803 | ` *  Finds out whether a variable is set.` |
|        - | 11804 | ` * Parameters` |
|        - | 11805 | ` *  One or more variable to check.` |
|        - | 11806 | ` * Return` |
|        - | 11807 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 11808 | ` */` |
|    89106 | 11809 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11810 |  |
|        - | 11811 | `	ph7_value *pObj;` |
|    89108 | 11812 | `	int res = 0;` |
|        - | 11813 | `	int i;` |
|    89108 | 11814 | `	if( nArg < 1 ){` |
|        - | 11815 | `		/* Missing arguments,return false */` |
|      ! 0 | 11816 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 11817 | `		return SXRET_OK;` |
|        - | 11818 | `	}` |
|        - | 11819 | `	/* Iterate over available arguments */` |
|   116580 | 11820 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    89108 | 11821 | `		pObj = apArg[i];` |
|    89108 | 11822 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|    60764 | 11823 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11824 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 11825 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 11826 | `			}` |
|    30381 | 11827 | `		}` |
|    89108 | 11828 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    89108 | 11829 | `		if( !res ){` |
|        - | 11830 | `			/* Variable not set,return FALSE */` |
|    61636 | 11831 | `			ph7_result_bool(pCtx,0);` |
|    61636 | 11832 | `			return SXRET_OK;` |
|        - | 11833 | `		}` |
|    13738 | 11834 | `	}` |
|        - | 11835 | `	/* All given variable are set,return TRUE */` |
|    27474 | 11836 | `	ph7_result_bool(pCtx,1);` |
|    27474 | 11837 | `	return SXRET_OK;` |
|    44555 | 11838 |  |
|        - | 11839 | `/*` |
|        - | 11840 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 11841 | ` * frame,the reference table and discard it's contents.` |
|        - | 11842 | ` * This function never fail and always return SXRET_OK.` |
|        - | 11843 | ` */` |
|  3119952 | 11844 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 11845 |  |
|        - | 11846 | `	ph7_value *pObj;` |
|        - | 11847 | `	VmRefObj *pRef;` |
|  3119954 | 11848 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3119954 | 11849 | `	if( pObj ){` |
|        - | 11850 | `		/* Release the object */` |
|  3119954 | 11851 | `		PH7_MemObjRelease(pObj);` |
|  1559976 | 11852 | `	}` |
|        - | 11853 | `	/* Remove old reference links */` |
|  3119954 | 11854 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3119954 | 11855 | `	if( pRef ){` |
|  3119948 | 11856 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 11857 | `		/* Unlink from the reference table */` |
|  3119948 | 11858 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3119948 | 11859 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 11860 | `			VmSlot sFree;` |
|        - | 11861 | `			/* Restore to the free list */` |
|  3119940 | 11862 | `			sFree.nIdx = nObjIdx;` |
|  3119940 | 11863 | `			sFree.pUserData = 0;` |
|  3119940 | 11864 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1559969 | 11865 | `		}` |
|  1559973 | 11866 | `	}` |
|  3119954 | 11867 | `	return SXRET_OK;` |
|        2 | 11868 |  |
|        - | 11869 | `/*` |
|        - | 11870 | ` * void unset($var,...)` |
|        - | 11871 | ` *   Unset one or more given variable.` |
|        - | 11872 | ` * Parameters` |
|        - | 11873 | ` *  One or more variable to unset.` |
|        - | 11874 | ` * Return` |
|        - | 11875 | ` *  Nothing.` |
|        - | 11876 | ` */` |
|     7332 | 11877 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11878 |  |
|        - | 11879 | `	ph7_value *pObj;` |
|        - | 11880 | `	ph7_vm *pVm;` |
|        - | 11881 | `	int i;` |
|        - | 11882 | `	/* Point to the target VM */` |
|     7334 | 11883 | `	pVm = pCtx->pVm;` |
|        - | 11884 | `	/* Iterate and unset */` |
|    14666 | 11885 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7334 | 11886 | `		pObj = apArg[i];` |
|     7334 | 11887 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      ! 0 | 11888 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 11889 | `				/* Throw an error */` |
|      ! 0 | 11890 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 11891 | `			}` |
|      ! 0 | 11892 | `		}else{` |
|     7334 | 11893 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 11894 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     7334 | 11895 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     7328 | 11896 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3663 | 11897 | `			}` |
|        - | 11898 | `		}` |
|     3668 | 11899 | `	}` |
|     7334 | 11900 | `	return SXRET_OK;` |
|        2 | 11901 |  |
|        - | 11902 | `/*` |
|        - | 11903 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 11904 | ` */` |
|      110 | 11905 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11906 |  |
|      111 | 11907 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 11908 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 11909 | `	ph7_value *pObj;` |
|        - | 11910 | `	sxu32 nIdx;` |
|        - | 11911 | `	/* Extract the memory object */` |
|      111 | 11912 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 11913 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 11914 | `	if( pObj ){` |
|      111 | 11915 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 11916 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 11917 | `				SyString sName;` |
|        - | 11918 | `				ph7_value sKey;` |
|        - | 11919 | `				/* Perform the insertion */` |
|      109 | 11920 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 11921 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 11922 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 11923 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 11924 | `			}` |
|       54 | 11925 | `		}` |
|       55 | 11926 | `	}` |
|      111 | 11927 | `	return SXRET_OK;` |
|        1 | 11928 |  |
|        - | 11929 | `/*` |
|        - | 11930 | ` * array get_defined_vars(void)` |
|        - | 11931 | ` *  Returns an array of all defined variables.` |
|        - | 11932 | ` * Parameter` |
|        - | 11933 | ` *  None` |
|        - | 11934 | ` * Return` |
|        - | 11935 | ` *  An array with all the variables defined in the current scope.` |
|        - | 11936 | ` */` |
|        2 | 11937 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11938 |  |
|        3 | 11939 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 11940 | `	ph7_value *pArray;` |
|        - | 11941 | `	/* Create a new array */` |
|        3 | 11942 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11943 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11944 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11945 | `		SXUNUSED(apArg);` |
|        - | 11946 | `		/* Return NULL */` |
|      ! 0 | 11947 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11948 | `		return SXRET_OK;` |
|        - | 11949 | `	}` |
|        - | 11950 | `	/* Superglobals first */` |
|        3 | 11951 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 11952 | `	/* Then variable defined in the current frame */` |
|        3 | 11953 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 11954 | `	/* Finally,return the created array */` |
|        3 | 11955 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11956 | `	return SXRET_OK;` |
|        2 | 11957 |  |
|        - | 11958 | `/*` |
|        - | 11959 | ` * bool gettype($var)` |
|        - | 11960 | ` *  Get the type of a variable` |
|        - | 11961 | ` * Parameters` |
|        - | 11962 | ` *   $var` |
|        - | 11963 | ` *    The variable being type checked.` |
|        - | 11964 | ` * Return` |
|        - | 11965 | ` *   String representation of the given variable type.` |
|        - | 11966 | ` */` |
|       32 | 11967 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11968 |  |
|       34 | 11969 | `	const char *zType = "Empty";` |
|       34 | 11970 | `	if( nArg > 0 ){` |
|       34 | 11971 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 11972 | `	}` |
|        - | 11973 | `	/* Return the variable type */` |
|       34 | 11974 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 11975 | `	return SXRET_OK;` |
|        2 | 11976 |  |
|        - | 11977 | `/*` |
|        - | 11978 | ` * string get_resource_type(resource $handle)` |
|        - | 11979 | ` *  This function gets the type of the given resource.` |
|        - | 11980 | ` * Parameters` |
|        - | 11981 | ` *  $handle` |
|        - | 11982 | ` *  The evaluated resource handle.` |
|        - | 11983 | ` * Return` |
|        - | 11984 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 11985 | ` *  representing its type. If the type is not identified by this function` |
|        - | 11986 | ` *  the return value will be the string Unknown.` |
|        - | 11987 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 11988 | ` *  is not a resource.` |
|        - | 11989 | ` */` |
|        2 | 11990 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11991 |  |
|        3 | 11992 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 11993 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 11994 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11995 | `		return PH7_OK;` |
|        - | 11996 | `	}` |
|        3 | 11997 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 11998 | `	return SXRET_OK;` |
|        2 | 11999 |  |
|        - | 12000 | `/*` |
|        - | 12001 | ` * void var_dump(expression,....)` |
|        - | 12002 | ` *   var_dump � Dumps information about a variable` |
|        - | 12003 | ` * Parameters` |
|        - | 12004 | ` *   One or more expression to dump.` |
|        - | 12005 | ` * Returns` |
|        - | 12006 | ` *  Nothing.` |
|        - | 12007 | ` */` |
|      218 | 12008 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12009 |  |
|        - | 12010 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12011 | `	int i;` |
|      220 | 12012 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12013 | `	/* Dump one or more expressions */` |
|      444 | 12014 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12015 | `		ph7_value *pObj = apArg[i];` |
|        - | 12016 | `		/* Reset the working buffer */` |
|      226 | 12017 | `		SyBlobReset(&sDump);` |
|        - | 12018 | `		/* Dump the given expression */` |
|      226 | 12019 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12020 | `		/* Output */` |
|      226 | 12021 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12022 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12023 | `		}` |
|      114 | 12024 | `	}` |
|        - | 12025 | `	/* Release the working buffer */` |
|      220 | 12026 | `	SyBlobRelease(&sDump);` |
|      220 | 12027 | `	return SXRET_OK;` |
|        2 | 12028 |  |
|        - | 12029 | `/*` |
|        - | 12030 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12031 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12032 | ` * Parameters` |
|        - | 12033 | ` *   expression: Expression to dump` |
|        - | 12034 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12035 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12036 | ` *            print_r() will return the information rather than print it.` |
|        - | 12037 | ` * Return` |
|        - | 12038 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12039 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12040 | ` */` |
|       16 | 12041 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12042 |  |
|       17 | 12043 | `	int ret_string = 0;` |
|        - | 12044 | `	SyBlob sDump;` |
|       17 | 12045 | `	if( nArg < 1 ){` |
|        - | 12046 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12047 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12048 | `		return SXRET_OK;` |
|        - | 12049 | `	}` |
|       17 | 12050 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12051 | `	if ( nArg > 1 ){` |
|        - | 12052 | `		/* Where to redirect output */` |
|       11 | 12053 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12054 | `	}` |
|        - | 12055 | `	/* Generate dump */` |
|       17 | 12056 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12057 | `	if( !ret_string ){` |
|        - | 12058 | `		/* Output dump */` |
|        7 | 12059 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12060 | `		/* Return true */` |
|        7 | 12061 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12062 | `	}else{` |
|        - | 12063 | `		/* Generated dump as return value */` |
|       11 | 12064 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12065 | `	}` |
|        - | 12066 | `	/* Release the working buffer */` |
|       17 | 12067 | `	SyBlobRelease(&sDump);` |
|       17 | 12068 | `	return SXRET_OK;` |
|        9 | 12069 |  |
|        - | 12070 | `/*` |
|        - | 12071 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12072 | ` * Same job as print_r. (see coment above)` |
|        - | 12073 | ` */` |
|        2 | 12074 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12075 |  |
|        3 | 12076 | `	int ret_string = 0;` |
|        - | 12077 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12078 | `	if( nArg < 1 ){` |
|        - | 12079 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12080 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12081 | `		return SXRET_OK;` |
|        - | 12082 | `	}` |
|        3 | 12083 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12084 | `	if ( nArg > 1 ){` |
|        - | 12085 | `		/* Where to redirect output */` |
|        3 | 12086 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12087 | `	}` |
|        - | 12088 | `	/* Generate dump */` |
|        3 | 12089 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12090 | `	if( !ret_string ){` |
|        - | 12091 | `		/* Output dump */` |
|      ! 0 | 12092 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12093 | `		/* Return NULL */` |
|      ! 0 | 12094 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12095 | `	}else{` |
|        - | 12096 | `		/* Generated dump as return value */` |
|        3 | 12097 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12098 | `	}` |
|        - | 12099 | `	/* Release the working buffer */` |
|        3 | 12100 | `	SyBlobRelease(&sDump);` |
|        3 | 12101 | `	return SXRET_OK;` |
|        2 | 12102 |  |
|        - | 12103 | `/*` |
|        - | 12104 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12105 | ` *  Set/get the various assert flags.` |
|        - | 12106 | ` * Parameter` |
|        - | 12107 | ` * $what` |
|        - | 12108 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12109 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12110 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12111 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12112 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12113 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12114 | ` * $value` |
|        - | 12115 | ` *   An optional new value for the option.` |
|        - | 12116 | ` * Return` |
|        - | 12117 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12118 | ` */` |
|       28 | 12119 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12120 |  |
|       30 | 12121 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12122 | `	int iOption;` |
|        - | 12123 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12124 | `	if( nArg < 1 ){` |
|        3 | 12125 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12126 | `			"ArgumentCountError",` |
|        - | 12127 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12128 | `			);` |
|        - | 12129 | `	}` |
|        - | 12130 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12131 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12132 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12133 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12134 | `			"TypeError",` |
|        - | 12135 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12136 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12137 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12138 | `			);` |
|        - | 12139 | `	}` |
|       28 | 12140 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12141 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12142 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12143 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12144 | `	switch( iOption ){` |
|        5 | 12145 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12146 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12147 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12148 | `		if( nArg > 1 ){` |
|        5 | 12149 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12150 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12151 | `			}else{` |
|        3 | 12152 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12153 | `			}` |
|        2 | 12154 | `		}` |
|       12 | 12155 | `		break;` |
|        1 | 12156 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12157 | `		/* Return old callback or null */` |
|        3 | 12158 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12159 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12160 | `		}else{` |
|        3 | 12161 | `			ph7_result_null(pCtx);` |
|        - | 12162 | `		}` |
|        3 | 12163 | `		if( nArg > 1 ){` |
|      ! 0 | 12164 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12165 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12166 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12167 | `			}else{` |
|      ! 0 | 12168 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12169 | `			}` |
|      ! 0 | 12170 | `		}` |
|        3 | 12171 | `		break;` |
|        5 | 12172 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12173 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12174 | `		if( nArg > 1 ){` |
|        5 | 12175 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12176 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12177 | `			}else{` |
|        3 | 12178 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12179 | `			}` |
|        2 | 12180 | `		}` |
|       11 | 12181 | `		break;` |
|      ! 0 | 12182 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12183 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12184 | `		break;` |
|        1 | 12185 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12186 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12187 | `		break;` |
|      ! 0 | 12188 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12189 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12190 | `		break;` |
|        1 | 12191 | `	default:` |
|        - | 12192 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12193 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12194 | `			"ValueError",` |
|        - | 12195 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12196 | `			);` |
|        - | 12197 | `	}` |
|       26 | 12198 | `	return PH7_OK;` |
|       16 | 12199 |  |
|        - | 12200 | `/*` |
|        - | 12201 | ` * bool assert(mixed $assertion)` |
|        - | 12202 | ` *  Checks if assertion is FALSE.` |
|        - | 12203 | ` * Parameter` |
|        - | 12204 | ` *  $assertion` |
|        - | 12205 | ` *    The assertion to test.` |
|        - | 12206 | ` * Return` |
|        - | 12207 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12208 | ` */` |
|       24 | 12209 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12210 |  |
|       26 | 12211 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12212 | `	int iFlags,iResult;` |
|        - | 12213 | `	const char *zDesc;` |
|        - | 12214 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12215 | `	if( nArg < 1 ){` |
|        3 | 12216 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12217 | `			"ArgumentCountError",` |
|        - | 12218 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12219 | `			);` |
|        - | 12220 | `	}` |
|       24 | 12221 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12222 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12223 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12224 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12225 | `		return PH7_OK;` |
|        - | 12226 | `	}` |
|        - | 12227 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12228 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12229 | `	if( !iResult ){` |
|        - | 12230 | `		/* Assertion failed */` |
|        - | 12231 | `		/* Extract optional description */` |
|       13 | 12232 | `		zDesc = 0;` |
|       13 | 12233 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12234 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12235 | `		}` |
|       13 | 12236 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12237 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12238 | `			ph7_value sFile,sLine;` |
|        - | 12239 | `			ph7_value *apCbArg[3];` |
|        - | 12240 | `			SyString *pFile;` |
|        - | 12241 | `			/* Extract the processed script */` |
|      ! 0 | 12242 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12243 | `			if( pFile == 0 ){` |
|      ! 0 | 12244 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12245 | `			}` |
|        - | 12246 | `			/* Invoke the callback */` |
|      ! 0 | 12247 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12248 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12249 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12250 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12251 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12252 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12253 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12254 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12255 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12256 | `		}` |
|       13 | 12257 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12258 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12259 | `			return PH7_ABORT;` |
|        - | 12260 | `		}` |
|        - | 12261 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12262 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12263 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12264 | `				"AssertionError",` |
|        - | 12265 | `				"%s",` |
|        1 | 12266 | `				zDesc` |
|        - | 12267 | `				);` |
|      ! 0 | 12268 | `		}else{` |
|       11 | 12269 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12270 | `				"AssertionError",` |
|        - | 12271 | `				"assert(false)"` |
|        - | 12272 | `				);` |
|        - | 12273 | `		}` |
|        - | 12274 | `	}` |
|        - | 12275 | `	/* Assertion passed */` |
|       11 | 12276 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12277 | `	return PH7_OK;` |
|       14 | 12278 |  |
|        - | 12279 | `/*` |
|        - | 12280 | ` * Section:` |
|        - | 12281 | ` *  Error reporting functions.` |
|        - | 12282 | ` * Status:` |
|        - | 12283 | ` *    Stable.` |
|        - | 12284 | ` */` |
|        - | 12285 | `/*` |
|        - | 12286 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12287 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12288 | ` * Parameters` |
|        - | 12289 | ` *  $error_msg` |
|        - | 12290 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12291 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12292 | ` * $error_type` |
|        - | 12293 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12294 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12295 | ` * Return` |
|        - | 12296 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12297 | ` */` |
|       12 | 12298 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12299 |  |
|       14 | 12300 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12301 | `	int rc = PH7_OK;` |
|       14 | 12302 | `	if( nArg > 0 ){` |
|        - | 12303 | `		const char *zErr;` |
|        - | 12304 | `		int nLen;` |
|        - | 12305 | `		/* Extract the error message */` |
|       12 | 12306 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12307 | `		if( nArg > 1 ){` |
|        - | 12308 | `			/* Extract the error type */` |
|       12 | 12309 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12310 | `			switch( nErr ){` |
|        1 | 12311 | `			case 1:   /* E_ERROR */` |
|        - | 12312 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12313 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12314 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12315 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12316 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12317 | `				break;` |
|        1 | 12318 | `			case 2:   /* E_WARNING */` |
|        - | 12319 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12320 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12321 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12322 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12323 | `				break;` |
|        3 | 12324 | `			default:` |
|        8 | 12325 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12326 | `				break;` |
|        - | 12327 | `			}` |
|        5 | 12328 | `		}` |
|        - | 12329 | `		/* Report error */` |
|       12 | 12330 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12331 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12332 | `			return rc;` |
|        - | 12333 | `		}` |
|        - | 12334 | `		/* Return true */` |
|       12 | 12335 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12336 | `	}else{` |
|        - | 12337 | `		/* Missing arguments,return FALSE */` |
|        3 | 12338 | `		ph7_result_bool(pCtx,0);` |
|        - | 12339 | `	}` |
|       14 | 12340 | `	return rc;` |
|        8 | 12341 |  |
|        - | 12342 | `/*` |
|        - | 12343 | ` * int error_reporting([int $level])` |
|        - | 12344 | ` *  Sets which PHP errors are reported.` |
|        - | 12345 | ` * Parameters` |
|        - | 12346 | ` *  $level` |
|        - | 12347 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12348 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12349 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12350 | ` *   levels will not always behave as expected.` |
|        - | 12351 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12352 | ` *   in the predefined constants.` |
|        - | 12353 | ` * Return` |
|        - | 12354 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12355 | ` *   parameter is given.` |
|        - | 12356 | ` */` |
|       38 | 12357 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12358 |  |
|       40 | 12359 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12360 | `	int nOld;` |
|        - | 12361 | `	/* Extract the old reporting level */` |
|       40 | 12362 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 12363 | `	if( nArg > 0 ){` |
|        - | 12364 | `		int nNew;` |
|        - | 12365 | `		/* Extract the desired error reporting level */` |
|       32 | 12366 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 12367 | `		if( !nNew ){` |
|        - | 12368 | `			/* Do not report errors at all */` |
|        5 | 12369 | `			pVm->bErrReport = 0;` |
|        3 | 12370 | `		}else{` |
|        - | 12371 | `			/* Report all errors */` |
|       28 | 12372 | `			pVm->bErrReport = 1;` |
|        - | 12373 | `		}` |
|       15 | 12374 | `	}` |
|        - | 12375 | `	/* Return the old level */` |
|       40 | 12376 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12377 | `	return PH7_OK;` |
|        2 | 12378 |  |
|        - | 12379 | `/*` |
|        - | 12380 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12381 | ` *  Send an error message somewhere.` |
|        - | 12382 | ` * Parameter` |
|        - | 12383 | ` *  $message` |
|        - | 12384 | ` *   The error message that should be logged.` |
|        - | 12385 | ` *  $message_type` |
|        - | 12386 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12387 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12388 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12389 | ` *       This is the default option.` |
|        - | 12390 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12391 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12392 | ` *    2  No longer an option.` |
|        - | 12393 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12394 | ` *       to the end of the message string.` |
|        - | 12395 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12396 | ` *  $destination` |
|        - | 12397 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12398 | ` *  $extra_headers` |
|        - | 12399 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12400 | ` * Return` |
|        - | 12401 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12402 | ` * NOTE:` |
|        - | 12403 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12404 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12405 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12406 | ` *  Otherwise this function is no-op.` |
|        - | 12407 | ` */` |
|        4 | 12408 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12409 |  |
|        - | 12410 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12411 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12412 | `	int iType = 0;` |
|        5 | 12413 | `	if( nArg < 1 ){` |
|        - | 12414 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12415 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12416 | `		return PH7_OK;` |
|        - | 12417 | `	}` |
|        5 | 12418 | `	if( pVm->xErrLog  ){` |
|        - | 12419 | `		/* Invoke the user callback */` |
|      ! 0 | 12420 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12421 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12422 | `		if( nArg > 1 ){` |
|      ! 0 | 12423 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12424 | `			if( nArg > 2 ){` |
|      ! 0 | 12425 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12426 | `				if( nArg > 3 ){` |
|      ! 0 | 12427 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12428 | `				}` |
|      ! 0 | 12429 | `			}` |
|      ! 0 | 12430 | `		}` |
|      ! 0 | 12431 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12432 | `	}` |
|        - | 12433 | `	/* Retun TRUE */` |
|        5 | 12434 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12435 | `	return PH7_OK;` |
|        3 | 12436 |  |
|        - | 12437 | `/*` |
|        - | 12438 | ` * bool restore_exception_handler(void)` |
|        - | 12439 | ` *  Restores the previously defined exception handler function.` |
|        - | 12440 | ` * Parameter` |
|        - | 12441 | ` *  None` |
|        - | 12442 | ` * Return` |
|        - | 12443 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12444 | ` */` |
|        4 | 12445 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12446 |  |
|        5 | 12447 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12448 | `	ph7_value *pOld,*pNew;` |
|        - | 12449 | `	/* Point to the old and the new handler */` |
|        5 | 12450 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12451 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12452 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12453 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12454 | `		SXUNUSED(apArg);` |
|        - | 12455 | `		/* No installed handler,return FALSE */` |
|        5 | 12456 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12457 | `		return PH7_OK;` |
|        - | 12458 | `	}` |
|        - | 12459 | `	/* Copy the old handler */` |
|      ! 0 | 12460 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12461 | `	PH7_MemObjRelease(pOld);` |
|        - | 12462 | `	/* Return TRUE */` |
|      ! 0 | 12463 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12464 | `	return PH7_OK;` |
|        3 | 12465 |  |
|        - | 12466 | `/*` |
|        - | 12467 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12468 | ` *  Sets a user-defined exception handler function.` |
|        - | 12469 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12470 | ` * NOTE` |
|        - | 12471 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12472 | ` *  the satndard PHP engine.` |
|        - | 12473 | ` * Parameters` |
|        - | 12474 | ` *  $exception_handler` |
|        - | 12475 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12476 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12477 | ` *   that was thrown.` |
|        - | 12478 | ` *  Note:` |
|        - | 12479 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12480 | ` * Return` |
|        - | 12481 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12482 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12483 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12484 | ` */` |
|        4 | 12485 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12486 |  |
|        6 | 12487 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12488 | `	ph7_value *pOld,*pNew;` |
|        - | 12489 | `	/* Point to the old and the new handler */` |
|        6 | 12490 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 12491 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 12492 | `	/* Return the old handler */` |
|        6 | 12493 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 12494 | `	if( nArg > 0 ){` |
|        6 | 12495 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12496 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 12497 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 12498 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 12499 | `		}else{` |
|        6 | 12500 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12501 | `			/* Install the new handler */` |
|        6 | 12502 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12503 | `		}` |
|        2 | 12504 | `	}` |
|        6 | 12505 | `	return PH7_OK;` |
|        2 | 12506 |  |
|        - | 12507 | `/*` |
|        - | 12508 | ` * bool restore_error_handler(void)` |
|        - | 12509 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12510 | ` * Parameters:` |
|        - | 12511 | ` *  None.` |
|        - | 12512 | ` * Return` |
|        - | 12513 | ` *  Always TRUE.` |
|        - | 12514 | ` */` |
|        6 | 12515 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12516 |  |
|        7 | 12517 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12518 | `	ph7_value *pOld,*pNew;` |
|        - | 12519 | `	/* Point to the old and the new handler */` |
|        7 | 12520 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 12521 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 12522 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 12523 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 12524 | `		SXUNUSED(apArg);` |
|        - | 12525 | `		/* No installed callback,return FALSE */` |
|        7 | 12526 | `		ph7_result_bool(pCtx,0);` |
|        7 | 12527 | `		return PH7_OK;` |
|        - | 12528 | `	}` |
|        - | 12529 | `	/* Copy the old callback */` |
|      ! 0 | 12530 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12531 | `	PH7_MemObjRelease(pOld);` |
|        - | 12532 | `	/* Return TRUE */` |
|      ! 0 | 12533 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12534 | `	return PH7_OK;` |
|        4 | 12535 |  |
|        - | 12536 | `/*` |
|        - | 12537 | ` * value set_error_handler(callable $error_handler)` |
|        - | 12538 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12539 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 12540 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 12541 | ` *  Sets a user-defined error handler function.` |
|        - | 12542 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 12543 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 12544 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 12545 | ` *  conditions (using trigger_error()).` |
|        - | 12546 | ` * Parameters` |
|        - | 12547 | ` *  $error_handler` |
|        - | 12548 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 12549 | ` *   describing the error.` |
|        - | 12550 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 12551 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 12552 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 12553 | ` *   The function can be shown as:` |
|        - | 12554 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 12555 | ` *     errno` |
|        - | 12556 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 12557 | ` *   errstr` |
|        - | 12558 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 12559 | ` *   errfile` |
|        - | 12560 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 12561 | ` *     was raised in, as a string.` |
|        - | 12562 | ` *  Note:` |
|        - | 12563 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12564 | ` * Return` |
|        - | 12565 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 12566 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12567 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12568 | ` */` |
|    10532 | 12569 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12570 |  |
|    10534 | 12571 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12572 | `	ph7_value *pOld,*pNew;` |
|        - | 12573 | `	/* Point to the old and the new handler */` |
|    10534 | 12574 | `	pOld = &pVm->aErrCB[0];` |
|    10534 | 12575 | `	pNew = &pVm->aErrCB[1];` |
|        - | 12576 | `	/* Return the old handler */` |
|    10534 | 12577 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10534 | 12578 | `	if( nArg > 0 ){` |
|    10534 | 12579 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 12580 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5261 | 12581 | `			PH7_MemObjRelease(pNew);` |
|     5261 | 12582 | `			ph7_result_bool(pCtx,1);` |
|     2631 | 12583 | `		}else{` |
|     5274 | 12584 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 12585 | `			/* Install the new handler */` |
|     5274 | 12586 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 12587 | `		}` |
|     5266 | 12588 | `	}` |
|    10534 | 12589 | `	return PH7_OK;` |
|        2 | 12590 |  |
|        - | 12591 | `/*` |
|        - | 12592 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 12593 | ` *  Generates a backtrace.` |
|        - | 12594 | ` * Paramaeter` |
|        - | 12595 | ` *  $options` |
|        - | 12596 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 12597 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 12598 | ` *   all the function/method arguments, to save memory.` |
|        - | 12599 | ` * $limit` |
|        - | 12600 | ` *   (Not Used)` |
|        - | 12601 | ` * Return` |
|        - | 12602 | ` *  An array.The possible returned elements are as follows:` |
|        - | 12603 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 12604 | ` *          Name        Type      Description` |
|        - | 12605 | ` *          ------      ------     -----------` |
|        - | 12606 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 12607 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 12608 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 12609 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 12610 | ` *          object      object    The current object.` |
|        - | 12611 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 12612 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 12613 | ` */` |
|      832 | 12614 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12615 |  |
|      834 | 12616 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12617 | `	ph7_value *pArray;` |
|        - | 12618 | `	ph7_class *pClass;` |
|        - | 12619 | `	ph7_value *pValue;` |
|        - | 12620 | `	SyString *pFile;` |
|        - | 12621 | `	/* Create a new array */` |
|      834 | 12622 | `	pArray = ph7_context_new_array(pCtx);` |
|      834 | 12623 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      834 | 12624 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 12625 | `		/* Out of memory,return NULL */` |
|      ! 0 | 12626 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 12627 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12628 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12629 | `		SXUNUSED(apArg);` |
|      ! 0 | 12630 | `		return PH7_OK;` |
|        - | 12631 | `	}` |
|        - | 12632 | `	/* Dump running function name and it's arguments  */` |
|      834 | 12633 | `	if( pVm->pFrame->pParent ){` |
|      834 | 12634 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 12635 | `		ph7_vm_func *pFunc;` |
|        - | 12636 | `		ph7_value *pArg;` |
|      834 | 12637 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      834 | 12638 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      834 | 12639 | `		if( pFrame->pParent && pFunc ){` |
|      834 | 12640 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      834 | 12641 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      834 | 12642 | `			ph7_value_reset_string_cursor(pValue);` |
|      416 | 12643 | `		}` |
|        - | 12644 | `		/* Function arguments */` |
|      834 | 12645 | `		pArg = ph7_context_new_array(pCtx);` |
|      834 | 12646 | `		if( pArg  ){` |
|        - | 12647 | `			ph7_value *pObj;` |
|        - | 12648 | `			VmSlot *aSlot;` |
|        - | 12649 | `			sxu32 n;` |
|        - | 12650 | `			/* Start filling the array with the given arguments */` |
|      834 | 12651 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3334 | 12652 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2502 | 12653 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2502 | 12654 | `				if( pObj ){` |
|     2502 | 12655 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1250 | 12656 | `				}` |
|     1252 | 12657 | `			}` |
|        - | 12658 | `			/* Save the array */` |
|      834 | 12659 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      416 | 12660 | `		}` |
|      416 | 12661 | `	}` |
|      834 | 12662 | `	ph7_value_int(pValue,1);` |
|        - | 12663 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 12664 | `	 * line numbers at run-time. )` |
|        - | 12665 | `	 */` |
|      834 | 12666 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 12667 | `	/* Current processed script */` |
|      834 | 12668 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      834 | 12669 | `	if( pFile ){` |
|      834 | 12670 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      834 | 12671 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      834 | 12672 | `		ph7_value_reset_string_cursor(pValue);` |
|      416 | 12673 | `	}` |
|        - | 12674 | `	/* Top class */` |
|      834 | 12675 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      834 | 12676 | `	if( pClass ){` |
|      830 | 12677 | `		ph7_value_reset_string_cursor(pValue);` |
|      830 | 12678 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      830 | 12679 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      414 | 12680 | `	}` |
|        - | 12681 | `	/* Return the freshly created array */` |
|      834 | 12682 | `	ph7_result_value(pCtx,pArray);` |
|        - | 12683 | `	/*` |
|        - | 12684 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 12685 | `	 * as soon we return from this function.` |
|        - | 12686 | `	 */` |
|      834 | 12687 | `	return PH7_OK;` |
|      418 | 12688 |  |
|        - | 12689 | `/*` |
|        - | 12690 | ` * Generate a small backtrace.` |
|        - | 12691 | ` * Store the generated dump in the given BLOB` |
|        - | 12692 | ` */` |
|        4 | 12693 | `static int VmMiniBacktrace(` |
|        - | 12694 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12695 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 12696 | `	)` |
|        1 | 12697 |  |
|        5 | 12698 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 12699 | `	ph7_vm_func *pFunc;` |
|        - | 12700 | `	ph7_class *pClass;` |
|        - | 12701 | `	SyString *pFile;` |
|        - | 12702 | `	/* Called function */` |
|        5 | 12703 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 12704 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 12705 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12706 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 12707 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 12708 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 12709 | `	}else{` |
|      ! 0 | 12710 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 12711 | `	}` |
|        5 | 12712 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 12713 | `	/* Current processed script */` |
|        5 | 12714 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 12715 | `	if( pFile ){` |
|        5 | 12716 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 12717 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 12718 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 12719 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 12720 | `	}` |
|        - | 12721 | `	/* Top class */` |
|        5 | 12722 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 12723 | `	if( pClass ){` |
|      ! 0 | 12724 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 12725 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 12726 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 12727 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 12728 | `	}` |
|        5 | 12729 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 12730 | `	/* All done */` |
|        5 | 12731 | `	return SXRET_OK;` |
|        1 | 12732 |  |
|        - | 12733 | `/*` |
|        - | 12734 | ` * void debug_print_backtrace()` |
|        - | 12735 | ` *  Prints a backtrace` |
|        - | 12736 | ` * Parameters` |
|        - | 12737 | ` * None` |
|        - | 12738 | ` * Return` |
|        - | 12739 | ` * NULL` |
|        - | 12740 | ` */` |
|        2 | 12741 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12742 |  |
|        3 | 12743 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12744 | `	SyBlob sDump;` |
|        3 | 12745 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12746 | `	/* Generate the backtrace */` |
|        3 | 12747 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12748 | `	/* Output backtrace */` |
|        3 | 12749 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12750 | `	/* All done,cleanup */` |
|        3 | 12751 | `	SyBlobRelease(&sDump);` |
|        1 | 12752 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12753 | `	SXUNUSED(apArg);` |
|        3 | 12754 | `	return PH7_OK;` |
|        1 | 12755 |  |
|        - | 12756 | `/*` |
|        - | 12757 | ` * string debug_string_backtrace()` |
|        - | 12758 | ` *  Generate a backtrace` |
|        - | 12759 | ` * Parameters` |
|        - | 12760 | ` * None` |
|        - | 12761 | ` * Return` |
|        - | 12762 | ` *  A mini backtrace().` |
|        - | 12763 | ` * Note that this is a symisc extension.` |
|        - | 12764 | ` */` |
|        2 | 12765 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12766 |  |
|        3 | 12767 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12768 | `	SyBlob sDump;` |
|        3 | 12769 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 12770 | `	/* Generate the backtrace */` |
|        3 | 12771 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 12772 | `	/* Return the backtrace */` |
|        3 | 12773 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 12774 | `	/* All done,cleanup */` |
|        3 | 12775 | `	SyBlobRelease(&sDump);` |
|        1 | 12776 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 12777 | `	SXUNUSED(apArg);` |
|        3 | 12778 | `	return PH7_OK;` |
|        1 | 12779 |  |
|        - | 12780 | `/*` |
|        - | 12781 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 12782 | ` * exception is triggered.` |
|        - | 12783 | ` */` |
|      510 | 12784 | `static sxi32 VmUncaughtException(` |
|        - | 12785 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 12786 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12787 | `	)` |
|        1 | 12788 |  |
|        - | 12789 | `	ph7_value *apArg[2],sArg;` |
|      511 | 12790 | `	int nArg = 1;` |
|        - | 12791 | `	sxi32 rc;` |
|      511 | 12792 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 12793 | `		/* Nesting limit reached */` |
|      ! 0 | 12794 | `		return SXRET_OK;` |
|        - | 12795 | `	}` |
|        - | 12796 | `	/* Call any exception handler if available */` |
|      511 | 12797 | `	PH7_MemObjInit(pVm,&sArg);` |
|      511 | 12798 | `	if( pThis ){` |
|        - | 12799 | `		/* Load the exception instance */` |
|      511 | 12800 | `		sArg.x.pOther = pThis;` |
|      511 | 12801 | `		pThis->iRef++;` |
|      511 | 12802 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      256 | 12803 | `	}else{` |
|      ! 0 | 12804 | `		nArg = 0;` |
|        - | 12805 | `	}` |
|      511 | 12806 | `	apArg[0] = &sArg;` |
|        - | 12807 | `	/* Call the exception handler if available */` |
|      511 | 12808 | `	pVm->nExceptDepth++;` |
|      511 | 12809 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      511 | 12810 | `	pVm->nExceptDepth--;` |
|      511 | 12811 | `	if( rc != SXRET_OK ){` |
|        - | 12812 | `		SyBlob sMsgBuf;` |
|      509 | 12813 | `		const char *zClass = "Exception";` |
|      509 | 12814 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 12815 | `		const char *zMsg;` |
|        - | 12816 | `		sxu32 nMsg;` |
|        - | 12817 | `		const char *zFuncName;` |
|        - | 12818 | `		int nFuncLen;` |
|      509 | 12819 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      509 | 12820 | `		if( pThis ){` |
|        - | 12821 | `			ph7_class_method *pGetMessage;` |
|        - | 12822 | `			ph7_value sMsg;` |
|        - | 12823 | `			const char *zTmp;` |
|        - | 12824 | `			int nTmp;` |
|      509 | 12825 | `			zClass = pThis->pClass->sName.zString;` |
|      509 | 12826 | `			nClass = pThis->pClass->sName.nByte;` |
|      509 | 12827 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      509 | 12828 | `			if( pGetMessage ){` |
|      509 | 12829 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      509 | 12830 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      509 | 12831 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      509 | 12832 | `					if( zTmp && nTmp > 0 ){` |
|      509 | 12833 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      254 | 12834 | `					}` |
|      254 | 12835 | `				}` |
|      509 | 12836 | `				PH7_MemObjRelease(&sMsg);` |
|      254 | 12837 | `			}` |
|      254 | 12838 | `		}` |
|      509 | 12839 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      509 | 12840 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      509 | 12841 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      509 | 12842 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      509 | 12843 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 12844 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      509 | 12845 | `		rc = SXERR_ABORT;` |
|      254 | 12846 | `	}` |
|      511 | 12847 | `	PH7_MemObjRelease(&sArg);` |
|      511 | 12848 | `	return rc;` |
|      256 | 12849 |  |
|        - | 12850 | `/*` |
|        - | 12851 | ` * Throw a user exception.` |
|        - | 12852 | ` *` |
|        - | 12853 | ` * Exception dispatch follows this sequence:` |
|        - | 12854 | ` *` |
|        - | 12855 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 12856 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 12857 | ` *` |
|        - | 12858 | ` * 2. If NO catch matches:` |
|        - | 12859 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 12860 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 12861 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 12862 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 12863 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 12864 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 12865 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 12866 | ` *` |
|        - | 12867 | ` * 3. If a catch DOES match:` |
|        - | 12868 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 12869 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 12870 | ` *       inside the catch body from immediately propagating past our` |
|        - | 12871 | ` *       finally block.` |
|        - | 12872 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 12873 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 12874 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 12875 | ` *       in pPendingException (step 2c).` |
|        - | 12876 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 12877 | ` *    d. Run finally (if present).` |
|        - | 12878 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 12879 | ` *       that handlers are restored and finally has run.` |
|        - | 12880 | ` */` |
|      788 | 12881 | `static sxi32 VmThrowException(` |
|        - | 12882 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 12883 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 12884 | `	)` |
|        2 | 12885 |  |
|        - | 12886 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 12887 | `	ph7_exception **apException;` |
|        - | 12888 | `	ph7_exception *pException;` |
|        - | 12889 | `	/* Point to the stack of loaded exceptions */` |
|      790 | 12890 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      790 | 12891 | `	pException = 0;` |
|      790 | 12892 | `	pCatch = 0;` |
|      790 | 12893 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12894 | `		ph7_exception_block *aCatch;` |
|        - | 12895 | `		ph7_class *pClass;` |
|        - | 12896 | `		SyString *aNames;` |
|        - | 12897 | `		sxu32 nNames;` |
|        - | 12898 | `		int matched;` |
|        - | 12899 | `		sxu32 j,k;` |
|        - | 12900 | `		/* Locate the appropriate block to execute */` |
|      272 | 12901 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      272 | 12902 | `		(void)SySetPop(&pVm->aException);` |
|      272 | 12903 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      280 | 12904 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 12905 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      278 | 12906 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      278 | 12907 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      278 | 12908 | `			matched = 0;` |
|      304 | 12909 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 12910 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 12911 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 12912 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      296 | 12913 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      296 | 12914 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 12915 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 12916 | `					continue;` |
|        - | 12917 | `				}` |
|      296 | 12918 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      270 | 12919 | `					matched = 1;` |
|      270 | 12920 | `					break;` |
|        - | 12921 | `				}` |
|       14 | 12922 | `			}` |
|      278 | 12923 | `			if( matched ){` |
|        - | 12924 | `				/* Catch block found,break immediately */` |
|      270 | 12925 | `				pCatch = &aCatch[j];` |
|      270 | 12926 | `				break;` |
|        - | 12927 | `			}` |
|        5 | 12928 | `		}` |
|      135 | 12929 | `	}` |
|        - | 12930 | `	/* Execute the cached block if available */` |
|      790 | 12931 | `	if( pCatch == 0 ){` |
|        - | 12932 | `		sxi32 rc;` |
|        - | 12933 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      522 | 12934 | `		if( pException && pException->iHasFinally ){` |
|        3 | 12935 | `			pException->iFinallyDone = 1;` |
|        3 | 12936 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 12937 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 12938 | `				return SXERR_ABORT;` |
|        - | 12939 | `			}` |
|        1 | 12940 | `		}` |
|        - | 12941 | `		/* Check if there is an outer exception handler on the stack */` |
|      522 | 12942 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 12943 | `			/* Re-throw to the outer handler */` |
|        3 | 12944 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 12945 | `		}` |
|        - | 12946 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 12947 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 12948 | `		 * exception instead of reporting it uncaught.` |
|        - | 12949 | `		 */` |
|      520 | 12950 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 12951 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 12952 | `			 * by looking for a catch frame on the stack.` |
|        - | 12953 | `			 */` |
|      520 | 12954 | `			VmFrame *pF = pVm->pFrame;` |
|      520 | 12955 | `			int inCatch = 0;` |
|     1046 | 12956 | `			while( pF ){` |
|      536 | 12957 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 12958 | `					inCatch = 1;` |
|        9 | 12959 | `					break;` |
|        - | 12960 | `				}` |
|      527 | 12961 | `				pF = pF->pParent;` |
|        1 | 12962 | `			}` |
|      520 | 12963 | `			if( inCatch ){` |
|        - | 12964 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 12965 | `				pThis->iRef++;` |
|        9 | 12966 | `				pVm->pPendingException = pThis;` |
|        9 | 12967 | `				return SXRET_OK;` |
|        - | 12968 | `			}` |
|      255 | 12969 | `		}` |
|        - | 12970 | `		/* Truly uncaught */` |
|      511 | 12971 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      511 | 12972 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 12973 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 12974 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 12975 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 12976 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 12977 | `			}` |
|      ! 0 | 12978 | `		}` |
|      511 | 12979 | `		return rc;` |
|      ! 0 | 12980 | `	}else{` |
|      270 | 12981 | `		VmFrame *pFrame = pVm->pFrame;` |
|      270 | 12982 | `		ph7_exception **apSaved = 0;` |
|        - | 12983 | `		sxu32 nSavedCount;` |
|        - | 12984 | `		sxi32 rc;` |
|      270 | 12985 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      270 | 12986 | `		if( pException->pFrame == pFrame ){` |
|      220 | 12987 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      109 | 12988 | `		}` |
|        - | 12989 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 12990 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 12991 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 12992 | `		 */` |
|      270 | 12993 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      270 | 12994 | `		if( nSavedCount > 0 ){` |
|       16 | 12995 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 12996 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 12997 | `			if( apSaved ){` |
|       16 | 12998 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 12999 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13000 | `				SySetReset(&pVm->aException);` |
|        5 | 13001 | `			}` |
|        5 | 13002 | `		}` |
|        - | 13003 | `		/* Create a private frame first */` |
|      270 | 13004 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      270 | 13005 | `		if( rc == SXRET_OK ){` |
|      270 | 13006 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      270 | 13007 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      270 | 13008 | `			if( pObj ){` |
|      270 | 13009 | `				pThis->iRef++;` |
|      270 | 13010 | `				pObj->x.pOther = pThis;` |
|      270 | 13011 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      134 | 13012 | `			}` |
|        - | 13013 | `			/* Execute the catch block */` |
|      270 | 13014 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13015 | `			/* Leave the frame */` |
|      270 | 13016 | `			VmLeaveFrame(&(*pVm));` |
|      134 | 13017 | `		}` |
|        - | 13018 | `		/* Restore the outer exception handlers */` |
|      270 | 13019 | `		if( apSaved ){` |
|        - | 13020 | `			sxu32 k;` |
|        - | 13021 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13022 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13023 | `			 * Restore the original outer entries.` |
|        - | 13024 | `			 */` |
|       11 | 13025 | `			SySetReset(&pVm->aException);` |
|       21 | 13026 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13027 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13028 | `			}` |
|       11 | 13029 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13030 | `		}` |
|        - | 13031 | `		/* Execute the finally block after catch */` |
|      270 | 13032 | `		if( pException->iHasFinally ){` |
|       16 | 13033 | `			pException->iFinallyDone = 1;` |
|        - | 13034 | `			{` |
|       16 | 13035 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13036 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13037 | `					return SXERR_ABORT;` |
|        - | 13038 | `				}` |
|        - | 13039 | `			}` |
|        7 | 13040 | `		}` |
|      270 | 13041 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13042 | `			return SXERR_ABORT;` |
|        - | 13043 | `		}` |
|        - | 13044 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13045 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13046 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13047 | `		 */` |
|      270 | 13048 | `		if( pVm->pPendingException ){` |
|        9 | 13049 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13050 | `			pVm->pPendingException = 0;` |
|        9 | 13051 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13052 | `		}` |
|        - | 13053 | `	}` |
|        - | 13054 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13055 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13056 | `	 */` |
|      262 | 13057 | `	return SXRET_OK;` |
|      396 | 13058 |  |
|        - | 13059 | `/*` |
|        - | 13060 | ` * Section:` |
|        - | 13061 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13062 | ` * Status:` |
|        - | 13063 | ` *    Stable.` |
|        - | 13064 | ` */` |
|        - | 13065 | `/*` |
|        - | 13066 | ` * string ph7version(void)` |
|        - | 13067 | ` *  Returns the running version of the PH7 version.` |
|        - | 13068 | ` * Parameters` |
|        - | 13069 | ` *  None` |
|        - | 13070 | ` * Return` |
|        - | 13071 | ` * Current PH7 version.` |
|        - | 13072 | ` */` |
|        2 | 13073 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13074 |  |
|        1 | 13075 | `	SXUNUSED(nArg);` |
|        1 | 13076 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13077 | `	/* Current engine version */` |
|        3 | 13078 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13079 | `	return PH7_OK;` |
|        1 | 13080 |  |
|        - | 13081 | `/*` |
|        - | 13082 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13083 | ` */` |
|        - | 13084 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13085 | ` "<html><head>"\` |
|        - | 13086 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13087 | ` "<style type=\"text/css\">"\` |
|        - | 13088 | ` "div {"\` |
|        - | 13089 | `     "border: 1px solid #cccccc;"\` |
|        - | 13090 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13091 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13092 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13093 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13094 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13095 | `     "-o-border-radius: 10px;"\` |
|        - | 13096 | `     "border-radius: 10px;"\` |
|        - | 13097 | `     "padding-left: 2em;"\` |
|        - | 13098 | `     "background-color: white;"\` |
|        - | 13099 | `     "margin-left: auto;"\` |
|        - | 13100 | `     "font-family: verdana;"\` |
|        - | 13101 | `     "padding-right: 2em;"\` |
|        - | 13102 | `     "margin-right: auto;"\` |
|        - | 13103 | `     "}"\` |
|        - | 13104 | `     "body {"\` |
|        - | 13105 | `     "padding: 0.2em;"\` |
|        - | 13106 | `     "font-style: normal;"\` |
|        - | 13107 | `     "font-size: medium;"\` |
|        - | 13108 | `     "background-color: #f2f2f2;"\` |
|        - | 13109 | `     "}"\` |
|        - | 13110 | `     "hr {"\` |
|        - | 13111 | `     "border-style: solid none none;"\` |
|        - | 13112 | `     "border-width: 1px medium medium;"\` |
|        - | 13113 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13114 | `     "height: 1px;"\` |
|        - | 13115 | `     "}"\` |
|        - | 13116 | `     "a {"\` |
|        - | 13117 | `     "color: #3366cc;"\` |
|        - | 13118 | `     "text-decoration: none;"\` |
|        - | 13119 | `     "}"\` |
|        - | 13120 | `     "a:hover {"\` |
|        - | 13121 | `     "color: #999999;"\` |
|        - | 13122 | `     "}"\` |
|        - | 13123 | `     "a:active {"\` |
|        - | 13124 | `     "color: #663399;"\` |
|        - | 13125 | `     "}"\` |
|        - | 13126 | `     "h1 {"\` |
|        - | 13127 | `     "margin: 0;"\` |
|        - | 13128 | `     "padding: 0;"\` |
|        - | 13129 | `     "font-family: Verdana;"\` |
|        - | 13130 | `     "font-weight: bold;"\` |
|        - | 13131 | `     "font-style: normal;"\` |
|        - | 13132 | `     "font-size: medium;"\` |
|        - | 13133 | `     "text-transform: capitalize;"\` |
|        - | 13134 | `     "color: #0a328c;"\` |
|        - | 13135 | `     "}"\` |
|        - | 13136 | `     "p {"\` |
|        - | 13137 | `     "margin: 0 auto;"\` |
|        - | 13138 | `     "font-size: medium;"\` |
|        - | 13139 | `     "font-style: normal;"\` |
|        - | 13140 | `     "font-family: verdana;"\` |
|        - | 13141 | `     "}"\` |
|        - | 13142 | `"</style></head><body>"\` |
|        - | 13143 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13144 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13145 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13146 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13147 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13148 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13149 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13150 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13151 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13152 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13153 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13154 |  |
|        - | 13155 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13156 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13157 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13158 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13159 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13160 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13161 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13162 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13163 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13164 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13165 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13166 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13167 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13168 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13169 |  |
|        - | 13170 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13171 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13172 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13173 | `"&nbsp;*<br>"\` |
|        - | 13174 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13175 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13176 | `"&nbsp;* are met:<br>"\` |
|        - | 13177 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13178 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13179 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13180 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13181 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13182 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13183 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13184 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13185 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13186 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13187 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13188 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13189 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13190 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13191 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13192 | `"&nbsp;*<br>"\` |
|        - | 13193 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13194 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13195 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13196 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13197 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13198 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13199 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13200 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13201 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13202 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13203 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13204 | `"&nbsp;*/<br>"\` |
|        - | 13205 | `"</span></small></small></p>"\` |
|        - | 13206 | `"</div></body></html>"` |
|        - | 13207 | `/*` |
|        - | 13208 | ` * bool ph7credits(void)` |
|        - | 13209 | ` * bool ph7info(void)` |
|        - | 13210 | ` * bool ph7copyright(void)` |
|        - | 13211 | ` *  Prints out the credits for PH7 engine` |
|        - | 13212 | ` * Parameters` |
|        - | 13213 | ` *  None` |
|        - | 13214 | ` * Return` |
|        - | 13215 | ` *  Always TRUE` |
|        - | 13216 | ` */` |
|        2 | 13217 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13218 |  |
|        3 | 13219 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13220 | `	/* Expand the HTML page above*/` |
|        3 | 13221 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13222 | `	ph7_context_output_format(` |
|        1 | 13223 | `		pCtx,` |
|        - | 13224 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13225 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13226 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13227 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13228 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13229 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13230 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13231 | `#ifdef __WINNT__` |
|        - | 13232 | `		"Windows NT"` |
|        - | 13233 | `#elif defined(__UNIXES__)` |
|        - | 13234 | `		"UNIX-Like"` |
|        - | 13235 | `#else` |
|        - | 13236 | `		"Other OS"` |
|        - | 13237 | `#endif` |
|        - | 13238 | `		);` |
|        3 | 13239 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13240 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13241 | `	SXUNUSED(apArg);` |
|        - | 13242 | `	/* Return TRUE */` |
|        - | 13243 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13244 | `	return PH7_OK;` |
|        1 | 13245 |  |
|        - | 13246 | `/*` |
|        - | 13247 | ` * Section:` |
|        - | 13248 | ` *    URL related routines.` |
|        - | 13249 | ` * Status:` |
|        - | 13250 | ` *    Stable.` |
|        - | 13251 | ` */` |
|        - | 13252 | `/*` |
|        - | 13253 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13254 | ` *  Parse a URL and return its fields.` |
|        - | 13255 | ` * Parameters` |
|        - | 13256 | ` *  $url` |
|        - | 13257 | ` *   The URL to parse.` |
|        - | 13258 | ` * $component` |
|        - | 13259 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13260 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13261 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13262 | ` *  in which case the return value will be an integer).` |
|        - | 13263 | ` * Return` |
|        - | 13264 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13265 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13266 | ` *  this array are:` |
|        - | 13267 | ` *   scheme - e.g. http` |
|        - | 13268 | ` *   host` |
|        - | 13269 | ` *   port` |
|        - | 13270 | ` *   user` |
|        - | 13271 | ` *   pass` |
|        - | 13272 | ` *   path` |
|        - | 13273 | ` *   query - after the question mark ?` |
|        - | 13274 | ` *   fragment - after the hashmark #` |
|        - | 13275 | ` * Note:` |
|        - | 13276 | ` *  FALSE is returned on failure.` |
|        - | 13277 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13278 | ` *  with the standard PHP engine.` |
|        - | 13279 | ` */` |
|       28 | 13280 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13281 |  |
|        - | 13282 | `	const char *zStr; /* Input string */` |
|        - | 13283 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13284 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13285 | `	int nLen;` |
|        - | 13286 | `	sxi32 rc;` |
|       29 | 13287 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13288 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13289 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13290 | `		return PH7_OK;` |
|        - | 13291 | `	}` |
|        - | 13292 | `	/* Extract the given URI */` |
|       29 | 13293 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13294 | `	if( nLen < 1 ){` |
|        - | 13295 | `		/* Nothing to process,return FALSE */` |
|        3 | 13296 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13297 | `		return PH7_OK;` |
|        - | 13298 | `	}` |
|        - | 13299 | `	/* Get a parse */` |
|       27 | 13300 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13301 | `	if( rc != SXRET_OK ){` |
|        - | 13302 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13303 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13304 | `		return PH7_OK;` |
|        - | 13305 | `	}` |
|       27 | 13306 | `	if( nArg > 1 ){` |
|      ! 0 | 13307 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13308 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13309 | `		switch(nComponent){` |
|      ! 0 | 13310 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13311 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13312 | `			if( pComp->nByte < 1 ){` |
|        - | 13313 | `				/* No available value,return NULL */` |
|      ! 0 | 13314 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13315 | `			}else{` |
|      ! 0 | 13316 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13317 | `			}` |
|      ! 0 | 13318 | `			break;` |
|      ! 0 | 13319 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13320 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13321 | `			if( pComp->nByte < 1 ){` |
|        - | 13322 | `				/* No available value,return NULL */` |
|      ! 0 | 13323 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13324 | `			}else{` |
|      ! 0 | 13325 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13326 | `			}` |
|      ! 0 | 13327 | `			break;` |
|      ! 0 | 13328 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13329 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13330 | `			if( pComp->nByte < 1 ){` |
|        - | 13331 | `				/* No available value,return NULL */` |
|      ! 0 | 13332 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13333 | `			}else{` |
|      ! 0 | 13334 | `				int iPort = 0;` |
|        - | 13335 | `				/* Cast the value to integer */` |
|      ! 0 | 13336 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13337 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13338 | `			}` |
|      ! 0 | 13339 | `			break;` |
|      ! 0 | 13340 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13341 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13342 | `			if( pComp->nByte < 1 ){` |
|        - | 13343 | `				/* No available value,return NULL */` |
|      ! 0 | 13344 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13345 | `			}else{` |
|      ! 0 | 13346 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13347 | `			}` |
|      ! 0 | 13348 | `			break;` |
|      ! 0 | 13349 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13350 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13351 | `			if( pComp->nByte < 1 ){` |
|        - | 13352 | `				/* No available value,return NULL */` |
|      ! 0 | 13353 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13354 | `			}else{` |
|      ! 0 | 13355 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13356 | `			}` |
|      ! 0 | 13357 | `			break;` |
|      ! 0 | 13358 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13359 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13360 | `			if( pComp->nByte < 1 ){` |
|        - | 13361 | `				/* No available value,return NULL */` |
|      ! 0 | 13362 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13363 | `			}else{` |
|      ! 0 | 13364 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13365 | `			}` |
|      ! 0 | 13366 | `			break;` |
|      ! 0 | 13367 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13368 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13369 | `			if( pComp->nByte < 1 ){` |
|        - | 13370 | `				/* No available value,return NULL */` |
|      ! 0 | 13371 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13372 | `			}else{` |
|      ! 0 | 13373 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13374 | `			}` |
|      ! 0 | 13375 | `			break;` |
|      ! 0 | 13376 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13377 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13378 | `			if( pComp->nByte < 1 ){` |
|        - | 13379 | `				/* No available value,return NULL */` |
|      ! 0 | 13380 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13381 | `			}else{` |
|      ! 0 | 13382 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13383 | `			}` |
|      ! 0 | 13384 | `			break;` |
|      ! 0 | 13385 | `		default:` |
|        - | 13386 | `			/* No such entry,return NULL */` |
|      ! 0 | 13387 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13388 | `			break;` |
|        - | 13389 | `		}` |
|      ! 0 | 13390 | `	}else{` |
|        - | 13391 | `		ph7_value *pArray,*pValue;` |
|        - | 13392 | `		/* Return an associative array */` |
|       27 | 13393 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13394 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13395 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13396 | `			/* Out of memory */` |
|      ! 0 | 13397 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13398 | `			/* Return false */` |
|      ! 0 | 13399 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13400 | `			return PH7_OK;` |
|        - | 13401 | `		}` |
|        - | 13402 | `		/* Fill the array */` |
|       27 | 13403 | `		pComp = &sURI.sScheme;` |
|       27 | 13404 | `		if( pComp->nByte > 0 ){` |
|       19 | 13405 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13406 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13407 | `		}` |
|        - | 13408 | `		/* Reset the string cursor */` |
|       27 | 13409 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13410 | `		pComp = &sURI.sHost;` |
|       27 | 13411 | `		if( pComp->nByte > 0 ){` |
|       25 | 13412 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13413 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13414 | `		}` |
|        - | 13415 | `		/* Reset the string cursor */` |
|       27 | 13416 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13417 | `		pComp = &sURI.sPort;` |
|       27 | 13418 | `		if( pComp->nByte > 0 ){` |
|       11 | 13419 | `			int iPort = 0;/* cc warning */` |
|        - | 13420 | `			/* Convert to integer */` |
|       11 | 13421 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13422 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13423 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13424 | `		}` |
|        - | 13425 | `		/* Reset the string cursor */` |
|       27 | 13426 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13427 | `		pComp = &sURI.sUser;` |
|       27 | 13428 | `		if( pComp->nByte > 0 ){` |
|        7 | 13429 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13430 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13431 | `		}` |
|        - | 13432 | `		/* Reset the string cursor */` |
|       27 | 13433 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13434 | `		pComp = &sURI.sPass;` |
|       27 | 13435 | `		if( pComp->nByte > 0 ){` |
|        7 | 13436 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13437 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13438 | `		}` |
|        - | 13439 | `		/* Reset the string cursor */` |
|       27 | 13440 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13441 | `		pComp = &sURI.sPath;` |
|       27 | 13442 | `		if( pComp->nByte > 0 ){` |
|       17 | 13443 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13444 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13445 | `		}` |
|        - | 13446 | `		/* Reset the string cursor */` |
|       27 | 13447 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13448 | `		pComp = &sURI.sQuery;` |
|       27 | 13449 | `		if( pComp->nByte > 0 ){` |
|        5 | 13450 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13451 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13452 | `		}` |
|        - | 13453 | `		/* Reset the string cursor */` |
|       27 | 13454 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13455 | `		pComp = &sURI.sFragment;` |
|       27 | 13456 | `		if( pComp->nByte > 0 ){` |
|        5 | 13457 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13458 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13459 | `		}` |
|        - | 13460 | `		/* Return the created array */` |
|       27 | 13461 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13462 | `		/* NOTE:` |
|        - | 13463 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13464 | `		 * automatically as soon we return from this function.` |
|        - | 13465 | `		 */` |
|        - | 13466 | `	}` |
|        - | 13467 | `	/* All done */` |
|       27 | 13468 | `	return PH7_OK;` |
|       15 | 13469 |  |
|        - | 13470 | `/*` |
|        - | 13471 | ` * Section:` |
|        - | 13472 | ` *   Array related routines.` |
|        - | 13473 | ` * Status:` |
|        - | 13474 | ` *    Stable.` |
|        - | 13475 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13476 | ` *  Array related functions that need access to the underlying` |
|        - | 13477 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13478 | ` */` |
|        - | 13479 | `/*` |
|        - | 13480 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13481 | ` * of the following structure.` |
|        - | 13482 | ` */` |
|        - | 13483 | `struct compact_data` |
|        - | 13484 |  |
|        - | 13485 | `	ph7_value *pArray;  /* Target array */` |
|        - | 13486 | `	int nRecCount;      /* Recursion count */` |
|        - | 13487 | `};` |
|        - | 13488 | `/*` |
|        - | 13489 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 13490 | ` */` |
|      ! 0 | 13491 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 13492 |  |
|      ! 0 | 13493 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 13494 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 13495 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 13496 | `	/* Act according to the hashmap value */` |
|      ! 0 | 13497 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 13498 | `		SyString sVar;` |
|      ! 0 | 13499 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 13500 | `		if( sVar.nByte > 0 ){` |
|        - | 13501 | `			/* Query the current frame */` |
|      ! 0 | 13502 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 13503 | `			/* ^` |
|        - | 13504 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 13505 | `			 */` |
|      ! 0 | 13506 | `			if( pKey ){` |
|        - | 13507 | `				/* Perform the insertion */` |
|      ! 0 | 13508 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 13509 | `			}` |
|      ! 0 | 13510 | `		}` |
|      ! 0 | 13511 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 13512 | `		int rc;` |
|        - | 13513 | `		/* Recursively traverse this array */` |
|      ! 0 | 13514 | `		pData->nRecCount++;` |
|      ! 0 | 13515 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 13516 | `		pData->nRecCount--;` |
|      ! 0 | 13517 | `		return rc;` |
|        - | 13518 | `	}` |
|      ! 0 | 13519 | `	return SXRET_OK;` |
|      ! 0 | 13520 |  |
|        - | 13521 | `/*` |
|        - | 13522 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 13523 | ` *  Create array containing variables and their values.` |
|        - | 13524 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 13525 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 13526 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 13527 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 13528 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 13529 | ` * Parameters` |
|        - | 13530 | ` *  $varname` |
|        - | 13531 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 13532 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 13533 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 13534 | ` *   it recursively.` |
|        - | 13535 | ` * Return` |
|        - | 13536 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 13537 | ` */` |
|        2 | 13538 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13539 |  |
|        - | 13540 | `	ph7_value *pArray,*pObj;` |
|        3 | 13541 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13542 | `	const char *zName;` |
|        - | 13543 | `	SyString sVar;` |
|        - | 13544 | `	int i,nLen;` |
|        3 | 13545 | `	if( nArg < 1 ){` |
|        - | 13546 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 13547 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13548 | `		return PH7_OK;` |
|        - | 13549 | `	}` |
|        - | 13550 | `	/* Create the array */` |
|        3 | 13551 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 13552 | `	if( pArray == 0 ){` |
|        - | 13553 | `		/* Out of memory */` |
|      ! 0 | 13554 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13555 | `		/* Return NULL */` |
|      ! 0 | 13556 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13557 | `		return PH7_OK;` |
|        - | 13558 | `	}` |
|        - | 13559 | `	/* Perform the requested operation */` |
|        7 | 13560 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 13561 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 13562 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 13563 | `				struct compact_data sData;` |
|      ! 0 | 13564 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 13565 | `				/* Recursively walk the array */` |
|      ! 0 | 13566 | `				sData.nRecCount = 0;` |
|      ! 0 | 13567 | `				sData.pArray = pArray;` |
|      ! 0 | 13568 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 13569 | `			}` |
|      ! 0 | 13570 | `		}else{` |
|        - | 13571 | `			/* Extract variable name */` |
|        5 | 13572 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 13573 | `			if( nLen > 0 ){` |
|        5 | 13574 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 13575 | `				/* Check if the variable is available in the current frame */` |
|        5 | 13576 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 13577 | `				if( pObj ){` |
|        5 | 13578 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 13579 | `				}` |
|        2 | 13580 | `			}` |
|        - | 13581 | `		}` |
|        3 | 13582 | `	}` |
|        - | 13583 | `	/* Return the array */` |
|        3 | 13584 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 13585 | `	return PH7_OK;` |
|        2 | 13586 |  |
|        - | 13587 | `/*` |
|        - | 13588 | ` * The [extract()] function store it's state information in an instance` |
|        - | 13589 | ` * of the following structure.` |
|        - | 13590 | ` */` |
|        - | 13591 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 13592 | `struct extract_aux_data` |
|        - | 13593 |  |
|        - | 13594 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 13595 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 13596 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 13597 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 13598 | `	int iFlags;           /* Control flags */` |
|        - | 13599 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 13600 | `};` |
|        - | 13601 | `/* Forward declaration */` |
|        - | 13602 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 13603 | `/*` |
|        - | 13604 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 13605 | ` *   Import variables into the current symbol table from an array.` |
|        - | 13606 | ` * Parameters` |
|        - | 13607 | ` * $var_array` |
|        - | 13608 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 13609 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 13610 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 13611 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 13612 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 13613 | ` * $extract_type` |
|        - | 13614 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 13615 | ` *  It can be one of the following values:` |
|        - | 13616 | ` *   EXTR_OVERWRITE` |
|        - | 13617 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 13618 | ` *   EXTR_SKIP` |
|        - | 13619 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 13620 | ` *   EXTR_PREFIX_SAME` |
|        - | 13621 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 13622 | ` *   EXTR_PREFIX_ALL` |
|        - | 13623 | ` *       Prefix all variable names with prefix.` |
|        - | 13624 | ` *   EXTR_PREFIX_INVALID` |
|        - | 13625 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 13626 | ` *   EXTR_IF_EXISTS` |
|        - | 13627 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 13628 | ` *       otherwise do nothing.` |
|        - | 13629 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 13630 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 13631 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 13632 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 13633 | ` *      the current symbol table.` |
|        - | 13634 | ` * $prefix` |
|        - | 13635 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 13636 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 13637 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 13638 | ` *  underscore character.` |
|        - | 13639 | ` * Return` |
|        - | 13640 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 13641 | ` */` |
|        4 | 13642 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13643 |  |
|        - | 13644 | `	extract_aux_data sAux;` |
|        - | 13645 | `	ph7_hashmap *pMap;` |
|        5 | 13646 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 13647 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 13648 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13649 | `		return PH7_OK;` |
|        - | 13650 | `	}` |
|        - | 13651 | `	/* Point to the target hashmap */` |
|        5 | 13652 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 13653 | `	if( pMap->nEntry < 1 ){` |
|        - | 13654 | `		/* Empty map,return  0 */` |
|      ! 0 | 13655 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 13656 | `		return PH7_OK;` |
|        - | 13657 | `	}` |
|        - | 13658 | `	/* Prepare the aux data */` |
|        5 | 13659 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 13660 | `	if( nArg > 1 ){` |
|        3 | 13661 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 13662 | `		if( nArg > 2 ){` |
|      ! 0 | 13663 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 13664 | `		}` |
|        1 | 13665 | `	}` |
|        5 | 13666 | `	sAux.pVm = pCtx->pVm;` |
|        - | 13667 | `	/* Invoke the worker callback */` |
|        5 | 13668 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 13669 | `	/* Number of variables successfully imported */` |
|        5 | 13670 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 13671 | `	return PH7_OK;` |
|        3 | 13672 |  |
|        - | 13673 | `/*` |
|        - | 13674 | ` * Worker callback for the [extract()] function defined` |
|        - | 13675 | ` * below.` |
|        - | 13676 | ` */` |
|        8 | 13677 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13678 |  |
|        9 | 13679 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 13680 | `	int iFlags = pAux->iFlags;` |
|        9 | 13681 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13682 | `	ph7_value *pObj;` |
|        - | 13683 | `	SyString sVar;` |
|        9 | 13684 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 13685 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 13686 | `	}` |
|        - | 13687 | `	/* Perform a string cast */` |
|        9 | 13688 | `	PH7_MemObjToString(pKey);` |
|        9 | 13689 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13690 | `		/* Unavailable variable name */` |
|      ! 0 | 13691 | `		return SXRET_OK;` |
|        - | 13692 | `	}` |
|        9 | 13693 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 13694 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 13695 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13696 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13697 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13698 | `			);` |
|      ! 0 | 13699 | `	}else{` |
|       13 | 13700 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 13701 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13702 | `	}` |
|        9 | 13703 | `	sVar.zString = pAux->zWorker;` |
|        - | 13704 | `	/* Try to extract the variable */` |
|        9 | 13705 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 13706 | `	if( pObj ){` |
|        - | 13707 | `		/* Collision */` |
|        5 | 13708 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 13709 | `			return SXRET_OK;` |
|        - | 13710 | `		}` |
|        5 | 13711 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 13712 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 13713 | `				/* Already prefixed */` |
|      ! 0 | 13714 | `				return SXRET_OK;` |
|        - | 13715 | `			}` |
|      ! 0 | 13716 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 13717 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 13718 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13719 | `				);` |
|      ! 0 | 13720 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 13721 | `		}` |
|        3 | 13722 | `	}else{` |
|        - | 13723 | `		/* Create the variable */` |
|        5 | 13724 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 13725 | `	}` |
|        9 | 13726 | `	if( pObj ){` |
|        - | 13727 | `		/* Overwrite the old value */` |
|        9 | 13728 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 13729 | `		/* Increment counter */` |
|        9 | 13730 | `		pAux->iCount++;` |
|        4 | 13731 | `	}` |
|        9 | 13732 | `	return SXRET_OK;` |
|        5 | 13733 |  |
|        - | 13734 | `/*` |
|        - | 13735 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 13736 | ` * defined below.` |
|        - | 13737 | ` */` |
|        2 | 13738 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 13739 |  |
|        3 | 13740 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 13741 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 13742 | `	ph7_value *pObj;` |
|        - | 13743 | `	SyString sVar;` |
|        - | 13744 | `	/* Perform a string cast */` |
|        3 | 13745 | `	PH7_MemObjToString(pKey);` |
|        3 | 13746 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 13747 | `		/* Unavailable variable name */` |
|      ! 0 | 13748 | `		return SXRET_OK;` |
|        - | 13749 | `	}` |
|        3 | 13750 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 13751 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 13752 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 13753 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 13754 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 13755 | `			);` |
|        2 | 13756 | `	}else{` |
|      ! 0 | 13757 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 13758 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 13759 | `	}` |
|        3 | 13760 | `	sVar.zString = pAux->zWorker;` |
|        - | 13761 | `	/* Extract the variable */` |
|        3 | 13762 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 13763 | `	if( pObj ){` |
|        3 | 13764 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 13765 | `	}` |
|        3 | 13766 | `	return SXRET_OK;` |
|        2 | 13767 |  |
|        - | 13768 | `/*` |
|        - | 13769 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 13770 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 13771 | ` * Parameters` |
|        - | 13772 | ` * $types` |
|        - | 13773 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 13774 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 13775 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 13776 | ` *  POST includes the POST uploaded file information.` |
|        - | 13777 | ` *  Note:` |
|        - | 13778 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 13779 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 13780 | ` * $prefix` |
|        - | 13781 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 13782 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 13783 | ` *  variable named $pref_userid.` |
|        - | 13784 | ` * Return` |
|        - | 13785 | ` *  TRUE on success or FALSE on failure.` |
|        - | 13786 | ` */` |
|        2 | 13787 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13788 |  |
|        - | 13789 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 13790 | `	extract_aux_data sAux;` |
|        - | 13791 | `	int nLen,nPrefixLen;` |
|        - | 13792 | `	ph7_value *pSuper;` |
|        - | 13793 | `	ph7_vm *pVm;` |
|        - | 13794 | `	/* By default import only $_GET variables  */` |
|        3 | 13795 | `	zImport = "G";` |
|        3 | 13796 | `	nLen = (int)sizeof(char);` |
|        3 | 13797 | `	zPrefix = 0;` |
|        3 | 13798 | `	nPrefixLen = 0;` |
|        3 | 13799 | `	if( nArg > 0 ){` |
|        3 | 13800 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 13801 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 13802 | `		}` |
|        3 | 13803 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 13804 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 13805 | `		}` |
|        1 | 13806 | `	}` |
|        - | 13807 | `	/* Point to the underlying VM */` |
|        3 | 13808 | `	pVm = pCtx->pVm;` |
|        - | 13809 | `	/* Initialize the aux data */` |
|        3 | 13810 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 13811 | `	sAux.zPrefix = zPrefix;` |
|        3 | 13812 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 13813 | `	sAux.pVm = pVm;` |
|        - | 13814 | `	/* Extract */` |
|        3 | 13815 | `	zEnd = &zImport[nLen];` |
|        5 | 13816 | `	while( zImport < zEnd ){` |
|        3 | 13817 | `		int c = zImport[0];` |
|        3 | 13818 | `		pSuper = 0;` |
|        3 | 13819 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 13820 | `			/* Import $_GET variables */` |
|        3 | 13821 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 13822 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 13823 | `			/* Import $_POST variables */` |
|      ! 0 | 13824 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 13825 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 13826 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 13827 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 13828 | `		}` |
|        3 | 13829 | `		if( pSuper ){` |
|        - | 13830 | `			/* Iterate throw array entries */` |
|        3 | 13831 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 13832 | `		}` |
|        - | 13833 | `		/* Advance the cursor */` |
|        3 | 13834 | `		zImport++;` |
|        1 | 13835 | `	}` |
|        - | 13836 | `	/* All done,return TRUE*/` |
|        3 | 13837 | `	ph7_result_bool(pCtx,0);` |
|        3 | 13838 | `	return PH7_OK;` |
|        1 | 13839 |  |
|        - | 13840 | `/*` |
|        - | 13841 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 13842 | ` * Refer to the eval() language construct implementation for more` |
|        - | 13843 | ` * information.` |
|        - | 13844 | ` */` |
|    12284 | 13845 | `static sxi32 VmEvalChunk(` |
|        - | 13846 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 13847 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 13848 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 13849 | `	int iFlags,         /* Compile flag */` |
|        - | 13850 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 13851 | `	)` |
|        2 | 13852 |  |
|        - | 13853 | `	SySet *pByteCode,aByteCode;` |
|        - | 13854 | `	SyBlob sSavedNs;` |
|    12286 | 13855 | `	ProcConsumer xErr = 0;` |
|    12286 | 13856 | `	void *pErrData = 0;` |
|        - | 13857 | `	/* Initialize bytecode container */` |
|    12286 | 13858 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12286 | 13859 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 13860 | `	/* Reset the code generator */` |
|    12286 | 13861 | `	if( bTrueReturn ){` |
|        - | 13862 | `		/* Included file,log compile-time errors */` |
|     9274 | 13863 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9274 | 13864 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4636 | 13865 | `	}` |
|    12286 | 13866 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 13867 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 13868 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 13869 | `	 * the caller's namespace is restored. */` |
|    12286 | 13870 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12286 | 13871 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12286 | 13872 | `	if( bTrueReturn ){` |
|        - | 13873 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9274 | 13874 | `		SyBlobReset(&pVm->sNamespace);` |
|     4636 | 13875 | `	}` |
|        - | 13876 | `	/* Swap bytecode container */` |
|    12286 | 13877 | `	pByteCode = pVm->pByteContainer;` |
|    12286 | 13878 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 13879 | `	/* Compile the chunk */` |
|    12286 | 13880 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18428 | 13881 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 13882 | `		/* Compilation error,return false */` |
|        3 | 13883 | `		if( pCtx ){` |
|        3 | 13884 | `			ph7_result_bool(pCtx,0);` |
|        1 | 13885 | `		}` |
|        2 | 13886 | `	}else{` |
|        - | 13887 | `		/* Mount any newly defined classes */` |
|        - | 13888 | `		SyHashEntry *pEntry;` |
|        - | 13889 | `		ph7_class *pClass;` |
|        - | 13890 | `		ph7_value sResult; /* Return value */` |
|        - | 13891 | `		sxi32 rc;` |
|    12284 | 13892 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   630423 | 13893 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   612000 | 13894 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 13895 | `			/* Only mount classes that haven't been mounted yet */` |
|   612000 | 13896 | `			if( !pClass->bMounted ){` |
|   111350 | 13897 | `				rc = VmMountUserClass(pVm,pClass);` |
|   111350 | 13898 | `				if( rc != SXRET_OK ){` |
|        - | 13899 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 13900 | `					if( pCtx ){` |
|      ! 0 | 13901 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 13902 | `					}` |
|      ! 0 | 13903 | `					goto Cleanup;` |
|        - | 13904 | `				}` |
|    55674 | 13905 | `			}` |
|        2 | 13906 | `		}` |
|    12284 | 13907 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 13908 | `			/* Out of memory */` |
|      ! 0 | 13909 | `			if( pCtx ){` |
|      ! 0 | 13910 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 13911 | `			}` |
|      ! 0 | 13912 | `			goto Cleanup;` |
|        - | 13913 | `		}` |
|    12284 | 13914 | `		if( bTrueReturn ){` |
|        - | 13915 | `			/* Assume a boolean true return value */` |
|     9274 | 13916 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4638 | 13917 | `		}else{` |
|        - | 13918 | `			/* Assume a null return value */` |
|     3012 | 13919 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 13920 | `		}` |
|        - | 13921 | `		/* Execute the compiled chunk */` |
|    12284 | 13922 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12284 | 13923 | `		if( pCtx ){` |
|        - | 13924 | `			/* Set the execution result */` |
|     9292 | 13925 | `			ph7_result_value(pCtx,&sResult);` |
|     4645 | 13926 | `		}` |
|    12284 | 13927 | `		PH7_MemObjRelease(&sResult);` |
|        - | 13928 | `	}` |
|     6142 | 13929 | `Cleanup:` |
|        - | 13930 | `	/* Cleanup the mess left behind */` |
|    12286 | 13931 | `	pVm->pByteContainer = pByteCode;` |
|    12286 | 13932 | `	SySetRelease(&aByteCode);` |
|        - | 13933 | `	/* Restore caller's namespace state */` |
|    12286 | 13934 | `	SyBlobReset(&pVm->sNamespace);` |
|    12286 | 13935 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12286 | 13936 | `	SyBlobRelease(&sSavedNs);` |
|    12286 | 13937 | `	return SXRET_OK;` |
|        2 | 13938 |  |
|        - | 13939 | `/*` |
|        - | 13940 | ` * value eval(string $code)` |
|        - | 13941 | ` *   Evaluate a string as PHP code.` |
|        - | 13942 | ` * Parameter` |
|        - | 13943 | ` *  code: PHP code to evaluate.` |
|        - | 13944 | ` * Return` |
|        - | 13945 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 13946 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 13947 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 13948 | ` */` |
|       22 | 13949 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13950 |  |
|        - | 13951 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 13952 | `	if( nArg < 1 ){` |
|        - | 13953 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 13954 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13955 | `		return SXRET_OK;` |
|        - | 13956 | `	}` |
|        - | 13957 | `	/* Chunk to evaluate */` |
|       24 | 13958 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 13959 | `	if( sChunk.nByte < 1 ){` |
|        - | 13960 | `		/* Empty string,return NULL */` |
|        3 | 13961 | `		ph7_result_null(pCtx);` |
|        3 | 13962 | `		return SXRET_OK;` |
|        - | 13963 | `	}` |
|        - | 13964 | `	/* Eval the chunk */` |
|       22 | 13965 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 13966 | `	return SXRET_OK;` |
|       13 | 13967 |  |
|        - | 13968 | `/*` |
|        - | 13969 | ` * Check if a file path is already included.` |
|        - | 13970 | ` */` |
|    18540 | 13971 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 13972 |  |
|        - | 13973 | `	SyString *aEntries;` |
|        - | 13974 | `	sxu32 n;` |
|    18542 | 13975 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 13976 | `	/* Perform a linear search */` |
| 85878306 | 13977 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 85859772 | 13978 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 13979 | `			/* Already included */` |
|        7 | 13980 | `			return TRUE;` |
|        - | 13981 | `		}` |
| 42929884 | 13982 | `	}` |
|    18536 | 13983 | `	return FALSE;` |
|     9272 | 13984 |  |
|        - | 13985 | `/*` |
|        - | 13986 | ` * Push a file path in the appropriate VM container.` |
|        - | 13987 | ` */` |
|    21524 | 13988 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 13989 |  |
|        - | 13990 | `	SyString sPath;` |
|        - | 13991 | `	char *zDup;` |
|        - | 13992 | `#ifdef __WINNT__` |
|        - | 13993 | `	char *zCur;` |
|        - | 13994 | `#endif` |
|        - | 13995 | `	sxi32 rc;` |
|    21526 | 13996 | `	if( nLen < 0 ){` |
|     2986 | 13997 | `		nLen = SyStrlen(zPath);` |
|     1492 | 13998 | `	}` |
|        - | 13999 | `	/* Duplicate the file path first */` |
|    21526 | 14000 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    21526 | 14001 | `	if( zDup == 0 ){` |
|      ! 0 | 14002 | `		return SXERR_MEM;` |
|        - | 14003 | `	}` |
|        - | 14004 | `#ifdef __WINNT__` |
|        - | 14005 | `	/* Normalize path on windows` |
|        - | 14006 | `	 * Example:` |
|        - | 14007 | `	 *    Path/To/File.php` |
|        - | 14008 | `	 * becomes` |
|        - | 14009 | `	 *   path\to\file.php` |
|        - | 14010 | `	 */` |
|        2 | 14011 | `	zCur = zDup;` |
|        2 | 14012 | `	while( zCur[0] != 0 ){` |
|        2 | 14013 | `		if( zCur[0] == '/' ){` |
|        2 | 14014 | `			zCur[0] = '\\';` |
|        2 | 14015 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14016 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14017 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14018 | `		}` |
|        2 | 14019 | `		zCur++;` |
|        2 | 14020 | `	}` |
|        - | 14021 | `#endif` |
|        - | 14022 | `	/* Install the file path */` |
|    21526 | 14023 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    21526 | 14024 | `	if( !bMain ){` |
|    18542 | 14025 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14026 | `			/* Already included */` |
|        7 | 14027 | `			*pNew = 0;` |
|        4 | 14028 | `		}else{` |
|        - | 14029 | `			/* Insert in the corresponding container */` |
|    18536 | 14030 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18536 | 14031 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14032 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14033 | `				return rc;` |
|        - | 14034 | `			}` |
|    18536 | 14035 | `			*pNew = 1;` |
|        - | 14036 | `		}` |
|     9270 | 14037 | `	}` |
|    21526 | 14038 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    21526 | 14039 | `	return SXRET_OK;` |
|    10764 | 14040 |  |
|        - | 14041 | `/*` |
|        - | 14042 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14043 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14044 | ` * indicates failure.` |
|        - | 14045 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14046 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14047 | ` * operations.` |
|        - | 14048 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14049 | ` * this function is a no-op.` |
|        - | 14050 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14051 | ` * constructs for more information.` |
|        - | 14052 | ` */` |
|     9282 | 14053 | `static sxi32 VmExecIncludedFile(` |
|        - | 14054 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14055 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14056 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14057 | `	 )` |
|        2 | 14058 |  |
|        - | 14059 | `	sxi32 rc;` |
|        - | 14060 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14061 | `	const ph7_io_stream *pStream;` |
|        - | 14062 | `	SyBlob sContents;` |
|        - | 14063 | `	void *pHandle;` |
|        - | 14064 | `	ph7_vm *pVm;` |
|        - | 14065 | `	int isNew;` |
|        - | 14066 | `	/* Initialize fields */` |
|     9284 | 14067 | `	pVm = pCtx->pVm;` |
|     9284 | 14068 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9284 | 14069 | `	isNew = 0;` |
|        - | 14070 | `	/* Extract the associated stream */` |
|     9284 | 14071 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14072 | `	/*` |
|        - | 14073 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14074 | `	 * in a read-only mode.` |
|        - | 14075 | `	 */` |
|     9284 | 14076 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9284 | 14077 | `	if( pHandle == 0 ){` |
|        8 | 14078 | `		return SXERR_IO;` |
|        - | 14079 | `	}` |
|     9278 | 14080 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9278 | 14081 | `	if( IncludeOnce && !isNew ){` |
|        - | 14082 | `		/* Already included */` |
|        5 | 14083 | `		rc = SXERR_EXISTS;` |
|        3 | 14084 | `	}else{` |
|        - | 14085 | `		/* Read the whole file contents */` |
|     9274 | 14086 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9274 | 14087 | `		if( rc == SXRET_OK ){` |
|        - | 14088 | `			SyString sScript;` |
|        - | 14089 | `			/* Compile and execute the script */` |
|     9274 | 14090 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9274 | 14091 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4636 | 14092 | `		}` |
|        - | 14093 | `	}` |
|        - | 14094 | `	/* Pop from the set of included file */` |
|     9278 | 14095 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14096 | `	/* Close the handle */` |
|     9278 | 14097 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14098 | `	/* Release the working buffer */` |
|     9278 | 14099 | `	SyBlobRelease(&sContents);` |
|        - | 14100 | `#else` |
|        - | 14101 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14102 | `	SXUNUSED(pPath);` |
|        - | 14103 | `	SXUNUSED(IncludeOnce);` |
|        - | 14104 | `	rc = SXERR_IO;` |
|        - | 14105 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9278 | 14106 | `	return rc;` |
|     4643 | 14107 |  |
|        - | 14108 | `/*` |
|        - | 14109 | ` * string get_include_path(void)` |
|        - | 14110 | ` *  Gets the current include_path configuration option.` |
|        - | 14111 | ` * Parameter` |
|        - | 14112 | ` *  None` |
|        - | 14113 | ` * Return` |
|        - | 14114 | ` *  Included paths as a string` |
|        - | 14115 | ` */` |
|        2 | 14116 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14117 |  |
|        3 | 14118 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14119 | `	SyString *aEntry;` |
|        - | 14120 | `	int dir_sep;` |
|        - | 14121 | `	sxu32 n;` |
|        - | 14122 | `#ifdef __WINNT__` |
|        1 | 14123 | `	dir_sep = ';';` |
|        - | 14124 | `#else` |
|        - | 14125 | `	/* Assume UNIX path separator */` |
|        2 | 14126 | `	dir_sep = ':';` |
|        - | 14127 | `#endif` |
|        1 | 14128 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14129 | `	SXUNUSED(apArg);` |
|        - | 14130 | `	/* Point to the list of import paths */` |
|        3 | 14131 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14132 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14133 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14134 | `		if( n > 0 ){` |
|        - | 14135 | `			/* Append dir seprator */` |
|      ! 0 | 14136 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14137 | `		}` |
|        - | 14138 | `		/* Append path */` |
|        3 | 14139 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14140 | `	}` |
|        3 | 14141 | `	return PH7_OK;` |
|        1 | 14142 |  |
|        - | 14143 | `/*` |
|        - | 14144 | ` * string get_get_included_files(void)` |
|        - | 14145 | ` *  Gets the current include_path configuration option.` |
|        - | 14146 | ` * Parameter` |
|        - | 14147 | ` *  None` |
|        - | 14148 | ` * Return` |
|        - | 14149 | ` *  Included paths as a string` |
|        - | 14150 | ` */` |
|        2 | 14151 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14152 |  |
|        3 | 14153 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14154 | `	ph7_value *pArray,*pWorker;` |
|        - | 14155 | `	SyString *pEntry;` |
|        - | 14156 | `	int c,d;` |
|        - | 14157 | `	/* Create an array and a working value */` |
|        3 | 14158 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14159 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14160 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14161 | `		/* Out of memory,return null */` |
|      ! 0 | 14162 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14163 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14164 | `		SXUNUSED(apArg);` |
|      ! 0 | 14165 | `		return PH7_OK;` |
|        - | 14166 | `	}` |
|        3 | 14167 | `	c = d = '/';` |
|        - | 14168 | `#ifdef __WINNT__` |
|        1 | 14169 | `	d = '\\';` |
|        - | 14170 | `#endif` |
|        - | 14171 | `	/* Iterate throw entries */` |
|        3 | 14172 | `	SySetResetCursor(pFiles);` |
|     3839 | 14173 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14174 | `		const char *zBase,*zEnd;` |
|        - | 14175 | `		int iLen;` |
|        - | 14176 | `		/* reset the string cursor */` |
|     3837 | 14177 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14178 | `		/* Extract base name */` |
|     3837 | 14179 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14180 | `		/* Ignore trailing '/' */` |
|     5755 | 14181 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14182 | `			zEnd--;` |
|      ! 0 | 14183 | `		}` |
|     3837 | 14184 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 14185 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 14186 | `			zEnd--;` |
|        1 | 14187 | `		}` |
|     3837 | 14188 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 14189 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14190 | `		/* Copy entry name */` |
|     3837 | 14191 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14192 | `		/* Perform the insertion */` |
|     3837 | 14193 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14194 | `	}` |
|        - | 14195 | `	/* All done,return the created array */` |
|        3 | 14196 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14197 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14198 | `	 * by the engine as soon we return from this foreign` |
|        - | 14199 | `	 * function.` |
|        - | 14200 | `	 */` |
|        3 | 14201 | `	return PH7_OK;` |
|        2 | 14202 |  |
|        - | 14203 | `/*` |
|        - | 14204 | ` * include:` |
|        - | 14205 | ` * According to the PHP reference manual.` |
|        - | 14206 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14207 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14208 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14209 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14210 | ` *  and the current working directory before failing. The include()` |
|        - | 14211 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14212 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14213 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14214 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14215 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14216 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14217 | ` *  directory to find the requested file.` |
|        - | 14218 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14219 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14220 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14221 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14222 | ` */` |
|     9264 | 14223 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14224 |  |
|        - | 14225 | `	SyString sFile;` |
|        - | 14226 | `	sxi32 rc;` |
|     9266 | 14227 | `	if( nArg < 1 ){` |
|        - | 14228 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14229 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14230 | `		return SXRET_OK;` |
|        - | 14231 | `	}` |
|        - | 14232 | `	/* File to include */` |
|     9266 | 14233 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9266 | 14234 | `	if( sFile.nByte < 1 ){` |
|        - | 14235 | `		/* Empty string,return NULL */` |
|      ! 0 | 14236 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14237 | `		return SXRET_OK;` |
|        - | 14238 | `	}` |
|        - | 14239 | `	/* Open,compile and execute the desired script */` |
|     9266 | 14240 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9266 | 14241 | `	if( rc != SXRET_OK ){` |
|        - | 14242 | `		/* Emit a warning and return false */` |
|        3 | 14243 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14244 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14245 | `	}` |
|     9266 | 14246 | `	return SXRET_OK;` |
|     4634 | 14247 |  |
|        - | 14248 | `/*` |
|        - | 14249 | ` * include_once:` |
|        - | 14250 | ` *  According to the PHP reference manual.` |
|        - | 14251 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14252 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14253 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14254 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14255 | ` *   just once.` |
|        - | 14256 | ` */` |
|        4 | 14257 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14258 |  |
|        - | 14259 | `	SyString sFile;` |
|        - | 14260 | `	sxi32 rc;` |
|        5 | 14261 | `	if( nArg < 1 ){` |
|        - | 14262 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14263 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14264 | `		return SXRET_OK;` |
|        - | 14265 | `	}` |
|        - | 14266 | `	/* File to include */` |
|        5 | 14267 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14268 | `	if( sFile.nByte < 1 ){` |
|        - | 14269 | `		/* Empty string,return NULL */` |
|      ! 0 | 14270 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14271 | `		return SXRET_OK;` |
|        - | 14272 | `	}` |
|        - | 14273 | `	/* Open,compile and execute the desired script */` |
|        5 | 14274 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14275 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14276 | `		/* File already included,return TRUE */` |
|        3 | 14277 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14278 | `		return SXRET_OK;` |
|        - | 14279 | `	}` |
|        3 | 14280 | `	if( rc != SXRET_OK ){` |
|        - | 14281 | `		/* Emit a warning and return false */` |
|      ! 0 | 14282 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14283 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14284 | ` 	}` |
|        3 | 14285 | `	return SXRET_OK;` |
|        3 | 14286 |  |
|        - | 14287 | `/*` |
|        - | 14288 | ` * require.` |
|        - | 14289 | ` *  According to the PHP reference manual.` |
|        - | 14290 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14291 | ` *   also produce a fatal level error.` |
|        - | 14292 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14293 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14294 | ` */` |
|        6 | 14295 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14296 |  |
|        - | 14297 | `	SyString sFile;` |
|        - | 14298 | `	sxi32 rc;` |
|        8 | 14299 | `	if( nArg < 1 ){` |
|        - | 14300 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14301 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14302 | `		return SXRET_OK;` |
|        - | 14303 | `	}` |
|        - | 14304 | `	/* File to include */` |
|        8 | 14305 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14306 | `	if( sFile.nByte < 1 ){` |
|        - | 14307 | `		/* Empty string,return NULL */` |
|      ! 0 | 14308 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14309 | `		return SXRET_OK;` |
|        - | 14310 | `	}` |
|        - | 14311 | `	/* Open,compile and execute the desired script */` |
|        8 | 14312 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14313 | `	if( rc != SXRET_OK ){` |
|        - | 14314 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14315 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14316 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14317 | `		return PH7_ABORT;` |
|        - | 14318 | `	}` |
|        8 | 14319 | `	return SXRET_OK;` |
|        5 | 14320 |  |
|        - | 14321 | `/*` |
|        - | 14322 | ` * require_once:` |
|        - | 14323 | ` *  According to the PHP reference manual.` |
|        - | 14324 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14325 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14326 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14327 | ` *   and how it differs from its non _once siblings.` |
|        - | 14328 | ` */` |
|        4 | 14329 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14330 |  |
|        - | 14331 | `	SyString sFile;` |
|        - | 14332 | `	sxi32 rc;` |
|        5 | 14333 | `	if( nArg < 1 ){` |
|        - | 14334 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14335 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14336 | `		return SXRET_OK;` |
|        - | 14337 | `	}` |
|        - | 14338 | `	/* File to include */` |
|        5 | 14339 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14340 | `	if( sFile.nByte < 1 ){` |
|        - | 14341 | `		/* Empty string,return NULL */` |
|      ! 0 | 14342 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14343 | `		return SXRET_OK;` |
|        - | 14344 | `	}` |
|        - | 14345 | `	/* Open,compile and execute the desired script */` |
|        5 | 14346 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14347 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14348 | `		/* File already included,return TRUE */` |
|        3 | 14349 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14350 | `		return SXRET_OK;` |
|        - | 14351 | `	}` |
|        3 | 14352 | `	if( rc != SXRET_OK ){` |
|        - | 14353 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14354 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14355 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14356 | `		return PH7_ABORT;` |
|        - | 14357 | `	}` |
|        3 | 14358 | `	return SXRET_OK;` |
|        3 | 14359 |  |
|        - | 14360 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 14361 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 14362 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 14363 | `/*` |
|        - | 14364 | ` * Section:` |
|        - | 14365 | ` *  SPL Autoloading functions.` |
|        - | 14366 | ` * Status:` |
|        - | 14367 | ` *  Stable.` |
|        - | 14368 | ` */` |
|        - | 14369 | `/*` |
|        - | 14370 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14371 | ` *  Register given function as __autoload() implementation.` |
|        - | 14372 | ` * Parameters` |
|        - | 14373 | ` *  callback` |
|        - | 14374 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14375 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14376 | ` *  throw` |
|        - | 14377 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14378 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14379 | ` *  prepend` |
|        - | 14380 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14381 | ` *   autoload stack instead of appending it.` |
|        - | 14382 | ` * Return` |
|        - | 14383 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14384 | ` */` |
|       34 | 14385 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14386 |  |
|        - | 14387 | `	VmAutoloadCB sEntry;` |
|       36 | 14388 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14389 | `	int iPrepend = 0;` |
|        - | 14390 | `	sxu32 n;` |
|       36 | 14391 | `	if( nArg < 1 ){` |
|        - | 14392 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14393 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14394 | `		/* Check for duplicates first */` |
|        9 | 14395 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14396 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14397 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14398 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14399 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14400 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14401 | `				return SXRET_OK;` |
|        - | 14402 | `			}` |
|      ! 0 | 14403 | `		}` |
|        5 | 14404 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14405 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14406 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14407 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14408 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14409 | `		return SXRET_OK;` |
|        - | 14410 | `	}` |
|        - | 14411 | `	/* Validate that the callback is callable */` |
|       28 | 14412 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14413 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14414 | `		if( nArg >= 2 ){` |
|      ! 0 | 14415 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14416 | `		}` |
|      ! 0 | 14417 | `		if( iThrow ){` |
|      ! 0 | 14418 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14419 | `				"Argument is not callable");` |
|      ! 0 | 14420 | `		}` |
|      ! 0 | 14421 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14422 | `		return SXRET_OK;` |
|        - | 14423 | `	}` |
|        - | 14424 | `	/* Check for duplicates */` |
|       46 | 14425 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14426 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14427 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14428 | `			/* Already registered */` |
|      ! 0 | 14429 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14430 | `			return SXRET_OK;` |
|        - | 14431 | `		}` |
|       11 | 14432 | `	}` |
|        - | 14433 | `	/* Check prepend flag */` |
|       28 | 14434 | `	if( nArg >= 3 ){` |
|        3 | 14435 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14436 | `	}` |
|        - | 14437 | `	/* Store the callback */` |
|       28 | 14438 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14439 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14440 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14441 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14442 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14443 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14444 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14445 | `		VmAutoloadCB *aBase;` |
|        3 | 14446 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14447 | `		/* Rotate: move last entry to front */` |
|        3 | 14448 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14449 | `		if( aBase ){` |
|        - | 14450 | `			VmAutoloadCB sTemp;` |
|        - | 14451 | `			sxu32 i;` |
|        3 | 14452 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14453 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14454 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14455 | `			}` |
|        3 | 14456 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14457 | `		}` |
|        2 | 14458 | `	}else{` |
|       26 | 14459 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14460 | `	}` |
|       28 | 14461 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14462 | `	return SXRET_OK;` |
|       19 | 14463 |  |
|        - | 14464 | `/*` |
|        - | 14465 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14466 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14467 | ` * Parameters` |
|        - | 14468 | ` *  callback` |
|        - | 14469 | ` *   The autoload function being unregistered.` |
|        - | 14470 | ` * Return` |
|        - | 14471 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14472 | ` */` |
|       32 | 14473 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14474 |  |
|       34 | 14475 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14476 | `	sxu32 n,nEntry;` |
|       34 | 14477 | `	if( nArg < 1 ){` |
|      ! 0 | 14478 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14479 | `		return SXRET_OK;` |
|        - | 14480 | `	}` |
|       34 | 14481 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14482 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14483 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14484 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14485 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 14486 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 14487 | `			sxu32 i;` |
|       32 | 14488 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 14489 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 14490 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 14491 | `			}` |
|        - | 14492 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 14493 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 14494 | `			ph7_result_bool(pCtx,1);` |
|       32 | 14495 | `			return SXRET_OK;` |
|        - | 14496 | `		}` |
|        3 | 14497 | `	}` |
|        3 | 14498 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14499 | `	return SXRET_OK;` |
|       18 | 14500 |  |
|        - | 14501 | `/*` |
|        - | 14502 | ` * array spl_autoload_functions(void)` |
|        - | 14503 | ` *  Return all registered __autoload() functions.` |
|        - | 14504 | ` * Return` |
|        - | 14505 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 14506 | ` *  an empty array is returned.` |
|        - | 14507 | ` */` |
|       20 | 14508 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14509 |  |
|       21 | 14510 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14511 | `	ph7_value *pArray;` |
|        - | 14512 | `	sxu32 n,nEntry;` |
|       10 | 14513 | `	SXUNUSED(nArg);` |
|       10 | 14514 | `	SXUNUSED(apArg);` |
|       21 | 14515 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 14516 | `	if( pArray == 0 ){` |
|      ! 0 | 14517 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14518 | `		return SXRET_OK;` |
|        - | 14519 | `	}` |
|       21 | 14520 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 14521 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 14522 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 14523 | `		if( pEntry ){` |
|       15 | 14524 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 14525 | `		}` |
|        8 | 14526 | `	}` |
|       21 | 14527 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 14528 | `	return SXRET_OK;` |
|       11 | 14529 |  |
|        - | 14530 | `/*` |
|        - | 14531 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 14532 | ` *  Default implementation of __autoload().` |
|        - | 14533 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 14534 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 14535 | ` * Parameters` |
|        - | 14536 | ` *  class` |
|        - | 14537 | ` *   The class name being searched.` |
|        - | 14538 | ` *  file_extensions` |
|        - | 14539 | ` *   Comma-separated list of file extensions to try.` |
|        - | 14540 | ` */` |
|        2 | 14541 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14542 |  |
|        - | 14543 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 14544 | `	SyBlob sPath;` |
|        - | 14545 | `	int nClass;` |
|        - | 14546 | `	sxi32 rc;` |
|        3 | 14547 | `	if( nArg < 1 ){` |
|      ! 0 | 14548 | `		return SXRET_OK;` |
|        - | 14549 | `	}` |
|        3 | 14550 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 14551 | `	if( nClass < 1 ){` |
|      ! 0 | 14552 | `		return SXRET_OK;` |
|        - | 14553 | `	}` |
|        - | 14554 | `	/* Default extensions */` |
|        3 | 14555 | `	zExt = ".php,.inc";` |
|        3 | 14556 | `	if( nArg >= 2 ){` |
|        - | 14557 | `		int nExt;` |
|      ! 0 | 14558 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 14559 | `		if( nExt < 1 ){` |
|      ! 0 | 14560 | `			zExt = ".php,.inc";` |
|      ! 0 | 14561 | `		}` |
|      ! 0 | 14562 | `	}` |
|        3 | 14563 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 14564 | `	/* Iterate over comma-separated extensions */` |
|        3 | 14565 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 14566 | `	zCur = zExt;` |
|        7 | 14567 | `	while( zCur < zEnd ){` |
|        - | 14568 | `		const char *zComma;` |
|        - | 14569 | `		SyString sFile;` |
|        - | 14570 | `		int i;` |
|        - | 14571 | `		/* Find next comma or end */` |
|        5 | 14572 | `		zComma = zCur;` |
|       21 | 14573 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 14574 | `			zComma++;` |
|        1 | 14575 | `		}` |
|        - | 14576 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 14577 | `		SyBlobReset(&sPath);` |
|       69 | 14578 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 14579 | `			char c = zClass[i];` |
|       65 | 14580 | `			if( c == '\\' ){` |
|      ! 0 | 14581 | `				c = '/';` |
|       65 | 14582 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 14583 | `				c = c + ('a' - 'A');` |
|        6 | 14584 | `			}` |
|       65 | 14585 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 14586 | `		}` |
|        - | 14587 | `		/* Append extension */` |
|        5 | 14588 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 14589 | `		/* Try to include the file */` |
|        5 | 14590 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 14591 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 14592 | `		if( rc == SXRET_OK ){` |
|        - | 14593 | `			/* File included successfully */` |
|      ! 0 | 14594 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 14595 | `			return SXRET_OK;` |
|        - | 14596 | `		}` |
|        - | 14597 | `		/* Move past the comma */` |
|        5 | 14598 | `		zCur = zComma;` |
|        5 | 14599 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 14600 | `			zCur++;` |
|        1 | 14601 | `		}` |
|        1 | 14602 | `	}` |
|        3 | 14603 | `	SyBlobRelease(&sPath);` |
|        3 | 14604 | `	return SXRET_OK;` |
|        2 | 14605 |  |
|        - | 14606 | `/* Table of built-in VM functions. */` |
|        - | 14607 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 14608 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 14609 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 14610 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 14611 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 14612 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 14613 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 14614 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 14615 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 14616 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 14617 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 14618 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 14619 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 14620 | `	    /* Constants management */` |
|        - | 14621 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 14622 | `	{ "define",   vm_builtin_define               },` |
|        - | 14623 | `	{ "constant", vm_builtin_constant             },` |
|        - | 14624 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 14625 | `	   /* Class/Object functions */` |
|        - | 14626 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 14627 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 14628 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 14629 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 14630 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 14631 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 14632 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 14633 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 14634 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 14635 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 14636 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 14637 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 14638 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 14639 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 14640 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 14641 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 14642 | `	   /* SPL Autoloading */` |
|        - | 14643 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 14644 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 14645 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 14646 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 14647 | `	   /* Random numbers/strings generators */` |
|        - | 14648 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 14649 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 14650 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 14651 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 14652 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 14653 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 14654 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 14655 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14656 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 14657 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 14658 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 14659 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14660 | `	   /* Language constructs functions */` |
|        - | 14661 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 14662 | `	{ "print", vm_builtin_print                   },` |
|        - | 14663 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 14664 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 14665 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 14666 | `	  /* Variable handling functions */` |
|        - | 14667 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 14668 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 14669 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 14670 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 14671 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 14672 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 14673 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 14674 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 14675 | `	  /* Ouput control functions */` |
|        - | 14676 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 14677 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 14678 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 14679 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 14680 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 14681 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 14682 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 14683 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 14684 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 14685 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 14686 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 14687 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 14688 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 14689 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 14690 | `	  /* Assertion functions */` |
|        - | 14691 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 14692 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 14693 | `	  /* Error reporting functions */` |
|        - | 14694 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 14695 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 14696 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 14697 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 14698 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 14699 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 14700 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 14701 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 14702 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 14703 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 14704 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 14705 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 14706 | `	  /* Release info */` |
|        - | 14707 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 14708 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 14709 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 14710 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 14711 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 14712 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 14713 | `	  /* hashmap */` |
|        - | 14714 | `	{"compact",          vm_builtin_compact       },` |
|        - | 14715 | `	{"extract",          vm_builtin_extract       },` |
|        - | 14716 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 14717 | `	  /* URL related function */` |
|        - | 14718 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 14719 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 14720 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14721 | `	   /* XML processing functions */` |
|        - | 14722 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 14723 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 14724 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 14725 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 14726 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 14727 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 14728 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 14729 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 14730 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 14731 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 14732 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 14733 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 14734 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 14735 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 14736 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 14737 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 14738 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 14739 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 14740 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 14741 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 14742 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 14743 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 14744 | `	   /* UTF-8 encoding/decoding */` |
|        - | 14745 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 14746 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 14747 | `	   /* Command line processing */` |
|        - | 14748 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 14749 | `	   /* JSON encoding/decoding */` |
|        - | 14750 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 14751 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 14752 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 14753 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 14754 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 14755 | `	   /* Files/URI inclusion facility */` |
|        - | 14756 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 14757 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 14758 | `	{ "include",      vm_builtin_include          },` |
|        - | 14759 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 14760 | `	{ "require",      vm_builtin_require          },` |
|        - | 14761 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 14762 | `};` |
|        - | 14763 | `/*` |
|        - | 14764 | ` * Register the built-in VM functions defined above.` |
|        - | 14765 | ` */` |
|     2678 | 14766 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 14767 |  |
|        - | 14768 | `	sxi32 rc;` |
|        - | 14769 | `	sxu32 n;` |
|   350820 | 14770 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 14771 | `		/* Note that these special functions have access` |
|        - | 14772 | `		 * to the underlying virtual machine as their` |
|        - | 14773 | `		 * private data.` |
|        - | 14774 | `		 */` |
|   348142 | 14775 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   348142 | 14776 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 14777 | `			return rc;` |
|        - | 14778 | `		}` |
|   174072 | 14779 | `	}` |
|     2680 | 14780 | `	return SXRET_OK;` |
|     1341 | 14781 |  |
|        - | 14782 | `/*` |
|        - | 14783 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 14784 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 14785 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 14786 | ` */` |
|    41730 | 14787 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 14788 |  |
|    41732 | 14789 | `	if( !iLoadable ){` |
|    39866 | 14790 | `		return pClass;` |
|        - | 14791 | `	}` |
|     1872 | 14792 | `	while(pClass){` |
|     1868 | 14793 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1864 | 14794 | `			return pClass;` |
|        - | 14795 | `		}` |
|        5 | 14796 | `		pClass = pClass->pNextName;` |
|        1 | 14797 | `	}` |
|        5 | 14798 | `	return 0;` |
|    20867 | 14799 |  |
|        - | 14800 | `/*` |
|        - | 14801 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 14802 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 14803 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 14804 | ` * registered in the VM's class table.` |
|        - | 14805 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 14806 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 14807 | ` */` |
|       38 | 14808 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14809 |  |
|        - | 14810 | `	VmAutoloadCB *pEntry;` |
|        - | 14811 | `	ph7_value sArg,sResult;` |
|        - | 14812 | `	SyHashEntry *pHashEntry;` |
|        - | 14813 | `	ph7_class *pClass;` |
|        - | 14814 | `	sxu32 n,nEntry;` |
|       40 | 14815 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 14816 | `	if( nEntry < 1 ){` |
|       26 | 14817 | `		return 0;` |
|        - | 14818 | `	}` |
|        - | 14819 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 14820 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 14821 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 14822 | `	}` |
|        - | 14823 | `	/* Mark this class as being autoloaded */` |
|       14 | 14824 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 14825 | `	/* Prepare the class name argument */` |
|       14 | 14826 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 14827 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 14828 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 14829 | `	pClass = 0;` |
|       28 | 14830 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 14831 | `		ph7_value *apArg[1];` |
|       24 | 14832 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 14833 | `		if( pEntry == 0 ){` |
|      ! 0 | 14834 | `			continue;` |
|        - | 14835 | `		}` |
|       24 | 14836 | `		apArg[0] = &sArg;` |
|       24 | 14837 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 14838 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 14839 | `			continue;` |
|        - | 14840 | `		}` |
|        - | 14841 | `		/* Check if the class is now available */` |
|       24 | 14842 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 14843 | `		if( pHashEntry ){` |
|       10 | 14844 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 14845 | `			if( pClass ){` |
|       10 | 14846 | `				break;` |
|        - | 14847 | `			}` |
|      ! 0 | 14848 | `		}` |
|        9 | 14849 | `	}` |
|       14 | 14850 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 14851 | `	PH7_MemObjRelease(&sResult);` |
|        - | 14852 | `	/* Remove reentrancy guard */` |
|       14 | 14853 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 14854 | `	return pClass;` |
|       21 | 14855 |  |
|        - | 14856 | `/*` |
|        - | 14857 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 14858 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 14859 | ` */` |
|       18 | 14860 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 14861 |  |
|       20 | 14862 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 14863 |  |
|        - | 14864 | `/*` |
|        - | 14865 | ` * Check if the given name refer to an installed class.` |
|        - | 14866 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 14867 | ` */` |
|    41742 | 14868 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 14869 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 14870 | `	const char *zName,  /* Name of the target class */` |
|        - | 14871 | `	sxu32 nByte,        /* zName length */` |
|        - | 14872 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 14873 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 14874 | `						 */` |
|        - | 14875 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 14876 | `	)` |
|        2 | 14877 |  |
|        - | 14878 | `	SyHashEntry *pEntry;` |
|        - | 14879 | `	ph7_class *pClass;` |
|    20871 | 14880 | `	SXUNUSED(iNest);` |
|        - | 14881 | `	/* Exact class lookup.` |
|        - | 14882 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 14883 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    41744 | 14884 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    41744 | 14885 | `	if( pEntry == 0 ){` |
|        - | 14886 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 14887 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 14888 | `	}` |
|    41724 | 14889 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    41724 | 14890 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    20873 | 14891 |  |
|        - | 14892 | `/*` |
|        - | 14893 | ` * Reference Table Implementation` |
|        - | 14894 | ` * Status: stable <chm@symisc.net>` |
|        - | 14895 | ` * Intro` |
|        - | 14896 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 14897 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 14898 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 14899 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 14900 | ` *  Refer to the official for more information on this powerful` |
|        - | 14901 | ` *  extension.` |
|        - | 14902 | ` */` |
|        - | 14903 | `/*` |
|        - | 14904 | ` * Allocate a new reference entry.` |
|        - | 14905 | ` */` |
|  3159022 | 14906 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 14907 |  |
|        - | 14908 | `	VmRefObj *pRef;` |
|        - | 14909 | `	/* Allocate a new instance */` |
|  3159024 | 14910 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3159024 | 14911 | `	if( pRef == 0 ){` |
|      ! 0 | 14912 | `		return 0;` |
|        - | 14913 | `	}` |
|        - | 14914 | `	/* Zero the structure */` |
|  3159024 | 14915 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 14916 | `	/* Initialize fields */` |
|  3159024 | 14917 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3159024 | 14918 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3159024 | 14919 | `	pRef->nIdx = nIdx;` |
|  3159024 | 14920 | `	return pRef;` |
|  1579513 | 14921 |  |
|        - | 14922 | `/*` |
|        - | 14923 | ` * Default hash function used by the reference table` |
|        - | 14924 | ` * for lookup/insertion operations.` |
|        - | 14925 | ` */` |
| 17348898 | 14926 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 14927 |  |
|        - | 14928 | `	/* Calculate the hash based on the memory object index */` |
| 17348900 | 14929 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 14930 |  |
|        - | 14931 | `/*` |
|        - | 14932 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 14933 | ` * in the reference table.` |
|        - | 14934 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 14935 | ` * otherwise.` |
|        - | 14936 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14937 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14938 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14939 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14940 | ` * Refer to the official for more information on this powerful` |
|        - | 14941 | ` * extension.` |
|        - | 14942 | ` */` |
|  9420764 | 14943 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 14944 |  |
|        - | 14945 | `	VmRefObj *pRef;` |
|        - | 14946 | `	sxu32 nBucket;` |
|        - | 14947 | `	/* Point to the appropriate bucket */` |
|  9420766 | 14948 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 14949 | `	/* Perform the lookup */` |
|  9420766 | 14950 | `	pRef = pVm->apRefObj[nBucket];` |
| 20576954 | 14951 | `	for(;;){` |
| 41145642 | 14952 | `		if( pRef == 0 ){` |
|  3259912 | 14953 | `			break;` |
|        - | 14954 | `		}` |
| 37885732 | 14955 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 14956 | `			/* Entry found */` |
|  6160856 | 14957 | `			return pRef;` |
|        - | 14958 | `		}` |
|        - | 14959 | `		/* Point to the next entry */` |
| 31724878 | 14960 | `		pRef = pRef->pNextCollide;` |
|        2 | 14961 | `	}` |
|        - | 14962 | `	/* No such entry,return NULL */` |
|  3259912 | 14963 | `	return 0;` |
|  4710384 | 14964 |  |
|        - | 14965 | `/*` |
|        - | 14966 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 14967 | ` *` |
|        - | 14968 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 14969 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 14970 | ` * the reference implementation is consistent,solid and it's` |
|        - | 14971 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 14972 | ` * Refer to the official for more information on this powerful` |
|        - | 14973 | ` * extension.` |
|        - | 14974 | ` */` |
|  3159022 | 14975 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 14976 |  |
|        - | 14977 | `	sxu32 nBucket;` |
|  3159024 | 14978 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 14979 | `		VmRefObj **apNew;` |
|        - | 14980 | `		sxu32 nNew;` |
|        - | 14981 | `		/* Allocate a larger table */` |
|     4572 | 14982 | `		nNew = pVm->nRefSize << 1;` |
|     4572 | 14983 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4572 | 14984 | `		if( apNew ){` |
|     4572 | 14985 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 14986 | `			sxu32 n;` |
|        - | 14987 | `			/* Zero the structure */` |
|     4572 | 14988 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 14989 | `			/* Rehash all referenced entries */` |
|  2846954 | 14990 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 14991 | `				/* Remove old collision links */` |
|  2842384 | 14992 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 14993 | `				/* Point to the appropriate bucket */` |
|  2842384 | 14994 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 14995 | `				/* Insert the entry  */` |
|  2842384 | 14996 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2842384 | 14997 | `				if( apNew[nBucket] ){` |
|  2298896 | 14998 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1149447 | 14999 | `				}` |
|  2842384 | 15000 | `				apNew[nBucket] = pEntry;` |
|        - | 15001 | `				/* Point to the next entry */` |
|  2842384 | 15002 | `				pEntry = pEntry->pNext;` |
|  1421193 | 15003 | `			}` |
|        - | 15004 | `			/* Release the old table */` |
|     4572 | 15005 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15006 | `			/* Install the new one */` |
|     4572 | 15007 | `			pVm->apRefObj = apNew;` |
|     4572 | 15008 | `			pVm->nRefSize = nNew;` |
|     2285 | 15009 | `		}` |
|     2285 | 15010 | `	}` |
|        - | 15011 | `	/* Point to the appropriate bucket */` |
|  3159024 | 15012 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15013 | `	/* Insert the entry */` |
|  3159024 | 15014 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3159024 | 15015 | `	if( pVm->apRefObj[nBucket] ){` |
|  2582835 | 15016 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1291428 | 15017 | `	}` |
|  3159024 | 15018 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3159024 | 15019 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3159024 | 15020 | `	pVm->nRefUsed++;` |
|  3159024 | 15021 | `	return SXRET_OK;` |
|        2 | 15022 |  |
|        - | 15023 | `/*` |
|        - | 15024 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15025 | ` * the reference table.` |
|        - | 15026 | ` * This function is invoked when the user perform an unset` |
|        - | 15027 | ` * call [i.e: unset($var); ].` |
|        - | 15028 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15029 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15030 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15031 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15032 | ` * Refer to the official for more information on this powerful` |
|        - | 15033 | ` * extension.` |
|        - | 15034 | ` */` |
|  3119946 | 15035 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15036 |  |
|        - | 15037 | `	ph7_hashmap_node **apNode;` |
|        - | 15038 | `	SyHashEntry **apEntry;` |
|        - | 15039 | `	sxu32 n;` |
|        - | 15040 | `	/* Point to the reference table */` |
|  3119948 | 15041 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3119948 | 15042 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15043 | `	/* Unlink the entry from the reference table */` |
|  3227356 | 15044 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   107410 | 15045 | `		if( apEntry[n] ){` |
|   107360 | 15046 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    53679 | 15047 | `		}` |
|    53706 | 15048 | `	}` |
|  6133728 | 15049 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3013782 | 15050 | `		if( apNode[n] ){` |
|     7448 | 15051 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3723 | 15052 | `		}` |
|  1506892 | 15053 | `	}` |
|  3119948 | 15054 | `	if( pRef->pPrevCollide ){` |
|  1193218 | 15055 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   596700 | 15056 | `	}else{` |
|  1926732 | 15057 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15058 | `	}` |
|  3119948 | 15059 | `	if( pRef->pNextCollide ){` |
|  1768514 | 15060 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   884181 | 15061 | `	}` |
|  3119948 | 15062 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15063 | `	/* Release the node */` |
|  3119948 | 15064 | `	SySetRelease(&pRef->aReference);` |
|  3119948 | 15065 | `	SySetRelease(&pRef->aArrEntries);` |
|  3119948 | 15066 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3119948 | 15067 | `	pVm->nRefUsed--;` |
|  3119948 | 15068 | `	return SXRET_OK;` |
|        2 | 15069 |  |
|        - | 15070 | `/*` |
|        - | 15071 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15072 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15073 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15074 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15075 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15076 | ` * Refer to the official for more information on this powerful` |
|        - | 15077 | ` * extension.` |
|        - | 15078 | ` */` |
|  3193530 | 15079 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15080 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15081 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15082 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15083 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15084 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15085 | `	)` |
|        2 | 15086 |  |
|  3193532 | 15087 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15088 | `	VmRefObj *pRef;` |
|        - | 15089 | `	/* Check if the referenced object already exists */` |
|  3193532 | 15090 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3193532 | 15091 | `	if( pRef == 0 ){` |
|        - | 15092 | `		/* Create a new entry */` |
|  3159024 | 15093 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3159024 | 15094 | `		if( pRef == 0 ){` |
|      ! 0 | 15095 | `			return SXERR_MEM;` |
|        - | 15096 | `		}` |
|  3159024 | 15097 | `		pRef->iFlags = iFlags;` |
|        - | 15098 | `		/* Install the entry */` |
|  3159024 | 15099 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1579511 | 15100 | `	}` |
|  3193532 | 15101 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3193532 | 15102 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15103 | `		VmSlot sRef;` |
|        - | 15104 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15105 | `		 * be deleted when we leave this frame.` |
|        - | 15106 | `		 */` |
|   100986 | 15107 | `		sRef.nIdx = nIdx;` |
|   100986 | 15108 | `		sRef.pUserData = pEntry;` |
|   100986 | 15109 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15110 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15111 | `		}` |
|    50492 | 15112 | `	}` |
|  3193532 | 15113 | `	if( pEntry ){` |
|        - | 15114 | `		/* Address of the hash-entry */` |
|   135294 | 15115 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    67646 | 15116 | `	}` |
|  3193532 | 15117 | `	if( pMapEntry ){` |
|        - | 15118 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3050538 | 15119 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1525268 | 15120 | `	}` |
|  3193532 | 15121 | `	return SXRET_OK;` |
|  1596767 | 15122 |  |
|        - | 15123 | `/*` |
|        - | 15124 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15125 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15126 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15127 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15128 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15129 | ` * Refer to the official for more information on this powerful` |
|        - | 15130 | ` * extension.` |
|        - | 15131 | ` */` |
|  3107282 | 15132 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15133 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15134 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15135 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15136 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15137 | `	)` |
|        2 | 15138 |  |
|        - | 15139 | `	VmRefObj *pRef;` |
|        - | 15140 | `	sxu32 n;` |
|        - | 15141 | `	/* Check if the referenced object already exists */` |
|  3107284 | 15142 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3107284 | 15143 | `	if( pRef == 0 ){` |
|        - | 15144 | `		/* Not such entry */` |
|   100884 | 15145 | `		return SXERR_NOTFOUND;` |
|        - | 15146 | `	}` |
|        - | 15147 | `	/* Remove the desired entry */` |
|  3006402 | 15148 | `	if( pEntry ){` |
|        - | 15149 | `		SyHashEntry **apEntry;` |
|       62 | 15150 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 15151 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 15152 | `			if( apEntry[n] == pEntry ){` |
|        - | 15153 | `				/* Nullify the entry */` |
|       62 | 15154 | `				apEntry[n] = 0;` |
|        - | 15155 | `				/*` |
|        - | 15156 | `				 * NOTE:` |
|        - | 15157 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15158 | `				 * we avoid wasting spaces.` |
|        - | 15159 | `				 */` |
|       30 | 15160 | `			}` |
|       85 | 15161 | `		}` |
|       30 | 15162 | `	}` |
|  3006402 | 15163 | `	if( pMapEntry ){` |
|        - | 15164 | `		ph7_hashmap_node **apNode;` |
|  3006342 | 15165 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6012776 | 15166 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3006436 | 15167 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15168 | `				/* nullify the entry */` |
|  3006342 | 15169 | `				apNode[n] = 0;` |
|  1503170 | 15170 | `			}` |
|  1503219 | 15171 | `		}` |
|  1503170 | 15172 | `	}` |
|  3006402 | 15173 | `	return SXRET_OK;` |
|  1553643 | 15174 |  |
|        - | 15175 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15176 | `/*` |
|        - | 15177 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15178 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15179 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15180 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15181 | ` * For more information on how to register IO stream devices,please` |
|        - | 15182 | ` * refer to the official documentation.` |
|        - | 15183 | ` */` |
|    28128 | 15184 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15185 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15186 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15187 | `	int nByte              /* *pzDevice length*/` |
|        - | 15188 | `	)` |
|        2 | 15189 |  |
|        - | 15190 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15191 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15192 | `	SyString sDev,sCur;` |
|        - | 15193 | `	sxu32 n,nEntry;` |
|        - | 15194 | `	int rc;` |
|        - | 15195 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28130 | 15196 | `	zNext = zCur = zIn = *pzDevice;` |
|    28130 | 15197 | `	zEnd = &zIn[nByte];` |
|  1794338 | 15198 | `	while( zIn < zEnd ){` |
|  1766212 | 15199 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15200 | `			/* Got one */` |
|        3 | 15201 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15202 | `			break;` |
|        - | 15203 | `		}` |
|        - | 15204 | `		/* Advance the cursor */` |
|  1766210 | 15205 | `		zIn++;` |
|        2 | 15206 | `	}` |
|    28130 | 15207 | `	if( zIn >= zEnd ){` |
|        - | 15208 | `		/* No such scheme,return the default stream */` |
|    28128 | 15209 | `		return pVm->pDefStream;` |
|        - | 15210 | `	}` |
|        3 | 15211 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15212 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15213 | `	SyStringFullTrim(&sDev);` |
|        - | 15214 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15215 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15216 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15217 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15218 | `		pStream = apStream[n];` |
|        3 | 15219 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15220 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15221 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15222 | `		if( rc == 0 ){` |
|        - | 15223 | `			/* Stream device found */` |
|        3 | 15224 | `			*pzDevice = zNext;` |
|        3 | 15225 | `			return pStream;` |
|        - | 15226 | `		}` |
|      ! 0 | 15227 | `	}` |
|        - | 15228 | `	/* No such stream,return NULL */` |
|      ! 0 | 15229 | `	return 0;` |
|    14066 | 15230 |  |
|        - | 15231 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15232 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15233 |  |
