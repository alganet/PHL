# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6470/8319 lines (77.77%)

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
|   909066 |   142 | `static sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|        2 |   143 |  |
|   909068 |   144 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       35 |   145 | `		return TRUE;` |
|        - |   146 | `	}` |
|   909034 |   147 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       11 |   148 | `		return TRUE;` |
|        - |   149 | `	}` |
|   909024 |   150 | `	return FALSE;` |
|   454557 |   151 |  |
|        - |   152 | `/*` |
|        - |   153 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|        - |   154 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|        - |   155 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|        - |   156 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|        - |   157 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|        - |   158 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|        - |   159 | ` * still go through the existing numeric coercion.` |
|        - |   160 | ` */` |
|   335228 |   161 | `static int VmStringWantsPerlIncr(ph7_value *pVal)` |
|        2 |   162 |  |
|        - |   163 | `	SyString sStr;` |
|   335230 |   164 | `	sxu8 bReal = FALSE;` |
|   335230 |   165 | `	const char *zTail = 0;` |
|        - |   166 | `	const char *zEnd;` |
|   335230 |   167 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   335160 |   168 | `		return FALSE;` |
|        - |   169 | `	}` |
|       71 |   170 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|       71 |   171 | `	if( sStr.nByte == 0 ){` |
|        5 |   172 | `		return TRUE;` |
|        - |   173 | `	}` |
|       67 |   174 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|       47 |   175 | `		return TRUE;` |
|        - |   176 | `	}` |
|        - |   177 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|        - |   178 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|        - |   179 | `	 * still takes the Perl path. */` |
|       21 |   180 | `	zEnd = sStr.zString + sStr.nByte;` |
|       25 |   181 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|        5 |   182 | `		zTail++;` |
|        1 |   183 | `	}` |
|       21 |   184 | `	return zTail < zEnd;` |
|   167638 |   185 |  |
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
|   609362 |   200 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
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
|   609364 |   211 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   609364 |   212 | `	if( pEntry ){` |
|        - |   213 | `		/* Overwrite the old definition and return immediately */` |
|        6 |   214 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|        6 |   215 | `		pCons->xExpand = xExpand;` |
|        6 |   216 | `		pCons->pUserData = pUserData;` |
|        6 |   217 | `		return SXRET_OK;` |
|        - |   218 | `	}` |
|        - |   219 | `	/* Allocate a new constant instance */` |
|   609360 |   220 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   609360 |   221 | `	if( pCons == 0 ){` |
|      ! 0 |   222 | `		return 0;` |
|        - |   223 | `	}` |
|        - |   224 | `	/* Duplicate constant name */` |
|   609360 |   225 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   609360 |   226 | `	if( zDupName == 0 ){` |
|      ! 0 |   227 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   228 | `		return 0;` |
|        - |   229 | `	}` |
|        - |   230 | `	/* Install the constant */` |
|   609360 |   231 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   609360 |   232 | `	pCons->xExpand = xExpand;` |
|   609360 |   233 | `	pCons->pUserData = pUserData;` |
|   609360 |   234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   609360 |   235 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   236 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|      ! 0 |   237 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|      ! 0 |   238 | `		return rc;` |
|        - |   239 | `	}` |
|        - |   240 | `	/* All done,constant can be invoked from PHP code */` |
|   609360 |   241 | `	return SXRET_OK;` |
|   304683 |   242 |  |
|        - |   243 | `/*` |
|        - |   244 | ` * Allocate a new foreign function instance.` |
|        - |   245 | ` * This function return SXRET_OK on success. Any other` |
|        - |   246 | ` * return value indicates failure.` |
|        - |   247 | ` * Please refer to the official documentation for an introduction to` |
|        - |   248 | ` * the foreign function mechanism.` |
|        - |   249 | ` */` |
|  1353806 |   250 | `static sxi32 PH7_NewForeignFunction(` |
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
|  1353808 |   261 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  1353808 |   262 | `	if( pFunc == 0 ){` |
|      ! 0 |   263 | `		return SXERR_MEM;` |
|        - |   264 | `	}` |
|        - |   265 | `	/* Duplicate function name */` |
|  1353808 |   266 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  1353808 |   267 | `	if( zDup == 0 ){` |
|      ! 0 |   268 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   269 | `		return SXERR_MEM;` |
|        - |   270 | `	}` |
|        - |   271 | `	/* Zero the structure */` |
|  1353808 |   272 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|        - |   273 | `	/* Initialize structure fields */` |
|  1353808 |   274 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  1353808 |   275 | `	pFunc->pVm   = pVm;` |
|  1353808 |   276 | `	pFunc->xFunc = xFunc;` |
|  1353808 |   277 | `	pFunc->pUserData = pUserData;` |
|  1353808 |   278 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |   279 | `	/* Write a pointer to the new function */` |
|  1353808 |   280 | `	*ppOut = pFunc;` |
|  1353808 |   281 | `	return SXRET_OK;` |
|   676905 |   282 |  |
|        - |   283 | `/*` |
|        - |   284 | ` * Install a foreign function and it's associated callback so that` |
|        - |   285 | ` * it can be invoked from the target PHP code.` |
|        - |   286 | ` * This function return SXRET_OK on successful registration. Any other` |
|        - |   287 | ` * return value indicates failure.` |
|        - |   288 | ` * Please refer to the official documentation for an introduction to` |
|        - |   289 | ` * the foreign function mechanism.` |
|        - |   290 | ` */` |
|  1356614 |   291 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
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
|  1356616 |   302 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|  1356616 |   303 | `	if( pEntry ){` |
|     2810 |   304 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|     2810 |   305 | `		pFunc->pUserData = pUserData;` |
|     2810 |   306 | `		pFunc->xFunc = xFunc;` |
|     2810 |   307 | `		SySetReset(&pFunc->aAux);` |
|     2810 |   308 | `		return SXRET_OK;` |
|        - |   309 | `	}` |
|        - |   310 | `	/* Create a new user function */` |
|  1353808 |   311 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|  1353808 |   312 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   313 | `		return rc;` |
|        - |   314 | `	}` |
|        - |   315 | `	/* Install the function in the corresponding hashtable */` |
|  1353808 |   316 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|  1353808 |   317 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   318 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|      ! 0 |   319 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|      ! 0 |   320 | `		return rc;` |
|        - |   321 | `	}` |
|        - |   322 | `	/* User function successfully installed */` |
|  1353808 |   323 | `	return SXRET_OK;` |
|   678309 |   324 |  |
|        - |   325 | `/*` |
|        - |   326 | ` * Initialize a VM function.` |
|        - |   327 | ` */` |
|   273406 |   328 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|        - |   329 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   330 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|        - |   331 | `	const char *zName,  /* Function name */` |
|        - |   332 | `	sxu32 nByte,        /* zName length */` |
|        - |   333 | `	sxi32 iFlags,       /* Configuration flags */` |
|        - |   334 | `	void *pUserData     /* Function private data */` |
|        - |   335 | `	)` |
|        2 |   336 |  |
|        - |   337 | `	/* Zero the structure */` |
|   273408 |   338 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|        - |   339 | `	/* Initialize structure fields */` |
|        - |   340 | `	/* Arguments container */` |
|   273408 |   341 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|        - |   342 | `	/* Static variable container */` |
|   273408 |   343 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|        - |   344 | `	/* Bytecode container */` |
|   273408 |   345 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|        - |   346 | `    /* Preallocate some instruction slots */` |
|   273408 |   347 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|        - |   348 | `	/* Closure environment */` |
|   273408 |   349 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|        - |   350 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   273408 |   351 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|   273408 |   352 | `	pFunc->iFlags = iFlags;` |
|   273408 |   353 | `	pFunc->pUserData = pUserData;` |
|        - |   354 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|        - |   355 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   273408 |   356 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   273408 |   357 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   273408 |   358 | `	return SXRET_OK;` |
|        2 |   359 |  |
|        - |   360 | `/*` |
|        - |   361 | ` * Namespace-aware function lookup.` |
|        - |   362 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|        - |   363 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|        - |   364 | ` */` |
|        - |   365 | `/*` |
|        - |   366 | ` * Install a user defined function in the corresponding VM container.` |
|        - |   367 | ` */` |
|   754436 |   368 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|        - |   369 | `	ph7_vm *pVm,        /* Target VM */` |
|        - |   370 | `	ph7_vm_func *pFunc, /* Target function */` |
|        - |   371 | `	SyString *pName     /* Function name */` |
|        - |   372 | `	)` |
|        2 |   373 |  |
|        - |   374 | `	SyHashEntry *pEntry;` |
|        - |   375 | `	sxi32 rc;` |
|   754438 |   376 | `	if( pName == 0 ){` |
|        - |   377 | `		/* Use the built-in name */` |
|    41560 |   378 | `		pName = &pFunc->sName;` |
|    20779 |   379 | `	}` |
|        - |   380 | `	/* Check for duplicates (functions with the same name) first */` |
|   754438 |   381 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|   754438 |   382 | `	if( pEntry ){` |
|   559372 |   383 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|   559372 |   384 | `		if( pLink != pFunc ){` |
|        - |   385 | `			/* Link */` |
|      188 |   386 | `			pFunc->pNextName = pLink;` |
|      188 |   387 | `			pEntry->pUserData = pFunc;` |
|       93 |   388 | `		}` |
|   559372 |   389 | `		return SXRET_OK;` |
|        - |   390 | `	}` |
|        - |   391 | `	/* First time seen */` |
|   195068 |   392 | `	pFunc->pNextName = 0;` |
|   195068 |   393 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   195068 |   394 | `	return rc;` |
|   377220 |   395 |  |
|        - |   396 | `/*` |
|        - |   397 | ` * Install a user defined class in the corresponding VM container.` |
|        - |   398 | ` */` |
|    75908 |   399 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|        - |   400 | `	ph7_vm *pVm,      /* Target VM  */` |
|        - |   401 | `	ph7_class *pClass /* Target Class */` |
|        - |   402 | `	)` |
|        2 |   403 |  |
|    75910 |   404 | `	SyString *pName = &pClass->sName;` |
|        - |   405 | `	SyHashEntry *pEntry;` |
|        - |   406 | `	sxi32 rc;` |
|        - |   407 | `	/* Check for duplicates */` |
|    75910 |   408 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    75910 |   409 | `	if( pEntry ){` |
|       31 |   410 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|        - |   411 | `		/* Link entry with the same name */` |
|       31 |   412 | `		pClass->pNextName = pLink;` |
|       31 |   413 | `		pEntry->pUserData = pClass;` |
|       31 |   414 | `		return SXRET_OK;` |
|        - |   415 | `	}` |
|    75880 |   416 | `	pClass->pNextName = 0;` |
|        - |   417 | `	/* Perform a simple hashtable insertion */` |
|    75880 |   418 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    75880 |   419 | `	return rc;` |
|    37956 |   420 |  |
|        - |   421 | `/*` |
|        - |   422 | ` * Instruction builder interface.` |
|        - |   423 | ` */` |
|  4226958 |   424 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
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
|  4226960 |   436 | `	sInstr.iOp = (sxu8)iOp;` |
|  4226960 |   437 | `	sInstr.iP1 = iP1;` |
|  4226960 |   438 | `	sInstr.iP2 = iP2;` |
|  4226960 |   439 | `	sInstr.p3  = p3;` |
|  4226960 |   440 | `	if( pIndex ){` |
|        - |   441 | `		/* Instruction index in the bytecode array */` |
|   229640 |   442 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|   114819 |   443 | `	}` |
|        - |   444 | `	/* Finally,record the instruction */` |
|  4226960 |   445 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  4226960 |   446 | `	if( rc != SXRET_OK ){` |
|      ! 0 |   447 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|        - |   448 | `		/* Fall throw */` |
|      ! 0 |   449 | `	}` |
|  4226960 |   450 | `	return rc;` |
|        2 |   451 |  |
|        - |   452 | `/*` |
|        - |   453 | ` * Swap the current bytecode container with the given one.` |
|        - |   454 | ` */` |
|   549088 |   455 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|        2 |   456 |  |
|   549090 |   457 | `	if( pContainer == 0 ){` |
|        - |   458 | `		/* Point to the default container */` |
|      ! 0 |   459 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|      ! 0 |   460 | `	}else{` |
|        - |   461 | `		/* Change container */` |
|   549090 |   462 | `		pVm->pByteContainer = &(*pContainer);` |
|        - |   463 | `	}` |
|   549090 |   464 | `	return SXRET_OK;` |
|        2 |   465 |  |
|        - |   466 | `/*` |
|        - |   467 | ` * Return the current bytecode container.` |
|        - |   468 | ` */` |
|   274544 |   469 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|        2 |   470 |  |
|   274546 |   471 | `	return pVm->pByteContainer;` |
|        2 |   472 |  |
|        - |   473 | `/*` |
|        - |   474 | ` * Extract the VM instruction rooted at nIndex.` |
|        - |   475 | ` */` |
|   226440 |   476 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|        2 |   477 |  |
|        - |   478 | `	VmInstr *pInstr;` |
|   226442 |   479 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   226442 |   480 | `	return pInstr;` |
|        2 |   481 |  |
|        - |   482 | `/*` |
|        - |   483 | ` * Return the total number of VM instructions recorded so far.` |
|        - |   484 | ` */` |
|  1270716 |   485 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|        2 |   486 |  |
|  1270718 |   487 | `	return SySetUsed(pVm->pByteContainer);` |
|        2 |   488 |  |
|        - |   489 | `/*` |
|        - |   490 | ` * Pop the last VM instruction.` |
|        - |   491 | ` */` |
|   209510 |   492 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|        2 |   493 |  |
|   209512 |   494 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|        2 |   495 |  |
|        - |   496 | `/*` |
|        - |   497 | ` * Peek the last VM instruction.` |
|        - |   498 | ` */` |
|   832874 |   499 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|        2 |   500 |  |
|   832876 |   501 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|        2 |   502 |  |
|    33108 |   503 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|        2 |   504 |  |
|        - |   505 | `	VmInstr *aInstr;` |
|        - |   506 | `	sxu32 n;` |
|    33110 |   507 | `	n = SySetUsed(pVm->pByteContainer);` |
|    33110 |   508 | `	if( n < 2 ){` |
|      ! 0 |   509 | `		return 0;` |
|        - |   510 | `	}` |
|    33110 |   511 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    33110 |   512 | `	return &aInstr[n - 2];` |
|    16556 |   513 |  |
|        - |   514 | `/*` |
|        - |   515 | ` * Allocate a new virtual machine frame.` |
|        - |   516 | ` */` |
|    21786 |   517 | `static VmFrame * VmNewFrame(` |
|        - |   518 | `	ph7_vm *pVm,              /* Target VM */` |
|        - |   519 | `	void *pUserData,          /* Upper-layer private data */` |
|        - |   520 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   521 | `	)` |
|        2 |   522 |  |
|        - |   523 | `	VmFrame *pFrame;` |
|        - |   524 | `	/* Allocate a new vm frame */` |
|    21788 |   525 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|    21788 |   526 | `	if( pFrame == 0 ){` |
|      ! 0 |   527 | `		return 0;` |
|        - |   528 | `	}` |
|        - |   529 | `	/* Zero the structure */` |
|    21788 |   530 | `	SyZero(pFrame,sizeof(VmFrame));` |
|        - |   531 | `	/* Initialize frame fields */` |
|    21788 |   532 | `	pFrame->pUserData = pUserData;` |
|    21788 |   533 | `	pFrame->pThis = pThis;` |
|    21788 |   534 | `	pFrame->pVm = pVm;` |
|    21788 |   535 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|    21788 |   536 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|    21788 |   537 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|    21788 |   538 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|    21788 |   539 | `	return pFrame;` |
|    10895 |   540 |  |
|        - |   541 | `/* Forward declaration */` |
|        - |   542 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame);` |
|        - |   543 | `/*` |
|        - |   544 | ` * Enter a VM frame.` |
|        - |   545 | ` */` |
|    21740 |   546 | `static sxi32 VmEnterFrame(` |
|        - |   547 | `	ph7_vm *pVm,               /* Target VM */` |
|        - |   548 | `	void *pUserData,           /* Upper-layer private data */` |
|        - |   549 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|        - |   550 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|        - |   551 | `	)` |
|        2 |   552 |  |
|        - |   553 | `	VmFrame *pFrame;` |
|        - |   554 | `	/* Allocate a new frame */` |
|    21742 |   555 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|    21742 |   556 | `	if( pFrame == 0 ){` |
|      ! 0 |   557 | `		return SXERR_MEM;` |
|        - |   558 | `	}` |
|        - |   559 | `	/* Link to the list of active VM frame */` |
|    21742 |   560 | `	pFrame->pParent = pVm->pFrame;` |
|    21742 |   561 | `	pVm->pFrame = pFrame;` |
|    21742 |   562 | `	if( ppFrame ){` |
|        - |   563 | `		/* Write a pointer to the new VM frame */` |
|    18620 |   564 | `		*ppFrame = pFrame;` |
|     9309 |   565 | `	}` |
|    21742 |   566 | `	return SXRET_OK;` |
|    10872 |   567 |  |
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
|    18608 |   611 | `static void VmLeaveFrame(ph7_vm *pVm)` |
|        2 |   612 |  |
|    18610 |   613 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|    18610 |   614 | `	if( pCurFrame ){` |
|        - |   615 | `		/* Unlink from the list of active VM frame */` |
|    18610 |   616 | `		pVm->pFrame = pCurFrame->pParent;` |
|    18610 |   617 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|        - |   618 | `			VmSlot  *aSlot;` |
|        - |   619 | `			sxu32 n;` |
|        - |   620 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    18284 |   621 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   121534 |   622 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|        - |   623 | `				/* Unset the local variable */` |
|   103252 |   624 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    51627 |   625 | `			}` |
|        - |   626 | `			/* Remove local reference */` |
|    18284 |   627 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   121596 |   628 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   103314 |   629 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    51658 |   630 | `			}` |
|     9141 |   631 | `		}` |
|        - |   632 | `		/* Release internal containers */` |
|    18610 |   633 | `		SyHashRelease(&pCurFrame->hVar);` |
|    18610 |   634 | `		SySetRelease(&pCurFrame->sArg);` |
|    18610 |   635 | `		SySetRelease(&pCurFrame->sLocal);` |
|    18610 |   636 | `		SySetRelease(&pCurFrame->sRef);` |
|        - |   637 | `		/* Release the whole structure */` |
|    18610 |   638 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|     9304 |   639 | `	}` |
|    18610 |   640 |  |
|        - |   641 | `/*` |
|        - |   642 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|        - |   643 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|        - |   644 | ` * should be skipped when looking for the real execution context.` |
|        - |   645 | ` */` |
|  7028246 |   646 | `static VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|        2 |   647 |  |
|  7030330 |   648 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     2084 |   649 | `		pFrame = pFrame->pParent;` |
|        2 |   650 | `	}` |
|  7028248 |   651 | `	return pFrame;` |
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
|   257002 |   771 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|        - |   772 | `	ph7_vm *pVm,      /* Target VM */` |
|        - |   773 | `	ph7_class *pClass /* Class to be mounted */` |
|        - |   774 | `	)` |
|        2 |   775 |  |
|        - |   776 | `	ph7_class_method *pMeth;` |
|        - |   777 | `	ph7_class_attr *pAttr;` |
|        - |   778 | `	SyHashEntry *pEntry;` |
|        - |   779 | `	sxi32 rc;` |
|        - |   780 | `	/* Reset the loop cursor */` |
|   257004 |   781 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|        - |   782 | `	/* Process only static and constant attribute */` |
|   786421 |   783 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   784 | `		/* Extract the current attribute */` |
|   400918 |   785 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   400918 |   786 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|        - |   787 | `			ph7_value *pMemObj;` |
|        - |   788 | `			/* Reserve a memory object for this constant/static attribute */` |
|     1792 |   789 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     1792 |   790 | `			if( pMemObj == 0 ){` |
|      ! 0 |   791 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |   792 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|      ! 0 |   793 | `					&pClass->sName,&pAttr->sName` |
|        - |   794 | `					);` |
|      ! 0 |   795 | `				return SXERR_MEM;` |
|        - |   796 | `			}` |
|     1792 |   797 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   798 | `				/* Initialize attribute default value (any complex expression) */` |
|     1788 |   799 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|      893 |   800 | `			}` |
|        - |   801 | `			/* Record attribute index */` |
|     1792 |   802 | `			pAttr->nIdx = pMemObj->nIdx;` |
|        - |   803 | `			/* Install static attribute in the reference table */` |
|     1792 |   804 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   805 | `			/* If this is a typed static property, register the slot so the` |
|        - |   806 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|        - |   807 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|        - |   808 | `			 * points at its own nIdx field (stable for the VM lifetime). */` |
|     1792 |   809 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
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
|      895 |   828 | `		}` |
|        2 |   829 | `	}` |
|        - |   830 | `	/* Install class methods */` |
|   257004 |   831 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|        - |   832 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|        - |   833 | `		 */` |
|   173164 |   834 | `		return SXRET_OK;` |
|        - |   835 | `	}` |
|        - |   836 | `	/* Create constructor alias if not yet done */` |
|    83842 |   837 | `	if( SyHashGet(&pClass->hMethod,"__construct",sizeof("__construct")-1) == 0 ){` |
|        - |   838 | `		/* User constructor with the same base class name */` |
|     6612 |   839 | `		pEntry = SyHashGet(&pClass->hMethod,SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|     6612 |   840 | `		if( pEntry ){` |
|      ! 0 |   841 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|        - |   842 | `			/* Create the alias */` |
|      ! 0 |   843 | `			SyHashInsert(&pClass->hMethod,"__construct",sizeof("__construct")-1,pMeth);` |
|      ! 0 |   844 | `		}` |
|     3305 |   845 | `	}` |
|        - |   846 | `	/* Install the methods now */` |
|    83842 |   847 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   838648 |   848 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   712888 |   849 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   712888 |   850 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|   712880 |   851 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|   712880 |   852 | `			if( rc != SXRET_OK ){` |
|      ! 0 |   853 | `				return rc;` |
|        - |   854 | `			}` |
|   356439 |   855 | `		}` |
|        2 |   856 | `	}` |
|        - |   857 | `	/* Mark class as mounted to avoid redundant mounting */` |
|    83842 |   858 | `	pClass->bMounted = TRUE;` |
|    83842 |   859 | `	return SXRET_OK;` |
|   128503 |   860 |  |
|        - |   861 | `/*` |
|        - |   862 | ` * Allocate a private frame for attributes of the given` |
|        - |   863 | ` * class instance (Object in the PHP jargon).` |
|        - |   864 | ` */` |
|     2000 |   865 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|        - |   866 | `	ph7_vm *pVm, /* Target VM */` |
|        - |   867 | `	ph7_class_instance *pObj /* Class instance */` |
|        - |   868 | `	)` |
|        2 |   869 |  |
|     2002 |   870 | `	ph7_class *pClass = pObj->pClass;` |
|        - |   871 | `	ph7_class_attr *pAttr;` |
|        - |   872 | `	SyHashEntry *pEntry;` |
|        - |   873 | `	sxi32 rc;` |
|        - |   874 | `	/* Install class attribute in the private frame associated with this instance */` |
|     2002 |   875 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|     8216 |   876 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|        - |   877 | `		VmClassAttr *pVmAttr;` |
|        - |   878 | `		/* Extract the current attribute */` |
|     6216 |   879 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     6216 |   880 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|     6216 |   881 | `		if( pVmAttr == 0 ){` |
|      ! 0 |   882 | `			return SXERR_MEM;` |
|        - |   883 | `		}` |
|     6216 |   884 | `		pVmAttr->pAttr = pAttr;` |
|     6216 |   885 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|        - |   886 | `			ph7_value *pMemObj;` |
|        - |   887 | `			/* Reserve a memory object for this attribute */` |
|     6192 |   888 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|     6192 |   889 | `			if( pMemObj == 0 ){` |
|      ! 0 |   890 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   891 | `				return SXERR_MEM;` |
|        - |   892 | `			}` |
|     6192 |   893 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|     6192 |   894 | `			pVmAttr->iState = 0;` |
|     6192 |   895 | `			pVmAttr->pOwner = pClass;` |
|     6192 |   896 | `			if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|        - |   897 | `				/* Initialize attribute default value (any complex expression) */` |
|     2136 |   898 | `				VmLocalExec(&(*pVm),&pAttr->aByteCode,pMemObj);` |
|     5125 |   899 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        - |   900 | `				/* Typed property without a default: mark uninitialized. Reading` |
|        - |   901 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|       74 |   902 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       36 |   903 | `			}` |
|     6192 |   904 | `			rc = SyHashInsert(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|     6192 |   905 | `			if( rc != SXRET_OK ){` |
|        - |   906 | `				VmSlot sSlot;` |
|        - |   907 | `				/* Restore memory object */` |
|      ! 0 |   908 | `				sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   909 | `				sSlot.pUserData = 0;` |
|      ! 0 |   910 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   911 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   912 | `				return SXERR_MEM;` |
|        - |   913 | `			}` |
|        - |   914 | `			/* Install attribute in the reference table */` |
|     6192 |   915 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        - |   916 | `			/* Register typed property slot for assignment-time enforcement.` |
|        - |   917 | `			 * On failure roll back the just-installed hAttr entry and the` |
|        - |   918 | `			 * reserved memobj so the caller sees a consistent instance. */` |
|     6192 |   919 | `			if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      170 |   920 | `				rc = SyHashInsert(&pVm->hTypedSlot,(const void *)&pVmAttr->nIdx,sizeof(sxu32),pVmAttr);` |
|      170 |   921 | `				if( rc != SXRET_OK ){` |
|        - |   922 | `					VmSlot sSlot;` |
|      ! 0 |   923 | `					SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|      ! 0 |   924 | `					sSlot.nIdx = pMemObj->nIdx;` |
|      ! 0 |   925 | `					sSlot.pUserData = 0;` |
|      ! 0 |   926 | `					SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|      ! 0 |   927 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|      ! 0 |   928 | `					return SXERR_MEM;` |
|        - |   929 | `				}` |
|       84 |   930 | `			}` |
|     3097 |   931 | `		}else{` |
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
|     2002 |   943 | `	return SXRET_OK;` |
|     1002 |   944 |  |
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
|   451856 |   956 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   957 |  |
|        - |   958 | `	ph7_value *pObj;` |
|        - |   959 | `	sxi32 rc;` |
|   451858 |   960 | `	if( pIndex ){` |
|        - |   961 | `		/* Object index in the object table */` |
|   442492 |   962 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   221245 |   963 | `	}` |
|        - |   964 | `	/* Reserve a slot for the new object */` |
|   451858 |   965 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   451858 |   966 | `	if( rc != SXRET_OK ){` |
|        - |   967 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   968 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   969 | `		 */` |
|      ! 0 |   970 | `		return 0;` |
|        - |   971 | `	}` |
|   451858 |   972 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   451858 |   973 | `	return pObj;` |
|   225930 |   974 |  |
|        - |   975 | `/*` |
|        - |   976 | ` * Reserve a memory object.` |
|        - |   977 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |   978 | ` */` |
|  2151344 |   979 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|        2 |   980 |  |
|        - |   981 | `	ph7_value *pObj;` |
|        - |   982 | `	sxi32 rc;` |
|  2151346 |   983 | `	if( pIndex ){` |
|        - |   984 | `		/* Object index in the object table */` |
|  2151346 |   985 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|  1075672 |   986 | `	}` |
|        - |   987 | `	/* Reserve a slot for the new object */` |
|  2151346 |   988 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|  2151346 |   989 | `	if( rc != SXRET_OK ){` |
|        - |   990 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|        - |   991 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|        - |   992 | `		 */` |
|      ! 0 |   993 | `		return 0;` |
|        - |   994 | `	}` |
|  2151346 |   995 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|  2151346 |   996 | `	return pObj;` |
|  1075674 |   997 |  |
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
|        - |  1049 | `	"interface Traversable {}"\` |
|        - |  1050 | `	"interface ArrayAccess {"\` |
|        - |  1051 | `	"public function offsetExists($offset);"\` |
|        - |  1052 | `	"public function offsetGet($offset);"\` |
|        - |  1053 | `	"public function offsetSet($offset, $value);"\` |
|        - |  1054 | `	"public function offsetUnset($offset);"\` |
|        - |  1055 | `	"}"\` |
|        - |  1056 | `	"interface Countable {"\` |
|        - |  1057 | `	"public function count();"\` |
|        - |  1058 | `	"}"\` |
|        - |  1059 | `	"interface Stringable {"\` |
|        - |  1060 | `	"public function __toString();"\` |
|        - |  1061 | `	"}"\` |
|        - |  1062 | `	"interface UnitEnum {"\` |
|        - |  1063 | `	"public static function cases();"\` |
|        - |  1064 | `	"}"\` |
|        - |  1065 | `	"interface BackedEnum extends UnitEnum {"\` |
|        - |  1066 | `	"public static function from($value);"\` |
|        - |  1067 | `	"public static function tryFrom($value);"\` |
|        - |  1068 | `	"}"\` |
|        - |  1069 | `	"class Exception implements Throwable { "\` |
|        - |  1070 | `    "protected $message = '';"\` |
|        - |  1071 | `    "protected $code = 0;"\` |
|        - |  1072 | `    "protected $file;"\` |
|        - |  1073 | `    "protected $line;"\` |
|        - |  1074 | `    "protected $trace;"\` |
|        - |  1075 | `    "protected $previous;"\` |
|        - |  1076 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1077 | `	"   if( isset($message) ){"\` |
|        - |  1078 | `	"	  $this->message = $message;"\` |
|        - |  1079 | `	"   }"\` |
|        - |  1080 | `	"   $this->code = $code;"\` |
|        - |  1081 | `	"   $this->file = __FILE__;"\` |
|        - |  1082 | `	"   $this->line = __LINE__;"\` |
|        - |  1083 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1084 | `	"   if( isset($previous) ){"\` |
|        - |  1085 | `	"     $this->previous = $previous;"\` |
|        - |  1086 | `	"   }"\` |
|        - |  1087 | `	"}"\` |
|        - |  1088 | `	"public function getMessage(){"\` |
|        - |  1089 | `	"   return $this->message;"\` |
|        - |  1090 | `	"}"\` |
|        - |  1091 | `	" public function getCode(){"\` |
|        - |  1092 | `	"  return $this->code;"\` |
|        - |  1093 | `	"}"\` |
|        - |  1094 | `	"public function getFile(){"\` |
|        - |  1095 | `	"  return $this->file;"\` |
|        - |  1096 | `	"}"\` |
|        - |  1097 | `	"public function getLine(){"\` |
|        - |  1098 | `	"  return $this->line;"\` |
|        - |  1099 | `	"}"\` |
|        - |  1100 | `	"public function getTrace(){"\` |
|        - |  1101 | `	"   return $this->trace;"\` |
|        - |  1102 | `	"}"\` |
|        - |  1103 | `	"public function getTraceAsString(){"\` |
|        - |  1104 | `	"  return debug_string_backtrace();"\` |
|        - |  1105 | `	"}"\` |
|        - |  1106 | `	"public function getPrevious(){"\` |
|        - |  1107 | `	"    return $this->previous;"\` |
|        - |  1108 | `	"}"\` |
|        - |  1109 | `	"public function __toString(){"\` |
|        - |  1110 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1111 | `    "}"\` |
|        - |  1112 | `	"}"\` |
|        - |  1113 | `	"class Error implements Throwable { "\` |
|        - |  1114 | `    "protected $message = '';"\` |
|        - |  1115 | `    "protected $code = 0;"\` |
|        - |  1116 | `    "protected $file;"\` |
|        - |  1117 | `    "protected $line;"\` |
|        - |  1118 | `    "protected $trace;"\` |
|        - |  1119 | `    "protected $previous;"\` |
|        - |  1120 | `	"public function __construct($message = null, $code = 0, Throwable $previous = null){"\` |
|        - |  1121 | `	"   if( isset($message) ){"\` |
|        - |  1122 | `	"	  $this->message = $message;"\` |
|        - |  1123 | `	"   }"\` |
|        - |  1124 | `	"   $this->code = $code;"\` |
|        - |  1125 | `	"   $this->file = __FILE__;"\` |
|        - |  1126 | `	"   $this->line = __LINE__;"\` |
|        - |  1127 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1128 | `	"   if( isset($previous) ){"\` |
|        - |  1129 | `	"     $this->previous = $previous;"\` |
|        - |  1130 | `	"   }"\` |
|        - |  1131 | `	"}"\` |
|        - |  1132 | `	"public function getMessage(){"\` |
|        - |  1133 | `	"   return $this->message;"\` |
|        - |  1134 | `	"}"\` |
|        - |  1135 | `	"public function getCode(){"\` |
|        - |  1136 | `	"  return $this->code;"\` |
|        - |  1137 | `	"}"\` |
|        - |  1138 | `	"public function getFile(){"\` |
|        - |  1139 | `	"  return $this->file;"\` |
|        - |  1140 | `	"}"\` |
|        - |  1141 | `	"public function getLine(){"\` |
|        - |  1142 | `	"  return $this->line;"\` |
|        - |  1143 | `	"}"\` |
|        - |  1144 | `	"public function getTrace(){"\` |
|        - |  1145 | `	"   return $this->trace;"\` |
|        - |  1146 | `	"}"\` |
|        - |  1147 | `	"public function getTraceAsString(){"\` |
|        - |  1148 | `	"  return debug_string_backtrace();"\` |
|        - |  1149 | `	"}"\` |
|        - |  1150 | `	"public function getPrevious(){"\` |
|        - |  1151 | `	"    return $this->previous;"\` |
|        - |  1152 | `	"}"\` |
|        - |  1153 | `	"public function __toString(){"\` |
|        - |  1154 | `	"   return $this->file.' '.$this->line.' '.$this->code.' '.$this->message;"\` |
|        - |  1155 | `	"}"\` |
|        - |  1156 | `	"}"\` |
|        - |  1157 | `	"class TypeError extends Error { }"\` |
|        - |  1158 | `	"class ArgumentCountError extends TypeError { }"\` |
|        - |  1159 | `	"class ValueError extends Error { }"\` |
|        - |  1160 | `	"class FiberError extends Error { }"\` |
|        - |  1161 | `	"class AssertionError extends Error { }"\` |
|        - |  1162 | `	"class ArithmeticError extends Error { }"\` |
|        - |  1163 | `	"class DivisionByZeroError extends ArithmeticError { }"\` |
|        - |  1164 | `	"class ErrorException extends Exception { "\` |
|        - |  1165 | `	"protected $severity;"\` |
|        - |  1166 | `	"public function __construct(string $message = null,"\` |
|        - |  1167 | `	"int $code = 0,int $severity = 1,string $filename = __FILE__ ,int $lineno = __LINE__ ,Throwable $previous = null){"\` |
|        - |  1168 | `	"   if( isset($message) ){"\` |
|        - |  1169 | `	"	  $this->message = $message;"\` |
|        - |  1170 | `	"   }"\` |
|        - |  1171 | `	"   $this->severity = $severity;"\` |
|        - |  1172 | `	"   $this->code = $code;"\` |
|        - |  1173 | `	"   $this->file = $filename;"\` |
|        - |  1174 | `	"   $this->line = $lineno;"\` |
|        - |  1175 | `	"   $this->trace = debug_backtrace();"\` |
|        - |  1176 | `	"   if( isset($previous) ){"\` |
|        - |  1177 | `	"     $this->previous = $previous;"\` |
|        - |  1178 | `	"   }"\` |
|        - |  1179 | `	"}"\` |
|        - |  1180 | `	"public function getSeverity(){"\` |
|        - |  1181 | `	"   return $this->severity;"\` |
|        - |  1182 | `    "}"\` |
|        - |  1183 | `	"}"\` |
|        - |  1184 | `	"interface Iterator extends Traversable {"\` |
|        - |  1185 | `	"public function current();"\` |
|        - |  1186 | `	"public function key();"\` |
|        - |  1187 | `	"public function next();"\` |
|        - |  1188 | `	"public function rewind();"\` |
|        - |  1189 | `	"public function valid();"\` |
|        - |  1190 | `	"}"\` |
|        - |  1191 | `	"interface IteratorAggregate extends Traversable {"\` |
|        - |  1192 | `	"public function getIterator();"\` |
|        - |  1193 | `	"}"\` |
|        - |  1194 | `	"interface Serializable {"\` |
|        - |  1195 | `	"public function serialize();"\` |
|        - |  1196 | `	"public function unserialize(string $serialized);"\` |
|        - |  1197 | `	"}"\` |
|        - |  1198 | `	"/* Directory releated IO */"\` |
|        - |  1199 | `	"class Directory {"\` |
|        - |  1200 | `	"public $handle = null;"\` |
|        - |  1201 | `	"public $path  = null;"\` |
|        - |  1202 | `	"public function __construct(string $path)"\` |
|        - |  1203 | `	"{"\` |
|        - |  1204 | `	"   $this->handle = opendir($path);"\` |
|        - |  1205 | `	"   if( $this->handle !== FALSE ){"\` |
|        - |  1206 | `	"      $this->path = $path;"\` |
|        - |  1207 | `	"   }"\` |
|        - |  1208 | `	"}"\` |
|        - |  1209 | `	"public function __destruct()"\` |
|        - |  1210 | `	"{"\` |
|        - |  1211 | `	"  if( $this->handle != null ){"\` |
|        - |  1212 | `	"       closedir($this->handle);"\` |
|        - |  1213 | `	"  }"\` |
|        - |  1214 | `	"}"\` |
|        - |  1215 | `	"public function read()"\` |
|        - |  1216 | `	"{"\` |
|        - |  1217 | `	"    return readdir($this->handle);"\` |
|        - |  1218 | `	"}"\` |
|        - |  1219 | `	"public function rewind()"\` |
|        - |  1220 | `	"{"\` |
|        - |  1221 | `	"    rewinddir($this->handle);"\` |
|        - |  1222 | `	"}"\` |
|        - |  1223 | `	"public function close()"\` |
|        - |  1224 | `	"{"\` |
|        - |  1225 | `	"    closedir($this->handle);"\` |
|        - |  1226 | `	"    $this->handle = null;"\` |
|        - |  1227 | `	"}"\` |
|        - |  1228 | `	"}"\` |
|        - |  1229 | `	"class Fiber {"\` |
|        - |  1230 | `	"  private $__ctx;"\` |
|        - |  1231 | `	"  private $__callable;"\` |
|        - |  1232 | `	"  public function __construct($callable){ __fiber_construct($this,$callable); }"\` |
|        - |  1233 | `	"  public function start(){ return __fiber_start($this, func_get_args()); }"\` |
|        - |  1234 | `	"  public function resume($value = null){ return __fiber_resume($this,$value); }"\` |
|        - |  1235 | `	"  public function getReturn(){ return __fiber_getReturn($this); }"\` |
|        - |  1236 | `	"  public function isStarted(){ return __fiber_isStarted($this); }"\` |
|        - |  1237 | `	"  public function isRunning(){ return __fiber_isRunning($this); }"\` |
|        - |  1238 | `	"  public function isSuspended(){ return __fiber_isSuspended($this); }"\` |
|        - |  1239 | `	"  public function isTerminated(){ return __fiber_isTerminated($this); }"\` |
|        - |  1240 | `	"  public static function suspend($value = null){ return __fiber_suspend($value); }"\` |
|        - |  1241 | `	"  public function __destruct(){ __fiber_destruct($this); }"\` |
|        - |  1242 | `	"}"\` |
|        - |  1243 | `	"class Generator implements Iterator {"\` |
|        - |  1244 | `	"  private $__ctx;"\` |
|        - |  1245 | `	"  public function current(){ return __gen_current($this); }"\` |
|        - |  1246 | `	"  public function key(){ return __gen_key($this); }"\` |
|        - |  1247 | `	"  public function next(){ return __gen_next($this); }"\` |
|        - |  1248 | `	"  public function rewind(){ return __gen_rewind($this); }"\` |
|        - |  1249 | `	"  public function valid(){ return __gen_valid($this); }"\` |
|        - |  1250 | `	"  public function send($value = null){ return __gen_send($this,$value); }"\` |
|        - |  1251 | `	"  public function throw($exception){ return __gen_throw($this,$exception); }"\` |
|        - |  1252 | `	"  public function getReturn(){ return __gen_getReturn($this); }"\` |
|        - |  1253 | `	"  public function __destruct(){ __gen_destruct($this); }"\` |
|        - |  1254 | `	"}"\` |
|        - |  1255 | `	"class stdClass{"\` |
|        - |  1256 | `	"  public $value;"\` |
|        - |  1257 | `	" /* Magic methods */"\` |
|        - |  1258 | `	" public function __toInt(){ return (int)$this->value; }"\` |
|        - |  1259 | `	" public function __toBool(){ return (bool)$this->value; }"\` |
|        - |  1260 | `	" public function __toFloat(){ return (float)$this->value; }"\` |
|        - |  1261 | `	" public function __toString(){ return (string)$this->value; }"\` |
|        - |  1262 | `	" function __construct($v){ $this->value = $v; }"\` |
|        - |  1263 | `	"}"\` |
|        - |  1264 | `	"function dir(string $path){"\` |
|        - |  1265 | `	"   return new Directory($path);"\` |
|        - |  1266 | `	"}"\` |
|        - |  1267 | `	"function Dir(string $path){"\` |
|        - |  1268 | `	"   return new Directory($path);"\` |
|        - |  1269 | `	"}"\` |
|        - |  1270 | `	"function scandir(string $directory,int $sort_order = SCANDIR_SORT_ASCENDING)"\` |
|        - |  1271 | `    "{"\` |
|        - |  1272 | `	"  if( func_num_args() < 1 ){ return FALSE; }"\` |
|        - |  1273 | `	"  $aDir = array();"\` |
|        - |  1274 | `	"  $pHandle = opendir($directory);"\` |
|        - |  1275 | `	"  if( $pHandle == FALSE ){ return FALSE; }"\` |
|        - |  1276 | `	"  while(FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1277 | `	"      $aDir[] = $pEntry;"\` |
|        - |  1278 | `	"   }"\` |
|        - |  1279 | `	"  closedir($pHandle);"\` |
|        - |  1280 | `	"  if( $sort_order == SCANDIR_SORT_DESCENDING ){"\` |
|        - |  1281 | `	"      rsort($aDir);"\` |
|        - |  1282 | `	"  }else if( $sort_order == SCANDIR_SORT_ASCENDING ){"\` |
|        - |  1283 | `	"      sort($aDir);"\` |
|        - |  1284 | `	"  }"\` |
|        - |  1285 | `	"  return $aDir;"\` |
|        - |  1286 | `	"}"\` |
|        - |  1287 | `	"function glob(string $pattern,int $iFlags = 0){"\` |
|        - |  1288 | `	"/* Open the target directory */"\` |
|        - |  1289 | `	"$zDir = dirname($pattern);"\` |
|        - |  1290 | `	"if(!is_string($zDir) ){ $zDir = './'; }"\` |
|        - |  1291 | `	"$pHandle = opendir($zDir);"\` |
|        - |  1292 | `	"if( $pHandle == FALSE ){"\` |
|        - |  1293 | `	"   /* IO error while opening the current directory,return FALSE */"\` |
|        - |  1294 | `	"	return FALSE;"\` |
|        - |  1295 | `	"}"\` |
|        - |  1296 | `	"$pattern = basename($pattern);"\` |
|        - |  1297 | `	"$pArray = array(); /* Empty array */"\` |
|        - |  1298 | `	"/* Loop throw available entries */"\` |
|        - |  1299 | `	"while( FALSE !== ($pEntry = readdir($pHandle)) ){"\` |
|        - |  1300 | `	" /* Use the built-in strglob function which is a Symisc eXtension for wildcard comparison*/"\` |
|        - |  1301 | `	"	$rc = strglob($pattern,$pEntry);"\` |
|        - |  1302 | `	"	if( $rc ){"\` |
|        - |  1303 | `	"	   if( is_dir($pEntry) ){"\` |
|        - |  1304 | `	"	      if( $iFlags & GLOB_MARK ){"\` |
|        - |  1305 | `	"		     /* Adds a slash to each directory returned */"\` |
|        - |  1306 | `	"			 $pEntry .= DIRECTORY_SEPARATOR;"\` |
|        - |  1307 | `	"		  }"\` |
|        - |  1308 | `	"	   }else if( $iFlags & GLOB_ONLYDIR ){"\` |
|        - |  1309 | `	"	     /* Not a directory,ignore */"\` |
|        - |  1310 | `	"		 continue;"\` |
|        - |  1311 | `	"	   }"\` |
|        - |  1312 | `	"	   /* Add the entry */"\` |
|        - |  1313 | `	"	   $pArray[] = $pEntry;"\` |
|        - |  1314 | `	"	}"\` |
|        - |  1315 | `	" }"\` |
|        - |  1316 | `	"/* Close the handle */"\` |
|        - |  1317 | `	"closedir($pHandle);"\` |
|        - |  1318 | `	"if( ($iFlags & GLOB_NOSORT) == 0 ){"\` |
|        - |  1319 | `	"  /* Sort the array */"\` |
|        - |  1320 | `	"  sort($pArray);"\` |
|        - |  1321 | `	"}"\` |
|        - |  1322 | `	"if( ($iFlags & GLOB_NOCHECK) && sizeof($pArray) < 1 ){"\` |
|        - |  1323 | `	"  /* Return the search pattern if no files matching were found */"\` |
|        - |  1324 | `	"  $pArray[] = $pattern;"\` |
|        - |  1325 | `	"}"\` |
|        - |  1326 | `	"/* Return the created array */"\` |
|        - |  1327 | `	"return $pArray;"\` |
|        - |  1328 | `   "}"\` |
|        - |  1329 | `   "/* Creates a temporary file */"\` |
|        - |  1330 | `   "function tmpfile(){"\` |
|        - |  1331 | `   "  /* Extract the temp directory */"\` |
|        - |  1332 | `   "  $zTempDir = sys_get_temp_dir();"\` |
|        - |  1333 | `   "  if( strlen($zTempDir) < 1 ){"\` |
|        - |  1334 | `   "    /* Use the current dir */"\` |
|        - |  1335 | `   "    $zTempDir = '.';"\` |
|        - |  1336 | `   "  }"\` |
|        - |  1337 | `   "  /* Create the file */"\` |
|        - |  1338 | `   "  $pHandle = fopen($zTempDir.DIRECTORY_SEPARATOR.'PH7'.rand_str(12),'w+');"\` |
|        - |  1339 | `   "  return $pHandle;"\` |
|        - |  1340 | `   "}"\` |
|        - |  1341 | `   "/* Creates a temporary filename */"\` |
|        - |  1342 | `   "function tempnam(string $zDir = sys_get_temp_dir() /* Symisc eXtension */,string $zPrefix = 'PH7')"\` |
|        - |  1343 | `   "{"\` |
|        - |  1344 | `   "   return $zDir.DIRECTORY_SEPARATOR.$zPrefix.rand_str(12);"\` |
|        - |  1345 | `   "}"\` |
|        - |  1346 | `   "function array_unshift(&$pArray ){"\` |
|        - |  1347 | `   " if( func_num_args() < 1 ){ throw new ArgumentCountError('array_unshift() expects at least 1 argument, 0 given'); }"\` |
|        - |  1348 | `   " if( !is_array($pArray) ){ throw new TypeError('array_unshift(): Argument #1 ($array) must be of type array, ' . gettype($pArray) . ' given'); }"\` |
|        - |  1349 | `   "/* Copy arguments */"\` |
|        - |  1350 | `   "$nArgs = func_num_args();"\` |
|        - |  1351 | `   "$pNew = array();"\` |
|        - |  1352 | `   "for( $i = 1 ; $i < $nArgs ; ++$i ){"\` |
|        - |  1353 | `    " $pNew[] = func_get_arg($i);"\` |
|        - |  1354 | `    "}"\` |
|        - |  1355 | `   	"/* Make a copy of the old entries */"\` |
|        - |  1356 | `	"$pOld = array_copy($pArray);"\` |
|        - |  1357 | `	"/* Erase */"\` |
|        - |  1358 | `	"array_erase($pArray);"\` |
|        - |  1359 | `	"/* Unshift */"\` |
|        - |  1360 | `	"$pArray = array_merge($pNew,$pOld);"\` |
|        - |  1361 | `	"return sizeof($pArray);"\` |
|        - |  1362 | `    "}"\` |
|        - |  1363 | `	"function array_merge_recursive(){"\` |
|        - |  1364 | `	" if( func_num_args() < 1 ){ return array(); }"\` |
|        - |  1365 | `    "$arrays = func_get_args();"\` |
|        - |  1366 | `    "$narrays = count($arrays);"\` |
|        - |  1367 | `    "$ret = array();"\` |
|        - |  1368 | `    "for( $i = 0; $i < $narrays; $i++ ){"\` |
|        - |  1369 | `	 " if( !is_array($arrays[$i]) ){"\` |
|        - |  1370 | `	 "  throw new TypeError('array_merge_recursive(): Argument #'.($i + 1).' must be of type array, '.gettype($arrays[$i]).' given');"\` |
|        - |  1371 | `	 " }"\` |
|        - |  1372 | `     " foreach ($arrays[$i] as $key => $value) {"\` |
|        - |  1373 | `     "  $keyIsInt = is_int($key) \|\| (is_string($key) && (string)intval($key) === $key);"\` |
|        - |  1374 | `     "  if( $keyIsInt ) {"\` |
|        - |  1375 | `     "   $ret[] = $value;"\` |
|        - |  1376 | `     "  } else {"\` |
|        - |  1377 | `     "   if (array_key_exists($key, $ret)) {"\` |
|        - |  1378 | `     "    $cur = $ret[$key];"\` |
|        - |  1379 | `     "    if (is_array($cur) && is_array($value)) {"\` |
|        - |  1380 | `     "     $ret[$key] = array_merge_recursive($cur, $value);"\` |
|        - |  1381 | `     "    } elseif (is_array($cur)) {"\` |
|        - |  1382 | `     "     $ret[$key] = array_merge_recursive($cur, array($value));"\` |
|        - |  1383 | `     "    } elseif (is_array($value)) {"\` |
|        - |  1384 | `     "     $ret[$key] = array_merge_recursive(array($cur), $value);"\` |
|        - |  1385 | `     "    } else {"\` |
|        - |  1386 | `     "     $ret[$key] = array($cur, $value);"\` |
|        - |  1387 | `     "    }"\` |
|        - |  1388 | `     "   } else {"\` |
|        - |  1389 | `     "    $ret[$key] = $value;"\` |
|        - |  1390 | `     "   }"\` |
|        - |  1391 | `     "  }"\` |
|        - |  1392 | `     " }"\` |
|        - |  1393 | `	 " }"\` |
|        - |  1394 | `	 " return $ret;"\` |
|        - |  1395 | `    "}"\` |
|        - |  1396 | `	"function max(){"\` |
|        - |  1397 | `    "  $pArgs = func_get_args();"\` |
|        - |  1398 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1399 | `	"  return null;"\` |
|        - |  1400 | `    " }"\` |
|        - |  1401 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1402 | `    " $pArg = $pArgs[0];"\` |
|        - |  1403 | `	" if( !is_array($pArg) ){"\` |
|        - |  1404 | `	"   return $pArg; "\` |
|        - |  1405 | `	" }"\` |
|        - |  1406 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1407 | `	"   return null;"\` |
|        - |  1408 | `	" }"\` |
|        - |  1409 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1410 | `	" reset($pArg);"\` |
|        - |  1411 | `	" $max = current($pArg);"\` |
|        - |  1412 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1413 | `	"   if( $val > $max ){"\` |
|        - |  1414 | `	"     $max = $val;"\` |
|        - |  1415 | `    " }"\` |
|        - |  1416 | `	" }"\` |
|        - |  1417 | `	" return $max;"\` |
|        - |  1418 | `    " }"\` |
|        - |  1419 | `    " $max = $pArgs[0];"\` |
|        - |  1420 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1421 | `    " $val = $pArgs[$i];"\` |
|        - |  1422 | `	"if( $val > $max ){"\` |
|        - |  1423 | `	" $max = $val;"\` |
|        - |  1424 | `	"}"\` |
|        - |  1425 | `    " }"\` |
|        - |  1426 | `	" return $max;"\` |
|        - |  1427 | `    "}"\` |
|        - |  1428 | `	"function min(){"\` |
|        - |  1429 | `    "  $pArgs = func_get_args();"\` |
|        - |  1430 | `    " if( sizeof($pArgs) < 1 ){"\` |
|        - |  1431 | `	"  return null;"\` |
|        - |  1432 | `    " }"\` |
|        - |  1433 | `    " if( sizeof($pArgs) < 2 ){"\` |
|        - |  1434 | `    " $pArg = $pArgs[0];"\` |
|        - |  1435 | `	" if( !is_array($pArg) ){"\` |
|        - |  1436 | `	"   return $pArg; "\` |
|        - |  1437 | `	" }"\` |
|        - |  1438 | `	" if( sizeof($pArg) < 1 ){"\` |
|        - |  1439 | `	"   return null;"\` |
|        - |  1440 | `	" }"\` |
|        - |  1441 | `	" $pArg = array_copy($pArgs[0]);"\` |
|        - |  1442 | `	" reset($pArg);"\` |
|        - |  1443 | `	" $min = current($pArg);"\` |
|        - |  1444 | `	" while( FALSE !== ($val = next($pArg)) ){"\` |
|        - |  1445 | `	"   if( $val < $min ){"\` |
|        - |  1446 | `	"     $min = $val;"\` |
|        - |  1447 | `    " }"\` |
|        - |  1448 | `	" }"\` |
|        - |  1449 | `	" return $min;"\` |
|        - |  1450 | `    " }"\` |
|        - |  1451 | `    " $min = $pArgs[0];"\` |
|        - |  1452 | `    " for( $i = 1; $i < sizeof($pArgs) ; ++$i ){"\` |
|        - |  1453 | `    " $val = $pArgs[$i];"\` |
|        - |  1454 | `	"if( $val < $min ){"\` |
|        - |  1455 | `	" $min = $val;"\` |
|        - |  1456 | `	" }"\` |
|        - |  1457 | `    " }"\` |
|        - |  1458 | `	" return $min;"\` |
|        - |  1459 | `	"}"\` |
|        - |  1460 | `	"function fileowner(string $file){"\` |
|        - |  1461 | `    " $a = stat($file);"\` |
|        - |  1462 | `	" if( !is_array($a) ){"\` |
|        - |  1463 | `	"	return false;"\` |
|        - |  1464 | `	" }"\` |
|        - |  1465 | `	" return $a['uid'];"\` |
|        - |  1466 | `    "}"\` |
|        - |  1467 | `    "function filegroup(string $file){"\` |
|        - |  1468 | `	" $a = stat($file);"\` |
|        - |  1469 | `	" if( !is_array($a) ){"\` |
|        - |  1470 | `	"	return false;"\` |
|        - |  1471 | `	" }"\` |
|        - |  1472 | `	" return $a['gid'];"\` |
|        - |  1473 | `    "}"\` |
|        - |  1474 | `	 "function fileinode(string $file){"\` |
|        - |  1475 | `	" $a = stat($file);"\` |
|        - |  1476 | `	" if( !is_array($a) ){"\` |
|        - |  1477 | `	"	return false;"\` |
|        - |  1478 | `	" }"\` |
|        - |  1479 | `	" return $a['ino'];"\` |
|        - |  1480 | `    "}"` |
|        - |  1481 |  |
|        - |  1482 | `/*` |
|        - |  1483 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|        - |  1484 | ` * start compiling the target PHP program.` |
|        - |  1485 | ` */` |
|     3122 |  1486 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|        - |  1487 | `	 ph7_vm *pVm, /* Initialize this */` |
|        - |  1488 | `	 ph7 *pEngine /* Master engine */` |
|        - |  1489 | `	 )` |
|        2 |  1490 |  |
|        - |  1491 | `	SyString sBuiltin;` |
|        - |  1492 | `	ph7_value *pObj;` |
|        - |  1493 | `	sxi32 rc;` |
|        - |  1494 | `	/* Zero the structure */` |
|     3124 |  1495 | `	SyZero(pVm,sizeof(ph7_vm));` |
|        - |  1496 | `	/* Initialize VM fields */` |
|     3124 |  1497 | `	pVm->pEngine = &(*pEngine);` |
|     3124 |  1498 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|        - |  1499 | `	/* Instructions containers */` |
|     3124 |  1500 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|     3124 |  1501 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|     3124 |  1502 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|        - |  1503 | `	/* Object containers */` |
|     3124 |  1504 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3124 |  1505 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|        - |  1506 | `	/* Virtual machine internal containers */` |
|     3124 |  1507 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|     3124 |  1508 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|     3124 |  1509 | `	SyBlobInit(&pVm->sArgv,&pVm->sAllocator);` |
|     3124 |  1510 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|     3124 |  1511 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|     3124 |  1512 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,0,0);` |
|     3124 |  1513 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,0,0);` |
|     3124 |  1514 | `	SyBlobInit(&pVm->sNamespace,&pVm->sAllocator);` |
|     3124 |  1515 | `	SyHashInit(&pVm->hUseImports,&pVm->sAllocator,0,0);` |
|     3124 |  1516 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|     3124 |  1517 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|     3124 |  1518 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|     3124 |  1519 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|     3124 |  1520 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|     3124 |  1521 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|     3124 |  1522 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|     3124 |  1523 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|     3124 |  1524 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|     3124 |  1525 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|     3124 |  1526 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|     3124 |  1527 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|     3124 |  1528 | `	pVm->pPendingException = 0;` |
|        - |  1529 | `	/* Configuration containers */` |
|     3124 |  1530 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1531 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1532 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|     3124 |  1533 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|     3124 |  1534 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|     3124 |  1535 | `	pVm->iResponseStatus = 200;` |
|     3124 |  1536 | `	pVm->bHeadersSent = 0;` |
|     3124 |  1537 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|        - |  1538 | `	/* Error callbacks containers */` |
|     3124 |  1539 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[0]);` |
|     3124 |  1540 | `	PH7_MemObjInit(&(*pVm),&pVm->aExceptionCB[1]);` |
|     3124 |  1541 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[0]);` |
|     3124 |  1542 | `	PH7_MemObjInit(&(*pVm),&pVm->aErrCB[1]);` |
|     3124 |  1543 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|        - |  1544 | `	/* Set a default recursion limit */` |
|        - |  1545 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|     3124 |  1546 | `	pVm->nMaxDepth = 32;` |
|        - |  1547 | `#else` |
|        - |  1548 | `	pVm->nMaxDepth = 16;` |
|        - |  1549 | `#endif` |
|        - |  1550 | `	/* Default assertion flags */` |
|     3124 |  1551 | `	pVm->iAssertFlags = 0; /* PHP 8: no warning flag by default, AssertionError is thrown */` |
|        - |  1552 | `	/* JSON return status */` |
|     3124 |  1553 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|        - |  1554 | `	/* PRNG context */` |
|     3124 |  1555 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|        - |  1556 | `	/* Install the null constant */` |
|     3124 |  1557 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1558 | `	if( pObj == 0 ){` |
|      ! 0 |  1559 | `		rc = SXERR_MEM;` |
|      ! 0 |  1560 | `		goto Err;` |
|        - |  1561 | `	}` |
|     3124 |  1562 | `	PH7_MemObjInit(pVm,pObj);` |
|        - |  1563 | `	/* Install the boolean TRUE constant */` |
|     3124 |  1564 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1565 | `	if( pObj == 0 ){` |
|      ! 0 |  1566 | `		rc = SXERR_MEM;` |
|      ! 0 |  1567 | `		goto Err;` |
|        - |  1568 | `	}` |
|     3124 |  1569 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|        - |  1570 | `	/* Install the boolean FALSE constant */` |
|     3124 |  1571 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|     3124 |  1572 | `	if( pObj == 0 ){` |
|      ! 0 |  1573 | `		rc = SXERR_MEM;` |
|      ! 0 |  1574 | `		goto Err;` |
|        - |  1575 | `	}` |
|     3124 |  1576 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|        - |  1577 | `	/* Install a shared empty string constant so that every "" literal can` |
|        - |  1578 | `	 * reuse the same slot rather than allocating a new one.` |
|        - |  1579 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|     3124 |  1580 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|     3124 |  1581 | `	if( pObj == 0 ){` |
|      ! 0 |  1582 | `		rc = SXERR_MEM;` |
|      ! 0 |  1583 | `		goto Err;` |
|        - |  1584 | `	}` |
|     3124 |  1585 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|        - |  1586 | `	/* Create the global frame */` |
|     3124 |  1587 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|     3124 |  1588 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1589 | `		goto Err;` |
|        - |  1590 | `	}` |
|        - |  1591 | `	/* Initialize the code generator */` |
|     3124 |  1592 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3124 |  1593 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1594 | `		goto Err;` |
|        - |  1595 | `	}` |
|        - |  1596 | `	/* VM correctly initialized,set the magic number */` |
|     3124 |  1597 | `	pVm->nMagic = PH7_VM_INIT;` |
|     3124 |  1598 | `	SyStringInitFromBuf(&sBuiltin,PH7_BUILTIN_LIB,sizeof(PH7_BUILTIN_LIB)-1);` |
|        - |  1599 | `	/* Compile the built-in library */` |
|     3124 |  1600 | `	VmEvalChunk(&(*pVm),0,&sBuiltin,PH7_PHP_ONLY,FALSE);` |
|        - |  1601 | `	/* Cache the Fiber class pointer for fast dispatch */` |
|     3124 |  1602 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|        - |  1603 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|     3124 |  1604 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|     3124 |  1605 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|     3124 |  1606 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|        - |  1607 | `	/* Initialize null-coalesce-assign scratch slot */` |
|     3124 |  1608 | `	pVm->pCoalesceObj = 0;` |
|     3124 |  1609 | `	pVm->bCoalesceArmed = 0;` |
|     3124 |  1610 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|        - |  1611 | `	/* Register Fiber internal C functions */` |
|     3124 |  1612 | `	ph7_create_function(pVm,"__fiber_suspend",vm_builtin_Fiber_suspend,0);` |
|     3124 |  1613 | `	ph7_create_function(pVm,"__fiber_construct",vm_builtin_Fiber_construct,0);` |
|     3124 |  1614 | `	ph7_create_function(pVm,"__fiber_start",vm_builtin_Fiber_start,0);` |
|     3124 |  1615 | `	ph7_create_function(pVm,"__fiber_resume",vm_builtin_Fiber_resume,0);` |
|     3124 |  1616 | `	ph7_create_function(pVm,"__fiber_getReturn",vm_builtin_Fiber_getReturn,0);` |
|     3124 |  1617 | `	ph7_create_function(pVm,"__fiber_isStarted",vm_builtin_Fiber_isStarted,0);` |
|     3124 |  1618 | `	ph7_create_function(pVm,"__fiber_isRunning",vm_builtin_Fiber_isRunning,0);` |
|     3124 |  1619 | `	ph7_create_function(pVm,"__fiber_isSuspended",vm_builtin_Fiber_isSuspended,0);` |
|     3124 |  1620 | `	ph7_create_function(pVm,"__fiber_isTerminated",vm_builtin_Fiber_isTerminated,0);` |
|     3124 |  1621 | `	ph7_create_function(pVm,"__fiber_destruct",vm_builtin_Fiber_destruct,0);` |
|        - |  1622 | `	/* Cache the Generator class pointer and register generator functions */` |
|     3124 |  1623 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|     3124 |  1624 | `	ph7_create_function(pVm,"__gen_rewind",vm_builtin_Generator_rewind,0);` |
|     3124 |  1625 | `	ph7_create_function(pVm,"__gen_valid",vm_builtin_Generator_valid,0);` |
|     3124 |  1626 | `	ph7_create_function(pVm,"__gen_current",vm_builtin_Generator_current,0);` |
|     3124 |  1627 | `	ph7_create_function(pVm,"__gen_key",vm_builtin_Generator_key,0);` |
|     3124 |  1628 | `	ph7_create_function(pVm,"__gen_next",vm_builtin_Generator_next,0);` |
|     3124 |  1629 | `	ph7_create_function(pVm,"__gen_send",vm_builtin_Generator_send,0);` |
|     3124 |  1630 | `	ph7_create_function(pVm,"__gen_throw",vm_builtin_Generator_throw,0);` |
|     3124 |  1631 | `	ph7_create_function(pVm,"__gen_getReturn",vm_builtin_Generator_getReturn,0);` |
|     3124 |  1632 | `	ph7_create_function(pVm,"__gen_destruct",vm_builtin_Generator_destruct,0);` |
|        - |  1633 | `	/* Reset the code generator */` |
|     3124 |  1634 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|     3124 |  1635 | `	return SXRET_OK;` |
|      ! 0 |  1636 | `Err:` |
|      ! 0 |  1637 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      ! 0 |  1638 | `	return rc;` |
|     1563 |  1639 |  |
|        - |  1640 | `/*` |
|        - |  1641 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|        - |  1642 | ` * routine which store the output in an internal blob.` |
|        - |  1643 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|        - |  1644 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|        - |  1645 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|        - |  1646 | ` * Refer to the official docurmentation for additional information.` |
|        - |  1647 | ` * Note that for performance reason it's preferable to install a VM output` |
|        - |  1648 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|        - |  1649 | ` * to finish executing and extracting the output.` |
|        - |  1650 | ` */` |
|       38 |  1651 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|        - |  1652 | `	const void *pOut,   /* VM Generated output*/` |
|        - |  1653 | `	unsigned int nLen,  /* Generated output length */` |
|        - |  1654 | `	void *pUserData     /* User private data */` |
|        - |  1655 | `	)` |
|      ! 0 |  1656 |  |
|        - |  1657 | `	 sxi32 rc;` |
|        - |  1658 | `	 /* Store the output in an internal BLOB */` |
|       38 |  1659 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       38 |  1660 | `	 return rc;` |
|      ! 0 |  1661 |  |
|        - |  1662 | `/*` |
|        - |  1663 | ` * Track output length and mark headers as sent when output reaches` |
|        - |  1664 | ` * a real external consumer (not the internal blob or OB buffer).` |
|        - |  1665 | ` */` |
|    19468 |  1666 | `static void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|        2 |  1667 |  |
|    19470 |  1668 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    19470 |  1669 | `	if( xCons != VmObConsumer ){` |
|     8070 |  1670 | `		pVm->nOutputLen += nLen;` |
|     8070 |  1671 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|     1010 |  1672 | `			pVm->bHeadersSent = 1;` |
|      504 |  1673 | `		}` |
|     4034 |  1674 | `	}` |
|    19470 |  1675 |  |
|        - |  1676 | `#define VM_STACK_GUARD 16` |
|        - |  1677 | `/*` |
|        - |  1678 | ` * Allocate a new operand stack so that we can start executing` |
|        - |  1679 | ` * our compiled PHP program.` |
|        - |  1680 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|        - |  1681 | ` * on success. NULL (Fatal error) on failure.` |
|        - |  1682 | ` */` |
|    43670 |  1683 | `static ph7_value * VmNewOperandStack(` |
|        - |  1684 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  1685 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|        - |  1686 | `	)` |
|        2 |  1687 |  |
|        - |  1688 | `	ph7_value *pStack;` |
|        - |  1689 | `  /* No instruction ever pushes more than a single element onto the` |
|        - |  1690 | `  ** stack and the stack never grows on successive executions of the` |
|        - |  1691 | `  ** same loop. So the total number of instructions is an upper bound` |
|        - |  1692 | `  ** on the maximum stack depth required.` |
|        - |  1693 | `  **` |
|        - |  1694 | `  ** Allocation all the stack space we will ever need.` |
|        - |  1695 | `  */` |
|    43672 |  1696 | `	nInstr += VM_STACK_GUARD;` |
|    43672 |  1697 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|    43672 |  1698 | `	if( pStack == 0 ){` |
|      ! 0 |  1699 | `		return 0;` |
|        - |  1700 | `	}` |
|        - |  1701 | `	/* Initialize the operand stack */` |
|  2979538 |  1702 | `	while( nInstr > 0 ){` |
|  2935868 |  1703 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  2935868 |  1704 | `		--nInstr;` |
|        2 |  1705 | `	}` |
|        - |  1706 | `	/* Ready for bytecode execution */` |
|    43672 |  1707 | `	return pStack;` |
|    21837 |  1708 |  |
|        - |  1709 | `/* Forward declaration */` |
|        - |  1710 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|        - |  1711 | `/*` |
|        - |  1712 | ` * Prepare the Virtual Machine for byte-code execution.` |
|        - |  1713 | ` * This routine gets called by the PH7 engine after` |
|        - |  1714 | ` * successful compilation of the target PHP program.` |
|        - |  1715 | ` */` |
|     2808 |  1716 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|        - |  1717 | `	ph7_vm *pVm /* Target VM */` |
|        - |  1718 | `	)` |
|        2 |  1719 |  |
|        - |  1720 | `	SyHashEntry *pEntry;` |
|        - |  1721 | `	sxi32 rc;` |
|     2810 |  1722 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|        - |  1723 | `		/* Initialize your VM first */` |
|      ! 0 |  1724 | `		return SXERR_CORRUPT;` |
|        - |  1725 | `	}` |
|        - |  1726 | `	/* Mark the VM ready for byte-code execution */` |
|     2810 |  1727 | `	pVm->nMagic = PH7_VM_RUN;` |
|        - |  1728 | `	/* Release the code generator now we have compiled our program */` |
|     2810 |  1729 | `	PH7_ResetCodeGenerator(pVm,0,0);` |
|        - |  1730 | `	/* Emit the DONE instruction */` |
|     2810 |  1731 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|     2810 |  1732 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  1733 | `		return SXERR_MEM;` |
|        - |  1734 | `	}` |
|        - |  1735 | `	/* Script return value */` |
|     2810 |  1736 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|        - |  1737 | `	/* Allocate a new operand stack */` |
|     2810 |  1738 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|     2810 |  1739 | `	if( pVm->aOps == 0 ){` |
|      ! 0 |  1740 | `		return SXERR_MEM;` |
|        - |  1741 | `	}` |
|        - |  1742 | `	/* Set the default VM output consumer callback and it's` |
|        - |  1743 | `	 * private data. */` |
|     2810 |  1744 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|     2810 |  1745 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|        - |  1746 | `	/* Allocate the reference table */` |
|     2810 |  1747 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|     2810 |  1748 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|     2810 |  1749 | `	if( pVm->apRefObj == 0 ){` |
|        - |  1750 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1751 | `		return SXERR_MEM;` |
|        - |  1752 | `	}` |
|        - |  1753 | `	/* Zero the reference table */` |
|     2810 |  1754 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|        - |  1755 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|     2810 |  1756 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|     2810 |  1757 | `	if( rc != SXRET_OK ){` |
|        - |  1758 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1759 | `		return rc;` |
|        - |  1760 | `	}` |
|        - |  1761 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|     2810 |  1762 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|     2810 |  1763 | `	if( rc != SXRET_OK ){` |
|        - |  1764 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|      ! 0 |  1765 | `		return rc;` |
|        - |  1766 | `	}` |
|        - |  1767 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|     2810 |  1768 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|        - |  1769 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|     2810 |  1770 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|        - |  1771 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|     2810 |  1772 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|        - |  1773 | `#ifdef PH7_ENABLE_PCRE` |
|        - |  1774 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|     2810 |  1775 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|     2810 |  1776 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|        - |  1777 | `#endif` |
|        - |  1778 | `	/* Initialize and install static and constants class attributes */` |
|     2810 |  1779 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    70546 |  1780 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    67738 |  1781 | `		rc = VmMountUserClass(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    67738 |  1782 | `		if( rc != SXRET_OK ){` |
|      ! 0 |  1783 | `			return rc;` |
|        - |  1784 | `		}` |
|        2 |  1785 | `	}` |
|        - |  1786 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|     2810 |  1787 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|        - |  1788 | `	/* VM is ready for bytecode execution */` |
|     2810 |  1789 | `	return SXRET_OK;` |
|     1406 |  1790 |  |
|        - |  1791 | `/*` |
|        - |  1792 | ` * Reset a Virtual Machine to it's initial state.` |
|        - |  1793 | ` */` |
|      ! 0 |  1794 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|      ! 0 |  1795 |  |
|      ! 0 |  1796 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|      ! 0 |  1797 | `		return SXERR_CORRUPT;` |
|        - |  1798 | `	}` |
|        - |  1799 | `	/* TICKET 1433-003: As of this version, the VM is automatically reset */` |
|      ! 0 |  1800 | `	SyBlobReset(&pVm->sConsumer);` |
|      ! 0 |  1801 | `	PH7_MemObjRelease(&pVm->sExec);` |
|        - |  1802 | `	/* Reset HTTP response state (frees header strings) */` |
|      ! 0 |  1803 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|      ! 0 |  1804 | `	pVm->iResponseStatus = 200;` |
|      ! 0 |  1805 | `	pVm->bHeadersSent = 0;` |
|      ! 0 |  1806 | `	pVm->bHttpContext = 0;` |
|        - |  1807 | `	/* Set the ready flag */` |
|      ! 0 |  1808 | `	pVm->nMagic = PH7_VM_RUN;` |
|      ! 0 |  1809 | `	return SXRET_OK;` |
|      ! 0 |  1810 |  |
|        - |  1811 | `/*` |
|        - |  1812 | ` * Release a Virtual Machine.` |
|        - |  1813 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|        - |  1814 | ` */` |
|     2800 |  1815 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|        2 |  1816 |  |
|        - |  1817 | `	/* Set the stale magic number */` |
|     2802 |  1818 | `	pVm->nMagic = PH7_VM_STALE;` |
|        - |  1819 | `	/* Release the private memory subsystem */` |
|     2802 |  1820 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|     2802 |  1821 | `	return SXRET_OK;` |
|        2 |  1822 |  |
|        - |  1823 | `/*` |
|        - |  1824 | ` * Initialize a foreign function call context.` |
|        - |  1825 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|        - |  1826 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|        - |  1827 | ` * functions.` |
|        - |  1828 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|        - |  1829 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|        - |  1830 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|        - |  1831 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|        - |  1832 | ` */` |
|   683980 |  1833 | `static sxi32 VmInitCallContext(` |
|        - |  1834 | `	ph7_context *pOut,    /* Call Context */` |
|        - |  1835 | `	ph7_vm *pVm,          /* Target VM */` |
|        - |  1836 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|        - |  1837 | `	ph7_value *pRet,      /* Store return value here*/` |
|        - |  1838 | `	sxi32 iFlags          /* Control flags */` |
|        - |  1839 | `	)` |
|        2 |  1840 |  |
|   683982 |  1841 | `	pOut->pFunc = pFunc;` |
|   683982 |  1842 | `	pOut->pVm   = pVm;` |
|   683982 |  1843 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   683982 |  1844 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|        - |  1845 | `	/* Assume a null return value */` |
|   683982 |  1846 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   683982 |  1847 | `	pOut->pRet = pRet;` |
|   683982 |  1848 | `	pOut->iFlags = iFlags;` |
|   683982 |  1849 | `	return SXRET_OK;` |
|        2 |  1850 |  |
|        - |  1851 | `/*` |
|        - |  1852 | ` * Release a foreign function call context and cleanup the mess` |
|        - |  1853 | ` * left behind.` |
|        - |  1854 | ` */` |
|   683980 |  1855 | `static void VmReleaseCallContext(ph7_context *pCtx)` |
|        2 |  1856 |  |
|        - |  1857 | `	sxu32 n;` |
|   683982 |  1858 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     8366 |  1859 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|    24400 |  1860 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    16036 |  1861 | `			if( apObj[n] == 0 ){` |
|        - |  1862 | `				/* Already released */` |
|      318 |  1863 | `				continue;` |
|        - |  1864 | `			}` |
|    15720 |  1865 | `			PH7_MemObjRelease(apObj[n]);` |
|    15720 |  1866 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     7861 |  1867 | `		}` |
|     8366 |  1868 | `		SySetRelease(&pCtx->sVar);` |
|     4182 |  1869 | `	}` |
|   683982 |  1870 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|        - |  1871 | `		ph7_aux_data *aAux;` |
|        - |  1872 | `		void *pChunk;` |
|        - |  1873 | `		/* Automatic release of dynamically allocated chunk` |
|        - |  1874 | `		 * using [ph7_context_alloc_chunk()].` |
|        - |  1875 | `		 */` |
|        9 |  1876 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|       33 |  1877 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|       25 |  1878 | `			pChunk = aAux[n].pAuxData;` |
|        - |  1879 | `			/* Release the chunk */` |
|       25 |  1880 | `			if( pChunk ){` |
|       25 |  1881 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|       12 |  1882 | `			}` |
|       13 |  1883 | `		}` |
|        9 |  1884 | `		SySetRelease(&pCtx->sChunk);` |
|        4 |  1885 | `	}` |
|   683982 |  1886 |  |
|        - |  1887 | `/*` |
|        - |  1888 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|        - |  1889 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|        - |  1890 | ` */` |
|      316 |  1891 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|        - |  1892 | `	ph7_context *pCtx, /* Call context */` |
|        - |  1893 | `	ph7_value *pValue  /* Release this value */` |
|        - |  1894 | `	)` |
|        2 |  1895 |  |
|      318 |  1896 | `	if( pValue == 0 ){` |
|        - |  1897 | `		/* NULL value is a harmless operation */` |
|      ! 0 |  1898 | `		return;` |
|        - |  1899 | `	}` |
|      318 |  1900 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      318 |  1901 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|        - |  1902 | `		sxu32 n;` |
|     1116 |  1903 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     1116 |  1904 | `			if( apObj[n] == pValue ){` |
|      318 |  1905 | `				PH7_MemObjRelease(pValue);` |
|      318 |  1906 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|        - |  1907 | `				/* Mark as released */` |
|      318 |  1908 | `				apObj[n] = 0;` |
|      318 |  1909 | `				break;` |
|        - |  1910 | `			}` |
|      401 |  1911 | `		}` |
|      158 |  1912 | `	}` |
|      160 |  1913 |  |
|        - |  1914 | `/*` |
|        - |  1915 | ` * Pop and release as many memory object from the operand stack.` |
|        - |  1916 | ` */` |
|  3894402 |  1917 | `static void VmPopOperand(` |
|        - |  1918 | `	ph7_value **ppTos, /* Operand stack */` |
|        - |  1919 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|        - |  1920 | `	)` |
|        2 |  1921 |  |
|  3894404 |  1922 | `	ph7_value *pTos = *ppTos;` |
|  8292964 |  1923 | `	while( nPop > 0 ){` |
|  4398562 |  1924 | `		PH7_MemObjRelease(pTos);` |
|  4398562 |  1925 | `		pTos--;` |
|  4398562 |  1926 | `		nPop--;` |
|        2 |  1927 | `	}` |
|        - |  1928 | `	/* Top of the stack */` |
|  3894404 |  1929 | `	*ppTos = pTos;` |
|  3894404 |  1930 |  |
|        - |  1931 | `/*` |
|        - |  1932 | ` * Reserve a memory object.` |
|        - |  1933 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|        - |  1934 | ` */` |
|  3172686 |  1935 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|        2 |  1936 |  |
|  3172688 |  1937 | `	ph7_value *pObj = 0;` |
|        - |  1938 | `	VmSlot *pSlot;` |
|        - |  1939 | `	sxu32 nIdx;` |
|        - |  1940 | `	/* Check for a free slot */` |
|  3172688 |  1941 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  3172688 |  1942 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  3172688 |  1943 | `	if( pSlot ){` |
|  1021344 |  1944 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  1021344 |  1945 | `		nIdx = pSlot->nIdx;` |
|   510671 |  1946 | `	}` |
|  3172688 |  1947 | `	if( pObj == 0 ){` |
|        - |  1948 | `		/* Reserve a new memory object */` |
|  2151346 |  1949 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|  2151346 |  1950 | `		if( pObj == 0 ){` |
|      ! 0 |  1951 | `			return 0;` |
|        - |  1952 | `		}` |
|  1075672 |  1953 | `	}` |
|        - |  1954 | `	/* Set a null default value */` |
|  3172688 |  1955 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  3172688 |  1956 | `	pObj->nIdx = nIdx;` |
|  3172688 |  1957 | `	return pObj;` |
|  1586345 |  1958 |  |
|        - |  1959 | `/*` |
|        - |  1960 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|        - |  1961 | ` */` |
|    34992 |  1962 | `static sxi32 VmHashmapRefInsert(` |
|        - |  1963 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|        - |  1964 | `	const char *zKey,  /* Entry key */` |
|        - |  1965 | `	sxu32 nByte,       /* Key length */` |
|        - |  1966 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|        - |  1967 | `	)` |
|        2 |  1968 |  |
|        - |  1969 | `	ph7_value sKey;` |
|        - |  1970 | `	sxi32 rc;` |
|    34994 |  1971 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|    34994 |  1972 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|        - |  1973 | `	/* Perform the insertion */` |
|    34994 |  1974 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|    34994 |  1975 | `	PH7_MemObjRelease(&sKey);` |
|    34994 |  1976 | `	return rc;` |
|        2 |  1977 |  |
|        - |  1978 | `/*` |
|        - |  1979 | ` * Extract a variable value from the top active VM frame.` |
|        - |  1980 | ` * Return a pointer to the variable value on success.` |
|        - |  1981 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|        - |  1982 | ` */` |
|  3620056 |  1983 | `static ph7_value * VmExtractMemObj(` |
|        - |  1984 | `	ph7_vm *pVm,           /* Target VM */` |
|        - |  1985 | `	const SyString *pName, /* Variable name */` |
|        - |  1986 | `	int bDup,              /* True to duplicate variable name */` |
|        - |  1987 | `	int bCreate            /* True to create the variable if non-existent */` |
|        - |  1988 | `	)` |
|        2 |  1989 |  |
|  3620058 |  1990 | `	int bNullify = FALSE;` |
|        - |  1991 | `	SyHashEntry *pEntry;` |
|        - |  1992 | `	VmFrame *pFrame;` |
|        - |  1993 | `	ph7_value *pObj;` |
|        - |  1994 | `	sxu32 nIdx;` |
|        - |  1995 | `	sxi32 rc;` |
|        - |  1996 | `	/* Point to the top active frame */` |
|  3620058 |  1997 | `	pFrame = pVm->pFrame;` |
|  3620058 |  1998 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - |  1999 | `	/* Perform the lookup */` |
|  3620058 |  2000 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|        - |  2001 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|      ! 0 |  2002 | `		pName = &sAnnon;` |
|        - |  2003 | `		/* Always nullify the object */` |
|      ! 0 |  2004 | `		bNullify = TRUE;` |
|      ! 0 |  2005 | `		bDup = FALSE;` |
|      ! 0 |  2006 | `	}` |
|        - |  2007 | `	/* Check the superglobals table first */` |
|  3620058 |  2008 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  3620058 |  2009 | `	if( pEntry == 0 ){` |
|        - |  2010 | `		/* Query the top active frame */` |
|  3620018 |  2011 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  3620018 |  2012 | `		if( pEntry == 0 ){` |
|   111112 |  2013 | `			char *zName = (char *)pName->zString;` |
|        - |  2014 | `			VmSlot sLocal;` |
|   111112 |  2015 | `			if( !bCreate ){` |
|        - |  2016 | `				/* Do not create the variable,return NULL instead */` |
|      930 |  2017 | `				return 0;` |
|        - |  2018 | `			}` |
|        - |  2019 | `			/* No such variable,automatically create a new one and install` |
|        - |  2020 | `			 * it in the current frame.` |
|        - |  2021 | `			 */` |
|   110184 |  2022 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   110184 |  2023 | `			if( pObj == 0 ){` |
|      ! 0 |  2024 | `				return 0;` |
|        - |  2025 | `			}` |
|   110184 |  2026 | `			nIdx = pObj->nIdx;` |
|   110184 |  2027 | `			if( bDup ){` |
|        - |  2028 | `				/* Duplicate name */` |
|      196 |  2029 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      196 |  2030 | `				if( zName == 0 ){` |
|      ! 0 |  2031 | `					return 0;` |
|        - |  2032 | `				}` |
|       97 |  2033 | `			}` |
|        - |  2034 | `			/* Link to the top active VM frame */` |
|   110184 |  2035 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   110184 |  2036 | `			if( rc != SXRET_OK ){` |
|        - |  2037 | `				/* Return the slot to the free pool */` |
|      ! 0 |  2038 | `				sLocal.nIdx = nIdx;` |
|      ! 0 |  2039 | `				sLocal.pUserData = 0;` |
|      ! 0 |  2040 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|      ! 0 |  2041 | `				return 0;` |
|        - |  2042 | `			}` |
|   110184 |  2043 | `			if( pFrame->pParent != 0 ){` |
|        - |  2044 | `				/* Local variable */` |
|   103300 |  2045 | `				sLocal.nIdx = nIdx;` |
|   103300 |  2046 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    51651 |  2047 | `			}else{` |
|        - |  2048 | `				/* Register in the $GLOBALS array */` |
|     6886 |  2049 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|        - |  2050 | `			}` |
|        - |  2051 | `			/* Install in the reference table */` |
|   110184 |  2052 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|        - |  2053 | `			/* Save object index */` |
|   110184 |  2054 | `			pObj->nIdx = nIdx;` |
|    55093 |  2055 | `		}else{` |
|        - |  2056 | `			/* Extract variable contents */` |
|  3508908 |  2057 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  3508908 |  2058 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  3508908 |  2059 | `			if( bNullify && pObj ){` |
|      ! 0 |  2060 | `				PH7_MemObjRelease(pObj);` |
|      ! 0 |  2061 | `			}` |
|        - |  2062 | `		}` |
|  1809656 |  2063 | `	}else{` |
|        - |  2064 | `		/* Superglobal */` |
|       42 |  2065 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       42 |  2066 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|        - |  2067 | `	}` |
|  3619130 |  2068 | `	return pObj;` |
|  1810140 |  2069 |  |
|        - |  2070 | `/*` |
|        - |  2071 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|        - |  2072 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|        - |  2073 | ` */` |
|     3112 |  2074 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|        - |  2075 | `	ph7_vm *pVm,       /* Target VM */` |
|        - |  2076 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|        - |  2077 | `	sxu32 nByte        /* zName length */` |
|        - |  2078 | `	)` |
|        2 |  2079 |  |
|        - |  2080 | `	SyHashEntry *pEntry;` |
|        - |  2081 | `	ph7_value *pValue;` |
|        - |  2082 | `	sxu32 nIdx;` |
|        - |  2083 | `	/* Query the superglobal table */` |
|     3114 |  2084 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     3114 |  2085 | `	if( pEntry == 0 ){` |
|        - |  2086 | `		/* No such entry */` |
|      ! 0 |  2087 | `		return 0;` |
|        - |  2088 | `	}` |
|        - |  2089 | `	/* Extract the superglobal index in the global object pool */` |
|     3114 |  2090 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2091 | `	/* Extract the variable value  */` |
|     3114 |  2092 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     3114 |  2093 | `	return pValue;` |
|     1558 |  2094 |  |
|        - |  2095 | `/*` |
|        - |  2096 | ` * Perform a raw hashmap insertion.` |
|        - |  2097 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|        - |  2098 | ` */` |
|     3142 |  2099 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|        - |  2100 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|        - |  2101 | `	const char *zKey,   /* Entry key */` |
|        - |  2102 | `	int nKeylen,        /* zKey length*/` |
|        - |  2103 | `	const char *zData,  /* Entry data */` |
|        - |  2104 | `	int nLen            /* zData length */` |
|        - |  2105 | `	)` |
|        2 |  2106 |  |
|        - |  2107 | `	ph7_value sKey,sValue;` |
|        - |  2108 | `	sxi32 rc;` |
|     3144 |  2109 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     3144 |  2110 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     3144 |  2111 | `	if( zKey ){` |
|     3122 |  2112 | `		if( nKeylen < 0 ){` |
|     3070 |  2113 | `			nKeylen = (int)SyStrlen(zKey);` |
|     1534 |  2114 | `		}` |
|     3122 |  2115 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     1560 |  2116 | `	}` |
|     3144 |  2117 | `	if( zData ){` |
|     3144 |  2118 | `		if( nLen < 0 ){` |
|        - |  2119 | `			/* Compute length automatically */` |
|      144 |  2120 | `			nLen = (int)SyStrlen(zData);` |
|       72 |  2121 | `		}` |
|     3144 |  2122 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     1571 |  2123 | `	}` |
|        - |  2124 | `	/* Perform the insertion */` |
|     3144 |  2125 | `	rc = PH7_HashmapInsert(&(*pMap),&sKey,&sValue);` |
|     3144 |  2126 | `	PH7_MemObjRelease(&sKey);` |
|     3144 |  2127 | `	PH7_MemObjRelease(&sValue);` |
|     3144 |  2128 | `	return rc;` |
|        2 |  2129 |  |
|        - |  2130 | `/*` |
|        - |  2131 | ` * Configure a working virtual machine instance.` |
|        - |  2132 | ` *` |
|        - |  2133 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|        - |  2134 | ` * successful call to one of the compile interface such as ph7_compile()` |
|        - |  2135 | ` * ph7_compile_v2() or ph7_compile_file().` |
|        - |  2136 | ` * The second argument to this function is an integer configuration option` |
|        - |  2137 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|        - |  2138 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|        - |  2139 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|        - |  2140 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|        - |  2141 | ` * Refer to the official documentation for the list of allowed verbs.` |
|        - |  2142 | ` */` |
|    45258 |  2143 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|        - |  2144 | `	ph7_vm *pVm, /* Target VM */` |
|        - |  2145 | `	sxi32 nOp,   /* Configuration verb */` |
|        - |  2146 | `	va_list ap   /* Subsequent option arguments */` |
|        - |  2147 | `	)` |
|        2 |  2148 |  |
|    45260 |  2149 | `	sxi32 rc = SXRET_OK;` |
|    45260 |  2150 | `	switch(nOp){` |
|     1396 |  2151 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|     2794 |  2152 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|     2794 |  2153 | `		void *pUserData = va_arg(ap,void *);` |
|        - |  2154 | `		/* VM output consumer callback */` |
|        - |  2155 | `#ifdef UNTRUST` |
|        - |  2156 | `		if( xConsumer == 0 ){` |
|        - |  2157 | `			rc = SXERR_CORRUPT;` |
|        - |  2158 | `			break;` |
|        - |  2159 | `		}` |
|        - |  2160 | `#endif` |
|        - |  2161 | `		/* Install the output consumer */` |
|     2794 |  2162 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|     2794 |  2163 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|     2794 |  2164 | `		break;` |
|        - |  2165 | `							   }` |
|     1404 |  2166 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|        - |  2167 | `		/* Import path */` |
|        - |  2168 | `		  const char *zPath;` |
|        - |  2169 | `		  SyString sPath;` |
|     2810 |  2170 | `		  zPath = va_arg(ap,const char *);` |
|        - |  2171 | `#if defined(UNTRUST)` |
|        - |  2172 | `		  if( zPath == 0 ){` |
|        - |  2173 | `			  rc = SXERR_EMPTY;` |
|        - |  2174 | `			  break;` |
|        - |  2175 | `		  }` |
|        - |  2176 | `#endif` |
|     2810 |  2177 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|        - |  2178 | `		  /* Remove trailing slashes and backslashes */` |
|        - |  2179 | `#ifdef __WINNT__` |
|        2 |  2180 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|        - |  2181 | `#endif` |
|     5618 |  2182 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|        - |  2183 | `		  /* Remove leading and trailing white spaces */` |
|     2810 |  2184 | `		  SyStringFullTrim(&sPath);` |
|     2810 |  2185 | `		  if( sPath.nByte > 0 ){` |
|        - |  2186 | `			  /* Store the path in the corresponding conatiner */` |
|     2810 |  2187 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|     1404 |  2188 | `		  }` |
|     2810 |  2189 | `		  break;` |
|        - |  2190 | `									 }` |
|     1404 |  2191 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|        - |  2192 | `		/* Run-Time Error report */` |
|     2810 |  2193 | `		pVm->bErrReport = 1;` |
|     2810 |  2194 | `		break;` |
|      ! 0 |  2195 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|        - |  2196 | `		/* Recursion depth */` |
|      ! 0 |  2197 | `		int nDepth = va_arg(ap,int);` |
|      ! 0 |  2198 | `		if( nDepth > 2 && nDepth < 1024 ){` |
|      ! 0 |  2199 | `			pVm->nMaxDepth = nDepth;` |
|      ! 0 |  2200 | `		}` |
|      ! 0 |  2201 | `		break;` |
|        - |  2202 | `									   }` |
|      ! 0 |  2203 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|        - |  2204 | `		/* VM output length in bytes */` |
|      ! 0 |  2205 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|        - |  2206 | `#ifdef UNTRUST` |
|        - |  2207 | `		if( pOut == 0 ){` |
|        - |  2208 | `			rc = SXERR_CORRUPT;` |
|        - |  2209 | `			break;` |
|        - |  2210 | `		}` |
|        - |  2211 | `#endif` |
|      ! 0 |  2212 | `		*pOut = pVm->nOutputLen;` |
|      ! 0 |  2213 | `		break;` |
|        - |  2214 | `							   }` |
|        - |  2215 |  |
|    14040 |  2216 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|        - |  2217 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|        - |  2218 | `		/* Create a new superglobal/global variable */` |
|    28082 |  2219 | `		const char *zName = va_arg(ap,const char *);` |
|    28082 |  2220 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|        - |  2221 | `		SyHashEntry *pEntry;` |
|        - |  2222 | `		ph7_value *pObj;` |
|        - |  2223 | `		sxu32 nByte;` |
|        - |  2224 | `		sxu32 nIdx;` |
|        - |  2225 | `#ifdef UNTRUST` |
|        - |  2226 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|        - |  2227 | `			rc = SXERR_CORRUPT;` |
|        - |  2228 | `			break;` |
|        - |  2229 | `		}` |
|        - |  2230 | `#endif` |
|    28082 |  2231 | `		nByte = SyStrlen(zName);` |
|    28082 |  2232 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2233 | `			/* Check if the superglobal is already installed */` |
|    28082 |  2234 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|    14042 |  2235 | `		}else{` |
|        - |  2236 | `			/* Query the top active VM frame */` |
|      ! 0 |  2237 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|        - |  2238 | `		}` |
|    28082 |  2239 | `		if( pEntry ){` |
|        - |  2240 | `			/* Variable already installed */` |
|      ! 0 |  2241 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|        - |  2242 | `			/* Extract contents */` |
|      ! 0 |  2243 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      ! 0 |  2244 | `			if( pObj ){` |
|        - |  2245 | `				/* Overwrite old contents */` |
|      ! 0 |  2246 | `				PH7_MemObjStore(pValue,pObj);` |
|      ! 0 |  2247 | `			}` |
|      ! 0 |  2248 | `		}else{` |
|        - |  2249 | `			/* Install a new variable */` |
|    28082 |  2250 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|    28082 |  2251 | `			if( pObj == 0 ){` |
|      ! 0 |  2252 | `				rc = SXERR_MEM;` |
|      ! 0 |  2253 | `				break;` |
|        - |  2254 | `			}` |
|    28082 |  2255 | `			nIdx = pObj->nIdx;` |
|        - |  2256 | `			/* Copy value */` |
|    28082 |  2257 | `			PH7_MemObjStore(pValue,pObj);` |
|    28082 |  2258 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|        - |  2259 | `				/* Install the superglobal */` |
|    28082 |  2260 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|    14042 |  2261 | `			}else{` |
|        - |  2262 | `				/* Install in the current frame */` |
|      ! 0 |  2263 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|        - |  2264 | `			}` |
|    28082 |  2265 | `			if( rc == SXRET_OK ){` |
|        - |  2266 | `				SyHashEntry *pRef;` |
|    28082 |  2267 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|    28082 |  2268 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|    14042 |  2269 | `				}else{` |
|      ! 0 |  2270 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|        - |  2271 | `				}` |
|        - |  2272 | `				/* Install in the reference table */` |
|    28082 |  2273 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|    28082 |  2274 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|        - |  2275 | `					/* Register in the $GLOBALS array */` |
|    28082 |  2276 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|    14040 |  2277 | `				}` |
|    14040 |  2278 | `			}` |
|        - |  2279 | `		}` |
|    28082 |  2280 | `		break;` |
|        - |  2281 | `									}` |
|     1534 |  2282 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|        - |  2283 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|        - |  2284 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|        - |  2285 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|        - |  2286 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|        - |  2287 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|        - |  2288 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     3070 |  2289 | `		const char *zKey   = va_arg(ap,const char *);` |
|     3070 |  2290 | `		const char *zValue = va_arg(ap,const char *);` |
|     3070 |  2291 | `		int nLen = va_arg(ap,int);` |
|        - |  2292 | `		ph7_hashmap *pMap;` |
|        - |  2293 | `		ph7_value *pValue;` |
|     3070 |  2294 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|        - |  2295 | `			/* Extract the $_ENV superglobal */` |
|        3 |  2296 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     3069 |  2297 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|        - |  2298 | `			/* Extract the $_POST superglobal */` |
|      ! 0 |  2299 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     3068 |  2300 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|        - |  2301 | `			/* Extract the $_GET superglobal */` |
|      ! 0 |  2302 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     3068 |  2303 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|        - |  2304 | `			/* Extract the $_COOKIE superglobal */` |
|      ! 0 |  2305 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     3068 |  2306 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|        - |  2307 | `			/* Extract the $_SESSION superglobal */` |
|      ! 0 |  2308 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     3068 |  2309 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|        - |  2310 | `			/* Extract the $_HEADER superglobale */` |
|      ! 0 |  2311 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|      ! 0 |  2312 | `		}else{` |
|        - |  2313 | `			/* Extract the $_SERVER superglobal */` |
|     3068 |  2314 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|        - |  2315 | `		}` |
|     3070 |  2316 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2317 | `			/* No such entry */` |
|      ! 0 |  2318 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2319 | `			break;` |
|        - |  2320 | `		}` |
|        - |  2321 | `		/* Point to the hashmap */` |
|     3070 |  2322 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2323 | `		/* Perform the insertion */` |
|     3070 |  2324 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     3070 |  2325 | `		break;` |
|        - |  2326 | `								   }` |
|       11 |  2327 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|        - |  2328 | `		/* Script arguments */` |
|       24 |  2329 | `		const char *zValue = va_arg(ap,const char *);` |
|        - |  2330 | `		ph7_hashmap *pMap;` |
|        - |  2331 | `		ph7_value *pValue;` |
|        - |  2332 | `		sxu32 n;` |
|       24 |  2333 | `		if( SX_EMPTY_STR(zValue) ){` |
|      ! 0 |  2334 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2335 | `			break;` |
|        - |  2336 | `		}` |
|        - |  2337 | `		/* Extract the $argv array */` |
|       24 |  2338 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|       24 |  2339 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  2340 | `			/* No such entry */` |
|      ! 0 |  2341 | `			rc = SXERR_NOTFOUND;` |
|      ! 0 |  2342 | `			break;` |
|        - |  2343 | `		}` |
|        - |  2344 | `		/* Point to the hashmap */` |
|       24 |  2345 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - |  2346 | `		/* Perform the insertion */` |
|       24 |  2347 | `		n = (sxu32)SyStrlen(zValue);` |
|       24 |  2348 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|       24 |  2349 | `		if( rc == SXRET_OK ){` |
|       24 |  2350 | `			if( pMap->nEntry > 1 ){` |
|        - |  2351 | `				/* Append space separator first */` |
|       18 |  2352 | `				SyBlobAppend(&pVm->sArgv,(const void *)" ",sizeof(char));` |
|        8 |  2353 | `			}` |
|       24 |  2354 | `			SyBlobAppend(&pVm->sArgv,(const void *)zValue,n);` |
|       11 |  2355 | `		}` |
|       24 |  2356 | `		break;` |
|        - |  2357 | `								  }` |
|      ! 0 |  2358 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|        - |  2359 | `		/* error_log() consumer */` |
|      ! 0 |  2360 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|      ! 0 |  2361 | `		pVm->xErrLog = xErrLog;` |
|      ! 0 |  2362 | `		break;` |
|        - |  2363 | `										}` |
|      ! 0 |  2364 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|        - |  2365 | `		/* Script return value */` |
|      ! 0 |  2366 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|        - |  2367 | `#ifdef UNTRUST` |
|        - |  2368 | `		if( ppValue == 0 ){` |
|        - |  2369 | `			rc = SXERR_CORRUPT;` |
|        - |  2370 | `			break;` |
|        - |  2371 | `		}` |
|        - |  2372 | `#endif` |
|      ! 0 |  2373 | `		*ppValue = &pVm->sExec;` |
|      ! 0 |  2374 | `		break;` |
|        - |  2375 | `								   }` |
|     2808 |  2376 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|        - |  2377 | `		/* Register an IO stream device */` |
|     5618 |  2378 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|        - |  2379 | `		/* Make sure we are dealing with a valid IO stream */` |
|     8424 |  2380 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     5618 |  2381 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|        - |  2382 | `				/* Invalid stream */` |
|      ! 0 |  2383 | `				rc = SXERR_INVALID;` |
|      ! 0 |  2384 | `				break;` |
|        - |  2385 | `		}` |
|     5618 |  2386 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|        - |  2387 | `			/* Make the 'file://' stream the defaut stream device */` |
|     2810 |  2388 | `			pVm->pDefStream = pStream;` |
|     1404 |  2389 | `		}` |
|        - |  2390 | `		/* Insert in the appropriate container */` |
|     5618 |  2391 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     5618 |  2392 | `		break;` |
|        - |  2393 | `								  }` |
|        8 |  2394 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|        - |  2395 | `		/* Point to the VM internal output consumer buffer */` |
|       16 |  2396 | `		const void **ppOut = va_arg(ap,const void **);` |
|       16 |  2397 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|        - |  2398 | `#ifdef UNTRUST` |
|        - |  2399 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|        - |  2400 | `			rc = SXERR_CORRUPT;` |
|        - |  2401 | `			break;` |
|        - |  2402 | `		}` |
|        - |  2403 | `#endif` |
|       16 |  2404 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|       16 |  2405 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|       16 |  2406 | `		break;` |
|        - |  2407 | `									   }` |
|        8 |  2408 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|        - |  2409 | `		/* Raw HTTP request*/` |
|       16 |  2410 | `		const char *zRequest = va_arg(ap,const char *);` |
|       16 |  2411 | `		int nByte = va_arg(ap,int);` |
|       16 |  2412 | `		if( SX_EMPTY_STR(zRequest) ){` |
|      ! 0 |  2413 | `			rc = SXERR_EMPTY;` |
|      ! 0 |  2414 | `			break;` |
|        - |  2415 | `		}` |
|       16 |  2416 | `		if( nByte < 0 ){` |
|        - |  2417 | `			/* Compute length automatically */` |
|      ! 0 |  2418 | `			nByte = (int)SyStrlen(zRequest);` |
|      ! 0 |  2419 | `		}` |
|        - |  2420 | `		/* Process the request */` |
|       16 |  2421 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|        - |  2422 | `		/* Mark this VM as operating in HTTP context only on success */` |
|       16 |  2423 | `		if( rc == SXRET_OK ){` |
|       16 |  2424 | `			pVm->bHttpContext = 1;` |
|        8 |  2425 | `		}` |
|       16 |  2426 | `		break;` |
|        - |  2427 | `									}` |
|        8 |  2428 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|        - |  2429 | `		/* Extract HTTP response status code */` |
|       16 |  2430 | `		int *pStatus = va_arg(ap, int *);` |
|       16 |  2431 | `		if( pStatus ){` |
|       16 |  2432 | `			*pStatus = pVm->iResponseStatus;` |
|        8 |  2433 | `		}` |
|       16 |  2434 | `		break;` |
|        - |  2435 | `										}` |
|        8 |  2436 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|        - |  2437 | `		/* Iterate response headers via callback */` |
|        - |  2438 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|       16 |  2439 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|       16 |  2440 | `		void *pUserData = va_arg(ap, void *);` |
|       16 |  2441 | `		if( xCallback ){` |
|       16 |  2442 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|       16 |  2443 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       28 |  2444 | `			for( k = 0; k < nHdr; k++ ){` |
|       18 |  2445 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|       12 |  2446 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        6 |  2447 | `							   pUserData);` |
|       12 |  2448 | `				if( rc != PH7_OK ){` |
|      ! 0 |  2449 | `					break;` |
|        - |  2450 | `				}` |
|        6 |  2451 | `			}` |
|        8 |  2452 | `		}` |
|       16 |  2453 | `		break;` |
|        - |  2454 | `										 }` |
|      ! 0 |  2455 | `	default:` |
|        - |  2456 | `		/* Unknown configuration option */` |
|      ! 0 |  2457 | `		rc = SXERR_UNKNOWN;` |
|      ! 0 |  2458 | `		break;` |
|        - |  2459 | `	}` |
|    45260 |  2460 | `	return rc;` |
|        2 |  2461 |  |
|        - |  2462 | `/* Forward declaration */` |
|        - |  2463 | `static const char * VmInstrToString(sxi32 nOp);` |
|        - |  2464 | `/*` |
|        - |  2465 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|        - |  2466 | ` * format.` |
|        - |  2467 | ` * The dump is redirected to the given consumer callback which is responsible` |
|        - |  2468 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|        - |  2469 | ` * (STDOUT).` |
|        - |  2470 | ` */` |
|        2 |  2471 | `static sxi32 VmByteCodeDump(` |
|        - |  2472 | `	SySet *pByteCode,       /* Bytecode container */` |
|        - |  2473 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|        - |  2474 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - |  2475 | `	)` |
|        1 |  2476 |  |
|        - |  2477 | `	static const char zDump[] = {` |
|        - |  2478 | `		"====================================================\n"` |
|        - |  2479 | `		"PH7 VM Dump\n"` |
|        - |  2480 | `		"====================================================\n"` |
|        - |  2481 | `	};` |
|        - |  2482 | `	VmInstr *pInstr,*pEnd;` |
|        3 |  2483 | `	sxi32 rc = SXRET_OK;` |
|        - |  2484 | `	sxu32 n;` |
|        - |  2485 | `	/* Point to the PH7 instructions */` |
|        3 |  2486 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|        3 |  2487 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|        3 |  2488 | `	n = 0;` |
|        3 |  2489 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|        - |  2490 | `	/* Dump instructions */` |
|        7 |  2491 | `	for(;;){` |
|       15 |  2492 | `		if( pInstr >= pEnd ){` |
|        - |  2493 | `			/* No more instructions */` |
|        3 |  2494 | `			break;` |
|        - |  2495 | `		}` |
|        - |  2496 | `		/* Format and call the consumer callback */` |
|       19 |  2497 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u]\n",` |
|       12 |  2498 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|       12 |  2499 | `			SX_PTR_TO_INT(pInstr->p3),n);` |
|       13 |  2500 | `		if( rc != SXRET_OK ){` |
|        - |  2501 | `			/* Consumer routine request an operation abort */` |
|      ! 0 |  2502 | `			return rc;` |
|        - |  2503 | `		}` |
|       13 |  2504 | `		++n;` |
|       13 |  2505 | `		pInstr++; /* Next instruction in the stream */` |
|        1 |  2506 | `	}` |
|        3 |  2507 | `	return rc;` |
|        2 |  2508 |  |
|        - |  2509 | `/* Forward declaration */` |
|        - |  2510 | `static sxi32 VmUncaughtException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2511 | `static sxi32 VmThrowException(ph7_vm *pVm,ph7_class_instance *pThis);` |
|        - |  2512 | `static int VmMiniBacktrace(ph7_vm *pVm,SyBlob *pOut);` |
|        - |  2513 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen);` |
|        - |  2514 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen);` |
|        - |  2515 | `/*` |
|        - |  2516 | ` * Consume a generated run-time error message by invoking the VM output` |
|        - |  2517 | ` * consumer callback.` |
|        - |  2518 | ` */` |
|      600 |  2519 | `static sxi32 VmCallErrorHandler(ph7_vm *pVm,SyBlob *pMsg)` |
|        1 |  2520 |  |
|      601 |  2521 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      601 |  2522 | `	sxi32 rc = SXRET_OK;` |
|        - |  2523 | `	/* Append a new line */` |
|        - |  2524 | `#ifdef __WINNT__` |
|        1 |  2525 | `	SyBlobAppend(pMsg,"\r\n",sizeof("\r\n")-1);` |
|        - |  2526 | `#else` |
|      600 |  2527 | `	SyBlobAppend(pMsg,"\n",sizeof(char));` |
|        - |  2528 | `#endif` |
|        - |  2529 | `	/* Invoke the output consumer callback */` |
|      601 |  2530 | `	rc = pCons->xConsumer(SyBlobData(pMsg),SyBlobLength(pMsg),pCons->pUserData);` |
|      601 |  2531 | `	VmTrackOutput(pVm, SyBlobLength(pMsg));` |
|      601 |  2532 | `	return rc;` |
|        1 |  2533 |  |
|        - |  2534 | `/*` |
|        - |  2535 | ` * Throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2536 | ` * Refer to the implementation of [ph7_context_throw_error()] for additional` |
|        - |  2537 | ` * information.` |
|        - |  2538 | ` */` |
|      148 |  2539 | `static sxi32 VmInvokeErrorHandler(ph7_vm *pVm, sxi32 iErr, const char *zMessage, sxi32 nLen, SyString *pFile, sxi32 iLine)` |
|        2 |  2540 |  |
|      150 |  2541 | `	if( ph7_value_is_callable(&pVm->aErrCB[1]) ){` |
|        - |  2542 | `		ph7_value apArg[4];` |
|        - |  2543 | `		ph7_value *apArgPtr[4];` |
|        - |  2544 | `		ph7_value sResult;` |
|        - |  2545 | `		SyString sErr;` |
|        - |  2546 | `		/* Prepare arguments */` |
|       76 |  2547 | `		PH7_MemObjInitFromInt(pVm,&apArg[0],iErr);` |
|        - |  2548 | `			/* use explicit message length to avoid reading past buffer */` |
|       76 |  2549 | `			SyStringInitFromBuf(&sErr,zMessage,nLen);` |
|       76 |  2550 | `			PH7_MemObjInitFromString(pVm,&apArg[1],&sErr);` |
|       76 |  2551 | `		if( pFile ){` |
|       76 |  2552 | `			SyStringInitFromBuf(&sErr,pFile->zString,pFile->nByte);` |
|       76 |  2553 | `			PH7_MemObjInitFromString(pVm,&apArg[2],&sErr);` |
|       39 |  2554 | `		}else{` |
|      ! 0 |  2555 | `			PH7_MemObjInit(pVm,&apArg[2]);` |
|        - |  2556 | `		}` |
|       76 |  2557 | `		PH7_MemObjInitFromInt(pVm,&apArg[3],iLine);` |
|       76 |  2558 | `		PH7_MemObjInit(pVm,&sResult);` |
|        - |  2559 | `		/* Set up pointer array */` |
|       76 |  2560 | `		apArgPtr[0] = &apArg[0];` |
|       76 |  2561 | `		apArgPtr[1] = &apArg[1];` |
|       76 |  2562 | `		apArgPtr[2] = &apArg[2];` |
|       76 |  2563 | `		apArgPtr[3] = &apArg[3];` |
|        - |  2564 | `		/* Call the handler */` |
|       76 |  2565 | `		PH7_VmCallUserFunction(pVm,&pVm->aErrCB[1],4,apArgPtr,&sResult);` |
|        - |  2566 | `		/* Check return value */` |
|       76 |  2567 | `		if( (sResult.iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  2568 | `			PH7_MemObjToBool(&sResult);` |
|      ! 0 |  2569 | `		}` |
|        - |  2570 | `		/* Release */` |
|       76 |  2571 | `		PH7_MemObjRelease(&apArg[0]);` |
|       76 |  2572 | `		PH7_MemObjRelease(&apArg[1]);` |
|       76 |  2573 | `		PH7_MemObjRelease(&apArg[2]);` |
|       76 |  2574 | `		PH7_MemObjRelease(&apArg[3]);` |
|       76 |  2575 | `		PH7_MemObjRelease(&sResult);` |
|        - |  2576 | `		/* Return TRUE  (proceed to report error) if handler returned FALSE (he's reporting he couldn't catch the error)` |
|        - |  2577 | `		          FALSE (proceed to omit error)   if handler returned TRUE (he's reporting he caught the error) */` |
|       76 |  2578 | `		return sResult.x.iVal == 0 ? TRUE : FALSE;` |
|        - |  2579 | `	}` |
|        - |  2580 | `	/* No handler, always call error handler */` |
|       75 |  2581 | `	return TRUE;` |
|       76 |  2582 |  |
|      110 |  2583 | `PH7_PRIVATE sxi32 PH7_VmThrowError(` |
|        - |  2584 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2585 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2586 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice]*/` |
|        - |  2587 | `	const char *zMessage /* Null terminated error message */` |
|        - |  2588 | `	)` |
|        2 |  2589 |  |
|      112 |  2590 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2591 | `	SyString *pFile;` |
|        - |  2592 | `	char *zErr;` |
|      112 |  2593 | `	sxi32 rc = SXRET_OK;` |
|      112 |  2594 | `	if( !pVm->bErrReport ){` |
|        - |  2595 | `		/* Don't bother reporting errors */` |
|        3 |  2596 | `		return SXRET_OK;` |
|        - |  2597 | `	}` |
|        - |  2598 | `	/* Reset the working buffer */` |
|      110 |  2599 | `	SyBlobReset(pWorker);` |
|        - |  2600 | `	/* Peek the processed file if available */` |
|      110 |  2601 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      110 |  2602 | `	if( pFile ){` |
|        - |  2603 | `		/* Append file name */` |
|      110 |  2604 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|      110 |  2605 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       54 |  2606 | `	}` |
|        - |  2607 | `	/* Default prefix is "Error:".  Only the built-in warning/notice` |
|        - |  2608 | `	 * severities adjust the textual prefix.  Do not modify the raw error` |
|        - |  2609 | `	 * code; user handlers rely on seeing the original value (e.g. 8192 for` |
|        - |  2610 | `	 * E_DEPRECATED). */` |
|      110 |  2611 | `	zErr = "Error:  ";` |
|      110 |  2612 | `	switch(iErr){` |
|       19 |  2613 | `	case PH7_CTX_WARNING:` |
|       40 |  2614 | `		zErr = "Warning:  ";` |
|       40 |  2615 | `		break;` |
|        6 |  2616 | `	case PH7_CTX_NOTICE:` |
|       14 |  2617 | `		zErr = "Notice:  ";` |
|       12 |  2618 | `		break;` |
|       29 |  2619 | `	default:` |
|        - |  2620 | `		/* keep iErr unchanged */` |
|       58 |  2621 | `		break;` |
|        - |  2622 | `	}` |
|      110 |  2623 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|      110 |  2624 | `	if( pFuncName ){` |
|        - |  2625 | `		/* Append function name first */` |
|       23 |  2626 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       23 |  2627 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       11 |  2628 | `	}` |
|      110 |  2629 | `	SyBlobAppend(pWorker,zMessage,SyStrlen(zMessage));` |
|        - |  2630 | `	/* Check for user error handler.  compute length of C string */` |
|      110 |  2631 | `	if( VmInvokeErrorHandler(pVm, iErr, zMessage, (sxi32)SyStrlen(zMessage), pFile, 0) ){` |
|       49 |  2632 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       24 |  2633 | `	}` |
|      110 |  2634 | `	return rc;` |
|       57 |  2635 |  |
|        - |  2636 | `/*` |
|        - |  2637 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  2638 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  2639 | ` * information.` |
|        - |  2640 | ` */` |
|       40 |  2641 | `static sxi32 VmThrowErrorAp(` |
|        - |  2642 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  2643 | `	SyString *pFuncName, /* Function name. NULL otherwise */` |
|        - |  2644 | `	sxi32 iErr,          /* Severity level: [i.e: Error,Warning or Notice] */` |
|        - |  2645 | `	const char *zFormat, /* Format message */` |
|        - |  2646 | `	va_list ap           /* Variable list of arguments */` |
|        - |  2647 | `	)` |
|        2 |  2648 |  |
|       42 |  2649 | `	SyBlob *pWorker = &pVm->sWorker;` |
|        - |  2650 | `	SyBlob sMsg;` |
|        - |  2651 | `	SyString *pFile;` |
|        - |  2652 | `	char *zErr;` |
|       42 |  2653 | `	sxi32 rc = SXRET_OK;` |
|       42 |  2654 | `	if( !pVm->bErrReport ){` |
|        - |  2655 | `		/* Don't bother reporting errors */` |
|      ! 0 |  2656 | `		return SXRET_OK;` |
|        - |  2657 | `	}` |
|        - |  2658 | `	/* Reset the working buffer */` |
|       42 |  2659 | `	SyBlobReset(pWorker);` |
|        - |  2660 | `	/* Peek the processed file if available */` |
|       42 |  2661 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       42 |  2662 | `	if( pFile ){` |
|        - |  2663 | `		/* Append file name */` |
|       42 |  2664 | `		SyBlobAppend(pWorker,pFile->zString,pFile->nByte);` |
|       42 |  2665 | `		SyBlobAppend(pWorker,(const void *)" ",sizeof(char));` |
|       20 |  2666 | `	}` |
|        - |  2667 | `	/* Default prefix is "Error:".  Only WARNING/NOTICE use a special` |
|        - |  2668 | `	 * prefix; leave other error codes untouched so the handler receives` |
|        - |  2669 | `	 * the correct errno value. */` |
|       42 |  2670 | `	zErr = "Error:  ";` |
|       42 |  2671 | `	switch(iErr){` |
|        4 |  2672 | `	case PH7_CTX_WARNING:` |
|        9 |  2673 | `		zErr = "Warning:  ";` |
|        9 |  2674 | `		break;` |
|        3 |  2675 | `	case PH7_CTX_NOTICE:` |
|        7 |  2676 | `		zErr = "Notice:  ";` |
|        6 |  2677 | `		break;` |
|       13 |  2678 | `	default:` |
|        - |  2679 | `		/* do not change iErr */` |
|       26 |  2680 | `		break;` |
|        - |  2681 | `	}` |
|       42 |  2682 | `	SyBlobAppend(pWorker,zErr,SyStrlen(zErr));` |
|       42 |  2683 | `	if( pFuncName ){` |
|        - |  2684 | `		/* Append function name first */` |
|       26 |  2685 | `		SyBlobAppend(pWorker,pFuncName->zString,pFuncName->nByte);` |
|       26 |  2686 | `		SyBlobAppend(pWorker,"(): ",sizeof("(): ")-1);` |
|       12 |  2687 | `	}` |
|        - |  2688 | `	/* Format the raw message */` |
|       42 |  2689 | `	SyBlobInit(&sMsg, &pVm->sAllocator);` |
|       42 |  2690 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|        - |  2691 | `	/* Check if a user error handler is installed */` |
|       42 |  2692 | `	if( VmInvokeErrorHandler(pVm, iErr, (const char *)SyBlobData(&sMsg), (sxi32)SyBlobLength(&sMsg), pFile, 0) ){` |
|        - |  2693 | `		/* No handler or handler returned TRUE, normal processing */` |
|       27 |  2694 | `		SyBlobAppend(pWorker,SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       27 |  2695 | `		rc = VmCallErrorHandler(&(*pVm),pWorker);` |
|       13 |  2696 | `	}` |
|       42 |  2697 | `	SyBlobRelease(&sMsg);` |
|       42 |  2698 | `	return rc;` |
|       22 |  2699 |  |
|        - |  2700 | `/*` |
|        - |  2701 | ` * Throw a PHP-compatible TypeError whose message describes a failed typed` |
|        - |  2702 | ` * property assignment. Called from the STORE path when coercion is not` |
|        - |  2703 | ` * possible.` |
|        - |  2704 | ` */` |
|       38 |  2705 | `static sxi32 VmThrowPropertyTypeError(ph7_vm *pVm,VmClassAttr *pVmAttr,const char *zGiven)` |
|        1 |  2706 |  |
|        - |  2707 | `	ph7_class *pClass;` |
|       39 |  2708 | `	ph7_class_attr *pAttr = pVmAttr->pAttr;` |
|        - |  2709 | `	ph7_class_instance *pThis;` |
|        - |  2710 | `	ph7_class_method *pCons;` |
|        - |  2711 | `	ph7_value sArg;` |
|        - |  2712 | `	ph7_value *apArg[1];` |
|        - |  2713 | `	SyBlob sMsg;` |
|        - |  2714 | `	SyString sMsgStr;` |
|        - |  2715 | `	VmFrame *pFrame;` |
|        - |  2716 | `	sxi32 rc;` |
|       39 |  2717 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       39 |  2718 | `	if( pClass == 0 ){` |
|      ! 0 |  2719 | `		return PH7_ABORT;` |
|        - |  2720 | `	}` |
|       39 |  2721 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       39 |  2722 | `	if( pThis == 0 ){` |
|      ! 0 |  2723 | `		return PH7_ABORT;` |
|        - |  2724 | `	}` |
|       39 |  2725 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2726 | `	/* Prefer the declaring class over the runtime instance class so that an` |
|        - |  2727 | `	 * inherited typed property reports its original owner, matching PHP. */` |
|        - |  2728 | `	{` |
|       39 |  2729 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pVmAttr->pOwner;` |
|       39 |  2730 | `		if( pOwner ){` |
|       39 |  2731 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property %z::$%z of type %z",` |
|       19 |  2732 | `				zGiven,&pOwner->sName,&pAttr->sName,&pAttr->sTypeName);` |
|       20 |  2733 | `		}else{` |
|      ! 0 |  2734 | `			SyBlobFormat(&sMsg,"Cannot assign %s to property $%z of type %z",` |
|      ! 0 |  2735 | `				zGiven,&pAttr->sName,&pAttr->sTypeName);` |
|        - |  2736 | `		}` |
|        - |  2737 | `	}` |
|       39 |  2738 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       39 |  2739 | `	if( pCons ){` |
|       39 |  2740 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       39 |  2741 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       39 |  2742 | `		apArg[0] = &sArg;` |
|       39 |  2743 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       39 |  2744 | `		PH7_MemObjRelease(&sArg);` |
|       19 |  2745 | `	}` |
|       39 |  2746 | `	SyBlobRelease(&sMsg);` |
|       39 |  2747 | `	pFrame = pVm->pFrame;` |
|       39 |  2748 | `	if( pFrame ){` |
|       39 |  2749 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       39 |  2750 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       19 |  2751 | `	}` |
|       39 |  2752 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       39 |  2753 | `	PH7_ClassInstanceUnref(pThis);` |
|       39 |  2754 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2755 | `		return PH7_ABORT;` |
|        - |  2756 | `	}` |
|       39 |  2757 | `	return PH7_EXCEPTION;` |
|       20 |  2758 |  |
|        - |  2759 |  |
|        - |  2760 | `/*` |
|        - |  2761 | ` * Throw a PHP-compatible Error for reading an uninitialized typed property.` |
|        - |  2762 | ` */` |
|        4 |  2763 | `static sxi32 VmThrowUninitializedPropertyError(ph7_vm *pVm,ph7_class *pClass,ph7_class_attr *pAttr)` |
|        1 |  2764 |  |
|        - |  2765 | `	ph7_class *pErrClass;` |
|        - |  2766 | `	ph7_class_instance *pThis;` |
|        - |  2767 | `	ph7_class_method *pCons;` |
|        - |  2768 | `	ph7_value sArg;` |
|        - |  2769 | `	ph7_value *apArg[1];` |
|        - |  2770 | `	SyBlob sMsg;` |
|        - |  2771 | `	SyString sMsgStr;` |
|        - |  2772 | `	VmFrame *pFrame;` |
|        - |  2773 | `	sxi32 rc;` |
|        5 |  2774 | `	pErrClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        5 |  2775 | `	if( pErrClass == 0 ){` |
|      ! 0 |  2776 | `		return PH7_ABORT;` |
|        - |  2777 | `	}` |
|        5 |  2778 | `	pThis = PH7_NewClassInstance(&(*pVm),pErrClass);` |
|        5 |  2779 | `	if( pThis == 0 ){` |
|      ! 0 |  2780 | `		return PH7_ABORT;` |
|        - |  2781 | `	}` |
|        5 |  2782 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  2783 | `	{` |
|        5 |  2784 | `		ph7_class *pOwner = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|        5 |  2785 | `		const char *zKind = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? "static property" : "property";` |
|        5 |  2786 | `		SyBlobFormat(&sMsg,"Typed %s %z::$%z must not be accessed before initialization",` |
|        2 |  2787 | `			zKind,&pOwner->sName,&pAttr->sName);` |
|        - |  2788 | `	}` |
|        5 |  2789 | `	pCons = PH7_ClassExtractMethod(pErrClass,"__construct",sizeof("__construct")-1);` |
|        5 |  2790 | `	if( pCons ){` |
|        5 |  2791 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        5 |  2792 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        5 |  2793 | `		apArg[0] = &sArg;` |
|        5 |  2794 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        5 |  2795 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  2796 | `	}` |
|        5 |  2797 | `	SyBlobRelease(&sMsg);` |
|        5 |  2798 | `	pFrame = pVm->pFrame;` |
|        5 |  2799 | `	if( pFrame ){` |
|        5 |  2800 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  2801 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  2802 | `	}` |
|        5 |  2803 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        5 |  2804 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  2805 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  2806 | `		return PH7_ABORT;` |
|        - |  2807 | `	}` |
|        5 |  2808 | `	return PH7_EXCEPTION;` |
|        3 |  2809 |  |
|        - |  2810 |  |
|        - |  2811 | `/*` |
|        - |  2812 | ` * Enforce a typed-property assignment. On entry pValue holds the incoming` |
|        - |  2813 | ` * value. For scalar types it may be coerced in place (PHP 7.4 weak mode).` |
|        - |  2814 | ` * For class types, instanceof is verified.` |
|        - |  2815 | ` *` |
|        - |  2816 | ` * Returns SXRET_OK on success (value may have been coerced), PH7_EXCEPTION` |
|        - |  2817 | ` * after throwing TypeError, or PH7_ABORT on fatal error.` |
|        - |  2818 | ` */` |
|        - |  2819 | `/*` |
|        - |  2820 | ` * PHP-strict numeric-string check used by typed-property enforcement.` |
|        - |  2821 | ` * Returns TRUE only if the entire string (optionally surrounded by` |
|        - |  2822 | ` * whitespace, with optional sign) is a valid numeric literal. Unlike the` |
|        - |  2823 | ` * permissive is_numeric() implementation which accepts leading-numeric` |
|        - |  2824 | ` * strings like "43x", this mirrors PHP's rules for coercing to int/float.` |
|        - |  2825 | ` */` |
|       20 |  2826 | `static int VmStringIsStrictNumeric(ph7_value *pValue)` |
|        2 |  2827 |  |
|        - |  2828 | `	const char *z, *zEnd, *zTail;` |
|        - |  2829 | `	sxu32 n;` |
|        - |  2830 | `	sxu8 bReal;` |
|        - |  2831 | `	sxi32 rc;` |
|       22 |  2832 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2833 | `		return 0;` |
|        - |  2834 | `	}` |
|       22 |  2835 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       22 |  2836 | `	n = SyBlobLength(&pValue->sBlob);` |
|       22 |  2837 | `	zEnd = z + n;` |
|       22 |  2838 | `	if( n == 0 ){` |
|      ! 0 |  2839 | `		return 0;` |
|        - |  2840 | `	}` |
|       22 |  2841 | `	zTail = 0;` |
|       22 |  2842 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       22 |  2843 | `	if( rc != SXRET_OK \|\| zTail == 0 ){` |
|        7 |  2844 | `		return 0;` |
|        - |  2845 | `	}` |
|        - |  2846 | `	/* Trailing whitespace is allowed by PHP, trailing anything else is not. */` |
|       16 |  2847 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ){` |
|      ! 0 |  2848 | `		zTail++;` |
|      ! 0 |  2849 | `	}` |
|       16 |  2850 | `	return zTail == zEnd ? 1 : 0;` |
|       12 |  2851 |  |
|        - |  2852 |  |
|        - |  2853 | `/*` |
|        - |  2854 | ` * Numeric-string classification used by union weak-mode coercion. Returns:` |
|        - |  2855 | ` *   1 if the string is a strictly-numeric integer (no fraction, no exponent)` |
|        - |  2856 | ` *   2 if it's strictly numeric with a fractional/exponent part (i.e. float)` |
|        - |  2857 | ` *   0 if it's not strictly numeric.` |
|        - |  2858 | ` */` |
|       16 |  2859 | `static int VmStringNumericKind(ph7_value *pValue)` |
|        2 |  2860 |  |
|        - |  2861 | `	const char *z, *zEnd, *zTail;` |
|        - |  2862 | `	sxu32 n;` |
|       18 |  2863 | `	sxu8 bReal = 0;` |
|        - |  2864 | `	sxi32 rc;` |
|       18 |  2865 | `	if( (pValue->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  2866 | `		return 0;` |
|        - |  2867 | `	}` |
|       18 |  2868 | `	z = (const char *)SyBlobData(&pValue->sBlob);` |
|       18 |  2869 | `	n = SyBlobLength(&pValue->sBlob);` |
|       18 |  2870 | `	zEnd = z + n;` |
|       18 |  2871 | `	if( n == 0 ) return 0;` |
|       18 |  2872 | `	zTail = 0;` |
|       18 |  2873 | `	rc = SyStrIsNumeric(z,n,&bReal,&zTail);` |
|       18 |  2874 | `	if( rc != SXRET_OK \|\| zTail == 0 ) return 0;` |
|       19 |  2875 | `	while( zTail < zEnd && SyisSpace(zTail[0]) ) zTail++;` |
|       15 |  2876 | `	if( zTail != zEnd ) return 0;` |
|       15 |  2877 | `	return bReal ? 2 : 1;` |
|       10 |  2878 |  |
|        - |  2879 |  |
|        - |  2880 | `/*` |
|        - |  2881 | ` * Try to coerce *pValue* to fit one of the alternatives in *pAlts*. When` |
|        - |  2882 | ` * *bStrict* is zero this applies PHP 8 weak-mode union semantics (permissive` |
|        - |  2883 | ` * scalar coercion). When bStrict is non-zero, only exact type matches are` |
|        - |  2884 | ` * accepted, plus the single implicit widening int -> float (so an int value` |
|        - |  2885 | `` * against a `float\|X` union succeeds; string -> int does not).`` |
|        - |  2886 | ` * Returns SXRET_OK on accept (pValue may have been mutated by the cast),` |
|        - |  2887 | ` * SXERR_INVALID on reject. Caller is responsible for the actual TypeError` |
|        - |  2888 | ` * throw.` |
|        - |  2889 | ` *` |
|        - |  2890 | ` * The class match for object values consults the active VM self-stack to` |
|        - |  2891 | `` * resolve `self`/`parent` aliases when present.`` |
|        - |  2892 | ` */` |
|       98 |  2893 | `static sxi32 VmCoerceToUnion(ph7_vm *pVm, ph7_value *pValue, SySet *pAlts, int bNullable, int bStrict)` |
|        2 |  2894 |  |
|        - |  2895 | `	sxu32 i;` |
|        - |  2896 | `	ph7_type_alt *aAlts;` |
|        - |  2897 | `	int bHasArray, bHasObjAlt, bHasClassAlt;` |
|        - |  2898 | `	int bHasInt, bHasFloat, bHasString, bHasBool;` |
|      100 |  2899 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  2900 | `		return bNullable ? SXRET_OK : SXERR_INVALID;` |
|        - |  2901 | `	}` |
|       88 |  2902 | `	aAlts = (ph7_type_alt *)SySetBasePtr(pAlts);` |
|       88 |  2903 | `	bHasArray = bHasObjAlt = bHasClassAlt = 0;` |
|       88 |  2904 | `	bHasInt = bHasFloat = bHasString = bHasBool = 0;` |
|      260 |  2905 | `	for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|      174 |  2906 | `		if( aAlts[i].nType == SXU32_HIGH ) bHasClassAlt = 1;` |
|      150 |  2907 | `		else if( aAlts[i].nType == MEMOBJ_OBJ ) bHasObjAlt = 1;` |
|      150 |  2908 | `		else if( aAlts[i].nType == MEMOBJ_HASHMAP ) bHasArray = 1;` |
|      150 |  2909 | `		else if( aAlts[i].nType == MEMOBJ_INT ) bHasInt = 1;` |
|       76 |  2910 | `		else if( aAlts[i].nType == MEMOBJ_REAL ) bHasFloat = 1;` |
|       48 |  2911 | `		else if( aAlts[i].nType == MEMOBJ_STRING ) bHasString = 1;` |
|      ! 0 |  2912 | `		else if( aAlts[i].nType == MEMOBJ_BOOL ) bHasBool = 1;` |
|       88 |  2913 | `	}` |
|        - |  2914 | `	/* Object handling */` |
|       88 |  2915 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       18 |  2916 | `		if( bHasObjAlt ) return SXRET_OK;` |
|       18 |  2917 | `		if( bHasClassAlt ){` |
|       14 |  2918 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       14 |  2919 | `			ph7_class *pSelfNow = 0;` |
|       14 |  2920 | `			if( SySetUsed(&pVm->aSelf) > 0 ){` |
|      ! 0 |  2921 | `				ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|      ! 0 |  2922 | `				pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|      ! 0 |  2923 | `			}` |
|       26 |  2924 | `			for( i = 0; i < SySetUsed(pAlts); i++ ){` |
|        - |  2925 | `				ph7_class *pExpected;` |
|        - |  2926 | `				SyString *pCN;` |
|       22 |  2927 | `				if( aAlts[i].nType != SXU32_HIGH ) continue;` |
|       22 |  2928 | `				pCN = &aAlts[i].sClass;` |
|       22 |  2929 | `				if( pCN->nByte == 4 && SyMemcmp(pCN->zString,"self",4) == 0 ){` |
|      ! 0 |  2930 | `					pExpected = pSelfNow;` |
|       22 |  2931 | `				}else if( pCN->nByte == 6 && SyMemcmp(pCN->zString,"parent",6) == 0 ){` |
|      ! 0 |  2932 | `					pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  2933 | `				}else{` |
|       22 |  2934 | `					pExpected = PH7_VmExtractClass(pVm,pCN->zString,pCN->nByte,TRUE,0);` |
|        - |  2935 | `				}` |
|       22 |  2936 | `				if( pExpected && PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        9 |  2937 | `					return SXRET_OK;` |
|        - |  2938 | `				}` |
|        8 |  2939 | `			}` |
|        2 |  2940 | `		}` |
|        9 |  2941 | `		return SXERR_INVALID;` |
|        - |  2942 | `	}` |
|        - |  2943 | `	/* Array handling */` |
|       72 |  2944 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        7 |  2945 | `		return bHasArray ? SXRET_OK : SXERR_INVALID;` |
|        - |  2946 | `	}` |
|        - |  2947 | `	/* Scalar handling — exact match first */` |
|       66 |  2948 | `	if( pValue->iFlags & MEMOBJ_INT ){` |
|       26 |  2949 | `		if( bHasInt ) return SXRET_OK;` |
|      ! 0 |  2950 | `	}` |
|       42 |  2951 | `	if( pValue->iFlags & MEMOBJ_REAL ){` |
|        5 |  2952 | `		if( bHasFloat ) return SXRET_OK;` |
|      ! 0 |  2953 | `	}` |
|       38 |  2954 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       38 |  2955 | `		if( bHasString ) return SXRET_OK;` |
|        8 |  2956 | `	}` |
|       18 |  2957 | `	if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2958 | `		if( bHasBool ) return SXRET_OK;` |
|      ! 0 |  2959 | `	}` |
|       18 |  2960 | `	if( bStrict ){` |
|        - |  2961 | `		/* Strict mode: only int -> float widening is allowed implicitly. */` |
|      ! 0 |  2962 | `		if( (pValue->iFlags & MEMOBJ_INT) && bHasFloat ){` |
|      ! 0 |  2963 | `			PH7_MemObjToReal(pValue);` |
|      ! 0 |  2964 | `			return SXRET_OK;` |
|        - |  2965 | `		}` |
|      ! 0 |  2966 | `		return SXERR_INVALID;` |
|        - |  2967 | `	}` |
|        - |  2968 | `	/* Weak coercion preference order: int > float > string > bool.` |
|        - |  2969 | `	 * Numeric-string handling distinguishes integer-shaped from float-shaped` |
|        - |  2970 | `	 * to match PHP's union RFC. */` |
|        - |  2971 | `	{` |
|       18 |  2972 | `		int kind = VmStringNumericKind(pValue);` |
|       18 |  2973 | `		if( bHasInt ){` |
|        - |  2974 | `			/* int target accepts: bool, int (already exact), float w/o fraction,` |
|        - |  2975 | `			 * numeric-string-int. Float→int with fraction loses info → skip. */` |
|       18 |  2976 | `			if( pValue->iFlags & MEMOBJ_BOOL ){` |
|      ! 0 |  2977 | `				PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2978 | `				return SXRET_OK;` |
|        - |  2979 | `			}` |
|       18 |  2980 | `			if( pValue->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  2981 | `				ph7_real r = pValue->rVal;` |
|      ! 0 |  2982 | `				if( r == (ph7_real)(sxi64)r ){` |
|      ! 0 |  2983 | `					PH7_MemObjToInteger(pValue);` |
|      ! 0 |  2984 | `					return SXRET_OK;` |
|        - |  2985 | `				}` |
|      ! 0 |  2986 | `			}` |
|       18 |  2987 | `			if( kind == 1 ){` |
|        9 |  2988 | `				PH7_MemObjToInteger(pValue);` |
|        9 |  2989 | `				return SXRET_OK;` |
|        - |  2990 | `			}` |
|        4 |  2991 | `		}` |
|       10 |  2992 | `		if( bHasFloat ){` |
|       10 |  2993 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT) ){` |
|      ! 0 |  2994 | `				PH7_MemObjToReal(pValue);` |
|      ! 0 |  2995 | `				return SXRET_OK;` |
|        - |  2996 | `			}` |
|       10 |  2997 | `			if( kind == 1 \|\| kind == 2 ){` |
|        7 |  2998 | `				PH7_MemObjToReal(pValue);` |
|        7 |  2999 | `				return SXRET_OK;` |
|        - |  3000 | `			}` |
|        1 |  3001 | `		}` |
|        3 |  3002 | `		if( bHasString ){` |
|      ! 0 |  3003 | `			if( pValue->iFlags & (MEMOBJ_BOOL\|MEMOBJ_INT\|MEMOBJ_REAL) ){` |
|      ! 0 |  3004 | `				PH7_MemObjToString(pValue);` |
|      ! 0 |  3005 | `				return SXRET_OK;` |
|        - |  3006 | `			}` |
|      ! 0 |  3007 | `		}` |
|        3 |  3008 | `		if( bHasBool ){` |
|      ! 0 |  3009 | `			if( pValue->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING) ){` |
|      ! 0 |  3010 | `				PH7_MemObjToBool(pValue);` |
|      ! 0 |  3011 | `				return SXRET_OK;` |
|        - |  3012 | `			}` |
|      ! 0 |  3013 | `		}` |
|        - |  3014 | `	}` |
|        3 |  3015 | `	return SXERR_INVALID;` |
|       51 |  3016 |  |
|        - |  3017 |  |
|        - |  3018 | `/*` |
|        - |  3019 | ` * Enforce a scalar type hint on a single argument/return value under the` |
|        - |  3020 | ` * current strict-types mode. Pre: *pVal* does not already match *nType*,` |
|        - |  3021 | ` * and *nType* is a scalar MEMOBJ_* flag (not SXU32_HIGH, not MEMOBJ_OBJ).` |
|        - |  3022 | ` * Returns SXRET_OK after coercion/widening, or SXERR_INVALID if strict` |
|        - |  3023 | ` * mode rejects the value. Callers throw the TypeError on rejection.` |
|        - |  3024 | ` */` |
|       34 |  3025 | `static sxi32 VmEnforceScalarType(ph7_value *pVal, sxu32 nType, int bStrict)` |
|        2 |  3026 |  |
|       36 |  3027 | `	if( bStrict ){` |
|        - |  3028 | `		/* Only int -> float widening is allowed implicitly. */` |
|       10 |  3029 | `		if( nType == MEMOBJ_REAL && (pVal->iFlags & MEMOBJ_INT) ){` |
|        3 |  3030 | `			PH7_MemObjToReal(pVal);` |
|        3 |  3031 | `			return SXRET_OK;` |
|        - |  3032 | `		}` |
|        7 |  3033 | `		return SXERR_INVALID;` |
|        - |  3034 | `	}` |
|        - |  3035 | `	{` |
|       28 |  3036 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(nType);` |
|       28 |  3037 | `		if( xCast ) xCast(pVal);` |
|        - |  3038 | `	}` |
|       28 |  3039 | `	return SXRET_OK;` |
|       19 |  3040 |  |
|        - |  3041 |  |
|        - |  3042 | `/*` |
|        - |  3043 | ` * Render a scalar-type name suitable for the "Argument ... must be of type X"` |
|        - |  3044 | ` * TypeError message. Prefers the declared textual form when available.` |
|        - |  3045 | ` *` |
|        - |  3046 | ` * The declared SyString is length-delimited, not necessarily NUL-terminated,` |
|        - |  3047 | ` * so we bounded-copy it into the caller's *zBuf* before returning it as a` |
|        - |  3048 | ` * C string safe for "%s" formatting. If no declared text is present we fall` |
|        - |  3049 | ` * back to a static literal and ignore zBuf entirely.` |
|        - |  3050 | ` */` |
|        8 |  3051 | `static const char *VmScalarTypeName(sxu32 nType, SyString *pDeclared, char *zBuf, sxu32 nBuf)` |
|        1 |  3052 |  |
|        9 |  3053 | `	if( pDeclared && SyStringLength(pDeclared) > 0 && zBuf && nBuf > 0 ){` |
|        9 |  3054 | `		sxu32 nCopy = SyStringLength(pDeclared);` |
|        9 |  3055 | `		if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|        9 |  3056 | `		if( pDeclared->zString && nCopy > 0 ){` |
|        9 |  3057 | `			SyMemcpy(pDeclared->zString, zBuf, nCopy);` |
|        4 |  3058 | `		}` |
|        9 |  3059 | `		zBuf[nCopy] = 0;` |
|        9 |  3060 | `		return zBuf;` |
|        - |  3061 | `	}` |
|      ! 0 |  3062 | `	switch( nType ){` |
|      ! 0 |  3063 | `		case MEMOBJ_INT:     return "int";` |
|      ! 0 |  3064 | `		case MEMOBJ_REAL:    return "float";` |
|      ! 0 |  3065 | `		case MEMOBJ_STRING:  return "string";` |
|      ! 0 |  3066 | `		case MEMOBJ_BOOL:    return "bool";` |
|      ! 0 |  3067 | `		case MEMOBJ_HASHMAP: return "array";` |
|      ! 0 |  3068 | `		case MEMOBJ_OBJ:     return "object";` |
|      ! 0 |  3069 | `		default:             return "scalar";` |
|        - |  3070 | `	}` |
|        5 |  3071 |  |
|        - |  3072 |  |
|        - |  3073 | `/*` |
|        - |  3074 | ` * Format the class name of an object-typed ph7_value into a small caller` |
|        - |  3075 | ` * buffer, for use in TypeError messages. Returns the buffer pointer.` |
|        - |  3076 | ` */` |
|       18 |  3077 | `static const char *VmFormatValueClassName(ph7_value *pValue,char *zBuf,sxu32 nBuf)` |
|        1 |  3078 |  |
|       19 |  3079 | `	ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       28 |  3080 | `	SyBufferFormat(zBuf,nBuf,"%.*s",` |
|       18 |  3081 | `		(int)pInst->pClass->sName.nByte,pInst->pClass->sName.zString);` |
|       19 |  3082 | `	return zBuf;` |
|        1 |  3083 |  |
|        - |  3084 |  |
|    14290 |  3085 | `static sxi32 VmEnforcePropertyTypeOnStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        2 |  3086 |  |
|        - |  3087 | `	SyHashEntry *pSlot;` |
|        - |  3088 | `	VmClassAttr *pVmAttr;` |
|        - |  3089 | `	ph7_class_attr *pAttr;` |
|    14292 |  3090 | `	pSlot = SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32));` |
|    14292 |  3091 | `	if( pSlot == 0 ){` |
|    14084 |  3092 | `		return SXRET_OK; /* Not a typed slot */` |
|        - |  3093 | `	}` |
|      210 |  3094 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      210 |  3095 | `	pAttr = pVmAttr->pAttr;` |
|      210 |  3096 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      ! 0 |  3097 | `		return SXRET_OK;` |
|        - |  3098 | `	}` |
|        - |  3099 | `	/* Union type: dispatch to the shared coercion helper. Typed properties` |
|        - |  3100 | `	 * are always evaluated in weak mode regardless of declare(strict_types),` |
|        - |  3101 | `	 * matching PHP's documented behavior. */` |
|      210 |  3102 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_UNION ){` |
|       23 |  3103 | `		sxi32 rc = VmCoerceToUnion(pVm, pValue, &pAttr->aUnionAlts,` |
|       14 |  3104 | `			(pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE) ? 1 : 0,` |
|        - |  3105 |  |
|       16 |  3106 | `		if( rc == SXRET_OK ){` |
|        9 |  3107 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        9 |  3108 | `			return SXRET_OK;` |
|        - |  3109 | `		}` |
|        7 |  3110 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3111 | `			char zBuf[128];` |
|        4 |  3112 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        1 |  3113 | `				VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3114 | `		}` |
|        5 |  3115 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3116 | `	}` |
|        - |  3117 | `	/* NULL handling: allowed only if the type is nullable. */` |
|      196 |  3118 | `	if( pValue->iFlags & MEMOBJ_NULL ){` |
|       14 |  3119 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NULLABLE ){` |
|       12 |  3120 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       12 |  3121 | `			return SXRET_OK;` |
|        - |  3122 | `		}` |
|        3 |  3123 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,"null");` |
|        - |  3124 | `	}` |
|        - |  3125 | `	/* Bare 'object' type hint: accept any class instance, reject non-objects.` |
|        - |  3126 | `	 * Must be checked before the generic scalar branch since MEMOBJ_OBJ is` |
|        - |  3127 | `	 * otherwise treated as "scalar, not array" and would be rejected. */` |
|      184 |  3128 | `	if( pAttr->nType == MEMOBJ_OBJ ){` |
|       12 |  3129 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3130 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|        5 |  3131 | `			return SXRET_OK;` |
|        - |  3132 | `		}` |
|        7 |  3133 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3134 | `	}` |
|      174 |  3135 | `	if( pAttr->nType == SXU32_HIGH ){` |
|        - |  3136 | `		/* Class / interface type. Resolve self/parent relative to the class` |
|        - |  3137 | `		 * currently active on the self-stack. */` |
|       26 |  3138 | `		ph7_class *pExpected = 0;` |
|       26 |  3139 | `		SyString *pClassName = &pAttr->sClass;` |
|       26 |  3140 | `		ph7_class *pSelfNow = 0;` |
|       26 |  3141 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        3 |  3142 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        3 |  3143 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        1 |  3144 | `		}` |
|       26 |  3145 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        5 |  3146 | `			pExpected = pSelfNow;` |
|       24 |  3147 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3148 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3149 | `		}else{` |
|       22 |  3150 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3151 | `		}` |
|       26 |  3152 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3153 | `			return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3154 | `		}` |
|       26 |  3155 | `		if( pExpected ){` |
|       22 |  3156 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|       22 |  3157 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|        - |  3158 | `				char zBuf[128];` |
|        7 |  3159 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        2 |  3160 | `					VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3161 | `			}` |
|        8 |  3162 | `		}` |
|       22 |  3163 | `		pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|       22 |  3164 | `		return SXRET_OK;` |
|        - |  3165 | `	}` |
|        - |  3166 | `	/* Scalar type. PHP 7.4 weak mode: attempt coercion using the same cast` |
|        - |  3167 | `	 * helpers used by function-argument hints. Reject object→scalar. */` |
|      150 |  3168 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - |  3169 | `		char zBuf[128];` |
|       10 |  3170 | `		return VmThrowPropertyTypeError(pVm,pVmAttr,` |
|        3 |  3171 | `			VmFormatValueClassName(pValue,zBuf,sizeof(zBuf)));` |
|        - |  3172 | `	}` |
|      144 |  3173 | `	if( (pValue->iFlags & pAttr->nType) == 0 ){` |
|       26 |  3174 | `		ProcMemObjCast xCast = PH7_MemObjCastMethod(pAttr->nType);` |
|       26 |  3175 | `		if( xCast ){` |
|        - |  3176 | `			/* Reject array<->scalar coercion to match PHP strictness */` |
|       26 |  3177 | `			if( pAttr->nType == MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  3178 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3179 | `			}` |
|       24 |  3180 | `			if( pAttr->nType != MEMOBJ_HASHMAP && (pValue->iFlags & MEMOBJ_HASHMAP) ){` |
|        5 |  3181 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,ph7_type_name(pValue));` |
|        - |  3182 | `			}` |
|        - |  3183 | `			/* PHP weak mode: reject string->int/float unless the string is` |
|        - |  3184 | `			 * strictly numeric. Silent coercion of "abc" or "43x" to 0/43` |
|        - |  3185 | `			 * would hide bugs and diverges from PHP's TypeError. */` |
|       26 |  3186 | `			if( (pAttr->nType == MEMOBJ_INT \|\| pAttr->nType == MEMOBJ_REAL)` |
|       17 |  3187 | `			 && (pValue->iFlags & MEMOBJ_STRING)` |
|       19 |  3188 | `			 && !VmStringIsStrictNumeric(pValue) ){` |
|        9 |  3189 | `				return VmThrowPropertyTypeError(pVm,pVmAttr,"string");` |
|        - |  3190 | `			}` |
|       12 |  3191 | `			xCast(pValue);` |
|        5 |  3192 | `		}` |
|        5 |  3193 | `	}` |
|      130 |  3194 | `	pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      130 |  3195 | `	return SXRET_OK;` |
|     7147 |  3196 |  |
|        - |  3197 |  |
|        - |  3198 | `/*` |
|        - |  3199 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3200 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3201 | ` * information.` |
|        - |  3202 | ` * ------------------------------------` |
|        - |  3203 | ` * Simple boring wrapper function.` |
|        - |  3204 | ` * ------------------------------------` |
|        - |  3205 | ` */` |
|       16 |  3206 | `PH7_PRIVATE sxi32 VmErrorFormat(ph7_vm *pVm,sxi32 iErr,const char *zFormat,...)` |
|        1 |  3207 |  |
|        - |  3208 | `	va_list ap;` |
|        - |  3209 | `	sxi32 rc;` |
|       17 |  3210 | `	va_start(ap,zFormat);` |
|       17 |  3211 | `	rc = VmThrowErrorAp(&(*pVm),0,iErr,zFormat,ap);` |
|       17 |  3212 | `	va_end(ap);` |
|       17 |  3213 | `	return rc;` |
|        1 |  3214 |  |
|        - |  3215 | `/*` |
|        - |  3216 | ` * Throw a TypeError exception from within the VM execution loop.` |
|        - |  3217 | ` * Used for user-defined function type hint violations (e.g. object type hint).` |
|        - |  3218 | ` */` |
|       34 |  3219 | `static sxi32 VmThrowTypeErrorForArg(ph7_vm *pVm,SyString *pFuncName,sxu32 nArg,SyString *pArgName,const char *zExpected,const char *zGiven)` |
|        1 |  3220 |  |
|        - |  3221 | `	ph7_class *pClass;` |
|        - |  3222 | `	ph7_class_instance *pThis;` |
|        - |  3223 | `	ph7_class_method *pCons;` |
|        - |  3224 | `	ph7_value sArg;` |
|        - |  3225 | `	ph7_value *apArg[1];` |
|        - |  3226 | `	SyBlob sMsg;` |
|        - |  3227 | `	SyString sMsgStr;` |
|        - |  3228 | `	VmFrame *pFrame;` |
|        - |  3229 | `	sxi32 rc;` |
|       35 |  3230 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|       35 |  3231 | `	if( pClass == 0 ){` |
|      ! 0 |  3232 | `		return PH7_ABORT;` |
|        - |  3233 | `	}` |
|       35 |  3234 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       35 |  3235 | `	if( pThis == 0 ){` |
|      ! 0 |  3236 | `		return PH7_ABORT;` |
|        - |  3237 | `	}` |
|       35 |  3238 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       35 |  3239 | `	SyBlobFormat(&sMsg,"%z(): Argument #%u ($%z) must be of type %s, %s given",` |
|       17 |  3240 | `		pFuncName,nArg,pArgName,zExpected,zGiven);` |
|       35 |  3241 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       35 |  3242 | `	if( pCons ){` |
|       35 |  3243 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       35 |  3244 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       35 |  3245 | `		apArg[0] = &sArg;` |
|       35 |  3246 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       35 |  3247 | `		PH7_MemObjRelease(&sArg);` |
|       17 |  3248 | `	}` |
|       35 |  3249 | `	SyBlobRelease(&sMsg);` |
|       35 |  3250 | `	pFrame = pVm->pFrame;` |
|       35 |  3251 | `	if( pFrame ){` |
|       35 |  3252 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       35 |  3253 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       17 |  3254 | `	}` |
|       35 |  3255 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       35 |  3256 | `	PH7_ClassInstanceUnref(pThis);` |
|       35 |  3257 | `	if( rc == SXERR_ABORT ){` |
|        5 |  3258 | `		return PH7_ABORT;` |
|        - |  3259 | `	}` |
|       31 |  3260 | `	return PH7_EXCEPTION;` |
|       18 |  3261 |  |
|        - |  3262 | `/*` |
|        - |  3263 | ` * Throw a PHP-compatible TypeError describing a return-value type mismatch.` |
|        - |  3264 | ` * Message format: "funcname(): Return value must be of type X, Y returned".` |
|        - |  3265 | ` */` |
|        6 |  3266 | `static sxi32 VmThrowTypeErrorForReturn(ph7_vm *pVm,SyString *pFuncName,const char *zExpected,const char *zGiven)` |
|        1 |  3267 |  |
|        - |  3268 | `	ph7_class *pClass;` |
|        - |  3269 | `	ph7_class_instance *pThis;` |
|        - |  3270 | `	ph7_class_method *pCons;` |
|        - |  3271 | `	ph7_value sArg;` |
|        - |  3272 | `	ph7_value *apArg[1];` |
|        - |  3273 | `	SyBlob sMsg;` |
|        - |  3274 | `	SyString sMsgStr;` |
|        - |  3275 | `	VmFrame *pFrame;` |
|        - |  3276 | `	sxi32 rc;` |
|        7 |  3277 | `	pClass = PH7_VmExtractClass(&(*pVm),"TypeError",sizeof("TypeError")-1,TRUE,0);` |
|        7 |  3278 | `	if( pClass == 0 ){` |
|      ! 0 |  3279 | `		return PH7_ABORT;` |
|        - |  3280 | `	}` |
|        7 |  3281 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|        7 |  3282 | `	if( pThis == 0 ){` |
|      ! 0 |  3283 | `		return PH7_ABORT;` |
|        - |  3284 | `	}` |
|        7 |  3285 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        7 |  3286 | `	SyBlobFormat(&sMsg,"%z(): Return value must be of type %s, %s returned",` |
|        3 |  3287 | `		pFuncName,zExpected,zGiven);` |
|        7 |  3288 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        7 |  3289 | `	if( pCons ){` |
|        7 |  3290 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        7 |  3291 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|        7 |  3292 | `		apArg[0] = &sArg;` |
|        7 |  3293 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|        7 |  3294 | `		PH7_MemObjRelease(&sArg);` |
|        3 |  3295 | `	}` |
|        7 |  3296 | `	SyBlobRelease(&sMsg);` |
|        7 |  3297 | `	pFrame = pVm->pFrame;` |
|        7 |  3298 | `	if( pFrame ){` |
|        7 |  3299 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        7 |  3300 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 |  3301 | `	}` |
|        7 |  3302 | `	rc = VmThrowException(&(*pVm),pThis);` |
|        7 |  3303 | `	PH7_ClassInstanceUnref(pThis);` |
|        7 |  3304 | `	if( rc == SXERR_ABORT ){` |
|        7 |  3305 | `		return PH7_ABORT;` |
|        - |  3306 | `	}` |
|      ! 0 |  3307 | `	return PH7_EXCEPTION;` |
|        4 |  3308 |  |
|        - |  3309 | `/*` |
|        - |  3310 | ` * Format the "X given" portion of error messages following PHP's value-name` |
|        - |  3311 | ` * convention: "true"/"false" for booleans, class name for objects, otherwise` |
|        - |  3312 | ` * the bare type name. zBuf must hold at least 64 bytes.` |
|        - |  3313 | ` */` |
|       16 |  3314 | `static const char * VmValueGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|        1 |  3315 |  |
|       17 |  3316 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|        5 |  3317 | `		return pVal->x.iVal ? "true" : "false";` |
|        - |  3318 | `	}` |
|       13 |  3319 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|        5 |  3320 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        5 |  3321 | `		if( pThis && pThis->pClass ){` |
|        5 |  3322 | `			SyString *pName = &pThis->pClass->sName;` |
|        5 |  3323 | `			sxu32 n = pName->nByte;` |
|        5 |  3324 | `			if( n >= nBuf ){` |
|      ! 0 |  3325 | `				n = nBuf - 1;` |
|      ! 0 |  3326 | `			}` |
|        5 |  3327 | `			SyMemcpy(pName->zString,zBuf,n);` |
|        5 |  3328 | `			zBuf[n] = 0;` |
|        5 |  3329 | `			return zBuf;` |
|        - |  3330 | `		}` |
|      ! 0 |  3331 | `		return "object";` |
|        - |  3332 | `	}` |
|        9 |  3333 | `	return ph7_type_name(pVal);` |
|        9 |  3334 |  |
|        - |  3335 | `/*` |
|        - |  3336 | ` * Throw a PHP-compatible Error when array unpacking ('...$expr') receives a` |
|        - |  3337 | ` * non-array value at runtime. Matches the message and class PHP raises` |
|        - |  3338 | ` * ("Only arrays and Traversables can be unpacked, X given"). The class is` |
|        - |  3339 | ` * \TypeError for objects, \Error otherwise — matching PHP's distinction.` |
|        - |  3340 | ` */` |
|       16 |  3341 | `static sxi32 VmThrowSpreadError(ph7_vm *pVm,ph7_value *pBad)` |
|        1 |  3342 |  |
|        - |  3343 | `	ph7_class *pClass;` |
|        - |  3344 | `	ph7_class_instance *pThis;` |
|        - |  3345 | `	ph7_class_method *pCons;` |
|        - |  3346 | `	ph7_value sArg;` |
|        - |  3347 | `	ph7_value *apArg[1];` |
|        - |  3348 | `	SyBlob sMsg;` |
|        - |  3349 | `	SyString sMsgStr;` |
|        - |  3350 | `	VmFrame *pFrame;` |
|        - |  3351 | `	sxi32 rc;` |
|       17 |  3352 | `	const char *zErrClass = (pBad->iFlags & MEMOBJ_OBJ) ? "TypeError" : "Error";` |
|        - |  3353 | `	char zNameBuf[64];` |
|       17 |  3354 | `	const char *zGiven = VmValueGivenName(pBad,zNameBuf,sizeof(zNameBuf));` |
|       17 |  3355 | `	pClass = PH7_VmExtractClass(&(*pVm),zErrClass,SyStrlen(zErrClass),TRUE,0);` |
|       17 |  3356 | `	if( pClass == 0 ){` |
|      ! 0 |  3357 | `		return PH7_ABORT;` |
|        - |  3358 | `	}` |
|       17 |  3359 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|       17 |  3360 | `	if( pThis == 0 ){` |
|      ! 0 |  3361 | `		return PH7_ABORT;` |
|        - |  3362 | `	}` |
|       17 |  3363 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       17 |  3364 | `	SyBlobFormat(&sMsg,"Only arrays and Traversables can be unpacked, %s given",zGiven);` |
|       17 |  3365 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       17 |  3366 | `	if( pCons ){` |
|       17 |  3367 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|       17 |  3368 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|       17 |  3369 | `		apArg[0] = &sArg;` |
|       17 |  3370 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|       17 |  3371 | `		PH7_MemObjRelease(&sArg);` |
|        8 |  3372 | `	}` |
|       17 |  3373 | `	SyBlobRelease(&sMsg);` |
|       17 |  3374 | `	pFrame = pVm->pFrame;` |
|       17 |  3375 | `	if( pFrame ){` |
|       17 |  3376 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       17 |  3377 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        8 |  3378 | `	}` |
|       17 |  3379 | `	rc = VmThrowException(&(*pVm),pThis);` |
|       17 |  3380 | `	PH7_ClassInstanceUnref(pThis);` |
|       17 |  3381 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  3382 | `		return PH7_ABORT;` |
|        - |  3383 | `	}` |
|       17 |  3384 | `	return PH7_EXCEPTION;` |
|        9 |  3385 |  |
|        - |  3386 | `/*` |
|        - |  3387 | ` * Enforce the declared return type of *pFunc* against the value returned` |
|        - |  3388 | ` * (or NULL if the function returned without a value). Mutates *pValue* to` |
|        - |  3389 | ` * perform allowed widening (int->float) or weak-mode coercion. On` |
|        - |  3390 | ` * violation, throws TypeError and returns PH7_EXCEPTION.` |
|        - |  3391 | ` */` |
|        - |  3392 | `/*` |
|        - |  3393 | ` * Bounded-copy *pStr* into *zBuf* (NUL-terminated, max nBuf-1 bytes). The` |
|        - |  3394 | ` * caller's buffer is then safe to pass through "%s" formatters. An empty or` |
|        - |  3395 | ` * null SyString yields an empty C string. Returns zBuf.` |
|        - |  3396 | ` */` |
|       24 |  3397 | `static const char *VmSyStringToCStr(const SyString *pStr, char *zBuf, sxu32 nBuf)` |
|        2 |  3398 |  |
|        - |  3399 | `	sxu32 nCopy;` |
|       26 |  3400 | `	if( nBuf == 0 ) return "";` |
|       26 |  3401 | `	if( pStr == 0 \|\| pStr->zString == 0 ){` |
|      ! 0 |  3402 | `		zBuf[0] = 0;` |
|      ! 0 |  3403 | `		return zBuf;` |
|        - |  3404 | `	}` |
|       26 |  3405 | `	nCopy = SyStringLength(pStr);` |
|       26 |  3406 | `	if( nCopy >= nBuf ) nCopy = nBuf - 1;` |
|       26 |  3407 | `	if( nCopy > 0 ) SyMemcpy(pStr->zString, zBuf, nCopy);` |
|       26 |  3408 | `	zBuf[nCopy] = 0;` |
|       26 |  3409 | `	return zBuf;` |
|       14 |  3410 |  |
|        - |  3411 |  |
|      376 |  3412 | `static sxi32 VmEnforceReturnType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_value *pValue)` |
|        2 |  3413 |  |
|      378 |  3414 | `	int bStrict = pFunc->bStrictTypes ? 1 : 0;` |
|        - |  3415 | `	const char *zGiven;` |
|        - |  3416 | `	char zBuf[128];` |
|        - |  3417 | `	char zTypeBuf[128];` |
|        - |  3418 | `	/* Untyped function: no enforcement. */` |
|      378 |  3419 | `	if( pFunc->nReturnType == 0 ){` |
|      ! 0 |  3420 | `		return SXRET_OK;` |
|        - |  3421 | `	}` |
|        - |  3422 | `	/* void return type: the function must not produce a value. */` |
|      378 |  3423 | `	if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      136 |  3424 | `		if( pValue == 0 ){` |
|      134 |  3425 | `			return SXRET_OK;` |
|        - |  3426 | `		}` |
|        - |  3427 | ``		/* PHP allows `return;` but rejects `return null;` — iP1=1 with NULL`` |
|        - |  3428 | `		 * still counts as "returned a value" here. */` |
|        3 |  3429 | `		zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|        3 |  3430 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,"void",zGiven);` |
|        - |  3431 | `	}` |
|        - |  3432 | `	/* Function fell off the end without an explicit return: PHP implicitly` |
|        - |  3433 | `	 * returns null. For a typed non-nullable return, that's a TypeError. */` |
|      244 |  3434 | `	if( pValue == 0 ){` |
|      ! 0 |  3435 | `		const char *zExpected = "value";` |
|      ! 0 |  3436 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3437 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3438 | `		}` |
|      ! 0 |  3439 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,"null");` |
|        - |  3440 | `	}` |
|        - |  3441 | `	/* Union return type — delegate. The function has no flag for nullable` |
|        - |  3442 | `	 * unions; a null alternative is represented inside aReturnUnion, so pass` |
|        - |  3443 | `	 * bNullable=0 here. */` |
|      244 |  3444 | `	if( SySetUsed(&pFunc->aReturnUnion) > 0 ){` |
|        - |  3445 | `		sxi32 rcU;` |
|      ! 0 |  3446 | `		int bNullable = 0;` |
|      ! 0 |  3447 | `		const char *zExpected = "union";` |
|        - |  3448 | ``		/* Scan alternatives for MEMOBJ_NULL, which serves as `T\|null`. */`` |
|        - |  3449 | `		{` |
|        - |  3450 | `			sxu32 i;` |
|      ! 0 |  3451 | `			ph7_type_alt *aAlts = (ph7_type_alt *)SySetBasePtr(&pFunc->aReturnUnion);` |
|      ! 0 |  3452 | `			for( i = 0; i < SySetUsed(&pFunc->aReturnUnion); i++ ){` |
|      ! 0 |  3453 | `				if( aAlts[i].nType == MEMOBJ_NULL ){ bNullable = 1; break; }` |
|      ! 0 |  3454 | `			}` |
|        - |  3455 | `		}` |
|      ! 0 |  3456 | `		rcU = VmCoerceToUnion(pVm, pValue, &pFunc->aReturnUnion, bNullable, bStrict);` |
|      ! 0 |  3457 | `		if( rcU == SXRET_OK ){` |
|      ! 0 |  3458 | `			return SXRET_OK;` |
|        - |  3459 | `		}` |
|      ! 0 |  3460 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3461 | `			zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3462 | `		}else if( pValue->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  3463 | `			zGiven = "null";` |
|      ! 0 |  3464 | `		}else{` |
|      ! 0 |  3465 | `			zGiven = ph7_type_name(pValue);` |
|        - |  3466 | `		}` |
|      ! 0 |  3467 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|      ! 0 |  3468 | `			zExpected = VmSyStringToCStr(&pFunc->sReturnTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  3469 | `		}` |
|      ! 0 |  3470 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3471 | `	}` |
|        - |  3472 | `	/* Class return type — instanceof check. The class name is a length-` |
|        - |  3473 | `	 * delimited SyString; copy it into a local buffer before formatting` |
|        - |  3474 | `	 * it into the TypeError message. */` |
|      244 |  3475 | `	if( pFunc->nReturnType == SXU32_HIGH ){` |
|        6 |  3476 | `		SyString *pClassName = &pFunc->sReturnClass;` |
|        - |  3477 | `		const char *zExpected;` |
|        - |  3478 | `		ph7_class *pExpected;` |
|        6 |  3479 | `		ph7_class *pSelfNow = 0;` |
|        6 |  3480 | `		if( SySetUsed(&pVm->aSelf) > 0 ){` |
|        6 |  3481 | `			ph7_class **apSelf = (ph7_class **)SySetBasePtr(&pVm->aSelf);` |
|        6 |  3482 | `			pSelfNow = apSelf[SySetUsed(&pVm->aSelf)-1];` |
|        2 |  3483 | `		}` |
|        6 |  3484 | `		if( pClassName->nByte == 4 && SyMemcmp(pClassName->zString,"self",4) == 0 ){` |
|        3 |  3485 | `			pExpected = pSelfNow;` |
|        4 |  3486 | `		}else if( pClassName->nByte == 6 && SyMemcmp(pClassName->zString,"parent",6) == 0 ){` |
|      ! 0 |  3487 | `			pExpected = pSelfNow ? pSelfNow->pBase : 0;` |
|      ! 0 |  3488 | `		}else{` |
|        3 |  3489 | `			pExpected = PH7_VmExtractClass(&(*pVm),pClassName->zString,pClassName->nByte,TRUE,0);` |
|        - |  3490 | `		}` |
|        6 |  3491 | `		zExpected = VmSyStringToCStr(pClassName, zTypeBuf, sizeof(zTypeBuf));` |
|        6 |  3492 | `		if( (pValue->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  3493 | `			zGiven = (pValue->iFlags & MEMOBJ_NULL) ? "null" : ph7_type_name(pValue);` |
|      ! 0 |  3494 | `			return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3495 | `		}` |
|        6 |  3496 | `		if( pExpected ){` |
|        6 |  3497 | `			ph7_class_instance *pInst = (ph7_class_instance *)pValue->x.pOther;` |
|        6 |  3498 | `			if( !PH7_VmInstanceOf(pInst->pClass,pExpected) ){` |
|      ! 0 |  3499 | `				zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3500 | `				return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,zExpected,zGiven);` |
|        - |  3501 | `			}` |
|        2 |  3502 | `		}` |
|        6 |  3503 | `		return SXRET_OK;` |
|        - |  3504 | `	}` |
|        - |  3505 | `	/* Scalar return type. Allow null pass-through if the function is` |
|        - |  3506 | `	 * nullable (textual "?T" gets that flag, though union+null is handled` |
|        - |  3507 | `	 * above). There's no explicit nullable flag on ph7_vm_func, so detect` |
|        - |  3508 | `	 * via the type-text leading '?'. */` |
|      240 |  3509 | `	if( (pValue->iFlags & MEMOBJ_NULL) ){` |
|        6 |  3510 | `		if( SyStringLength(&pFunc->sReturnTypeName) > 0` |
|        8 |  3511 | `		 && pFunc->sReturnTypeName.zString[0] == '?' ){` |
|        8 |  3512 | `			return SXRET_OK;` |
|        - |  3513 | `		}` |
|      ! 0 |  3514 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3515 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3516 | `			"null");` |
|        - |  3517 | `	}` |
|        - |  3518 | `	/* Exact match? Done. */` |
|      234 |  3519 | `	if( pValue->iFlags & pFunc->nReturnType ){` |
|      228 |  3520 | `		return SXRET_OK;` |
|        - |  3521 | `	}` |
|        - |  3522 | `	/* Object->scalar is never compatible. */` |
|        8 |  3523 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  3524 | `		zGiven = VmFormatValueClassName(pValue,zBuf,sizeof(zBuf));` |
|      ! 0 |  3525 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3526 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3527 | `			zGiven);` |
|        - |  3528 | `	}` |
|        - |  3529 | `	/* Array <-> scalar is never compatible. */` |
|        8 |  3530 | `	if( ((sxu32)(pValue->iFlags) & MEMOBJ_HASHMAP) != (pFunc->nReturnType & MEMOBJ_HASHMAP) ){` |
|      ! 0 |  3531 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|      ! 0 |  3532 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      ! 0 |  3533 | `			ph7_type_name(pValue));` |
|        - |  3534 | `	}` |
|        - |  3535 | `	/* PHP's weak-mode rule: string -> int/float is allowed only if the` |
|        - |  3536 | `	 * string is strictly numeric. Silently coercing "abc"/"43x" to 0/43` |
|        - |  3537 | `	 * would hide the bug and diverges from PHP. Strict mode falls through` |
|        - |  3538 | `	 * to VmEnforceScalarType below which rejects string->int outright. */` |
|        8 |  3539 | `	if( !bStrict` |
|        5 |  3540 | `	 && (pFunc->nReturnType == MEMOBJ_INT \|\| pFunc->nReturnType == MEMOBJ_REAL)` |
|        4 |  3541 | `	 && (pValue->iFlags & MEMOBJ_STRING)` |
|        6 |  3542 | `	 && !VmStringIsStrictNumeric(pValue) ){` |
|        4 |  3543 | `		return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3544 | `			VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        - |  3545 | `			"string");` |
|        - |  3546 | `	}` |
|        6 |  3547 | `	if( VmEnforceScalarType(pValue, pFunc->nReturnType, bStrict) == SXRET_OK ){` |
|        3 |  3548 | `		return SXRET_OK;` |
|        - |  3549 | `	}` |
|        4 |  3550 | `	return VmThrowTypeErrorForReturn(pVm,&pFunc->sName,` |
|        1 |  3551 | `		VmScalarTypeName(pFunc->nReturnType,&pFunc->sReturnTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|        1 |  3552 | `		ph7_type_name(pValue));` |
|      190 |  3553 |  |
|        - |  3554 | `/*` |
|        - |  3555 | ` * Report a fatal named-argument error.` |
|        - |  3556 | ` * Outputs a PHP-compatible "Uncaught Error:" message and aborts execution.` |
|        - |  3557 | ` */` |
|        6 |  3558 | `static sxi32 VmThrowNamedArgError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|        1 |  3559 |  |
|        7 |  3560 | `	const char *zFunc = 0;` |
|        7 |  3561 | `	int nFunc = 0;` |
|        7 |  3562 | `	VmGetFrameContext(pVm,&zFunc,&nFunc);` |
|        7 |  3563 | `	return VmReportUncaughtException(pVm,"Error",5,zMsg,nMsg,zFunc,nFunc);` |
|        1 |  3564 |  |
|        - |  3565 | `/*` |
|        - |  3566 | ` * Format and throw a run-time error and invoke the supplied VM output consumer callback.` |
|        - |  3567 | ` * Refer to the implementation of [ph7_context_throw_error_format()] for additional` |
|        - |  3568 | ` * information.` |
|        - |  3569 | ` * ------------------------------------` |
|        - |  3570 | ` * Simple boring wrapper function.` |
|        - |  3571 | ` * ------------------------------------` |
|        - |  3572 | ` */` |
|       24 |  3573 | `PH7_PRIVATE sxi32 PH7_VmThrowErrorAp(ph7_vm *pVm,SyString *pFuncName,sxi32 iErr,const char *zFormat,va_list ap)` |
|        2 |  3574 |  |
|        - |  3575 | `	sxi32 rc;` |
|       26 |  3576 | `	rc = VmThrowErrorAp(&(*pVm),&(*pFuncName),iErr,zFormat,ap);` |
|       26 |  3577 | `	return rc;` |
|        2 |  3578 |  |
|        - |  3579 | `/*` |
|        - |  3580 | ` * Resolve function context from the current frame.` |
|        - |  3581 | ` */` |
|     1018 |  3582 | `static void VmGetFrameContext(ph7_vm *pVm,const char **pzFuncName,int *pnFuncLen)` |
|        1 |  3583 |  |
|        - |  3584 | `	VmFrame *pFrame;` |
|        - |  3585 | `	ph7_vm_func *pFunc;` |
|     1019 |  3586 | `	*pzFuncName = 0;` |
|     1019 |  3587 | `	*pnFuncLen = 0;` |
|     1019 |  3588 | `	pFrame = pVm->pFrame;` |
|     1019 |  3589 | `	if( pFrame == 0 ){` |
|      ! 0 |  3590 | `		return;` |
|        - |  3591 | `	}` |
|     1019 |  3592 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|     1019 |  3593 | `	if( pFrame->pParent == 0 ){` |
|      995 |  3594 | `		return;` |
|        - |  3595 | `	}` |
|       25 |  3596 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       25 |  3597 | `	if( pFunc == 0 ){` |
|      ! 0 |  3598 | `		return;` |
|        - |  3599 | `	}` |
|       25 |  3600 | `	*pzFuncName = pFunc->sName.zString;` |
|       25 |  3601 | `	*pnFuncLen = (int)pFunc->sName.nByte;` |
|      510 |  3602 |  |
|        - |  3603 | `/*` |
|        - |  3604 | ` * Emit a PHP-compatible uncaught exception message and stack trace.` |
|        - |  3605 | ` */` |
|      524 |  3606 | `static sxi32 VmReportUncaughtException(ph7_vm *pVm,const char *zClass,sxu32 nClass,const char *zMsg,sxu32 nMsg,const char *zFuncName,int nFuncLen)` |
|        1 |  3607 |  |
|        - |  3608 | `	SyBlob sOut;` |
|        - |  3609 | `	SyString *pFile;` |
|      525 |  3610 | `	if( !pVm->bErrReport ){` |
|      ! 0 |  3611 | `		return PH7_OK;` |
|        - |  3612 | `	}` |
|      525 |  3613 | `	if( zClass == 0 \|\| nClass == 0 ){` |
|      ! 0 |  3614 | `		zClass = "Exception";` |
|      ! 0 |  3615 | `		nClass = (sxu32)sizeof("Exception") - 1;` |
|      ! 0 |  3616 | `	}` |
|      525 |  3617 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      503 |  3618 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      251 |  3619 | `	}` |
|      525 |  3620 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      525 |  3621 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      525 |  3622 | `	SyBlobAppend(&sOut,"PHP Fatal error:  Uncaught ",sizeof("PHP Fatal error:  Uncaught ")-1);` |
|      525 |  3623 | `	SyBlobAppend(&sOut,zClass,nClass);` |
|      525 |  3624 | `	if( zMsg && nMsg > 0 ){` |
|      525 |  3625 | `		SyBlobAppend(&sOut,": ",sizeof(": ")-1);` |
|      525 |  3626 | `		SyBlobAppend(&sOut,zMsg,nMsg);` |
|      262 |  3627 | `	}` |
|      525 |  3628 | `	if( pFile ){` |
|      525 |  3629 | `		SyBlobAppend(&sOut," in ",sizeof(" in ")-1);` |
|      525 |  3630 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3631 | `		SyBlobAppend(&sOut,":1",sizeof(":1")-1);` |
|      262 |  3632 | `	}` |
|      525 |  3633 | `	SyBlobAppend(&sOut,"\nStack trace:\n",sizeof("\nStack trace:\n")-1);` |
|      525 |  3634 | `	if( pFile ){` |
|      525 |  3635 | `		SyBlobAppend(&sOut,"#0 ",sizeof("#0 ")-1);` |
|      525 |  3636 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3637 | `		if( zFuncName && nFuncLen > 0 ){` |
|       25 |  3638 | `			SyBlobFormat(&sOut,"(1): %.*s()\n",nFuncLen,zFuncName);` |
|       13 |  3639 | `		}else{` |
|      501 |  3640 | `			SyBlobAppend(&sOut,"(1): {main}\n",sizeof("(1): {main}\n")-1);` |
|        1 |  3641 | `		}` |
|      262 |  3642 | `	}else if( zFuncName && nFuncLen > 0 ){` |
|      ! 0 |  3643 | `		SyBlobFormat(&sOut,"#0 [internal function]: %.*s()\n",nFuncLen,zFuncName);` |
|      ! 0 |  3644 | `	}else{` |
|      ! 0 |  3645 | `		SyBlobAppend(&sOut,"#0 {main}\n",sizeof("#0 {main}\n")-1);` |
|        - |  3646 | `	}` |
|      525 |  3647 | `	SyBlobAppend(&sOut,"#1 {main}",sizeof("#1 {main}")-1);` |
|      525 |  3648 | `	if( pFile ){` |
|      525 |  3649 | `		SyBlobAppend(&sOut,"\n",sizeof("\n")-1);` |
|      525 |  3650 | `		SyBlobAppend(&sOut,"  thrown in ",sizeof("  thrown in ")-1);` |
|      525 |  3651 | `		SyBlobAppend(&sOut,pFile->zString,pFile->nByte);` |
|      525 |  3652 | `		SyBlobAppend(&sOut," on line 1",sizeof(" on line 1")-1);` |
|      262 |  3653 | `	}` |
|      525 |  3654 | `	VmCallErrorHandler(pVm,&sOut);` |
|      525 |  3655 | `	SyBlobRelease(&sOut);` |
|      525 |  3656 | `	return PH7_ABORT;` |
|      263 |  3657 |  |
|        - |  3658 | `/*` |
|        - |  3659 | ` * Disarm the null-coalesce-assign scratch slot armed by LOAD_IDX iP2=3.` |
|        - |  3660 | ` *` |
|        - |  3661 | ` * Arming holds a ref on the cached ArrayAccess instance so it survives the` |
|        - |  3662 | ` * intervening RHS evaluation until NULLC_STORE consumes it. Anything that` |
|        - |  3663 | ` * abandons that store path before NULLC_STORE runs — an exception thrown` |
|        - |  3664 | ` * while evaluating the RHS, a re-arm for a different target — must disarm` |
|        - |  3665 | ` * here, both to release the leaked instance ref/key and to stop a later` |
|        - |  3666 | ` * unrelated NULLC_STORE from dispatching offsetSet() on the stale slot.` |
|        - |  3667 | ` */` |
|      828 |  3668 | `static void VmCoalesceDisarm(ph7_vm *pVm)` |
|        2 |  3669 |  |
|      830 |  3670 | `	if( pVm->bCoalesceArmed ){` |
|        7 |  3671 | `		if( pVm->pCoalesceObj ){` |
|        7 |  3672 | `			PH7_ClassInstanceUnref(pVm->pCoalesceObj);` |
|        3 |  3673 | `		}` |
|        7 |  3674 | `		PH7_MemObjRelease(&pVm->sCoalesceKey);` |
|        7 |  3675 | `		pVm->pCoalesceObj = 0;` |
|        7 |  3676 | `		pVm->bCoalesceArmed = 0;` |
|        3 |  3677 | `	}` |
|      830 |  3678 |  |
|        - |  3679 | `/*` |
|        - |  3680 | ` * Throw a PHP-compatible exception of the named class from inside the VM` |
|        - |  3681 | ` * bytecode dispatch loop (where no ph7_context is available). The message` |
|        - |  3682 | ` * is a literal, non-formatted string; callers that need formatting should` |
|        - |  3683 | ` * build the SyBlob themselves and pass its data + length.` |
|        - |  3684 | ` *` |
|        - |  3685 | ` * Returns SXERR_ABORT if the throw machinery itself fails, SXRET_OK on` |
|        - |  3686 | `` * successful throw (the caller should typically `goto Abort` afterwards if`` |
|        - |  3687 | ` * the surrounding opcode cannot continue). Mirrors the inline pattern in` |
|        - |  3688 | ` * PH7_OP_THROW (see "case PH7_OP_THROW").` |
|        - |  3689 | ` */` |
|        4 |  3690 | `static sxi32 VmThrowFromVm(` |
|        - |  3691 | `	ph7_vm *pVm,` |
|        - |  3692 | `	const char *zClass,` |
|        - |  3693 | `	const char *zMsg,` |
|        - |  3694 | `	sxu32 nMsg` |
|        1 |  3695 | `){` |
|        - |  3696 | `	ph7_class *pClass;` |
|        - |  3697 | `	ph7_class_instance *pThis;` |
|        - |  3698 | `	ph7_class_method *pCons;` |
|        - |  3699 | `	VmFrame *pFrame;` |
|        - |  3700 | `	sxi32 rc;` |
|        5 |  3701 | `	pClass = PH7_VmExtractClass(pVm,zClass,SyStrlen(zClass),TRUE,0);` |
|        5 |  3702 | `	if( pClass == 0 ){` |
|      ! 0 |  3703 | `		return SXERR_ABORT;` |
|        - |  3704 | `	}` |
|        5 |  3705 | `	pThis = PH7_NewClassInstance(pVm,pClass);` |
|        5 |  3706 | `	if( pThis == 0 ){` |
|      ! 0 |  3707 | `		return SXERR_ABORT;` |
|        - |  3708 | `	}` |
|        5 |  3709 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|        5 |  3710 | `	if( pCons ){` |
|        - |  3711 | `		ph7_value sArg;` |
|        - |  3712 | `		ph7_value *apArg[1];` |
|        - |  3713 | `		SyString sMsgStr;` |
|        5 |  3714 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|        5 |  3715 | `		PH7_MemObjInit(pVm,&sArg);` |
|        5 |  3716 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        5 |  3717 | `		apArg[0] = &sArg;` |
|        5 |  3718 | `		PH7_VmCallClassMethod(pVm,pThis,pCons,0,1,apArg);` |
|        5 |  3719 | `		PH7_MemObjRelease(&sArg);` |
|        2 |  3720 | `	}` |
|        5 |  3721 | `	pFrame = pVm->pFrame;` |
|        5 |  3722 | `	if( pFrame ){` |
|        5 |  3723 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 |  3724 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        2 |  3725 | `	}` |
|        5 |  3726 | `	rc = VmThrowException(pVm,pThis);` |
|        5 |  3727 | `	PH7_ClassInstanceUnref(pThis);` |
|        5 |  3728 | `	return rc;` |
|        3 |  3729 |  |
|        - |  3730 | `/*` |
|        - |  3731 | ` * Throw an internal exception instance that can be intercepted by try/catch.` |
|        - |  3732 | ` */` |
|      574 |  3733 | `PH7_PRIVATE sxi32 PH7_VmThrowException(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|        2 |  3734 |  |
|        - |  3735 | `	ph7_vm *pVm;` |
|        - |  3736 | `	ph7_class *pClass;` |
|        - |  3737 | `	ph7_class_instance *pThis;` |
|        - |  3738 | `	ph7_class_method *pCons;` |
|        - |  3739 | `	ph7_value sArg;` |
|        - |  3740 | `	ph7_value *apArg[1];` |
|        - |  3741 | `	SyBlob sMsg;` |
|        - |  3742 | `	SyString sMsgStr;` |
|        - |  3743 | `	VmFrame *pFrame;` |
|        - |  3744 | `	va_list ap;` |
|        - |  3745 | `	sxi32 rc;` |
|        - |  3746 |  |
|      576 |  3747 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3748 | `		return PH7_ABORT;` |
|        - |  3749 | `	}` |
|      576 |  3750 | `	pVm = pCtx->pVm;` |
|      576 |  3751 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3752 | `		zClass = "Error";` |
|      ! 0 |  3753 | `	}` |
|      576 |  3754 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,SyStrlen(zClass),TRUE,0);` |
|      576 |  3755 | `	if( pClass == 0 ){` |
|      ! 0 |  3756 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3757 | `			"Cannot throw internal exception, class '%s' is not available",` |
|      ! 0 |  3758 | `			zClass` |
|        - |  3759 | `			);` |
|        - |  3760 | `	}` |
|      576 |  3761 | `	pThis = PH7_NewClassInstance(&(*pVm),pClass);` |
|      576 |  3762 | `	if( pThis == 0 ){` |
|      ! 0 |  3763 | `		return PH7_VmThrowExceptionTrace(pCtx,zClass,` |
|        - |  3764 | `			"Cannot throw internal exception, PH7 is running out of memory"` |
|        - |  3765 | `			);` |
|        - |  3766 | `	}` |
|        - |  3767 |  |
|      576 |  3768 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      576 |  3769 | `	va_start(ap,zFormat);` |
|      576 |  3770 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      576 |  3771 | `	va_end(ap);` |
|        - |  3772 |  |
|      576 |  3773 | `	pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|      576 |  3774 | `	if( pCons ){` |
|      576 |  3775 | `		SyStringInitFromBuf(&sMsgStr,(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|      576 |  3776 | `		PH7_MemObjInitFromString(&(*pVm),&sArg,&sMsgStr);` |
|      576 |  3777 | `		apArg[0] = &sArg;` |
|      576 |  3778 | `		PH7_VmCallClassMethod(&(*pVm),pThis,pCons,0,1,apArg);` |
|      576 |  3779 | `		PH7_MemObjRelease(&sArg);` |
|      287 |  3780 | `	}` |
|      576 |  3781 | `	SyBlobRelease(&sMsg);` |
|        - |  3782 |  |
|      576 |  3783 | `	pFrame = pVm->pFrame;` |
|      576 |  3784 | `	if( pFrame ){` |
|      576 |  3785 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      576 |  3786 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|      287 |  3787 | `	}` |
|      576 |  3788 | `	rc = VmThrowException(&(*pVm),pThis);` |
|      576 |  3789 | `	PH7_ClassInstanceUnref(pThis);` |
|      576 |  3790 | `	if( rc == SXERR_ABORT ){` |
|      491 |  3791 | `		return PH7_ABORT;` |
|        - |  3792 | `	}` |
|       86 |  3793 | `	return PH7_EXCEPTION;` |
|      289 |  3794 |  |
|        - |  3795 | `/*` |
|        - |  3796 | ` * Throw an internal error as a PHP-like uncaught exception message with stack trace.` |
|        - |  3797 | ` * This is intentionally separate from PH7_VmThrowError*() to allow gradual migration.` |
|        - |  3798 | ` */` |
|      ! 0 |  3799 | `PH7_PRIVATE sxi32 PH7_VmThrowExceptionTrace(ph7_context *pCtx,const char *zClass,const char *zFormat,...)` |
|      ! 0 |  3800 |  |
|        - |  3801 | `	ph7_vm *pVm;` |
|        - |  3802 | `	SyBlob sMsg;` |
|      ! 0 |  3803 | `	const char *zFuncName = 0;` |
|      ! 0 |  3804 | `	int nFuncLen = 0;` |
|        - |  3805 | `	va_list ap;` |
|        - |  3806 | `	sxi32 rc;` |
|        - |  3807 |  |
|      ! 0 |  3808 | `	if( pCtx == 0 \|\| pCtx->pVm == 0 ){` |
|      ! 0 |  3809 | `		return PH7_OK;` |
|        - |  3810 | `	}` |
|      ! 0 |  3811 | `	pVm = pCtx->pVm;` |
|      ! 0 |  3812 | `	if( zClass == 0 \|\| zClass[0] == 0 ){` |
|      ! 0 |  3813 | `		zClass = "Error";` |
|      ! 0 |  3814 | `	}` |
|        - |  3815 |  |
|      ! 0 |  3816 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        - |  3817 |  |
|      ! 0 |  3818 | `	va_start(ap,zFormat);` |
|      ! 0 |  3819 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|      ! 0 |  3820 | `	va_end(ap);` |
|        - |  3821 |  |
|      ! 0 |  3822 | `	if( pCtx->pFunc ){` |
|      ! 0 |  3823 | `		zFuncName = pCtx->pFunc->sName.zString;` |
|      ! 0 |  3824 | `		nFuncLen = (int)pCtx->pFunc->sName.nByte;` |
|      ! 0 |  3825 | `	}` |
|      ! 0 |  3826 | `	if( zFuncName == 0 \|\| nFuncLen <= 0 ){` |
|      ! 0 |  3827 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      ! 0 |  3828 | `	}` |
|      ! 0 |  3829 | `	rc = VmReportUncaughtException(pVm,zClass,SyStrlen(zClass),` |
|      ! 0 |  3830 | `		(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg),zFuncName,nFuncLen);` |
|      ! 0 |  3831 | `	SyBlobRelease(&sMsg);` |
|      ! 0 |  3832 | `	return rc;` |
|      ! 0 |  3833 |  |
|        - |  3834 | `/*` |
|        - |  3835 | ` * Save the execution state of a fiber/generator context.` |
|        - |  3836 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|        - |  3837 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|        - |  3838 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|        - |  3839 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|        - |  3840 | ` * when VmByteCodeExec returns.` |
|        - |  3841 | ` */` |
|      144 |  3842 | `static sxi32 VmSuspendCtx(` |
|        - |  3843 | `	ph7_vm *pVm,` |
|        - |  3844 | `	ph7_exec_ctx *pCtx,` |
|        - |  3845 | `	sxi32 pc,` |
|        - |  3846 | `	sxi32 nTos` |
|        - |  3847 | `	)` |
|        2 |  3848 |  |
|       72 |  3849 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      146 |  3850 | `	pCtx->pc = pc;` |
|      146 |  3851 | `	pCtx->nTos = nTos;` |
|      146 |  3852 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      146 |  3853 | `	return PH7_SUSPEND;` |
|        2 |  3854 |  |
|        - |  3855 | `/*` |
|        - |  3856 | ` * Resolve named-argument mapping.` |
|        - |  3857 | ` *` |
|        - |  3858 | ` * For each actual argument in the call, determine which formal parameter it` |
|        - |  3859 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|        - |  3860 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|        - |  3861 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|        - |  3862 | ` * every formal parameter that received a value.` |
|        - |  3863 | ` *` |
|        - |  3864 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|        - |  3865 | ` * positional-overlaps-named) it calls VmThrowNamedArgError and returns` |
|        - |  3866 | ` * PH7_ABORT so the caller can jump to its Abort label.` |
|        - |  3867 | ` */` |
|       98 |  3868 | `static sxi32 VmResolveNamedArgs(` |
|        - |  3869 | `	ph7_vm *pVm,` |
|        - |  3870 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|        - |  3871 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|        - |  3872 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|        - |  3873 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|        - |  3874 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|        - |  3875 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|        - |  3876 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|        - |  3877 |  |
|        2 |  3878 |  |
|      100 |  3879 | `	sxi32 posIdx = 0;` |
|        - |  3880 | `	sxu32 i;` |
|        - |  3881 | `	char zErrMsg[256];` |
|      100 |  3882 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      296 |  3883 | `	for( i = 0; i < nActual; i++ ){` |
|      198 |  3884 | `		aSlot[i] = -2;` |
|      100 |  3885 | `	}` |
|      290 |  3886 | `	for( i = 0; i < nActual; i++ ){` |
|      286 |  3887 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|        - |  3888 | `			/* Named argument — find formal by name */` |
|      184 |  3889 | `			int found = 0;` |
|        - |  3890 | `			sxu32 k;` |
|      304 |  3891 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      290 |  3892 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      281 |  3893 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      268 |  3894 | `						pMap->aNames[i].zString,` |
|      402 |  3895 | `						pMap->aNames[i].nByte) == 0 ){` |
|      172 |  3896 | `					if( aUsed[k] ){` |
|        7 |  3897 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3898 | `							"Named parameter $%.*s overwrites previous argument",` |
|        4 |  3899 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        5 |  3900 | `						VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        5 |  3901 | `						return PH7_ABORT;` |
|        - |  3902 | `					}` |
|      168 |  3903 | `					aSlot[i] = (sxi32)k;` |
|      168 |  3904 | `					aUsed[k] = 1;` |
|      168 |  3905 | `					found = 1;` |
|      168 |  3906 | `					break;` |
|        - |  3907 | `				}` |
|       62 |  3908 | `			}` |
|      180 |  3909 | `			if( !found ){` |
|       14 |  3910 | `				if( iVariadicIdx >= 0 ){` |
|       11 |  3911 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        6 |  3912 | `				}else{` |
|        4 |  3913 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3914 | `						"Unknown named parameter $%.*s",` |
|        2 |  3915 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        3 |  3916 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|        3 |  3917 | `					return PH7_ABORT;` |
|        - |  3918 | `				}` |
|        5 |  3919 | `			}` |
|       90 |  3920 | `		}else{` |
|        - |  3921 | `			/* Positional argument */` |
|       16 |  3922 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       16 |  3923 | `				if( aUsed[posIdx] ){` |
|      ! 0 |  3924 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|        - |  3925 | `						"Named parameter $%.*s overwrites previous argument",` |
|      ! 0 |  3926 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|      ! 0 |  3927 | `					VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|      ! 0 |  3928 | `					return PH7_ABORT;` |
|        - |  3929 | `				}` |
|       16 |  3930 | `				aSlot[i] = posIdx;` |
|       16 |  3931 | `				aUsed[posIdx] = 1;` |
|        7 |  3932 | `			}else if( iVariadicIdx >= 0 ){` |
|      ! 0 |  3933 | `				aSlot[i] = -1; /* overflow to variadic */` |
|      ! 0 |  3934 | `			}` |
|       16 |  3935 | `			posIdx++;` |
|        - |  3936 | `		}` |
|       97 |  3937 | `	}` |
|       93 |  3938 | `	return SXRET_OK;` |
|       51 |  3939 |  |
|        - |  3940 | `/*` |
|        - |  3941 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  3942 | ` *` |
|        - |  3943 | ` * [PH7_VmMakeReady()] must be called before this routine in order to` |
|        - |  3944 | ` * close the program with a final OP_DONE and to set up the default` |
|        - |  3945 | ` * consumer routines and other stuff. Refer to the implementation` |
|        - |  3946 | ` * of [PH7_VmMakeReady()] for additional information.` |
|        - |  3947 | ` * If the installed VM output consumer callback ever returns PH7_ABORT` |
|        - |  3948 | ` * then the program execution is halted.` |
|        - |  3949 | ` * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]` |
|        - |  3950 | ` * should be used respectively to clean up the mess that was left behind` |
|        - |  3951 | ` * or to reset the VM to it's initial state.` |
|        - |  3952 | ` */` |
|    43768 |  3953 | `static sxi32 VmByteCodeExec(` |
|        - |  3954 | `	ph7_vm *pVm,         /* Target VM */` |
|        - |  3955 | `	VmInstr *aInstr,     /* PH7 bytecode program */` |
|        - |  3956 | `	ph7_value *pStack,   /* Operand stack */` |
|        - |  3957 | `	int nTos,            /* Top entry in the operand stack (usually -1) */` |
|        - |  3958 | `	ph7_value *pResult,  /* Store program return value here. NULL otherwise */` |
|        - |  3959 | `	sxu32 *pLastRef,     /* Last referenced ph7_value index */` |
|        - |  3960 | `	int is_callback,     /* TRUE if we are executing a callback */` |
|        - |  3961 | `	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */` |
|        - |  3962 | `	ph7_vm_func *pEnforceRetFunc /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */` |
|        - |  3963 | `	)` |
|        2 |  3964 |  |
|        - |  3965 | `	VmInstr *pInstr;` |
|        - |  3966 | `	ph7_value *pTos;` |
|        - |  3967 | `	SySet aArg;` |
|        - |  3968 | `	sxu32 nExceptionBase; /* Exception stack depth at entry (for finally drain guard) */` |
|        - |  3969 | `	sxi32 pc;` |
|        - |  3970 | `	sxi32 rc;` |
|        - |  3971 | `	/* Argument container */` |
|    43770 |  3972 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|    43770 |  3973 | `	if( nTos < 0 ){` |
|    40800 |  3974 | `		pTos = &pStack[-1];` |
|    20401 |  3975 | `	}else{` |
|     2972 |  3976 | `		pTos = &pStack[nTos];` |
|        - |  3977 | `	}` |
|    43770 |  3978 | `	nExceptionBase = SySetUsed(&pVm->aException);` |
|    43770 |  3979 | `	pc = nPc;` |
|        - |  3980 | `/*` |
|        - |  3981 | ` * Typed-property enforcement helper for compound stores. Called before` |
|        - |  3982 | ` * PH7_MemObjStore writes into a member memobj slot. On failure throws a` |
|        - |  3983 | ` * PHP TypeError and either jumps to the nearest catch block or propagates` |
|        - |  3984 | ` * out of the VM loop. Must be used inside a case of the main switch.` |
|        - |  3985 | ` */` |
|        - |  3986 | `#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \` |
|        - |  3987 | `	{ \` |
|        - |  3988 | `		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg)); \` |
|        - |  3989 | `		if( _rcT == PH7_ABORT ){ goto Abort; } \` |
|        - |  3990 | `		if( _rcT == PH7_EXCEPTION ){ \` |
|        - |  3991 | `			VmFrame *_pFrmT = pVm->pFrame; \` |
|        - |  3992 | `			if( _pFrmT && (_pFrmT->iFlags & VM_FRAME_EXCEPTION) && _pFrmT->iExceptionJump > 0 ){ \` |
|        - |  3993 | `				pc = _pFrmT->iExceptionJump - 1; \` |
|        - |  3994 | `				break; \` |
|        - |  3995 | `			} \` |
|        - |  3996 | `			goto Exception; \` |
|        - |  3997 | `		} \` |
|        - |  3998 | `	}` |
|        - |  3999 | `	/* Execute as much as we can */` |
|  5824666 |  4000 | `	for(;;){` |
|        - |  4001 | `		/* Fetch the instruction to execute */` |
| 11648630 |  4002 | `		pInstr = &aInstr[pc];` |
| 11648630 |  4003 | `		rc = SXRET_OK;` |
|        - |  4004 | `/*` |
|        - |  4005 | ` * What follows here is a massive switch statement where each case implements a` |
|        - |  4006 | ` * separate instruction in the virtual machine.  If we follow the usual` |
|        - |  4007 | ` * indentation convention each case should be indented by 6 spaces.  But` |
|        - |  4008 | ` * that is a lot of wasted space on the left margin.  So the code within` |
|        - |  4009 | ` * the switch statement will break with convention and be flush-left.` |
|        - |  4010 | ` */` |
| 11648630 |  4011 | `		switch(pInstr->iOp){` |
|        - |  4012 | `/*` |
|        - |  4013 | ` * DONE: P1 * *` |
|        - |  4014 | ` *` |
|        - |  4015 | ` * Program execution completed: Clean up the mess left behind` |
|        - |  4016 | ` * and return immediately.` |
|        - |  4017 | ` */` |
|    21532 |  4018 | `case PH7_OP_DONE:` |
|        - |  4019 | `	/* Return-type enforcement: only the user-function CALL handler (and` |
|        - |  4020 | `	 * the fiber start/resume paths) set pEnforceRetFunc, so this branch is` |
|        - |  4021 | `	 * skipped for default-value bytecode, class-method mini-programs,` |
|        - |  4022 | `	 * callback trampolines, and the main script. */` |
|    43066 |  4023 | `	if( pEnforceRetFunc && pEnforceRetFunc->nReturnType > 0 ){` |
|      378 |  4024 | `		ph7_value *pRetVal = 0;` |
|      378 |  4025 | `		if( pInstr->iP1 && pTos >= pStack ){` |
|      246 |  4026 | `			pRetVal = pTos;` |
|      122 |  4027 | `		}` |
|      378 |  4028 | `		rc = VmEnforceReturnType(&(*pVm), pEnforceRetFunc, pRetVal);` |
|      378 |  4029 | `		if( rc == PH7_ABORT ) goto Abort;` |
|      372 |  4030 | `		if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  4031 | `			if( pInstr->iP1 && pTos >= pStack ){` |
|      ! 0 |  4032 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4033 | `				pTos--;` |
|      ! 0 |  4034 | `			}` |
|      ! 0 |  4035 | `			goto Exception;` |
|        - |  4036 | `		}` |
|        - |  4037 | `		/* Don't enforce twice if the function loops through multiple` |
|        - |  4038 | `		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but` |
|        - |  4039 | `		 * defensively we clear the pointer after a successful check). */` |
|      372 |  4040 | `		pEnforceRetFunc = 0;` |
|      185 |  4041 | `	}` |
|    43060 |  4042 | `	if( pInstr->iP1 ){` |
|        - |  4043 | `#ifdef UNTRUST` |
|        - |  4044 | `		if( pTos < pStack ){` |
|        - |  4045 | `			goto Abort;` |
|        - |  4046 | `		}` |
|        - |  4047 | `#endif` |
|    26110 |  4048 | `		if( pLastRef ){` |
|    16050 |  4049 | `			*pLastRef = pTos->nIdx;` |
|     8024 |  4050 | `		}` |
|    26110 |  4051 | `		if( pResult ){` |
|        - |  4052 | `			/* Execution result */` |
|    24690 |  4053 | `			PH7_MemObjStore(pTos,pResult);` |
|    12344 |  4054 | `		}` |
|    26110 |  4055 | `		VmPopOperand(&pTos,1);` |
|    30006 |  4056 | `	}else if( pLastRef ){` |
|        - |  4057 | `		/* Nothing referenced */` |
|     1842 |  4058 | `		*pLastRef = SXU32_HIGH;` |
|      920 |  4059 | `	}` |
|        - |  4060 | `	/* Execute pending finally blocks for any try/catch contexts pushed during` |
|        - |  4061 | `	 * this execution. When 'return' is used inside a try block,` |
|        - |  4062 | `	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before` |
|        - |  4063 | `	 * returning. Only drain entries above nExceptionBase to avoid interfering` |
|        - |  4064 | `	 * with exception contexts from an outer VmByteCodeExec invocation.` |
|        - |  4065 | `	 * This runs AFTER storing the return value so that 'return' in a finally` |
|        - |  4066 | `	 * block can override it.` |
|        - |  4067 | `	 */` |
|    43062 |  4068 | `	while( SySetUsed(&pVm->aException) > nExceptionBase ){` |
|        3 |  4069 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        3 |  4070 | `		ph7_exception *pExc = apExc[SySetUsed(&pVm->aException) - 1];` |
|        3 |  4071 | `		(void)SySetPop(&pVm->aException);` |
|        3 |  4072 | `		pExc->pFrame = 0;` |
|        3 |  4073 | `		VmLeaveFrame(&(*pVm));` |
|        3 |  4074 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|        3 |  4075 | `			pExc->iFinallyDone = 1;` |
|        - |  4076 | `			/* Pass pResult so that 'return' inside finally can override the value */` |
|        3 |  4077 | `			rc = VmLocalExec(&(*pVm),&pExc->sFinally,pResult);` |
|        3 |  4078 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 |  4079 | `				goto Abort;` |
|        - |  4080 | `			}` |
|        1 |  4081 | `		}` |
|        1 |  4082 | `	}` |
|    43060 |  4083 | `	goto Done;` |
|        - |  4084 | `/*` |
|        - |  4085 | ` * HALT: P1 * *` |
|        - |  4086 | ` *` |
|        - |  4087 | ` * Program execution aborted: Clean up the mess left behind` |
|        - |  4088 | ` * and abort immediately.` |
|        - |  4089 | ` */` |
|        4 |  4090 | `case PH7_OP_HALT:` |
|        9 |  4091 | `	if( pInstr->iP1 ){` |
|        - |  4092 | `#ifdef UNTRUST` |
|        - |  4093 | `		if( pTos < pStack ){` |
|        - |  4094 | `			goto Abort;` |
|        - |  4095 | `		}` |
|        - |  4096 | `#endif` |
|        9 |  4097 | `		if( pLastRef ){` |
|      ! 0 |  4098 | `			*pLastRef = pTos->nIdx;` |
|      ! 0 |  4099 | `		}` |
|        9 |  4100 | `		if( pTos->iFlags & MEMOBJ_STRING ){` |
|        5 |  4101 | `			if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  4102 | `				/* Output the exit message */` |
|        7 |  4103 | `				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|        2 |  4104 | `					pVm->sVmConsumer.pUserData);` |
|        5 |  4105 | `				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));` |
|        3 |  4106 | `			}` |
|        7 |  4107 | `		}else if(pTos->iFlags & MEMOBJ_INT ){` |
|        - |  4108 | `			/* Record exit status */` |
|        5 |  4109 | `			pVm->iExitStatus = (sxi32)pTos->x.iVal;` |
|        2 |  4110 | `		}` |
|        9 |  4111 | `		VmPopOperand(&pTos,1);` |
|        4 |  4112 | `	}else if( pLastRef ){` |
|        - |  4113 | `		/* Nothing referenced */` |
|      ! 0 |  4114 | `		*pLastRef = SXU32_HIGH;` |
|      ! 0 |  4115 | `	}` |
|        - |  4116 | `	/* Check if we're in an included file context */` |
|        9 |  4117 | `	if( SySetUsed(&pVm->aFiles) > 0 ){` |
|        - |  4118 | `		/* Terminate the entire process */` |
|        9 |  4119 | `		exit(pVm->iExitStatus);` |
|        - |  4120 | `	}` |
|      ! 0 |  4121 | `	goto Abort;` |
|        - |  4122 | `/*` |
|        - |  4123 | ` * JMP: * P2 *` |
|        - |  4124 | ` *` |
|        - |  4125 | ` * Unconditional jump: The next instruction executed will be` |
|        - |  4126 | ` * the one at index P2 from the beginning of the program.` |
|        - |  4127 | ` */` |
|   248530 |  4128 | `case PH7_OP_JMP:` |
|   497106 |  4129 | `	pc = pInstr->iP2 - 1;` |
|   497106 |  4130 | `	break;` |
|        - |  4131 | `/*` |
|        - |  4132 | ` * JZ: P1 P2 *` |
|        - |  4133 | ` *` |
|        - |  4134 | ` * Take the jump if the top value is zero (FALSE jump).Pop the top most` |
|        - |  4135 | ` * entry in the stack if P1 is zero.` |
|        - |  4136 | ` */` |
|   589785 |  4137 | `case PH7_OP_JZ:` |
|        - |  4138 | `#ifdef UNTRUST` |
|        - |  4139 | `	if( pTos < pStack ){` |
|        - |  4140 | `		goto Abort;` |
|        - |  4141 | `	}` |
|        - |  4142 | `#endif` |
|        - |  4143 | `	/* Get a boolean value */` |
|  1179660 |  4144 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      172 |  4145 | `		PH7_MemObjToBool(pTos);` |
|       85 |  4146 | `	}` |
|  1179660 |  4147 | `	if( !pTos->x.iVal ){` |
|        - |  4148 | `		/* Take the jump */` |
|   606094 |  4149 | `		pc = pInstr->iP2 - 1;` |
|   303046 |  4150 | `	}` |
|  1179660 |  4151 | `	if( !pInstr->iP1 ){` |
|   935874 |  4152 | `		VmPopOperand(&pTos,1);` |
|   467958 |  4153 | `	}` |
|  1179660 |  4154 | `	break;` |
|        - |  4155 | `/*` |
|        - |  4156 | ` * JNZ: P1 P2 *` |
|        - |  4157 | ` *` |
|        - |  4158 | ` * Take the jump if the top value is not zero (TRUE jump).Pop the top most` |
|        - |  4159 | ` * entry in the stack if P1 is zero.` |
|        - |  4160 | ` */` |
|    61241 |  4161 | `case PH7_OP_JNZ:` |
|        - |  4162 | `#ifdef UNTRUST` |
|        - |  4163 | `	if( pTos < pStack ){` |
|        - |  4164 | `		goto Abort;` |
|        - |  4165 | `	}` |
|        - |  4166 | `#endif` |
|        - |  4167 | `	/* Get a boolean value */` |
|   122484 |  4168 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  4169 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  4170 | `	}` |
|   122484 |  4171 | `	if( pTos->x.iVal ){` |
|        - |  4172 | `		/* Take the jump */` |
|     5484 |  4173 | `		pc = pInstr->iP2 - 1;` |
|     2741 |  4174 | `	}` |
|   122484 |  4175 | `	if( !pInstr->iP1 ){` |
|      ! 0 |  4176 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  4177 | `	}` |
|   122484 |  4178 | `	break;` |
|        - |  4179 | `/*` |
|        - |  4180 | ` * NOOP: * * *` |
|        - |  4181 | ` *` |
|        - |  4182 | ` * Do nothing. This instruction is often useful as a jump` |
|        - |  4183 | ` * destination.` |
|        - |  4184 | ` */` |
|      ! 0 |  4185 | `case PH7_OP_NOOP:` |
|      ! 0 |  4186 | `	break;` |
|        - |  4187 | `/*` |
|        - |  4188 | ` * POP: P1 * *` |
|        - |  4189 | ` *` |
|        - |  4190 | ` * Pop P1 elements from the operand stack.` |
|        - |  4191 | ` */` |
|   454750 |  4192 | `case PH7_OP_POP: {` |
|   909546 |  4193 | `	sxi32 n = pInstr->iP1;` |
|   909546 |  4194 | `	if( &pTos[-n+1] < pStack ){` |
|        - |  4195 | `		/* TICKET 1433-51 Stack underflow must be handled at run-time */` |
|       50 |  4196 | `		n = (sxi32)(pTos - pStack);` |
|       24 |  4197 | `	}` |
|   909546 |  4198 | `	VmPopOperand(&pTos,n);` |
|   909546 |  4199 | `	break;` |
|        - |  4200 | `				 }` |
|        - |  4201 | `/*` |
|        - |  4202 | ` * DUP: * * *` |
|        - |  4203 | ` *` |
|        - |  4204 | ` * Duplicate the top of the stack.` |
|        - |  4205 | ` */` |
|       41 |  4206 | `case PH7_OP_DUP:` |
|        - |  4207 | `#ifdef UNTRUST` |
|        - |  4208 | `	if( pTos < pStack ){` |
|        - |  4209 | `		goto Abort;` |
|        - |  4210 | `	}` |
|        - |  4211 | `#endif` |
|       84 |  4212 | `	pTos++;` |
|       84 |  4213 | `	PH7_MemObjInit(pVm,pTos);` |
|       84 |  4214 | `	PH7_MemObjStore(pTos - 1,pTos);` |
|       84 |  4215 | `	break;` |
|        - |  4216 | `/*` |
|        - |  4217 | ` * NSSWITCH: * * P3` |
|        - |  4218 | ` *` |
|        - |  4219 | ` * Switch the active namespace at runtime.` |
|        - |  4220 | ` * P3 points to the namespace string (pool-allocated, NULL for global).` |
|        - |  4221 | ` */` |
|     7696 |  4222 | `case PH7_OP_NSSWITCH:` |
|    15394 |  4223 | `	SyBlobReset(&pVm->sNamespace);` |
|    15394 |  4224 | `	if( pInstr->p3 ){` |
|       98 |  4225 | `		const char *zNs = (const char *)pInstr->p3;` |
|       98 |  4226 | `		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));` |
|       48 |  4227 | `	}` |
|        - |  4228 | `	/* Clear namespace-scoped use-const imports */` |
|    15394 |  4229 | `	SyHashRelease(&pVm->hUseConstImports);` |
|    15394 |  4230 | `	SyHashInit(&pVm->hUseConstImports,&pVm->sAllocator,0,0);` |
|    15394 |  4231 | `	break;` |
|        - |  4232 | `/* OP_USECONST P1 * P3` |
|        - |  4233 | ` * Register a use-const import at runtime. P1 is the alias length,` |
|        - |  4234 | ` * P3 points to a two-pointer array: [0]=alias, [1]=FQN.` |
|        - |  4235 | ` * This is namespace-scoped: NSSWITCH clears all imports.` |
|        - |  4236 | ` */` |
|        7 |  4237 | `case PH7_OP_USECONST: {` |
|       16 |  4238 | `	char **azPair = (char **)pInstr->p3;` |
|       16 |  4239 | `	if( azPair ){` |
|       16 |  4240 | `		SyHashInsert(&pVm->hUseConstImports,azPair[0],(sxu32)pInstr->iP1,azPair[1]);` |
|        7 |  4241 | `	}` |
|       16 |  4242 | `	break;` |
|        - |  4243 | `				}` |
|        - |  4244 | `/*` |
|        - |  4245 | ` * CVT_INT: * * *` |
|        - |  4246 | ` *` |
|        - |  4247 | ` * Force the top of the stack to be an integer.` |
|        - |  4248 | ` */` |
|       80 |  4249 | `case PH7_OP_CVT_INT:` |
|        - |  4250 | `#ifdef UNTRUST` |
|        - |  4251 | `	if( pTos < pStack ){` |
|        - |  4252 | `		goto Abort;` |
|        - |  4253 | `	}` |
|        - |  4254 | `#endif` |
|      162 |  4255 | `	if((pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      111 |  4256 | `		PH7_MemObjToInteger(pTos);` |
|       55 |  4257 | `	}` |
|        - |  4258 | `	/* Invalidate any prior representation */` |
|      162 |  4259 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|      162 |  4260 | `	break;` |
|        - |  4261 | `/*` |
|        - |  4262 | ` * CVT_REAL: * * *` |
|        - |  4263 | ` *` |
|        - |  4264 | ` * Force the top of the stack to be a real.` |
|        - |  4265 | ` */` |
|        5 |  4266 | `case PH7_OP_CVT_REAL:` |
|        - |  4267 | `#ifdef UNTRUST` |
|        - |  4268 | `	if( pTos < pStack ){` |
|        - |  4269 | `		goto Abort;` |
|        - |  4270 | `	}` |
|        - |  4271 | `#endif` |
|       11 |  4272 | `	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  4273 | `		PH7_MemObjToReal(pTos);` |
|        3 |  4274 | `	}` |
|        - |  4275 | `	/* Invalidate any prior representation */` |
|       11 |  4276 | `	MemObjSetType(pTos,MEMOBJ_REAL);` |
|       11 |  4277 | `	break;` |
|        - |  4278 | `/*` |
|        - |  4279 | ` * CVT_STR: * * *` |
|        - |  4280 | ` *` |
|        - |  4281 | ` * Force the top of the stack to be a string.` |
|        - |  4282 | ` */` |
|      149 |  4283 | `case PH7_OP_CVT_STR:` |
|        - |  4284 | `#ifdef UNTRUST` |
|        - |  4285 | `	if( pTos < pStack ){` |
|        - |  4286 | `		goto Abort;` |
|        - |  4287 | `	}` |
|        - |  4288 | `#endif` |
|      300 |  4289 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      300 |  4290 | `		PH7_MemObjToString(pTos);` |
|      149 |  4291 | `	}` |
|      300 |  4292 | `	break;` |
|        - |  4293 | `/*` |
|        - |  4294 | ` * CVT_BOOL: * * *` |
|        - |  4295 | ` *` |
|        - |  4296 | ` * Force the top of the stack to be a boolean.` |
|        - |  4297 | ` */` |
|        5 |  4298 | `case PH7_OP_CVT_BOOL:` |
|        - |  4299 | `#ifdef UNTRUST` |
|        - |  4300 | `	if( pTos < pStack ){` |
|        - |  4301 | `		goto Abort;` |
|        - |  4302 | `	}` |
|        - |  4303 | `#endif` |
|       11 |  4304 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        7 |  4305 | `		PH7_MemObjToBool(pTos);` |
|        3 |  4306 | `	}` |
|       11 |  4307 | `	break;` |
|        - |  4308 | `/*` |
|        - |  4309 | ` * CVT_NULL: * * *` |
|        - |  4310 | ` *` |
|        - |  4311 | ` * Nullify the top of the stack.` |
|        - |  4312 | ` */` |
|        3 |  4313 | `case PH7_OP_CVT_NULL:` |
|        - |  4314 | `#ifdef UNTRUST` |
|        - |  4315 | `	if( pTos < pStack ){` |
|        - |  4316 | `		goto Abort;` |
|        - |  4317 | `	}` |
|        - |  4318 | `#endif` |
|        7 |  4319 | `	PH7_MemObjRelease(pTos);` |
|        7 |  4320 | `	break;` |
|        - |  4321 | `/*` |
|        - |  4322 | ` * CVT_NUMC: * * *` |
|        - |  4323 | ` *` |
|        - |  4324 | ` * Force the top of the stack to be a numeric type (integer,real or both).` |
|        - |  4325 | ` */` |
|      ! 0 |  4326 | `case PH7_OP_CVT_NUMC:` |
|        - |  4327 | `#ifdef UNTRUST` |
|        - |  4328 | `	if( pTos < pStack ){` |
|        - |  4329 | `		goto Abort;` |
|        - |  4330 | `	}` |
|        - |  4331 | `#endif` |
|        - |  4332 | `	/* Force a numeric cast */` |
|      ! 0 |  4333 | `	PH7_MemObjToNumeric(pTos);` |
|      ! 0 |  4334 | `	break;` |
|        - |  4335 | `/*` |
|        - |  4336 | ` * CVT_ARRAY: * * *` |
|        - |  4337 | ` *` |
|        - |  4338 | ` * Force the top of the stack to be a hashmap aka 'array'.` |
|        - |  4339 | ` */` |
|       10 |  4340 | `case PH7_OP_CVT_ARRAY:` |
|        - |  4341 | `#ifdef UNTRUST` |
|        - |  4342 | `	if( pTos < pStack ){` |
|        - |  4343 | `		goto Abort;` |
|        - |  4344 | `	}` |
|        - |  4345 | `#endif` |
|        - |  4346 | `	/* Force a hashmap cast */` |
|       21 |  4347 | `	rc = PH7_MemObjToHashmap(pTos);` |
|       21 |  4348 | `	if( rc != SXRET_OK ){` |
|        - |  4349 | `		/* Not so fatal,emit a simple warning */` |
|      ! 0 |  4350 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4351 | `			"PH7 engine is running out of memory while performing an array cast");` |
|      ! 0 |  4352 | `	}` |
|       21 |  4353 | `	break;` |
|        - |  4354 | `/*` |
|        - |  4355 | ` * CVT_OBJ: * * *` |
|        - |  4356 | ` *` |
|        - |  4357 | ` * Force the top of the stack to be a class instance (Object in the PHP jargon).` |
|        - |  4358 | ` */` |
|        8 |  4359 | `case PH7_OP_CVT_OBJ:` |
|        - |  4360 | `#ifdef UNTRUST` |
|        - |  4361 | `	if( pTos < pStack ){` |
|        - |  4362 | `		goto Abort;` |
|        - |  4363 | `	}` |
|        - |  4364 | `#endif` |
|       17 |  4365 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        - |  4366 | `		/* Force a 'stdClass()' cast */` |
|       17 |  4367 | `		PH7_MemObjToObject(pTos);` |
|        8 |  4368 | `	}` |
|       17 |  4369 | `	break;` |
|        - |  4370 | `/*` |
|        - |  4371 | ` * ERR_CTRL * * *` |
|        - |  4372 | ` *` |
|        - |  4373 | ` * Error control operator.` |
|        - |  4374 | ` */` |
|    15730 |  4375 | `case PH7_OP_ERR_CTRL:` |
|        - |  4376 | `	/*` |
|        - |  4377 | `	 * TICKET 1433-038:` |
|        - |  4378 | `	 * As of this version ,the error control operator '@' is a no-op,simply` |
|        - |  4379 | `	 * use the public API,to control error output.` |
|        - |  4380 | `	 */` |
|    31460 |  4381 | `	break;` |
|        - |  4382 | `/*` |
|        - |  4383 | ` * IS_A * * *` |
|        - |  4384 | ` *` |
|        - |  4385 | ` * Pop the top two operands from the stack and check whether the first operand` |
|        - |  4386 | ` * is an object and is an instance of the second operand (which must be a string` |
|        - |  4387 | ` * holding a class name or an object).` |
|        - |  4388 | ` * Push TRUE on success. FALSE otherwise.` |
|        - |  4389 | ` */` |
|       64 |  4390 | `case PH7_OP_IS_A:{` |
|      130 |  4391 | `	ph7_value *pNos = &pTos[-1];` |
|      130 |  4392 | `	sxi32 iRes = 0; /* assume false by default */` |
|        - |  4393 | `#ifdef UNTRUST` |
|        - |  4394 | `	if( pNos < pStack ){` |
|        - |  4395 | `		goto Abort;` |
|        - |  4396 | `	}` |
|        - |  4397 | `#endif` |
|      130 |  4398 | `	if( pNos->iFlags& MEMOBJ_OBJ ){` |
|      128 |  4399 | `		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      128 |  4400 | `		ph7_class *pClass = 0;` |
|        - |  4401 | `		/* Extract the target class */` |
|      128 |  4402 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4403 | `			/* Instance already loaded */` |
|      ! 0 |  4404 | `			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      128 |  4405 | `		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){` |
|      128 |  4406 | `			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|      128 |  4407 | `			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);` |
|        - |  4408 | `			/* Handle self/static/parent keywords */` |
|      128 |  4409 | `			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|        5 |  4410 | `				pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|      126 |  4411 | `			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|        3 |  4412 | `				pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      125 |  4413 | `			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|        5 |  4414 | `				ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|        5 |  4415 | `				if( pSelf && pSelf->pBase ){` |
|        5 |  4416 | `					pClass = pSelf->pBase;` |
|        2 |  4417 | `				}` |
|        3 |  4418 | `			}else{` |
|      118 |  4419 | `				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  4420 | `			}` |
|       63 |  4421 | `		}` |
|      128 |  4422 | `		if( pClass ){` |
|        - |  4423 | `			/* Perform the query */` |
|      128 |  4424 | `			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);` |
|       63 |  4425 | `		}` |
|       63 |  4426 | `	}` |
|        - |  4427 | `	/* Push result */` |
|      130 |  4428 | `	VmPopOperand(&pTos,1);` |
|      130 |  4429 | `	PH7_MemObjRelease(pTos);` |
|      130 |  4430 | `	pTos->x.iVal = iRes;` |
|      130 |  4431 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      130 |  4432 | `	break;` |
|        - |  4433 | `				 }` |
|        - |  4434 |  |
|        - |  4435 | `/*` |
|        - |  4436 | ` * LOADC P1 P2 *` |
|        - |  4437 | ` *` |
|        - |  4438 | ` * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.` |
|        - |  4439 | ` * If P1 is set,then this constant is candidate for expansion via user installable callbacks.` |
|        - |  4440 | ` */` |
|   997357 |  4441 | `case PH7_OP_LOADC: {` |
|        - |  4442 | `	ph7_value *pObj;` |
|        - |  4443 | `	/* Reserve a room */` |
|  1994760 |  4444 | `	pTos++;` |
|  2982501 |  4445 | `	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){` |
|  1994760 |  4446 | `		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){` |
|        - |  4447 | `			SyHashEntry *pEntry;` |
|        - |  4448 | `			/* Check use const imports first — imports take precedence */` |
|        - |  4449 | `			{` |
|        - |  4450 | `				SyHashEntry *pConstImport;` |
|    28988 |  4451 | `				pConstImport = SyHashGet(&pVm->hUseConstImports,` |
|    19324 |  4452 | `					SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19326 |  4453 | `				if( pConstImport ){` |
|       11 |  4454 | `					const char *zFQN = (const char *)pConstImport->pUserData;` |
|       11 |  4455 | `					pEntry = SyHashGet(&pVm->hConstant,zFQN,SyStrlen(zFQN));` |
|       11 |  4456 | `					if( pEntry ){` |
|       11 |  4457 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|       11 |  4458 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|       11 |  4459 | `						SyBlobReset(&pTos->sBlob);` |
|       11 |  4460 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|       11 |  4461 | `						pTos->nIdx = SXU32_HIGH;` |
|       11 |  4462 | `						break;` |
|        - |  4463 | `					}` |
|        - |  4464 | `					/* Import found but constant not defined — fall through */` |
|      ! 0 |  4465 | `				}` |
|        - |  4466 | `			}` |
|        - |  4467 | `			/* Candidate for expansion via user defined callbacks */` |
|    19316 |  4468 | `			pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob));` |
|    19316 |  4469 | `			if( pEntry ){` |
|    19310 |  4470 | `				ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|        - |  4471 | `				/* Set a NULL default value */` |
|    19310 |  4472 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    19310 |  4473 | `				SyBlobReset(&pTos->sBlob);` |
|        - |  4474 | `				/* Invoke the callback and deal with the expanded value */` |
|    19310 |  4475 | `				pCons->xExpand(pTos,pCons->pUserData);` |
|        - |  4476 | `				/* Mark as constant */` |
|    19310 |  4477 | `				pTos->nIdx = SXU32_HIGH;` |
|    19310 |  4478 | `				break;` |
|        - |  4479 | `			}` |
|        - |  4480 | `			/* Constant not found by bare name.  If a namespace is active and` |
|        - |  4481 | `			 * the name is unqualified, try namespace\name (PHP resolution order:` |
|        - |  4482 | `			 * use-const imports → current NS → global → string fallback).` |
|        - |  4483 | `			 * Absolute references (\NAME) skip the NS fallback too. */` |
|        - |  4484 | `			{` |
|        8 |  4485 | `				const char *zLit = (const char *)SyBlobData(&pObj->sBlob);` |
|        8 |  4486 | `				sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);` |
|        - |  4487 | `				sxu32 j;` |
|        8 |  4488 | `				int isQualified = (pInstr->iP1 & PH7_LOADC_ABSOLUTE) != 0;` |
|       24 |  4489 | `				for( j = 0; !isQualified && j < nLit; j++ ){` |
|       18 |  4490 | `					if( zLit[j] == '\\' ){ isQualified = 1; break; }` |
|       10 |  4491 | `				}` |
|        8 |  4492 | `				if( !isQualified && SyBlobLength(&pVm->sNamespace) > 0 ){` |
|        - |  4493 | `					/* Try current_namespace\name */` |
|      ! 0 |  4494 | `					SyBlobReset(&pVm->sWorker);` |
|      ! 0 |  4495 | `					SyBlobAppend(&pVm->sWorker,SyBlobData(&pVm->sNamespace),SyBlobLength(&pVm->sNamespace));` |
|      ! 0 |  4496 | `					SyBlobAppend(&pVm->sWorker,"\\",1);` |
|      ! 0 |  4497 | `					SyBlobAppend(&pVm->sWorker,zLit,nLit);` |
|      ! 0 |  4498 | `					pEntry = SyHashGet(&pVm->hConstant,SyBlobData(&pVm->sWorker),SyBlobLength(&pVm->sWorker));` |
|      ! 0 |  4499 | `					if( pEntry ){` |
|      ! 0 |  4500 | `						ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;` |
|      ! 0 |  4501 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4502 | `						SyBlobReset(&pTos->sBlob);` |
|      ! 0 |  4503 | `						pCons->xExpand(pTos,pCons->pUserData);` |
|      ! 0 |  4504 | `						pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4505 | `						break;` |
|        - |  4506 | `					}` |
|        - |  4507 | `					/* Not in current namespace either — fall through to global/string */` |
|      ! 0 |  4508 | `				}` |
|        8 |  4509 | `				if( isQualified ){` |
|        - |  4510 | `					/* Qualified name: must be a real constant. */` |
|        3 |  4511 | `					SyString *pErrFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        - |  4512 | `					SyBlob sErr;` |
|        3 |  4513 | `					SyBlobInit(&sErr,&pVm->sAllocator);` |
|        3 |  4514 | `					SyBlobFormat(&sErr,"PHP Fatal error:  Uncaught Error: Undefined constant \"%.*s\"",nLit,zLit);` |
|        3 |  4515 | `					if( pErrFile ){` |
|        3 |  4516 | `						SyBlobFormat(&sErr," in %.*s:%u",pErrFile->nByte,pErrFile->zString,1);` |
|        1 |  4517 | `					}` |
|        3 |  4518 | `					SyBlobAppend(&sErr,"\n",1);` |
|        3 |  4519 | `					VmCallErrorHandler(&(*pVm),&sErr);` |
|        3 |  4520 | `					SyBlobRelease(&sErr);` |
|        3 |  4521 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        3 |  4522 | `					pTos->nIdx = SXU32_HIGH;` |
|        3 |  4523 | `					goto LoadC_Done;` |
|        - |  4524 | `				}` |
|        - |  4525 | `			}` |
|        2 |  4526 | `		}` |
|  1975440 |  4527 | `		PH7_MemObjLoad(pObj,pTos);` |
|   987743 |  4528 | `	}else{` |
|        - |  4529 | `		/* Set a NULL value */` |
|      ! 0 |  4530 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4531 | `	}` |
|   987698 |  4532 | `LoadC_Done:` |
|        - |  4533 | `	/* Mark as constant */` |
|  1975442 |  4534 | `	pTos->nIdx = SXU32_HIGH;` |
|  1975442 |  4535 | `	break;` |
|        - |  4536 | `				  }` |
|        - |  4537 | `/*` |
|        - |  4538 | ` * LOAD: P1 * P3` |
|        - |  4539 | ` *` |
|        - |  4540 | ` * Load a variable where it's name is taken from the top of the stack or` |
|        - |  4541 | ` * from the P3 operand.` |
|        - |  4542 | ` * If P1 is set,then perform a lookup only.In other words do not create` |
|        - |  4543 | ` * the variable if non existent and push the NULL constant instead.` |
|        - |  4544 | ` */` |
|  1560056 |  4545 | `case PH7_OP_LOAD:{` |
|        - |  4546 | `	ph7_value *pObj;` |
|        - |  4547 | `	SyString sName;` |
|  3120334 |  4548 | `	if( pInstr->p3 == 0 ){` |
|        - |  4549 | `		/* Take the variable name from the top of the stack */` |
|        - |  4550 | `#ifdef UNTRUST` |
|        - |  4551 | `		if( pTos < pStack ){` |
|        - |  4552 | `			goto Abort;` |
|        - |  4553 | `		}` |
|        - |  4554 | `#endif` |
|        - |  4555 | `		/* Force a string cast */` |
|       19 |  4556 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  4557 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  4558 | `		}` |
|       19 |  4559 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       10 |  4560 | `	}else{` |
|  3120316 |  4561 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  4562 | `		/* Reserve a room for the target object */` |
|  3120316 |  4563 | `		pTos++;` |
|        - |  4564 | `	}` |
|        - |  4565 | `	/* Extract the requested memory object */` |
|  3120334 |  4566 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);` |
|  3120334 |  4567 | `	if( pObj == 0 ){` |
|      836 |  4568 | `		if( pInstr->iP1 ){` |
|        - |  4569 | `			/* Variable not found,load NULL */` |
|      836 |  4570 | `			if( !pInstr->p3 ){` |
|      ! 0 |  4571 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4572 | `			}else{` |
|      836 |  4573 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4574 | `			}` |
|      836 |  4575 | `			pTos->nIdx = SXU32_HIGH; /* Mark as constant */` |
|  1560475 |  4576 | `			break;` |
|      ! 0 |  4577 | `		}else{` |
|        - |  4578 | `			/* Fatal error */` |
|      ! 0 |  4579 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  4580 | `			goto Abort;` |
|        - |  4581 | `		}` |
|        - |  4582 | `	}` |
|        - |  4583 | `	/* Load variable contents */` |
|  3119500 |  4584 | `	PH7_MemObjLoad(pObj,pTos);` |
|  3119500 |  4585 | `	pTos->nIdx = pObj->nIdx;` |
|  3119500 |  4586 | `	break;` |
|        - |  4587 | `				   }` |
|        - |  4588 | `/*` |
|        - |  4589 | ` * LOAD_MAP P1 * *` |
|        - |  4590 | ` *` |
|        - |  4591 | ` * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.` |
|        - |  4592 | ` * If the P1 operand is greater than zero then pop P1 elements from the` |
|        - |  4593 | ` * stack and insert them (key => value pair) in the new hashmap.` |
|        - |  4594 | ` */` |
|    22260 |  4595 | `case PH7_OP_LOAD_MAP: {` |
|        - |  4596 | `	ph7_hashmap *pMap;` |
|        - |  4597 | `	/* Allocate a new hashmap instance */` |
|    44522 |  4598 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|    44522 |  4599 | `	if( pMap == 0 ){` |
|      ! 0 |  4600 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4601 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|      ! 0 |  4602 | `		goto Abort;` |
|        - |  4603 | `	}` |
|    44522 |  4604 | `	if( pInstr->iP1 > 0 ){` |
|     2574 |  4605 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|     2574 |  4606 | `		sxi32 rcSpread = SXRET_OK;` |
|        - |  4607 | `		/* Perform the insertion */` |
|     7862 |  4608 | `		while( pEntry < pTos ){` |
|     5306 |  4609 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|        - |  4610 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|        - |  4611 | `				 * semantics — string keys preserved (later wins), int keys` |
|        - |  4612 | `				 * renumbered. Same routine that backs array_merge. */` |
|       64 |  4613 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|       47 |  4614 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|       47 |  4615 | `					if( rcMerge != SXRET_OK ){` |
|        - |  4616 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|        - |  4617 | `						 * path — emit fatal and abort, leaving no partial` |
|        - |  4618 | `						 * map dangling. */` |
|      ! 0 |  4619 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      ! 0 |  4620 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|      ! 0 |  4621 | `						rcSpread = PH7_ABORT;` |
|      ! 0 |  4622 | `						break;` |
|        - |  4623 | `					}` |
|       24 |  4624 | `				}else{` |
|        - |  4625 | `					/* Throw a catchable Error matching PHP semantics. */` |
|       17 |  4626 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|       17 |  4627 | `					break;` |
|        1 |  4628 | `				}` |
|     5267 |  4629 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|        - |  4630 | `				/* Insertion by reference */` |
|      142 |  4631 | `				PH7_HashmapInsertByRef(pMap,` |
|       94 |  4632 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|       94 |  4633 | `					(sxu32)pEntry[1].x.iVal` |
|        - |  4634 | `					);` |
|       48 |  4635 | `			}else{` |
|        - |  4636 | `				/* Standard insertion */` |
|     7724 |  4637 | `				PH7_HashmapInsert(pMap,` |
|     5148 |  4638 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|     2574 |  4639 | `					&pEntry[1]` |
|        - |  4640 | `				);` |
|        - |  4641 | `			}` |
|        - |  4642 | `			/* Next pair on the stack */` |
|     5290 |  4643 | `			pEntry += 2;` |
|        2 |  4644 | `		}` |
|        - |  4645 | `		/* Pop P1 elements */` |
|     2574 |  4646 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|     2574 |  4647 | `		if( rcSpread != SXRET_OK ){` |
|        - |  4648 | `			/* Discard the partially-built map and propagate the exception. */` |
|       17 |  4649 | `			PH7_HashmapRelease(pMap,TRUE);` |
|       17 |  4650 | `			if( rcSpread == PH7_ABORT ){` |
|      ! 0 |  4651 | `				goto Abort;` |
|        - |  4652 | `			}` |
|        - |  4653 | `			{` |
|       17 |  4654 | `				VmFrame *pFrm2 = pVm->pFrame;` |
|       17 |  4655 | `				if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  4656 | `					pc = pFrm2->iExceptionJump - 1;` |
|        3 |  4657 | `					break;` |
|        - |  4658 | `				}` |
|        - |  4659 | `			}` |
|       15 |  4660 | `			goto Exception;` |
|        - |  4661 | `		}` |
|     1278 |  4662 | `	}` |
|        - |  4663 | `	/* Push the hashmap */` |
|    44506 |  4664 | `	pTos++;` |
|    44506 |  4665 | `	pTos->nIdx = SXU32_HIGH;` |
|    44506 |  4666 | `	pTos->x.pOther = pMap;` |
|    44506 |  4667 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|    44506 |  4668 | `	break;` |
|        - |  4669 | `					  }` |
|        - |  4670 | `/*` |
|        - |  4671 | ` * LOAD_LIST: P1 * *` |
|        - |  4672 | ` *` |
|        - |  4673 | ` * Assign hashmap entries values to the top P1 entries.` |
|        - |  4674 | ` * This is the VM implementation of the list() PHP construct.` |
|        - |  4675 | ` * Caveats:` |
|        - |  4676 | ` *  This implementation support only a single nesting level.` |
|        - |  4677 | ` */` |
|       48 |  4678 | `case PH7_OP_LOAD_LIST: {` |
|        - |  4679 | `	ph7_value *pEntry;` |
|       98 |  4680 | `	if( pInstr->iP1 <= 0 ){` |
|        - |  4681 | `		/* Empty list,break immediately */` |
|      ! 0 |  4682 | `		break;` |
|        - |  4683 | `	}` |
|       98 |  4684 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|        - |  4685 | `#ifdef UNTRUST` |
|        - |  4686 | `	if( &pEntry[-1] < pStack ){` |
|        - |  4687 | `		goto Abort;` |
|        - |  4688 | `	}` |
|        - |  4689 | `#endif` |
|       98 |  4690 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|       91 |  4691 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|        - |  4692 | `		ph7_hashmap_node *pNode;` |
|        - |  4693 | `		ph7_value sKey,*pObj;` |
|        - |  4694 | `		/* Start Copying */` |
|       91 |  4695 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|      283 |  4696 | `		while( pEntry <= pTos ){` |
|      193 |  4697 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|      165 |  4698 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|      165 |  4699 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      165 |  4700 | `					if( rc == SXRET_OK ){` |
|        - |  4701 | `						/* Store node value */` |
|      165 |  4702 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|       83 |  4703 | `					}else{` |
|        - |  4704 | `						/* Undefined array key */` |
|        - |  4705 | `						char zMsg[128];` |
|      ! 0 |  4706 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|      ! 0 |  4707 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  4708 | `						PH7_MemObjRelease(pObj);` |
|        - |  4709 | `					}` |
|       82 |  4710 | `				}` |
|       82 |  4711 | `			}` |
|      193 |  4712 | `			sKey.x.iVal++; /* Next numeric index */` |
|      193 |  4713 | `			pEntry++;` |
|        1 |  4714 | `		}` |
|       46 |  4715 | `	}else{` |
|        - |  4716 | `		/* Source is not an array */` |
|        - |  4717 | `		ph7_value *pObj;` |
|       18 |  4718 | `		while( pEntry <= pTos ){` |
|       12 |  4719 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|       12 |  4720 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|       12 |  4721 | `					PH7_MemObjRelease(pObj);` |
|        5 |  4722 | `				}` |
|        5 |  4723 | `			}` |
|       12 |  4724 | `			pEntry++;` |
|        2 |  4725 | `		}` |
|        8 |  4726 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - |  4727 | `			/* Emit PHP-compatible warning with type name */` |
|        3 |  4728 | `			const char *zType = "unknown";` |
|        3 |  4729 | `			sxi32 iFlags = pTos[-pInstr->iP1].iFlags;` |
|        - |  4730 | `			char zMsg[256];` |
|        3 |  4731 | `			if( iFlags & MEMOBJ_STRING ){` |
|        3 |  4732 | `				zType = "string";` |
|        1 |  4733 | `			}else if( iFlags & MEMOBJ_INT ){` |
|      ! 0 |  4734 | `				zType = "int";` |
|      ! 0 |  4735 | `			}else if( iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  4736 | `				zType = "float";` |
|      ! 0 |  4737 | `			}else if( iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  4738 | `				zType = "object";` |
|      ! 0 |  4739 | `			}else if( iFlags & MEMOBJ_RES ){` |
|      ! 0 |  4740 | `				zType = "resource";` |
|      ! 0 |  4741 | `			}` |
|        3 |  4742 | `			SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        3 |  4743 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        1 |  4744 | `		}` |
|        - |  4745 | `	}` |
|       98 |  4746 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       98 |  4747 | `	break;` |
|        - |  4748 | `					   }` |
|        - |  4749 | `/*` |
|        - |  4750 | ` * LOAD_IDX: P1 P2 *` |
|        - |  4751 | ` *` |
|        - |  4752 | ` * Load a hasmap entry where it's index (either numeric or string) is taken` |
|        - |  4753 | ` * from the stack.` |
|        - |  4754 | ` * If the index does not refer to a valid element,then push the NULL constant` |
|        - |  4755 | ` * instead.` |
|        - |  4756 | ` */` |
|   249611 |  4757 | `case PH7_OP_LOAD_IDX: {` |
|   499268 |  4758 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
|   499268 |  4759 | `	ph7_hashmap *pMap = 0;` |
|        - |  4760 | `	ph7_value *pIdx;` |
|   499268 |  4761 | `	pIdx = 0;` |
|   499268 |  4762 | `	if( pInstr->iP1 == 0 ){` |
|      ! 0 |  4763 | `		if( !pInstr->iP2){` |
|        - |  4764 | `			/* No available index,load NULL */` |
|      ! 0 |  4765 | `			if( pTos >= pStack ){` |
|      ! 0 |  4766 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4767 | `			}else{` |
|        - |  4768 | `				/* TICKET 1433-020: Empty stack */` |
|      ! 0 |  4769 | `				pTos++;` |
|      ! 0 |  4770 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      ! 0 |  4771 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4772 | `			}` |
|        - |  4773 | `			/* Emit a notice */` |
|      ! 0 |  4774 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|        - |  4775 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|      ! 0 |  4776 | `			break;` |
|        - |  4777 | `		}` |
|      ! 0 |  4778 | `	}else{` |
|   499268 |  4779 | `		pIdx = pTos;` |
|   499268 |  4780 | `		pTos--;` |
|        - |  4781 | `	}` |
|   499268 |  4782 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|        - |  4783 | `		/* String access */` |
|   387716 |  4784 | `		if( pIdx ){` |
|        - |  4785 | `			sxu32 nOfft;` |
|   387716 |  4786 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|        - |  4787 | `				/* Force an int cast */` |
|      ! 0 |  4788 | `				PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  4789 | `			}` |
|   387716 |  4790 | `			nOfft = (sxu32)pIdx->x.iVal;` |
|   387716 |  4791 | `			if( nOfft >= SyBlobLength(&pTos->sBlob) ){` |
|        - |  4792 | `				/* Invalid offset,load null */` |
|      ! 0 |  4793 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4794 | `			}else{` |
|   387716 |  4795 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
|   387716 |  4796 | `				int c = zData[nOfft];` |
|   387716 |  4797 | `				PH7_MemObjRelease(pTos);` |
|   387716 |  4798 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
|   387716 |  4799 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|        - |  4800 | `			}` |
|   193881 |  4801 | `		}else{` |
|        - |  4802 | `			/* No available index,load NULL */` |
|      ! 0 |  4803 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|        - |  4804 | `		}` |
|   387716 |  4805 | `		break;` |
|        - |  4806 | `	}` |
|   111554 |  4807 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  4808 | `		/* Object subscript: ArrayAccess dispatch.` |
|        - |  4809 | `		 * iP2 codes:` |
|        - |  4810 | `		 *   0 = read       → offsetGet` |
|        - |  4811 | `		 *   3 = ?? peek    → offsetExists; offsetGet on hit; arm coalesce` |
|        - |  4812 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|        - |  4813 | `		 *   4 = isset()    → offsetExists` |
|        - |  4814 | `		 *   5 = unset()    → offsetUnset` |
|        - |  4815 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit */` |
|      126 |  4816 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|      126 |  4817 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|      126 |  4818 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  4819 | `			ph7_class_method *pMeth;` |
|        - |  4820 | `			ph7_value sResult;` |
|        - |  4821 | `			ph7_value *apArg[1];` |
|      124 |  4822 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3) && pIdx == 0 ){` |
|        - |  4823 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|      ! 0 |  4824 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|        - |  4825 | `					"Cannot use [] for reading");` |
|      ! 0 |  4826 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  4827 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  4828 | `				break;` |
|        - |  4829 | `			}` |
|      124 |  4830 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|      124 |  4831 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 ){` |
|        - |  4832 | `				/* isset, empty, and ??= all start with offsetExists. */` |
|       51 |  4833 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4834 | `					"offsetExists",sizeof("offsetExists")-1);` |
|       51 |  4835 | `				apArg[0] = pIdx;` |
|       51 |  4836 | `				if( pMeth ){` |
|       51 |  4837 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       26 |  4838 | `				}` |
|       99 |  4839 | `			}else if( pInstr->iP2 == 5 ){` |
|        9 |  4840 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4841 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|        9 |  4842 | `				apArg[0] = pIdx;` |
|        9 |  4843 | `				if( pMeth ){` |
|        9 |  4844 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|        4 |  4845 | `				}` |
|        5 |  4846 | `			}else{` |
|       66 |  4847 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4848 | `					"offsetGet",sizeof("offsetGet")-1);` |
|       66 |  4849 | `				apArg[0] = pIdx;` |
|       66 |  4850 | `				if( pMeth ){` |
|       66 |  4851 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|       32 |  4852 | `				}` |
|        - |  4853 | `			}` |
|      124 |  4854 | `			if( pInstr->iP2 == 4 ){` |
|        - |  4855 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|        - |  4856 | `				 * right truth value AND skips its "Expecting a variable not` |
|        - |  4857 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|       33 |  4858 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       33 |  4859 | `				PH7_MemObjRelease(pTos);` |
|       33 |  4860 | `				pTos->nIdx = SXU32_HIGH;` |
|       33 |  4861 | `				if( bExists ){` |
|       17 |  4862 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       17 |  4863 | `					pTos->x.iVal = 1;` |
|        9 |  4864 | `				}else{` |
|       17 |  4865 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        1 |  4866 | `				}` |
|      108 |  4867 | `			}else if( pInstr->iP2 == 5 ){` |
|        - |  4868 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|        - |  4869 | `				 * vm_builtin_unset is a harmless no-op. */` |
|        9 |  4870 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4871 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4872 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       88 |  4873 | `			}else if( pInstr->iP2 == 6 ){` |
|        - |  4874 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|        - |  4875 | `				 * without calling offsetGet. If true, call offsetGet and` |
|        - |  4876 | `				 * push the value so PH7_builtin_empty evaluates emptiness. */` |
|       11 |  4877 | `				int bExists = ph7_value_to_bool(&sResult);` |
|       11 |  4878 | `				PH7_MemObjRelease(&sResult);` |
|       11 |  4879 | `				PH7_MemObjRelease(pTos);` |
|       11 |  4880 | `				pTos->nIdx = SXU32_HIGH;` |
|       11 |  4881 | `				if( !bExists ){` |
|        3 |  4882 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        2 |  4883 | `				}else{` |
|        9 |  4884 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4885 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        - |  4886 | `					ph7_value sValue;` |
|        9 |  4887 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4888 | `					apArg[0] = pIdx;` |
|        9 |  4889 | `					if( pGet ){` |
|        9 |  4890 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        4 |  4891 | `					}` |
|        9 |  4892 | `					PH7_MemObjStore(&sValue,pTos);` |
|        9 |  4893 | `					PH7_MemObjRelease(&sValue);` |
|        - |  4894 | `				}` |
|       11 |  4895 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|       11 |  4896 | `				break; /* skip the duplicate sResult release below */` |
|       74 |  4897 | `			}else if( pInstr->iP2 == 3 ){` |
|        - |  4898 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|        - |  4899 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|        - |  4900 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|        - |  4901 | `				 *     and push NULL.` |
|        - |  4902 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|        9 |  4903 | `				int bExists = ph7_value_to_bool(&sResult);` |
|        9 |  4904 | `				int bShouldArm = !bExists;` |
|        - |  4905 | `				ph7_value sValue;` |
|        9 |  4906 | `				PH7_MemObjRelease(&sResult);` |
|        - |  4907 | `				/* Reset any prior arming defensively */` |
|        9 |  4908 | `				VmCoalesceDisarm(pVm);` |
|        9 |  4909 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|        9 |  4910 | `				if( bExists ){` |
|        5 |  4911 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  4912 | `						"offsetGet",sizeof("offsetGet")-1);` |
|        5 |  4913 | `					apArg[0] = pIdx;` |
|        5 |  4914 | `					if( pGet ){` |
|        5 |  4915 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|        2 |  4916 | `					}` |
|        5 |  4917 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|        3 |  4918 | `						bShouldArm = 1;` |
|        1 |  4919 | `					}` |
|        2 |  4920 | `				}` |
|        9 |  4921 | `				PH7_MemObjRelease(pTos);` |
|        9 |  4922 | `				pTos->nIdx = SXU32_HIGH;` |
|        9 |  4923 | `				if( bShouldArm ){` |
|        - |  4924 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|        - |  4925 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|        - |  4926 | `					 * intervening expression evaluation. */` |
|        7 |  4927 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|        7 |  4928 | `					if( pIdx ){` |
|        7 |  4929 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|        3 |  4930 | `					}` |
|        7 |  4931 | `					pVm->pCoalesceObj = pInst;` |
|        7 |  4932 | `					pInst->iRef++;` |
|        7 |  4933 | `					pVm->bCoalesceArmed = 1;` |
|        4 |  4934 | `				}else{` |
|        3 |  4935 | `					PH7_MemObjStore(&sValue,pTos);` |
|        - |  4936 | `				}` |
|        9 |  4937 | `				PH7_MemObjRelease(&sValue);` |
|        9 |  4938 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        9 |  4939 | `				break;` |
|      ! 0 |  4940 | `			}else{` |
|        - |  4941 | `				/* offsetGet: replace pTos with the returned value. */` |
|       66 |  4942 | `				PH7_MemObjRelease(pTos);` |
|       66 |  4943 | `				PH7_MemObjStore(&sResult,pTos);` |
|       66 |  4944 | `				pTos->nIdx = SXU32_HIGH;` |
|        - |  4945 | `			}` |
|      106 |  4946 | `			PH7_MemObjRelease(&sResult);` |
|      106 |  4947 | `			if( pIdx ){` |
|      106 |  4948 | `				PH7_MemObjRelease(pIdx);` |
|       52 |  4949 | `			}` |
|      106 |  4950 | `			break;` |
|        - |  4951 | `		}` |
|        - |  4952 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|        - |  4953 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|        3 |  4954 | `		if( pInst ){` |
|        - |  4955 | `			char zMsg[256];` |
|        3 |  4956 | `			SyString *pName = &pInst->pClass->sName;` |
|        4 |  4957 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  4958 | `				"Cannot use object of type %.*s as array",` |
|        2 |  4959 | `				(int)pName->nByte,pName->zString);` |
|        3 |  4960 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  4961 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|        3 |  4962 | `			PH7_MemObjRelease(pTos);` |
|        3 |  4963 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  4964 | `			if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  4965 | `			break;` |
|        - |  4966 | `		}` |
|      ! 0 |  4967 | `	}` |
|   111430 |  4968 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        3 |  4969 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  4970 | `			ph7_value *pObj;` |
|        3 |  4971 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  4972 | `				PH7_MemObjToHashmap(pObj);` |
|        3 |  4973 | `				PH7_MemObjLoad(pObj,pTos);` |
|        1 |  4974 | `			}` |
|        1 |  4975 | `		}` |
|        1 |  4976 | `	}` |
|   111430 |  4977 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|   111430 |  4978 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|   111430 |  4979 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|        - |  4980 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|        - |  4981 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|        - |  4982 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|        - |  4983 | `			 * NOT separate — that would defeat COW on every element read.` |
|        - |  4984 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|        - |  4985 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|      894 |  4986 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|      446 |  4987 | `		}` |
|        - |  4988 | `		/* Point to the hashmap */` |
|   111430 |  4989 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
|   111430 |  4990 | `		if( pIdx ){` |
|        - |  4991 | `			/* Load the desired entry */` |
|   111430 |  4992 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|    55714 |  4993 | `		}` |
|   111430 |  4994 | `		if( pInstr->iP2 == 3 ){` |
|        - |  4995 | `			/* Null coalescing assign peek mode: separate only when we will` |
|        - |  4996 | `			 * actually write back. If the looked-up value is non-null, the` |
|        - |  4997 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|        - |  4998 | `			 * the parent can stay shared. If the value is null or the key is` |
|        - |  4999 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|        - |  5000 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|        - |  5001 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|        - |  5002 | `			 * correct for the outermost write. */` |
|       19 |  5003 | `			int needWrite = (rc != SXRET_OK);` |
|       19 |  5004 | `			if( !needWrite && pNode ){` |
|       13 |  5005 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|       13 |  5006 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|        7 |  5007 | `					needWrite = 1;` |
|        3 |  5008 | `				}` |
|        6 |  5009 | `			}` |
|       19 |  5010 | `			if( needWrite ){` |
|       13 |  5011 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|       13 |  5012 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|        - |  5013 | `					/* The map was actually copied — re-lookup so pNode points` |
|        - |  5014 | `					 * into the new map's storage. */` |
|        7 |  5015 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        7 |  5016 | `					if( pIdx ){` |
|        7 |  5017 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|        3 |  5018 | `					}` |
|        3 |  5019 | `				}` |
|        6 |  5020 | `			}` |
|        9 |  5021 | `		}` |
|   111430 |  5022 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|        - |  5023 | `			/* Create a new empty entry */` |
|      273 |  5024 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|      273 |  5025 | `			if( rc == SXRET_OK ){` |
|        - |  5026 | `				/* Point to the last inserted entry */` |
|      273 |  5027 | `				pNode = pMap->pLast;` |
|      136 |  5028 | `			}` |
|      136 |  5029 | `		}` |
|    55714 |  5030 | `	}` |
|   111430 |  5031 | `	if( rc != SXRET_OK && pInstr->iP2 == 2 && pIdx ){` |
|        - |  5032 | `		/* List destructuring context: emit PHP-compatible warning for missing key */` |
|        - |  5033 | `		char zMsg[128];` |
|      ! 0 |  5034 | `		if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5035 | `			PH7_MemObjToInteger(pIdx);` |
|      ! 0 |  5036 | `		}` |
|      ! 0 |  5037 | `		SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)pIdx->x.iVal);` |
|      ! 0 |  5038 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|      ! 0 |  5039 | `	}` |
|   111430 |  5040 | `	if( pIdx ){` |
|   111430 |  5041 | `		PH7_MemObjRelease(pIdx);` |
|    55714 |  5042 | `	}` |
|   111430 |  5043 | `	if( rc == SXRET_OK ){` |
|        - |  5044 | `		/* Load entry contents */` |
|    49452 |  5045 | `		if( pMap->iRef < 2 ){` |
|        - |  5046 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|        - |  5047 | `			 * of the entry value,rather than pointing to it.` |
|        - |  5048 | `			 */` |
|       28 |  5049 | `			pTos->nIdx = SXU32_HIGH;` |
|       28 |  5050 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|       15 |  5051 | `		}else{` |
|    49426 |  5052 | `			pTos->nIdx = pNode->nValIdx;` |
|    49426 |  5053 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|    49426 |  5054 | `			PH7_HashmapUnref(pMap);` |
|        - |  5055 | `		}` |
|    24727 |  5056 | `	}else{` |
|        - |  5057 | `		/* No such entry,load NULL */` |
|    61980 |  5058 | `		PH7_MemObjRelease(pTos);` |
|    61980 |  5059 | `		pTos->nIdx = SXU32_HIGH;` |
|        - |  5060 | `	}` |
|   111430 |  5061 | `	break;` |
|        - |  5062 | `					  }` |
|        - |  5063 | `/*` |
|        - |  5064 | ` * LOAD_CLOSURE * * P3` |
|        - |  5065 | ` *` |
|        - |  5066 | ` * Set-up closure environment described by the P3 oeprand and push the closure` |
|        - |  5067 | ` * name in the stack.` |
|        - |  5068 | ` */` |
|       47 |  5069 | `case PH7_OP_LOAD_CLOSURE:{` |
|       96 |  5070 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|       96 |  5071 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  5072 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|        - |  5073 | `		ph7_vm_func *pClosure;` |
|        - |  5074 | `		char *zName;` |
|        - |  5075 | `		sxu32 mLen;` |
|        - |  5076 | `		sxu32 n;` |
|        - |  5077 | `		/* Create a new VM function */` |
|       96 |  5078 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|        - |  5079 | `		/* Generate an unique closure name */` |
|       96 |  5080 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|       96 |  5081 | `		if( pClosure == 0 \|\| zName == 0){` |
|      ! 0 |  5082 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|      ! 0 |  5083 | `			goto Abort;` |
|        - |  5084 | `		}` |
|       96 |  5085 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|       96 |  5086 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|      ! 0 |  5087 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|      ! 0 |  5088 | `		}` |
|        - |  5089 | `		/* Zero the stucture */` |
|       96 |  5090 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|        - |  5091 | `		/* Perform a structure assignment on read-only items */` |
|       96 |  5092 | `		pClosure->aArgs = pFunc->aArgs;` |
|       96 |  5093 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|       96 |  5094 | `		pClosure->aStatic = pFunc->aStatic;` |
|       96 |  5095 | `		pClosure->iFlags = pFunc->iFlags;` |
|       96 |  5096 | `		pClosure->pUserData = pFunc->pUserData;` |
|       96 |  5097 | `		pClosure->sSignature = pFunc->sSignature;` |
|       96 |  5098 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|       96 |  5099 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|       96 |  5100 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|       96 |  5101 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|       96 |  5102 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|        - |  5103 | `		/* Register the closure */` |
|       96 |  5104 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|        - |  5105 | `		/* Set up closure environment */` |
|       96 |  5106 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|       96 |  5107 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      256 |  5108 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|        - |  5109 | `			ph7_value *pValue;` |
|      162 |  5110 | `			pEnv = &aEnv[n];` |
|      162 |  5111 | `			sEnv.sName  = pEnv->sName;` |
|      162 |  5112 | `			sEnv.iFlags = pEnv->iFlags;` |
|      162 |  5113 | `			sEnv.nIdx = SXU32_HIGH;` |
|      162 |  5114 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|      162 |  5115 | `			if( sEnv.iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  5116 | `				/* Pass by reference */` |
|      ! 0 |  5117 | `				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - |  5118 | `					"Closure: Pass by reference is disabled in the current release of the PH7 engine,PH7 is switching to pass by value"` |
|        - |  5119 | `					);` |
|      ! 0 |  5120 | `			}` |
|        - |  5121 | `			/* Standard pass by value */` |
|      162 |  5122 | `			pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|      162 |  5123 | `			if( pValue ){` |
|        - |  5124 | `				/* Copy imported value */` |
|       72 |  5125 | `				PH7_MemObjStore(pValue,&sEnv.sValue);` |
|       35 |  5126 | `			}` |
|        - |  5127 | `			/* Insert the imported variable */` |
|      162 |  5128 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|       82 |  5129 | `		}` |
|        - |  5130 | `		/* Finally,load the closure name on the stack */` |
|       96 |  5131 | `		pTos++;` |
|       96 |  5132 | `		PH7_MemObjStringAppend(pTos,zName,mLen);` |
|       47 |  5133 | `	}` |
|       96 |  5134 | `	break;` |
|        - |  5135 | `						 }` |
|        - |  5136 | `/*` |
|        - |  5137 | ` * STORE * P2 P3` |
|        - |  5138 | ` *` |
|        - |  5139 | ` * Perform a store (Assignment) operation.` |
|        - |  5140 | ` */` |
|   140658 |  5141 | `case PH7_OP_STORE: {` |
|        - |  5142 | `	ph7_value *pObj;` |
|        - |  5143 | `	SyString sName;` |
|        - |  5144 | `#ifdef UNTRUST` |
|        - |  5145 | `	if( pTos < pStack ){` |
|        - |  5146 | `		goto Abort;` |
|        - |  5147 | `	}` |
|        - |  5148 | `#endif` |
|   281318 |  5149 | `	if( pInstr->iP2 ){` |
|        - |  5150 | `		sxu32 nIdx;` |
|        - |  5151 | `		sxi32 rcT;` |
|        - |  5152 | `		/* Member store operation */` |
|     4972 |  5153 | `		nIdx = pTos->nIdx;` |
|     4972 |  5154 | `		VmPopOperand(&pTos,1);` |
|     4972 |  5155 | `		if( nIdx == SXU32_HIGH ){` |
|        5 |  5156 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5157 | `				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");` |
|        5 |  5158 | `			pTos->nIdx = SXU32_HIGH;` |
|        3 |  5159 | `		}else{` |
|        - |  5160 | `			/* Enforce typed property declaration if any. May coerce the` |
|        - |  5161 | `			 * incoming value in place (weak mode) or throw TypeError. */` |
|     4968 |  5162 | `			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos);` |
|     4968 |  5163 | `			if( rcT == PH7_ABORT ){` |
|      ! 0 |  5164 | `				goto Abort;` |
|        - |  5165 | `			}` |
|     4968 |  5166 | `			if( rcT == PH7_EXCEPTION ){` |
|        - |  5167 | `				/* TypeError was thrown. Pop the rejected rvalue and hand` |
|        - |  5168 | `				 * control to the nearest catch block if any, otherwise` |
|        - |  5169 | `				 * propagate out of the VM loop. */` |
|       37 |  5170 | `				VmPopOperand(&pTos,1);` |
|        - |  5171 | `				{` |
|       37 |  5172 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       37 |  5173 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|       37 |  5174 | `						pc = pFrm2->iExceptionJump - 1;` |
|   140677 |  5175 | `						break;` |
|        - |  5176 | `					}` |
|        - |  5177 | `				}` |
|      ! 0 |  5178 | `				goto Exception;` |
|        - |  5179 | `			}` |
|        - |  5180 | `			/* Point to the desired memory object */` |
|     4932 |  5181 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     4932 |  5182 | `			if( pObj ){` |
|        - |  5183 | `				/* Perform the store operation */` |
|     4932 |  5184 | `				PH7_MemObjStore(pTos,pObj);` |
|     2465 |  5185 | `			}` |
|        - |  5186 | `		}` |
|     4936 |  5187 | `		break;` |
|   276348 |  5188 | `	}else if( pInstr->p3 == 0 ){` |
|        - |  5189 | `		/* Take the variable name from the next on the stack */` |
|        7 |  5190 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  5191 | `			/* Force a string cast */` |
|      ! 0 |  5192 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  5193 | `		}` |
|        7 |  5194 | `		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        7 |  5195 | `		pTos--;` |
|        - |  5196 | `#ifdef UNTRUST` |
|        - |  5197 | `		if( pTos < pStack  ){` |
|        - |  5198 | `			goto Abort;` |
|        - |  5199 | `		}` |
|        - |  5200 | `#endif` |
|        4 |  5201 | `	}else{` |
|   276342 |  5202 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  5203 | `	}` |
|        - |  5204 | `	/* Extract the desired variable and if not available dynamically create it */` |
|   276348 |  5205 | `	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);` |
|   276348 |  5206 | `	if( pObj == 0 ){` |
|      ! 0 |  5207 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  5208 | `			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  5209 | `		goto Abort;` |
|        - |  5210 | `	}` |
|   276348 |  5211 | `	if( !pInstr->p3 ){` |
|        7 |  5212 | `		PH7_MemObjRelease(&pTos[1]);` |
|        3 |  5213 | `	}` |
|        - |  5214 | `	/* Perform the store operation */` |
|   276348 |  5215 | `	PH7_MemObjStore(pTos,pObj);` |
|   276348 |  5216 | `	break;` |
|        - |  5217 | `				   }` |
|        - |  5218 | `/*` |
|        - |  5219 | ` * STORE_IDX:   P1 * P3` |
|        - |  5220 | ` * STORE_IDX_R: P1 * P3` |
|        - |  5221 | ` *` |
|        - |  5222 | ` * Perfrom a store operation an a hashmap entry.` |
|        - |  5223 | ` */` |
|    95439 |  5224 | `case PH7_OP_STORE_IDX:` |
|        - |  5225 | `case PH7_OP_STORE_IDX_REF: {` |
|   190880 |  5226 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|        - |  5227 | `	ph7_value *pKey;` |
|        - |  5228 | `	sxu32 nIdx;` |
|   190880 |  5229 | `	if( pInstr->iP1 ){` |
|        - |  5230 | `		/* Key is next on stack */` |
|    62738 |  5231 | `		pKey = pTos;` |
|    62738 |  5232 | `		pTos--;` |
|    31370 |  5233 | `	}else{` |
|   128144 |  5234 | `		pKey = 0;` |
|        - |  5235 | `	}` |
|   190880 |  5236 | `	nIdx = pTos->nIdx;` |
|        - |  5237 | `	{` |
|        - |  5238 | `		/* ArrayAccess::offsetSet dispatch.` |
|        - |  5239 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|        - |  5240 | `		 * the backing variable slot at nIdx. */` |
|   190880 |  5241 | `		ph7_class_instance *pInst = 0;` |
|   190880 |  5242 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       34 |  5243 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
|   190864 |  5244 | `		}else if( nIdx != SXU32_HIGH ){` |
|   190848 |  5245 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   190848 |  5246 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|      ! 0 |  5247 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|      ! 0 |  5248 | `			}` |
|    95423 |  5249 | `		}` |
|   190880 |  5250 | `		if( pInst ){` |
|       34 |  5251 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|       34 |  5252 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|        - |  5253 | `				ph7_class_method *pMeth;` |
|        - |  5254 | `				ph7_value sNullKey;` |
|        - |  5255 | `				ph7_value *apArg[2];` |
|       32 |  5256 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|      ! 0 |  5257 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  5258 | `						"Cannot assign by reference to overloaded object");` |
|      ! 0 |  5259 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      ! 0 |  5260 | `					VmPopOperand(&pTos,2); /* container + value */` |
|      ! 0 |  5261 | `					break;` |
|        - |  5262 | `				}` |
|       32 |  5263 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  5264 | `					"offsetSet",sizeof("offsetSet")-1);` |
|        - |  5265 | `				/* Pop container; pTos now points to the value */` |
|       32 |  5266 | `				VmPopOperand(&pTos,1);` |
|       32 |  5267 | `				if( pKey == 0 ){` |
|        7 |  5268 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|        7 |  5269 | `					apArg[0] = &sNullKey;` |
|        4 |  5270 | `				}else{` |
|       26 |  5271 | `					apArg[0] = pKey;` |
|        - |  5272 | `				}` |
|       32 |  5273 | `				apArg[1] = pTos;` |
|       32 |  5274 | `				if( pMeth ){` |
|       32 |  5275 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|       15 |  5276 | `				}` |
|       32 |  5277 | `				if( pKey ){` |
|       26 |  5278 | `					PH7_MemObjRelease(pKey);` |
|       14 |  5279 | `				}else{` |
|        7 |  5280 | `					PH7_MemObjRelease(&sNullKey);` |
|        - |  5281 | `				}` |
|        - |  5282 | `				/* Pop the value */` |
|       32 |  5283 | `				VmPopOperand(&pTos,1);` |
|       32 |  5284 | `				break;` |
|        - |  5285 | `			}` |
|        - |  5286 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|        - |  5287 | `			 * than silently coercing the object into a hashmap (which is` |
|        - |  5288 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|        - |  5289 | `			 * a few lines below). Match PHP. */` |
|        - |  5290 | `			{` |
|        - |  5291 | `				char zMsg[256];` |
|        3 |  5292 | `				SyString *pName = &pInst->pClass->sName;` |
|        4 |  5293 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - |  5294 | `					"Cannot use object of type %.*s as array",` |
|        2 |  5295 | `					(int)pName->nByte,pName->zString);` |
|        3 |  5296 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|        3 |  5297 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|        3 |  5298 | `				VmPopOperand(&pTos,2); /* container + value */` |
|        3 |  5299 | `				if( rc == SXERR_ABORT ){ goto Abort; }` |
|        3 |  5300 | `				break;` |
|        - |  5301 | `			}` |
|        - |  5302 | `		}` |
|        - |  5303 | `	}` |
|   190848 |  5304 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  5305 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|        - |  5306 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|        - |  5307 | `		 * checking true sharing count, then re-add after separation. */` |
|   190796 |  5308 | `		if( nIdx != SXU32_HIGH ){` |
|   190796 |  5309 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|   286193 |  5310 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|   190796 |  5311 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5312 | `				/* Only adjust refcount / perform COW if the backing variable` |
|        - |  5313 | `				 * is still sharing the same hashmap instance. This mirrors` |
|        - |  5314 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|        - |  5315 | `				 * refcounts if the backing array was already separated. */` |
|   190796 |  5316 | `				if( pBacking->x.pOther == (void *)pCur ){` |
|   190796 |  5317 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
|   190796 |  5318 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|   190796 |  5319 | `					pMap->iRef++;  /* Re-add stack ref */` |
|   190796 |  5320 | `					pTos->x.pOther = pMap;` |
|    95399 |  5321 | `				}else{` |
|        - |  5322 | `					/* Backing variable no longer points at pCur: skip COW here` |
|        - |  5323 | `					 * and operate on the hashmap currently on the stack. */` |
|      ! 0 |  5324 | `					pMap = pCur;` |
|        - |  5325 | `				}` |
|    95399 |  5326 | `			}else{` |
|      ! 0 |  5327 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5328 | `			}` |
|    95399 |  5329 | `		}else{` |
|      ! 0 |  5330 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  5331 | `		}` |
|   190796 |  5332 | `		if( pMap->iRef < 2 ){` |
|        - |  5333 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|        - |  5334 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|        - |  5335 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|        - |  5336 | `			 * no code checks iRef for COW decisions. */` |
|      ! 0 |  5337 | `			pMap->iRef = 2;` |
|      ! 0 |  5338 | `		}` |
|    95399 |  5339 | `	}else{` |
|        - |  5340 | `		ph7_value *pObj;` |
|       53 |  5341 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       53 |  5342 | `		if( pObj == 0 ){` |
|      ! 0 |  5343 | `			if( pKey ){` |
|      ! 0 |  5344 | `			  PH7_MemObjRelease(pKey);` |
|      ! 0 |  5345 | `			}` |
|      ! 0 |  5346 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  5347 | `			break;` |
|        - |  5348 | `		}` |
|        - |  5349 | `		/* Phase#1: Load the array */` |
|       53 |  5350 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|       53 |  5351 | `			VmPopOperand(&pTos,1);` |
|       53 |  5352 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|        - |  5353 | `				/* Force a string cast */` |
|      ! 0 |  5354 | `				PH7_MemObjToString(pTos);` |
|      ! 0 |  5355 | `			}` |
|       53 |  5356 | `			if( pKey == 0 ){` |
|        - |  5357 | `				/* Append string */` |
|        3 |  5358 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|        3 |  5359 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        1 |  5360 | `				}` |
|        2 |  5361 | `			}else{` |
|        - |  5362 | `				sxu32 nOfft;` |
|       51 |  5363 | `				if((pKey->iFlags & MEMOBJ_INT)){` |
|        - |  5364 | `					/* Force an int cast */` |
|       51 |  5365 | `					PH7_MemObjToInteger(pKey);` |
|       25 |  5366 | `				}` |
|       51 |  5367 | `				nOfft = (sxu32)pKey->x.iVal;` |
|       51 |  5368 | `				if( nOfft < SyBlobLength(&pObj->sBlob) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       51 |  5369 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|       51 |  5370 | `					char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|       51 |  5371 | `					zData[nOfft] = zBlob[0];` |
|       26 |  5372 | `				}else{` |
|      ! 0 |  5373 | `					if( SyBlobLength(&pTos->sBlob) >= sizeof(char) ){` |
|        - |  5374 | `						/* Perform an append operation */` |
|      ! 0 |  5375 | `						SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),sizeof(char));` |
|      ! 0 |  5376 | `					}` |
|        - |  5377 | `				}` |
|        - |  5378 | `			}` |
|       53 |  5379 | `			if( pKey ){` |
|       51 |  5380 | `			  PH7_MemObjRelease(pKey);` |
|       25 |  5381 | `			}` |
|       53 |  5382 | `			break;` |
|      ! 0 |  5383 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|        - |  5384 | `			/* Force a hashmap cast  */` |
|      ! 0 |  5385 | `			rc = PH7_MemObjToHashmap(pObj);` |
|      ! 0 |  5386 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  5387 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|      ! 0 |  5388 | `				goto Abort;` |
|        - |  5389 | `			}` |
|      ! 0 |  5390 | `		}` |
|        - |  5391 | `		/* COW separate the backing variable before mutation */` |
|      ! 0 |  5392 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|        - |  5393 | `	}` |
|   190796 |  5394 | `	VmPopOperand(&pTos,1);` |
|        - |  5395 | `	/* Phase#2: Perform the insertion */` |
|   190796 |  5396 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|        - |  5397 | `		/* Insertion by reference */` |
|       15 |  5398 | `		PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|        8 |  5399 | `	}else{` |
|   190782 |  5400 | `		PH7_HashmapInsert(pMap,pKey,pTos);` |
|        - |  5401 | `	}` |
|   190796 |  5402 | `	if( pKey ){` |
|    62662 |  5403 | `		PH7_MemObjRelease(pKey);` |
|    31330 |  5404 | `	}` |
|   190796 |  5405 | `	break;` |
|        - |  5406 | `					   }` |
|        - |  5407 | `/*` |
|        - |  5408 | ` * INCR: P1 * *` |
|        - |  5409 | ` *` |
|        - |  5410 | ` * Force a numeric cast and increment the top of the stack by 1.` |
|        - |  5411 | ` * If the P1 operand is set then perform a duplication of the top of` |
|        - |  5412 | ` * the stack and increment after that.` |
|        - |  5413 | ` */` |
|   167579 |  5414 | `case PH7_OP_INCR:` |
|        - |  5415 | `#ifdef UNTRUST` |
|        - |  5416 | `	if( pTos < pStack ){` |
|        - |  5417 | `		goto Abort;` |
|        - |  5418 | `	}` |
|        - |  5419 | `#endif` |
|   335204 |  5420 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0 ){` |
|   335204 |  5421 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5422 | `			ph7_value *pObj;` |
|   335204 |  5423 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|   335204 |  5424 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5425 | `					/* Perl-style string increment.` |
|        - |  5426 | `					 * Post-increment: pTos may alias pObj's buffer via SXBLOB_RDONLY` |
|        - |  5427 | `					 * (set by PH7_MemObjLoad).  Force ownership so the upcoming` |
|        - |  5428 | `					 * mutation of pObj doesn't bleed into pTos's old-value view. */` |
|       49 |  5429 | `					if( pInstr->iP1 == 0 ){` |
|       45 |  5430 | `						SyBlobNullAppend(&pTos->sBlob);` |
|       22 |  5431 | `					}` |
|       49 |  5432 | `					PH7_MemObjStringIncrement(pObj);` |
|       49 |  5433 | `					if( pInstr->iP1 ){` |
|        - |  5434 | `						/* Pre-increment: deep-copy pObj into pTos. */` |
|        5 |  5435 | `						PH7_MemObjStore(pObj,pTos);` |
|        2 |  5436 | `					}` |
|       25 |  5437 | `				}else{` |
|        - |  5438 | `					/* Numeric coercion. Post-increment must preserve pTos's` |
|        - |  5439 | `					 * original value: pTos may alias pObj's blob via` |
|        - |  5440 | `					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and` |
|        - |  5441 | `					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING` |
|        - |  5442 | `					 * pObj. Force pTos to take ownership of its blob first` |
|        - |  5443 | `					 * so its old-value view survives the coercion. */` |
|   335156 |  5444 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|       13 |  5445 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        6 |  5446 | `					}` |
|        - |  5447 | `					/* Force a numeric cast on the variable */` |
|   335156 |  5448 | `					PH7_MemObjToNumeric(pObj);` |
|   335156 |  5449 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5450 | `						pObj->rVal++;` |
|        - |  5451 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5452 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5453 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5454 | `						 * integer-valued real. */` |
|        9 |  5455 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5456 | `					}else{` |
|   335148 |  5457 | `						pObj->x.iVal++;` |
|        - |  5458 | `					}` |
|   335156 |  5459 | `					if( pInstr->iP1 ){` |
|        - |  5460 | `						/* Pre-increment: result is the new value. */` |
|       77 |  5461 | `						PH7_MemObjStore(pObj,pTos);` |
|       38 |  5462 | `					}` |
|        - |  5463 | `					/* Post-increment: pTos retains the old value (a string` |
|        - |  5464 | `					 * for "5"++, an int/float for direct numeric operands). */` |
|        - |  5465 | `				}` |
|   167623 |  5466 | `			}` |
|   167625 |  5467 | `		}else{` |
|      ! 0 |  5468 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5469 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|      ! 0 |  5470 | `					PH7_MemObjStringIncrement(pTos);` |
|      ! 0 |  5471 | `				}else{` |
|        - |  5472 | `					/* Force a numeric cast */` |
|      ! 0 |  5473 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5474 | `					/* Pre-increment */` |
|      ! 0 |  5475 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5476 | `						pTos->rVal++;` |
|        - |  5477 | `						/* Try to get an integer representation */` |
|      ! 0 |  5478 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5479 | `					}else{` |
|      ! 0 |  5480 | `						pTos->x.iVal++;` |
|      ! 0 |  5481 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5482 | `					}` |
|        - |  5483 | `				}` |
|      ! 0 |  5484 | `			}` |
|        - |  5485 | `		}` |
|   167623 |  5486 | `	}` |
|   335204 |  5487 | `	break;` |
|        - |  5488 | `/*` |
|        - |  5489 | ` * DECR: P1 * *` |
|        - |  5490 | ` *` |
|        - |  5491 | ` * Force a numeric cast and decrement the top of the stack by 1.` |
|        - |  5492 | ` * If the P1 operand is set then perform a duplication of the top of the stack` |
|        - |  5493 | ` * and decrement after that.` |
|        - |  5494 | ` */` |
|       14 |  5495 | `case PH7_OP_DECR:` |
|        - |  5496 | `#ifdef UNTRUST` |
|        - |  5497 | `	if( pTos < pStack ){` |
|        - |  5498 | `		goto Abort;` |
|        - |  5499 | `	}` |
|        - |  5500 | `#endif` |
|        - |  5501 | ``	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op). */`` |
|       29 |  5502 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|       27 |  5503 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|        - |  5504 | `			ph7_value *pObj;` |
|       27 |  5505 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       27 |  5506 | `				if( VmStringWantsPerlIncr(pObj) ){` |
|        - |  5507 | ``					/* PHP has no string decrement: `--` on a non-numeric string`` |
|        - |  5508 | ``					 * is a no-op (unlike `++`, which is Perl-style). Leave pObj`` |
|        - |  5509 | `					 * unchanged; the result is simply that unchanged value. */` |
|        7 |  5510 | `					if( pInstr->iP1 ){` |
|        - |  5511 | `						/* Pre-decrement: result is the (unchanged) value. */` |
|      ! 0 |  5512 | `						PH7_MemObjStore(pObj,pTos);` |
|      ! 0 |  5513 | `					}` |
|        - |  5514 | `					/* Post-decrement: pTos already holds the old value. */` |
|        4 |  5515 | `				}else{` |
|        - |  5516 | `					/* Numeric coercion. Mirror INCR's aliasing care: a` |
|        - |  5517 | `					 * post-decrement must preserve pTos's original value, which` |
|        - |  5518 | `					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).` |
|        - |  5519 | `					 * Force pTos to own its blob before coercing pObj. */` |
|       21 |  5520 | `					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){` |
|        5 |  5521 | `						SyBlobNullAppend(&pTos->sBlob);` |
|        2 |  5522 | `					}` |
|       21 |  5523 | `					PH7_MemObjToNumeric(pObj);` |
|       21 |  5524 | `					if( pObj->iFlags & MEMOBJ_REAL ){` |
|        9 |  5525 | `						pObj->rVal--;` |
|        - |  5526 | `						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it` |
|        - |  5527 | `						 * stays consistent with the new rVal; otherwise (int)$a,` |
|        - |  5528 | `						 * ===, intdiv() etc. read a stale int for an` |
|        - |  5529 | `						 * integer-valued real. */` |
|        9 |  5530 | `						PH7_MemObjTryInteger(pObj);` |
|        5 |  5531 | `					}else{` |
|       13 |  5532 | `						pObj->x.iVal--;` |
|        - |  5533 | `					}` |
|       21 |  5534 | `					if( pInstr->iP1 ){` |
|        - |  5535 | `						/* Pre-decrement: result is the new value. */` |
|        3 |  5536 | `						PH7_MemObjStore(pObj,pTos);` |
|        1 |  5537 | `					}` |
|        - |  5538 | `					/* Post-decrement: pTos retains the old value. */` |
|        - |  5539 | `				}` |
|       13 |  5540 | `			}` |
|       14 |  5541 | `		}else{` |
|      ! 0 |  5542 | `			if( pInstr->iP1 ){` |
|      ! 0 |  5543 | `				if( VmStringWantsPerlIncr(pTos) ){` |
|        - |  5544 | `					/* Non-numeric string, no lvalue: no-op (value unchanged). */` |
|      ! 0 |  5545 | `				}else{` |
|        - |  5546 | `					/* Force a numeric cast */` |
|      ! 0 |  5547 | `					PH7_MemObjToNumeric(pTos);` |
|        - |  5548 | `					/* Pre-decrement */` |
|      ! 0 |  5549 | `					if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5550 | `						pTos->rVal--;` |
|        - |  5551 | `						/* Keep the cached int consistent with the new rVal. */` |
|      ! 0 |  5552 | `						PH7_MemObjTryInteger(pTos);` |
|      ! 0 |  5553 | `					}else{` |
|      ! 0 |  5554 | `						pTos->x.iVal--;` |
|      ! 0 |  5555 | `						MemObjSetType(pTos,MEMOBJ_INT);` |
|        - |  5556 | `					}` |
|        - |  5557 | `				}` |
|      ! 0 |  5558 | `			}` |
|        - |  5559 | `		}` |
|       13 |  5560 | `	}` |
|       29 |  5561 | `	break;` |
|        - |  5562 | `/*` |
|        - |  5563 | ` * UMINUS: * * *` |
|        - |  5564 | ` *` |
|        - |  5565 | ` * Perform a unary minus operation.` |
|        - |  5566 | ` */` |
|    29132 |  5567 | `case PH7_OP_UMINUS:` |
|        - |  5568 | `#ifdef UNTRUST` |
|        - |  5569 | `	if( pTos < pStack ){` |
|        - |  5570 | `		goto Abort;` |
|        - |  5571 | `	}` |
|        - |  5572 | `#endif` |
|        - |  5573 | `	/* Force a numeric (integer,real or both) cast */` |
|    58266 |  5574 | `	PH7_MemObjToNumeric(pTos);` |
|    58266 |  5575 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|       31 |  5576 | `		pTos->rVal = -pTos->rVal;` |
|       15 |  5577 | `	}` |
|    58266 |  5578 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|    58236 |  5579 | `		pTos->x.iVal = -pTos->x.iVal;` |
|    29117 |  5580 | `	}` |
|    58266 |  5581 | `	break;` |
|        - |  5582 | `/*` |
|        - |  5583 | ` * UPLUS: * * *` |
|        - |  5584 | ` *` |
|        - |  5585 | ` * Perform a unary plus operation.` |
|        - |  5586 | ` */` |
|       18 |  5587 | `case PH7_OP_UPLUS:` |
|        - |  5588 | `#ifdef UNTRUST` |
|        - |  5589 | `	if( pTos < pStack ){` |
|        - |  5590 | `		goto Abort;` |
|        - |  5591 | `	}` |
|        - |  5592 | `#endif` |
|        - |  5593 | `	/* Force a numeric (integer,real or both) cast */` |
|       37 |  5594 | `	PH7_MemObjToNumeric(pTos);` |
|       37 |  5595 | `	if( pTos->iFlags & MEMOBJ_REAL ){` |
|      ! 0 |  5596 | `		pTos->rVal = +pTos->rVal;` |
|      ! 0 |  5597 | `	}` |
|       37 |  5598 | `	if( pTos->iFlags & MEMOBJ_INT ){` |
|       37 |  5599 | `		pTos->x.iVal = +pTos->x.iVal;` |
|       18 |  5600 | `	}` |
|       37 |  5601 | `	break;` |
|        - |  5602 | `/*` |
|        - |  5603 | ` * OP_LNOT: * * *` |
|        - |  5604 | ` *` |
|        - |  5605 | ` * Interpret the top of the stack as a boolean value.  Replace it` |
|        - |  5606 | ` * with its complement.` |
|        - |  5607 | ` */` |
|    44614 |  5608 | `case PH7_OP_LNOT:` |
|        - |  5609 | `#ifdef UNTRUST` |
|        - |  5610 | `	if( pTos < pStack ){` |
|        - |  5611 | `		goto Abort;` |
|        - |  5612 | `	}` |
|        - |  5613 | `#endif` |
|        - |  5614 | `	/* Force a boolean cast */` |
|    89274 |  5615 | `	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|       23 |  5616 | `		PH7_MemObjToBool(pTos);` |
|       11 |  5617 | `	}` |
|    89274 |  5618 | `	pTos->x.iVal = !pTos->x.iVal;` |
|    89274 |  5619 | `	break;` |
|        - |  5620 | `/*` |
|        - |  5621 | ` * OP_BITNOT: * * *` |
|        - |  5622 | ` *` |
|        - |  5623 | ` * Interpret the top of the stack as an value.Replace it` |
|        - |  5624 | ` * with its ones-complement.` |
|        - |  5625 | ` */` |
|       15 |  5626 | `case PH7_OP_BITNOT:` |
|        - |  5627 | `#ifdef UNTRUST` |
|        - |  5628 | `	if( pTos < pStack ){` |
|        - |  5629 | `		goto Abort;` |
|        - |  5630 | `	}` |
|        - |  5631 | `#endif` |
|        - |  5632 | `	/* Force an integer cast */` |
|       32 |  5633 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5634 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5635 | `	}` |
|       32 |  5636 | `	pTos->x.iVal = ~pTos->x.iVal;` |
|       32 |  5637 | `	break;` |
|        - |  5638 | `/* OP_MUL * * *` |
|        - |  5639 | ` * OP_MUL_STORE * * *` |
|        - |  5640 | ` *` |
|        - |  5641 | ` * Pop the top two elements from the stack, multiply them together,` |
|        - |  5642 | ` * and push the result back onto the stack.` |
|        - |  5643 | ` */` |
|     1288 |  5644 | `case PH7_OP_MUL:` |
|        - |  5645 | `case PH7_OP_MUL_STORE: {` |
|     2578 |  5646 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5647 | `	/* Force the operand to be numeric */` |
|        - |  5648 | `#ifdef UNTRUST` |
|        - |  5649 | `	if( pNos < pStack ){` |
|        - |  5650 | `		goto Abort;` |
|        - |  5651 | `	}` |
|        - |  5652 | `#endif` |
|     2578 |  5653 | `	PH7_MemObjToNumeric(pTos);` |
|     2578 |  5654 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5655 | `	/* Perform the requested operation */` |
|     2578 |  5656 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5657 | `		/* Floating point arithemic */` |
|        - |  5658 | `		ph7_real a,b,r;` |
|       21 |  5659 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        9 |  5660 | `			PH7_MemObjToReal(pTos);` |
|        4 |  5661 | `		}` |
|       21 |  5662 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        7 |  5663 | `			PH7_MemObjToReal(pNos);` |
|        3 |  5664 | `		}` |
|       21 |  5665 | `		a = pNos->rVal;` |
|       21 |  5666 | `		b = pTos->rVal;` |
|       21 |  5667 | `		r = a * b;` |
|        - |  5668 | `		/* Push the result */` |
|       21 |  5669 | `		pNos->rVal = r;` |
|       21 |  5670 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5671 | `		/* Try to get an integer representation */` |
|       21 |  5672 | `		PH7_MemObjTryInteger(pNos);` |
|       11 |  5673 | `	}else{` |
|        - |  5674 | `		/* Integer arithmetic */` |
|        - |  5675 | `		sxi64 a,b,r;` |
|     2558 |  5676 | `		a = pNos->x.iVal;` |
|     2558 |  5677 | `		b = pTos->x.iVal;` |
|     2558 |  5678 | `		r = a * b;` |
|        - |  5679 | `		/* Push the result */` |
|     2558 |  5680 | `		pNos->x.iVal = r;` |
|     2558 |  5681 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5682 | `	}` |
|     2578 |  5683 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|        - |  5684 | `		ph7_value *pObj;` |
|       32 |  5685 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5686 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       32 |  5687 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       32 |  5688 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       32 |  5689 | `			PH7_MemObjStore(pNos,pObj);` |
|       15 |  5690 | `		}` |
|       15 |  5691 | `	}` |
|     2578 |  5692 | `	VmPopOperand(&pTos,1);` |
|     2578 |  5693 | `	break;` |
|        - |  5694 | `				 }` |
|        - |  5695 | `/* OP_POW * * *` |
|        - |  5696 | ` * OP_POW_STORE * * *` |
|        - |  5697 | ` *` |
|        - |  5698 | ` * Pop the top two elements from the stack, raise the second to the` |
|        - |  5699 | ` * power of the first, and push the result. PHP semantics: int**int` |
|        - |  5700 | ` * stays integer iff the exponent is non-negative and the exact result` |
|        - |  5701 | ` * fits in sxi64; otherwise the result is a double.` |
|        - |  5702 | ` */` |
|       67 |  5703 | `case PH7_OP_POW:` |
|        - |  5704 | `case PH7_OP_POW_STORE: {` |
|      135 |  5705 | `	ph7_value *pNos = &pTos[-1];` |
|      135 |  5706 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|        - |  5707 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|        - |  5708 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|        - |  5709 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|        - |  5710 | `	 */` |
|      135 |  5711 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|      135 |  5712 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|        - |  5713 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|        - |  5714 | `	int bBothInt;` |
|      135 |  5715 | `	int usedInt = 0;` |
|        - |  5716 | `	ph7_real a, b, r;` |
|        - |  5717 | `#endif` |
|      135 |  5718 | `	sxi64 base_i = 0, exp_i = 0;` |
|        - |  5719 | `#ifdef UNTRUST` |
|        - |  5720 | `	if( pNos < pStack ){` |
|        - |  5721 | `		goto Abort;` |
|        - |  5722 | `	}` |
|        - |  5723 | `#endif` |
|      135 |  5724 | `	PH7_MemObjToNumeric(pTos);` |
|      135 |  5725 | `	PH7_MemObjToNumeric(pNos);` |
|        - |  5726 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      265 |  5727 | `	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&` |
|      130 |  5728 | `	           ((pNos->iFlags & MEMOBJ_REAL) == 0);` |
|      135 |  5729 | `	if( bBothInt ){` |
|      123 |  5730 | `		base_i = pBase->x.iVal;` |
|      123 |  5731 | `		exp_i  = pExp->x.iVal;` |
|       61 |  5732 | `	}` |
|      135 |  5733 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|      125 |  5734 | `		PH7_MemObjToReal(pBase);` |
|       62 |  5735 | `	}` |
|      135 |  5736 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|      133 |  5737 | `		PH7_MemObjToReal(pExp);` |
|       66 |  5738 | `	}` |
|      135 |  5739 | `	a = pBase->rVal;` |
|      135 |  5740 | `	b = pExp->rVal;` |
|      135 |  5741 | `	r = pow(a, b);` |
|        - |  5742 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|        - |  5743 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|        - |  5744 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|        - |  5745 | `	 * representable as double but not as signed int64. */` |
|      135 |  5746 | `	if( bBothInt && exp_i >= 0 ){` |
|      117 |  5747 | `		sxi64 result_i = 1;` |
|      117 |  5748 | `		sxi64 cur_base = base_i;` |
|      117 |  5749 | `		sxi64 cur_exp  = exp_i;` |
|      117 |  5750 | `		int overflow = 0;` |
|      401 |  5751 | `		while( cur_exp > 0 ){` |
|      289 |  5752 | `			if( cur_exp & 1 ){` |
|      189 |  5753 | `				if( VmMulOverflow64(result_i, cur_base, &result_i) ){` |
|        3 |  5754 | `					overflow = 1;` |
|        3 |  5755 | `					break;` |
|        - |  5756 | `				}` |
|       93 |  5757 | `			}` |
|      287 |  5758 | `			cur_exp >>= 1;` |
|      287 |  5759 | `			if( cur_exp > 0 ){` |
|      181 |  5760 | `				if( VmMulOverflow64(cur_base, cur_base, &cur_base) ){` |
|        3 |  5761 | `					overflow = 1;` |
|        3 |  5762 | `					break;` |
|        - |  5763 | `				}` |
|       89 |  5764 | `			}` |
|        1 |  5765 | `		}` |
|      117 |  5766 | `		if( !overflow ){` |
|      113 |  5767 | `			pNos->x.iVal = result_i;` |
|      113 |  5768 | `			MemObjSetType(pNos, MEMOBJ_INT);` |
|      113 |  5769 | `			usedInt = 1;` |
|       56 |  5770 | `		}` |
|       58 |  5771 | `	}` |
|      135 |  5772 | `	if( !usedInt ){` |
|       23 |  5773 | `		pNos->rVal = r;` |
|       23 |  5774 | `		MemObjSetType(pNos, MEMOBJ_REAL);` |
|       11 |  5775 | `	}` |
|        - |  5776 | `#else` |
|        - |  5777 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|        - |  5778 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|        - |  5779 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|        - |  5780 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|        - |  5781 | `	 * represented. */` |
|        - |  5782 | `	base_i = pBase->x.iVal;` |
|        - |  5783 | `	exp_i  = pExp->x.iVal;` |
|        - |  5784 | `	{` |
|        - |  5785 | `		sxi64 result_i = 1;` |
|        - |  5786 | `		sxi64 cur_base = base_i;` |
|        - |  5787 | `		sxi64 cur_exp  = exp_i;` |
|        - |  5788 | `		if( cur_exp < 0 ){` |
|        - |  5789 | `			result_i = 0;` |
|        - |  5790 | `		}else{` |
|        - |  5791 | `			while( cur_exp > 0 ){` |
|        - |  5792 | `				if( cur_exp & 1 ){` |
|        - |  5793 | `					result_i *= cur_base;` |
|        - |  5794 | `				}` |
|        - |  5795 | `				cur_exp >>= 1;` |
|        - |  5796 | `				if( cur_exp > 0 ){` |
|        - |  5797 | `					cur_base *= cur_base;` |
|        - |  5798 | `				}` |
|        - |  5799 | `			}` |
|        - |  5800 | `		}` |
|        - |  5801 | `		pNos->x.iVal = result_i;` |
|        - |  5802 | `		MemObjSetType(pNos, MEMOBJ_INT);` |
|        - |  5803 | `	}` |
|        - |  5804 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|      135 |  5805 | `	if( bStore ){` |
|        - |  5806 | `		ph7_value *pObj;` |
|       23 |  5807 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5808 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       23 |  5809 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       23 |  5810 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       23 |  5811 | `			PH7_MemObjStore(pNos,pObj);` |
|       11 |  5812 | `		}` |
|       11 |  5813 | `	}` |
|      135 |  5814 | `	VmPopOperand(&pTos,1);` |
|      135 |  5815 | `	break;` |
|        - |  5816 | `				 }` |
|        - |  5817 | `/* OP_ADD * * *` |
|        - |  5818 | ` *` |
|        - |  5819 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5820 | ` * and push the result back onto the stack.` |
|        - |  5821 | ` */` |
|      517 |  5822 | `case PH7_OP_ADD:{` |
|     1036 |  5823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5824 | `#ifdef UNTRUST` |
|        - |  5825 | `	if( pNos < pStack ){` |
|        - |  5826 | `		goto Abort;` |
|        - |  5827 | `	}` |
|        - |  5828 | `#endif` |
|        - |  5829 | `	/* Perform the addition */` |
|     1036 |  5830 | `	PH7_MemObjAdd(pNos,pTos,FALSE);` |
|     1036 |  5831 | `	VmPopOperand(&pTos,1);` |
|     1036 |  5832 | `	break;` |
|        - |  5833 | `				}` |
|        - |  5834 | `/*` |
|        - |  5835 | ` * OP_ADD_STORE * * *` |
|        - |  5836 | ` *` |
|        - |  5837 | ` * Pop the top two elements from the stack, add them together,` |
|        - |  5838 | ` * and push the result back onto the stack.` |
|        - |  5839 | ` */` |
|      502 |  5840 | `case PH7_OP_ADD_STORE:{` |
|     1006 |  5841 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5842 | `	ph7_value *pObj;` |
|        - |  5843 | `	sxu32 nIdx;` |
|        - |  5844 | `#ifdef UNTRUST` |
|        - |  5845 | `	if( pNos < pStack ){` |
|        - |  5846 | `		goto Abort;` |
|        - |  5847 | `	}` |
|        - |  5848 | `#endif` |
|        - |  5849 | `	/* Perform the addition */` |
|     1006 |  5850 | `	nIdx = pTos->nIdx;` |
|     1006 |  5851 | `	PH7_MemObjAdd(pTos,pNos,TRUE);` |
|        - |  5852 | `	/* Peform the store operation */` |
|     1006 |  5853 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  5854 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     1006 |  5855 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     1006 |  5856 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     1006 |  5857 | `		PH7_MemObjStore(pTos,pObj);` |
|      502 |  5858 | `	}` |
|        - |  5859 | `	/* Ticket 1433-35: Perform a stack dup */` |
|     1006 |  5860 | `	PH7_MemObjStore(pTos,pNos);` |
|     1006 |  5861 | `	VmPopOperand(&pTos,1);` |
|     1006 |  5862 | `	break;` |
|        - |  5863 | `				}` |
|        - |  5864 | `/* OP_SUB * * *` |
|        - |  5865 | ` *` |
|        - |  5866 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5867 | ` * first (what was next on the stack) from the second (the` |
|        - |  5868 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5869 | ` */` |
|      349 |  5870 | `case PH7_OP_SUB: {` |
|      700 |  5871 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5872 | `#ifdef UNTRUST` |
|        - |  5873 | `	if( pNos < pStack ){` |
|        - |  5874 | `		goto Abort;` |
|        - |  5875 | `	}` |
|        - |  5876 | `#endif` |
|      700 |  5877 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5878 | `		/* Floating point arithemic */` |
|        - |  5879 | `		ph7_real a,b,r;` |
|       97 |  5880 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5881 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5882 | `		}` |
|       97 |  5883 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  5884 | `			PH7_MemObjToReal(pNos);` |
|        2 |  5885 | `		}` |
|       97 |  5886 | `		a = pNos->rVal;` |
|       97 |  5887 | `		b = pTos->rVal;` |
|       97 |  5888 | `		r = a - b;` |
|        - |  5889 | `		/* Push the result */` |
|       97 |  5890 | `		pNos->rVal = r;` |
|       97 |  5891 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5892 | `		/* Try to get an integer representation */` |
|       97 |  5893 | `		PH7_MemObjTryInteger(pNos);` |
|       49 |  5894 | `	}else{` |
|        - |  5895 | `		/* Integer arithmetic */` |
|        - |  5896 | `		sxi64 a,b,r;` |
|      604 |  5897 | `		a = pNos->x.iVal;` |
|      604 |  5898 | `		b = pTos->x.iVal;` |
|      604 |  5899 | `		r = a - b;` |
|        - |  5900 | `		/* Push the result */` |
|      604 |  5901 | `		pNos->x.iVal = r;` |
|      604 |  5902 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5903 | `	}` |
|      700 |  5904 | `	VmPopOperand(&pTos,1);` |
|      700 |  5905 | `	break;` |
|        - |  5906 | `				 }` |
|        - |  5907 | `/* OP_SUB_STORE * * *` |
|        - |  5908 | ` *` |
|        - |  5909 | ` * Pop the top two elements from the stack, subtract the` |
|        - |  5910 | ` * first (what was next on the stack) from the second (the` |
|        - |  5911 | ` * top of the stack) and push the result back onto the stack.` |
|        - |  5912 | ` */` |
|        4 |  5913 | `case PH7_OP_SUB_STORE: {` |
|       10 |  5914 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5915 | `	ph7_value *pObj;` |
|        - |  5916 | `#ifdef UNTRUST` |
|        - |  5917 | `	if( pNos < pStack ){` |
|        - |  5918 | `		goto Abort;` |
|        - |  5919 | `	}` |
|        - |  5920 | `#endif` |
|       10 |  5921 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|        - |  5922 | `		/* Floating point arithemic */` |
|        - |  5923 | `		ph7_real a,b,r;` |
|      ! 0 |  5924 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5925 | `			PH7_MemObjToReal(pTos);` |
|      ! 0 |  5926 | `		}` |
|      ! 0 |  5927 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      ! 0 |  5928 | `			PH7_MemObjToReal(pNos);` |
|      ! 0 |  5929 | `		}` |
|      ! 0 |  5930 | `		a = pTos->rVal;` |
|      ! 0 |  5931 | `		b = pNos->rVal;` |
|      ! 0 |  5932 | `		r = a - b;` |
|        - |  5933 | `		/* Push the result */` |
|      ! 0 |  5934 | `		pNos->rVal = r;` |
|      ! 0 |  5935 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  5936 | `		/* Try to get an integer representation */` |
|      ! 0 |  5937 | `		PH7_MemObjTryInteger(pNos);` |
|      ! 0 |  5938 | `	}else{` |
|        - |  5939 | `		/* Integer arithmetic */` |
|        - |  5940 | `		sxi64 a,b,r;` |
|       10 |  5941 | `		a = pTos->x.iVal;` |
|       10 |  5942 | `		b = pNos->x.iVal;` |
|       10 |  5943 | `		r = a - b;` |
|        - |  5944 | `		/* Push the result */` |
|       10 |  5945 | `		pNos->x.iVal = r;` |
|       10 |  5946 | `		MemObjSetType(pNos,MEMOBJ_INT);` |
|        - |  5947 | `	}` |
|       10 |  5948 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  5949 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       10 |  5950 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       10 |  5951 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       10 |  5952 | `		PH7_MemObjStore(pNos,pObj);` |
|        4 |  5953 | `	}` |
|       10 |  5954 | `	VmPopOperand(&pTos,1);` |
|       10 |  5955 | `	break;` |
|        - |  5956 | `				 }` |
|        - |  5957 |  |
|        - |  5958 | `/*` |
|        - |  5959 | ` * OP_MOD * * *` |
|        - |  5960 | ` *` |
|        - |  5961 | ` * Pop the top two elements from the stack, divide the` |
|        - |  5962 | ` * first (what was next on the stack) from the second (the` |
|        - |  5963 | ` * top of the stack) and push the remainder after division` |
|        - |  5964 | ` * onto the stack.` |
|        - |  5965 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  5966 | ` */` |
|      308 |  5967 | `case PH7_OP_MOD:{` |
|      618 |  5968 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  5969 | `	sxi64 a,b,r;` |
|        - |  5970 | `#ifdef UNTRUST` |
|        - |  5971 | `	if( pNos < pStack ){` |
|        - |  5972 | `		goto Abort;` |
|        - |  5973 | `	}` |
|        - |  5974 | `#endif` |
|        - |  5975 | `	/* Force the operands to be integer */` |
|      618 |  5976 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  5977 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  5978 | `	}` |
|      618 |  5979 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|        5 |  5980 | `		PH7_MemObjToInteger(pNos);` |
|        2 |  5981 | `	}` |
|        - |  5982 | `	/* Perform the requested operation */` |
|      618 |  5983 | `	a = pNos->x.iVal;` |
|      618 |  5984 | `	b = pTos->x.iVal;` |
|      618 |  5985 | `	if( b == 0 ){` |
|        3 |  5986 | `		r = 0;` |
|        3 |  5987 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  5988 | `		/* goto Abort; */` |
|        2 |  5989 | `	}else{` |
|      615 |  5990 | `		r = a%b;` |
|        - |  5991 | `	}` |
|        - |  5992 | `	/* Push the result */` |
|      618 |  5993 | `	pNos->x.iVal = r;` |
|      618 |  5994 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      618 |  5995 | `	VmPopOperand(&pTos,1);` |
|      618 |  5996 | `	break;` |
|        - |  5997 | `				}` |
|        - |  5998 | `/*` |
|        - |  5999 | ` * OP_MOD_STORE * * *` |
|        - |  6000 | ` *` |
|        - |  6001 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6002 | ` * first (what was next on the stack) from the second (the` |
|        - |  6003 | ` * top of the stack) and push the remainder after division` |
|        - |  6004 | ` * onto the stack.` |
|        - |  6005 | ` * Note: Only integer arithemtic is allowed.` |
|        - |  6006 | ` */` |
|        1 |  6007 | `case PH7_OP_MOD_STORE: {` |
|        3 |  6008 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6009 | `	ph7_value *pObj;` |
|        - |  6010 | `	sxi64 a,b,r;` |
|        - |  6011 | `#ifdef UNTRUST` |
|        - |  6012 | `	if( pNos < pStack ){` |
|        - |  6013 | `		goto Abort;` |
|        - |  6014 | `	}` |
|        - |  6015 | `#endif` |
|        - |  6016 | `	/* Force the operands to be integer */` |
|        3 |  6017 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6018 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6019 | `	}` |
|        3 |  6020 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6021 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6022 | `	}` |
|        - |  6023 | `	/* Perform the requested operation */` |
|        3 |  6024 | `	a = pTos->x.iVal;` |
|        3 |  6025 | `	b = pNos->x.iVal;` |
|        3 |  6026 | `	if( b == 0 ){` |
|      ! 0 |  6027 | `		r = 0;` |
|      ! 0 |  6028 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd%%0",a);` |
|        - |  6029 | `		/* goto Abort; */` |
|      ! 0 |  6030 | `	}else{` |
|        3 |  6031 | `		r = a%b;` |
|        - |  6032 | `	}` |
|        - |  6033 | `	/* Push the result */` |
|        3 |  6034 | `	pNos->x.iVal = r;` |
|        3 |  6035 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|        3 |  6036 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6037 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        3 |  6038 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        3 |  6039 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        3 |  6040 | `		PH7_MemObjStore(pNos,pObj);` |
|        1 |  6041 | `	}` |
|        3 |  6042 | `	VmPopOperand(&pTos,1);` |
|        3 |  6043 | `	break;` |
|        - |  6044 | `				}` |
|        - |  6045 | `/*` |
|        - |  6046 | ` * OP_DIV * * *` |
|        - |  6047 | ` *` |
|        - |  6048 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6049 | ` * first (what was next on the stack) from the second (the` |
|        - |  6050 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6051 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6052 | ` */` |
|       33 |  6053 | `case PH7_OP_DIV:{` |
|       68 |  6054 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6055 | `	ph7_real a,b,r;` |
|        - |  6056 | `#ifdef UNTRUST` |
|        - |  6057 | `	if( pNos < pStack ){` |
|        - |  6058 | `		goto Abort;` |
|        - |  6059 | `	}` |
|        - |  6060 | `#endif` |
|        - |  6061 | `	/* Force the operands to be real */` |
|       68 |  6062 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       62 |  6063 | `		PH7_MemObjToReal(pTos);` |
|       30 |  6064 | `	}` |
|       68 |  6065 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       28 |  6066 | `		PH7_MemObjToReal(pNos);` |
|       13 |  6067 | `	}` |
|        - |  6068 | `	/* Perform the requested operation */` |
|       68 |  6069 | `	a = pNos->rVal;` |
|       68 |  6070 | `	b = pTos->rVal;` |
|       68 |  6071 | `	if( b == 0 ){` |
|        - |  6072 | `		/* Division by zero */` |
|        3 |  6073 | `		pNos->rVal = 0;` |
|        3 |  6074 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Division by zero");` |
|        - |  6075 | `		/* goto Abort; */` |
|        2 |  6076 | `	}else{` |
|       65 |  6077 | `		r = a/b;` |
|        - |  6078 | `		/* Push the result */` |
|       65 |  6079 | `		pNos->rVal = r;` |
|       65 |  6080 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6081 | `		/* Try to get an integer representation */` |
|       65 |  6082 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6083 | `	}` |
|       68 |  6084 | `	VmPopOperand(&pTos,1);` |
|       68 |  6085 | `	break;` |
|        - |  6086 | `				}` |
|        - |  6087 | `/*` |
|        - |  6088 | ` * OP_DIV_STORE * * *` |
|        - |  6089 | ` *` |
|        - |  6090 | ` * Pop the top two elements from the stack, divide the` |
|        - |  6091 | ` * first (what was next on the stack) from the second (the` |
|        - |  6092 | ` * top of the stack) and push the result onto the stack.` |
|        - |  6093 | ` * Note: Only floating point arithemtic is allowed.` |
|        - |  6094 | ` */` |
|        2 |  6095 | `case PH7_OP_DIV_STORE:{` |
|        5 |  6096 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6097 | `	ph7_value *pObj;` |
|        - |  6098 | `	ph7_real a,b,r;` |
|        - |  6099 | `#ifdef UNTRUST` |
|        - |  6100 | `	if( pNos < pStack ){` |
|        - |  6101 | `		goto Abort;` |
|        - |  6102 | `	}` |
|        - |  6103 | `#endif` |
|        - |  6104 | `	/* Force the operands to be real */` |
|        5 |  6105 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6106 | `		PH7_MemObjToReal(pTos);` |
|        2 |  6107 | `	}` |
|        5 |  6108 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|        5 |  6109 | `		PH7_MemObjToReal(pNos);` |
|        2 |  6110 | `	}` |
|        - |  6111 | `	/* Perform the requested operation */` |
|        5 |  6112 | `	a = pTos->rVal;` |
|        5 |  6113 | `	b = pNos->rVal;` |
|        5 |  6114 | `	if( b == 0 ){` |
|        - |  6115 | `		/* Division by zero */` |
|      ! 0 |  6116 | `		r = 0;` |
|      ! 0 |  6117 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Division by zero %qd/0",a);` |
|        - |  6118 | `		/* goto Abort; */` |
|      ! 0 |  6119 | `	}else{` |
|        5 |  6120 | `		r = a/b;` |
|        - |  6121 | `		/* Push the result */` |
|        5 |  6122 | `		pNos->rVal = r;` |
|        5 |  6123 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|        - |  6124 | `		/* Try to get an integer representation */` |
|        5 |  6125 | `		PH7_MemObjTryInteger(pNos);` |
|        - |  6126 | `	}` |
|        5 |  6127 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6128 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|        5 |  6129 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|        5 |  6130 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|        5 |  6131 | `		PH7_MemObjStore(pNos,pObj);` |
|        2 |  6132 | `	}` |
|        5 |  6133 | `	VmPopOperand(&pTos,1);` |
|        5 |  6134 | `	break;` |
|        - |  6135 | `				}` |
|        - |  6136 | `/* OP_BAND * * *` |
|        - |  6137 | ` *` |
|        - |  6138 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6139 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6140 | ` * two elements.` |
|        - |  6141 | `*/` |
|        - |  6142 | `/* OP_BOR * * *` |
|        - |  6143 | ` *` |
|        - |  6144 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6145 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6146 | ` * two elements.` |
|        - |  6147 | ` */` |
|        - |  6148 | `/* OP_BXOR * * *` |
|        - |  6149 | ` *` |
|        - |  6150 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6151 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6152 | ` * two elements.` |
|        - |  6153 | ` */` |
|       44 |  6154 | `case PH7_OP_BAND:` |
|        - |  6155 | `case PH7_OP_BOR:` |
|        - |  6156 | `case PH7_OP_BXOR:{` |
|       90 |  6157 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6158 | `	sxi64 a,b,r;` |
|        - |  6159 | `#ifdef UNTRUST` |
|        - |  6160 | `	if( pNos < pStack ){` |
|        - |  6161 | `		goto Abort;` |
|        - |  6162 | `	}` |
|        - |  6163 | `#endif` |
|        - |  6164 | `	/* Force the operands to be integer */` |
|       90 |  6165 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6166 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6167 | `	}` |
|       90 |  6168 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6169 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6170 | `	}` |
|        - |  6171 | `	/* Perform the requested operation */` |
|       90 |  6172 | `	a = pNos->x.iVal;` |
|       90 |  6173 | `	b = pTos->x.iVal;` |
|       90 |  6174 | `	switch(pInstr->iOp){` |
|        7 |  6175 | `	case PH7_OP_BOR_STORE:` |
|       15 |  6176 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        7 |  6177 | `	case PH7_OP_BXOR_STORE:` |
|       15 |  6178 | `	case PH7_OP_BXOR: r = a^b; break;` |
|       30 |  6179 | `	case PH7_OP_BAND_STORE:` |
|       30 |  6180 | `	case PH7_OP_BAND:` |
|       62 |  6181 | `	default:          r = a&b; break;` |
|        - |  6182 | `	}` |
|        - |  6183 | `	/* Push the result */` |
|       90 |  6184 | `	pNos->x.iVal = r;` |
|       90 |  6185 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       90 |  6186 | `	VmPopOperand(&pTos,1);` |
|       90 |  6187 | `	break;` |
|        - |  6188 | `				 }` |
|        - |  6189 | `/* OP_BAND_STORE * * *` |
|        - |  6190 | ` *` |
|        - |  6191 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6192 | ` * to integers.  Push back onto the stack the bit-wise AND of the` |
|        - |  6193 | ` * two elements.` |
|        - |  6194 | `*/` |
|        - |  6195 | `/* OP_BOR_STORE * * *` |
|        - |  6196 | ` *` |
|        - |  6197 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6198 | ` * to integers.  Push back onto the stack the bit-wise OR of the` |
|        - |  6199 | ` * two elements.` |
|        - |  6200 | ` */` |
|        - |  6201 | `/* OP_BXOR_STORE * * *` |
|        - |  6202 | ` *` |
|        - |  6203 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6204 | ` * to integers.  Push back onto the stack the bit-wise XOR of the` |
|        - |  6205 | ` * two elements.` |
|        - |  6206 | ` */` |
|       10 |  6207 | `case PH7_OP_BAND_STORE:` |
|        - |  6208 | `case PH7_OP_BOR_STORE:` |
|        - |  6209 | `case PH7_OP_BXOR_STORE:{` |
|       21 |  6210 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6211 | `	ph7_value *pObj;` |
|        - |  6212 | `	sxi64 a,b,r;` |
|        - |  6213 | `#ifdef UNTRUST` |
|        - |  6214 | `	if( pNos < pStack ){` |
|        - |  6215 | `		goto Abort;` |
|        - |  6216 | `	}` |
|        - |  6217 | `#endif` |
|        - |  6218 | `	/* Force the operands to be integer */` |
|       21 |  6219 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6220 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6221 | `	}` |
|       21 |  6222 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6223 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6224 | `	}` |
|        - |  6225 | `	/* Perform the requested operation */` |
|       21 |  6226 | `	a = pTos->x.iVal;` |
|       21 |  6227 | `	b = pNos->x.iVal;` |
|       21 |  6228 | `	switch(pInstr->iOp){` |
|        3 |  6229 | `	case PH7_OP_BOR_STORE:` |
|        7 |  6230 | `	case PH7_OP_BOR:  r = a\|b; break;` |
|        4 |  6231 | `	case PH7_OP_BXOR_STORE:` |
|        9 |  6232 | `	case PH7_OP_BXOR: r = a^b; break;` |
|        3 |  6233 | `	case PH7_OP_BAND_STORE:` |
|        3 |  6234 | `	case PH7_OP_BAND:` |
|        7 |  6235 | `	default:          r = a&b; break;` |
|        - |  6236 | `	}` |
|        - |  6237 | `	/* Push the result */` |
|       21 |  6238 | `	pNos->x.iVal = r;` |
|       21 |  6239 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       21 |  6240 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6241 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       21 |  6242 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       21 |  6243 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       21 |  6244 | `		PH7_MemObjStore(pNos,pObj);` |
|       10 |  6245 | `	}` |
|       21 |  6246 | `	VmPopOperand(&pTos,1);` |
|       21 |  6247 | `	break;` |
|        - |  6248 | `				 }` |
|        - |  6249 | `/* OP_SHL * * *` |
|        - |  6250 | ` *` |
|        - |  6251 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6252 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6253 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6254 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6255 | ` */` |
|        - |  6256 | `/* OP_SHR * * *` |
|        - |  6257 | ` *` |
|        - |  6258 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6259 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6260 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6261 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6262 | ` */` |
|       12 |  6263 | `case PH7_OP_SHL:` |
|        - |  6264 | `case PH7_OP_SHR: {` |
|       25 |  6265 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6266 | `	sxi64 a,r;` |
|        - |  6267 | `	sxi32 b;` |
|        - |  6268 | `#ifdef UNTRUST` |
|        - |  6269 | `	if( pNos < pStack ){` |
|        - |  6270 | `		goto Abort;` |
|        - |  6271 | `	}` |
|        - |  6272 | `#endif` |
|        - |  6273 | `	/* Force the operands to be integer */` |
|       25 |  6274 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6275 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6276 | `	}` |
|       25 |  6277 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6278 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6279 | `	}` |
|        - |  6280 | `	/* Perform the requested operation */` |
|       25 |  6281 | `	a = pNos->x.iVal;` |
|       25 |  6282 | `	b = (sxi32)pTos->x.iVal;` |
|       25 |  6283 | `	if( pInstr->iOp == PH7_OP_SHL ){` |
|       15 |  6284 | `		r = a << b;` |
|        8 |  6285 | `	}else{` |
|       11 |  6286 | `		r = a >> b;` |
|        - |  6287 | `	}` |
|        - |  6288 | `	/* Push the result */` |
|       25 |  6289 | `	pNos->x.iVal = r;` |
|       25 |  6290 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       25 |  6291 | `	VmPopOperand(&pTos,1);` |
|       25 |  6292 | `	break;` |
|        - |  6293 | `				 }` |
|        - |  6294 | `/*  OP_SHL_STORE * * *` |
|        - |  6295 | ` *` |
|        - |  6296 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6297 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6298 | ` * left by N bits where N is the top element on the stack.` |
|        - |  6299 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6300 | ` */` |
|        - |  6301 | `/* OP_SHR_STORE * * *` |
|        - |  6302 | ` *` |
|        - |  6303 | ` * Pop the top two elements from the stack.  Convert both elements` |
|        - |  6304 | ` * to integers.  Push back onto the stack the second element shifted` |
|        - |  6305 | ` * right by N bits where N is the top element on the stack.` |
|        - |  6306 | ` * Note: Only integer arithmetic is allowed.` |
|        - |  6307 | ` */` |
|        9 |  6308 | `case PH7_OP_SHL_STORE:` |
|        - |  6309 | `case PH7_OP_SHR_STORE: {` |
|       19 |  6310 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6311 | `	ph7_value *pObj;` |
|        - |  6312 | `	sxi64 a,r;` |
|        - |  6313 | `	sxi32 b;` |
|        - |  6314 | `#ifdef UNTRUST` |
|        - |  6315 | `	if( pNos < pStack ){` |
|        - |  6316 | `		goto Abort;` |
|        - |  6317 | `	}` |
|        - |  6318 | `#endif` |
|        - |  6319 | `	/* Force the operands to be integer */` |
|       19 |  6320 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6321 | `		PH7_MemObjToInteger(pTos);` |
|      ! 0 |  6322 | `	}` |
|       19 |  6323 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      ! 0 |  6324 | `		PH7_MemObjToInteger(pNos);` |
|      ! 0 |  6325 | `	}` |
|        - |  6326 | `	/* Perform the requested operation */` |
|       19 |  6327 | `	a = pTos->x.iVal;` |
|       19 |  6328 | `	b = (sxi32)pNos->x.iVal;` |
|       19 |  6329 | `	if( pInstr->iOp == PH7_OP_SHL_STORE ){` |
|        9 |  6330 | `		r = a << b;` |
|        5 |  6331 | `	}else{` |
|       11 |  6332 | `		r = a >> b;` |
|        - |  6333 | `	}` |
|        - |  6334 | `	/* Push the result */` |
|       19 |  6335 | `	pNos->x.iVal = r;` |
|       19 |  6336 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|       19 |  6337 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6338 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|       19 |  6339 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|       19 |  6340 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|       19 |  6341 | `		PH7_MemObjStore(pNos,pObj);` |
|        9 |  6342 | `	}` |
|       19 |  6343 | `	VmPopOperand(&pTos,1);` |
|       19 |  6344 | `	break;` |
|        - |  6345 | `				 }` |
|        - |  6346 | `/* CAT:  P1 * *` |
|        - |  6347 | ` *` |
|        - |  6348 | ` * Pop P1 elements from the stack. Concatenate them togeher and push the result` |
|        - |  6349 | ` * back.` |
|        - |  6350 | ` */` |
|    71047 |  6351 | `case PH7_OP_CAT:{` |
|        - |  6352 | `	ph7_value *pNos,*pCur;` |
|   142096 |  6353 | `	if( pInstr->iP1 < 1 ){` |
|   114618 |  6354 | `		pNos = &pTos[-1];` |
|    57310 |  6355 | `	}else{` |
|    27480 |  6356 | `		pNos = &pTos[-pInstr->iP1+1];` |
|        - |  6357 | `	}` |
|        - |  6358 | `#ifdef UNTRUST` |
|        - |  6359 | `	if( pNos < pStack ){` |
|        - |  6360 | `		goto Abort;` |
|        - |  6361 | `	}` |
|        - |  6362 | `#endif` |
|        - |  6363 | `	/* Force a string cast */` |
|   142096 |  6364 | `	if( (pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|     1640 |  6365 | `		PH7_MemObjToString(pNos);` |
|      819 |  6366 | `	}` |
|   142096 |  6367 | `	pCur = &pNos[1];` |
|   286914 |  6368 | `	while( pCur <= pTos ){` |
|   144820 |  6369 | `		if( (pCur->iFlags & MEMOBJ_STRING) == 0 ){` |
|    50914 |  6370 | `			PH7_MemObjToString(pCur);` |
|    25456 |  6371 | `		}` |
|        - |  6372 | `		/* Perform the concatenation */` |
|   144820 |  6373 | `		if( SyBlobLength(&pCur->sBlob) > 0 ){` |
|   144778 |  6374 | `			PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob));` |
|    72388 |  6375 | `		}` |
|   144820 |  6376 | `		SyBlobRelease(&pCur->sBlob);` |
|   144820 |  6377 | `		pCur++;` |
|        2 |  6378 | `	}` |
|   142096 |  6379 | `	pTos = pNos;` |
|   142096 |  6380 | `	break;` |
|        - |  6381 | `				}` |
|        - |  6382 | `/*  CAT_STORE: * * *` |
|        - |  6383 | ` *` |
|        - |  6384 | ` * Pop two elements from the stack. Concatenate them togeher and push the result` |
|        - |  6385 | ` * back.` |
|        - |  6386 | ` */` |
|     4093 |  6387 | `case PH7_OP_CAT_STORE:{` |
|     8188 |  6388 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6389 | `	ph7_value *pObj;` |
|        - |  6390 | `#ifdef UNTRUST` |
|        - |  6391 | `	if( pNos < pStack ){` |
|        - |  6392 | `		goto Abort;` |
|        - |  6393 | `	}` |
|        - |  6394 | `#endif` |
|     8188 |  6395 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6396 | `		/* Force a string cast */` |
|        3 |  6397 | `		PH7_MemObjToString(pTos);` |
|        1 |  6398 | `	}` |
|     8188 |  6399 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  6400 | `		/* Force a string cast */` |
|      ! 0 |  6401 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6402 | `	}` |
|        - |  6403 | `	/* Perform the concatenation (Reverse order) */` |
|     8188 |  6404 | `	if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|     8188 |  6405 | `		PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|     4093 |  6406 | `	}` |
|        - |  6407 | `	/* Perform the store operation */` |
|     8188 |  6408 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|      ! 0 |  6409 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     8188 |  6410 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     8188 |  6411 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);` |
|     8186 |  6412 | `		PH7_MemObjStore(pTos,pObj);` |
|     4092 |  6413 | `	}` |
|     8186 |  6414 | `	PH7_MemObjStore(pTos,pNos);` |
|     8186 |  6415 | `	VmPopOperand(&pTos,1);` |
|     8186 |  6416 | `	break;` |
|        - |  6417 | `				}` |
|        - |  6418 | `/* OP_AND: * * *` |
|        - |  6419 | ` *` |
|        - |  6420 | ` * Pop two values off the stack.  Take the logical AND of the` |
|        - |  6421 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6422 | ` * stack.` |
|        - |  6423 | ` */` |
|        - |  6424 | `/* OP_OR: * * *` |
|        - |  6425 | ` *` |
|        - |  6426 | ` * Pop two values off the stack.  Take the logical OR of the` |
|        - |  6427 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6428 | ` * stack.` |
|        - |  6429 | ` */` |
|   107767 |  6430 | `case PH7_OP_LAND:` |
|        - |  6431 | `case PH7_OP_LOR: {` |
|   215580 |  6432 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6433 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|        - |  6434 | `#ifdef UNTRUST` |
|        - |  6435 | `	if( pNos < pStack ){` |
|        - |  6436 | `		goto Abort;` |
|        - |  6437 | `	}` |
|        - |  6438 | `#endif` |
|        - |  6439 | `	/* Force a boolean cast */` |
|   215580 |  6440 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|        3 |  6441 | `		PH7_MemObjToBool(pTos);` |
|        1 |  6442 | `	}` |
|   215580 |  6443 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6444 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6445 | `	}` |
|   215580 |  6446 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|   215580 |  6447 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|   215580 |  6448 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|        - |  6449 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|    98582 |  6450 | `		v1 = and_logic[v1*3+v2];` |
|    49314 |  6451 | `	}else{` |
|        - |  6452 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|   117000 |  6453 | `		v1 = or_logic[v1*3+v2];` |
|        - |  6454 | `	}` |
|   215580 |  6455 | `	if( v1 == 2 ){` |
|      ! 0 |  6456 | `		v1 = 1;` |
|      ! 0 |  6457 | `	}` |
|   215580 |  6458 | `	VmPopOperand(&pTos,1);` |
|   215580 |  6459 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|   215580 |  6460 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   215580 |  6461 | `	break;` |
|        - |  6462 | `				 }` |
|        - |  6463 | `/*` |
|        - |  6464 | ` * OP_NULLC: * * *` |
|        - |  6465 | ` * Null coalescing operator '??'.` |
|        - |  6466 | ` * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.` |
|        - |  6467 | ` * Otherwise push right. This is equivalent to: isset($a) ? $a : $b` |
|        - |  6468 | ` */` |
|        - |  6469 | `/*` |
|        - |  6470 | ` * OP_NULLC: * P2 *` |
|        - |  6471 | ` * Short-circuit null coalescing '??'.` |
|        - |  6472 | ` * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).` |
|        - |  6473 | ` * If TOS IS null, pop it and fall through to evaluate the RHS.` |
|        - |  6474 | ` */` |
|       93 |  6475 | `case PH7_OP_NULLC: {` |
|        - |  6476 | `#ifdef UNTRUST` |
|        - |  6477 | `	if( pTos < pStack ){` |
|        - |  6478 | `		goto Abort;` |
|        - |  6479 | `	}` |
|        - |  6480 | `#endif` |
|      188 |  6481 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - |  6482 | `		/* Left is not null — keep it and skip the RHS */` |
|      114 |  6483 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       58 |  6484 | `	}else{` |
|        - |  6485 | `		/* Left is null — discard it, fall through to evaluate RHS */` |
|       76 |  6486 | `		VmPopOperand(&pTos, 1);` |
|        - |  6487 | `	}` |
|      188 |  6488 | `	break;` |
|        - |  6489 |  |
|        - |  6490 | `/*` |
|        - |  6491 | ` * OP_NULLC_JMP: * P2 *` |
|        - |  6492 | ` * Null coalescing assignment short-circuit.` |
|        - |  6493 | ` * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).` |
|        - |  6494 | ` * If TOS IS null, fall through with TOS retained — it carries the LHS's` |
|        - |  6495 | ` * nIdx so the upcoming NULLC_STORE can write back into the variable slot.` |
|        - |  6496 | ` */` |
|       28 |  6497 | `case PH7_OP_NULLC_JMP: {` |
|        - |  6498 | `#ifdef UNTRUST` |
|        - |  6499 | `	if( pTos < pStack ){` |
|        - |  6500 | `		goto Abort;` |
|        - |  6501 | `	}` |
|        - |  6502 | `#endif` |
|       58 |  6503 | `	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|       22 |  6504 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       10 |  6505 | `	}` |
|       58 |  6506 | `	break;` |
|        - |  6507 |  |
|        - |  6508 | `/*` |
|        - |  6509 | ` * OP_NULLC_STORE: * * *` |
|        - |  6510 | ` * Null coalescing assignment store.` |
|        - |  6511 | ` * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],` |
|        - |  6512 | ` * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the` |
|        - |  6513 | ` * expression result.` |
|        - |  6514 | ` */` |
|        - |  6515 | `/*` |
|        - |  6516 | ` * OP_NULLSAFE_JMP: * P2 *` |
|        - |  6517 | `` * Nullsafe object operator short-circuit (PHP 8.0 `?->`).`` |
|        - |  6518 | ` * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL` |
|        - |  6519 | ` * on the stack as the result of the entire containing postfix chain. If` |
|        - |  6520 | ` * non-null, fall through without modifying the stack so the following` |
|        - |  6521 | ` * PH7_OP_MEMBER can consume the object as usual.` |
|        - |  6522 | ` */` |
|       51 |  6523 | `case PH7_OP_NULLSAFE_JMP: {` |
|        - |  6524 | `#ifdef UNTRUST` |
|        - |  6525 | `	if( pTos < pStack ){` |
|        - |  6526 | `		goto Abort;` |
|        - |  6527 | `	}` |
|        - |  6528 | `#endif` |
|      104 |  6529 | `	if( (pTos->iFlags & MEMOBJ_NULL) \|\| pTos->iFlags == 0 ){` |
|        - |  6530 | `		/* Object operand is NULL (or uninitialized) — short-circuit. The` |
|        - |  6531 | `		 * NULL slot already on TOS becomes the chain's final value. */` |
|       41 |  6532 | `		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */` |
|       20 |  6533 | `	}` |
|      104 |  6534 | `	break;` |
|        - |  6535 |  |
|       17 |  6536 | `case PH7_OP_NULLC_STORE: {` |
|       36 |  6537 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6538 | `	ph7_value *pObj;` |
|        - |  6539 | `	sxu32 nIdx;` |
|        - |  6540 | `#ifdef UNTRUST` |
|        - |  6541 | `	if( pNos < pStack ){` |
|        - |  6542 | `		goto Abort;` |
|        - |  6543 | `	}` |
|        - |  6544 | `#endif` |
|        - |  6545 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|        - |  6546 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|        - |  6547 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|       36 |  6548 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|        5 |  6549 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|        5 |  6550 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|        - |  6551 | `			"offsetSet",sizeof("offsetSet")-1);` |
|        - |  6552 | `		ph7_value *apArg[2];` |
|        5 |  6553 | `		apArg[0] = &pVm->sCoalesceKey;` |
|        5 |  6554 | `		apArg[1] = pTos;` |
|        5 |  6555 | `		if( pSet ){` |
|        5 |  6556 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|        2 |  6557 | `		}` |
|        - |  6558 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|        5 |  6559 | `		PH7_MemObjStore(pTos,pNos);` |
|        5 |  6560 | `		VmPopOperand(&pTos,1);` |
|        - |  6561 | `		/* Disarm and release the cached instance ref + key. */` |
|        5 |  6562 | `		VmCoalesceDisarm(pVm);` |
|        5 |  6563 | `		break;` |
|        - |  6564 | `	}` |
|       32 |  6565 | `	nIdx = pNos->nIdx;` |
|       32 |  6566 | `	if( nIdx == SXU32_HIGH ){` |
|      ! 0 |  6567 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  6568 | `			"Cannot perform assignment on a constant class attribute");` |
|       32 |  6569 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|       32 |  6570 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|       32 |  6571 | `		PH7_MemObjStore(pTos,pObj);` |
|       15 |  6572 | `	}` |
|       32 |  6573 | `	PH7_MemObjStore(pTos,pNos);` |
|       32 |  6574 | `	VmPopOperand(&pTos,1);` |
|       32 |  6575 | `	break;` |
|        - |  6576 |  |
|        - |  6577 | `/*` |
|        - |  6578 | ` * OP_SPREAD: * * *` |
|        - |  6579 | ` * Argument unpacking.  TOS must be an array (hashmap).` |
|        - |  6580 | ` * Replace TOS with the array's individual elements pushed onto the stack.` |
|        - |  6581 | ` * Accumulates the net stack growth in pVm->iSpreadExtra so the next CALL` |
|        - |  6582 | ` * can adjust its argument count (the CALL may not be the next instruction).` |
|        - |  6583 | ` */` |
|        9 |  6584 | `case PH7_OP_SPREAD: {` |
|        - |  6585 | `#ifdef UNTRUST` |
|        - |  6586 | `	if( pTos < pStack ){` |
|        - |  6587 | `		goto Abort;` |
|        - |  6588 | `	}` |
|        - |  6589 | `#endif` |
|       20 |  6590 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|       20 |  6591 | `		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;` |
|       20 |  6592 | `		sxu32 nEntry = pMap->nEntry;` |
|       20 |  6593 | `		if( nEntry == 0 ){` |
|        - |  6594 | `			/* Empty array — remove from stack */` |
|        3 |  6595 | `			VmPopOperand(&pTos, 1);` |
|        3 |  6596 | `			pVm->iSpreadExtra--; /* One expression produced zero args */` |
|       19 |  6597 | `		}else if( pVm->iSpreadExtra + (sxi32)(nEntry - 1) >= VM_STACK_GUARD ){` |
|        - |  6598 | `			/* Safety: refuse to expand beyond the stack guard margin */` |
|      ! 0 |  6599 | `			VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|        - |  6600 | `				"Argument unpacking: cumulative expansion exceeds stack guard (%d)",` |
|        - |  6601 | `				VM_STACK_GUARD);` |
|      ! 0 |  6602 | `		}else{` |
|        - |  6603 | `			ph7_hashmap_node *pNode2;` |
|        - |  6604 | `			ph7_value *pElem;` |
|        - |  6605 | `			sxu32 i;` |
|        - |  6606 | `			/* Overwrite TOS with first element */` |
|       18 |  6607 | `			pNode2 = pMap->pFirst;` |
|       18 |  6608 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       18 |  6609 | `			PH7_MemObjRelease(pTos);` |
|       18 |  6610 | `			if( pElem ){` |
|       18 |  6611 | `				PH7_MemObjLoad(pElem, pTos);` |
|        8 |  6612 | `			}` |
|       18 |  6613 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  6614 | `			/* Traverse in insertion order (pPrev is the forward link` |
|        - |  6615 | `			 * in PHL's circular doubly-linked hashmap node list). */` |
|       18 |  6616 | `			pNode2 = pNode2->pPrev;` |
|        - |  6617 | `			/* Push remaining elements */` |
|       44 |  6618 | `			for( i = 1; i < nEntry; i++ ){` |
|       28 |  6619 | `				pTos++;` |
|       28 |  6620 | `				PH7_MemObjInit(pVm, pTos);` |
|       28 |  6621 | `				pTos->nIdx = SXU32_HIGH;` |
|       28 |  6622 | `				pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode2->nValIdx);` |
|       28 |  6623 | `				if( pElem ){` |
|       28 |  6624 | `					PH7_MemObjLoad(pElem, pTos);` |
|       13 |  6625 | `				}` |
|       28 |  6626 | `				pNode2 = pNode2->pPrev;` |
|       15 |  6627 | `			}` |
|       18 |  6628 | `			pVm->iSpreadExtra += (sxi32)(nEntry - 1);` |
|        - |  6629 | `		}` |
|        9 |  6630 | `	}` |
|        - |  6631 | `	/* else: not an array — leave as-is (single arg) */` |
|       20 |  6632 | `	break;` |
|        - |  6633 |  |
|        - |  6634 | `/*` |
|        - |  6635 | ` * OP_FLAG_SPREAD: * * *` |
|        - |  6636 | ` * Mark the value at TOS as a spread source for the next LOAD_MAP.` |
|        - |  6637 | ` * Used by array literal unpacking '[...$arr]'.` |
|        - |  6638 | ` */` |
|       31 |  6639 | `case PH7_OP_FLAG_SPREAD: {` |
|        - |  6640 | `#ifdef UNTRUST` |
|        - |  6641 | `	if( pTos < pStack ){` |
|        - |  6642 | `		goto Abort;` |
|        - |  6643 | `	}` |
|        - |  6644 | `#endif` |
|       64 |  6645 | `	pTos->iFlags \|= MEMOBJ_AUX_SPREAD;` |
|       64 |  6646 | `	break;` |
|        - |  6647 |  |
|        - |  6648 | `/* OP_LXOR: * * *` |
|        - |  6649 | ` *` |
|        - |  6650 | ` * Pop two values off the stack. Take the logical XOR of the` |
|        - |  6651 | ` * two values and push the resulting boolean value back onto the` |
|        - |  6652 | ` * stack.` |
|        - |  6653 | ` * According to the PHP language reference manual:` |
|        - |  6654 | ` *  $a xor $b is evaluated to TRUE if either $a or $b is` |
|        - |  6655 | ` *  TRUE,but not both.` |
|        - |  6656 | ` */` |
|        5 |  6657 | `case PH7_OP_LXOR:{` |
|       11 |  6658 | `	ph7_value *pNos = &pTos[-1];` |
|       11 |  6659 | `	sxi32 v = 0;` |
|        - |  6660 | `#ifdef UNTRUST` |
|        - |  6661 | `	if( pNos < pStack ){` |
|        - |  6662 | `		goto Abort;` |
|        - |  6663 | `	}` |
|        - |  6664 | `#endif` |
|        - |  6665 | `	/* Force a boolean cast */` |
|       11 |  6666 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6667 | `		PH7_MemObjToBool(pTos);` |
|      ! 0 |  6668 | `	}` |
|       11 |  6669 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      ! 0 |  6670 | `		PH7_MemObjToBool(pNos);` |
|      ! 0 |  6671 | `	}` |
|       11 |  6672 | `	if( (pNos->x.iVal && !pTos->x.iVal) \|\| (pTos->x.iVal && !pNos->x.iVal) ){` |
|        7 |  6673 | `		v = 1;` |
|        3 |  6674 | `	}` |
|       11 |  6675 | `	VmPopOperand(&pTos,1);` |
|       11 |  6676 | `	pTos->x.iVal = v;` |
|       11 |  6677 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       11 |  6678 | `	break;` |
|        - |  6679 | `				 }` |
|        - |  6680 | `/* OP_EQ P1 P2 P3` |
|        - |  6681 | ` *` |
|        - |  6682 | ` * Pop the top two elements from the stack.  If they are equal, then` |
|        - |  6683 | ` * jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6684 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6685 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6686 | ` */` |
|        - |  6687 | `/* OP_NEQ P1 P2 P3` |
|        - |  6688 | ` *` |
|        - |  6689 | ` * Pop the top two elements from the stack. If they are not equal, then` |
|        - |  6690 | ` * jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6691 | ` * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6692 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6693 | ` */` |
|     4496 |  6694 | `case PH7_OP_EQ:` |
|        - |  6695 | `case PH7_OP_NEQ: {` |
|     8994 |  6696 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6697 | `	/* Perform the comparison and act accordingly */` |
|        - |  6698 | `#ifdef UNTRUST` |
|        - |  6699 | `	if( pNos < pStack ){` |
|        - |  6700 | `		goto Abort;` |
|        - |  6701 | `	}` |
|        - |  6702 | `#endif` |
|     8994 |  6703 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     8994 |  6704 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|       19 |  6705 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|     8985 |  6706 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|     8950 |  6707 | `		rc = rc == 0;` |
|     4476 |  6708 | `	}else{` |
|       28 |  6709 | `		rc = rc != 0;` |
|        - |  6710 | `	}` |
|     8994 |  6711 | `	VmPopOperand(&pTos,1);` |
|     8994 |  6712 | `	if( !pInstr->iP2 ){` |
|        - |  6713 | `		/* Push comparison result without taking the jump */` |
|     8994 |  6714 | `		PH7_MemObjRelease(pTos);` |
|     8994 |  6715 | `		pTos->x.iVal = rc;` |
|        - |  6716 | `		/* Invalidate any prior representation */` |
|     8994 |  6717 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     4498 |  6718 | `	}else{` |
|      ! 0 |  6719 | `		if( rc ){` |
|        - |  6720 | `			/* Jump to the desired location */` |
|      ! 0 |  6721 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6722 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6723 | `		}` |
|        - |  6724 | `	}` |
|     8994 |  6725 | `	break;` |
|        - |  6726 | `				 }` |
|        - |  6727 | `/* OP_TEQ P1 P2 *` |
|        - |  6728 | ` *` |
|        - |  6729 | ` * Pop the top two elements from the stack. If they have the same type and are equal` |
|        - |  6730 | ` * then jump to instruction P2. Otherwise, continue to the next instruction.` |
|        - |  6731 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6732 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6733 | ` */` |
|   159291 |  6734 | `case PH7_OP_TEQ: {` |
|   318584 |  6735 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6736 | `	/* Perform the comparison and act accordingly */` |
|        - |  6737 | `#ifdef UNTRUST` |
|        - |  6738 | `	if( pNos < pStack ){` |
|        - |  6739 | `		goto Abort;` |
|        - |  6740 | `	}` |
|        - |  6741 | `#endif` |
|   318584 |  6742 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   318584 |  6743 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6744 | `		rc = 0;` |
|        2 |  6745 | `	}else{` |
|   318582 |  6746 | `		rc = rc == 0;` |
|        - |  6747 | `	}` |
|   318584 |  6748 | `	VmPopOperand(&pTos,1);` |
|   318584 |  6749 | `	if( !pInstr->iP2 ){` |
|        - |  6750 | `		/* Push comparison result without taking the jump */` |
|   318584 |  6751 | `		PH7_MemObjRelease(pTos);` |
|   318584 |  6752 | `		pTos->x.iVal = rc;` |
|        - |  6753 | `		/* Invalidate any prior representation */` |
|   318584 |  6754 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   159293 |  6755 | `	}else{` |
|      ! 0 |  6756 | `		if( rc ){` |
|        - |  6757 | `			/* Jump to the desired location */` |
|      ! 0 |  6758 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6759 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6760 | `		}` |
|        - |  6761 | `	}` |
|   318584 |  6762 | `	break;` |
|        - |  6763 | `				 }` |
|        - |  6764 | `/* OP_TNE P1 P2 *` |
|        - |  6765 | ` *` |
|        - |  6766 | ` * Pop the top two elements from the stack.If they are not equal an they are not` |
|        - |  6767 | ` * of the same type, then jump to instruction P2. Otherwise, continue to the next` |
|        - |  6768 | ` * instruction.` |
|        - |  6769 | ` * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6770 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6771 | ` *` |
|        - |  6772 | ` */` |
|   122654 |  6773 | `case PH7_OP_TNE: {` |
|   245310 |  6774 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6775 | `	/* Perform the comparison and act accordingly */` |
|        - |  6776 | `#ifdef UNTRUST` |
|        - |  6777 | `	if( pNos < pStack ){` |
|        - |  6778 | `		goto Abort;` |
|        - |  6779 | `	}` |
|        - |  6780 | `#endif` |
|   245310 |  6781 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|   245310 |  6782 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        3 |  6783 | `		rc = 1;` |
|        2 |  6784 | `	}else{` |
|   245308 |  6785 | `		rc = rc != 0;` |
|        - |  6786 | `	}` |
|   245310 |  6787 | `	VmPopOperand(&pTos,1);` |
|   245310 |  6788 | `	if( !pInstr->iP2 ){` |
|        - |  6789 | `		/* Push comparison result without taking the jump */` |
|   245310 |  6790 | `		PH7_MemObjRelease(pTos);` |
|   245310 |  6791 | `		pTos->x.iVal = rc;` |
|        - |  6792 | `		/* Invalidate any prior representation */` |
|   245310 |  6793 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   122656 |  6794 | `	}else{` |
|      ! 0 |  6795 | `		if( rc ){` |
|        - |  6796 | `			/* Jump to the desired location */` |
|      ! 0 |  6797 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6798 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6799 | `		}` |
|        - |  6800 | `	}` |
|   245310 |  6801 | `	break;` |
|        - |  6802 | `				 }` |
|        - |  6803 | `/* OP_LT P1 P2 P3` |
|        - |  6804 | ` *` |
|        - |  6805 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6806 | ` * is less than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6807 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6808 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6809 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6810 | ` *` |
|        - |  6811 | ` */` |
|        - |  6812 | `/* OP_LE P1 P2 P3` |
|        - |  6813 | ` *` |
|        - |  6814 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6815 | ` * is less than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6816 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6817 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6818 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6819 | ` *` |
|        - |  6820 | ` */` |
|   112413 |  6821 | `case PH7_OP_LT:` |
|        - |  6822 | `case PH7_OP_LE: {` |
|   224872 |  6823 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6824 | `	/* Perform the comparison and act accordingly */` |
|        - |  6825 | `#ifdef UNTRUST` |
|        - |  6826 | `	if( pNos < pStack ){` |
|        - |  6827 | `		goto Abort;` |
|        - |  6828 | `	}` |
|        - |  6829 | `#endif` |
|   224872 |  6830 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   224872 |  6831 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6832 | `		rc = 0;` |
|   224868 |  6833 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|     1608 |  6834 | `		rc = rc < 1;` |
|      805 |  6835 | `	}else{` |
|   223258 |  6836 | `		rc = rc < 0;` |
|        - |  6837 | `	}` |
|   224872 |  6838 | `	VmPopOperand(&pTos,1);` |
|   224872 |  6839 | `	if( !pInstr->iP2 ){` |
|        - |  6840 | `		/* Push comparison result without taking the jump */` |
|   224872 |  6841 | `		PH7_MemObjRelease(pTos);` |
|   224872 |  6842 | `		pTos->x.iVal = rc;` |
|        - |  6843 | `		/* Invalidate any prior representation */` |
|   224872 |  6844 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   112459 |  6845 | `	}else{` |
|      ! 0 |  6846 | `		if( rc ){` |
|        - |  6847 | `			/* Jump to the desired location */` |
|      ! 0 |  6848 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6849 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6850 | `		}` |
|        - |  6851 | `	}` |
|   224872 |  6852 | `	break;` |
|        - |  6853 | `				}` |
|        - |  6854 | `/* OP_GT P1 P2 P3` |
|        - |  6855 | ` *` |
|        - |  6856 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6857 | ` * is greater than the first (next on stack),then jump to instruction P2.Otherwise` |
|        - |  6858 | ` * continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6859 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6860 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6861 | ` *` |
|        - |  6862 | ` */` |
|        - |  6863 | `/* OP_GE P1 P2 P3` |
|        - |  6864 | ` *` |
|        - |  6865 | ` * Pop the top two elements from the stack. If the second element (the top of stack)` |
|        - |  6866 | ` * is greater than or equal to the first (next on stack),then jump to instruction P2.` |
|        - |  6867 | ` * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.` |
|        - |  6868 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6869 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6870 | ` *` |
|        - |  6871 | ` */` |
|    55632 |  6872 | `case PH7_OP_GT:` |
|        - |  6873 | `case PH7_OP_GE: {` |
|   111266 |  6874 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6875 | `	/* Perform the comparison and act accordingly */` |
|        - |  6876 | `#ifdef UNTRUST` |
|        - |  6877 | `	if( pNos < pStack ){` |
|        - |  6878 | `		goto Abort;` |
|        - |  6879 | `	}` |
|        - |  6880 | `#endif` |
|   111266 |  6881 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   111266 |  6882 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        9 |  6883 | `		rc = 0;` |
|   111262 |  6884 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
|   110874 |  6885 | `		rc = rc >= 0;` |
|    55438 |  6886 | `	}else{` |
|      386 |  6887 | `		rc = rc > 0;` |
|        - |  6888 | `	}` |
|   111266 |  6889 | `	VmPopOperand(&pTos,1);` |
|   111266 |  6890 | `	if( !pInstr->iP2 ){` |
|        - |  6891 | `		/* Push comparison result without taking the jump */` |
|   111266 |  6892 | `		PH7_MemObjRelease(pTos);` |
|   111266 |  6893 | `		pTos->x.iVal = rc;` |
|        - |  6894 | `		/* Invalidate any prior representation */` |
|   111266 |  6895 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    55634 |  6896 | `	}else{` |
|      ! 0 |  6897 | `		if( rc ){` |
|        - |  6898 | `			/* Jump to the desired location */` |
|      ! 0 |  6899 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6900 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6901 | `		}` |
|        - |  6902 | `	}` |
|   111266 |  6903 | `	break;` |
|        - |  6904 | `				}` |
|        - |  6905 | `/* OP_SPACESHIP * * *` |
|        - |  6906 | ` *` |
|        - |  6907 | ` * Pop the top two elements from the stack. Push an integer result:` |
|        - |  6908 | ` *   -1 if left < right` |
|        - |  6909 | ` *    0 if left == right` |
|        - |  6910 | ` *    1 if left > right` |
|        - |  6911 | ` * Uses loose comparison (type juggling), same as <, >, ==.` |
|        - |  6912 | ` */` |
|       25 |  6913 | `case PH7_OP_SPACESHIP: {` |
|       51 |  6914 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6915 | `#ifdef UNTRUST` |
|        - |  6916 | `	if( pNos < pStack ){` |
|        - |  6917 | `		goto Abort;` |
|        - |  6918 | `	}` |
|        - |  6919 | `#endif` |
|       51 |  6920 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|       51 |  6921 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|        - |  6922 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|        7 |  6923 | `		rc = 1;` |
|        4 |  6924 | `	}else{` |
|        - |  6925 | `		/* Normalize to exactly -1, 0, or 1 */` |
|       45 |  6926 | `		rc = (rc > 0) - (rc < 0);` |
|        - |  6927 | `	}` |
|       51 |  6928 | `	VmPopOperand(&pTos,1);` |
|       51 |  6929 | `	PH7_MemObjRelease(pTos);` |
|       51 |  6930 | `	pTos->x.iVal = rc;` |
|       51 |  6931 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|       51 |  6932 | `	break;` |
|        - |  6933 | `				}` |
|        - |  6934 | `/* OP_SEQ P1 P2 *` |
|        - |  6935 | ` * Strict string comparison.` |
|        - |  6936 | ` * Pop the top two elements from the stack. If they are equal (pure text comparison)` |
|        - |  6937 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6938 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6939 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6940 | ` * use PH7_OP_EQ.` |
|        - |  6941 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6942 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6943 | ` */` |
|        - |  6944 | `/* OP_SNE P1 P2 *` |
|        - |  6945 | ` * Strict string comparison.` |
|        - |  6946 | ` * Pop the top two elements from the stack. If they are not equal (pure text comparison)` |
|        - |  6947 | ` * then jump to instruction P2.  Otherwise, continue to the next instruction.` |
|        - |  6948 | ` * If either operand is NULL then the comparison result is FALSE.` |
|        - |  6949 | ` * The SyMemcmp() routine is used for the comparison. For a numeric comparison` |
|        - |  6950 | ` * use PH7_OP_EQ.` |
|        - |  6951 | ` * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the` |
|        - |  6952 | ` * stack if the jump would have been taken, or a 0 (FALSE) if not.` |
|        - |  6953 | ` */` |
|       18 |  6954 | `case PH7_OP_SEQ:` |
|        - |  6955 | `case PH7_OP_SNE: {` |
|       38 |  6956 | `	ph7_value *pNos = &pTos[-1];` |
|        - |  6957 | `	SyString s1,s2;` |
|        - |  6958 | `	/* Perform the comparison and act accordingly */` |
|        - |  6959 | `#ifdef UNTRUST` |
|        - |  6960 | `	if( pNos < pStack ){` |
|        - |  6961 | `		goto Abort;` |
|        - |  6962 | `	}` |
|        - |  6963 | `#endif` |
|        - |  6964 | `	/* Force a string cast */` |
|       38 |  6965 | `	if((pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        5 |  6966 | `		PH7_MemObjToString(pTos);` |
|        2 |  6967 | `	}` |
|       38 |  6968 | `	if((pNos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 |  6969 | `		PH7_MemObjToString(pNos);` |
|      ! 0 |  6970 | `	}` |
|       38 |  6971 | `	SyStringInitFromBuf(&s1,SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob));` |
|       38 |  6972 | `	SyStringInitFromBuf(&s2,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       38 |  6973 | `	rc = SyStringCmp(&s1,&s2,SyMemcmp);` |
|       38 |  6974 | `	if( pInstr->iOp == PH7_OP_NEQ ){` |
|      ! 0 |  6975 | `		rc = rc != 0;` |
|      ! 0 |  6976 | `	}else{` |
|       38 |  6977 | `		rc = rc == 0;` |
|        - |  6978 | `	}` |
|       38 |  6979 | `	VmPopOperand(&pTos,1);` |
|       38 |  6980 | `	if( !pInstr->iP2 ){` |
|        - |  6981 | `		/* Push comparison result without taking the jump */` |
|       38 |  6982 | `		PH7_MemObjRelease(pTos);` |
|       38 |  6983 | `		pTos->x.iVal = rc;` |
|        - |  6984 | `		/* Invalidate any prior representation */` |
|       38 |  6985 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       20 |  6986 | `	}else{` |
|      ! 0 |  6987 | `		if( rc ){` |
|        - |  6988 | `			/* Jump to the desired location */` |
|      ! 0 |  6989 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  6990 | `			VmPopOperand(&pTos,1);` |
|      ! 0 |  6991 | `		}` |
|        - |  6992 | `	}` |
|       38 |  6993 | `	break;` |
|        - |  6994 | `				 }` |
|        - |  6995 | `/*` |
|        - |  6996 | ` * OP_LOAD_REF * * *` |
|        - |  6997 | ` * Push the index of a referenced object on the stack.` |
|        - |  6998 | ` */` |
|       57 |  6999 | `case PH7_OP_LOAD_REF: {` |
|        - |  7000 | `	sxu32 nIdx;` |
|        - |  7001 | `#ifdef UNTRUST` |
|        - |  7002 | `	if( pTos < pStack ){` |
|        - |  7003 | `		goto Abort;` |
|        - |  7004 | `	}` |
|        - |  7005 | `#endif` |
|        - |  7006 | `	/* Extract memory object index */` |
|      115 |  7007 | `	nIdx = pTos->nIdx;` |
|      115 |  7008 | `	if( nIdx != SXU32_HIGH /* Not a constant */ ){` |
|        - |  7009 | `		/* Nullify the object */` |
|       95 |  7010 | `		PH7_MemObjRelease(pTos);` |
|        - |  7011 | `		/* Mark as constant and store the index on the top of the stack */` |
|       95 |  7012 | `		pTos->x.iVal = (sxi64)nIdx;` |
|       95 |  7013 | `		pTos->nIdx = SXU32_HIGH;` |
|       95 |  7014 | `		pTos->iFlags = MEMOBJ_INT\|MEMOBJ_REFERENCE;` |
|       47 |  7015 | `	}` |
|      115 |  7016 | `	break;` |
|        - |  7017 | `					  }` |
|        - |  7018 | `/*` |
|        - |  7019 | ` * OP_STORE_REF * * P3` |
|        - |  7020 | ` * Perform an assignment operation by reference.` |
|        - |  7021 | ` */` |
|       16 |  7022 | ` case PH7_OP_STORE_REF: {` |
|       34 |  7023 | `	 SyString sName = { 0 , 0 };` |
|        - |  7024 | `	 VmFrame *pFrameLocal;` |
|        - |  7025 | `	SyHashEntry *pEntry;` |
|        - |  7026 | `	sxu32 nIdx;` |
|        - |  7027 | `#ifdef UNTRUST` |
|        - |  7028 | `	if( pTos < pStack ){` |
|        - |  7029 | `		goto Abort;` |
|        - |  7030 | `	}` |
|        - |  7031 | `#endif` |
|       34 |  7032 | `	if( pInstr->p3 == 0 ){` |
|        - |  7033 | `		char *zName;` |
|        - |  7034 | `		/* Take the variable name from the Next on the stack */` |
|      ! 0 |  7035 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7036 | `			/* Force a string cast */` |
|      ! 0 |  7037 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7038 | `		}` |
|      ! 0 |  7039 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7040 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|      ! 0 |  7041 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7042 | `			if( zName ){` |
|      ! 0 |  7043 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7044 | `			}` |
|      ! 0 |  7045 | `		}` |
|      ! 0 |  7046 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7047 | `		pTos--;` |
|      ! 0 |  7048 | `	}else{` |
|       34 |  7049 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7050 | `	}` |
|       34 |  7051 | `	nIdx = pTos->nIdx;` |
|       34 |  7052 | `	if(nIdx == SXU32_HIGH ){` |
|      ! 0 |  7053 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  7054 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7055 | `				"Reference operator require a variable not a constant as it's right operand");` |
|      ! 0 |  7056 | `		}else{` |
|        - |  7057 | `			ph7_value *pObj;` |
|        - |  7058 | `			/* Extract the desired variable and if not available dynamically create it */` |
|      ! 0 |  7059 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|      ! 0 |  7060 | `			if( pObj == 0 ){` |
|      ! 0 |  7061 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7062 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|      ! 0 |  7063 | `				goto Abort;` |
|        - |  7064 | `			}` |
|        - |  7065 | `			/* Perform the store operation */` |
|      ! 0 |  7066 | `			PH7_MemObjStore(pTos,pObj);` |
|      ! 0 |  7067 | `			pTos->nIdx = pObj->nIdx;` |
|      ! 0 |  7068 | `		}` |
|       34 |  7069 | `	}else if( sName.nByte > 0){` |
|       34 |  7070 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      ! 0 |  7071 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"$GLOBALS is a read-only array and therefore cannot be referenced");` |
|      ! 0 |  7072 | `		}else{` |
|       34 |  7073 | `			pFrameLocal = pVm->pFrame;` |
|       34 |  7074 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7075 | `			/* Query the local frame */` |
|       34 |  7076 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|       34 |  7077 | `			if( pEntry ){` |
|      ! 0 |  7078 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|      ! 0 |  7079 | `			}else{` |
|       34 |  7080 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|       34 |  7081 | `				if( pFrameLocal->pParent == 0 ){` |
|        - |  7082 | `					/* Insert in the $GLOBALS array */` |
|       30 |  7083 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|       14 |  7084 | `				}` |
|       34 |  7085 | `				if( rc == SXRET_OK ){` |
|       34 |  7086 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|       16 |  7087 | `				}` |
|        - |  7088 | `			}` |
|        - |  7089 | `		}` |
|       16 |  7090 | `	}` |
|       34 |  7091 | `	break;` |
|        - |  7092 | `				 }` |
|        - |  7093 | `/*` |
|        - |  7094 | ` * OP_UPLINK P1 * *` |
|        - |  7095 | ` * Link a variable to the top active VM frame.` |
|        - |  7096 | ` * This is used to implement the 'global' PHP construct.` |
|        - |  7097 | ` */` |
|       28 |  7098 | `case PH7_OP_UPLINK: {` |
|       58 |  7099 | `	if( pVm->pFrame->pParent ){` |
|       58 |  7100 | `		ph7_value *pLink = &pTos[-pInstr->iP1+1];` |
|        - |  7101 | `		SyString sName;` |
|        - |  7102 | `		/* Perform the link */` |
|      116 |  7103 | `		while( pLink <= pTos ){` |
|       60 |  7104 | `			if((pLink->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7105 | `				/* Force a string cast */` |
|      ! 0 |  7106 | `				PH7_MemObjToString(pLink);` |
|      ! 0 |  7107 | `			}` |
|       60 |  7108 | `			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));` |
|       60 |  7109 | `			if( sName.nByte > 0 ){` |
|       60 |  7110 | `				VmFrameLink(&(*pVm),&sName);` |
|       29 |  7111 | `			}` |
|       60 |  7112 | `			pLink++;` |
|        2 |  7113 | `		}` |
|       28 |  7114 | `	}` |
|       58 |  7115 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|       58 |  7116 | `	break;` |
|        - |  7117 | `					}` |
|        - |  7118 | `/*` |
|        - |  7119 | ` * OP_LOAD_EXCEPTION * P2 P3` |
|        - |  7120 | ` * Push an exception in the corresponding container so that` |
|        - |  7121 | ` * it can be thrown later by the OP_THROW instruction.` |
|        - |  7122 | ` */` |
|      163 |  7123 | `case PH7_OP_LOAD_EXCEPTION: {` |
|      328 |  7124 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|        - |  7125 | `	VmFrame *pFrameLocal;` |
|        - |  7126 | `	/* Reset per-entry state so finally runs on each iteration */` |
|      328 |  7127 | `	pException->iFinallyDone = 0;` |
|      328 |  7128 | `	SySetPut(&pVm->aException,(const void *)&pException);` |
|        - |  7129 | `	/* Create the exception frame */` |
|      328 |  7130 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|      328 |  7131 | `	if( rc != SXRET_OK ){` |
|      ! 0 |  7132 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|      ! 0 |  7133 | `		goto Abort;` |
|        - |  7134 | `	}` |
|        - |  7135 | `	/* Mark the special frame */` |
|      328 |  7136 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|      328 |  7137 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|        - |  7138 | `	/* Point to the frame that trigger the exception */` |
|      328 |  7139 | `	pFrameLocal = pFrameLocal->pParent;` |
|      328 |  7140 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      328 |  7141 | `	pException->pFrame = pFrameLocal;` |
|      328 |  7142 | `	break;` |
|        - |  7143 | `							}` |
|        - |  7144 | `/*` |
|        - |  7145 | ` * OP_POP_EXCEPTION * * P3` |
|        - |  7146 | ` * Pop a previously pushed exception from the corresponding container.` |
|        - |  7147 | ` */` |
|      162 |  7148 | `case PH7_OP_POP_EXCEPTION: {` |
|      326 |  7149 | `	ph7_exception *pException = (ph7_exception *)pInstr->p3;` |
|      326 |  7150 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - |  7151 | `		ph7_exception **apException;` |
|        - |  7152 | `		/* Pop the loaded exception */` |
|       32 |  7153 | `		apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       32 |  7154 | `		if( pException == apException[SySetUsed(&pVm->aException) - 1] ){` |
|       30 |  7155 | `			(void)SySetPop(&pVm->aException);` |
|       14 |  7156 | `		}` |
|       15 |  7157 | `	}` |
|      326 |  7158 | `	pException->pFrame = 0;` |
|        - |  7159 | `	/* Leave the exception frame */` |
|      326 |  7160 | `	VmLeaveFrame(&(*pVm));` |
|        - |  7161 | `	/* Execute the finally block if present and not already executed by catch path */` |
|      326 |  7162 | `	if( pException->iHasFinally && !pException->iFinallyDone ){` |
|        - |  7163 | `		sxi32 rcFinally;` |
|       20 |  7164 | `		pException->iFinallyDone = 1;` |
|       20 |  7165 | `		rcFinally = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       20 |  7166 | `		if( rcFinally == SXERR_ABORT ){` |
|      ! 0 |  7167 | `			goto Abort;` |
|        - |  7168 | `		}` |
|        9 |  7169 | `	}` |
|      326 |  7170 | `	break;` |
|        - |  7171 | `							}` |
|        - |  7172 |  |
|        - |  7173 | `/*` |
|        - |  7174 | ` * OP_THROW * P2 *` |
|        - |  7175 | ` * Throw an user exception.` |
|        - |  7176 | ` */` |
|       59 |  7177 | `case PH7_OP_THROW: {` |
|      120 |  7178 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|      120 |  7179 | `	sxu32 nJump = pInstr->iP2;` |
|        - |  7180 | `#ifdef UNTRUST` |
|        - |  7181 | `	if( pTos < pStack ){` |
|        - |  7182 | `		goto Abort;` |
|        - |  7183 | `	}` |
|        - |  7184 | `#endif` |
|      120 |  7185 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|        - |  7186 | `	/* Tell the upper layer that an exception was thrown */` |
|      120 |  7187 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|      120 |  7188 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      120 |  7189 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7190 | `		ph7_class *pThrowable;` |
|        - |  7191 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|      120 |  7192 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|      121 |  7193 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|        - |  7194 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|        - |  7195 | `			 * Error::__construct is defined in the built-in library and` |
|        - |  7196 | `			 * cannot realistically fail, so we do not check its return. */` |
|        3 |  7197 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|        3 |  7198 | `			ph7_class_instance *pErrInst = 0;` |
|        3 |  7199 | `			if( pErrorClass ){` |
|        3 |  7200 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|        1 |  7201 | `			}` |
|        3 |  7202 | `			if( pErrInst ){` |
|        - |  7203 | `				ph7_class_method *pCons;` |
|        3 |  7204 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|        3 |  7205 | `				if( pCons ){` |
|        - |  7206 | `					ph7_value sArg;` |
|        - |  7207 | `					ph7_value *apArg[1];` |
|        - |  7208 | `					SyString sMsgStr;` |
|        - |  7209 | `					static const char zErrMsg[] =` |
|        - |  7210 | `						"Cannot throw objects that do not implement Throwable";` |
|        3 |  7211 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|        3 |  7212 | `					PH7_MemObjInit(pVm,&sArg);` |
|        3 |  7213 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|        3 |  7214 | `					apArg[0] = &sArg;` |
|        3 |  7215 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|        3 |  7216 | `					PH7_MemObjRelease(&sArg);` |
|        1 |  7217 | `				}` |
|        3 |  7218 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|        3 |  7219 | `				PH7_ClassInstanceUnref(pErrInst);` |
|        3 |  7220 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7221 | `					goto Abort;` |
|        - |  7222 | `				}` |
|        2 |  7223 | `			}else{` |
|        - |  7224 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|      ! 0 |  7225 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|      ! 0 |  7226 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  7227 | `					goto Abort;` |
|        - |  7228 | `				}` |
|        - |  7229 | `			}` |
|        2 |  7230 | `		}else{` |
|        - |  7231 | `			/* Throw the exception */` |
|      118 |  7232 | `			rc = VmThrowException(&(*pVm),pThis);` |
|      118 |  7233 | `			if( rc == SXERR_ABORT ){` |
|        - |  7234 | `				/* Abort processing immediately */` |
|       11 |  7235 | `				goto Abort;` |
|        - |  7236 | `			}` |
|        - |  7237 | `		}` |
|       56 |  7238 | `	}else{` |
|        - |  7239 | `		/* Expecting a class instance */` |
|      ! 0 |  7240 | `		VmUncaughtException(&(*pVm),0);` |
|      ! 0 |  7241 | `		if( rc == SXERR_ABORT ){` |
|        - |  7242 | `			/* Abort processing immediately */` |
|      ! 0 |  7243 | `			goto Abort;` |
|        - |  7244 | `		}` |
|        - |  7245 | `	}` |
|        - |  7246 | `	/* Pop the top entry */` |
|      110 |  7247 | `	VmPopOperand(&pTos,1);` |
|        - |  7248 | `	/* Perform an unconditional jump */` |
|      110 |  7249 | `	pc = nJump - 1;` |
|      110 |  7250 | `	break;` |
|        - |  7251 | `				   }` |
|        - |  7252 | `/*` |
|        - |  7253 | ` * OP_FOREACH_INIT * P2 P3` |
|        - |  7254 | ` * Prepare a foreach step.` |
|        - |  7255 | ` */` |
|     6049 |  7256 | `case PH7_OP_FOREACH_INIT: {` |
|    12100 |  7257 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7258 | `	void *pName;` |
|        - |  7259 | `#ifdef UNTRUST` |
|        - |  7260 | `	if( pTos < pStack ){` |
|        - |  7261 | `		goto Abort;` |
|        - |  7262 | `	}` |
|        - |  7263 | `#endif` |
|    12100 |  7264 | `	if( SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7265 | `		/* Take the variable name from the top of the stack */` |
|      ! 0 |  7266 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7267 | `			/* Force a string cast */` |
|      ! 0 |  7268 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7269 | `		}` |
|        - |  7270 | `		/* Duplicate name */` |
|      ! 0 |  7271 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7272 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7273 | `			SyStringInitFromBuf(&pInfo->sValue,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7274 | `		}` |
|      ! 0 |  7275 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7276 | `	}` |
|    12100 |  7277 | `	if( (pInfo->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) < 1 ){` |
|      ! 0 |  7278 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|        - |  7279 | `			/* Force a string cast */` |
|      ! 0 |  7280 | `			PH7_MemObjToString(pTos);` |
|      ! 0 |  7281 | `		}` |
|        - |  7282 | `		/* Duplicate name */` |
|      ! 0 |  7283 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      ! 0 |  7284 | `			pName = SyMemBackendDup(&pVm->sAllocator,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7285 | `			SyStringInitFromBuf(&pInfo->sKey,pName,SyBlobLength(&pTos->sBlob));` |
|      ! 0 |  7286 | `		}` |
|      ! 0 |  7287 | `		VmPopOperand(&pTos,1);` |
|      ! 0 |  7288 | `	}` |
|        - |  7289 | `	/* Make sure we are dealing with a hashmap aka 'array' or an object */` |
|    12100 |  7290 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 \|\| SyStringLength(&pInfo->sValue) < 1 ){` |
|        - |  7291 | `		/* Jump out of the loop */` |
|      ! 0 |  7292 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  7293 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"Invalid argument supplied for the foreach statement,expecting array or class instance");` |
|      ! 0 |  7294 | `		}` |
|      ! 0 |  7295 | `		pc = pInstr->iP2 - 1;` |
|      ! 0 |  7296 | `	}else{` |
|        - |  7297 | `		ph7_foreach_step *pStep;` |
|    12100 |  7298 | `		pStep = (ph7_foreach_step *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_foreach_step));` |
|    12100 |  7299 | `		if( pStep == 0 ){` |
|      ! 0 |  7300 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|        - |  7301 | `			/* Jump out of the loop */` |
|      ! 0 |  7302 | `			pc = pInstr->iP2 - 1;` |
|      ! 0 |  7303 | `		}else{` |
|        - |  7304 | `			/* Zero the structure */` |
|    12100 |  7305 | `			SyZero(pStep,sizeof(ph7_foreach_step));` |
|        - |  7306 | `			/* Prepare the step */` |
|    12100 |  7307 | `			pStep->iFlags = pInfo->iFlags;` |
|    12100 |  7308 | `			if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  7309 | `				ph7_hashmap *pMap;` |
|        - |  7310 | `				/* COW: For by-reference foreach, eagerly separate the` |
|        - |  7311 | `				 * source array so mutations don't affect other sharers. */` |
|    12066 |  7312 | `				if( (pStep->iFlags & PH7_4EACH_STEP_REF) && pTos->nIdx != SXU32_HIGH ){` |
|        9 |  7313 | `					ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx);` |
|        9 |  7314 | `					if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
|        9 |  7315 | `						ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7316 | `						/* Only adjust refcounts/separate if the backing` |
|        - |  7317 | `						 * variable still points at the same hashmap as` |
|        - |  7318 | `						 * the stack value. */` |
|        9 |  7319 | `						if( pBacking->x.pOther == (void *)pCur ){` |
|        9 |  7320 | `							pCur->iRef--;` |
|        9 |  7321 | `							PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
|        9 |  7322 | `							pTos->x.pOther = pBacking->x.pOther;` |
|        9 |  7323 | `							((ph7_hashmap *)pTos->x.pOther)->iRef++;` |
|        4 |  7324 | `						}` |
|        4 |  7325 | `					}` |
|        4 |  7326 | `				}` |
|    12066 |  7327 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|        - |  7328 | `				/* Reset the internal loop cursor */` |
|    12066 |  7329 | `				PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7330 | `				/* Mark the step */` |
|    12066 |  7331 | `				pStep->iFlags \|= PH7_4EACH_STEP_HASHMAP;` |
|    12066 |  7332 | `				pStep->xIter.pMap = pMap;` |
|    12066 |  7333 | `				pMap->iRef++;` |
|     6034 |  7334 | `			}else{` |
|       36 |  7335 | `				ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  7336 | `				ph7_class *pIteratorClass;` |
|        - |  7337 | `				/* Check if the object implements Iterator */` |
|       36 |  7338 | `				pIteratorClass = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);` |
|       47 |  7339 | `				if( pIteratorClass && PH7_VmInstanceOf(pThis->pClass,pIteratorClass) ){` |
|        - |  7340 | `					/* Iterator-based iteration: call rewind() */` |
|        - |  7341 | `					ph7_class_method *pRewind;` |
|       24 |  7342 | `					pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|       24 |  7343 | `					pStep->xIter.pThis = pThis;` |
|       24 |  7344 | `					pThis->iRef++;` |
|       24 |  7345 | `					pRewind = PH7_ClassExtractMethod(pThis->pClass,"rewind",sizeof("rewind")-1);` |
|       24 |  7346 | `					if( pRewind ){` |
|       24 |  7347 | `						PH7_VmCallClassMethod(&(*pVm),pThis,pRewind,0,0,0);` |
|       11 |  7348 | `					}` |
|       13 |  7349 | `				}else{` |
|        - |  7350 | `					/* Check if the object implements IteratorAggregate */` |
|        - |  7351 | `					ph7_class *pIterAggClass;` |
|       14 |  7352 | `					pIterAggClass = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",` |
|        - |  7353 | `						sizeof("IteratorAggregate")-1,FALSE,0);` |
|       15 |  7354 | `					if( pIterAggClass && PH7_VmInstanceOf(pThis->pClass,pIterAggClass) ){` |
|        - |  7355 | `						/* Call getIterator() and use the returned Iterator object */` |
|        - |  7356 | `						ph7_class_method *pGetIter;` |
|        3 |  7357 | `						int iterAggOk = 0;` |
|        3 |  7358 | `						pGetIter = PH7_ClassExtractMethod(pThis->pClass,"getIterator",sizeof("getIterator")-1);` |
|        3 |  7359 | `						if( pGetIter ){` |
|        - |  7360 | `							ph7_value sResult;` |
|        3 |  7361 | `							PH7_MemObjInit(&(*pVm),&sResult);` |
|        3 |  7362 | `							PH7_VmCallClassMethod(&(*pVm),pThis,pGetIter,&sResult,0,0);` |
|        3 |  7363 | `							if( (sResult.iFlags & MEMOBJ_OBJ) && sResult.x.pOther ){` |
|        3 |  7364 | `								ph7_class_instance *pIterObj = (ph7_class_instance *)sResult.x.pOther;` |
|        3 |  7365 | `								if( pIteratorClass && PH7_VmInstanceOf(pIterObj->pClass,pIteratorClass) ){` |
|        - |  7366 | `									ph7_class_method *pRewind;` |
|        3 |  7367 | `									pStep->iFlags \|= PH7_4EACH_STEP_ITERATOR\|PH7_4EACH_STEP_FIRST;` |
|        3 |  7368 | `									pStep->xIter.pThis = pIterObj;` |
|        3 |  7369 | `									pIterObj->iRef++;` |
|        - |  7370 | `									/* Retain the aggregate so it lives for the duration of the foreach */` |
|        3 |  7371 | `									pStep->pOwner = pThis;` |
|        3 |  7372 | `									pThis->iRef++;` |
|        3 |  7373 | `									pRewind = PH7_ClassExtractMethod(pIterObj->pClass,"rewind",sizeof("rewind")-1);` |
|        3 |  7374 | `									if( pRewind ){` |
|        3 |  7375 | `										PH7_VmCallClassMethod(&(*pVm),pIterObj,pRewind,0,0,0);` |
|        1 |  7376 | `									}` |
|        3 |  7377 | `									iterAggOk = 1;` |
|        1 |  7378 | `								}` |
|        1 |  7379 | `							}` |
|        3 |  7380 | `							PH7_MemObjRelease(&sResult);` |
|        1 |  7381 | `						}` |
|        3 |  7382 | `						if( !iterAggOk ){` |
|        - |  7383 | `							/* getIterator() failed or returned non-Iterator: abort this foreach */` |
|      ! 0 |  7384 | `							PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  7385 | `								"Object returned by getIterator() must implement Iterator");` |
|      ! 0 |  7386 | `							SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      ! 0 |  7387 | `							pStep = 0; /* Signal: do not store this step */` |
|      ! 0 |  7388 | `							pc = pInstr->iP2 - 1;` |
|      ! 0 |  7389 | `						}` |
|        2 |  7390 | `					}else{` |
|        - |  7391 | `						/* Plain object iteration via hAttr */` |
|       12 |  7392 | `						SyHashResetLoopCursor(&pThis->hAttr);` |
|       12 |  7393 | `						pStep->iFlags \|= PH7_4EACH_STEP_OBJECT;` |
|       12 |  7394 | `						pStep->xIter.pThis = pThis;` |
|       12 |  7395 | `						pThis->iRef++;` |
|        - |  7396 | `					}` |
|        - |  7397 | `				}` |
|        - |  7398 | `			}` |
|        - |  7399 | `		}` |
|    12100 |  7400 | `		if( pStep ){` |
|    12100 |  7401 | `			if( SXRET_OK != SySetPut(&pInfo->aStep,(const void *)&pStep) ){` |
|      ! 0 |  7402 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"PH7 is running out of memory while preparing the 'foreach' step");` |
|      ! 0 |  7403 | `				SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        - |  7404 | `				/* Jump out of the loop */` |
|      ! 0 |  7405 | `				pc = pInstr->iP2 - 1;` |
|      ! 0 |  7406 | `			}` |
|     6049 |  7407 | `		}` |
|        - |  7408 | `	}` |
|    12100 |  7409 | `	VmPopOperand(&pTos,1);` |
|    12100 |  7410 | `	break;` |
|        - |  7411 | `						  }` |
|        - |  7412 | `/*` |
|        - |  7413 | ` * OP_FOREACH_STEP * P2 P3` |
|        - |  7414 | ` * Perform a foreach step. Jump to P2 at the end of the step.` |
|        - |  7415 | ` */` |
|    99185 |  7416 | `case PH7_OP_FOREACH_STEP: {` |
|   198372 |  7417 | `	ph7_foreach_info *pInfo = (ph7_foreach_info *)pInstr->p3;` |
|        - |  7418 | `	ph7_foreach_step **apStep,*pStep;` |
|        - |  7419 | `	ph7_value *pValue;` |
|        - |  7420 | `	VmFrame *pFrameLocal;` |
|        - |  7421 | `	/* Peek the last step */` |
|   198372 |  7422 | `	apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|   198372 |  7423 | `	pStep = apStep[SySetUsed(&pInfo->aStep) - 1];` |
|   198372 |  7424 | `	pFrameLocal = pVm->pFrame;` |
|   198372 |  7425 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   198372 |  7426 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|   198238 |  7427 | `		ph7_hashmap *pMap = pStep->xIter.pMap;` |
|        - |  7428 | `		ph7_hashmap_node *pNode;` |
|        - |  7429 | `		/* Extract the current node value */` |
|   198238 |  7430 | `		pNode = PH7_HashmapGetNextEntry(pMap);` |
|   198238 |  7431 | `		if( pNode == 0 ){` |
|        - |  7432 | `			/* No more entry to process */` |
|    12064 |  7433 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|    12064 |  7434 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7435 | `				/* Break the reference with the last element */` |
|        7 |  7436 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        3 |  7437 | `			}` |
|        - |  7438 | `			/* Automatically reset the loop cursor */` |
|    12064 |  7439 | `			PH7_HashmapResetLoopCursor(pMap);` |
|        - |  7440 | `			/* Cleanup the mess left behind */` |
|    12064 |  7441 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|    12064 |  7442 | `			SySetPop(&pInfo->aStep);` |
|    12064 |  7443 | `			PH7_HashmapUnref(pMap);` |
|     6033 |  7444 | `		}else{` |
|   186176 |  7445 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|      506 |  7446 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|      506 |  7447 | `				if( pKey ){` |
|      506 |  7448 | `					PH7_HashmapExtractNodeKey(pNode,pKey);` |
|      252 |  7449 | `				}` |
|      252 |  7450 | `			}` |
|   186176 |  7451 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7452 | `				SyHashEntry *pEntry;` |
|        - |  7453 | `				/* Pass by reference */` |
|       23 |  7454 | `				pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|       23 |  7455 | `				if( pEntry ){` |
|       21 |  7456 | `					pEntry->pUserData = SX_INT_TO_PTR(pNode->nValIdx);` |
|       11 |  7457 | `				}else{` |
|        4 |  7458 | `					SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|        2 |  7459 | `						SX_INT_TO_PTR(pNode->nValIdx));` |
|        - |  7460 | `				}` |
|       12 |  7461 | `			}else{` |
|        - |  7462 | `				/* Make a copy of the entry value */` |
|   186154 |  7463 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|   186154 |  7464 | `				if( pValue ){` |
|   186154 |  7465 | `					PH7_HashmapExtractNodeValue(pNode,pValue,TRUE);` |
|    93076 |  7466 | `				}` |
|        - |  7467 | `			}` |
|        2 |  7468 | `		}` |
|    99254 |  7469 | `	}else if( pStep->iFlags & PH7_4EACH_STEP_ITERATOR ){` |
|        - |  7470 | `		/* Iterator-based iteration.` |
|        - |  7471 | `		 * Sequence: on first call just check valid/current/key.` |
|        - |  7472 | `		 * On subsequent calls, advance with next() first, then check.` |
|        - |  7473 | `		 */` |
|      106 |  7474 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|        - |  7475 | `		ph7_class_method *pMethod;` |
|        - |  7476 | `		ph7_value sResult;` |
|      106 |  7477 | `		int isValid = 0;` |
|        - |  7478 | `		/* Call next() to advance — but skip on the first iteration */` |
|      106 |  7479 | `		if( pStep->iFlags & PH7_4EACH_STEP_FIRST ){` |
|       26 |  7480 | `			pStep->iFlags &= ~PH7_4EACH_STEP_FIRST;` |
|       14 |  7481 | `		}else{` |
|       82 |  7482 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"next",sizeof("next")-1);` |
|       82 |  7483 | `			if( pMethod ){` |
|       82 |  7484 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,0,0,0);` |
|       40 |  7485 | `			}` |
|        - |  7486 | `		}` |
|        - |  7487 | `		/* Call valid() */` |
|      106 |  7488 | `		PH7_MemObjInit(pVm,&sResult);` |
|      106 |  7489 | `		pMethod = PH7_ClassExtractMethod(pThis->pClass,"valid",sizeof("valid")-1);` |
|      106 |  7490 | `		if( pMethod ){` |
|      106 |  7491 | `			PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|      106 |  7492 | `			PH7_MemObjToBool(&sResult);` |
|      106 |  7493 | `			isValid = (sResult.x.iVal != 0);` |
|       52 |  7494 | `		}` |
|      106 |  7495 | `		PH7_MemObjRelease(&sResult);` |
|      106 |  7496 | `		if( !isValid ){` |
|        - |  7497 | `			/* Iterator exhausted */` |
|       24 |  7498 | `			pc = pInstr->iP2 - 1;` |
|        - |  7499 | `			/* Release the aggregate owner if this was an IteratorAggregate foreach */` |
|       24 |  7500 | `			if( pStep->pOwner ){` |
|        3 |  7501 | `				PH7_ClassInstanceUnref(pStep->pOwner);` |
|        1 |  7502 | `			}` |
|       24 |  7503 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       24 |  7504 | `			SySetPop(&pInfo->aStep);` |
|       24 |  7505 | `			PH7_ClassInstanceUnref(pThis);` |
|       13 |  7506 | `		}else{` |
|        - |  7507 | `			/* Call current() to get value */` |
|       84 |  7508 | `			PH7_MemObjInit(pVm,&sResult);` |
|       84 |  7509 | `			pMethod = PH7_ClassExtractMethod(pThis->pClass,"current",sizeof("current")-1);` |
|       84 |  7510 | `			if( pMethod ){` |
|       84 |  7511 | `				PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sResult,0,0);` |
|       41 |  7512 | `			}` |
|       84 |  7513 | `			pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       84 |  7514 | `			if( pValue ){` |
|       84 |  7515 | `				PH7_MemObjStore(&sResult,pValue);` |
|       41 |  7516 | `			}` |
|       84 |  7517 | `			PH7_MemObjRelease(&sResult);` |
|        - |  7518 | `			/* Call key() if needed */` |
|       84 |  7519 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0 ){` |
|        - |  7520 | `				ph7_value sKey;` |
|       35 |  7521 | `				PH7_MemObjInit(pVm,&sKey);` |
|       35 |  7522 | `				pMethod = PH7_ClassExtractMethod(pThis->pClass,"key",sizeof("key")-1);` |
|       35 |  7523 | `				if( pMethod ){` |
|       35 |  7524 | `					PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,&sKey,0,0);` |
|       17 |  7525 | `				}` |
|       35 |  7526 | `				pValue = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       35 |  7527 | `				if( pValue ){` |
|       35 |  7528 | `					PH7_MemObjStore(&sKey,pValue);` |
|       17 |  7529 | `				}` |
|       35 |  7530 | `				PH7_MemObjRelease(&sKey);` |
|       17 |  7531 | `			}` |
|        - |  7532 | `		}` |
|       54 |  7533 | `	}else{` |
|       32 |  7534 | `		ph7_class_instance *pThis = pStep->xIter.pThis;` |
|       32 |  7535 | `		VmClassAttr *pVmAttr = 0; /* Stupid cc -06 warning */` |
|        - |  7536 | `		SyHashEntry *pEntry;` |
|        - |  7537 | `		/* Point to the next attribute */` |
|       36 |  7538 | `		while((pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|       26 |  7539 | `			pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        - |  7540 | `			/* Check access permission */` |
|       38 |  7541 | `			if( PH7_VmClassMemberAccess(&(*pVm),pThis->pClass,&pVmAttr->pAttr->sName,` |
|       24 |  7542 | `				pVmAttr->pAttr->iProtection,FALSE) ){` |
|       22 |  7543 | `					break; /* Access is granted */` |
|        - |  7544 | `			}` |
|        1 |  7545 | `		}` |
|       32 |  7546 | `		if( pEntry == 0 ){` |
|        - |  7547 | `			/* Clean up the mess left behind */` |
|       12 |  7548 | `			pc = pInstr->iP2 - 1; /* Jump to this destination */` |
|       12 |  7549 | `			if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7550 | `				/* Break the reference with the last element */` |
|        3 |  7551 | `				SyHashDeleteEntry(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),0);` |
|        1 |  7552 | `			}` |
|       12 |  7553 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       12 |  7554 | `			SySetPop(&pInfo->aStep);` |
|       12 |  7555 | `			PH7_ClassInstanceUnref(pThis);` |
|        7 |  7556 | `		}else{` |
|       22 |  7557 | `			SyString *pAttrName = &pVmAttr->pAttr->sName;` |
|        - |  7558 | `			ph7_value *pAttrValue;` |
|       22 |  7559 | `			if( (pStep->iFlags & PH7_4EACH_STEP_KEY) && SyStringLength(&pInfo->sKey) > 0){` |
|        - |  7560 | `				/* Fill with the current attribute name */` |
|       22 |  7561 | `				ph7_value *pKey = VmExtractMemObj(&(*pVm),&pInfo->sKey,FALSE,TRUE);` |
|       22 |  7562 | `				if( pKey ){` |
|       22 |  7563 | `					SyBlobReset(&pKey->sBlob);` |
|       22 |  7564 | `					SyBlobAppend(&pKey->sBlob,pAttrName->zString,pAttrName->nByte);` |
|       22 |  7565 | `					MemObjSetType(pKey,MEMOBJ_STRING);` |
|       10 |  7566 | `				}` |
|       10 |  7567 | `			}` |
|        - |  7568 | `			/* Extract attribute value */` |
|       22 |  7569 | `			pAttrValue = PH7_ClassInstanceExtractAttrValue(pThis,pVmAttr);` |
|       22 |  7570 | `			if( pAttrValue ){` |
|       22 |  7571 | `				if( pStep->iFlags & PH7_4EACH_STEP_REF ){` |
|        - |  7572 | `					/* Pass by reference */` |
|        3 |  7573 | `					pEntry = SyHashGet(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue));` |
|        3 |  7574 | `					if( pEntry ){` |
|        3 |  7575 | `						pEntry->pUserData = SX_INT_TO_PTR(pVmAttr->nIdx);` |
|        2 |  7576 | `					}else{` |
|      ! 0 |  7577 | `						SyHashInsert(&pFrameLocal->hVar,SyStringData(&pInfo->sValue),SyStringLength(&pInfo->sValue),` |
|      ! 0 |  7578 | `							SX_INT_TO_PTR(pVmAttr->nIdx));` |
|        - |  7579 | `					}` |
|        2 |  7580 | `				}else{` |
|        - |  7581 | `					/* Make a copy of the attribute value */` |
|       20 |  7582 | `					pValue = VmExtractMemObj(&(*pVm),&pInfo->sValue,FALSE,TRUE);` |
|       20 |  7583 | `					if( pValue ){` |
|       20 |  7584 | `						PH7_MemObjStore(pAttrValue,pValue);` |
|        9 |  7585 | `					}` |
|        - |  7586 | `				}` |
|       10 |  7587 | `			}` |
|        - |  7588 | `		}` |
|        - |  7589 | `	}` |
|   198372 |  7590 | `	break;` |
|        - |  7591 | `						  }` |
|        - |  7592 | `/*` |
|        - |  7593 | ` * OP_MEMBER P1 P2` |
|        - |  7594 | ` * Load class attribute/method on the stack.` |
|        - |  7595 | ` */` |
|     3840 |  7596 | `case PH7_OP_MEMBER: {` |
|        - |  7597 | `	ph7_class_instance *pThis;` |
|        - |  7598 | `	ph7_value *pNos;` |
|        - |  7599 | `	SyString sName;` |
|     7682 |  7600 | `	if( !pInstr->iP1 ){` |
|     7456 |  7601 | `		pNos = &pTos[-1];` |
|        - |  7602 | `#ifdef UNTRUST` |
|        - |  7603 | `		if( pNos < pStack ){` |
|        - |  7604 | `			goto Abort;` |
|        - |  7605 | `		}` |
|        - |  7606 | `#endif` |
|     7456 |  7607 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7608 | `			ph7_class *pClass;` |
|        - |  7609 | `			/* Class already instantiated */` |
|     7454 |  7610 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        - |  7611 | `			/* Point to the instantiated class */` |
|     7454 |  7612 | `			pClass = pThis->pClass;` |
|        - |  7613 | `			/* Extract attribute name first */` |
|     7454 |  7614 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     7454 |  7615 | `			if( pInstr->iP2 ){` |
|        - |  7616 | `				/* Method call */` |
|      748 |  7617 | `				ph7_class_method *pMeth = 0;` |
|      748 |  7618 | `				if( sName.nByte > 0 ){` |
|        - |  7619 | `					/* Extract the target method */` |
|      748 |  7620 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|      373 |  7621 | `				}` |
|      748 |  7622 | `				if( pMeth == 0 ){` |
|      ! 0 |  7623 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class method '%z->%z',PH7 is loading NULL",` |
|      ! 0 |  7624 | `						&pClass->sName,&sName` |
|        - |  7625 | `						);` |
|        - |  7626 | `					/* Call the '__Call()' magic method if available */` |
|      ! 0 |  7627 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__call",sizeof("__call")-1,&sName);` |
|        - |  7628 | `					/* Pop the method name from the stack */` |
|      ! 0 |  7629 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7630 | `					PH7_MemObjRelease(pTos);` |
|      ! 0 |  7631 | `				}else{` |
|        - |  7632 | `					/* Push method name on the stack */` |
|      748 |  7633 | `					PH7_MemObjRelease(pTos);` |
|      748 |  7634 | `					SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|      748 |  7635 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7636 | `				}` |
|      748 |  7637 | `				pTos->nIdx = SXU32_HIGH;` |
|      375 |  7638 | `			}else{` |
|        - |  7639 | `				/* Attribute access */` |
|     6708 |  7640 | `				VmClassAttr *pObjAttr = 0;` |
|        - |  7641 | `				SyHashEntry *pEntry;` |
|        - |  7642 | `				/* Extract the target attribute */` |
|     6708 |  7643 | `				if( sName.nByte > 0 ){` |
|     6708 |  7644 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|     6708 |  7645 | `					if( pEntry ){` |
|        - |  7646 | `						/* Point to the attribute value */` |
|     6706 |  7647 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|     3352 |  7648 | `					}` |
|     3353 |  7649 | `				}` |
|     6708 |  7650 | `				if( pObjAttr == 0 ){` |
|        - |  7651 | `					/* No such attribute,load null */` |
|        4 |  7652 | `					VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z->%z',PH7 is loading NULL",` |
|        1 |  7653 | `						&pClass->sName,&sName);` |
|        - |  7654 | `					/* Call the __get magic method if available */` |
|        3 |  7655 | `					PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName);` |
|        1 |  7656 | `				}` |
|     6708 |  7657 | `				VmPopOperand(&pTos,1);` |
|        - |  7658 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|        - |  7659 | `				 * This is due to the following case:` |
|        - |  7660 | `				 *     (new TestClass())->foo;` |
|        - |  7661 | `				 */` |
|     6708 |  7662 | `				pThis->iRef++;` |
|     6708 |  7663 | `				PH7_MemObjRelease(pTos);` |
|     6708 |  7664 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     6708 |  7665 | `				if( pObjAttr ){` |
|     6706 |  7666 | `					ph7_value *pValue = 0; /* cc warning */` |
|        - |  7667 | `					/* Check attribute access */` |
|     6706 |  7668 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|        - |  7669 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|        - |  7670 | `						 * We can only raise it on a real read, not when the slot is the` |
|        - |  7671 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|        - |  7672 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|        - |  7673 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|     6704 |  7674 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|     3394 |  7675 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       82 |  7676 | `							VmInstr *pNext = pInstr + 1;` |
|       82 |  7677 | `							int bIsLhs = 0;` |
|       82 |  7678 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       80 |  7679 | `								bIsLhs = 1;` |
|       39 |  7680 | `							}` |
|       82 |  7681 | `							if( !bIsLhs ){` |
|        3 |  7682 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|        3 |  7683 | `								PH7_ClassInstanceUnref(pThis);` |
|        3 |  7684 | `								if( rcU == PH7_ABORT ){` |
|      ! 0 |  7685 | `									goto Abort;` |
|        - |  7686 | `								}` |
|        - |  7687 | `								{` |
|        3 |  7688 | `									VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7689 | `									if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7690 | `										pc = pFrm2->iExceptionJump - 1;` |
|     3840 |  7691 | `										break;` |
|        - |  7692 | `									}` |
|        - |  7693 | `								}` |
|      ! 0 |  7694 | `								goto Exception;` |
|        - |  7695 | `							}` |
|       39 |  7696 | `						}` |
|        - |  7697 | `						/* Load attribute */` |
|     6704 |  7698 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     6704 |  7699 | `						if( pValue ){` |
|     6704 |  7700 | `							if( pThis->iRef < 2 ){` |
|        - |  7701 | `								/* Perform a store operation,rather than a load operation since` |
|        - |  7702 | `								 * the class instance '$this' will be deleted shortly.` |
|        - |  7703 | `								 */` |
|        7 |  7704 | `								PH7_MemObjStore(pValue,pTos);` |
|        4 |  7705 | `							}else{` |
|        - |  7706 | `								/* Simple load */` |
|     6698 |  7707 | `								PH7_MemObjLoad(pValue,pTos);` |
|        - |  7708 | `							}` |
|     6704 |  7709 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|     6702 |  7710 | `								if( pThis->iRef > 1 ){` |
|        - |  7711 | `									/* Load attribute index */` |
|     6696 |  7712 | `									pTos->nIdx = pObjAttr->nIdx;` |
|     3347 |  7713 | `								}` |
|     3350 |  7714 | `							}` |
|     3351 |  7715 | `						}` |
|     3353 |  7716 | `					}else{` |
|        - |  7717 | `						/* Throw Error exception (PHP-compatible).` |
|        - |  7718 | `						 * Build message before unref — pObjAttr belongs to pThis->hAttr. */` |
|        - |  7719 | `						char zMsg[256];` |
|      ! 0 |  7720 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  7721 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7722 | `							zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7723 | `							(int)pObjAttr->pAttr->sName.nByte,pObjAttr->pAttr->sName.zString);` |
|      ! 0 |  7724 | `						PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7725 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  7726 | `						goto Abort;` |
|        - |  7727 | `					}` |
|     3351 |  7728 | `				}` |
|        - |  7729 | `				/* Safely unreference the object */` |
|     6706 |  7730 | `				PH7_ClassInstanceUnref(pThis);` |
|        - |  7731 | `			}` |
|     3727 |  7732 | `		}else{` |
|        3 |  7733 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"'->': Expecting class instance as left operand,PH7 is loading NULL");` |
|        3 |  7734 | `			VmPopOperand(&pTos,1);` |
|        3 |  7735 | `			PH7_MemObjRelease(pTos);` |
|        3 |  7736 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|        - |  7737 | `		}` |
|     3728 |  7738 | `	}else{` |
|        - |  7739 | `		/* Static member access using class name */` |
|      228 |  7740 | `		pNos = pTos;` |
|      228 |  7741 | `		pThis = 0;` |
|      228 |  7742 | `		if( !pInstr->p3 ){` |
|      190 |  7743 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      190 |  7744 | `			pNos--;` |
|        - |  7745 | `#ifdef UNTRUST` |
|        - |  7746 | `			if( pNos < pStack ){` |
|        - |  7747 | `				goto Abort;` |
|        - |  7748 | `			}` |
|        - |  7749 | `#endif` |
|       96 |  7750 | `		}else{` |
|        - |  7751 | `			/* Attribute name already computed */` |
|       40 |  7752 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|        - |  7753 | `		}` |
|      228 |  7754 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|      228 |  7755 | `			ph7_class *pClass = 0;` |
|      228 |  7756 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7757 | `				/* Class already instantiated */` |
|        5 |  7758 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|        5 |  7759 | `				pClass = pThis->pClass;` |
|        5 |  7760 | `				pThis->iRef++; /* Deffer garbage collection */` |
|        3 |  7761 | `			}else{` |
|        - |  7762 | `				/* Try to extract the target class */` |
|      224 |  7763 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|      224 |  7764 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|      224 |  7765 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|        - |  7766 | `					/* Handle self/static/parent keywords */` |
|      224 |  7767 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       62 |  7768 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       62 |  7769 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - |  7770 | `							/* In a trait method, self:: resolves to the using class */` |
|       13 |  7771 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|        8 |  7772 | `						}` |
|      194 |  7773 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|       28 |  7774 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      164 |  7775 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|       28 |  7776 | `						ph7_class *pSelf = PH7_VmPeekDeclaringClass(&(*pVm));` |
|       28 |  7777 | `						if( pSelf && pSelf->pBase ){` |
|       28 |  7778 | `							pClass = pSelf->pBase;` |
|       13 |  7779 | `						}` |
|       15 |  7780 | `					}else{` |
|      112 |  7781 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - |  7782 | `					}` |
|      111 |  7783 | `				}` |
|        - |  7784 | `			}` |
|      228 |  7785 | `			if( pClass == 0 ){` |
|        - |  7786 | `				/* Undefined class */` |
|      ! 0 |  7787 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Call to undefined class '%.*s',PH7 is loading NULL",` |
|      ! 0 |  7788 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob)` |
|        - |  7789 | `					);` |
|      ! 0 |  7790 | `				if( !pInstr->p3 ){` |
|      ! 0 |  7791 | `					VmPopOperand(&pTos,1);` |
|      ! 0 |  7792 | `				}` |
|      ! 0 |  7793 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 |  7794 | `				pTos->nIdx = SXU32_HIGH;` |
|      ! 0 |  7795 | `			}else{` |
|      228 |  7796 | `				if( pInstr->iP2 ){` |
|        - |  7797 | `					/* Method call */` |
|       86 |  7798 | `					ph7_class_method *pMeth = 0;` |
|       86 |  7799 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|        - |  7800 | `						/* Extract the target method */` |
|       86 |  7801 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|       42 |  7802 | `					}` |
|       86 |  7803 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      ! 0 |  7804 | `						if( pMeth ){` |
|      ! 0 |  7805 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot call abstract method '%z:%z',PH7 is loading NULL",` |
|      ! 0 |  7806 | `								&pClass->sName,&sName` |
|        - |  7807 | `								);` |
|      ! 0 |  7808 | `						}else{` |
|      ! 0 |  7809 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class static method '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7810 | `								&pClass->sName,&sName` |
|        - |  7811 | `								);` |
|        - |  7812 | `							/* Call the '__CallStatic()' magic method if available */` |
|      ! 0 |  7813 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__callStatic",sizeof("__callStatic")-1,&sName);` |
|        - |  7814 | `						}` |
|        - |  7815 | `						/* Pop the method name from the stack */` |
|      ! 0 |  7816 | `						if( !pInstr->p3 ){` |
|      ! 0 |  7817 | `							VmPopOperand(&pTos,1);` |
|      ! 0 |  7818 | `						}` |
|      ! 0 |  7819 | `						PH7_MemObjRelease(pTos);` |
|      ! 0 |  7820 | `					}else{` |
|        - |  7821 | `						/* Push method name on the stack */` |
|       86 |  7822 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7823 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|       86 |  7824 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|        - |  7825 | `					}` |
|       86 |  7826 | `					pTos->nIdx = SXU32_HIGH;` |
|       44 |  7827 | `				}else{` |
|        - |  7828 | `					/* Attribute access */` |
|      144 |  7829 | `					ph7_class_attr *pAttr = 0;` |
|        - |  7830 | `					/* Check for special ::class pseudo-constant */` |
|      190 |  7831 | `					if( sName.nByte == sizeof("class")-1 &&` |
|       92 |  7832 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|        - |  7833 | `						/* ::class returns the fully qualified class name */` |
|        - |  7834 | `						/* Pop the attribute name from the stack */` |
|       60 |  7835 | `						if( !pInstr->p3 ){` |
|       60 |  7836 | `							VmPopOperand(&pTos,1);` |
|       29 |  7837 | `						}` |
|       60 |  7838 | `						PH7_MemObjRelease(pTos);` |
|        - |  7839 | `						/* Load the class name */` |
|       60 |  7840 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|       60 |  7841 | `						pTos->nIdx = SXU32_HIGH;` |
|       31 |  7842 | `					}else{` |
|        - |  7843 | `						/* Extract the target attribute */` |
|       86 |  7844 | `						if( sName.nByte > 0 ){` |
|       86 |  7845 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|       42 |  7846 | `						}` |
|       86 |  7847 | `						if( pAttr == 0 ){` |
|        - |  7848 | `							/* No such attribute,load null */` |
|      ! 0 |  7849 | `							VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Undefined class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7850 | `								&pClass->sName,&sName);` |
|        - |  7851 | `							/* Call the __get magic method if available */` |
|      ! 0 |  7852 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,0,"__get",sizeof("__get")-1,&sName);` |
|      ! 0 |  7853 | `						}` |
|        - |  7854 | `						/* Pop the attribute name from the stack */` |
|       86 |  7855 | `						if( !pInstr->p3 ){` |
|       48 |  7856 | `							VmPopOperand(&pTos,1);` |
|       23 |  7857 | `						}` |
|       86 |  7858 | `						PH7_MemObjRelease(pTos);` |
|       86 |  7859 | `						pTos->nIdx = SXU32_HIGH;` |
|       86 |  7860 | `						if( pAttr ){` |
|       86 |  7861 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|        - |  7862 | `								/* Access to a non static attribute */` |
|      ! 0 |  7863 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|      ! 0 |  7864 | `									&pClass->sName,&pAttr->sName` |
|        - |  7865 | `									);` |
|      ! 0 |  7866 | `							}else{` |
|        - |  7867 | `								ph7_value *pValue;` |
|        - |  7868 | `								/* Check if the access to the attribute is allowed */` |
|       86 |  7869 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|        - |  7870 | `									/* PHP 7.4+: uninitialized typed static read.` |
|        - |  7871 | `									 * Same LHS-of-store peek as the instance path. */` |
|       80 |  7872 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|       55 |  7873 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       41 |  7874 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|       26 |  7875 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|       28 |  7876 | `										if( pS ){` |
|       28 |  7877 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|       28 |  7878 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|        8 |  7879 | `												VmInstr *pNext = pInstr + 1;` |
|        8 |  7880 | `												int bIsLhs = 0;` |
|        8 |  7881 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|        6 |  7882 | `													bIsLhs = 1;` |
|        2 |  7883 | `												}` |
|        8 |  7884 | `												if( !bIsLhs ){` |
|        3 |  7885 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|        3 |  7886 | `													if( pThis ){` |
|      ! 0 |  7887 | `														PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  7888 | `													}` |
|        3 |  7889 | `													if( rcU == PH7_ABORT ){` |
|      ! 0 |  7890 | `														goto Abort;` |
|        - |  7891 | `													}` |
|        - |  7892 | `													{` |
|        3 |  7893 | `														VmFrame *pFrm2 = pVm->pFrame;` |
|        3 |  7894 | `														if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION) && pFrm2->iExceptionJump > 0 ){` |
|        3 |  7895 | `															pc = pFrm2->iExceptionJump - 1;` |
|        3 |  7896 | `															break;` |
|        - |  7897 | `														}` |
|        - |  7898 | `													}` |
|      ! 0 |  7899 | `													goto Exception;` |
|        - |  7900 | `												}` |
|        2 |  7901 | `											}` |
|       12 |  7902 | `										}` |
|       12 |  7903 | `									}` |
|        - |  7904 | `									/* Load the desired attribute */` |
|       80 |  7905 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|       80 |  7906 | `									if( pValue ){` |
|       80 |  7907 | `										PH7_MemObjLoad(pValue,pTos);` |
|       80 |  7908 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|        - |  7909 | `											/* Load index number */` |
|       38 |  7910 | `											pTos->nIdx = pAttr->nIdx;` |
|       18 |  7911 | `										}` |
|       39 |  7912 | `									}` |
|       41 |  7913 | `								}else{` |
|        - |  7914 | `									/* Throw Error exception (PHP-compatible) */` |
|        - |  7915 | `									char zMsg[256];` |
|        5 |  7916 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|        5 |  7917 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|        7 |  7918 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|        4 |  7919 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|        4 |  7920 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        3 |  7921 | `									}else{` |
|      ! 0 |  7922 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|      ! 0 |  7923 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|      ! 0 |  7924 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|        - |  7925 | `									}` |
|        5 |  7926 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|        5 |  7927 | `									goto Abort;` |
|        - |  7928 | `								}` |
|        - |  7929 | `							}` |
|       39 |  7930 | `						}` |
|        - |  7931 | `					}` |
|        - |  7932 | `				}` |
|      222 |  7933 | `				if( pThis ){` |
|        - |  7934 | `					/* Safely unreference the object */` |
|        5 |  7935 | `					PH7_ClassInstanceUnref(pThis);` |
|        2 |  7936 | `				}` |
|        - |  7937 | `			}` |
|      112 |  7938 | `		}else{` |
|        - |  7939 | `			/* Pop operands */` |
|      ! 0 |  7940 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|      ! 0 |  7941 | `			if( !pInstr->p3 ){` |
|      ! 0 |  7942 | `				VmPopOperand(&pTos,1);` |
|      ! 0 |  7943 | `			}` |
|      ! 0 |  7944 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7945 | `			pTos->nIdx = SXU32_HIGH;` |
|        - |  7946 | `		}` |
|        - |  7947 | `	}` |
|     7674 |  7948 | `	break;` |
|        - |  7949 | `					}` |
|        - |  7950 | `/*` |
|        - |  7951 | ` * OP_NEW P1 * * *` |
|        - |  7952 | ` *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.` |
|        - |  7953 | ` */` |
|      614 |  7954 | `case PH7_OP_NEW: {` |
|     1230 |  7955 | `	ph7_value *pArg = &pTos[-pInstr->iP1]; /* Constructor arguments (if available) */` |
|     1230 |  7956 | `	ph7_class *pClass = 0;` |
|        - |  7957 | `	ph7_class_instance *pNew;` |
|     1230 |  7958 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|        - |  7959 | `		/* Try to extract the desired class */` |
|     1844 |  7960 | `		pClass = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     1228 |  7961 | `			SyBlobLength(&pTos->sBlob),TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|      614 |  7962 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|        - |  7963 | `		/* Take the base class from the loaded instance */` |
|      ! 0 |  7964 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|      ! 0 |  7965 | `	}` |
|     1230 |  7966 | `	if( pClass == 0 ){` |
|        - |  7967 | `		/* No such class — fatal error, stop execution (matches PHP behavior) */` |
|      ! 0 |  7968 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Class '%.*s' is not defined",` |
|      ! 0 |  7969 | `			SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob)` |
|        - |  7970 | `			);` |
|        - |  7971 | `		/* Release the class operand and any constructor arguments, then abort */` |
|      ! 0 |  7972 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  7973 | `		if( pInstr->iP1 > 0 ){` |
|        - |  7974 | `			/* Pop given arguments */` |
|      ! 0 |  7975 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7976 | `		}` |
|      ! 0 |  7977 | `		goto Abort;` |
|      ! 0 |  7978 | `	}else{` |
|        - |  7979 | `		ph7_class_method *pCons;` |
|        - |  7980 | `		/* Create a new class instance */` |
|     1230 |  7981 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|     1230 |  7982 | `		if( pNew == 0 ){` |
|      ! 0 |  7983 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  7984 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|      ! 0 |  7985 | `				&pClass->sName` |
|        - |  7986 | `			);` |
|      ! 0 |  7987 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  7988 | `			if( pInstr->iP1 > 0 ){` |
|        - |  7989 | `				/* Pop given arguments */` |
|      ! 0 |  7990 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  7991 | `			}` |
|      ! 0 |  7992 | `			break;` |
|        - |  7993 | `		}` |
|        - |  7994 | `		/* Check if a constructor is available */` |
|     1230 |  7995 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|     1230 |  7996 | `		if( pCons == 0 ){` |
|      906 |  7997 | `			SyString *pName = &pClass->sName;` |
|        - |  7998 | `			/* Check for a constructor with the same base class name */` |
|      906 |  7999 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|      452 |  8000 | `		}` |
|     1230 |  8001 | `		if( pCons ){` |
|        - |  8002 | `			/* Call the class constructor.  Collect args in stack order and` |
|        - |  8003 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|        - |  8004 | `			 * receiving OP_CALL path runs its named-argument matching` |
|        - |  8005 | `			 * (including variadic string-key packing). */` |
|      326 |  8006 | `			VmCallArgMap *pNewMap = (VmCallArgMap *)pInstr->p3;` |
|      326 |  8007 | `			SySetReset(&aArg);` |
|      640 |  8008 | `			while( pArg < pTos ){` |
|      316 |  8009 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      316 |  8010 | `				pArg++;` |
|        2 |  8011 | `			}` |
|      326 |  8012 | `			if( pVm->bErrReport && !(pNewMap && pNewMap->bHasNamed) ){` |
|        - |  8013 | `				ph7_vm_func_arg *pFuncArg;` |
|        - |  8014 | `				sxu32 n;` |
|       81 |  8015 | `				n = SySetUsed(&aArg);` |
|        - |  8016 | `				/* Emit a notice for missing arguments (positional-only:` |
|        - |  8017 | `				 * for named args the missing-arg check happens downstream` |
|        - |  8018 | `				 * after resolution). */` |
|      149 |  8019 | `				while( n < SySetUsed(&pCons->sFunc.aArgs) ){` |
|       69 |  8020 | `					pFuncArg = (ph7_vm_func_arg *)SySetAt(&pCons->sFunc.aArgs,n);` |
|       69 |  8021 | `					if( pFuncArg ){` |
|       69 |  8022 | `						if( SySetUsed(&pFuncArg->aByteCode) < 1 ){` |
|        7 |  8023 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,"Missing constructor argument %u($%z) for class '%z'",` |
|        2 |  8024 | `								n+1,&pFuncArg->sName,&pClass->sName);` |
|        2 |  8025 | `						}` |
|       34 |  8026 | `					}` |
|       69 |  8027 | `					n++;` |
|        1 |  8028 | `				}` |
|       40 |  8029 | `			}` |
|      326 |  8030 | `			VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|        - |  8031 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|      326 |  8032 | `			if( pNew->iRef < 1 ){` |
|      ! 0 |  8033 | `				pNew->iRef = 1;` |
|      ! 0 |  8034 | `			}` |
|      162 |  8035 | `		}` |
|     1230 |  8036 | `		if( pInstr->iP1 > 0 ){` |
|        - |  8037 | `			/* Pop given arguments */` |
|      262 |  8038 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|      130 |  8039 | `		}` |
|     1230 |  8040 | `		PH7_MemObjRelease(pTos);` |
|     1230 |  8041 | `		pTos->x.pOther = pNew;` |
|     1230 |  8042 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8043 | `	}` |
|     1230 |  8044 | `	break;` |
|        - |  8045 | `				 }` |
|        - |  8046 | `/*` |
|        - |  8047 | ` * OP_CLONE * * *` |
|        - |  8048 | ` * Perfome a clone operation.` |
|        - |  8049 | ` */` |
|       24 |  8050 | `case PH7_OP_CLONE: {` |
|        - |  8051 | `	ph7_class_instance *pSrc,*pClone;` |
|        - |  8052 | `#ifdef UNTRUST` |
|        - |  8053 | `	if( pTos < pStack ){` |
|        - |  8054 | `		goto Abort;` |
|        - |  8055 | `	}` |
|        - |  8056 | `#endif` |
|        - |  8057 | `	/* Make sure we are dealing with a class instance */` |
|       50 |  8058 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|        5 |  8059 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8060 | `			"Clone: Expecting a class instance as left operand,PH7 is loading NULL");` |
|        5 |  8061 | `		PH7_MemObjRelease(pTos);` |
|        5 |  8062 | `		break;` |
|        - |  8063 | `	}` |
|        - |  8064 | `	/* Point to the source */` |
|       46 |  8065 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8066 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|       46 |  8067 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|      ! 0 |  8068 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8069 | `			"Trying to clone an uncloneable object of class '%z'",` |
|      ! 0 |  8070 | `			&pSrc->pClass->sName);` |
|      ! 0 |  8071 | `		PH7_MemObjRelease(pTos);` |
|      ! 0 |  8072 | `		break;` |
|        - |  8073 | `	}` |
|        - |  8074 | `	/* Perform the clone operation */` |
|       46 |  8075 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|       46 |  8076 | `	PH7_MemObjRelease(pTos);` |
|       46 |  8077 | `	if( pClone == 0 ){` |
|      ! 0 |  8078 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - |  8079 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|      ! 0 |  8080 | `	}else{` |
|        - |  8081 | `		/* Load the cloned object */` |
|       46 |  8082 | `		pTos->x.pOther = pClone;` |
|       46 |  8083 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|        - |  8084 | `	}` |
|       46 |  8085 | `	break;` |
|        - |  8086 | `				   }` |
|        - |  8087 | `/*` |
|        - |  8088 | ` * OP_SWITCH * * P3` |
|        - |  8089 | ` *  This is the bytecode implementation of the complex switch() PHP construct.` |
|        - |  8090 | ` */` |
|       26 |  8091 | `case PH7_OP_SWITCH: {` |
|       54 |  8092 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|        - |  8093 | `	ph7_case_expr *aCase,*pCase;` |
|        - |  8094 | `	ph7_value sValue,sCaseValue;` |
|        - |  8095 | `	sxu32 n,nEntry;` |
|        - |  8096 | `#ifdef UNTRUST` |
|        - |  8097 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|        - |  8098 | `		goto Abort;` |
|        - |  8099 | `	}` |
|        - |  8100 | `#endif` |
|        - |  8101 | `	/* Point to the case table  */` |
|       54 |  8102 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|       54 |  8103 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|        - |  8104 | `	/* Select the appropriate case block to execute */` |
|       54 |  8105 | `	PH7_MemObjInit(pVm,&sValue);` |
|       54 |  8106 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|      132 |  8107 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|      130 |  8108 | `		pCase = &aCase[n];` |
|      130 |  8109 | `		PH7_MemObjLoad(pTos,&sValue);` |
|        - |  8110 | `		/* Execute the case expression first */` |
|      130 |  8111 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue);` |
|        - |  8112 | `		/* Compare the two expression */` |
|      130 |  8113 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|      130 |  8114 | `		PH7_MemObjRelease(&sValue);` |
|      130 |  8115 | `		PH7_MemObjRelease(&sCaseValue);` |
|      130 |  8116 | `		if( rc == 0 ){` |
|        - |  8117 | `			/* Value match,jump to this block */` |
|       52 |  8118 | `			pc = pCase->nStart - 1;` |
|       52 |  8119 | `			break;` |
|        - |  8120 | `		}` |
|       41 |  8121 | `	}` |
|       54 |  8122 | `	VmPopOperand(&pTos,1);` |
|       54 |  8123 | `	if( n >= nEntry ){` |
|        - |  8124 | `		/* No approprite case to execute,jump to the default case */` |
|        3 |  8125 | `		if( pSwitch->nDefault > 0 ){` |
|        3 |  8126 | `			pc = pSwitch->nDefault - 1;` |
|        2 |  8127 | `		}else{` |
|        - |  8128 | `			/* No default case,jump out of this switch */` |
|      ! 0 |  8129 | `			pc = pSwitch->nOut - 1;` |
|        - |  8130 | `		}` |
|        1 |  8131 | `	}` |
|       54 |  8132 | `	break;` |
|        - |  8133 | `					}` |
|        - |  8134 | `/*` |
|        - |  8135 | ` * OP_MATCH * * P3` |
|        - |  8136 | ` *  PHP 8.0 match expression. P3 points to a ph7_match struct holding` |
|        - |  8137 | ` *  the compiled arms. On entry, the subject is on top of the stack.` |
|        - |  8138 | ` *  On exit, the stack slot holds the matched arm's result value.` |
|        - |  8139 | ` *  Comparison is strict (===). No fallthrough. When no arm matches and` |
|        - |  8140 | ` *  no default is present, a fatal UnhandledMatchError is raised.` |
|        - |  8141 | ` */` |
|       54 |  8142 | `case PH7_OP_MATCH: {` |
|      110 |  8143 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|      110 |  8144 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|        - |  8145 | `	ph7_value sSubject,sCond,sResult;` |
|        - |  8146 | `	sxu32 i,j,nArm,nCond;` |
|      110 |  8147 | `	int matched = 0;` |
|        - |  8148 | `#ifdef UNTRUST` |
|        - |  8149 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|        - |  8150 | `		goto Abort;` |
|        - |  8151 | `	}` |
|        - |  8152 | `#endif` |
|      110 |  8153 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|      110 |  8154 | `	nArm = SySetUsed(&pMatch->aArms);` |
|      110 |  8155 | `	PH7_MemObjInit(pVm,&sSubject);` |
|      110 |  8156 | `	PH7_MemObjInit(pVm,&sCond);` |
|      110 |  8157 | `	PH7_MemObjInit(pVm,&sResult);` |
|      110 |  8158 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|      348 |  8159 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|      240 |  8160 | `		pArm = &aArm[i];` |
|      240 |  8161 | `		if( pArm->bDefault ){` |
|       13 |  8162 | `			pDefault = pArm;` |
|       13 |  8163 | `			continue;` |
|        - |  8164 | `		}` |
|      228 |  8165 | `		nCond = SySetUsed(&pArm->aConds);` |
|      394 |  8166 | `		for( j = 0; j < nCond; ++j ){` |
|      260 |  8167 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|      260 |  8168 | `			if( pCondBc == 0 ){` |
|      ! 0 |  8169 | `				continue;` |
|        - |  8170 | `			}` |
|      260 |  8171 | `			VmLocalExec(pVm,pCondBc,&sCond);` |
|      260 |  8172 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|      260 |  8173 | `			PH7_MemObjRelease(&sCond);` |
|      260 |  8174 | `			if( rc == 0 ){` |
|       93 |  8175 | `				VmLocalExec(pVm,&pArm->aResult,&sResult);` |
|       93 |  8176 | `				matched = 1;` |
|       93 |  8177 | `				break;` |
|        - |  8178 | `			}` |
|       85 |  8179 | `		}` |
|      115 |  8180 | `	}` |
|      110 |  8181 | `	if( !matched && pDefault ){` |
|       13 |  8182 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult);` |
|       13 |  8183 | `		matched = 1;` |
|        6 |  8184 | `	}` |
|      110 |  8185 | `	if( !matched ){` |
|        5 |  8186 | `		const char *zType = "unknown";` |
|        - |  8187 | `		char zMsg[128];` |
|        - |  8188 | `		sxu32 nMsg;` |
|        5 |  8189 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|      ! 0 |  8190 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|      ! 0 |  8191 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|        5 |  8192 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|      ! 0 |  8193 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|      ! 0 |  8194 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|      ! 0 |  8195 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|      ! 0 |  8196 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|      ! 0 |  8197 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|      ! 0 |  8198 | `		default: break;` |
|        - |  8199 | `		}` |
|        7 |  8200 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        2 |  8201 | `			"Unhandled match case of type %s",zType);` |
|        7 |  8202 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|        2 |  8203 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|        5 |  8204 | `		PH7_MemObjRelease(&sSubject);` |
|        5 |  8205 | `		PH7_MemObjRelease(&sResult);` |
|        5 |  8206 | `		goto Abort;` |
|        - |  8207 | `	}` |
|      105 |  8208 | `	PH7_MemObjRelease(&sSubject);` |
|        - |  8209 | `	/* Replace subject on TOS with the arm result */` |
|      105 |  8210 | `	PH7_MemObjStore(&sResult,pTos);` |
|      105 |  8211 | `	PH7_MemObjRelease(&sResult);` |
|      105 |  8212 | `	break;` |
|        - |  8213 | `					}` |
|        - |  8214 | `/*` |
|        - |  8215 | ` * OP_YIELD P1 P2 *` |
|        - |  8216 | ` *  Yield a value from a generator function.` |
|        - |  8217 | ` *  P1=1 if value on stack, P1=0 for bare yield.` |
|        - |  8218 | ` *  P2=1 if key=>value syntax (key below value on stack).` |
|        - |  8219 | ` */` |
|       34 |  8220 | `case PH7_OP_YIELD: {` |
|        - |  8221 | `	ph7_generator *pGen;` |
|       70 |  8222 | `	if( pVm->pActiveCtx == 0 \|\| pVm->pActiveCtx->pPrivate == 0 ){` |
|      ! 0 |  8223 | `		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");` |
|      ! 0 |  8224 | `		goto Abort;` |
|        - |  8225 | `	}` |
|       70 |  8226 | `	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;` |
|       70 |  8227 | `	if( pInstr->iP2 ){` |
|        - |  8228 | `		/* yield $key => $value: value on top, key below */` |
|        - |  8229 | `#ifdef UNTRUST` |
|        - |  8230 | `		if( pTos < &pStack[1] ) goto Abort;` |
|        - |  8231 | `#endif` |
|        7 |  8232 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|        7 |  8233 | `		VmPopOperand(&pTos, 1);` |
|        7 |  8234 | `		PH7_MemObjStore(pTos, &pGen->sYieldKey);` |
|        7 |  8235 | `		VmPopOperand(&pTos, 1);` |
|        - |  8236 | `		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */` |
|        7 |  8237 | `		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){` |
|      ! 0 |  8238 | `			sxi64 nKey = pGen->sYieldKey.x.iVal;` |
|      ! 0 |  8239 | `			if( nKey >= pGen->iImplicitKey ){` |
|      ! 0 |  8240 | `				pGen->iImplicitKey = nKey + 1;` |
|      ! 0 |  8241 | `			}` |
|        1 |  8242 | `		}` |
|       67 |  8243 | `	}else if( pInstr->iP1 ){` |
|        - |  8244 | `		/* yield $value */` |
|        - |  8245 | `#ifdef UNTRUST` |
|        - |  8246 | `		if( pTos < pStack ) goto Abort;` |
|        - |  8247 | `#endif` |
|       64 |  8248 | `		PH7_MemObjStore(pTos, &pGen->sYieldValue);` |
|       64 |  8249 | `		VmPopOperand(&pTos, 1);` |
|        - |  8250 | `		/* Auto-increment key */` |
|       64 |  8251 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|       64 |  8252 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|       64 |  8253 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|       33 |  8254 | `	}else{` |
|        - |  8255 | `		/* Bare yield — null value, auto-increment key */` |
|      ! 0 |  8256 | `		PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 |  8257 | `		PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 |  8258 | `		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;` |
|      ! 0 |  8259 | `		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);` |
|        - |  8260 | `	}` |
|        - |  8261 | `	/* Suspend execution — resume will push the send() value as the yield result */` |
|       70 |  8262 | `	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));` |
|       70 |  8263 | `	goto Suspend;` |
|        - |  8264 |  |
|        - |  8265 | `/*` |
|        - |  8266 | ` * OP_CALL P1 * *` |
|        - |  8267 | ` *  Call a PHP or a foreign function and push the return value of the called` |
|        - |  8268 | ` *  function on the stack.` |
|        - |  8269 | ` */` |
|   351019 |  8270 | `case PH7_OP_CALL: {` |
|   702084 |  8271 | `	sxi32 nCallArgs = pInstr->iP1 + pVm->iSpreadExtra;` |
|        - |  8272 | `	ph7_value *pArg;` |
|   702084 |  8273 | `	pVm->iSpreadExtra = 0; /* Always reset, even if zero */` |
|   702084 |  8274 | `	pArg = &pTos[-nCallArgs];` |
|        - |  8275 | `	SyHashEntry *pEntry;` |
|        - |  8276 | `	SyString sName;` |
|        - |  8277 | `	/* Extract function name */` |
|   702084 |  8278 | `	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       78 |  8279 | `		if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|        - |  8280 | `			ph7_value sResult;` |
|      ! 0 |  8281 | `			SySetReset(&aArg);` |
|      ! 0 |  8282 | `			while( pArg < pTos ){` |
|      ! 0 |  8283 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      ! 0 |  8284 | `				pArg++;` |
|      ! 0 |  8285 | `			}` |
|      ! 0 |  8286 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - |  8287 | `			/* May be a class instance and it's static method */` |
|      ! 0 |  8288 | `			PH7_VmCallUserFunction(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult);` |
|      ! 0 |  8289 | `			SySetReset(&aArg);` |
|        - |  8290 | `			/* Pop given arguments */` |
|      ! 0 |  8291 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8292 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8293 | `			}` |
|        - |  8294 | `			/* Copy result */` |
|      ! 0 |  8295 | `			PH7_MemObjStore(&sResult,pTos);` |
|      ! 0 |  8296 | `			PH7_MemObjRelease(&sResult);` |
|       78 |  8297 | `		}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       78 |  8298 | `			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|        - |  8299 | `			ph7_value sResult;` |
|        - |  8300 | `			sxi32 rcInv;` |
|       78 |  8301 | `			SySetReset(&aArg);` |
|      192 |  8302 | `			while( pArg < pTos ){` |
|      116 |  8303 | `				SySetPut(&aArg,(const void *)&pArg);` |
|      116 |  8304 | `				pArg++;` |
|        2 |  8305 | `			}` |
|       78 |  8306 | `			PH7_MemObjInit(pVm,&sResult);` |
|      116 |  8307 | `			rcInv = VmCallObjectInvoke(&(*pVm),pThis,` |
|       76 |  8308 | `				(int)SySetUsed(&aArg),` |
|       76 |  8309 | `				(ph7_value **)SySetBasePtr(&aArg),` |
|        - |  8310 | `				&sResult,` |
|       76 |  8311 | `				(VmCallArgMap *)pInstr->p3);` |
|       78 |  8312 | `			SySetReset(&aArg);` |
|       78 |  8313 | `			if( nCallArgs > 0 ){` |
|       74 |  8314 | `				VmPopOperand(&pTos,nCallArgs);` |
|       36 |  8315 | `			}` |
|       78 |  8316 | `			if( rcInv == SXERR_INVALID ){` |
|        - |  8317 | `				/* No __invoke: raise a catchable Error and route through try/catch.` |
|        - |  8318 | `				 * sResult was already released by VmCallObjectInvoke.` |
|        - |  8319 | `				 * Pin pThis: the release below would otherwise drop the last ref` |
|        - |  8320 | `				 * to a temporary callable like (new Plain())(...), freeing the` |
|        - |  8321 | `				 * instance before VmRaiseNotCallable reads its class name. */` |
|       13 |  8322 | `				pThis->iRef++;` |
|       13 |  8323 | `				PH7_MemObjRelease(pTos);` |
|       13 |  8324 | `				rc = VmRaiseNotCallable(&(*pVm),pThis);` |
|       13 |  8325 | `				PH7_ClassInstanceUnref(pThis);` |
|       13 |  8326 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8327 | `					goto Abort;` |
|        - |  8328 | `				}` |
|        - |  8329 | `				{` |
|       13 |  8330 | `					VmFrame *pFrm2 = pVm->pFrame;` |
|       12 |  8331 | `					if( pFrm2 && (pFrm2->iFlags & VM_FRAME_EXCEPTION)` |
|       13 |  8332 | `					 && pFrm2->iExceptionJump > 0 ){` |
|       13 |  8333 | `						pc = pFrm2->iExceptionJump - 1;` |
|       13 |  8334 | `						break;` |
|        - |  8335 | `					}` |
|        - |  8336 | `				}` |
|      ! 0 |  8337 | `				goto Exception;` |
|        - |  8338 | `			}` |
|       66 |  8339 | `			PH7_MemObjStore(&sResult,pTos);` |
|       66 |  8340 | `			PH7_MemObjRelease(&sResult);` |
|       34 |  8341 | `		}else{` |
|        - |  8342 | `			/* Raise exception: Invalid function name */` |
|      ! 0 |  8343 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Invalid function name,NULL will be returned");` |
|        - |  8344 | `			/* Pop given arguments */` |
|      ! 0 |  8345 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8346 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8347 | `			}` |
|        - |  8348 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8349 | `			PH7_MemObjRelease(pTos);` |
|        - |  8350 | `		}` |
|       66 |  8351 | `		break;` |
|        - |  8352 | `	}` |
|   702008 |  8353 | `	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|        - |  8354 | `	/* Check for a compiled function first.` |
|        - |  8355 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - |  8356 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|   702008 |  8357 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  8358 | `	/* If the compiler qualified this call with a namespace, and the namespaced` |
|        - |  8359 | `	 * function is not found, retry with the global name (strip the namespace` |
|        - |  8360 | `	 * prefix up to the last backslash) before falling back to host functions.` |
|        - |  8361 | `	 * This mirrors PHP's lookup order for unqualified function calls inside` |
|        - |  8362 | `	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */` |
|        - |  8363 | `	{` |
|   702008 |  8364 | `	VmCallArgMap *pCallMap = (VmCallArgMap *)pInstr->p3;` |
|   702008 |  8365 | `	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){` |
|        - |  8366 | `		const char *zFunc;` |
|        - |  8367 | `		const char *zEnd;` |
|        - |  8368 | `		const char *z;` |
|        - |  8369 | `		SyString sGlobal;` |
|       22 |  8370 | `		zFunc = sName.zString;` |
|       22 |  8371 | `		zEnd  = zFunc + sName.nByte;` |
|       22 |  8372 | `		z = zEnd;` |
|        - |  8373 | `		/* Find last namespace separator */` |
|      194 |  8374 | `		while( z > zFunc ){` |
|      194 |  8375 | `			if( z[-1] == '\\' ){` |
|       22 |  8376 | `				break;` |
|        - |  8377 | `			}` |
|      174 |  8378 | `			z--;` |
|        2 |  8379 | `		}` |
|       22 |  8380 | `		if( z > zFunc && z < zEnd ){` |
|        - |  8381 | `			/* Retry lookup using the unqualified/global function name */` |
|       22 |  8382 | `			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));` |
|       22 |  8383 | `			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);` |
|       10 |  8384 | `		}` |
|       10 |  8385 | `	}` |
|        - |  8386 | `	} /* end VmCallArgMap namespace scope */` |
|   702008 |  8387 | `	if( pEntry ){` |
|        - |  8388 | `		ph7_vm_func_arg *aFormalArg;` |
|        - |  8389 | `		ph7_class_instance *pThis;` |
|        - |  8390 | `		ph7_value *pFrameStack;` |
|        - |  8391 | `		ph7_vm_func *pVmFunc;` |
|        - |  8392 | `		ph7_class *pSelf;` |
|        - |  8393 | `		VmFrame *pFrame;` |
|        - |  8394 | `		ph7_value *pObj;` |
|        - |  8395 | `		VmSlot sArg;` |
|        - |  8396 | `		sxu32 n;` |
|        - |  8397 | `		/* initialize fields */` |
|    18024 |  8398 | `		pVmFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    18024 |  8399 | `		pThis = 0;` |
|    18024 |  8400 | `		pSelf = 0;` |
|    18024 |  8401 | `		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|        - |  8402 | `			ph7_class_method *pMeth;` |
|        - |  8403 | `			/* Class method call */` |
|     3212 |  8404 | `			ph7_value *pTarget = &pTos[-1];` |
|     3212 |  8405 | `			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_NULL)) ){` |
|        - |  8406 | `				/* Extract the 'this' pointer */` |
|     3212 |  8407 | `				if(pTarget->iFlags & MEMOBJ_OBJ ){` |
|        - |  8408 | `					/* Instance already loaded */` |
|     3122 |  8409 | `					pThis = (ph7_class_instance *)pTarget->x.pOther;` |
|     3122 |  8410 | `					pThis->iRef++;` |
|     3122 |  8411 | `					pSelf = pThis->pClass;` |
|     1560 |  8412 | `				}` |
|     3212 |  8413 | `				if( pSelf == 0 ){` |
|       92 |  8414 | `					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){` |
|        - |  8415 | `						/* "Late Static Binding" class name */` |
|      128 |  8416 | `						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|       42 |  8417 | `							SyBlobLength(&pTarget->sBlob),FALSE,0);` |
|       42 |  8418 | `					}` |
|       92 |  8419 | `					if( pSelf == 0 ){` |
|       21 |  8420 | `						pSelf = (ph7_class *)pVmFunc->pUserData;` |
|       10 |  8421 | `					}` |
|       45 |  8422 | `				}` |
|     3212 |  8423 | `				if( pThis == 0  ){` |
|       92 |  8424 | `					VmFrame *pFrameLocal = pVm->pFrame;` |
|       92 |  8425 | `					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       92 |  8426 | `					if( pFrameLocal->pParent ){` |
|        - |  8427 | `						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */` |
|       66 |  8428 | `						pThis = pFrameLocal->pThis;` |
|       66 |  8429 | `						if( pThis ){` |
|       21 |  8430 | `							pThis->iRef++;` |
|       10 |  8431 | `						}` |
|       32 |  8432 | `					}` |
|       45 |  8433 | `				}` |
|     3212 |  8434 | `				VmPopOperand(&pTos,1);` |
|     3212 |  8435 | `				PH7_MemObjRelease(pTos);` |
|        - |  8436 | `				/* Synchronize pointers */` |
|     3212 |  8437 | `				pArg = &pTos[-nCallArgs];` |
|        - |  8438 | `				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'` |
|        - |  8439 | `				 * user have already computed the random generated unique class method name` |
|        - |  8440 | `				 * and tries to call it outside it's context [i.e: global scope]. In that` |
|        - |  8441 | `				 * case we have to synchronize pointers to avoid stack underflow.` |
|        - |  8442 | `				 */` |
|     3212 |  8443 | `				while( pArg < pStack ){` |
|      ! 0 |  8444 | `					pArg++;` |
|      ! 0 |  8445 | `				}` |
|     3212 |  8446 | `				if( pSelf ){ /* Paranoid edition */` |
|        - |  8447 | `					/* Check if the call is allowed */` |
|     3212 |  8448 | `					pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);` |
|     3212 |  8449 | `					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       14 |  8450 | `						if( !PH7_VmClassMemberAccess(&(*pVm),pSelf,&pVmFunc->sName,pMeth->iProtection,FALSE) ){` |
|        - |  8451 | `							/* Throw Error exception (PHP-compatible) */` |
|        - |  8452 | `							char zMsg[256];` |
|      ! 0 |  8453 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      ! 0 |  8454 | `							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",` |
|      ! 0 |  8455 | `								zVis,(int)pSelf->sName.nByte,pSelf->sName.zString,` |
|      ! 0 |  8456 | `								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);` |
|        - |  8457 | `							/* Pop given arguments */` |
|      ! 0 |  8458 | `							if( nCallArgs > 0 ){` |
|      ! 0 |  8459 | `								VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8460 | `							}` |
|      ! 0 |  8461 | `							VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|      ! 0 |  8462 | `							goto Abort;` |
|        - |  8463 | `						}` |
|        6 |  8464 | `					}` |
|     1605 |  8465 | `				}` |
|     1605 |  8466 | `			}` |
|     1605 |  8467 | `		}` |
|        - |  8468 | `		/* Check The recursion limit */` |
|    18024 |  8469 | `		if( pVm->nRecursionDepth > pVm->nMaxDepth ){` |
|        4 |  8470 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8471 | `				"Recursion limit reached while invoking user function '%z',PH7 will set a NULL return value",` |
|        1 |  8472 | `				&pVmFunc->sName);` |
|        - |  8473 | `			/* Pop given arguments */` |
|        3 |  8474 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8475 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8476 | `			}` |
|        - |  8477 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        3 |  8478 | `			PH7_MemObjRelease(pTos);` |
|       14 |  8479 | `			break;` |
|        - |  8480 | `		}` |
|    18022 |  8481 | `		if( pVmFunc->pNextName ){` |
|        - |  8482 | `			/* Function is candidate for overloading,select the appropriate function to call */` |
|      140 |  8483 | `			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));` |
|       69 |  8484 | `		}` |
|    18022 |  8485 | `		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){` |
|        - |  8486 | `			/* Generator function: return a Generator object instead of executing */` |
|        - |  8487 | `			ph7_exec_ctx *pExecCtx;` |
|        - |  8488 | `			ph7_generator *pGenerator;` |
|        - |  8489 | `			ph7_class_instance *pGenObj;` |
|        - |  8490 | `			ph7_value *pCtxAttr;` |
|        - |  8491 | `			SyString sAttrName;` |
|        - |  8492 | `			ph7_value **apCallArgs;` |
|        - |  8493 | `			int nGenArgs, iArg;` |
|        - |  8494 | `			/* Collect arguments from the operand stack */` |
|       24 |  8495 | `			nGenArgs = (int)(pTos - pArg);` |
|       24 |  8496 | `			apCallArgs = 0;` |
|       24 |  8497 | `			if( nGenArgs > 0 ){` |
|       14 |  8498 | `				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8499 | `					nGenArgs * sizeof(ph7_value *));` |
|       10 |  8500 | `				if( apCallArgs == 0 ){` |
|        - |  8501 | `					/* OOM: fall back to zero args rather than NULL-deref */` |
|      ! 0 |  8502 | `					nGenArgs = 0;` |
|      ! 0 |  8503 | `				}else{` |
|       10 |  8504 | `					VmCallArgMap *pGenMap = (VmCallArgMap *)pInstr->p3;` |
|       10 |  8505 | `					int didReorder = 0;` |
|       10 |  8506 | `					if( pGenMap && pGenMap->bHasNamed ){` |
|        - |  8507 | `						/* Named-argument reordering for generator */` |
|        5 |  8508 | `						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        5 |  8509 | `						sxu32 nF = SySetUsed(&pVmFunc->aArgs);` |
|        5 |  8510 | `						sxu32 nNV = nF;` |
|        5 |  8511 | `						sxi32 iVIdx = -1;` |
|        - |  8512 | `						sxi32 *aGSlot;` |
|        - |  8513 | `						sxu8 *aGUsed;` |
|        - |  8514 | `						sxu32 gi;` |
|       13 |  8515 | `						for( gi = 0; gi < nF; gi++ ){` |
|        9 |  8516 | `							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }` |
|        5 |  8517 | `						}` |
|        7 |  8518 | `						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|        4 |  8519 | `							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));` |
|        5 |  8520 | `						if( aGSlot ){` |
|        5 |  8521 | `							aGUsed = (sxu8 *)&aGSlot[nGenArgs];` |
|        7 |  8522 | `							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,` |
|        2 |  8523 | `								(sxu32)nGenArgs,aGSlot,aGUsed);` |
|        5 |  8524 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  8525 | `								SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|      ! 0 |  8526 | `								SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8527 | `								goto Abort;` |
|        - |  8528 | `							}` |
|        - |  8529 | `							/* Build apCallArgs in formal-parameter order, then` |
|        - |  8530 | `							 * append overflow (variadic / positional beyond` |
|        - |  8531 | `							 * formals) so downstream sees every argument. */` |
|        - |  8532 | `							{` |
|        5 |  8533 | `								int nOut = 0;` |
|       13 |  8534 | `								for( gi = 0; gi < nNV; gi++ ){` |
|        - |  8535 | `									sxu32 gj;` |
|       13 |  8536 | `									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){` |
|       13 |  8537 | `										if( aGSlot[gj] == (sxi32)gi ){` |
|        9 |  8538 | `											apCallArgs[nOut++] = &pArg[gj];` |
|        9 |  8539 | `											break;` |
|        - |  8540 | `										}` |
|        3 |  8541 | `									}` |
|        5 |  8542 | `								}` |
|       13 |  8543 | `								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){` |
|        9 |  8544 | `									if( aGSlot[gi] == -1 \|\| aGSlot[gi] == -2 ){` |
|      ! 0 |  8545 | `										apCallArgs[nOut++] = &pArg[gi];` |
|      ! 0 |  8546 | `									}` |
|        5 |  8547 | `								}` |
|        5 |  8548 | `								nGenArgs = nOut;` |
|        - |  8549 | `							}` |
|        5 |  8550 | `							SyMemBackendFree(&pVm->sAllocator, aGSlot);` |
|        5 |  8551 | `							didReorder = 1;` |
|        2 |  8552 | `						}` |
|        - |  8553 | `						/* If aGSlot allocation failed, fall through to` |
|        - |  8554 | `						 * positional fill below — preserves arg order rather` |
|        - |  8555 | `						 * than passing an uninitialized apCallArgs. */` |
|        2 |  8556 | `					}` |
|       10 |  8557 | `					if( !didReorder ){` |
|       12 |  8558 | `						for( iArg = 0; iArg < nGenArgs; iArg++ ){` |
|        8 |  8559 | `							apCallArgs[iArg] = &pArg[iArg];` |
|        5 |  8560 | `						}` |
|        2 |  8561 | `					}` |
|        - |  8562 | `				}` |
|        4 |  8563 | `			}` |
|        - |  8564 | `			/* Create execution context and generator wrapper */` |
|       24 |  8565 | `			pExecCtx = VmNewExecCtx(pVm, pVmFunc);` |
|       24 |  8566 | `			if( pExecCtx == 0 ){` |
|      ! 0 |  8567 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8568 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8569 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8570 | `				break;` |
|        - |  8571 | `			}` |
|       24 |  8572 | `			pGenerator = VmNewGenerator(pVm, pExecCtx);` |
|       24 |  8573 | `			if( pGenerator == 0 ){` |
|      ! 0 |  8574 | `				VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 |  8575 | `				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|      ! 0 |  8576 | `				VmErrorFormat(&(*pVm), PH7_CTX_ERR,` |
|      ! 0 |  8577 | `					"Out of memory while creating generator for '%z'", &pVmFunc->sName);` |
|      ! 0 |  8578 | `				break;` |
|        - |  8579 | `			}` |
|        - |  8580 | `			/* Set up the frame with arguments, closure env, $this */` |
|       24 |  8581 | `			pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       24 |  8582 | `			pVm->pFrame = pExecCtx->pFrame;` |
|       24 |  8583 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs);` |
|       24 |  8584 | `			pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       24 |  8585 | `			pExecCtx->pFrame->pParent = 0;` |
|       24 |  8586 | `			if( apCallArgs ){` |
|       10 |  8587 | `				SyMemBackendFree(&pVm->sAllocator, apCallArgs);` |
|        4 |  8588 | `			}` |
|       24 |  8589 | `			if( rc != SXRET_OK ){` |
|      ! 0 |  8590 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8591 | `				if( pThis ){` |
|      ! 0 |  8592 | `					PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8593 | `				}` |
|      ! 0 |  8594 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 |  8595 | `					goto Abort;` |
|        - |  8596 | `				}` |
|      ! 0 |  8597 | `				break;` |
|        - |  8598 | `			}` |
|        - |  8599 | `			/* Create Generator class instance */` |
|       24 |  8600 | `			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);` |
|       24 |  8601 | `			if( pGenObj == 0 ){` |
|      ! 0 |  8602 | `				VmReleaseGenerator(pVm, pGenerator);` |
|      ! 0 |  8603 | `				break;` |
|        - |  8604 | `			}` |
|        - |  8605 | `			/* Store generator in __ctx attribute */` |
|       24 |  8606 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       24 |  8607 | `			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);` |
|       24 |  8608 | `			if( pCtxAttr ){` |
|       24 |  8609 | `				pCtxAttr->x.pOther = pGenerator;` |
|       24 |  8610 | `				MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       11 |  8611 | `			}` |
|        - |  8612 | `			/* Pop args and function name, push Generator object */` |
|       24 |  8613 | `			PH7_MemObjRelease(pTos);` |
|       24 |  8614 | `			pTos = &pTos[-nCallArgs];` |
|       24 |  8615 | `			pTos->x.pOther = pGenObj;` |
|       24 |  8616 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|       24 |  8617 | `			pGenObj->iRef++;` |
|       24 |  8618 | `			if( pThis ){` |
|      ! 0 |  8619 | `				PH7_ClassInstanceUnref(pThis);` |
|      ! 0 |  8620 | `			}` |
|       24 |  8621 | `			break;` |
|        - |  8622 | `		}` |
|        - |  8623 | `		/* Extract the formal argument set */` |
|    18000 |  8624 | `		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);` |
|        - |  8625 | `		/* Create a new VM frame  */` |
|    18000 |  8626 | `		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);` |
|    18000 |  8627 | `		if( rc != SXRET_OK ){` |
|        - |  8628 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  8629 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8630 | `				"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  8631 | `				&pVmFunc->sName);` |
|        - |  8632 | `			/* Pop given arguments */` |
|      ! 0 |  8633 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  8634 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  8635 | `			}` |
|        - |  8636 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|      ! 0 |  8637 | `			PH7_MemObjRelease(pTos);` |
|      ! 0 |  8638 | `			break;` |
|        - |  8639 | `		}` |
|    18000 |  8640 | `		if( (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) && pThis ){` |
|        - |  8641 | `			/* Install the '$this' variable */` |
|        - |  8642 | `			static const SyString sThis = { "this" , sizeof("this") - 1 };` |
|     3140 |  8643 | `			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);` |
|     3140 |  8644 | `			if( pObj ){` |
|        - |  8645 | `				/* Reflect the change */` |
|     3140 |  8646 | `				pObj->x.pOther = pThis;` |
|     3140 |  8647 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     1569 |  8648 | `			}` |
|     1569 |  8649 | `		}` |
|    18000 |  8650 | `		if( SySetUsed(&pVmFunc->aStatic) > 0 ){` |
|        - |  8651 | `			ph7_vm_func_static_var *pStatic,*aStatic;` |
|        - |  8652 | `			/* Install static variables */` |
|      ! 0 |  8653 | `			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);` |
|      ! 0 |  8654 | `			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){` |
|      ! 0 |  8655 | `				pStatic = &aStatic[n];` |
|      ! 0 |  8656 | `				if( pStatic->nIdx == SXU32_HIGH ){` |
|        - |  8657 | `					/* Initialize the static variables */` |
|      ! 0 |  8658 | `					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);` |
|      ! 0 |  8659 | `					if( pObj ){` |
|        - |  8660 | `						/* Assume a NULL initialization value */` |
|      ! 0 |  8661 | `						PH7_MemObjInit(&(*pVm),pObj);` |
|      ! 0 |  8662 | `						if( SySetUsed(&pStatic->aByteCode) > 0 ){` |
|        - |  8663 | `							/* Evaluate initialization expression (Any complex expression) */` |
|      ! 0 |  8664 | `							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj);` |
|      ! 0 |  8665 | `						}` |
|      ! 0 |  8666 | `						pObj->nIdx = pStatic->nIdx;` |
|      ! 0 |  8667 | `					}else{` |
|      ! 0 |  8668 | `						continue;` |
|        - |  8669 | `					}` |
|      ! 0 |  8670 | `				}` |
|        - |  8671 | `				/* Install in the current frame */` |
|      ! 0 |  8672 | `				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),` |
|      ! 0 |  8673 | `					SX_INT_TO_PTR(pStatic->nIdx));` |
|      ! 0 |  8674 | `			}` |
|      ! 0 |  8675 | `		}` |
|        - |  8676 | `		/* Push arguments in the local frame */` |
|        - |  8677 | `		{` |
|    18000 |  8678 | `		VmCallArgMap *pCallMap3 = (VmCallArgMap *)pInstr->p3;` |
|        - |  8679 | `		/* Caller file's strict_types mode — governs parameter coercion` |
|        - |  8680 | `		 * (but NOT return coercion, which uses the callee's file). */` |
|    18000 |  8681 | `		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;` |
|    18000 |  8682 | `		if( pCallMap3 && pCallMap3->bHasNamed ){` |
|        - |  8683 | `			/* ============================================================` |
|        - |  8684 | `			 * Named-argument matching path (PHP 8.0)` |
|        - |  8685 | `			 *` |
|        - |  8686 | `			 * Resolve each actual argument to its formal parameter by name` |
|        - |  8687 | `			 * or position, then install them in the frame.` |
|        - |  8688 | `			 * ============================================================ */` |
|       96 |  8689 | `			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);` |
|       96 |  8690 | `			sxu32 nActual = (sxu32)(pTos - pArg);` |
|       96 |  8691 | `			sxi32 iVariadicIdx = -1;` |
|        - |  8692 | `			sxu32 nNonVariadic;` |
|        - |  8693 | `			sxi32 *aSlot;` |
|        - |  8694 | `			sxu8  *aUsed;` |
|        - |  8695 | `			sxu32 i;` |
|        - |  8696 | `			/* Find variadic parameter index */` |
|      292 |  8697 | `			for( i = 0; i < nFormal; i++ ){` |
|      206 |  8698 | `				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        9 |  8699 | `					iVariadicIdx = (sxi32)i;` |
|        9 |  8700 | `					break;` |
|        - |  8701 | `				}` |
|      100 |  8702 | `			}` |
|       96 |  8703 | `			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;` |
|        - |  8704 | `			/* Allocate mapping arrays */` |
|      143 |  8705 | `			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       94 |  8706 | `				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));` |
|       96 |  8707 | `			if( aSlot == 0 ){` |
|      ! 0 |  8708 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");` |
|      ! 0 |  8709 | `				goto Abort;` |
|        - |  8710 | `			}` |
|       96 |  8711 | `			aUsed = (sxu8 *)&aSlot[nActual];` |
|        - |  8712 | `			/* Resolve named arguments to formal parameters */` |
|      143 |  8713 | `			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,` |
|       47 |  8714 | `				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);` |
|       96 |  8715 | `			if( rc == PH7_ABORT ){` |
|        7 |  8716 | `				SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        7 |  8717 | `				goto Abort;` |
|        - |  8718 | `			}` |
|        - |  8719 | `			/* Pass 2: install arguments into the frame by formal parameter order */` |
|      275 |  8720 | `			for( n = 0; n < nNonVariadic; n++ ){` |
|        - |  8721 | `				/* Find the stack arg mapped to formal n */` |
|      187 |  8722 | `				sxi32 iSrc = -1;` |
|      309 |  8723 | `				for( i = 0; i < nActual; i++ ){` |
|      291 |  8724 | `					if( aSlot[i] == (sxi32)n ){` |
|      169 |  8725 | `						iSrc = (sxi32)i;` |
|      169 |  8726 | `						break;` |
|        - |  8727 | `					}` |
|       62 |  8728 | `				}` |
|      187 |  8729 | `				if( iSrc >= 0 ){` |
|        - |  8730 | `					/* Argument was provided — install with type checking */` |
|      169 |  8731 | `					ph7_value *pVal = &pArg[iSrc];` |
|        - |  8732 | `					/* NULL-to-default redirect (existing behavior) */` |
|      168 |  8733 | `					if( (pVal->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|        5 |  8734 | `						&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|      ! 0 |  8735 | `						rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pVal);` |
|      ! 0 |  8736 | `						if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8737 | `					}` |
|        - |  8738 | `					/* Type checking: union types */` |
|      169 |  8739 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       19 |  8740 | `						sxi32 rcU = VmCoerceToUnion(pVm, pVal, &aFormalArg[n].aUnionAlts,` |
|       12 |  8741 | `							(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        6 |  8742 | `							bCallIsStrict);` |
|       13 |  8743 | `						if( rcU != SXRET_OK ){` |
|        - |  8744 | `							const char *zGiven;` |
|      ! 0 |  8745 | `							const char *zExpected = "union";` |
|        - |  8746 | `							char zBuf[128];` |
|        - |  8747 | `							char zTypeBuf[128];` |
|      ! 0 |  8748 | `							if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8749 | `								zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      ! 0 |  8750 | `							}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8751 | `								zGiven = "null";` |
|      ! 0 |  8752 | `							}else{` |
|      ! 0 |  8753 | `								zGiven = ph7_type_name(pVal);` |
|        - |  8754 | `							}` |
|      ! 0 |  8755 | `							if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|      ! 0 |  8756 | `								zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|      ! 0 |  8757 | `							}` |
|      ! 0 |  8758 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8759 | `								&aFormalArg[n].sName, zExpected, zGiven);` |
|      ! 0 |  8760 | `							if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8761 | `							SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8762 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  8763 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8764 | `							pFrameStack = 0;` |
|      ! 0 |  8765 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  8766 | `							goto SkipFuncBody;` |
|        - |  8767 | `						}` |
|      171 |  8768 | `					}else if( aFormalArg[n].nType > 0` |
|       91 |  8769 | `						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|        - |  8770 | `						/* Scalar/class type checking */` |
|       17 |  8771 | `						if( aFormalArg[n].nType == SXU32_HIGH ){` |
|      ! 0 |  8772 | `							SyString *pName = &aFormalArg[n].sClass;` |
|      ! 0 |  8773 | `							ph7_class *pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|      ! 0 |  8774 | `							if( pClass ){` |
|      ! 0 |  8775 | `								if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  8776 | `									if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  8777 | `										VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8778 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8779 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8780 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8781 | `									}` |
|      ! 0 |  8782 | `								}else{` |
|      ! 0 |  8783 | `									ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 |  8784 | `									if( !PH7_VmInstanceOf(pInst->pClass,pClass) ){` |
|      ! 0 |  8785 | `										VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  8786 | `											"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  8787 | `											&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  8788 | `										PH7_MemObjRelease(pVal);` |
|      ! 0 |  8789 | `									}` |
|        - |  8790 | `								}` |
|      ! 0 |  8791 | `							}` |
|       17 |  8792 | `						}else if( (pVal->iFlags & aFormalArg[n].nType) == 0 ){` |
|        7 |  8793 | `							if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|      ! 0 |  8794 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8795 | `									&aFormalArg[n].sName,"object",ph7_type_name(pVal));` |
|      ! 0 |  8796 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8797 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8798 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8799 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8800 | `								pFrameStack = 0;` |
|      ! 0 |  8801 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8802 | `								goto SkipFuncBody;` |
|        7 |  8803 | `							}else if( VmEnforceScalarType(pVal, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  8804 | `								char zTypeBuf[128];` |
|      ! 0 |  8805 | `								rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8806 | `									&aFormalArg[n].sName,` |
|      ! 0 |  8807 | `									VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  8808 | `									ph7_type_name(pVal));` |
|      ! 0 |  8809 | `								if( rc == PH7_ABORT ) goto Abort;` |
|      ! 0 |  8810 | `								SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|      ! 0 |  8811 | `								PH7_MemObjRelease(pTos);` |
|      ! 0 |  8812 | `								pTos = &pTos[-nCallArgs];` |
|      ! 0 |  8813 | `								pFrameStack = 0;` |
|      ! 0 |  8814 | `								rc = PH7_EXCEPTION;` |
|      ! 0 |  8815 | `								goto SkipFuncBody;` |
|        - |  8816 | `							}` |
|        3 |  8817 | `						}` |
|        8 |  8818 | `					}` |
|        - |  8819 | `					/* Install: by reference or by value */` |
|      169 |  8820 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        5 |  8821 | `						if( pVal->nIdx == SXU32_HIGH ){` |
|      ! 0 |  8822 | `							if( (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0 ){` |
|      ! 0 |  8823 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  8824 | `									"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  8825 | `									"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  8826 | `							}` |
|      ! 0 |  8827 | `							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  8828 | `						}else{` |
|        7 |  8829 | `							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,` |
|        4 |  8830 | `								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|        5 |  8831 | `							if( pRefEntry == 0 ){` |
|        7 |  8832 | `								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|        4 |  8833 | `									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));` |
|        5 |  8834 | `								sArg.nIdx = pVal->nIdx;` |
|        5 |  8835 | `								sArg.pUserData = 0;` |
|        5 |  8836 | `								SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        2 |  8837 | `							}` |
|        5 |  8838 | `							pObj = 0;` |
|        - |  8839 | `						}` |
|        3 |  8840 | `					}else{` |
|      165 |  8841 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  8842 | `					}` |
|      169 |  8843 | `					if( pObj ){` |
|      165 |  8844 | `						PH7_MemObjStore(pVal,pObj);` |
|      165 |  8845 | `						sArg.nIdx = pObj->nIdx;` |
|      165 |  8846 | `						sArg.pUserData = 0;` |
|      165 |  8847 | `						SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       82 |  8848 | `					}` |
|       85 |  8849 | `				}else{` |
|        - |  8850 | `					/* Argument was NOT provided — use default or leave unset */` |
|       19 |  8851 | `					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  8852 | `						/* Should not reach here; variadic handled separately below */` |
|       19 |  8853 | `					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       19 |  8854 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       19 |  8855 | `						if( pObj ){` |
|       19 |  8856 | `							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|       19 |  8857 | `							if( rc == PH7_ABORT ) goto Abort;` |
|       19 |  8858 | `							sArg.nIdx = pObj->nIdx;` |
|       19 |  8859 | `							sArg.pUserData = 0;` |
|       19 |  8860 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  8861 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|        1 |  8862 | `								&& (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  8863 | `								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  8864 | `								if( xCast ) xCast(pObj);` |
|      ! 0 |  8865 | `							}` |
|        9 |  8866 | `						}` |
|        9 |  8867 | `					}` |
|        - |  8868 | `					/* else: required param missing — leave unset (matches existing behavior) */` |
|        - |  8869 | `				}` |
|       94 |  8870 | `			}` |
|        - |  8871 | `			/* Handle variadic parameter */` |
|       89 |  8872 | `			if( iVariadicIdx >= 0 ){` |
|        9 |  8873 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);` |
|        9 |  8874 | `				if( pObj ){` |
|        9 |  8875 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8876 | `					{` |
|        9 |  8877 | `						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;` |
|       31 |  8878 | `						for( i = 0; i < nActual; i++ ){` |
|       23 |  8879 | `							if( aSlot[i] == -1 ){` |
|       16 |  8880 | `								if( i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0 ){` |
|        - |  8881 | `									/* Named variadic entry: insert with string key */` |
|        - |  8882 | `									ph7_value sKey;` |
|       11 |  8883 | `									PH7_MemObjInit(pVm, &sKey);` |
|       11 |  8884 | `									PH7_MemObjStringAppend(&sKey,` |
|       10 |  8885 | `										pCallMap3->aNames[i].zString,` |
|       10 |  8886 | `										(sxu32)pCallMap3->aNames[i].nByte);` |
|       11 |  8887 | `									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);` |
|       11 |  8888 | `									PH7_MemObjRelease(&sKey);` |
|        6 |  8889 | `								}else{` |
|        - |  8890 | `									/* Positional variadic entry */` |
|      ! 0 |  8891 | `									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);` |
|        - |  8892 | `								}` |
|        5 |  8893 | `							}` |
|       12 |  8894 | `						}` |
|        - |  8895 | `					}` |
|        9 |  8896 | `					sArg.nIdx = pObj->nIdx;` |
|        9 |  8897 | `					sArg.pUserData = 0;` |
|        9 |  8898 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        4 |  8899 | `				}` |
|        5 |  8900 | `			}else{` |
|        - |  8901 | `				/* No variadic — preserve unresolved positional overflow` |
|        - |  8902 | `				 * (aSlot[i] == -2) as anonymous frame args so` |
|        - |  8903 | `				 * func_get_args() / func_num_args() still see them, matching` |
|        - |  8904 | `				 * the positional-only path's behavior. */` |
|       81 |  8905 | `				sxu32 nAnon = nNonVariadic;` |
|      237 |  8906 | `				for( i = 0; i < nActual; i++ ){` |
|      157 |  8907 | `					if( aSlot[i] == -2 ){` |
|        - |  8908 | `						char zAnonBuf[32];` |
|        - |  8909 | `						SyString sAnonName;` |
|      ! 0 |  8910 | `						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),` |
|      ! 0 |  8911 | `							"[%u]apArg",nAnon);` |
|      ! 0 |  8912 | `						sAnonName.zString = zAnonBuf;` |
|      ! 0 |  8913 | `						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);` |
|      ! 0 |  8914 | `						if( pObj ){` |
|      ! 0 |  8915 | `							PH7_MemObjStore(&pArg[i],pObj);` |
|      ! 0 |  8916 | `							sArg.nIdx = pObj->nIdx;` |
|      ! 0 |  8917 | `							sArg.pUserData = 0;` |
|      ! 0 |  8918 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|      ! 0 |  8919 | `						}` |
|      ! 0 |  8920 | `						nAnon++;` |
|      ! 0 |  8921 | `					}` |
|       79 |  8922 | `				}` |
|        - |  8923 | `			}` |
|        - |  8924 | `			/* Release all stack arguments */` |
|      267 |  8925 | `			for( i = 0; i < nActual; i++ ){` |
|      179 |  8926 | `				PH7_MemObjRelease(&pArg[i]);` |
|       90 |  8927 | `			}` |
|       89 |  8928 | `			SyMemBackendFree(&pVm->sAllocator, aSlot);` |
|        - |  8929 | `			/* Set n to nFormal so the defaults loop below is skipped */` |
|       89 |  8930 | `			n = nFormal;` |
|       45 |  8931 | `		}else{` |
|        - |  8932 | `		/* ============================================================` |
|        - |  8933 | `		 * Positional-only matching path (original)` |
|        - |  8934 | `		 * ============================================================ */` |
|    17906 |  8935 | `		n = 0;` |
|    47822 |  8936 | `		while( pArg < pTos ){` |
|    29988 |  8937 | `			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){` |
|        - |  8938 | `				/* Variadic parameter: collect all remaining args into an array */` |
|       40 |  8939 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       40 |  8940 | `				if( pObj ){` |
|        - |  8941 | `					/* Initialize as empty array */` |
|       40 |  8942 | `					PH7_MemObjToHashmap(pObj);` |
|        - |  8943 | `					{` |
|       40 |  8944 | `						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      150 |  8945 | `						while( pArg < pTos ){` |
|        - |  8946 | `							/* Variadic union type: per-element coercion via the shared helper.` |
|        - |  8947 | `							 *` |
|        - |  8948 | `							 * TODO: PHP reports the runtime element index here` |
|        - |  8949 | `							 * ("Argument #3 must be...") but we report the formal-arg` |
|        - |  8950 | `							 * index (always n+1, the position of the variadic). The` |
|        - |  8951 | `							 * non-union variadic path below has the same limitation;` |
|        - |  8952 | `							 * fixing both wants a separate counter for elements` |
|        - |  8953 | `							 * already packed into the variadic array. */` |
|      114 |  8954 | `							if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       23 |  8955 | `								sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       14 |  8956 | `									(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|        7 |  8957 | `									bCallIsStrict);` |
|       16 |  8958 | `								if( rcU != SXRET_OK ){` |
|        - |  8959 | `									const char *zGiven;` |
|        3 |  8960 | `									const char *zExpected = "union";` |
|        - |  8961 | `									char zBuf[128];` |
|        - |  8962 | `									char zTypeBuf[128];` |
|        3 |  8963 | `									if( pArg->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 |  8964 | `										zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|        3 |  8965 | `									}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|      ! 0 |  8966 | `										zGiven = "null";` |
|      ! 0 |  8967 | `									}else{` |
|        3 |  8968 | `										zGiven = ph7_type_name(pArg);` |
|        - |  8969 | `									}` |
|        3 |  8970 | `									if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|        3 |  8971 | `										zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        1 |  8972 | `									}` |
|        4 |  8973 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        2 |  8974 | `										&aFormalArg[n].sName, zExpected, zGiven);` |
|        3 |  8975 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8976 | `										goto Abort;` |
|        - |  8977 | `									}` |
|        3 |  8978 | `									PH7_MemObjRelease(pTos);` |
|        3 |  8979 | `									pTos = &pTos[-nCallArgs];` |
|        3 |  8980 | `									pFrameStack = 0;` |
|        3 |  8981 | `									rc = PH7_EXCEPTION;` |
|        3 |  8982 | `									goto SkipFuncBody;` |
|        - |  8983 | `								}` |
|       14 |  8984 | `								PH7_HashmapInsert(pMap, 0, pArg);` |
|       14 |  8985 | `								pArg++;` |
|       14 |  8986 | `								continue;` |
|        - |  8987 | `							}` |
|        - |  8988 | `							/* Apply type coercion to each element if the variadic has a type hint.` |
|        - |  8989 | `							 * Nullable types (?type) allow null through without coercion. */` |
|      114 |  8990 | `							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|       42 |  8991 | `								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL))` |
|       41 |  8992 | `								&& (pArg->iFlags & aFormalArg[n].nType) == 0 ){` |
|       13 |  8993 | `								if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  8994 | `									/* object type hint on variadic: reject non-objects with TypeError */` |
|      ! 0 |  8995 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  8996 | `										&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|      ! 0 |  8997 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  8998 | `										goto Abort;` |
|        - |  8999 | `									}` |
|        - |  9000 | `									/* Skip function body, route through normal cleanup */` |
|      ! 0 |  9001 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9002 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9003 | `									pFrameStack = 0;` |
|      ! 0 |  9004 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9005 | `									goto SkipFuncBody;` |
|       13 |  9006 | `								}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9007 | `									char zTypeBuf[128];` |
|      ! 0 |  9008 | `									rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|      ! 0 |  9009 | `										&aFormalArg[n].sName,` |
|      ! 0 |  9010 | `										VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|      ! 0 |  9011 | `										ph7_type_name(pArg));` |
|      ! 0 |  9012 | `									if( rc == PH7_ABORT ){` |
|      ! 0 |  9013 | `										goto Abort;` |
|        - |  9014 | `									}` |
|      ! 0 |  9015 | `									PH7_MemObjRelease(pTos);` |
|      ! 0 |  9016 | `									pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9017 | `									pFrameStack = 0;` |
|      ! 0 |  9018 | `									rc = PH7_EXCEPTION;` |
|      ! 0 |  9019 | `									goto SkipFuncBody;` |
|        - |  9020 | `								}` |
|        6 |  9021 | `							}` |
|      100 |  9022 | `							PH7_HashmapInsert(pMap, 0, pArg);` |
|      100 |  9023 | `							pArg++;` |
|        2 |  9024 | `						}` |
|        - |  9025 | `					}` |
|       38 |  9026 | `					sArg.nIdx = pObj->nIdx;` |
|       38 |  9027 | `					sArg.pUserData = 0;` |
|       38 |  9028 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       18 |  9029 | `				}` |
|       38 |  9030 | `				break; /* All remaining args consumed */` |
|        - |  9031 | `			}` |
|    29950 |  9032 | `			if( n < SySetUsed(&pVmFunc->aArgs) ){` |
|    29766 |  9033 | `				if( (pArg->iFlags & MEMOBJ_NULL) && SySetUsed(&aFormalArg[n].aByteCode) > 0` |
|       37 |  9034 | `					&& !(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ){` |
|        - |  9035 | `					/* NULL values are redirected to default arguments (but not for nullable types) */` |
|      ! 0 |  9036 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pArg);` |
|      ! 0 |  9037 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9038 | `						goto Abort;` |
|        - |  9039 | `					}` |
|      ! 0 |  9040 | `				}` |
|        - |  9041 | `				/* Union type: dispatch to the shared coercion helper. */` |
|    29768 |  9042 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_UNION ){` |
|       89 |  9043 | `					sxi32 rcU = VmCoerceToUnion(pVm, pArg, &aFormalArg[n].aUnionAlts,` |
|       58 |  9044 | `						(aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,` |
|       29 |  9045 | `						bCallIsStrict);` |
|       60 |  9046 | `					if( rcU != SXRET_OK ){` |
|        - |  9047 | `						const char *zGiven;` |
|       19 |  9048 | `						const char *zExpected = "union";` |
|        - |  9049 | `						char zBuf[128];` |
|        - |  9050 | `						char zTypeBuf[128];` |
|       19 |  9051 | `						if( pArg->iFlags & MEMOBJ_OBJ ){` |
|        7 |  9052 | `							zGiven = VmFormatValueClassName(pArg,zBuf,sizeof(zBuf));` |
|       16 |  9053 | `						}else if( pArg->iFlags & MEMOBJ_NULL ){` |
|        9 |  9054 | `							zGiven = "null";` |
|        5 |  9055 | `						}else{` |
|        5 |  9056 | `							zGiven = ph7_type_name(pArg);` |
|        - |  9057 | `						}` |
|       19 |  9058 | `						if( SyStringLength(&aFormalArg[n].sTypeName) > 0 ){` |
|       19 |  9059 | `							zExpected = VmSyStringToCStr(&aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf));` |
|        9 |  9060 | `						}` |
|       28 |  9061 | `						rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       18 |  9062 | `							&aFormalArg[n].sName, zExpected, zGiven);` |
|       19 |  9063 | `						if( rc == PH7_ABORT ){` |
|      ! 0 |  9064 | `							goto Abort;` |
|        - |  9065 | `						}` |
|       19 |  9066 | `						PH7_MemObjRelease(pTos);` |
|       19 |  9067 | `						pTos = &pTos[-nCallArgs];` |
|       19 |  9068 | `						pFrameStack = 0;` |
|       19 |  9069 | `						rc = PH7_EXCEPTION;` |
|       19 |  9070 | `						goto SkipFuncBody;` |
|        - |  9071 | `					}` |
|       21 |  9072 | `				}else` |
|        - |  9073 | `				/* Make sure the given arguments are of the correct type.` |
|        - |  9074 | `				 * Nullable types (?type) allow null through without coercion. */` |
|    29734 |  9075 | `				if( aFormalArg[n].nType > 0` |
|    15560 |  9076 | `					&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pArg->iFlags & MEMOBJ_NULL)) ){` |
|     1384 |  9077 | `					if ( aFormalArg[n].nType == SXU32_HIGH ){` |
|        - |  9078 | `						/* Argument must be a class instance [i.e: object] */` |
|       26 |  9079 | `						SyString *pName = &aFormalArg[n].sClass;` |
|        - |  9080 | `						ph7_class *pClass;` |
|        - |  9081 | `						/* Try to extract the desired class */` |
|       26 |  9082 | `						pClass = PH7_VmExtractClass(&(*pVm),pName->zString,pName->nByte,TRUE,0);` |
|       26 |  9083 | `						if( pClass ){` |
|       22 |  9084 | `							if( (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9085 | `								if( (pArg->iFlags & MEMOBJ_NULL) == 0 ){` |
|      ! 0 |  9086 | `									VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9087 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9088 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9089 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9090 | `								}` |
|      ! 0 |  9091 | `							}else{` |
|        - |  9092 | `								/* reuse pThis declared in outer scope */` |
|       22 |  9093 | `								pThis = (ph7_class_instance *)pArg->x.pOther;` |
|        - |  9094 | `								/* Make sure the object is an instance of the given class */` |
|       22 |  9095 | `								if( ! PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      ! 0 |  9096 | `									VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|        - |  9097 | `										"Function '%z()':Argument %u must be an object of type '%z',PH7 is loading NULL instead",` |
|      ! 0 |  9098 | `										&pVmFunc->sName,n+1,pName);` |
|      ! 0 |  9099 | `									PH7_MemObjRelease(pArg);` |
|      ! 0 |  9100 | `								}` |
|        - |  9101 | `							}` |
|       12 |  9102 | `						}` |
|     1372 |  9103 | `					}else if( ((pArg->iFlags & aFormalArg[n].nType) == 0) ){` |
|       24 |  9104 | `						if( aFormalArg[n].nType == MEMOBJ_OBJ ){` |
|        - |  9105 | `							/* object type hint: reject non-objects with TypeError */` |
|       16 |  9106 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|       10 |  9107 | `								&aFormalArg[n].sName,"object",ph7_type_name(pArg));` |
|       11 |  9108 | `							if( rc == PH7_ABORT ){` |
|      ! 0 |  9109 | `								goto Abort;` |
|        - |  9110 | `							}` |
|        - |  9111 | `							/* Skip function body, route through normal cleanup */` |
|       11 |  9112 | `							PH7_MemObjRelease(pTos);` |
|       11 |  9113 | `							pTos = &pTos[-nCallArgs];` |
|       11 |  9114 | `							pFrameStack = 0;` |
|       11 |  9115 | `							rc = PH7_EXCEPTION;` |
|       11 |  9116 | `							goto SkipFuncBody;` |
|       14 |  9117 | `						}else if( VmEnforceScalarType(pArg, aFormalArg[n].nType, bCallIsStrict) != SXRET_OK ){` |
|        - |  9118 | `							char zTypeBuf[128];` |
|        7 |  9119 | `							rc = VmThrowTypeErrorForArg(&(*pVm),&pVmFunc->sName,n+1,` |
|        4 |  9120 | `								&aFormalArg[n].sName,` |
|        4 |  9121 | `								VmScalarTypeName(aFormalArg[n].nType, &aFormalArg[n].sTypeName, zTypeBuf, sizeof(zTypeBuf)),` |
|        2 |  9122 | `								ph7_type_name(pArg));` |
|        5 |  9123 | `							if( rc == PH7_ABORT ){` |
|        5 |  9124 | `								goto Abort;` |
|        - |  9125 | `							}` |
|      ! 0 |  9126 | `							PH7_MemObjRelease(pTos);` |
|      ! 0 |  9127 | `							pTos = &pTos[-nCallArgs];` |
|      ! 0 |  9128 | `							pFrameStack = 0;` |
|      ! 0 |  9129 | `							rc = PH7_EXCEPTION;` |
|      ! 0 |  9130 | `							goto SkipFuncBody;` |
|        - |  9131 | `						}` |
|        4 |  9132 | `					}` |
|      684 |  9133 | `				}` |
|    29736 |  9134 | `				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){` |
|        - |  9135 | `					/* Pass by reference */` |
|       58 |  9136 | `					if( pArg->nIdx == SXU32_HIGH ){` |
|        - |  9137 | `						/* Expecting a variable,not a constant,raise an exception */` |
|      ! 0 |  9138 | `						if((pArg->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES\|MEMOBJ_NULL)) == 0){` |
|      ! 0 |  9139 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|        - |  9140 | `								"Function '%z',%d argument: Pass by reference,expecting a variable not a "` |
|      ! 0 |  9141 | `								"constant,PH7 is switching to pass by value",&pVmFunc->sName,n+1);` |
|      ! 0 |  9142 | `						}` |
|        - |  9143 | `						/* Switch to pass by value */` |
|      ! 0 |  9144 | `						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|      ! 0 |  9145 | `					}else{` |
|        - |  9146 | `						SyHashEntry *pRefEntry;` |
|        - |  9147 | `						/* Install the referenced variable in the private function frame */` |
|       58 |  9148 | `						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));` |
|       58 |  9149 | `						if( pRefEntry == 0 ){` |
|       86 |  9150 | `							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),` |
|       56 |  9151 | `								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));` |
|       58 |  9152 | `							sArg.nIdx = pArg->nIdx;` |
|       58 |  9153 | `							sArg.pUserData = 0;` |
|       58 |  9154 | `							SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       28 |  9155 | `						}` |
|       58 |  9156 | `						pObj = 0;` |
|        - |  9157 | `					}` |
|       30 |  9158 | `				}else{` |
|        - |  9159 | `					/* Pass by value,make a copy of the given argument */` |
|    29680 |  9160 | `					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|        - |  9161 | `				}` |
|    14869 |  9162 | `			}else{` |
|        - |  9163 | `				char zName[32];` |
|        - |  9164 | `				SyString sArgName;` |
|        - |  9165 | `				/* Set a dummy name */` |
|      184 |  9166 | `				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);` |
|      184 |  9167 | `				sArgName.zString = zName;` |
|        - |  9168 | `				/* Annonymous argument */` |
|      184 |  9169 | `				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);` |
|        - |  9170 | `			}` |
|    29918 |  9171 | `			if( pObj ){` |
|    29862 |  9172 | `				PH7_MemObjStore(pArg,pObj);` |
|        - |  9173 | `				/* Insert argument index  */` |
|    29862 |  9174 | `				sArg.nIdx = pObj->nIdx;` |
|    29862 |  9175 | `				sArg.pUserData = 0;` |
|    29862 |  9176 | `				SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|    14930 |  9177 | `			}` |
|    29918 |  9178 | `			PH7_MemObjRelease(pArg);` |
|    29918 |  9179 | `			pArg++;` |
|    29918 |  9180 | `			++n;` |
|        2 |  9181 | `		}` |
|        - |  9182 | `		} /* end named vs positional branch */` |
|        - |  9183 | `		/* Set up closure environment */` |
|    17960 |  9184 | `		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - |  9185 | `			ph7_vm_func_closure_env *aEnv,*pEnv;` |
|        - |  9186 | `			ph7_value *pValue;` |
|        - |  9187 | `			sxu32 iEnv;` |
|      120 |  9188 | `			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);` |
|      306 |  9189 | `			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){` |
|      188 |  9190 | `				pEnv = &aEnv[iEnv];` |
|      188 |  9191 | `				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        - |  9192 | `					/* Do not install null value */` |
|      114 |  9193 | `					continue;` |
|        - |  9194 | `				}` |
|       76 |  9195 | `				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);` |
|       76 |  9196 | `				if( pValue == 0 ){` |
|      ! 0 |  9197 | `					continue;` |
|        - |  9198 | `				}` |
|        - |  9199 | `				/* Invalidate any prior representation */` |
|       76 |  9200 | `				PH7_MemObjRelease(pValue);` |
|        - |  9201 | `				/* Duplicate bound variable value */` |
|       76 |  9202 | `				PH7_MemObjStore(&pEnv->sValue,pValue);` |
|       39 |  9203 | `			}` |
|       59 |  9204 | `		}` |
|        - |  9205 | `		/* Process default values for remaining formal parameters */` |
|    20694 |  9206 | `		while( n < SySetUsed(&pVmFunc->aArgs) ){` |
|     2782 |  9207 | `			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - |  9208 | `				/* Variadic parameter with no extra args — create empty array */` |
|       48 |  9209 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|       48 |  9210 | `				if( pObj ){` |
|       48 |  9211 | `					PH7_MemObjToHashmap(pObj);` |
|       48 |  9212 | `					sArg.nIdx = pObj->nIdx;` |
|       48 |  9213 | `					sArg.pUserData = 0;` |
|       48 |  9214 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|       23 |  9215 | `				}` |
|       48 |  9216 | `				n++;` |
|       48 |  9217 | `				break; /* Variadic is always last */` |
|        - |  9218 | `			}` |
|     2736 |  9219 | `			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|     2730 |  9220 | `				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);` |
|     2730 |  9221 | `				if( pObj ){` |
|        - |  9222 | `					/* Evaluate the default value and extract it's result */` |
|     2730 |  9223 | `					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj);` |
|     2730 |  9224 | `					if( rc == PH7_ABORT ){` |
|      ! 0 |  9225 | `						goto Abort;` |
|        - |  9226 | `					}` |
|        - |  9227 | `					/* Insert argument index */` |
|     2730 |  9228 | `					sArg.nIdx = pObj->nIdx;` |
|     2730 |  9229 | `					sArg.pUserData = 0;` |
|     2730 |  9230 | `					SySetPut(&pFrame->sArg,(const void *)&sArg);` |
|        - |  9231 | `					/* Make sure the default argument is of the correct type */` |
|     2728 |  9232 | `					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ` |
|     1786 |  9233 | `						&& ((pObj->iFlags & aFormalArg[n].nType) == 0) ){` |
|        3 |  9234 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|        - |  9235 | `						/* Cast to the desired type */` |
|        3 |  9236 | `						xCast(pObj);` |
|        1 |  9237 | `					}` |
|     1364 |  9238 | `				}` |
|     1364 |  9239 | `			}` |
|     2736 |  9240 | `			++n;` |
|        2 |  9241 | `		}` |
|        - |  9242 | `		} /* end VmCallArgMap scope */` |
|        - |  9243 | `		/* Pop arguments,function name from the operand stack and assume the function` |
|        - |  9244 | `		 * does not return anything.` |
|        - |  9245 | `		 */` |
|    17960 |  9246 | `		PH7_MemObjRelease(pTos);` |
|    17960 |  9247 | `		pTos = &pTos[-nCallArgs];` |
|        - |  9248 | `		/* Allocate a new operand stack and evaluate the function body */` |
|    17960 |  9249 | `		pFrameStack = VmNewOperandStack(&(*pVm),SySetUsed(&pVmFunc->aByteCode));` |
|    17960 |  9250 | `		if( pFrameStack == 0 ){` |
|        - |  9251 | `			/* Raise exception: Out of memory */` |
|      ! 0 |  9252 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",` |
|      ! 0 |  9253 | `				&pVmFunc->sName);` |
|      ! 0 |  9254 | `			if( nCallArgs > 0 ){` |
|      ! 0 |  9255 | `				VmPopOperand(&pTos,nCallArgs);` |
|      ! 0 |  9256 | `			}` |
|      ! 0 |  9257 | `			break;` |
|        - |  9258 | `		}` |
|     8979 |  9259 | `SkipFuncBody:` |
|    17990 |  9260 | `		if( pSelf ){` |
|        - |  9261 | `			/* Push class name */` |
|     3210 |  9262 | `			SySetPut(&pVm->aSelf,(const void *)&pSelf);` |
|     1604 |  9263 | `		}` |
|        - |  9264 | `		/* Increment nesting level */` |
|    17990 |  9265 | `		pVm->nRecursionDepth++;` |
|    17990 |  9266 | `		if( rc != PH7_EXCEPTION ){` |
|        - |  9267 | `			/* Execute function body */` |
|    26939 |  9268 | `			rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),pFrameStack,-1,pTos,&n,FALSE,0,` |
|    17958 |  9269 | `				pVmFunc->nReturnType > 0 ? pVmFunc : 0);` |
|     8979 |  9270 | `		}` |
|        - |  9271 | `		/* Decrement nesting level */` |
|    17990 |  9272 | `		pVm->nRecursionDepth--;` |
|    17990 |  9273 | `		if( pSelf ){` |
|        - |  9274 | `			/* Pop class name */` |
|     3210 |  9275 | `			(void)SySetPop(&pVm->aSelf);` |
|     1604 |  9276 | `		}` |
|        - |  9277 | `		/* Cleanup the mess left behind */` |
|    17990 |  9278 | `		if( (pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){` |
|        - |  9279 | `			/* Return by reference,reflect that */` |
|        9 |  9280 | `			if( n != SXU32_HIGH ){` |
|        9 |  9281 | `				VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|        - |  9282 | `				sxu32 i;` |
|        - |  9283 | `				/* Make sure the referenced object is not a local variable */` |
|       13 |  9284 | `				for( i = 0 ; i < SySetUsed(&pFrame->sLocal) ; ++i ){` |
|        5 |  9285 | `					if( n == aSlot[i].nIdx ){` |
|      ! 0 |  9286 | `						pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|      ! 0 |  9287 | `						if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9288 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9289 | `								"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",` |
|      ! 0 |  9290 | `								&pVmFunc->sName);` |
|      ! 0 |  9291 | `						}` |
|      ! 0 |  9292 | `						n = SXU32_HIGH;` |
|      ! 0 |  9293 | `						break;` |
|        - |  9294 | `					}` |
|        3 |  9295 | `				}` |
|        5 |  9296 | `			}else{` |
|      ! 0 |  9297 | `				if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL\|MEMOBJ_RES)) == 0 ){` |
|      ! 0 |  9298 | `					VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|        - |  9299 | `						"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",` |
|      ! 0 |  9300 | `						&pVmFunc->sName);` |
|      ! 0 |  9301 | `				}` |
|        - |  9302 | `			}` |
|        9 |  9303 | `			pTos->nIdx = n;` |
|        4 |  9304 | `		}` |
|        - |  9305 | `		/* Cleanup the mess left behind */` |
|    17990 |  9306 | `		if( rc != PH7_ABORT && ((pFrame->iFlags & VM_FRAME_THROW) \|\| rc == PH7_EXCEPTION) ){` |
|        - |  9307 | `			/* An exception was throw in this frame */` |
|       64 |  9308 | `			pFrame = pFrame->pParent;` |
|       64 |  9309 | `			if( !is_callback && pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) && pFrame->iExceptionJump > 0 ){` |
|        - |  9310 | `				/* Pop the resutlt */` |
|       62 |  9311 | `				VmPopOperand(&pTos,1);` |
|        - |  9312 | `				/* Jump to this destination */` |
|       62 |  9313 | `				pc = pFrame->iExceptionJump - 1;` |
|       62 |  9314 | `				rc = PH7_OK;` |
|       32 |  9315 | `			}else{` |
|        3 |  9316 | `				if( pFrame->pParent ){` |
|        3 |  9317 | `					rc = PH7_EXCEPTION;` |
|        2 |  9318 | `				}else{` |
|        - |  9319 | `					/* Continue normal execution */` |
|      ! 0 |  9320 | `					rc = PH7_OK;` |
|        - |  9321 | `				}` |
|        - |  9322 | `			}` |
|       31 |  9323 | `		}` |
|        - |  9324 | `		/* Free the operand stack (NULL when function body was skipped) */` |
|    17990 |  9325 | `		if( pFrameStack ){` |
|    17960 |  9326 | `			SyMemBackendFree(&pVm->sAllocator,pFrameStack);` |
|     8979 |  9327 | `		}` |
|        - |  9328 | `		/* Leave the frame */` |
|    17990 |  9329 | `		VmLeaveFrame(&(*pVm));` |
|    17990 |  9330 | `		if( rc == PH7_ABORT ){` |
|        - |  9331 | `			/* Abort processing immeditaley */` |
|       15 |  9332 | `			goto Abort;` |
|    17976 |  9333 | `		}else if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9334 | `			/* A Fiber::suspend() was called somewhere inside this function.` |
|        - |  9335 | `			 * Re-save the fiber's state at THIS level (the fiber's body),` |
|        - |  9336 | `			 * overwriting the state saved by the inner level.` |
|        - |  9337 | `			 * pTos points to the result slot (not yet written).` |
|        - |  9338 | `			 * Save nTos one below so resume pushes at the result slot. */` |
|       40 |  9339 | `			VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack) - 1);` |
|       40 |  9340 | `			goto Suspend;` |
|    17938 |  9341 | `		}else if( rc == PH7_EXCEPTION ){` |
|        3 |  9342 | `			goto Exception;` |
|        - |  9343 | `		}` |
|     8969 |  9344 | `	}else{` |
|        - |  9345 | `		ph7_user_func *pFunc;` |
|        - |  9346 | `		ph7_context sCtx;` |
|        - |  9347 | `		ph7_value sRet;` |
|        - |  9348 | `		/* Look for an installed foreign function.` |
|        - |  9349 | `		 * Host functions are registered with short names (strlen, etc.).` |
|        - |  9350 | `		 * If the compiler namespace-qualified the name, extract the short` |
|        - |  9351 | `		 * name (last component after \) and try that. This implements PHP's` |
|        - |  9352 | `		 * global fallback for unqualified function calls in namespaces. */` |
|   683986 |  9353 | `		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);` |
|        - |  9354 | `		{` |
|   683986 |  9355 | `		VmCallArgMap *pCallMap2 = (VmCallArgMap *)pInstr->p3;` |
|   683986 |  9356 | `		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){` |
|        - |  9357 | `			/* Compiler-qualified: try short name as global fallback */` |
|       22 |  9358 | `			const char *zShort = sName.zString;` |
|        - |  9359 | `			sxu32 i;` |
|      334 |  9360 | `			for( i = 0; i < sName.nByte; i++ ){` |
|      314 |  9361 | `				if( sName.zString[i] == '\\' ){` |
|       28 |  9362 | `					zShort = &sName.zString[i + 1];` |
|       13 |  9363 | `				}` |
|      158 |  9364 | `			}` |
|       22 |  9365 | `			if( zShort != sName.zString ){` |
|       22 |  9366 | `				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));` |
|       22 |  9367 | `				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);` |
|       10 |  9368 | `			}` |
|       10 |  9369 | `		}` |
|        - |  9370 | `		} /* end VmCallArgMap namespace scope */` |
|   683986 |  9371 | `		if( pEntry == 0 ){` |
|        - |  9372 | `			/* Call to undefined function */` |
|        5 |  9373 | `			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Call to undefined function '%z',NULL will be returned",&sName);` |
|        - |  9374 | `			/* Pop given arguments */` |
|        5 |  9375 | `			if( pInstr->iP1 > 0 ){` |
|      ! 0 |  9376 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|      ! 0 |  9377 | `			}` |
|        - |  9378 | `			/* Assume a null return value so that the program continue it's execution normally */` |
|        5 |  9379 | `			PH7_MemObjRelease(pTos);` |
|       45 |  9380 | `			break;` |
|        - |  9381 | `		}` |
|   683982 |  9382 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|        - |  9383 | `		/* Start collecting function arguments */` |
|   683982 |  9384 | `		SySetReset(&aArg);` |
|  1842704 |  9385 | `		while( pArg < pTos ){` |
|  1158724 |  9386 | `			SySetPut(&aArg,(const void *)&pArg);` |
|  1158724 |  9387 | `			pArg++;` |
|        2 |  9388 | `		}` |
|        - |  9389 | `		/* Assume a null return value */` |
|   683982 |  9390 | `		PH7_MemObjInit(&(*pVm),&sRet);` |
|        - |  9391 | `		/* Init the call context */` |
|   683982 |  9392 | `		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);` |
|        - |  9393 | `		/* Call the foreign function */` |
|   683982 |  9394 | `		rc = pFunc->xFunc(&sCtx,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg));` |
|        - |  9395 | `		/* Release the call context */` |
|   683982 |  9396 | `		VmReleaseCallContext(&sCtx);` |
|   683982 |  9397 | `		if( rc == PH7_ABORT ){` |
|      491 |  9398 | `			goto Abort;` |
|   683492 |  9399 | `		}else if( rc == PH7_EXCEPTION ){` |
|       86 |  9400 | `			VmFrame *pFrm = pVm->pFrame;` |
|       86 |  9401 | `			pFrm = VmSkipExceptionFrames(pFrm);` |
|       86 |  9402 | `			if( pFrm->iFlags & VM_FRAME_THROW ){` |
|        - |  9403 | `				/* Exception was NOT caught, propagate */` |
|        5 |  9404 | `				goto Exception;` |
|        - |  9405 | `			}` |
|        - |  9406 | `			/* Exception was caught: pop args and the result slot */` |
|       82 |  9407 | `			PH7_MemObjRelease(&sRet);` |
|       82 |  9408 | `			if( pInstr->iP1 > 0 ){` |
|       66 |  9409 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       32 |  9410 | `			}` |
|        - |  9411 | `			/* Pop the call's return/name slot to restore pre-try stack */` |
|       82 |  9412 | `			VmPopOperand(&pTos,1);` |
|        - |  9413 | `			/* Jump past the try/catch block via the exception frame */` |
|       82 |  9414 | `			pFrm = pVm->pFrame;` |
|       82 |  9415 | `			if( (pFrm->iFlags & VM_FRAME_EXCEPTION) && pFrm->iExceptionJump > 0 ){` |
|       82 |  9416 | `				pc = pFrm->iExceptionJump - 1;` |
|       40 |  9417 | `			}` |
|       82 |  9418 | `			break;` |
|        - |  9419 | `		}` |
|   683408 |  9420 | `		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){` |
|        - |  9421 | `			/* Fiber::suspend() was called from within a fiber.` |
|        - |  9422 | `			 * Pop arguments (like normal path) but don't push a return value.` |
|        - |  9423 | `			 * Propagate PH7_SUSPEND up. If this is the fiber's own` |
|        - |  9424 | `			 * VmByteCodeExec, the CALL was to a foreign function directly` |
|        - |  9425 | `			 * and we need to save state here. If it's a nested call (method` |
|        - |  9426 | `			 * body), the user-function path above will handle re-saving. */` |
|       40 |  9427 | `			PH7_MemObjRelease(&sRet);` |
|       40 |  9428 | `			if( pInstr->iP1 > 0 ){` |
|       40 |  9429 | `				VmPopOperand(&pTos,pInstr->iP1);` |
|       19 |  9430 | `			}` |
|        - |  9431 | `			/* Save fiber state: pc+1 is the instruction after this CALL.` |
|        - |  9432 | `			 * nTos is one below pTos so resume pushes at the return-value slot. */` |
|       40 |  9433 | `			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);` |
|       40 |  9434 | `			goto Suspend;` |
|        - |  9435 | `		}` |
|   683370 |  9436 | `		if( pInstr->iP1 > 0 ){` |
|        - |  9437 | `			/* Pop function name and arguments */` |
|   661902 |  9438 | `			VmPopOperand(&pTos,pInstr->iP1);` |
|   330972 |  9439 | `		}` |
|        - |  9440 | `		/* Save foreign function return value */` |
|   683370 |  9441 | `		PH7_MemObjStore(&sRet,pTos);` |
|   683370 |  9442 | `		PH7_MemObjRelease(&sRet);` |
|        - |  9443 | `	}` |
|   701304 |  9444 | `	break;` |
|        - |  9445 | `				  }` |
|        - |  9446 | `/*` |
|        - |  9447 | ` * OP_CONSUME: P1 * *` |
|        - |  9448 | ` * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.` |
|        - |  9449 | ` */` |
|    15328 |  9450 | `case PH7_OP_CONSUME: {` |
|    30658 |  9451 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    30658 |  9452 | `	ph7_value *pCur,*pOut = pTos;` |
|        - |  9453 |  |
|    30658 |  9454 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|    30658 |  9455 | `	pCur = pOut;` |
|        - |  9456 | `	/* Start the consume process  */` |
|    61320 |  9457 | `	while( pOut <= pTos ){` |
|        - |  9458 | `		/* Force a string cast */` |
|    30664 |  9459 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|      970 |  9460 | `			PH7_MemObjToString(pOut);` |
|      484 |  9461 | `		}` |
|    30664 |  9462 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|        - |  9463 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|        - |  9464 | `			/* Invoke the output consumer callback */` |
|    18492 |  9465 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|    18492 |  9466 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|    18492 |  9467 | `			SyBlobRelease(&pOut->sBlob);` |
|    18492 |  9468 | `			if( rc == SXERR_ABORT ){` |
|        - |  9469 | `				/* Output consumer callback request an operation abort. */` |
|      ! 0 |  9470 | `				goto Abort;` |
|        - |  9471 | `			}` |
|     9245 |  9472 | `		}` |
|    30664 |  9473 | `		pOut++;` |
|        2 |  9474 | `	}` |
|    30658 |  9475 | `	pTos = &pCur[-1];` |
|    30656 |  9476 | `	break;` |
|        - |  9477 | `					 }` |
|        - |  9478 |  |
|        - |  9479 | `		} /* Switch() */` |
| 11604862 |  9480 | `		pc++; /* Next instruction in the stream */` |
|        2 |  9481 | `	} /* For(;;) */` |
|    21529 |  9482 | `Done:` |
|    43060 |  9483 | `	SySetRelease(&aArg);` |
|    43060 |  9484 | `	return SXRET_OK;` |
|       72 |  9485 | `Suspend:` |
|      146 |  9486 | `	SySetRelease(&aArg);` |
|      146 |  9487 | `	return PH7_SUSPEND;` |
|      269 |  9488 | `Abort:` |
|      539 |  9489 | `	SySetRelease(&aArg);` |
|     1839 |  9490 | `	while( pTos >= pStack ){` |
|     1301 |  9491 | `		PH7_MemObjRelease(pTos);` |
|     1301 |  9492 | `		pTos--;` |
|        1 |  9493 | `	}` |
|      539 |  9494 | `	return PH7_ABORT;` |
|       10 |  9495 | `Exception:` |
|       22 |  9496 | `	SySetRelease(&aArg);` |
|       36 |  9497 | `	while( pTos >= pStack ){` |
|       16 |  9498 | `		PH7_MemObjRelease(pTos);` |
|       16 |  9499 | `		pTos--;` |
|        2 |  9500 | `	}` |
|       22 |  9501 | `	return PH7_EXCEPTION;` |
|    21882 |  9502 |  |
|        - |  9503 | `/*` |
|        - |  9504 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|        - |  9505 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9506 | ` * See block-comment on that function for additional information.` |
|        - |  9507 | ` */` |
|    19986 |  9508 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult)` |
|        2 |  9509 |  |
|        - |  9510 | `	ph7_value *pStack;` |
|        - |  9511 | `	sxi32 rc;` |
|        - |  9512 | `	/* Allocate a new operand stack */` |
|    19988 |  9513 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|    19988 |  9514 | `	if( pStack == 0 ){` |
|      ! 0 |  9515 | `		return SXERR_MEM;` |
|        - |  9516 | `	}` |
|        - |  9517 | `	/* Execute the program */` |
|    19988 |  9518 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0);` |
|        - |  9519 | `	/* Free the operand stack */` |
|    19988 |  9520 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|        - |  9521 | `	/* Execution result */` |
|    19988 |  9522 | `	return rc;` |
|     9995 |  9523 |  |
|        - |  9524 | `/*` |
|        - |  9525 | ` * Invoke any installed shutdown callbacks.` |
|        - |  9526 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|        - |  9527 | ` * or more calls to [register_shutdown_function()].` |
|        - |  9528 | ` * These callbacks are invoked by the virtual machine when the program` |
|        - |  9529 | ` * execution ends.` |
|        - |  9530 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|        - |  9531 | ` * additional information.` |
|        - |  9532 | ` */` |
|     2800 |  9533 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|        2 |  9534 |  |
|        - |  9535 | `	VmShutdownCB *pEntry;` |
|        - |  9536 | `	ph7_value *apArg[10];` |
|        - |  9537 | `	sxu32 n,nEntry;` |
|        - |  9538 | `	int i;` |
|        - |  9539 | `	/* Point to the stack of registered callbacks */` |
|     2802 |  9540 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|    30802 |  9541 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|    28002 |  9542 | `		apArg[i] = 0;` |
|    14002 |  9543 | `	}` |
|     2804 |  9544 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        3 |  9545 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9546 | `		if( pEntry ){` |
|        - |  9547 | `			/* Prepare callback arguments if any */` |
|        3 |  9548 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|      ! 0 |  9549 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|      ! 0 |  9550 | `					break;` |
|        - |  9551 | `				}` |
|      ! 0 |  9552 | `				apArg[i] = &pEntry->aArg[i];` |
|      ! 0 |  9553 | `			}` |
|        - |  9554 | `			/* Invoke the callback */` |
|        3 |  9555 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|        - |  9556 | `			/*` |
|        - |  9557 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|        - |  9558 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|        - |  9559 | `			 */` |
|        3 |  9560 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        3 |  9561 | `			if( pEntry ){` |
|        3 |  9562 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        3 |  9563 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|      ! 0 |  9564 | `					PH7_MemObjRelease(apArg[i]);` |
|      ! 0 |  9565 | `				}` |
|        1 |  9566 | `			}` |
|        1 |  9567 | `		}` |
|        2 |  9568 | `	}` |
|     2802 |  9569 | `	SySetReset(&pVm->aShutdown);` |
|     2802 |  9570 |  |
|        - |  9571 | `/*` |
|        - |  9572 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|        - |  9573 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|        - |  9574 | ` * See block-comment on that function for additional information.` |
|        - |  9575 | ` */` |
|     2808 |  9576 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|        2 |  9577 |  |
|        - |  9578 | `	/* Make sure we are ready to execute this program */` |
|     2810 |  9579 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|      ! 0 |  9580 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|        - |  9581 | `	}` |
|        - |  9582 | `	/* Set the execution magic number  */` |
|     2810 |  9583 | `	pVm->nMagic = PH7_VM_EXEC;` |
|        - |  9584 | `	/* Execute the program */` |
|     2810 |  9585 | `	VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0);` |
|        - |  9586 | `	/* Invoke any shutdown callbacks */` |
|     2806 |  9587 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|        - |  9588 | `	/*` |
|        - |  9589 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|        - |  9590 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|        - |  9591 | `	 * [ph7_vm_reset()] first would fail.` |
|        - |  9592 | `	 */` |
|     2806 |  9593 | `	return SXRET_OK;` |
|     1406 |  9594 |  |
|        - |  9595 | `/* ======================== Fiber Infrastructure ======================== */` |
|        - |  9596 | `/*` |
|        - |  9597 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  9598 | ` * The context is in CREATED state and ready to be started.` |
|        - |  9599 | ` */` |
|       46 |  9600 | `static ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        2 |  9601 |  |
|        - |  9602 | `	ph7_exec_ctx *pCtx;` |
|        - |  9603 | `	ph7_value *pStack;` |
|        - |  9604 | `	VmFrame *pFrame;` |
|       48 |  9605 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|       48 |  9606 | `	if( pCtx == 0 ){` |
|      ! 0 |  9607 | `		return 0;` |
|        - |  9608 | `	}` |
|       48 |  9609 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|       48 |  9610 | `	pCtx->pVm = pVm;` |
|       48 |  9611 | `	pCtx->pFunc = pFunc;` |
|       48 |  9612 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|       48 |  9613 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|       48 |  9614 | `	pCtx->pc = 0;` |
|       48 |  9615 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|       48 |  9616 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|        - |  9617 | `	/* Allocate a private operand stack */` |
|       48 |  9618 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|       48 |  9619 | `	if( pStack == 0 ){` |
|      ! 0 |  9620 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9621 | `		return 0;` |
|        - |  9622 | `	}` |
|       48 |  9623 | `	pCtx->pStack = pStack;` |
|        - |  9624 | `	/* Create a detached frame for the fiber */` |
|       48 |  9625 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|       48 |  9626 | `	if( pFrame == 0 ){` |
|      ! 0 |  9627 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  9628 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  9629 | `		return 0;` |
|        - |  9630 | `	}` |
|       48 |  9631 | `	pCtx->pFrame = pFrame;` |
|       48 |  9632 | `	return pCtx;` |
|       25 |  9633 |  |
|        - |  9634 | `/*` |
|        - |  9635 | ` * Start executing a fiber context for the first time.` |
|        - |  9636 | ` */` |
|       46 |  9637 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        2 |  9638 |  |
|        - |  9639 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9640 | `	sxi32 rc;` |
|       48 |  9641 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 |  9642 | `		return SXERR_INVALID;` |
|        - |  9643 | `	}` |
|        - |  9644 | `	/* Attach the fiber's frame to the VM frame chain */` |
|       48 |  9645 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|       48 |  9646 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9647 | `	/* Save and set the active context */` |
|       48 |  9648 | `	pOldCtx = pVm->pActiveCtx;` |
|       48 |  9649 | `	pVm->pActiveCtx = pCtx;` |
|       48 |  9650 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       48 |  9651 | `	pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|       48 |  9652 | `	pVm->nRecursionDepth++;` |
|        - |  9653 | `	/* Execute from the beginning */` |
|       48 |  9654 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       23 |  9655 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|       46 |  9656 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|       48 |  9657 | `	pVm->nRecursionDepth--;` |
|        - |  9658 | `	/* Restore the previous context */` |
|       48 |  9659 | `	pVm->pActiveCtx = pOldCtx;` |
|       48 |  9660 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9661 | `		/* Fiber suspended. Detach the fiber's frame from the VM chain. */` |
|       46 |  9662 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       46 |  9663 | `		pCtx->pFrame->pParent = 0;` |
|       46 |  9664 | `		if( pResult ){` |
|       24 |  9665 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|       11 |  9666 | `		}` |
|       46 |  9667 | `		return SXRET_OK;` |
|        - |  9668 | `	}` |
|        - |  9669 | `	/* Detach frame */` |
|        3 |  9670 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|        3 |  9671 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|        3 |  9672 | `		pCtx->pFrame->pParent = 0;` |
|        1 |  9673 | `	}` |
|        3 |  9674 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9675 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9676 | `		return PH7_ABORT;` |
|        - |  9677 | `	}` |
|        3 |  9678 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9679 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9680 | `		return PH7_EXCEPTION;` |
|        - |  9681 | `	}` |
|        - |  9682 | `	/* Normal completion */` |
|        3 |  9683 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|        3 |  9684 | `	if( pResult ){` |
|        3 |  9685 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        1 |  9686 | `	}` |
|        3 |  9687 | `	return SXRET_OK;` |
|       25 |  9688 |  |
|        - |  9689 | `/*` |
|        - |  9690 | ` * Resume a suspended fiber context.` |
|        - |  9691 | ` */` |
|       98 |  9692 | `static sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        2 |  9693 |  |
|        - |  9694 | `	ph7_exec_ctx *pOldCtx;` |
|        - |  9695 | `	sxi32 rc;` |
|      100 |  9696 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 |  9697 | `		return SXERR_INVALID;` |
|        - |  9698 | `	}` |
|        - |  9699 | `	/* Push the resume value onto the fiber's operand stack.` |
|        - |  9700 | `	 * This makes it appear as the return value of Fiber::suspend() from` |
|        - |  9701 | `	 * the fiber's perspective. nTos was saved one below the return-value slot. */` |
|      100 |  9702 | `	if( pResumeValue ){` |
|       40 |  9703 | `		PH7_MemObjStore(pResumeValue, &pCtx->pStack[pCtx->nTos + 1]);` |
|       21 |  9704 | `	}else{` |
|       62 |  9705 | `		PH7_MemObjRelease(&pCtx->pStack[pCtx->nTos + 1]);` |
|        - |  9706 | `	}` |
|      100 |  9707 | `	pCtx->nTos++;` |
|        - |  9708 | `	/* Re-attach the fiber's frame to the VM frame chain */` |
|      100 |  9709 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      100 |  9710 | `	pVm->pFrame = pCtx->pFrame;` |
|        - |  9711 | `	/* Save and set the active context */` |
|      100 |  9712 | `	pOldCtx = pVm->pActiveCtx;` |
|      100 |  9713 | `	pVm->pActiveCtx = pCtx;` |
|      100 |  9714 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      100 |  9715 | `	pVm->nRecursionDepth++;` |
|        - |  9716 | `	/* Resume execution from saved PC */` |
|      100 |  9717 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|       49 |  9718 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|       98 |  9719 | `		pCtx->pFunc->nReturnType > 0 ? pCtx->pFunc : 0);` |
|      100 |  9720 | `	pVm->nRecursionDepth--;` |
|        - |  9721 | `	/* Restore the previous context */` |
|      100 |  9722 | `	pVm->pActiveCtx = pOldCtx;` |
|      100 |  9723 | `	if( rc == PH7_SUSPEND ){` |
|        - |  9724 | `		/* Fiber suspended again. Detach the fiber's frame. */` |
|       64 |  9725 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       64 |  9726 | `		pCtx->pFrame->pParent = 0;` |
|       64 |  9727 | `		if( pResult ){` |
|       18 |  9728 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|        8 |  9729 | `		}` |
|       64 |  9730 | `		return SXRET_OK;` |
|        - |  9731 | `	}` |
|        - |  9732 | `	/* Detach frame */` |
|       38 |  9733 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|       38 |  9734 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|       38 |  9735 | `		pCtx->pFrame->pParent = 0;` |
|       18 |  9736 | `	}` |
|       38 |  9737 | `	if( rc == PH7_ABORT ){` |
|      ! 0 |  9738 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9739 | `		return PH7_ABORT;` |
|        - |  9740 | `	}` |
|       38 |  9741 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 |  9742 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 |  9743 | `		return PH7_EXCEPTION;` |
|        - |  9744 | `	}` |
|        - |  9745 | `	/* Normal completion */` |
|       38 |  9746 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|       38 |  9747 | `	if( pResult ){` |
|       20 |  9748 | `		PH7_MemObjStore(&pCtx->sRetValue, pResult);` |
|        9 |  9749 | `	}` |
|       38 |  9750 | `	return SXRET_OK;` |
|       51 |  9751 |  |
|        - |  9752 | `/*` |
|        - |  9753 | ` * Release an execution context and all its resources.` |
|        - |  9754 | ` */` |
|        4 |  9755 | `static void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        1 |  9756 |  |
|        5 |  9757 | `	if( pCtx == 0 ){` |
|      ! 0 |  9758 | `		return;` |
|        - |  9759 | `	}` |
|        5 |  9760 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - |  9761 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 |  9762 | `		return;` |
|        - |  9763 | `	}` |
|        5 |  9764 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - |  9765 | `	/* Release values */` |
|        5 |  9766 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        5 |  9767 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|        - |  9768 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|        5 |  9769 | `	if( pCtx->pFrame ){` |
|        - |  9770 | `		VmSlot *aSlot;` |
|        - |  9771 | `		sxu32 n;` |
|        - |  9772 | `		/* Free local variables */` |
|        5 |  9773 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sLocal);` |
|       11 |  9774 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sLocal); ++n ){` |
|        7 |  9775 | `			PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|        4 |  9776 | `		}` |
|        - |  9777 | `		/* Remove local references */` |
|        5 |  9778 | `		aSlot = (VmSlot *)SySetBasePtr(&pCtx->pFrame->sRef);` |
|       11 |  9779 | `		for( n = 0; n < SySetUsed(&pCtx->pFrame->sRef); ++n ){` |
|        7 |  9780 | `			PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|        4 |  9781 | `		}` |
|        5 |  9782 | `		SyHashRelease(&pCtx->pFrame->hVar);` |
|        5 |  9783 | `		SySetRelease(&pCtx->pFrame->sArg);` |
|        5 |  9784 | `		SySetRelease(&pCtx->pFrame->sLocal);` |
|        5 |  9785 | `		SySetRelease(&pCtx->pFrame->sRef);` |
|        5 |  9786 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx->pFrame);` |
|        5 |  9787 | `		pCtx->pFrame = 0;` |
|        2 |  9788 | `	}` |
|        - |  9789 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - |  9790 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - |  9791 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|        5 |  9792 | `	if( pCtx->pStack ){` |
|        5 |  9793 | `		if( pCtx->nTos >= 0 ){` |
|        5 |  9794 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|        9 |  9795 | `			while( pTos >= pCtx->pStack ){` |
|        5 |  9796 | `				PH7_MemObjRelease(pTos);` |
|        5 |  9797 | `				pTos--;` |
|        1 |  9798 | `			}` |
|        2 |  9799 | `		}` |
|        5 |  9800 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|        5 |  9801 | `		pCtx->pStack = 0;` |
|        2 |  9802 | `	}` |
|        - |  9803 | `	/* Free the context itself */` |
|        5 |  9804 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|        3 |  9805 |  |
|        - |  9806 | `/*` |
|        - |  9807 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - |  9808 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - |  9809 | ` */` |
|       90 |  9810 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        2 |  9811 |  |
|        - |  9812 | `	ph7_class_instance *pThis;` |
|        - |  9813 | `	SyString sAttr;` |
|        - |  9814 | `	ph7_value *pAttr;` |
|       92 |  9815 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9816 | `		return 0;` |
|        - |  9817 | `	}` |
|       92 |  9818 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|       92 |  9819 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 |  9820 | `		return 0;` |
|        - |  9821 | `	}` |
|       92 |  9822 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|       92 |  9823 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|       92 |  9824 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|       32 |  9825 | `		return 0;` |
|        - |  9826 | `	}` |
|       62 |  9827 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|       47 |  9828 |  |
|        - |  9829 | `/*` |
|        - |  9830 | ` * Fiber::suspend($value = null) — static method.` |
|        - |  9831 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - |  9832 | ` */` |
|       38 |  9833 | `static int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9834 |  |
|       40 |  9835 | `	ph7_vm *pVm = pCtx->pVm;` |
|       40 |  9836 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 |  9837 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9838 | `			"Cannot suspend outside of a fiber");` |
|        - |  9839 | `	}` |
|       40 |  9840 | `	if( nArg > 0 ){` |
|       40 |  9841 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|       21 |  9842 | `	}else{` |
|      ! 0 |  9843 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - |  9844 | `	}` |
|       40 |  9845 | `	return PH7_SUSPEND;` |
|       21 |  9846 |  |
|        - |  9847 | `/*` |
|        - |  9848 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - |  9849 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - |  9850 | ` * and closure-environment binding happen with the correct argument context.` |
|        - |  9851 | ` */` |
|       24 |  9852 | `static int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 |  9853 |  |
|        - |  9854 | `	ph7_class_instance *pThis;` |
|        - |  9855 | `	ph7_value *pAttr;` |
|        - |  9856 | `	SyString sAttrName;` |
|       26 |  9857 | `	if( nArg < 2 ){` |
|      ! 0 |  9858 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9859 | `			"Fiber::__construct() expects a callable argument");` |
|        - |  9860 | `	}` |
|       26 |  9861 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 |  9862 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9863 | `			"Fiber::__construct(): invalid $this");` |
|        - |  9864 | `	}` |
|       26 |  9865 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       26 |  9866 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 |  9867 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9868 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - |  9869 | `	}` |
|        - |  9870 | `	/* Basic validation: callable must be a string or closure (object) */` |
|       26 |  9871 | `	if( (apArg[1]->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9872 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9873 | `			"Fiber::__construct() expects a callable (string or closure)");` |
|        - |  9874 | `	}` |
|        - |  9875 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|       26 |  9876 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9877 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 |  9878 | `	if( pAttr ){` |
|       26 |  9879 | `		PH7_MemObjStore(apArg[1], pAttr);` |
|       12 |  9880 | `	}` |
|       26 |  9881 | `	return PH7_OK;` |
|       14 |  9882 |  |
|        - |  9883 | `/*` |
|        - |  9884 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - |  9885 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - |  9886 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - |  9887 | ` * so that start() can bind it as $this for the closure environment.` |
|        - |  9888 | ` */` |
|       24 |  9889 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - |  9890 | `	ph7_class_instance **ppThis)` |
|        2 |  9891 |  |
|       26 |  9892 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  9893 | `	ph7_value *pCallable;` |
|        - |  9894 | `	SyString sAttrName;` |
|       26 |  9895 | `	*ppThis = 0;` |
|       26 |  9896 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       26 |  9897 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|       26 |  9898 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|      ! 0 |  9899 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 |  9900 | `		return 0;` |
|        - |  9901 | `	}` |
|       26 |  9902 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - |  9903 | `		/* String callable — look up in user functions with overload support */` |
|        - |  9904 | `		SyString sName;` |
|        - |  9905 | `		SyHashEntry *pEntry;` |
|        - |  9906 | `		ph7_vm_func *pFunc;` |
|       26 |  9907 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       26 |  9908 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|       26 |  9909 | `		if( pEntry == 0 ){` |
|      ! 0 |  9910 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|      ! 0 |  9911 | `				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);` |
|      ! 0 |  9912 | `			return 0;` |
|        - |  9913 | `		}` |
|       26 |  9914 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|       26 |  9915 | `		return pFunc;` |
|      ! 0 |  9916 | `	}else{` |
|        - |  9917 | `		/* Object callable (closure) — resolve __invoke method */` |
|      ! 0 |  9918 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 |  9919 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - |  9920 | `			sizeof("__invoke") - 1);` |
|      ! 0 |  9921 | `		if( pMethod == 0 ){` |
|      ! 0 |  9922 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - |  9923 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 |  9924 | `			return 0;` |
|        - |  9925 | `		}` |
|      ! 0 |  9926 | `		*ppThis = pClosure;` |
|      ! 0 |  9927 | `		return &pMethod->sFunc;` |
|        - |  9928 | `	}` |
|       14 |  9929 |  |
|        - |  9930 | `/*` |
|        - |  9931 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - |  9932 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - |  9933 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - |  9934 | ` */` |
|       46 |  9935 | `static sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - |  9936 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg)` |
|        2 |  9937 |  |
|       48 |  9938 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - |  9939 | `	ph7_vm_func_arg *aFormalArg;` |
|        - |  9940 | `	sxu32 nFormal, n;` |
|        - |  9941 | `	VmSlot sSlot;` |
|        - |  9942 | `	sxi32 rc;` |
|        - |  9943 | `	/* Install $this for closure/method callables */` |
|       48 |  9944 | `	if( pClosureThis ){` |
|        - |  9945 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      ! 0 |  9946 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      ! 0 |  9947 | `		if( pObj ){` |
|      ! 0 |  9948 | `			pObj->x.pOther = pClosureThis;` |
|      ! 0 |  9949 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      ! 0 |  9950 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      ! 0 |  9951 | `		}` |
|      ! 0 |  9952 | `	}` |
|        - |  9953 | `	/* Install static variables */` |
|       48 |  9954 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - |  9955 | `		ph7_vm_func_static_var *aStatic;` |
|        - |  9956 | `		ph7_value *pVal;` |
|      ! 0 |  9957 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 |  9958 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 |  9959 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 |  9960 | `			if( pVal ){` |
|      ! 0 |  9961 | `				sSlot.pUserData = 0;` |
|      ! 0 |  9962 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 |  9963 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 |  9964 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 |  9965 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 |  9966 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal);` |
|      ! 0 |  9967 | `				}` |
|      ! 0 |  9968 | `			}` |
|      ! 0 |  9969 | `		}` |
|      ! 0 |  9970 | `	}` |
|        - |  9971 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|       48 |  9972 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       48 |  9973 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       66 |  9974 | `	for( n = 0; n < nFormal; n++ ){` |
|        - |  9975 | `		ph7_value *pObj;` |
|       20 |  9976 | `		if( n < (sxu32)nArg ){` |
|        - |  9977 | `			/* Argument provided — install with type casting */` |
|       20 |  9978 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       20 |  9979 | `			if( pObj ){` |
|       20 |  9980 | `				PH7_MemObjStore(apArg[n], pObj);` |
|        - |  9981 | `				/* Type casting */` |
|       20 |  9982 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 |  9983 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 |  9984 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 |  9985 | `						if( xCast ){` |
|      ! 0 |  9986 | `							xCast(pObj);` |
|      ! 0 |  9987 | `						}` |
|      ! 0 |  9988 | `					}` |
|      ! 0 |  9989 | `				}` |
|       20 |  9990 | `				sSlot.nIdx = pObj->nIdx;` |
|       20 |  9991 | `				sSlot.pUserData = 0;` |
|       20 |  9992 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       11 |  9993 | `			}` |
|        9 |  9994 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - |  9995 | `			/* Default value */` |
|      ! 0 |  9996 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      ! 0 |  9997 | `			if( pObj ){` |
|      ! 0 |  9998 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj);` |
|      ! 0 |  9999 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 10000 | `					return rc;` |
|        - | 10001 | `				}` |
|      ! 0 | 10002 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH ){` |
|      ! 0 | 10003 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|      ! 0 | 10004 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|      ! 0 | 10005 | `						if( xCast ){` |
|      ! 0 | 10006 | `							xCast(pObj);` |
|      ! 0 | 10007 | `						}` |
|      ! 0 | 10008 | `					}` |
|      ! 0 | 10009 | `				}` |
|      ! 0 | 10010 | `				sSlot.nIdx = pObj->nIdx;` |
|      ! 0 | 10011 | `				sSlot.pUserData = 0;` |
|      ! 0 | 10012 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      ! 0 | 10013 | `			}` |
|      ! 0 | 10014 | `		}` |
|       11 | 10015 | `	}` |
|        - | 10016 | `	/* Install closure environment (captured variables) */` |
|       48 | 10017 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 10018 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 10019 | `		ph7_value *pValue;` |
|        - | 10020 | `		sxu32 iEnv;` |
|        3 | 10021 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|        9 | 10022 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|        7 | 10023 | `			pEnv = &aEnv[iEnv];` |
|        7 | 10024 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|        3 | 10025 | `				continue;` |
|        - | 10026 | `			}` |
|        5 | 10027 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|        5 | 10028 | `			if( pValue == 0 ){` |
|      ! 0 | 10029 | `				continue;` |
|        - | 10030 | `			}` |
|        5 | 10031 | `			PH7_MemObjRelease(pValue);` |
|        5 | 10032 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|        3 | 10033 | `		}` |
|        1 | 10034 | `	}` |
|       48 | 10035 | `	return SXRET_OK;` |
|       25 | 10036 |  |
|        - | 10037 | `/*` |
|        - | 10038 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 10039 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 10040 | ` * apArg[0] = $this, apArg[1] = func_get_args() array` |
|        - | 10041 | ` */` |
|       26 | 10042 | `static int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10043 |  |
|       28 | 10044 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10045 | `	ph7_class_instance *pThis;` |
|        - | 10046 | `	ph7_class_instance *pClosureThis;` |
|        - | 10047 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10048 | `	ph7_vm_func *pFunc;` |
|        - | 10049 | `	ph7_value sResult;` |
|        - | 10050 | `	ph7_value *pCtxAttr;` |
|        - | 10051 | `	SyString sAttrName;` |
|        - | 10052 | `	sxi32 rc;` |
|       28 | 10053 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10054 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 10055 | `	}` |
|       28 | 10056 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10057 | `	/* Check if already started (has a __ctx) */` |
|       28 | 10058 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       28 | 10059 | `	if( pExecCtx != 0 ){` |
|        3 | 10060 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10061 | `			"Cannot start a fiber that has already been started");` |
|        - | 10062 | `	}` |
|        - | 10063 | `	/* Resolve callable */` |
|       26 | 10064 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|       26 | 10065 | `	if( pFunc == 0 ){` |
|      ! 0 | 10066 | `		return PH7_EXCEPTION;` |
|        - | 10067 | `	}` |
|        - | 10068 | `	/* Create execution context now that we know the function */` |
|       26 | 10069 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|       26 | 10070 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10071 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10072 | `			"Fiber::start(): out of memory");` |
|        - | 10073 | `	}` |
|        - | 10074 | `	/* Store context in $this->__ctx */` |
|       26 | 10075 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       26 | 10076 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       26 | 10077 | `	if( pCtxAttr ){` |
|       26 | 10078 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       26 | 10079 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       12 | 10080 | `	}` |
|        - | 10081 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 10082 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 10083 | `	 * into the fiber's frame, not the caller's. */` |
|       26 | 10084 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|       26 | 10085 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 10086 | `	/* Unpack the args array and install into the frame */` |
|        - | 10087 | `	{` |
|       26 | 10088 | `		ph7_value **apValues = 0;` |
|       26 | 10089 | `		int nActual = 0;` |
|       26 | 10090 | `		if( nArg >= 2 && (apArg[1]->iFlags & MEMOBJ_HASHMAP) ){` |
|       26 | 10091 | `			ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|        - | 10092 | `			ph7_hashmap_node *pNode;` |
|       26 | 10093 | `			sxu32 nCount = pMap->nEntry;` |
|       26 | 10094 | `			if( nCount > 0 ){` |
|        3 | 10095 | `				sxu32 idx = 0;` |
|        4 | 10096 | `				apValues = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        1 | 10097 | `					nCount * sizeof(ph7_value *));` |
|        3 | 10098 | `				if( apValues ){` |
|        3 | 10099 | `					pNode = pMap->pFirst;` |
|        7 | 10100 | `					while( pNode && idx < nCount ){` |
|        5 | 10101 | `						apValues[idx] = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|        5 | 10102 | `						idx++;` |
|        5 | 10103 | `						pNode = pNode->pPrev;` |
|        1 | 10104 | `					}` |
|        3 | 10105 | `					nActual = (int)idx;` |
|        1 | 10106 | `				}` |
|        1 | 10107 | `			}` |
|       12 | 10108 | `		}` |
|       26 | 10109 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues);` |
|       26 | 10110 | `		if( apValues ){` |
|        3 | 10111 | `			SyMemBackendFree(&pVm->sAllocator, apValues);` |
|        1 | 10112 | `		}` |
|        - | 10113 | `	}` |
|        - | 10114 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|       26 | 10115 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|       26 | 10116 | `	pExecCtx->pFrame->pParent = 0;` |
|       26 | 10117 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 10118 | `		return PH7_ABORT;` |
|        - | 10119 | `	}` |
|       26 | 10120 | `	PH7_MemObjInit(pVm, &sResult);` |
|       26 | 10121 | `	rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       26 | 10122 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10123 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10124 | `		return PH7_ABORT;` |
|        - | 10125 | `	}` |
|       26 | 10126 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10127 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10128 | `		return PH7_EXCEPTION;` |
|        - | 10129 | `	}` |
|       26 | 10130 | `	ph7_result_value(pCtx, &sResult);` |
|       26 | 10131 | `	PH7_MemObjRelease(&sResult);` |
|       26 | 10132 | `	return PH7_OK;` |
|       15 | 10133 |  |
|        - | 10134 | `/*` |
|        - | 10135 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 10136 | ` */` |
|       36 | 10137 | `static int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10138 |  |
|       38 | 10139 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10140 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 10141 | `	ph7_value sResult;` |
|        - | 10142 | `	ph7_value *pResumeVal;` |
|        - | 10143 | `	sxi32 rc;` |
|       38 | 10144 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10145 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 10146 | `		return PH7_OK;` |
|        - | 10147 | `	}` |
|       38 | 10148 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|       38 | 10149 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10150 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 10151 | `		return PH7_OK;` |
|        - | 10152 | `	}` |
|       38 | 10153 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10154 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10155 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 10156 | `	}` |
|       36 | 10157 | `	pResumeVal = (nArg > 1) ? apArg[1] : 0;` |
|       36 | 10158 | `	PH7_MemObjInit(pVm, &sResult);` |
|       36 | 10159 | `	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|       36 | 10160 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 10161 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10162 | `		return PH7_ABORT;` |
|        - | 10163 | `	}` |
|       36 | 10164 | `	if( rc == PH7_EXCEPTION ){` |
|      ! 0 | 10165 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 10166 | `		return PH7_EXCEPTION;` |
|        - | 10167 | `	}` |
|       36 | 10168 | `	ph7_result_value(pCtx, &sResult);` |
|       36 | 10169 | `	PH7_MemObjRelease(&sResult);` |
|       36 | 10170 | `	return PH7_OK;` |
|       20 | 10171 |  |
|        - | 10172 | `/*` |
|        - | 10173 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 10174 | ` */` |
|        6 | 10175 | `static int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10176 |  |
|        8 | 10177 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10178 | `	ph7_exec_ctx *pExecCtx;` |
|        8 | 10179 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10180 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10181 | `		return PH7_OK;` |
|        - | 10182 | `	}` |
|        8 | 10183 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        8 | 10184 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 10185 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10186 | `		return PH7_OK;` |
|        - | 10187 | `	}` |
|        8 | 10188 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10189 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10190 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10191 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 10192 | `		}` |
|      ! 0 | 10193 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 10194 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 10195 | `	}` |
|        8 | 10196 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|        8 | 10197 | `	return PH7_OK;` |
|        5 | 10198 |  |
|        - | 10199 | `/*` |
|        - | 10200 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 10201 | ` */` |
|        6 | 10202 | `static int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10203 |  |
|        - | 10204 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10205 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10206 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10207 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|        7 | 10208 | `	return PH7_OK;` |
|        4 | 10209 |  |
|      ! 0 | 10210 | `static int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10211 |  |
|        - | 10212 | `	ph7_exec_ctx *pExecCtx;` |
|      ! 0 | 10213 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      ! 0 | 10214 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10215 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|      ! 0 | 10216 | `	return PH7_OK;` |
|      ! 0 | 10217 |  |
|        6 | 10218 | `static int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10219 |  |
|        - | 10220 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10221 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10222 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10223 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|        7 | 10224 | `	return PH7_OK;` |
|        4 | 10225 |  |
|        6 | 10226 | `static int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10227 |  |
|        - | 10228 | `	ph7_exec_ctx *pExecCtx;` |
|        7 | 10229 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|        7 | 10230 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, apArg[0]);` |
|        7 | 10231 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);` |
|        7 | 10232 | `	return PH7_OK;` |
|        4 | 10233 |  |
|        - | 10234 | `/*` |
|        - | 10235 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 10236 | ` */` |
|        4 | 10237 | `static int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10238 |  |
|        5 | 10239 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 10240 | `	ph7_exec_ctx *pExecCtx;` |
|        5 | 10241 | `	if( nArg < 1 ){` |
|      ! 0 | 10242 | `		return PH7_OK;` |
|        - | 10243 | `	}` |
|        5 | 10244 | `	pExecCtx = VmFiberExtractCtx(pVm, apArg[0]);` |
|        5 | 10245 | `	if( pExecCtx ){` |
|        5 | 10246 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 10247 | `		/* Clear the attribute so double-free is prevented */` |
|        5 | 10248 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|        5 | 10249 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10250 | `			SyString sAttrName;` |
|        - | 10251 | `			ph7_value *pAttr;` |
|        5 | 10252 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|        5 | 10253 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|        5 | 10254 | `			if( pAttr ){` |
|        5 | 10255 | `				PH7_MemObjRelease(pAttr);` |
|        2 | 10256 | `			}` |
|        2 | 10257 | `		}` |
|        2 | 10258 | `	}` |
|        5 | 10259 | `	return PH7_OK;` |
|        3 | 10260 |  |
|        - | 10261 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 10262 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 10263 |  |
|        - | 10264 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10265 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 10266 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 10267 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 10268 |  |
|      ! 0 | 10269 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 10270 |  |
|        - | 10271 | `	ph7_class_instance *pThis;` |
|      ! 0 | 10272 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 10273 | `	ph7_exec_ctx *pCtx;` |
|        - | 10274 | `	ph7_vm_func *pFunc;` |
|        - | 10275 | `	ph7_value *pCallable;` |
|        - | 10276 | `	ph7_value *pCtxAttr;` |
|        - | 10277 | `	SyString sAttrName;` |
|        - | 10278 | `	/* Must not already be started */` |
|      ! 0 | 10279 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10280 | `	if( pCtx != 0 ){` |
|      ! 0 | 10281 | `		return SXERR_INVALID;` |
|        - | 10282 | `	}` |
|      ! 0 | 10283 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10284 | `		return SXERR_INVALID;` |
|        - | 10285 | `	}` |
|      ! 0 | 10286 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 10287 | `	/* Get the callable */` |
|      ! 0 | 10288 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 10289 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10290 | `	if( pCallable == 0 ){` |
|      ! 0 | 10291 | `		return SXERR_INVALID;` |
|        - | 10292 | `	}` |
|        - | 10293 | `	/* Resolve callable */` |
|      ! 0 | 10294 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 10295 | `		SyString sName;` |
|        - | 10296 | `		SyHashEntry *pEntry;` |
|      ! 0 | 10297 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|      ! 0 | 10298 | `		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);` |
|      ! 0 | 10299 | `		if( pEntry == 0 ){` |
|      ! 0 | 10300 | `			return SXERR_NOTFOUND;` |
|        - | 10301 | `		}` |
|      ! 0 | 10302 | `		pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      ! 0 | 10303 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10304 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 10305 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 10306 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 10307 | `		if( pMethod == 0 ){` |
|      ! 0 | 10308 | `			return SXERR_INVALID;` |
|        - | 10309 | `		}` |
|      ! 0 | 10310 | `		pClosureThis = pClosure;` |
|      ! 0 | 10311 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 10312 | `	}else{` |
|      ! 0 | 10313 | `		return SXERR_INVALID;` |
|        - | 10314 | `	}` |
|        - | 10315 | `	/* Create context */` |
|      ! 0 | 10316 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 10317 | `	if( pCtx == 0 ){` |
|      ! 0 | 10318 | `		return SXERR_MEM;` |
|        - | 10319 | `	}` |
|        - | 10320 | `	/* Store in __ctx */` |
|      ! 0 | 10321 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10322 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10323 | `	if( pCtxAttr ){` |
|      ! 0 | 10324 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 10325 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 10326 | `	}` |
|        - | 10327 | `	/* Set up frame with args */` |
|      ! 0 | 10328 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 10329 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 10330 | `	VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg);` |
|      ! 0 | 10331 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 10332 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 10333 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 10334 |  |
|      ! 0 | 10335 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 10336 |  |
|      ! 0 | 10337 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10338 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 10339 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 10340 |  |
|      ! 0 | 10341 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10342 |  |
|      ! 0 | 10343 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10344 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 10345 |  |
|      ! 0 | 10346 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10347 |  |
|      ! 0 | 10348 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10349 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 10350 |  |
|      ! 0 | 10351 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 10352 |  |
|      ! 0 | 10353 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 10354 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 10355 | `	return &pCtx->sRetValue;` |
|      ! 0 | 10356 |  |
|        - | 10357 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 10358 | `/*` |
|        - | 10359 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 10360 | ` */` |
|       22 | 10361 | `static ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        2 | 10362 |  |
|        - | 10363 | `	ph7_generator *pGen;` |
|       24 | 10364 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|       24 | 10365 | `	if( pGen == 0 ){` |
|      ! 0 | 10366 | `		return 0;` |
|        - | 10367 | `	}` |
|       24 | 10368 | `	SyZero(pGen, sizeof(ph7_generator));` |
|       24 | 10369 | `	pGen->pCtx = pCtx;` |
|       24 | 10370 | `	pGen->iImplicitKey = 0;` |
|       24 | 10371 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|       24 | 10372 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 10373 | `	/* Link the generator back to the exec context */` |
|       24 | 10374 | `	pCtx->pPrivate = pGen;` |
|       24 | 10375 | `	return pGen;` |
|       13 | 10376 |  |
|        - | 10377 | `/*` |
|        - | 10378 | ` * Release a generator and its execution context.` |
|        - | 10379 | ` */` |
|      ! 0 | 10380 | `static void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|      ! 0 | 10381 |  |
|      ! 0 | 10382 | `	if( pGen == 0 ){` |
|      ! 0 | 10383 | `		return;` |
|        - | 10384 | `	}` |
|      ! 0 | 10385 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      ! 0 | 10386 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      ! 0 | 10387 | `	if( pGen->pCtx ){` |
|      ! 0 | 10388 | `		pGen->pCtx->pPrivate = 0;` |
|      ! 0 | 10389 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      ! 0 | 10390 | `		pGen->pCtx = 0;` |
|      ! 0 | 10391 | `	}` |
|      ! 0 | 10392 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      ! 0 | 10393 |  |
|        - | 10394 | `/*` |
|        - | 10395 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 10396 | ` */` |
|      236 | 10397 | `static ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        2 | 10398 |  |
|        - | 10399 | `	ph7_class_instance *pThis;` |
|        - | 10400 | `	SyString sAttr;` |
|        - | 10401 | `	ph7_value *pAttr;` |
|      238 | 10402 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 10403 | `		return 0;` |
|        - | 10404 | `	}` |
|      238 | 10405 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|      238 | 10406 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 10407 | `		return 0;` |
|        - | 10408 | `	}` |
|      238 | 10409 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      238 | 10410 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      238 | 10411 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 10412 | `		return 0;` |
|        - | 10413 | `	}` |
|      238 | 10414 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      120 | 10415 |  |
|        - | 10416 | `/*` |
|        - | 10417 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 10418 | ` */` |
|       22 | 10419 | `static int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10420 |  |
|        - | 10421 | `	ph7_generator *pGen;` |
|        - | 10422 | `	sxi32 rc;` |
|       24 | 10423 | `	if( nArg < 1 ) return PH7_OK;` |
|       24 | 10424 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       24 | 10425 | `	if( pGen == 0 ) return PH7_OK;` |
|       24 | 10426 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|       24 | 10427 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       24 | 10428 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       24 | 10429 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       11 | 10430 | `	}` |
|       24 | 10431 | `	return PH7_OK;` |
|       13 | 10432 |  |
|        - | 10433 | `/*` |
|        - | 10434 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 10435 | ` */` |
|       68 | 10436 | `static int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10437 |  |
|        - | 10438 | `	ph7_generator *pGen;` |
|       70 | 10439 | `	if( nArg < 1 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       70 | 10440 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10441 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|       70 | 10442 | `	return PH7_OK;` |
|       36 | 10443 |  |
|        - | 10444 | `/*` |
|        - | 10445 | ` * Generator::current() — return the last yielded value.` |
|        - | 10446 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10447 | ` */` |
|       68 | 10448 | `static int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10449 |  |
|        - | 10450 | `	ph7_generator *pGen;` |
|        - | 10451 | `	sxi32 rc;` |
|       70 | 10452 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10453 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       70 | 10454 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       70 | 10455 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10456 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10457 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10458 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10459 | `	}` |
|       70 | 10460 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       70 | 10461 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       36 | 10462 | `	}else{` |
|      ! 0 | 10463 | `		ph7_result_null(pCtx);` |
|        - | 10464 | `	}` |
|       70 | 10465 | `	return PH7_OK;` |
|       36 | 10466 |  |
|        - | 10467 | `/*` |
|        - | 10468 | ` * Generator::key() — return the last yielded key.` |
|        - | 10469 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 10470 | ` */` |
|       12 | 10471 | `static int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10472 |  |
|        - | 10473 | `	ph7_generator *pGen;` |
|        - | 10474 | `	sxi32 rc;` |
|       13 | 10475 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10476 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       13 | 10477 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       13 | 10478 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10479 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|      ! 0 | 10480 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      ! 0 | 10481 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      ! 0 | 10482 | `	}` |
|       13 | 10483 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       13 | 10484 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|        7 | 10485 | `	}else{` |
|      ! 0 | 10486 | `		ph7_result_null(pCtx);` |
|        - | 10487 | `	}` |
|       13 | 10488 | `	return PH7_OK;` |
|        7 | 10489 |  |
|        - | 10490 | `/*` |
|        - | 10491 | ` * Generator::next() — advance to the next yield point.` |
|        - | 10492 | ` */` |
|       60 | 10493 | `static int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 10494 |  |
|        - | 10495 | `	ph7_generator *pGen;` |
|        - | 10496 | `	sxi32 rc;` |
|       62 | 10497 | `	if( nArg < 1 ) return PH7_OK;` |
|       62 | 10498 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|       62 | 10499 | `	if( pGen == 0 ) return PH7_OK;` |
|       62 | 10500 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 10501 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|       62 | 10502 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       62 | 10503 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       32 | 10504 | `	}else{` |
|      ! 0 | 10505 | `		return PH7_OK;` |
|        - | 10506 | `	}` |
|       62 | 10507 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       62 | 10508 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       62 | 10509 | `	return PH7_OK;` |
|       32 | 10510 |  |
|        - | 10511 | `/*` |
|        - | 10512 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 10513 | ` */` |
|        4 | 10514 | `static int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10515 |  |
|        - | 10516 | `	ph7_generator *pGen;` |
|        - | 10517 | `	ph7_value *pSendVal;` |
|        - | 10518 | `	sxi32 rc;` |
|        5 | 10519 | `	if( nArg < 1 ) return PH7_OK;` |
|        5 | 10520 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        5 | 10521 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        5 | 10522 | `	pSendVal = (nArg > 1) ? apArg[1] : 0;` |
|        5 | 10523 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){` |
|        - | 10524 | `		/* First send starts the generator; sent value is ignored per PHP semantics */` |
|      ! 0 | 10525 | `		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);` |
|        5 | 10526 | `	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        5 | 10527 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|        3 | 10528 | `	}else{` |
|      ! 0 | 10529 | `		ph7_result_null(pCtx);` |
|      ! 0 | 10530 | `		return PH7_OK;` |
|        - | 10531 | `	}` |
|        5 | 10532 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|        5 | 10533 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        5 | 10534 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 10535 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|        2 | 10536 | `	}else{` |
|        3 | 10537 | `		ph7_result_null(pCtx);` |
|        - | 10538 | `	}` |
|        5 | 10539 | `	return PH7_OK;` |
|        3 | 10540 |  |
|        - | 10541 | `/*` |
|        - | 10542 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 10543 | ` *` |
|        - | 10544 | ` * TODO: Full PHP semantics require injecting the exception at the yield` |
|        - | 10545 | ` * point so the generator's own try/catch can handle it. This needs a` |
|        - | 10546 | ` * pending-exception field on ph7_exec_ctx and a check at the start of` |
|        - | 10547 | ` * VmByteCodeExec resume. For now we close the generator and propagate` |
|        - | 10548 | ` * the exception to the caller.` |
|        - | 10549 | ` */` |
|      ! 0 | 10550 | `static int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10551 |  |
|        - | 10552 | `	ph7_generator *pGen;` |
|        - | 10553 | `	const char *zMsg;` |
|        - | 10554 | `	int nLen;` |
|      ! 0 | 10555 | `	if( nArg < 2 ) return PH7_OK;` |
|      ! 0 | 10556 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10557 | `	if( pGen == 0 ) return PH7_OK;` |
|      ! 0 | 10558 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED \|\|` |
|      ! 0 | 10559 | `		pGen->pCtx->iState == PH7_CTX_STATE_CLOSED ){` |
|      ! 0 | 10560 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10561 | `			"Cannot throw into a closed generator");` |
|        - | 10562 | `	}` |
|        - | 10563 | `	/* Close the generator. Re-throw the exception properly via` |
|        - | 10564 | `	 * PH7_VmThrowException so that VM_FRAME_THROW is set and the` |
|        - | 10565 | `	 * exception dispatch path works correctly. Extract the message` |
|        - | 10566 | `	 * from the passed exception object if possible. */` |
|      ! 0 | 10567 | `	pGen->pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      ! 0 | 10568 | `	zMsg = "Unknown exception thrown into generator";` |
|      ! 0 | 10569 | `	nLen = 0;` |
|      ! 0 | 10570 | `	if( apArg[1]->iFlags & MEMOBJ_OBJ ){` |
|        - | 10571 | `		/* Try to get the exception's message */` |
|        - | 10572 | `		SyString sAttr;` |
|        - | 10573 | `		ph7_value *pMsgAttr;` |
|      ! 0 | 10574 | `		SyStringInitFromBuf(&sAttr, "message", 7);` |
|      ! 0 | 10575 | `		pMsgAttr = PH7_ClassInstanceFetchAttr(` |
|      ! 0 | 10576 | `			(ph7_class_instance *)apArg[1]->x.pOther, &sAttr);` |
|      ! 0 | 10577 | `		if( pMsgAttr && (pMsgAttr->iFlags & MEMOBJ_STRING) ){` |
|      ! 0 | 10578 | `			zMsg = (const char *)SyBlobData(&pMsgAttr->sBlob);` |
|      ! 0 | 10579 | `			nLen = (int)SyBlobLength(&pMsgAttr->sBlob);` |
|      ! 0 | 10580 | `		}` |
|      ! 0 | 10581 | `	}else if( apArg[1]->iFlags & MEMOBJ_STRING ){` |
|      ! 0 | 10582 | `		zMsg = (const char *)SyBlobData(&apArg[1]->sBlob);` |
|      ! 0 | 10583 | `		nLen = (int)SyBlobLength(&apArg[1]->sBlob);` |
|      ! 0 | 10584 | `	}` |
|      ! 0 | 10585 | `	(void)nLen;` |
|      ! 0 | 10586 | `	return PH7_VmThrowException(pCtx, "Exception", "%s", zMsg);` |
|      ! 0 | 10587 |  |
|        - | 10588 | `/*` |
|        - | 10589 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 10590 | ` */` |
|        2 | 10591 | `static int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 10592 |  |
|        - | 10593 | `	ph7_generator *pGen;` |
|        3 | 10594 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10595 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|        3 | 10596 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        3 | 10597 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|      ! 0 | 10598 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 10599 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 10600 | `	}` |
|        3 | 10601 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|        3 | 10602 | `	return PH7_OK;` |
|        2 | 10603 |  |
|        - | 10604 | `/*` |
|        - | 10605 | ` * Generator::__destruct() — clean up.` |
|        - | 10606 | ` */` |
|      ! 0 | 10607 | `static int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 10608 |  |
|        - | 10609 | `	ph7_generator *pGen;` |
|      ! 0 | 10610 | `	if( nArg < 1 ) return PH7_OK;` |
|      ! 0 | 10611 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, apArg[0]);` |
|      ! 0 | 10612 | `	if( pGen ){` |
|      ! 0 | 10613 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      ! 0 | 10614 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 10615 | `			ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|        - | 10616 | `			SyString sAttrName;` |
|        - | 10617 | `			ph7_value *pAttr;` |
|      ! 0 | 10618 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 10619 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 10620 | `			if( pAttr ){` |
|      ! 0 | 10621 | `				PH7_MemObjRelease(pAttr);` |
|      ! 0 | 10622 | `			}` |
|      ! 0 | 10623 | `		}` |
|      ! 0 | 10624 | `	}` |
|      ! 0 | 10625 | `	return PH7_OK;` |
|      ! 0 | 10626 |  |
|        - | 10627 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 10628 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 10629 | `/*` |
|        - | 10630 | ` * Invoke the installed VM output consumer callback to consume` |
|        - | 10631 | ` * the desired message.` |
|        - | 10632 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|        - | 10633 | ` * in 'api.c' for additional information.` |
|        - | 10634 | ` */` |
|      370 | 10635 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|        - | 10636 | `	ph7_vm *pVm,      /* Target VM */` |
|        - | 10637 | `	SyString *pString /* Message to output */` |
|        - | 10638 | `	)` |
|        2 | 10639 |  |
|      372 | 10640 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      372 | 10641 | `	sxi32 rc = SXRET_OK;` |
|        - | 10642 | `	/* Call the output consumer */` |
|      372 | 10643 | `	if( pString->nByte > 0 ){` |
|      372 | 10644 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|      372 | 10645 | `		VmTrackOutput(pVm, pString->nByte);` |
|      185 | 10646 | `	}` |
|      372 | 10647 | `	return rc;` |
|        2 | 10648 |  |
|        - | 10649 | `/*` |
|        - | 10650 | ` * Format a message and invoke the installed VM output consumer` |
|        - | 10651 | ` * callback to consume the formatted message.` |
|        - | 10652 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|        - | 10653 | ` * in 'api.c' for additional information.` |
|        - | 10654 | ` */` |
|        2 | 10655 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|        - | 10656 | `	ph7_vm *pVm,         /* Target VM */` |
|        - | 10657 | `	const char *zFormat, /* Formatted message to output */` |
|        - | 10658 | `	va_list ap           /* Variable list of arguments */` |
|        - | 10659 | `	)` |
|        1 | 10660 |  |
|        3 | 10661 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        3 | 10662 | `	sxi32 rc = SXRET_OK;` |
|        - | 10663 | `	SyBlob sWorker;` |
|        - | 10664 | `	/* Format the message and call the output consumer */` |
|        3 | 10665 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        3 | 10666 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        3 | 10667 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|        - | 10668 | `		/* Consume the formatted message */` |
|        3 | 10669 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        1 | 10670 | `	}` |
|        3 | 10671 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|        - | 10672 | `	/* Release the working buffer */` |
|        3 | 10673 | `	SyBlobRelease(&sWorker);` |
|        3 | 10674 | `	return rc;` |
|        1 | 10675 |  |
|        - | 10676 | `/*` |
|        - | 10677 | ` * Return a string representation of the given PH7 OP code.` |
|        - | 10678 | ` * This function never fail and always return a pointer` |
|        - | 10679 | ` * to a null terminated string.` |
|        - | 10680 | ` */` |
|       12 | 10681 | `static const char * VmInstrToString(sxi32 nOp)` |
|        1 | 10682 |  |
|       13 | 10683 | `	const char *zOp = "Unknown     ";` |
|       13 | 10684 | `	switch(nOp){` |
|        3 | 10685 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|      ! 0 | 10686 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|      ! 0 | 10687 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|        5 | 10688 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|      ! 0 | 10689 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|      ! 0 | 10690 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|      ! 0 | 10691 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|      ! 0 | 10692 | `	case PH7_OP_LOAD_CLOSURE:` |
|      ! 0 | 10693 | `		                    zOp = "LOAD_CLOSR "; break;` |
|      ! 0 | 10694 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|      ! 0 | 10695 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|      ! 0 | 10696 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|      ! 0 | 10697 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|      ! 0 | 10698 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|      ! 0 | 10699 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|      ! 0 | 10700 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|      ! 0 | 10701 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|      ! 0 | 10702 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|      ! 0 | 10703 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|      ! 0 | 10704 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|      ! 0 | 10705 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|      ! 0 | 10706 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|      ! 0 | 10707 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|      ! 0 | 10708 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|      ! 0 | 10709 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|      ! 0 | 10710 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|      ! 0 | 10711 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|      ! 0 | 10712 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|      ! 0 | 10713 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|      ! 0 | 10714 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|      ! 0 | 10715 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|      ! 0 | 10716 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|      ! 0 | 10717 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|      ! 0 | 10718 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|      ! 0 | 10719 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|      ! 0 | 10720 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|      ! 0 | 10721 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|      ! 0 | 10722 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|      ! 0 | 10723 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|      ! 0 | 10724 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|      ! 0 | 10725 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|      ! 0 | 10726 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|      ! 0 | 10727 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|      ! 0 | 10728 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|      ! 0 | 10729 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|      ! 0 | 10730 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|      ! 0 | 10731 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|      ! 0 | 10732 | `	case PH7_OP_STORE_IDX_REF:` |
|      ! 0 | 10733 | `		                    zOp = "STORE_IDX_R"; break;` |
|      ! 0 | 10734 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|      ! 0 | 10735 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|        3 | 10736 | `	case PH7_OP_NSSWITCH:   zOp = "NSSWITCH   "; break;` |
|      ! 0 | 10737 | `	case PH7_OP_USECONST:   zOp = "USECONST   "; break;` |
|      ! 0 | 10738 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|      ! 0 | 10739 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|      ! 0 | 10740 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|      ! 0 | 10741 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|      ! 0 | 10742 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|      ! 0 | 10743 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|      ! 0 | 10744 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|      ! 0 | 10745 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|      ! 0 | 10746 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|      ! 0 | 10747 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|      ! 0 | 10748 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|      ! 0 | 10749 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|      ! 0 | 10750 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|      ! 0 | 10751 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|      ! 0 | 10752 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|      ! 0 | 10753 | `	case PH7_OP_SEQ:        zOp = "SEQ        "; break;` |
|      ! 0 | 10754 | `	case PH7_OP_SNE:        zOp = "SNE        "; break;` |
|      ! 0 | 10755 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|      ! 0 | 10756 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|      ! 0 | 10757 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|      ! 0 | 10758 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|      ! 0 | 10759 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|      ! 0 | 10760 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|      ! 0 | 10761 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|      ! 0 | 10762 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|      ! 0 | 10763 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|      ! 0 | 10764 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|      ! 0 | 10765 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|      ! 0 | 10766 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|      ! 0 | 10767 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|        5 | 10768 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|      ! 0 | 10769 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|      ! 0 | 10770 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|      ! 0 | 10771 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|      ! 0 | 10772 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|      ! 0 | 10773 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|      ! 0 | 10774 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|      ! 0 | 10775 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|      ! 0 | 10776 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|      ! 0 | 10777 | `	case PH7_OP_LOAD_EXCEPTION:` |
|      ! 0 | 10778 | `		                    zOp = "LOAD_EXCEP "; break;` |
|      ! 0 | 10779 | `	case PH7_OP_POP_EXCEPTION:` |
|      ! 0 | 10780 | `		                    zOp = "POP_EXCEP  "; break;` |
|      ! 0 | 10781 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|      ! 0 | 10782 | `	case PH7_OP_FOREACH_INIT:` |
|      ! 0 | 10783 | `		                    zOp = "4EACH_INIT "; break;` |
|      ! 0 | 10784 | `	case PH7_OP_FOREACH_STEP:` |
|      ! 0 | 10785 | `						    zOp = "4EACH_STEP "; break;` |
|      ! 0 | 10786 | `	default:` |
|      ! 0 | 10787 | `		break;` |
|        - | 10788 | `	}` |
|       13 | 10789 | `	return zOp;` |
|        1 | 10790 |  |
|        - | 10791 | `/*` |
|        - | 10792 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|        - | 10793 | ` * The xConsumer() callback which is an used defined function` |
|        - | 10794 | ` * is responsible of consuming the generated dump.` |
|        - | 10795 | ` */` |
|        2 | 10796 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|        - | 10797 | `	ph7_vm *pVm,            /* Target VM */` |
|        - | 10798 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|        - | 10799 | `	void *pUserData         /* Last argument to xConsumer() */` |
|        - | 10800 | `	)` |
|        1 | 10801 |  |
|        - | 10802 | `	sxi32 rc;` |
|        3 | 10803 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|        3 | 10804 | `	return rc;` |
|        1 | 10805 |  |
|        - | 10806 | `/*` |
|        - | 10807 | ` * Default constant expansion callback used by the 'const' statement if used` |
|        - | 10808 | ` * outside a class body [i.e: global or function scope].` |
|        - | 10809 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|        - | 10810 | ` * in 'compile.c' for additional information.` |
|        - | 10811 | ` */` |
|       14 | 10812 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|        1 | 10813 |  |
|       15 | 10814 | `	SySet *pByteCode = (SySet *)pUserData;` |
|        - | 10815 | `	/* Evaluate and expand constant value */` |
|       15 | 10816 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal);` |
|       15 | 10817 |  |
|        - | 10818 | `/*` |
|        - | 10819 | ` * Section:` |
|        - | 10820 | ` *  Function handling functions.` |
|        - | 10821 | ` * Status:` |
|        - | 10822 | ` *    Stable.` |
|        - | 10823 | ` */` |
|        - | 10824 | `/*` |
|        - | 10825 | ` * int func_num_args(void)` |
|        - | 10826 | ` *   Returns the number of arguments passed to the function.` |
|        - | 10827 | ` * Parameters` |
|        - | 10828 | ` *   None.` |
|        - | 10829 | ` * Return` |
|        - | 10830 | ` *  Total number of arguments passed into the current user-defined function` |
|        - | 10831 | ` *  or -1 if called from the globe scope.` |
|        - | 10832 | ` */` |
|      960 | 10833 | `static int vm_builtin_func_num_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10834 |  |
|        - | 10835 | `	VmFrame *pFrame;` |
|        - | 10836 | `	ph7_vm *pVm;` |
|        - | 10837 | `	/* Point to the target VM */` |
|      962 | 10838 | `	pVm = pCtx->pVm;` |
|        - | 10839 | `	/* Current frame */` |
|      962 | 10840 | `	pFrame = pVm->pFrame;` |
|      962 | 10841 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|      962 | 10842 | `	if( pFrame->pParent == 0 ){` |
|      ! 0 | 10843 | `		SXUNUSED(nArg);` |
|      ! 0 | 10844 | `		SXUNUSED(apArg);` |
|        - | 10845 | `		/* Global frame,return -1 */` |
|      ! 0 | 10846 | `		ph7_result_int(pCtx,-1);` |
|      ! 0 | 10847 | `		return SXRET_OK;` |
|        - | 10848 | `	}` |
|        - | 10849 | `	/* Total number of arguments passed to the enclosing function */` |
|      962 | 10850 | `	nArg = (int)SySetUsed(&pFrame->sArg);` |
|      962 | 10851 | `	ph7_result_int(pCtx,nArg);` |
|      962 | 10852 | `	return SXRET_OK;` |
|      482 | 10853 |  |
|        - | 10854 | `/*` |
|        - | 10855 | ` * value func_get_arg(int $arg_num)` |
|        - | 10856 | ` *   Return an item from the argument list.` |
|        - | 10857 | ` * Parameters` |
|        - | 10858 | ` *  Argument number(index start from zero).` |
|        - | 10859 | ` * Return` |
|        - | 10860 | ` *  Returns the specified argument or FALSE on error.` |
|        - | 10861 | ` */` |
|       22 | 10862 | `static int vm_builtin_func_get_arg(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10863 |  |
|       24 | 10864 | `	ph7_value *pObj = 0;` |
|       24 | 10865 | `	VmSlot *pSlot = 0;` |
|        - | 10866 | `	VmFrame *pFrame;` |
|        - | 10867 | `	ph7_vm *pVm;` |
|        - | 10868 | `	/* Point to the target VM */` |
|       24 | 10869 | `	pVm = pCtx->pVm;` |
|        - | 10870 | `	/* Current frame */` |
|       24 | 10871 | `	pFrame = pVm->pFrame;` |
|       24 | 10872 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       24 | 10873 | `	if( nArg < 1 \|\| pFrame->pParent == 0 ){` |
|        - | 10874 | `		/* Global frame or Missing arguments,return FALSE */` |
|        3 | 10875 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|        3 | 10876 | `		ph7_result_bool(pCtx,0);` |
|        3 | 10877 | `		return SXRET_OK;` |
|        - | 10878 | `	}` |
|        - | 10879 | `	/* Extract the desired index */` |
|       21 | 10880 | `	nArg = ph7_value_to_int(apArg[0]);` |
|       21 | 10881 | `	if( nArg < 0 \|\| nArg >= (int)SySetUsed(&pFrame->sArg) ){` |
|        - | 10882 | `		/* Invalid index,return FALSE */` |
|      ! 0 | 10883 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10884 | `		return SXRET_OK;` |
|        - | 10885 | `	}` |
|        - | 10886 | `	/* Extract the desired argument */` |
|       21 | 10887 | `	if( (pSlot = (VmSlot *)SySetAt(&pFrame->sArg,(sxu32)nArg)) != 0 ){` |
|       21 | 10888 | `		if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx)) != 0 ){` |
|        - | 10889 | `			/* Return the desired argument */` |
|       21 | 10890 | `			ph7_result_value(pCtx,(ph7_value *)pObj);` |
|       11 | 10891 | `		}else{` |
|        - | 10892 | `			/* No such argument,return false */` |
|      ! 0 | 10893 | `			ph7_result_bool(pCtx,0);` |
|        - | 10894 | `		}` |
|       11 | 10895 | `	}else{` |
|        - | 10896 | `		/* CAN'T HAPPEN */` |
|      ! 0 | 10897 | `		ph7_result_bool(pCtx,0);` |
|        - | 10898 | `	}` |
|       21 | 10899 | `	return SXRET_OK;` |
|       13 | 10900 |  |
|        - | 10901 | `/*` |
|        - | 10902 | ` * array func_get_args_byref(void)` |
|        - | 10903 | ` *   Returns an array comprising a function's argument list.` |
|        - | 10904 | ` * Parameters` |
|        - | 10905 | ` *  None.` |
|        - | 10906 | ` * Return` |
|        - | 10907 | ` *  Returns an array in which each element is a POINTER to the corresponding` |
|        - | 10908 | ` *  member of the current user-defined function's argument list.` |
|        - | 10909 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10910 | ` * NOTE:` |
|        - | 10911 | ` *  Arguments are returned to the array by reference.` |
|        - | 10912 | ` */` |
|        2 | 10913 | `static int vm_builtin_func_get_args_byref(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 10914 |  |
|        - | 10915 | `	ph7_value *pArray;` |
|        - | 10916 | `	VmFrame *pFrame;` |
|        - | 10917 | `	VmSlot *aSlot;` |
|        - | 10918 | `	sxu32 n;` |
|        - | 10919 | `	/* Point to the current frame */` |
|        3 | 10920 | `	pFrame = pCtx->pVm->pFrame;` |
|        3 | 10921 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        3 | 10922 | `	if( pFrame->pParent == 0 ){` |
|        - | 10923 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10924 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10925 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10926 | `		return SXRET_OK;` |
|        - | 10927 | `	}` |
|        - | 10928 | `	/* Create a new array */` |
|        3 | 10929 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 10930 | `	if( pArray == 0 ){` |
|      ! 0 | 10931 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10932 | `		SXUNUSED(apArg);` |
|      ! 0 | 10933 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10934 | `		return SXRET_OK;` |
|        - | 10935 | `	}` |
|        - | 10936 | `	/* Start filling the array with the given arguments (Pass by reference) */` |
|        3 | 10937 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|        5 | 10938 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|        3 | 10939 | `		PH7_HashmapInsertByRef((ph7_hashmap *)pArray->x.pOther,0/*Automatic index assign*/,aSlot[n].nIdx);` |
|        2 | 10940 | `	}` |
|        - | 10941 | `	/* Return the freshly created array */` |
|        3 | 10942 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 10943 | `	return SXRET_OK;` |
|        2 | 10944 |  |
|        - | 10945 | `/*` |
|        - | 10946 | ` * array func_get_args(void)` |
|        - | 10947 | ` *   Returns an array comprising a copy of function's argument list.` |
|        - | 10948 | ` * Parameters` |
|        - | 10949 | ` *  None.` |
|        - | 10950 | ` * Return` |
|        - | 10951 | ` *  Returns an array in which each element is a copy of the corresponding` |
|        - | 10952 | ` *  member of the current user-defined function's argument list.` |
|        - | 10953 | ` *  Otherwise FALSE is returned on failure.` |
|        - | 10954 | ` */` |
|       88 | 10955 | `static int vm_builtin_func_get_args(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 10956 |  |
|       90 | 10957 | `	ph7_value *pObj = 0;` |
|        - | 10958 | `	ph7_value *pArray;` |
|        - | 10959 | `	VmFrame *pFrame;` |
|        - | 10960 | `	VmSlot *aSlot;` |
|        - | 10961 | `	sxu32 n;` |
|        - | 10962 | `	/* Point to the current frame */` |
|       90 | 10963 | `	pFrame = pCtx->pVm->pFrame;` |
|       90 | 10964 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|       90 | 10965 | `	if( pFrame->pParent == 0 ){` |
|        - | 10966 | `		/* Global frame,return FALSE */` |
|      ! 0 | 10967 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Called in the global scope");` |
|      ! 0 | 10968 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10969 | `		return SXRET_OK;` |
|        - | 10970 | `	}` |
|        - | 10971 | `	/* Create a new array */` |
|       90 | 10972 | `	pArray = ph7_context_new_array(pCtx);` |
|       90 | 10973 | `	if( pArray == 0 ){` |
|      ! 0 | 10974 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 10975 | `		SXUNUSED(apArg);` |
|      ! 0 | 10976 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 10977 | `		return SXRET_OK;` |
|        - | 10978 | `	}` |
|        - | 10979 | `	/* Start filling the array with the given arguments */` |
|       90 | 10980 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|      222 | 10981 | `	for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|      134 | 10982 | `		pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|      134 | 10983 | `		if( pObj ){` |
|      134 | 10984 | `			ph7_array_add_elem(pArray,0/* Automatic index assign*/,pObj);` |
|       66 | 10985 | `		}` |
|       68 | 10986 | `	}` |
|        - | 10987 | `	/* Return the freshly created array */` |
|       90 | 10988 | `	ph7_result_value(pCtx,pArray);` |
|       90 | 10989 | `	return SXRET_OK;` |
|       46 | 10990 |  |
|        - | 10991 | `/*` |
|        - | 10992 | ` * bool function_exists(string $name)` |
|        - | 10993 | ` *  Return TRUE if the given function has been defined.` |
|        - | 10994 | ` * Parameters` |
|        - | 10995 | ` *  The name of the desired function.` |
|        - | 10996 | ` * Return` |
|        - | 10997 | ` *  Return TRUE if the given function has been defined.False otherwise` |
|        - | 10998 | ` */` |
|     1716 | 10999 | `static int vm_builtin_func_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11000 |  |
|        - | 11001 | `	const char *zName;` |
|        - | 11002 | `	ph7_vm *pVm;` |
|        - | 11003 | `	int nLen;` |
|        - | 11004 | `	int res;` |
|     1718 | 11005 | `	if( nArg < 1 ){` |
|        - | 11006 | `		/* Missing argument,return FALSE */` |
|      ! 0 | 11007 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11008 | `		return SXRET_OK;` |
|        - | 11009 | `	}` |
|        - | 11010 | `	/* Point to the target VM */` |
|     1718 | 11011 | `	pVm = pCtx->pVm;` |
|        - | 11012 | `	/* Extract the function name */` |
|     1718 | 11013 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11014 | `	/* Assume the function is not defined */` |
|     1718 | 11015 | `	res = 0;` |
|        - | 11016 | `	/* Perform the lookup */` |
|     2574 | 11017 | `	if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|     1712 | 11018 | `		SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11019 | `			/* Function is defined */` |
|      238 | 11020 | `			res = 1;` |
|      118 | 11021 | `	}` |
|     1718 | 11022 | `	ph7_result_bool(pCtx,res);` |
|     1718 | 11023 | `	return SXRET_OK;` |
|      860 | 11024 |  |
|        - | 11025 | `/*` |
|        - | 11026 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11027 | ` * [i.e: Whether it is callable or not].` |
|        - | 11028 | ` * Return TRUE if callable.FALSE otherwise.` |
|        - | 11029 | ` */` |
|    22572 | 11030 | `PH7_PRIVATE int PH7_VmIsCallable(ph7_vm *pVm,ph7_value *pValue,int CallInvoke)` |
|        2 | 11031 |  |
|    22574 | 11032 | `	int res = 0;` |
|    22574 | 11033 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11034 | `		/* PHP semantics: an object is callable iff its class declares __invoke` |
|        - | 11035 | `		 * (inherited methods count). The CallInvoke flag is unused — it` |
|        - | 11036 | `		 * formerly invoked __invoke as a runtime predicate, which is not` |
|        - | 11037 | `		 * standard PHP behavior. */` |
|       20 | 11038 | `		ph7_class_instance *pThis = (ph7_class_instance *)pValue->x.pOther;` |
|       20 | 11039 | `		if( PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1) ){` |
|       18 | 11040 | `			res = 1;` |
|       10 | 11041 | `		}` |
|        9 | 11042 | `		(void)CallInvoke;` |
|    22565 | 11043 | `	}else if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       28 | 11044 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       28 | 11045 | `		if( pMap->nEntry == 2 ){` |
|        - | 11046 | `			ph7_class *pClass;` |
|        - | 11047 | `			ph7_value *pV;` |
|        - | 11048 | `			/* Extract the target class */` |
|       12 | 11049 | `			pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       12 | 11050 | `			if( pV ){` |
|       12 | 11051 | `				pClass = PH7_VmExtractClassFromValue(pVm,pV);` |
|       12 | 11052 | `				if( pClass ){` |
|        - | 11053 | `					ph7_class_method *pMethod;` |
|        - | 11054 | `					/* Extract the target method */` |
|       10 | 11055 | `					pV = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       10 | 11056 | `					if( pV && (pV->iFlags & MEMOBJ_STRING) && SyBlobLength(&pV->sBlob) > 0 ){` |
|        - | 11057 | `						/* Perform the lookup */` |
|       10 | 11058 | `						pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pV->sBlob),SyBlobLength(&pV->sBlob));` |
|       10 | 11059 | `						if( pMethod ){` |
|        - | 11060 | `							/* Method is callable */` |
|        5 | 11061 | `							res = 1;` |
|        2 | 11062 | `						}` |
|        4 | 11063 | `					}` |
|        4 | 11064 | `				}` |
|        5 | 11065 | `			}` |
|        7 | 11066 | `		}` |
|    22543 | 11067 | `	}else if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 11068 | `		const char *zName;` |
|        - | 11069 | `		int nLen;` |
|        - | 11070 | `		/* Extract the name */` |
|     5692 | 11071 | `		zName = ph7_value_to_string(pValue,&nLen);` |
|        - | 11072 | `		/* Perform the lookup */` |
|     5707 | 11073 | `		if( SyHashGet(&pVm->hFunction,(const void *)zName,(sxu32)nLen) != 0 \|\|` |
|       30 | 11074 | `			SyHashGet(&pVm->hHostFunction,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11075 | `				/* Function is callable */` |
|     5674 | 11076 | `				res = 1;` |
|     2836 | 11077 | `		}` |
|     2845 | 11078 | `	}` |
|    22574 | 11079 | `	return res;` |
|        2 | 11080 |  |
|        - | 11081 | `/*` |
|        - | 11082 | ` * bool is_callable(callable $name[,bool $syntax_only = false])` |
|        - | 11083 | ` * Verify that the contents of a variable can be called as a function.` |
|        - | 11084 | ` * Parameters` |
|        - | 11085 | ` * $name` |
|        - | 11086 | ` *    The callback function to check` |
|        - | 11087 | ` * $syntax_only` |
|        - | 11088 | ` *    If set to TRUE the function only verifies that name might be a function or method.` |
|        - | 11089 | ` *    It will only reject simple variables that are not strings, or an array that does` |
|        - | 11090 | ` *    not have a valid structure to be used as a callback. The valid ones are supposed` |
|        - | 11091 | ` *    to have only 2 entries, the first of which is an object or a string, and the second` |
|        - | 11092 | ` *    a string.` |
|        - | 11093 | ` * Return` |
|        - | 11094 | ` *  TRUE if name is callable, FALSE otherwise.` |
|        - | 11095 | ` */` |
|       20 | 11096 | `static int vm_builtin_is_callable(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11097 |  |
|        - | 11098 | `	ph7_vm *pVm;` |
|        - | 11099 | `	int res;` |
|       21 | 11100 | `	if( nArg < 1 ){` |
|        - | 11101 | `		/* Missing arguments,return FALSE */` |
|      ! 0 | 11102 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11103 | `		return SXRET_OK;` |
|        - | 11104 | `	}` |
|        - | 11105 | `	/* Point to the target VM */` |
|       21 | 11106 | `	pVm = pCtx->pVm;` |
|        - | 11107 | `	/* Perform the requested operation */` |
|       21 | 11108 | `	res = PH7_VmIsCallable(pVm,apArg[0],TRUE);` |
|       21 | 11109 | `	ph7_result_bool(pCtx,res);` |
|       21 | 11110 | `	return SXRET_OK;` |
|       11 | 11111 |  |
|        - | 11112 | `/*` |
|        - | 11113 | ` * Hash walker callback used by the [get_defined_functions()] function` |
|        - | 11114 | ` * defined below.` |
|        - | 11115 | ` */` |
|     1228 | 11116 | `static int VmHashFuncStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11117 |  |
|     1229 | 11118 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11119 | `	ph7_value sName;` |
|        - | 11120 | `	sxi32 rc;` |
|        - | 11121 | `	/* Prepare the function name for insertion */` |
|     1229 | 11122 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|     1229 | 11123 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11124 | `	/* Perform the insertion */` |
|     1229 | 11125 | `	rc = ph7_array_add_elem(pArray,0/* Automatic index assign */,&sName); /* Will make it's own copy */` |
|     1229 | 11126 | `	PH7_MemObjRelease(&sName);` |
|     1229 | 11127 | `	return rc;` |
|        1 | 11128 |  |
|        - | 11129 | `/*` |
|        - | 11130 | ` * array get_defined_functions(void)` |
|        - | 11131 | ` *  Returns an array of all defined functions.` |
|        - | 11132 | ` * Parameter` |
|        - | 11133 | ` *  None.` |
|        - | 11134 | ` * Return` |
|        - | 11135 | ` *  Returns an multidimensional array containing a list of all defined functions` |
|        - | 11136 | ` *  both built-in (internal) and user-defined.` |
|        - | 11137 | ` *  The internal functions will be accessible via $arr["internal"], and the user` |
|        - | 11138 | ` *  defined ones using $arr["user"].` |
|        - | 11139 | ` * Note:` |
|        - | 11140 | ` *  NULL is returned on failure.` |
|        - | 11141 | ` */` |
|        2 | 11142 | `static int vm_builtin_get_defined_func(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11143 |  |
|        - | 11144 | `	ph7_value *pArray,*pEntry;` |
|        - | 11145 | `	/* NOTE:` |
|        - | 11146 | `	 * Don't worry about freeing memory here,every allocated resource will be released` |
|        - | 11147 | `	 * automatically by the engine as soon we return from this foreign function.` |
|        - | 11148 | `	 */` |
|        3 | 11149 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11150 | ` 	if( pArray == 0 ){` |
|      ! 0 | 11151 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11152 | `		SXUNUSED(apArg);` |
|        - | 11153 | `		/* Return NULL */` |
|      ! 0 | 11154 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11155 | `		return SXRET_OK;` |
|        - | 11156 | `	}` |
|        3 | 11157 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11158 | `	if( pEntry == 0 ){` |
|        - | 11159 | `		/* Return NULL */` |
|      ! 0 | 11160 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11161 | `		return SXRET_OK;` |
|        - | 11162 | `	}` |
|        - | 11163 | `	/* Fill with the appropriate information */` |
|        3 | 11164 | `	SyHashForEach(&pCtx->pVm->hHostFunction,VmHashFuncStep,pEntry);` |
|        - | 11165 | `	/* Create the 'internal' index */` |
|        3 | 11166 | `	ph7_array_add_strkey_elem(pArray,"internal",pEntry); /* Will make it's own copy */` |
|        - | 11167 | `	/* Create the user-func array */` |
|        3 | 11168 | `	pEntry = ph7_context_new_array(pCtx);` |
|        3 | 11169 | `	if( pEntry == 0 ){` |
|        - | 11170 | `		/* Return NULL */` |
|      ! 0 | 11171 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11172 | `		return SXRET_OK;` |
|        - | 11173 | `	}` |
|        - | 11174 | `	/* Fill with the appropriate information */` |
|        3 | 11175 | `	SyHashForEach(&pCtx->pVm->hFunction,VmHashFuncStep,pEntry);` |
|        - | 11176 | `	/* Create the 'user' index */` |
|        3 | 11177 | `	ph7_array_add_strkey_elem(pArray,"user",pEntry); /* Will make it's own copy */` |
|        - | 11178 | `	/* Return the multi-dimensional array */` |
|        3 | 11179 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11180 | `	return SXRET_OK;` |
|        2 | 11181 |  |
|        - | 11182 | `/*` |
|        - | 11183 | ` * void register_shutdown_function(callable $callback[,mixed $param,...)` |
|        - | 11184 | ` *  Register a function for execution on shutdown.` |
|        - | 11185 | ` * Note` |
|        - | 11186 | ` *  Multiple calls to register_shutdown_function() can be made, and each will` |
|        - | 11187 | ` *  be called in the same order as they were registered.` |
|        - | 11188 | ` * Parameters` |
|        - | 11189 | ` *  $callback` |
|        - | 11190 | ` *   The shutdown callback to register.` |
|        - | 11191 | ` * $param` |
|        - | 11192 | ` *  One or more Parameter to pass to the registered callback.` |
|        - | 11193 | ` * Return` |
|        - | 11194 | ` *  Nothing.` |
|        - | 11195 | ` */` |
|        2 | 11196 | `static int vm_builtin_register_shutdown_function(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11197 |  |
|        - | 11198 | `	VmShutdownCB sEntry;` |
|        - | 11199 | `	int i,j;` |
|        3 | 11200 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11201 | `		/* Missing/Invalid arguments,return immediately */` |
|      ! 0 | 11202 | `		return PH7_OK;` |
|        - | 11203 | `	}` |
|        - | 11204 | `	/* Zero the Entry */` |
|        3 | 11205 | `	SyZero(&sEntry,sizeof(VmShutdownCB));` |
|        - | 11206 | `	/* Initialize fields */` |
|        3 | 11207 | `	PH7_MemObjInit(pCtx->pVm,&sEntry.sCallback);` |
|        - | 11208 | `	/* Save the callback name for later invocation name */` |
|        3 | 11209 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       23 | 11210 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(sEntry.aArg) ; ++i ){` |
|       21 | 11211 | `		PH7_MemObjInit(pCtx->pVm,&sEntry.aArg[i]);` |
|       11 | 11212 | `	}` |
|        - | 11213 | `	/* Copy arguments */` |
|        3 | 11214 | `	for(j = 0, i = 1 ; i < nArg ; j++,i++ ){` |
|      ! 0 | 11215 | `		if( j >= (int)SX_ARRAYSIZE(sEntry.aArg) ){` |
|        - | 11216 | `			/* Limit reached */` |
|      ! 0 | 11217 | `			break;` |
|        - | 11218 | `		}` |
|      ! 0 | 11219 | `		PH7_MemObjStore(apArg[i],&sEntry.aArg[j]);` |
|      ! 0 | 11220 | `	}` |
|        3 | 11221 | `	sEntry.nArg = j;` |
|        - | 11222 | `	/* Install the callback */` |
|        3 | 11223 | `	SySetPut(&pCtx->pVm->aShutdown,(const void *)&sEntry);` |
|        3 | 11224 | `	return PH7_OK;` |
|        2 | 11225 |  |
|        - | 11226 | `/*` |
|        - | 11227 | ` * Section:` |
|        - | 11228 | ` *  Class handling functions.` |
|        - | 11229 | ` * Status:` |
|        - | 11230 | ` *    Stable.` |
|        - | 11231 | ` */` |
|        - | 11232 | `/*` |
|        - | 11233 | ` * Extract the top active class. NULL is returned` |
|        - | 11234 | ` * if the class stack is empty.` |
|        - | 11235 | ` */` |
|      926 | 11236 | `PH7_PRIVATE ph7_class * PH7_VmPeekTopClass(ph7_vm *pVm)` |
|        2 | 11237 |  |
|      928 | 11238 | `	SySet *pSet = &pVm->aSelf;` |
|        - | 11239 | `	ph7_class **apClass;` |
|      928 | 11240 | `	if( SySetUsed(pSet) <= 0 ){` |
|        - | 11241 | `		/* Empty stack,return NULL */` |
|       15 | 11242 | `		return 0;` |
|        - | 11243 | `	}` |
|        - | 11244 | `	/* Peek the last entry */` |
|      914 | 11245 | `	apClass = (ph7_class **)SySetBasePtr(pSet);` |
|      914 | 11246 | `	return apClass[pSet->nUsed - 1];` |
|      465 | 11247 |  |
|        - | 11248 | `/*` |
|        - | 11249 | ` * ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        - | 11250 | ` *   Get the class that declared the currently executing method.` |
|        - | 11251 | ` *   This is used for resolving the 'self::' constant.` |
|        - | 11252 | ` *` |
|        - | 11253 | ` * Parameters` |
|        - | 11254 | ` *   pVm: Target VM` |
|        - | 11255 | ` *` |
|        - | 11256 | ` * Return` |
|        - | 11257 | ` *   The declaring class of the current method, or NULL if:` |
|        - | 11258 | ` *   - Not executing within a class method` |
|        - | 11259 | ` *` |
|        - | 11260 | ` * Note` |
|        - | 11261 | ` *   This differs from PH7_VmPeekTopClass() which returns the runtime class` |
|        - | 11262 | ` *   from the 'self' stack. For self::, we need the class that declared the` |
|        - | 11263 | ` *   currently executing method, not the runtime class (use static:: for that).` |
|        - | 11264 | ` *   This is found by walking the call frames to locate the method's` |
|        - | 11265 | ` *   declaring class.` |
|        - | 11266 | ` */` |
|       98 | 11267 | `PH7_PRIVATE ph7_class * PH7_VmPeekDeclaringClass(ph7_vm *pVm)` |
|        2 | 11268 |  |
|      100 | 11269 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 11270 | `	ph7_vm_func *pVmFunc;` |
|        - | 11271 |  |
|        - | 11272 | `	/* Skip exception frames to find the actual method frame */` |
|      100 | 11273 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        - | 11274 |  |
|        - | 11275 | `	/* Check if we're in a method context */` |
|      100 | 11276 | `	if( pFrame->pParent ){` |
|       96 | 11277 | `		pVmFunc = (ph7_vm_func *)pFrame->pUserData;` |
|       96 | 11278 | `		if( pVmFunc && (pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ){` |
|        - | 11279 | `			/* Return the declaring class */` |
|       96 | 11280 | `			return (ph7_class *)pVmFunc->pUserData;` |
|        - | 11281 | `		}` |
|      ! 0 | 11282 | `	}` |
|        - | 11283 |  |
|        5 | 11284 | `	return 0;` |
|       51 | 11285 |  |
|        - | 11286 |  |
|        - | 11287 | `/* Class/OOP builtin functions moved to vm_builtin_class.c */` |
|        - | 11288 | `/*` |
|        - | 11289 | ` * Call a class method where the name of the method is stored in the pMethod` |
|        - | 11290 | ` * parameter and the given arguments are stored in the apArg[] array.` |
|        - | 11291 | ` * Return SXRET_OK if the method was successfuly called.Any other` |
|        - | 11292 | ` * return value indicates failure.` |
|        - | 11293 | ` */` |
|        - | 11294 | `/*` |
|        - | 11295 | ` * Internal variant of PH7_VmCallClassMethod that threads a VmCallArgMap` |
|        - | 11296 | ` * through to the synthetic CALL instruction.  Used by the NEW handler so` |
|        - | 11297 | ` * that constructor calls with named arguments reach the named-arg path` |
|        - | 11298 | ` * (with variadic string-key packing) rather than the positional path.` |
|        - | 11299 | ` */` |
|     2380 | 11300 | `static sxi32 VmCallClassMethodWithMap(` |
|        - | 11301 | `	ph7_vm *pVm,` |
|        - | 11302 | `	ph7_class_instance *pThis,` |
|        - | 11303 | `	ph7_class_method *pMethod,` |
|        - | 11304 | `	ph7_value *pResult,` |
|        - | 11305 | `	int nArg,` |
|        - | 11306 | `	ph7_value **apArg,` |
|        - | 11307 | `	VmCallArgMap *pMap` |
|        - | 11308 | `	)` |
|        2 | 11309 |  |
|        - | 11310 | `	ph7_value *aStack;` |
|        - | 11311 | `	VmInstr aInstr[2];` |
|        - | 11312 | `	int iCursor;` |
|        - | 11313 | `	int i;` |
|     2382 | 11314 | `	aStack = VmNewOperandStack(&(*pVm),2+nArg);` |
|     2382 | 11315 | `	if( aStack == 0 ){` |
|      ! 0 | 11316 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11317 | `			"PH7 is running out of memory while invoking class method");` |
|      ! 0 | 11318 | `		return SXERR_MEM;` |
|        - | 11319 | `	}` |
|     3854 | 11320 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1474 | 11321 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|     1474 | 11322 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      738 | 11323 | `	}` |
|     2382 | 11324 | `	iCursor = nArg + 1;` |
|     2382 | 11325 | `	if( pThis ){` |
|     2376 | 11326 | `		pThis->iRef++;` |
|     2376 | 11327 | `		aStack[i].x.pOther = pThis;` |
|     2376 | 11328 | `		aStack[i].iFlags = MEMOBJ_OBJ;` |
|     1187 | 11329 | `	}` |
|     2382 | 11330 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2382 | 11331 | `	i++;` |
|     2382 | 11332 | `	SyBlobReset(&aStack[i].sBlob);` |
|     2382 | 11333 | `	SyBlobAppend(&aStack[i].sBlob,(const void *)SyStringData(&pMethod->sVmName),SyStringLength(&pMethod->sVmName));` |
|     2382 | 11334 | `	aStack[i].iFlags = MEMOBJ_STRING;` |
|     2382 | 11335 | `	aStack[i].nIdx = SXU32_HIGH;` |
|     2382 | 11336 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|     2382 | 11337 | `	aInstr[0].iP1 = nArg;` |
|     2382 | 11338 | `	aInstr[0].iP2 = 0;` |
|     2382 | 11339 | `	aInstr[0].p3  = (void *)pMap; /* forward named-arg metadata */` |
|     2382 | 11340 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|     2382 | 11341 | `	aInstr[1].iP1 = 1;` |
|     2382 | 11342 | `	aInstr[1].iP2 = 0;` |
|     2382 | 11343 | `	aInstr[1].p3  = 0;` |
|     2382 | 11344 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,iCursor,pResult,0,TRUE,0,0);` |
|     2382 | 11345 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|     2382 | 11346 | `	return PH7_OK;` |
|     1192 | 11347 |  |
|     1902 | 11348 | `PH7_PRIVATE sxi32 PH7_VmCallClassMethod(` |
|        - | 11349 | `	ph7_vm *pVm,               /* Target VM */` |
|        - | 11350 | `	ph7_class_instance *pThis, /* Target class instance [i.e: Object in the PHP jargon]*/` |
|        - | 11351 | `	ph7_class_method *pMethod, /* Method name */` |
|        - | 11352 | `	ph7_value *pResult,        /* Store method return value here. NULL otherwise */` |
|        - | 11353 | `	int nArg,                  /* Total number of given arguments */` |
|        - | 11354 | `	ph7_value **apArg          /* Method arguments */` |
|        - | 11355 | `	)` |
|        2 | 11356 |  |
|     1904 | 11357 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,0);` |
|        2 | 11358 |  |
|        - | 11359 | `/*` |
|        - | 11360 | ` * Dispatch a call to an object's __invoke magic method, forwarding arguments` |
|        - | 11361 | ` * and the return value. Used by the PH7_OP_CALL object-callable branch and by` |
|        - | 11362 | ` * PH7_VmCallUserFunction so that $obj(...), call_user_func($obj, ...) and` |
|        - | 11363 | ` * call_user_func_array($obj, [...]) all reach __invoke uniformly.` |
|        - | 11364 | ` *` |
|        - | 11365 | ` * Visibility is intentionally not checked: PHP allows private/protected` |
|        - | 11366 | ` * __invoke to be invoked via $obj() from any scope, and PHL's existing` |
|        - | 11367 | ` * is_callable / closure-invoke paths follow the same rule.` |
|        - | 11368 | ` *` |
|        - | 11369 | ` * pMap forwards the call-site VmCallArgMap so named-argument resolution and` |
|        - | 11370 | ` * strict_types coercion work for $obj(...) the same way they do for normal` |
|        - | 11371 | ` * function calls. Pass 0 from C-API call sites (call_user_func and friends),` |
|        - | 11372 | ` * which receive arguments positionally and don't carry a strict-types context.` |
|        - | 11373 | ` *` |
|        - | 11374 | ` * Returns SXRET_OK on success, SXERR_INVALID if __invoke is missing.` |
|        - | 11375 | ` */` |
|      166 | 11376 | `static sxi32 VmCallObjectInvoke(` |
|        - | 11377 | `	ph7_vm *pVm,` |
|        - | 11378 | `	ph7_class_instance *pThis,` |
|        - | 11379 | `	int nArg,` |
|        - | 11380 | `	ph7_value **apArg,` |
|        - | 11381 | `	ph7_value *pResult,` |
|        - | 11382 | `	VmCallArgMap *pMap` |
|        - | 11383 | `	)` |
|        2 | 11384 |  |
|        - | 11385 | `	ph7_class_method *pMethod;` |
|      168 | 11386 | `	pMethod = PH7_ClassExtractMethod(pThis->pClass,"__invoke",sizeof("__invoke")-1);` |
|      168 | 11387 | `	if( pMethod == 0 ){` |
|       13 | 11388 | `		if( pResult ){` |
|       13 | 11389 | `			PH7_MemObjRelease(pResult);` |
|        6 | 11390 | `		}` |
|       13 | 11391 | `		return SXERR_INVALID;` |
|        - | 11392 | `	}` |
|      156 | 11393 | `	return VmCallClassMethodWithMap(pVm,pThis,pMethod,pResult,nArg,apArg,pMap);` |
|       85 | 11394 |  |
|        - | 11395 | `/*` |
|        - | 11396 | ` * Raise a catchable Error("Object of type X is not callable") when an object` |
|        - | 11397 | ` * is invoked as a function but lacks __invoke. Mirrors the OP_THROW pattern` |
|        - | 11398 | ` * (vm.c PH7_OP_THROW): build the Error instance, mark the current frame as` |
|        - | 11399 | ` * throwing, dispatch via VmThrowException so the nearest try/catch can handle` |
|        - | 11400 | ` * it. Caller is responsible for the post-throw control flow (iExceptionJump` |
|        - | 11401 | ` * lookup or 'goto Exception').` |
|        - | 11402 | ` *` |
|        - | 11403 | ` * Returns the result of VmThrowException (SXRET_OK on handled exception,` |
|        - | 11404 | ` * SXERR_ABORT on abort), or SXERR_ABORT if the Error class itself cannot` |
|        - | 11405 | ` * be bootstrapped — in which case an uncaught fatal has already been` |
|        - | 11406 | ` * reported.` |
|        - | 11407 | ` */` |
|       12 | 11408 | `static sxi32 VmRaiseNotCallable(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        1 | 11409 |  |
|        - | 11410 | `	ph7_class *pErrorClass;` |
|       13 | 11411 | `	ph7_class_instance *pErrInst = 0;` |
|        - | 11412 | `	ph7_class_method *pCons;` |
|        - | 11413 | `	VmFrame *pThrowFrame;` |
|        - | 11414 | `	char zMsg[256];` |
|        - | 11415 | `	int nMsg;` |
|        - | 11416 | `	sxi32 rc;` |
|       25 | 11417 | `	nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|        - | 11418 | `		"Object of type %.*s is not callable",` |
|       12 | 11419 | `		(int)pThis->pClass->sName.nByte,` |
|       12 | 11420 | `		pThis->pClass->sName.zString);` |
|       13 | 11421 | `	pErrorClass = PH7_VmExtractClass(pVm,"Error",sizeof("Error")-1,TRUE,0);` |
|       13 | 11422 | `	if( pErrorClass ){` |
|       13 | 11423 | `		pErrInst = PH7_NewClassInstance(pVm,pErrorClass);` |
|        6 | 11424 | `	}` |
|       13 | 11425 | `	if( pErrInst == 0 ){` |
|        - | 11426 | `		/* Bootstrap failure: Error class is part of the built-in library and` |
|        - | 11427 | `		 * should always be available, so this branch is effectively unreachable.` |
|        - | 11428 | `		 * Degrade to an uncaught fatal report so the failure is at least` |
|        - | 11429 | `		 * visible to the user. */` |
|      ! 0 | 11430 | `		VmReportUncaughtException(pVm,"Error",5,zMsg,(sxu32)nMsg,0,0);` |
|      ! 0 | 11431 | `		return SXERR_ABORT;` |
|        - | 11432 | `	}` |
|       13 | 11433 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|       13 | 11434 | `	if( pCons ){` |
|        - | 11435 | `		ph7_value sArg;` |
|        - | 11436 | `		ph7_value *apMsg[1];` |
|        - | 11437 | `		SyString sMsgStr;` |
|       13 | 11438 | `		SyStringInitFromBuf(&sMsgStr,zMsg,(sxu32)nMsg);` |
|       13 | 11439 | `		PH7_MemObjInit(pVm,&sArg);` |
|       13 | 11440 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|       13 | 11441 | `		apMsg[0] = &sArg;` |
|       13 | 11442 | `		PH7_VmCallClassMethod(pVm,pErrInst,pCons,0,1,apMsg);` |
|       13 | 11443 | `		PH7_MemObjRelease(&sArg);` |
|        6 | 11444 | `	}` |
|        - | 11445 | `	/* Else: Error::__construct is part of the built-in library and should` |
|        - | 11446 | `	 * always be present; if it isn't, the thrown exception still surfaces` |
|        - | 11447 | `	 * with an empty getMessage() rather than crashing. */` |
|       13 | 11448 | `	pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|       13 | 11449 | `	if( pThrowFrame ){` |
|       13 | 11450 | `		pThrowFrame->iFlags \|= VM_FRAME_THROW;` |
|        6 | 11451 | `	}` |
|       13 | 11452 | `	rc = VmThrowException(pVm,pErrInst);` |
|       13 | 11453 | `	PH7_ClassInstanceUnref(pErrInst);` |
|       13 | 11454 | `	return rc;` |
|        7 | 11455 |  |
|        - | 11456 | `/*` |
|        - | 11457 | ` * Call a user defined or foreign function where the name of the function` |
|        - | 11458 | ` * is stored in the pFunc parameter and the given arguments are stored` |
|        - | 11459 | ` * in the apArg[] array.` |
|        - | 11460 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11461 | ` * return value indicates failure.` |
|        - | 11462 | ` */` |
|     1102 | 11463 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunction(` |
|        - | 11464 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11465 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11466 | `	int nArg,          /* Total number of given arguments */` |
|        - | 11467 | `	ph7_value **apArg, /* Callback arguments */` |
|        - | 11468 | `	ph7_value *pResult /* Store callback return value here. NULL otherwise */` |
|        - | 11469 | `	)` |
|        2 | 11470 |  |
|        - | 11471 | `	ph7_value *aStack;` |
|        - | 11472 | `	VmInstr aInstr[2];` |
|        - | 11473 | `	int i;` |
|     1104 | 11474 | `	if( pFunc->iFlags & MEMOBJ_OBJ ){` |
|        - | 11475 | `		/* Object callable: dispatch through __invoke when available.` |
|        - | 11476 | `		 * No VmCallArgMap: call_user_func / array_map / usort and the C API` |
|        - | 11477 | `		 * pass arguments positionally and inherit no strict-types context. */` |
|      137 | 11478 | `		return VmCallObjectInvoke(&(*pVm),` |
|       90 | 11479 | `			(ph7_class_instance *)pFunc->x.pOther,` |
|       45 | 11480 | `			nArg,apArg,pResult,0);` |
|        - | 11481 | `	}` |
|     1014 | 11482 | `	if((pFunc->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP)) == 0 ){` |
|        - | 11483 | `		/* Don't bother processing,it's invalid anyway */` |
|      511 | 11484 | `		if( pResult ){` |
|        - | 11485 | `			/* Assume a null return value */` |
|      ! 0 | 11486 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11487 | `		}` |
|      511 | 11488 | `		return SXERR_INVALID;` |
|        - | 11489 | `	}` |
|      504 | 11490 | `	if( pFunc->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 11491 | `		/* Class method */` |
|       11 | 11492 | `		ph7_hashmap *pMap = (ph7_hashmap *)pFunc->x.pOther;` |
|       11 | 11493 | `		ph7_class_method *pMethod = 0;` |
|       11 | 11494 | `		ph7_class_instance *pThis = 0;` |
|       11 | 11495 | `		ph7_class *pClass = 0;` |
|        - | 11496 | `		ph7_value *pValue;` |
|        - | 11497 | `		sxi32 rc;` |
|       11 | 11498 | `		if( pMap->nEntry < 2 /* Class name/instance + method name */){` |
|        - | 11499 | `			/* Empty hashmap,nothing to call */` |
|      ! 0 | 11500 | `			if( pResult ){` |
|        - | 11501 | `				/* Assume a null return value */` |
|      ! 0 | 11502 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11503 | `			}` |
|      ! 0 | 11504 | `			return SXRET_OK;` |
|        - | 11505 | `		}` |
|        - | 11506 | `		/* Extract the class name or an instance of it */` |
|       11 | 11507 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       11 | 11508 | `		if( pValue ){` |
|       11 | 11509 | `			pClass = PH7_VmExtractClassFromValue(&(*pVm),pValue);` |
|        5 | 11510 | `		}` |
|       11 | 11511 | `		if( pClass == 0 ){` |
|        - | 11512 | `			/* No such class,return NULL */` |
|      ! 0 | 11513 | `			if( pResult ){` |
|      ! 0 | 11514 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11515 | `			}` |
|      ! 0 | 11516 | `			return SXRET_OK;` |
|        - | 11517 | `		}` |
|       11 | 11518 | `		if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 11519 | `			/* Point to the class instance */` |
|        5 | 11520 | `			pThis = (ph7_class_instance *)pValue->x.pOther;` |
|        2 | 11521 | `		}` |
|        - | 11522 | `		/* Try to extract the method */` |
|       11 | 11523 | `		pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pMap->pFirst->pPrev->nValIdx);` |
|       11 | 11524 | `		if( pValue ){` |
|       11 | 11525 | `			if( (pValue->iFlags & MEMOBJ_STRING) && SyBlobLength(&pValue->sBlob) > 0 ){` |
|       16 | 11526 | `				pMethod = PH7_ClassExtractMethod(pClass,(const char *)SyBlobData(&pValue->sBlob),` |
|        5 | 11527 | `					SyBlobLength(&pValue->sBlob));` |
|        5 | 11528 | `			}` |
|        5 | 11529 | `		}` |
|       11 | 11530 | `		if( pMethod == 0 ){` |
|        - | 11531 | `			/* No such method,return NULL */` |
|      ! 0 | 11532 | `			if( pResult ){` |
|      ! 0 | 11533 | `				PH7_MemObjRelease(pResult);` |
|      ! 0 | 11534 | `			}` |
|      ! 0 | 11535 | `			return SXRET_OK;` |
|        - | 11536 | `		}` |
|        - | 11537 | `		/* Call the class method */` |
|       11 | 11538 | `		rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       11 | 11539 | `		return rc;` |
|        - | 11540 | `	}` |
|        - | 11541 | `	/* Create a new operand stack */` |
|      494 | 11542 | `	aStack = VmNewOperandStack(&(*pVm),1+nArg);` |
|      494 | 11543 | `	if( aStack == 0 ){` |
|      ! 0 | 11544 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|        - | 11545 | `			"PH7 is running out of memory while invoking user callback");` |
|      ! 0 | 11546 | `		if( pResult ){` |
|        - | 11547 | `			/* Assume a null return value */` |
|      ! 0 | 11548 | `			PH7_MemObjRelease(pResult);` |
|      ! 0 | 11549 | `		}` |
|      ! 0 | 11550 | `		return SXERR_MEM;` |
|        - | 11551 | `	}` |
|        - | 11552 | `	/* Fill the operand stack with the given arguments */` |
|     1604 | 11553 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     1112 | 11554 | `		PH7_MemObjLoad(apArg[i],&aStack[i]);` |
|        - | 11555 | `		/*` |
|        - | 11556 | `		 * Symisc eXtension:` |
|        - | 11557 | `		 *  Parameters to [call_user_func()] can be passed by reference.` |
|        - | 11558 | `		 */` |
|     1112 | 11559 | `		aStack[i].nIdx = apArg[i]->nIdx;` |
|      557 | 11560 | `	}` |
|        - | 11561 | `	/* Push the function name */` |
|      494 | 11562 | `	PH7_MemObjLoad(pFunc,&aStack[i]);` |
|      494 | 11563 | `	aStack[i].nIdx = SXU32_HIGH; /* Mark as constant */` |
|        - | 11564 | `	/* Emit the CALL istruction */` |
|      494 | 11565 | `	aInstr[0].iOp = PH7_OP_CALL;` |
|      494 | 11566 | `	aInstr[0].iP1 = nArg; /* Total number of given arguments */` |
|      494 | 11567 | `	aInstr[0].iP2 = 0;` |
|      494 | 11568 | `	aInstr[0].p3  = 0;` |
|        - | 11569 | `	/* Emit the DONE instruction */` |
|      494 | 11570 | `	aInstr[1].iOp = PH7_OP_DONE;` |
|      494 | 11571 | `	aInstr[1].iP1 = 1;   /* Extract function return value if available */` |
|      494 | 11572 | `	aInstr[1].iP2 = 0;` |
|      494 | 11573 | `	aInstr[1].p3  = 0;` |
|        - | 11574 | `	/* Execute the function body (if available) */` |
|      494 | 11575 | `	VmByteCodeExec(&(*pVm),aInstr,aStack,nArg,pResult,0,TRUE,0,0);` |
|        - | 11576 | `	/* Clean up the mess left behind */` |
|      494 | 11577 | `	SyMemBackendFree(&pVm->sAllocator,aStack);` |
|      494 | 11578 | `	return PH7_OK;` |
|      553 | 11579 |  |
|        - | 11580 | `/*` |
|        - | 11581 | ` * Call a user defined or foreign function whith a varibale number` |
|        - | 11582 | ` * of arguments where the name of the function is stored in the pFunc` |
|        - | 11583 | ` * parameter.` |
|        - | 11584 | ` * Return SXRET_OK if the function was successfuly called.Any other` |
|        - | 11585 | ` * return value indicates failure.` |
|        - | 11586 | ` */` |
|      236 | 11587 | `PH7_PRIVATE sxi32 PH7_VmCallUserFunctionAp(` |
|        - | 11588 | `	ph7_vm *pVm,       /* Target VM */` |
|        - | 11589 | `	ph7_value *pFunc,  /* Callback name */` |
|        - | 11590 | `	ph7_value *pResult,/* Store callback return value here. NULL otherwise */` |
|        - | 11591 | `	...                /* 0 (Zero) or more Callback arguments */` |
|        - | 11592 | `	)` |
|        1 | 11593 |  |
|        - | 11594 | `	ph7_value *pArg;` |
|        - | 11595 | `	SySet aArg;` |
|        - | 11596 | `	va_list ap;` |
|        - | 11597 | `	sxi32 rc;` |
|      237 | 11598 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|        - | 11599 | `	/* Copy arguments one after one */` |
|      237 | 11600 | `	va_start(ap,pResult);` |
|      393 | 11601 | `	for(;;){` |
|      787 | 11602 | `		pArg = va_arg(ap,ph7_value *);` |
|      787 | 11603 | `		if( pArg == 0 ){` |
|      237 | 11604 | `			break;` |
|        - | 11605 | `		}` |
|      551 | 11606 | `		SySetPut(&aArg,(const void *)&pArg);` |
|        1 | 11607 | `	}` |
|        - | 11608 | `	/* Call the core routine */` |
|      237 | 11609 | `	rc = PH7_VmCallUserFunction(&(*pVm),pFunc,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pResult);` |
|        - | 11610 | `	/* Cleanup */` |
|      237 | 11611 | `	SySetRelease(&aArg);` |
|      237 | 11612 | `	return rc;` |
|        1 | 11613 |  |
|        - | 11614 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|        - | 11615 | `/*` |
|        - | 11616 | ` * bool defined(string $name)` |
|        - | 11617 | ` *  Checks whether a given named constant exists.` |
|        - | 11618 | ` * Parameter:` |
|        - | 11619 | ` *  Name of the desired constant.` |
|        - | 11620 | ` * Return` |
|        - | 11621 | ` *  TRUE if the given constant exists.FALSE otherwise.` |
|        - | 11622 | ` */` |
|       16 | 11623 | `static int vm_builtin_defined(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11624 |  |
|        - | 11625 | `	const char *zName;` |
|       18 | 11626 | `	int nLen = 0;` |
|       18 | 11627 | `	int res = 0;` |
|       18 | 11628 | `	if( nArg < 1 ){` |
|        - | 11629 | `		/* Missing constant name,return FALSE */` |
|      ! 0 | 11630 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name");` |
|      ! 0 | 11631 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11632 | `		return SXRET_OK;` |
|        - | 11633 | `	}` |
|        - | 11634 | `	/* Extract constant name */` |
|       18 | 11635 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11636 | `	/* Perform the lookup */` |
|       18 | 11637 | `	if( nLen > 0 && SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen) != 0 ){` |
|        - | 11638 | `		/* Already defined */` |
|       12 | 11639 | `		res = 1;` |
|        5 | 11640 | `	}` |
|       18 | 11641 | `	ph7_result_bool(pCtx,res);` |
|       18 | 11642 | `	return SXRET_OK;` |
|       10 | 11643 |  |
|        - | 11644 | `/*` |
|        - | 11645 | ` * Constant expansion callback used by the [define()] function defined` |
|        - | 11646 | ` * below.` |
|        - | 11647 | ` */` |
|       10 | 11648 | `static void VmExpandUserConstant(ph7_value *pVal,void *pUserData)` |
|        2 | 11649 |  |
|       12 | 11650 | `	ph7_value *pConstantValue = (ph7_value *)pUserData;` |
|        - | 11651 | `	/* Expand constant value */` |
|       12 | 11652 | `	PH7_MemObjStore(pConstantValue,pVal);` |
|       12 | 11653 |  |
|        - | 11654 | `/*` |
|        - | 11655 | ` * bool define(string $constant_name,expression value)` |
|        - | 11656 | ` *  Defines a named constant at runtime.` |
|        - | 11657 | ` * Parameter:` |
|        - | 11658 | ` *  $constant_name` |
|        - | 11659 | ` *   The name of the constant` |
|        - | 11660 | ` *  $value` |
|        - | 11661 | ` *   Constant value` |
|        - | 11662 | ` * Return:` |
|        - | 11663 | ` *   TRUE on success,FALSE on failure.` |
|        - | 11664 | ` */` |
|       12 | 11665 | `static int vm_builtin_define(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11666 |  |
|        - | 11667 | `	const char *zName;  /* Constant name */` |
|        - | 11668 | `	ph7_value *pValue;  /* Duplicated constant value */` |
|       14 | 11669 | `	int nLen = 0;       /* Name length */` |
|        - | 11670 | `	sxi32 rc;` |
|       14 | 11671 | `	if( nArg < 2 ){` |
|        - | 11672 | `		/* Missing arguments,throw a ntoice and return false */` |
|      ! 0 | 11673 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing constant name/value pair");` |
|      ! 0 | 11674 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11675 | `		return SXRET_OK;` |
|        - | 11676 | `	}` |
|       14 | 11677 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      ! 0 | 11678 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Invalid constant name");` |
|      ! 0 | 11679 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11680 | `		return SXRET_OK;` |
|        - | 11681 | `	}` |
|        - | 11682 | `	/* Extract constant name */` |
|       14 | 11683 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|       14 | 11684 | `	if( nLen < 1 ){` |
|      ! 0 | 11685 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Empty constant name");` |
|      ! 0 | 11686 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11687 | `		return SXRET_OK;` |
|        - | 11688 | `	}` |
|        - | 11689 | `	/* Duplicate constant value */` |
|       14 | 11690 | `	pValue = (ph7_value *)SyMemBackendPoolAlloc(&pCtx->pVm->sAllocator,sizeof(ph7_value));` |
|       14 | 11691 | `	if( pValue == 0 ){` |
|      ! 0 | 11692 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11693 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11694 | `		return SXRET_OK;` |
|        - | 11695 | `	}` |
|        - | 11696 | `	/* Initialize the memory object */` |
|       14 | 11697 | `	PH7_MemObjInit(pCtx->pVm,pValue);` |
|        - | 11698 | `	/* Register the constant */` |
|       14 | 11699 | `	rc = ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|       14 | 11700 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 11701 | `		SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|      ! 0 | 11702 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Cannot register constant due to a memory failure");` |
|      ! 0 | 11703 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 11704 | `		return SXRET_OK;` |
|        - | 11705 | `	}` |
|        - | 11706 | `	/* Duplicate constant value */` |
|       14 | 11707 | `	PH7_MemObjStore(apArg[1],pValue);` |
|       14 | 11708 | `	if( nArg == 3 && ph7_value_is_bool(apArg[2]) && ph7_value_to_bool(apArg[2]) ){` |
|        - | 11709 | `		/* Lower case the constant name */` |
|      ! 0 | 11710 | `		char *zCur = (char *)zName;` |
|      ! 0 | 11711 | `		while( zCur < &zName[nLen] ){` |
|      ! 0 | 11712 | `			if( (unsigned char)zCur[0] >= 0xc0 ){` |
|        - | 11713 | `				/* UTF-8 stream */` |
|      ! 0 | 11714 | `				zCur++;` |
|      ! 0 | 11715 | `				while( zCur < &zName[nLen] && (((unsigned char)zCur[0] & 0xc0) == 0x80) ){` |
|      ! 0 | 11716 | `					zCur++;` |
|      ! 0 | 11717 | `				}` |
|      ! 0 | 11718 | `				continue;` |
|        - | 11719 | `			}` |
|      ! 0 | 11720 | `			if( SyisUpper(zCur[0]) ){` |
|      ! 0 | 11721 | `				int c = SyToLower(zCur[0]);` |
|      ! 0 | 11722 | `				zCur[0] = (char)c;` |
|      ! 0 | 11723 | `			}` |
|      ! 0 | 11724 | `			zCur++;` |
|      ! 0 | 11725 | `		}` |
|        - | 11726 | `		/* Finally,register the constant */` |
|      ! 0 | 11727 | `		ph7_create_constant(pCtx->pVm,zName,VmExpandUserConstant,pValue);` |
|      ! 0 | 11728 | `	}` |
|        - | 11729 | `	/* All done,return TRUE */` |
|       14 | 11730 | `	ph7_result_bool(pCtx,1);` |
|       14 | 11731 | `	return SXRET_OK;` |
|        8 | 11732 |  |
|        - | 11733 | `/*` |
|        - | 11734 | ` * value constant(string $name)` |
|        - | 11735 | ` *  Returns the value of a constant` |
|        - | 11736 | ` * Parameter` |
|        - | 11737 | ` *  $name` |
|        - | 11738 | ` *    Name of the constant.` |
|        - | 11739 | ` * Return` |
|        - | 11740 | ` *  Constant value or NULL if not defined.` |
|        - | 11741 | ` */` |
|        8 | 11742 | `static int vm_builtin_constant(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11743 |  |
|        - | 11744 | `	SyHashEntry *pEntry;` |
|        - | 11745 | `	ph7_constant *pCons;` |
|        - | 11746 | `	const char *zName; /* Constant name */` |
|        - | 11747 | `	ph7_value sVal;    /* Constant value */` |
|        - | 11748 | `	int nLen;` |
|       10 | 11749 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 11750 | `		/* Invallid argument,return NULL */` |
|      ! 0 | 11751 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Missing/Invalid constant name");` |
|      ! 0 | 11752 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11753 | `		return SXRET_OK;` |
|        - | 11754 | `	}` |
|        - | 11755 | `	/* Extract the constant name */` |
|       10 | 11756 | `	zName = ph7_value_to_string(apArg[0],&nLen);` |
|        - | 11757 | `	/* Perform the query */` |
|       10 | 11758 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant,(const void *)zName,(sxu32)nLen);` |
|       10 | 11759 | `	if( pEntry == 0 ){` |
|        3 | 11760 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"'%.*s': Undefined constant",nLen,zName);` |
|        3 | 11761 | `		ph7_result_null(pCtx);` |
|        3 | 11762 | `		return SXRET_OK;` |
|        - | 11763 | `	}` |
|        8 | 11764 | `	PH7_MemObjInit(pCtx->pVm,&sVal);` |
|        - | 11765 | `	/* Point to the structure that describe the constant */` |
|        8 | 11766 | `	pCons = (ph7_constant *)SyHashEntryGetUserData(pEntry);` |
|        - | 11767 | `	/* Extract constant value by calling it's associated callback */` |
|        8 | 11768 | `	pCons->xExpand(&sVal,pCons->pUserData);` |
|        - | 11769 | `	/* Return that value */` |
|        8 | 11770 | `	ph7_result_value(pCtx,&sVal);` |
|        - | 11771 | `	/* Cleanup */` |
|        8 | 11772 | `	PH7_MemObjRelease(&sVal);` |
|        8 | 11773 | `	return SXRET_OK;` |
|        6 | 11774 |  |
|        - | 11775 | `/*` |
|        - | 11776 | ` * Hash walker callback used by the [get_defined_constants()] function` |
|        - | 11777 | ` * defined below.` |
|        - | 11778 | ` */` |
|      452 | 11779 | `static int VmHashConstStep(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 11780 |  |
|      453 | 11781 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|        - | 11782 | `	ph7_value sName;` |
|        - | 11783 | `	sxi32 rc;` |
|        - | 11784 | `	/* Prepare the constant name for insertion */` |
|      453 | 11785 | `	PH7_MemObjInitFromString(pArray->pVm,&sName,0);` |
|      453 | 11786 | `	PH7_MemObjStringAppend(&sName,(const char *)pEntry->pKey,pEntry->nKeyLen);` |
|        - | 11787 | `	/* Perform the insertion */` |
|      453 | 11788 | `	rc = ph7_array_add_elem(pArray,0,&sName); /* Will make it's own copy */` |
|      453 | 11789 | `	PH7_MemObjRelease(&sName);` |
|      453 | 11790 | `	return rc;` |
|        1 | 11791 |  |
|        - | 11792 | `/*` |
|        - | 11793 | ` * array get_defined_constants(void)` |
|        - | 11794 | ` *  Returns an associative array with the names of all defined` |
|        - | 11795 | ` *  constants.` |
|        - | 11796 | ` * Parameters` |
|        - | 11797 | ` *  NONE.` |
|        - | 11798 | ` * Returns` |
|        - | 11799 | ` *  Returns the names of all the constants currently defined.` |
|        - | 11800 | ` */` |
|        2 | 11801 | `static int vm_builtin_get_defined_constants(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11802 |  |
|        - | 11803 | `	ph7_value *pArray;` |
|        - | 11804 | `	/* Create the array first*/` |
|        3 | 11805 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 11806 | `	if( pArray == 0 ){` |
|      ! 0 | 11807 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 11808 | `		SXUNUSED(apArg);` |
|        - | 11809 | `		/* Return NULL */` |
|      ! 0 | 11810 | `		ph7_result_null(pCtx);` |
|      ! 0 | 11811 | `		return SXRET_OK;` |
|        - | 11812 | `	}` |
|        - | 11813 | `	/* Fill the array with the defined constants */` |
|        3 | 11814 | `	SyHashForEach(&pCtx->pVm->hConstant,VmHashConstStep,pArray);` |
|        - | 11815 | `	/* Return the created array */` |
|        3 | 11816 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 11817 | `	return SXRET_OK;` |
|        2 | 11818 |  |
|        - | 11819 | `/* Output buffering builtins moved to vm_builtin_ob.c */` |
|        - | 11820 | `/*` |
|        - | 11821 | ` * Section:` |
|        - | 11822 | ` *  Random numbers/string generators.` |
|        - | 11823 | ` * Status:` |
|        - | 11824 | ` *    Stable.` |
|        - | 11825 | ` */` |
|        - | 11826 | `/*` |
|        - | 11827 | ` * Generate a random 32-bit unsigned integer.` |
|        - | 11828 | ` * PH7 use it's own private PRNG which is based on the one` |
|        - | 11829 | ` * used by te SQLite3 library.` |
|        - | 11830 | ` */` |
|     2878 | 11831 | `PH7_PRIVATE sxu32 PH7_VmRandomNum(ph7_vm *pVm)` |
|        2 | 11832 |  |
|        - | 11833 | `	sxu32 iNum;` |
|     2880 | 11834 | `	SyRandomness(&pVm->sPrng,(void *)&iNum,sizeof(sxu32));` |
|     2880 | 11835 | `	return iNum;` |
|        2 | 11836 |  |
|        - | 11837 | `/*` |
|        - | 11838 | ` * Generate a random string (English Alphabet) of length nLen.` |
|        - | 11839 | ` * Note that the generated string is NOT null terminated.` |
|        - | 11840 | ` * PH7 use it's own private PRNG which is based on the one used` |
|        - | 11841 | ` * by te SQLite3 library.` |
|        - | 11842 | ` */` |
|   231980 | 11843 | `PH7_PRIVATE void PH7_VmRandomString(ph7_vm *pVm,char *zBuf,int nLen)` |
|        2 | 11844 |  |
|        - | 11845 | `	static const char zBase[] = {"abcdefghijklmnopqrstuvwxyz"}; /* English Alphabet */` |
|        - | 11846 | `	int i;` |
|        - | 11847 | `	/* Generate a binary string first */` |
|   231982 | 11848 | `	SyRandomness(&pVm->sPrng,zBuf,(sxu32)nLen);` |
|        - | 11849 | `	/* Turn the binary string into english based alphabet */` |
|  2551950 | 11850 | `	for( i = 0 ; i < nLen ; ++i ){` |
|  2319970 | 11851 | `		 zBuf[i] = zBase[zBuf[i] % (sizeof(zBase)-1)];` |
|  1159986 | 11852 | `	 }` |
|   231982 | 11853 |  |
|        - | 11854 | `/*` |
|        - | 11855 | ` * int rand()` |
|        - | 11856 | ` * int mt_rand()` |
|        - | 11857 | ` * int rand(int $min,int $max)` |
|        - | 11858 | ` * int mt_rand(int $min,int $max)` |
|        - | 11859 | ` *  Generate a random (unsigned 32-bit) integer.` |
|        - | 11860 | ` * Parameter` |
|        - | 11861 | ` *  $min` |
|        - | 11862 | ` *    The lowest value to return (default: 0)` |
|        - | 11863 | ` *  $max` |
|        - | 11864 | ` *   The highest value to return (default: getrandmax())` |
|        - | 11865 | ` * Return` |
|        - | 11866 | ` *   A pseudo random value between min (or 0) and max (or getrandmax(), inclusive).` |
|        - | 11867 | ` * Note:` |
|        - | 11868 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11869 | ` *  by te SQLite3 library.` |
|        - | 11870 | ` */` |
|       20 | 11871 | `static int vm_builtin_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11872 |  |
|        - | 11873 | `	sxu32 iNum;` |
|        - | 11874 | `	/* Generate the random number */` |
|       21 | 11875 | `	iNum = PH7_VmRandomNum(pCtx->pVm);` |
|       21 | 11876 | `	if( nArg > 1 ){` |
|        - | 11877 | `		sxu32 iMin,iMax;` |
|        3 | 11878 | `		iMin = (sxu32)ph7_value_to_int(apArg[0]);` |
|        3 | 11879 | `		iMax = (sxu32)ph7_value_to_int(apArg[1]);` |
|        3 | 11880 | `		if( iMin < iMax ){` |
|        3 | 11881 | `			sxu32 iDiv = iMax+1-iMin;` |
|        3 | 11882 | `			if( iDiv > 0 ){` |
|        3 | 11883 | `				iNum = (iNum % iDiv)+iMin;` |
|        2 | 11884 | `			}` |
|        1 | 11885 | `		}else if(iMax > 0 ){` |
|      ! 0 | 11886 | `			iNum %= iMax;` |
|      ! 0 | 11887 | `		}` |
|        1 | 11888 | `	}` |
|        - | 11889 | `	/* Return the number */` |
|       21 | 11890 | `	ph7_result_int64(pCtx,(ph7_int64)iNum);` |
|       21 | 11891 | `	return SXRET_OK;` |
|        1 | 11892 |  |
|        - | 11893 | `/*` |
|        - | 11894 | ` * int getrandmax(void)` |
|        - | 11895 | ` * int mt_getrandmax(void)` |
|        - | 11896 | ` * int rc4_getrandmax(void)` |
|        - | 11897 | ` *   Show largest possible random value` |
|        - | 11898 | ` * Return` |
|        - | 11899 | ` *  The largest possible random value returned by rand() which is in` |
|        - | 11900 | ` *  this implementation 0xFFFFFFFF.` |
|        - | 11901 | ` * Note:` |
|        - | 11902 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11903 | ` *  by te SQLite3 library.` |
|        - | 11904 | ` */` |
|        4 | 11905 | `static int vm_builtin_getrandmax(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11906 |  |
|        2 | 11907 | `	SXUNUSED(nArg); /* cc warning */` |
|        2 | 11908 | `	SXUNUSED(apArg);` |
|        5 | 11909 | `	ph7_result_int64(pCtx,SXU32_HIGH);` |
|        5 | 11910 | `	return SXRET_OK;` |
|        1 | 11911 |  |
|        - | 11912 | `/*` |
|        - | 11913 | ` * string rand_str()` |
|        - | 11914 | ` * string rand_str(int $len)` |
|        - | 11915 | ` *  Generate a random string (English alphabet).` |
|        - | 11916 | ` * Parameter` |
|        - | 11917 | ` *  $len` |
|        - | 11918 | ` *    Length of the desired string (default: 16,Min: 1,Max: 1024)` |
|        - | 11919 | ` * Return` |
|        - | 11920 | ` *   A pseudo random string.` |
|        - | 11921 | ` * Note:` |
|        - | 11922 | ` *  PH7 use it's own private PRNG which is based on the one used` |
|        - | 11923 | ` *  by te SQLite3 library.` |
|        - | 11924 | ` *  This function is a symisc extension.` |
|        - | 11925 | ` */` |
|      120 | 11926 | `static int vm_builtin_rand_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 11927 |  |
|        - | 11928 | `	char zString[1024];` |
|      122 | 11929 | `	int iLen = 0x10;` |
|      122 | 11930 | `	if( nArg > 0 ){` |
|        - | 11931 | `		/* Get the desired length */` |
|      122 | 11932 | `		iLen = ph7_value_to_int(apArg[0]);` |
|      122 | 11933 | `		if( iLen < 1 \|\| iLen > 1024 ){` |
|        - | 11934 | `			/* Default length */` |
|        3 | 11935 | `			iLen = 0x10;` |
|        1 | 11936 | `		}` |
|       60 | 11937 | `	}` |
|        - | 11938 | `	/* Generate the random string */` |
|      122 | 11939 | `	PH7_VmRandomString(pCtx->pVm,zString,iLen);` |
|        - | 11940 | `	/* Return the generated string */` |
|      122 | 11941 | `	ph7_result_string(pCtx,zString,iLen); /* Will make it's own copy */` |
|      122 | 11942 | `	return SXRET_OK;` |
|        2 | 11943 |  |
|        - | 11944 | `/*` |
|        - | 11945 | ` * Reject non-numeric values (array/object/resource and non-numeric strings)` |
|        - | 11946 | ` * the same way intdiv() does. Returns SXRET_OK if the value is acceptable as` |
|        - | 11947 | ` * an int (PHP coerces float and numeric string silently).` |
|        - | 11948 | ` */` |
|      488 | 11949 | `static int VmRandomCheckIntArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgPos,const char *zParamName)` |
|        1 | 11950 |  |
|      488 | 11951 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg)` |
|      483 | 11952 | `		\|\| ph7_value_is_resource(pArg) ){` |
|       10 | 11953 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11954 | `			"TypeError",` |
|        - | 11955 | `			"%s(): Argument #%d (%s) must be of type int, %s given",` |
|        3 | 11956 | `			zFunc,iArgPos,zParamName,` |
|        3 | 11957 | `			ph7_type_name(pArg)` |
|        - | 11958 | `			);` |
|        - | 11959 | `	}` |
|      483 | 11960 | `	if( ph7_value_is_string(pArg) ){` |
|        - | 11961 | `		int len;` |
|        9 | 11962 | `		const char *zStr = ph7_value_to_string(pArg, &len);` |
|        9 | 11963 | `		if( SyStrIsNumeric(zStr, (sxu32)len, 0, 0) != SXRET_OK ){` |
|        7 | 11964 | `			return PH7_VmThrowException(pCtx,` |
|        - | 11965 | `				"TypeError",` |
|        - | 11966 | `				"%s(): Argument #%d (%s) must be of type int, string given",` |
|        2 | 11967 | `				zFunc,iArgPos,zParamName` |
|        - | 11968 | `				);` |
|        - | 11969 | `		}` |
|        2 | 11970 | `	}` |
|      479 | 11971 | `	return SXRET_OK;` |
|      245 | 11972 |  |
|        - | 11973 | `/*` |
|        - | 11974 | ` * int random_int(int $min, int $max)` |
|        - | 11975 | ` *  Generate a cryptographically secure pseudo-random integer in [$min, $max].` |
|        - | 11976 | ` *  Mirrors PHP 7.0+ random_int(). Uses the OS CSPRNG via SyOSCSPRNG().` |
|        - | 11977 | ` *  Distribution is uniform via rejection sampling against the smallest` |
|        - | 11978 | ` *  power-of-two mask covering the range.` |
|        - | 11979 | ` */` |
|      242 | 11980 | `static int vm_builtin_random_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 11981 |  |
|        - | 11982 | `	sxi64 iMin,iMax;` |
|        - | 11983 | `	sxu64 uRange,uMask,uResult;` |
|        - | 11984 | `	unsigned int nAttempt;` |
|        - | 11985 | `	int rc;` |
|      243 | 11986 | `	if( nArg != 2 ){` |
|       10 | 11987 | `		return PH7_VmThrowException(pCtx,` |
|        - | 11988 | `			"ArgumentCountError",` |
|        - | 11989 | `			"random_int() expects exactly 2 arguments, %d given",` |
|        3 | 11990 | `			nArg` |
|        - | 11991 | `			);` |
|        - | 11992 | `	}` |
|      237 | 11993 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_int",1,"$min");` |
|      237 | 11994 | `	if( rc != SXRET_OK ){ return rc; }` |
|      233 | 11995 | `	rc = VmRandomCheckIntArg(pCtx,apArg[1],"random_int",2,"$max");` |
|      233 | 11996 | `	if( rc != SXRET_OK ){ return rc; }` |
|      231 | 11997 | `	iMin = ph7_value_to_int64(apArg[0]);` |
|      231 | 11998 | `	iMax = ph7_value_to_int64(apArg[1]);` |
|      231 | 11999 | `	if( iMin > iMax ){` |
|        3 | 12000 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12001 | `			"ValueError",` |
|        - | 12002 | `			"random_int(): Argument #1 ($min) must be less than or equal to argument #2 ($max)"` |
|        - | 12003 | `			);` |
|        - | 12004 | `	}` |
|      229 | 12005 | `	if( iMin == iMax ){` |
|        5 | 12006 | `		ph7_result_int64(pCtx,iMin);` |
|        5 | 12007 | `		return SXRET_OK;` |
|        - | 12008 | `	}` |
|      225 | 12009 | `	uRange = (sxu64)iMax - (sxu64)iMin;` |
|      225 | 12010 | `	uMask = uRange;` |
|      225 | 12011 | `	uMask \|= uMask >> 1;` |
|      225 | 12012 | `	uMask \|= uMask >> 2;` |
|      225 | 12013 | `	uMask \|= uMask >> 4;` |
|      225 | 12014 | `	uMask \|= uMask >> 8;` |
|      225 | 12015 | `	uMask \|= uMask >> 16;` |
|      225 | 12016 | `	uMask \|= uMask >> 32;` |
|      225 | 12017 | `	uResult = 0;` |
|      352 | 12018 | `	for( nAttempt = 0 ; nAttempt < 50 ; ++nAttempt ){` |
|        - | 12019 | `		/* Always draw a full 8 bytes so endianness of the cast doesn't matter` |
|        - | 12020 | `		 * (a 4-byte fill into a sxu64 would land in the high half on big-endian` |
|        - | 12021 | `		 * and the low-half mask would always read 0). */` |
|        - | 12022 | `		sxu64 uDraw;` |
|      352 | 12023 | `		if( SyOSCSPRNG(&uDraw,sizeof(uDraw)) != SXRET_OK ){` |
|        - | 12024 | `			/* PHP 8.2+ would throw Random\RandomException here; that class` |
|        - | 12025 | `			 * is not yet registered in PHL (see PLAN.md item 6.13). */` |
|      ! 0 | 12026 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12027 | `				"Exception",` |
|        - | 12028 | `				"Cannot gather sufficient random data"` |
|        - | 12029 | `				);` |
|        - | 12030 | `		}` |
|      352 | 12031 | `		uDraw &= uMask;` |
|      352 | 12032 | `		if( uDraw <= uRange ){` |
|      225 | 12033 | `			uResult = uDraw;` |
|      225 | 12034 | `			break;` |
|        - | 12035 | `		}` |
|       58 | 12036 | `	}` |
|      225 | 12037 | `	if( nAttempt >= 50 ){` |
|      ! 0 | 12038 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12039 | `			"Exception",` |
|        - | 12040 | `			"Cannot gather sufficient random data"` |
|        - | 12041 | `			);` |
|        - | 12042 | `	}` |
|      225 | 12043 | `	ph7_result_int64(pCtx,(sxi64)((sxu64)iMin + uResult));` |
|      225 | 12044 | `	return SXRET_OK;` |
|      122 | 12045 |  |
|        - | 12046 | `/*` |
|        - | 12047 | ` * string random_bytes(int $length)` |
|        - | 12048 | ` *  Generate $length cryptographically secure random bytes via SyOSCSPRNG().` |
|        - | 12049 | ` *  Mirrors PHP 7.0+ random_bytes().` |
|        - | 12050 | ` */` |
|       24 | 12051 | `static int vm_builtin_random_bytes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12052 |  |
|        - | 12053 | `	sxi64 iLen;` |
|        - | 12054 | `	unsigned char zStack[256];` |
|        - | 12055 | `	void *pBuf;` |
|        - | 12056 | `	int rc;` |
|       25 | 12057 | `	int bHeap = 0;` |
|       25 | 12058 | `	if( nArg != 1 ){` |
|        7 | 12059 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12060 | `			"ArgumentCountError",` |
|        - | 12061 | `			"random_bytes() expects exactly 1 argument, %d given",` |
|        2 | 12062 | `			nArg` |
|        - | 12063 | `			);` |
|        - | 12064 | `	}` |
|       21 | 12065 | `	rc = VmRandomCheckIntArg(pCtx,apArg[0],"random_bytes",1,"$length");` |
|       21 | 12066 | `	if( rc != SXRET_OK ){ return rc; }` |
|       17 | 12067 | `	iLen = ph7_value_to_int64(apArg[0]);` |
|       17 | 12068 | `	if( iLen < 1 ){` |
|        5 | 12069 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12070 | `			"ValueError",` |
|        - | 12071 | `			"random_bytes(): Argument #1 ($length) must be greater than 0"` |
|        - | 12072 | `			);` |
|        - | 12073 | `	}` |
|        - | 12074 | `	/* The PH7 allocator and ph7_result_string both take sxu32/int sizes,` |
|        - | 12075 | `	 * so we can't honor a length above 2 GiB. Reject early rather than` |
|        - | 12076 | `	 * silently truncating via the (sxu32) cast below. */` |
|       13 | 12077 | `	if( iLen > 0x7FFFFFFF ){` |
|      ! 0 | 12078 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12079 | `			"ValueError",` |
|        - | 12080 | `			"random_bytes(): Argument #1 ($length) is too large"` |
|        - | 12081 | `			);` |
|        - | 12082 | `	}` |
|       13 | 12083 | `	if( iLen <= (sxi64)sizeof(zStack) ){` |
|       13 | 12084 | `		pBuf = zStack;` |
|        7 | 12085 | `	}else{` |
|      ! 0 | 12086 | `		pBuf = SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)iLen);` |
|      ! 0 | 12087 | `		if( pBuf == 0 ){` |
|      ! 0 | 12088 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12089 | `				"Exception",` |
|        - | 12090 | `				"random_bytes(): Failed to allocate %qd bytes",` |
|      ! 0 | 12091 | `				iLen` |
|        - | 12092 | `				);` |
|        - | 12093 | `		}` |
|      ! 0 | 12094 | `		bHeap = 1;` |
|        - | 12095 | `	}` |
|       13 | 12096 | `	if( SyOSCSPRNG(pBuf,(sxu32)iLen) != SXRET_OK ){` |
|      ! 0 | 12097 | `		if( bHeap ){` |
|      ! 0 | 12098 | `			SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12099 | `		}` |
|      ! 0 | 12100 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12101 | `			"Exception",` |
|        - | 12102 | `			"Cannot gather sufficient random data"` |
|        - | 12103 | `			);` |
|        - | 12104 | `	}` |
|       13 | 12105 | `	ph7_result_string(pCtx,(const char *)pBuf,(int)iLen);` |
|       13 | 12106 | `	if( bHeap ){` |
|      ! 0 | 12107 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,pBuf);` |
|      ! 0 | 12108 | `	}` |
|       13 | 12109 | `	return SXRET_OK;` |
|       13 | 12110 |  |
|        - | 12111 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 12112 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 12113 | `/* Unique ID private data */` |
|        - | 12114 | `struct unique_id_data` |
|        - | 12115 |  |
|        - | 12116 | `	ph7_context *pCtx; /* Call context */` |
|        - | 12117 | `	int entropy;       /* TRUE if the more_entropy flag is set */` |
|        - | 12118 | `};` |
|        - | 12119 | `/*` |
|        - | 12120 | ` * Binary to hex consumer callback.` |
|        - | 12121 | ` * This callback is the default consumer used by [uniqid()] function` |
|        - | 12122 | ` * defined below.` |
|        - | 12123 | ` */` |
|      192 | 12124 | `static int HexConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|        1 | 12125 |  |
|      193 | 12126 | `	struct unique_id_data *pUniq = (struct unique_id_data *)pUserData;` |
|        - | 12127 | `	sxu32 nBuflen;` |
|        - | 12128 | `	/* Extract result buffer length */` |
|      193 | 12129 | `	nBuflen = ph7_context_result_buf_length(pUniq->pCtx);` |
|      193 | 12130 | `	if( nBuflen > 12 && !pUniq->entropy ){` |
|        - | 12131 | `			/*` |
|        - | 12132 | `			 * If the more_entropy flag is not set,then the returned` |
|        - | 12133 | `			 * string will be 13 characters long` |
|        - | 12134 | `			 */` |
|       25 | 12135 | `		return SXERR_ABORT;` |
|        - | 12136 | `	}` |
|      169 | 12137 | `	if( nBuflen > 22 ){` |
|      ! 0 | 12138 | `		return SXERR_ABORT;` |
|        - | 12139 | `	}` |
|        - | 12140 | `	/* Safely Consume the hex stream */` |
|      169 | 12141 | `	ph7_result_string(pUniq->pCtx,(const char *)pData,(int)nLen);` |
|      169 | 12142 | `	return SXRET_OK;` |
|       97 | 12143 |  |
|        - | 12144 | `/*` |
|        - | 12145 | ` * string uniqid([string $prefix = "" [, bool $more_entropy = false]])` |
|        - | 12146 | ` *  Generate a unique ID` |
|        - | 12147 | ` * Parameter` |
|        - | 12148 | ` * $prefix` |
|        - | 12149 | ` *  Append this prefix to the generated unique ID.` |
|        - | 12150 | ` *  With an empty prefix, the returned string will be 13 characters long.` |
|        - | 12151 | ` *  If more_entropy is TRUE, it will be 23 characters.` |
|        - | 12152 | ` * $more_entropy` |
|        - | 12153 | ` *  If set to TRUE, uniqid() will add additional entropy which increases the likelihood` |
|        - | 12154 | ` *  that the result will be unique.` |
|        - | 12155 | ` * Return` |
|        - | 12156 | ` *  Returns the unique identifier, as a string.` |
|        - | 12157 | ` */` |
|       24 | 12158 | `static int vm_builtin_uniqid(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12159 |  |
|        - | 12160 | `	struct unique_id_data sUniq;` |
|        - | 12161 | `	unsigned char zDigest[20];` |
|       25 | 12162 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12163 | `	const char *zPrefix;` |
|        - | 12164 | `	SHA1Context sCtx;` |
|        - | 12165 | `	char zRandom[7];` |
|        - | 12166 | `	int nPrefix;` |
|        - | 12167 | `	int entropy;` |
|        - | 12168 | `	/* Generate a random string first */` |
|       25 | 12169 | `	PH7_VmRandomString(pVm,zRandom,(int)sizeof(zRandom));` |
|        - | 12170 | `	/* Initialize fields */` |
|       25 | 12171 | `	zPrefix = 0;` |
|       25 | 12172 | `	nPrefix = 0;` |
|       25 | 12173 | `	entropy = 0;` |
|       25 | 12174 | `	if( nArg > 0 ){` |
|        - | 12175 | `		/* Append this prefix to the generated unqiue ID */` |
|      ! 0 | 12176 | `		zPrefix = ph7_value_to_string(apArg[0],&nPrefix);` |
|      ! 0 | 12177 | `		if( nArg > 1 ){` |
|      ! 0 | 12178 | `			entropy = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 12179 | `		}` |
|      ! 0 | 12180 | `	}` |
|       25 | 12181 | `	SHA1Init(&sCtx);` |
|        - | 12182 | `	/* Generate the random ID */` |
|       25 | 12183 | `	if( nPrefix > 0 ){` |
|      ! 0 | 12184 | `		SHA1Update(&sCtx,(const unsigned char *)zPrefix,(unsigned int)nPrefix);` |
|      ! 0 | 12185 | `	}` |
|        - | 12186 | `	/* Append the random ID */` |
|       25 | 12187 | `	SHA1Update(&sCtx,(const unsigned char *)&pVm->unique_id,sizeof(int));` |
|        - | 12188 | `	/* Append the random string */` |
|       25 | 12189 | `	SHA1Update(&sCtx,(const unsigned char *)zRandom,sizeof(zRandom));` |
|        - | 12190 | `	/* Increment the number */` |
|       25 | 12191 | `	pVm->unique_id++;` |
|       25 | 12192 | `	SHA1Final(&sCtx,zDigest);` |
|        - | 12193 | `	/* Hexify the digest */` |
|       25 | 12194 | `	sUniq.pCtx = pCtx;` |
|       25 | 12195 | `	sUniq.entropy = entropy;` |
|       25 | 12196 | `	SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HexConsumer,&sUniq);` |
|        - | 12197 | `	/* All done */` |
|       25 | 12198 | `	return PH7_OK;` |
|        1 | 12199 |  |
|        - | 12200 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 12201 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 12202 | `/*` |
|        - | 12203 | ` * Section:` |
|        - | 12204 | ` *  Language construct implementation as foreign functions.` |
|        - | 12205 | ` * Status:` |
|        - | 12206 | ` *    Stable.` |
|        - | 12207 | ` */` |
|        - | 12208 | `/*` |
|        - | 12209 | ` * void echo($string...)` |
|        - | 12210 | ` *  Output one or more messages.` |
|        - | 12211 | ` * Parameters` |
|        - | 12212 | ` *  $string` |
|        - | 12213 | ` *   Message to output.` |
|        - | 12214 | ` * Return` |
|        - | 12215 | ` *  NULL.` |
|        - | 12216 | ` */` |
|      ! 0 | 12217 | `static int vm_builtin_echo(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12218 |  |
|        - | 12219 | `	const char *zData;` |
|      ! 0 | 12220 | `	int nDataLen = 0;` |
|        - | 12221 | `	ph7_vm *pVm;` |
|        - | 12222 | `	int i,rc;` |
|        - | 12223 | `	/* Point to the target VM */` |
|      ! 0 | 12224 | `	pVm = pCtx->pVm;` |
|        - | 12225 | `	/* Output */` |
|      ! 0 | 12226 | `	for( i = 0 ; i < nArg ; ++i ){` |
|      ! 0 | 12227 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|      ! 0 | 12228 | `		if( nDataLen > 0 ){` |
|      ! 0 | 12229 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|      ! 0 | 12230 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|      ! 0 | 12231 | `			if( rc == SXERR_ABORT ){` |
|        - | 12232 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12233 | `				return PH7_ABORT;` |
|        - | 12234 | `			}` |
|      ! 0 | 12235 | `		}` |
|      ! 0 | 12236 | `	}` |
|      ! 0 | 12237 | `	return SXRET_OK;` |
|      ! 0 | 12238 |  |
|        - | 12239 | `/*` |
|        - | 12240 | ` * int print($string...)` |
|        - | 12241 | ` *  Output one or more messages.` |
|        - | 12242 | ` * Parameters` |
|        - | 12243 | ` *  $string` |
|        - | 12244 | ` *   Message to output.` |
|        - | 12245 | ` * Return` |
|        - | 12246 | ` *  1 always.` |
|        - | 12247 | ` */` |
|        2 | 12248 | `static int vm_builtin_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12249 |  |
|        - | 12250 | `	const char *zData;` |
|        3 | 12251 | `	int nDataLen = 0;` |
|        - | 12252 | `	ph7_vm *pVm;` |
|        - | 12253 | `	int i,rc;` |
|        - | 12254 | `	/* Point to the target VM */` |
|        3 | 12255 | `	pVm = pCtx->pVm;` |
|        - | 12256 | `	/* Output */` |
|        5 | 12257 | `	for( i = 0 ; i < nArg ; ++i ){` |
|        3 | 12258 | `		zData = ph7_value_to_string(apArg[i],&nDataLen);` |
|        3 | 12259 | `		if( nDataLen > 0 ){` |
|        3 | 12260 | `			rc = pVm->sVmConsumer.xConsumer((const void *)zData,(unsigned int)nDataLen,pVm->sVmConsumer.pUserData);` |
|        3 | 12261 | `			VmTrackOutput(pVm, (sxu32)nDataLen);` |
|        3 | 12262 | `			if( rc == SXERR_ABORT ){` |
|        - | 12263 | `				/* Output consumer callback request an operation abort */` |
|      ! 0 | 12264 | `				return PH7_ABORT;` |
|        - | 12265 | `			}` |
|        1 | 12266 | `		}` |
|        2 | 12267 | `	}` |
|        - | 12268 | `	/* Return 1 */` |
|        3 | 12269 | `	ph7_result_int(pCtx,1);` |
|        3 | 12270 | `	return SXRET_OK;` |
|        2 | 12271 |  |
|        - | 12272 | `/*` |
|        - | 12273 | ` * void exit(string $msg)` |
|        - | 12274 | ` * void exit(int $status)` |
|        - | 12275 | ` * void die(string $ms)` |
|        - | 12276 | ` * void die(int $status)` |
|        - | 12277 | ` *   Output a message and terminate program execution.` |
|        - | 12278 | ` * Parameter` |
|        - | 12279 | ` *  If status is a string, this function prints the status just before exiting.` |
|        - | 12280 | ` *  If status is an integer, that value will be used as the exit status` |
|        - | 12281 | ` *  and not printed` |
|        - | 12282 | ` * Return` |
|        - | 12283 | ` *  NULL` |
|        - | 12284 | ` */` |
|      ! 0 | 12285 | `static int vm_builtin_exit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      ! 0 | 12286 |  |
|      ! 0 | 12287 | `	if( nArg > 0 ){` |
|      ! 0 | 12288 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        - | 12289 | `			const char *zData;` |
|      ! 0 | 12290 | `			int iLen = 0;` |
|        - | 12291 | `			/* Print exit message */` |
|      ! 0 | 12292 | `			zData = ph7_value_to_string(apArg[0],&iLen);` |
|      ! 0 | 12293 | `			ph7_context_output(pCtx,zData,iLen);` |
|      ! 0 | 12294 | `		}else if(ph7_value_is_int(apArg[0]) ){` |
|        - | 12295 | `			sxi32 iExitStatus;` |
|        - | 12296 | `			/* Record exit status code */` |
|      ! 0 | 12297 | `			iExitStatus = ph7_value_to_int(apArg[0]);` |
|      ! 0 | 12298 | `			pCtx->pVm->iExitStatus = iExitStatus;` |
|      ! 0 | 12299 | `		}` |
|      ! 0 | 12300 | `	}` |
|        - | 12301 | `	/* Check if we are in an included file */` |
|      ! 0 | 12302 | `	if( SySetUsed(&pCtx->pVm->aFiles) > 0 ){` |
|        - | 12303 | `		/* Exit the entire process */` |
|      ! 0 | 12304 | `		exit(pCtx->pVm->iExitStatus);` |
|        - | 12305 | `	}` |
|        - | 12306 | `	/* Abort processing immediately */` |
|      ! 0 | 12307 | `	return PH7_ABORT;` |
|      ! 0 | 12308 |  |
|        - | 12309 | `/*` |
|        - | 12310 | ` * bool isset($var,...)` |
|        - | 12311 | ` *  Finds out whether a variable is set.` |
|        - | 12312 | ` * Parameters` |
|        - | 12313 | ` *  One or more variable to check.` |
|        - | 12314 | ` * Return` |
|        - | 12315 | ` *  1 if var exists and has value other than NULL, 0 otherwise.` |
|        - | 12316 | ` */` |
|    90844 | 12317 | `static int vm_builtin_isset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12318 |  |
|        - | 12319 | `	ph7_value *pObj;` |
|    90846 | 12320 | `	int res = 0;` |
|        - | 12321 | `	int i;` |
|    90846 | 12322 | `	if( nArg < 1 ){` |
|        - | 12323 | `		/* Missing arguments,return false */` |
|      ! 0 | 12324 | `		ph7_result_bool(pCtx,res);` |
|      ! 0 | 12325 | `		return SXRET_OK;` |
|        - | 12326 | `	}` |
|        - | 12327 | `	/* Iterate over available arguments */` |
|   118804 | 12328 | `	for( i = 0 ; i < nArg ; ++i ){` |
|    90856 | 12329 | `		pObj = apArg[i];` |
|    90856 | 12330 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|        - | 12331 | `			/* Skip the "expecting a variable" warning for MEMOBJ_BOOL —` |
|        - | 12332 | `			 * synthesized by LOAD_IDX iP2=4 (ArrayAccess::offsetExists) and` |
|        - | 12333 | `			 * by anyone passing a bool literal (rare, harmless). */` |
|    62000 | 12334 | `			if( (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|        - | 12335 | `				/* Not so fatal,Throw a warning */` |
|      ! 0 | 12336 | `				ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Expecting a variable not a constant");` |
|      ! 0 | 12337 | `			}` |
|    30999 | 12338 | `		}` |
|    90856 | 12339 | `		res = (pObj->iFlags & MEMOBJ_NULL) ? 0 : 1;` |
|    90856 | 12340 | `		if( !res ){` |
|        - | 12341 | `			/* Variable not set,return FALSE */` |
|    62898 | 12342 | `			ph7_result_bool(pCtx,0);` |
|    62898 | 12343 | `			return SXRET_OK;` |
|        - | 12344 | `		}` |
|    13981 | 12345 | `	}` |
|        - | 12346 | `	/* All given variable are set,return TRUE */` |
|    27950 | 12347 | `	ph7_result_bool(pCtx,1);` |
|    27950 | 12348 | `	return SXRET_OK;` |
|    45424 | 12349 |  |
|        - | 12350 | `/*` |
|        - | 12351 | ` * Unset a memory object [i.e: a ph7_value],remove it from the current` |
|        - | 12352 | ` * frame,the reference table and discard it's contents.` |
|        - | 12353 | ` * This function never fail and always return SXRET_OK.` |
|        - | 12354 | ` */` |
|  3129018 | 12355 | `PH7_PRIVATE sxi32 PH7_VmUnsetMemObj(ph7_vm *pVm,sxu32 nObjIdx,int bForce)` |
|        2 | 12356 |  |
|        - | 12357 | `	ph7_value *pObj;` |
|        - | 12358 | `	VmRefObj *pRef;` |
|  3129020 | 12359 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);` |
|  3129020 | 12360 | `	if( pObj ){` |
|        - | 12361 | `		/* Release the object */` |
|  3129020 | 12362 | `		PH7_MemObjRelease(pObj);` |
|  1564509 | 12363 | `	}` |
|        - | 12364 | `	/* Remove old reference links */` |
|  3129020 | 12365 | `	pRef = VmRefObjExtract(&(*pVm),nObjIdx);` |
|  3129020 | 12366 | `	if( pRef ){` |
|  3129014 | 12367 | `		sxi32 iFlags = pRef->iFlags;` |
|        - | 12368 | `		/* Unlink from the reference table */` |
|  3129014 | 12369 | `		VmRefObjUnlink(&(*pVm),pRef);` |
|  3129014 | 12370 | `		if( (bForce == TRUE) \|\| (iFlags & VM_REF_IDX_KEEP) == 0 ){` |
|        - | 12371 | `			VmSlot sFree;` |
|        - | 12372 | `			/* Restore to the free list */` |
|  3129006 | 12373 | `			sFree.nIdx = nObjIdx;` |
|  3129006 | 12374 | `			sFree.pUserData = 0;` |
|  3129006 | 12375 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|  1564502 | 12376 | `		}` |
|  1564506 | 12377 | `	}` |
|  3129020 | 12378 | `	return SXRET_OK;` |
|        2 | 12379 |  |
|        - | 12380 | `/*` |
|        - | 12381 | ` * void unset($var,...)` |
|        - | 12382 | ` *   Unset one or more given variable.` |
|        - | 12383 | ` * Parameters` |
|        - | 12384 | ` *  One or more variable to unset.` |
|        - | 12385 | ` * Return` |
|        - | 12386 | ` *  Nothing.` |
|        - | 12387 | ` */` |
|     7438 | 12388 | `static int vm_builtin_unset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12389 |  |
|        - | 12390 | `	ph7_value *pObj;` |
|        - | 12391 | `	ph7_vm *pVm;` |
|        - | 12392 | `	int i;` |
|        - | 12393 | `	/* Point to the target VM */` |
|     7440 | 12394 | `	pVm = pCtx->pVm;` |
|        - | 12395 | `	/* Iterate and unset */` |
|    14878 | 12396 | `	for( i = 0 ; i < nArg ; ++i ){` |
|     7440 | 12397 | `		pObj = apArg[i];` |
|     7440 | 12398 | `		if( pObj->nIdx == SXU32_HIGH ){` |
|      818 | 12399 | `			if( (pObj->iFlags & MEMOBJ_NULL) == 0 ){` |
|        - | 12400 | `				/* Throw an error */` |
|      ! 0 | 12401 | `				ph7_context_throw_error(pCtx,PH7_CTX_ERR,"Expecting a variable not a constant");` |
|      ! 0 | 12402 | `			}` |
|      410 | 12403 | `		}else{` |
|     6624 | 12404 | `			sxu32 nIdx = pObj->nIdx;` |
|        - | 12405 | `			/* TICKET 1433-35: Protect the $GLOBALS array from deletion */` |
|     6624 | 12406 | `			if( nIdx != pVm->nGlobalIdx ){` |
|     6618 | 12407 | `				PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|     3308 | 12408 | `			}` |
|        - | 12409 | `		}` |
|     3721 | 12410 | `	}` |
|     7440 | 12411 | `	return SXRET_OK;` |
|        2 | 12412 |  |
|        - | 12413 | `/*` |
|        - | 12414 | ` * Hash walker callback used by the [get_defined_vars()] function.` |
|        - | 12415 | ` */` |
|      110 | 12416 | `static sxi32 VmHashVarWalker(SyHashEntry *pEntry,void *pUserData)` |
|        1 | 12417 |  |
|      111 | 12418 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      111 | 12419 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 12420 | `	ph7_value *pObj;` |
|        - | 12421 | `	sxu32 nIdx;` |
|        - | 12422 | `	/* Extract the memory object */` |
|      111 | 12423 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|      111 | 12424 | `	pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      111 | 12425 | `	if( pObj ){` |
|      111 | 12426 | `		if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 \|\| (ph7_hashmap *)pObj->x.pOther != pVm->pGlobal ){` |
|      109 | 12427 | `			if( pEntry->nKeyLen > 0 ){` |
|        - | 12428 | `				SyString sName;` |
|        - | 12429 | `				ph7_value sKey;` |
|        - | 12430 | `				/* Perform the insertion */` |
|      109 | 12431 | `				SyStringInitFromBuf(&sName,pEntry->pKey,pEntry->nKeyLen);` |
|      109 | 12432 | `				PH7_MemObjInitFromString(pVm,&sKey,&sName);` |
|      109 | 12433 | `				ph7_array_add_elem(pArray,&sKey/*Will make it's own copy*/,pObj);` |
|      109 | 12434 | `				PH7_MemObjRelease(&sKey);` |
|       54 | 12435 | `			}` |
|       54 | 12436 | `		}` |
|       55 | 12437 | `	}` |
|      111 | 12438 | `	return SXRET_OK;` |
|        1 | 12439 |  |
|        - | 12440 | `/*` |
|        - | 12441 | ` * array get_defined_vars(void)` |
|        - | 12442 | ` *  Returns an array of all defined variables.` |
|        - | 12443 | ` * Parameter` |
|        - | 12444 | ` *  None` |
|        - | 12445 | ` * Return` |
|        - | 12446 | ` *  An array with all the variables defined in the current scope.` |
|        - | 12447 | ` */` |
|        2 | 12448 | `static int vm_builtin_get_defined_vars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12449 |  |
|        3 | 12450 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12451 | `	ph7_value *pArray;` |
|        - | 12452 | `	/* Create a new array */` |
|        3 | 12453 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 12454 | ` 	if( pArray == 0 ){` |
|      ! 0 | 12455 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 12456 | `		SXUNUSED(apArg);` |
|        - | 12457 | `		/* Return NULL */` |
|      ! 0 | 12458 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12459 | `		return SXRET_OK;` |
|        - | 12460 | `	}` |
|        - | 12461 | `	/* Superglobals first */` |
|        3 | 12462 | `	SyHashForEach(&pVm->hSuper,VmHashVarWalker,pArray);` |
|        - | 12463 | `	/* Then variable defined in the current frame */` |
|        3 | 12464 | `	SyHashForEach(&pVm->pFrame->hVar,VmHashVarWalker,pArray);` |
|        - | 12465 | `	/* Finally,return the created array */` |
|        3 | 12466 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 12467 | `	return SXRET_OK;` |
|        2 | 12468 |  |
|        - | 12469 | `/*` |
|        - | 12470 | ` * bool gettype($var)` |
|        - | 12471 | ` *  Get the type of a variable` |
|        - | 12472 | ` * Parameters` |
|        - | 12473 | ` *   $var` |
|        - | 12474 | ` *    The variable being type checked.` |
|        - | 12475 | ` * Return` |
|        - | 12476 | ` *   String representation of the given variable type.` |
|        - | 12477 | ` */` |
|       32 | 12478 | `static int vm_builtin_gettype(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12479 |  |
|       34 | 12480 | `	const char *zType = "Empty";` |
|       34 | 12481 | `	if( nArg > 0 ){` |
|       34 | 12482 | `		zType = PH7_MemObjTypeDump(apArg[0]);` |
|       16 | 12483 | `	}` |
|        - | 12484 | `	/* Return the variable type */` |
|       34 | 12485 | `	ph7_result_string(pCtx,zType,-1/*Compute length automatically*/);` |
|       34 | 12486 | `	return SXRET_OK;` |
|        2 | 12487 |  |
|        - | 12488 | `/*` |
|        - | 12489 | ` * string get_resource_type(resource $handle)` |
|        - | 12490 | ` *  This function gets the type of the given resource.` |
|        - | 12491 | ` * Parameters` |
|        - | 12492 | ` *  $handle` |
|        - | 12493 | ` *  The evaluated resource handle.` |
|        - | 12494 | ` * Return` |
|        - | 12495 | ` *  If the given handle is a resource, this function will return a string` |
|        - | 12496 | ` *  representing its type. If the type is not identified by this function` |
|        - | 12497 | ` *  the return value will be the string Unknown.` |
|        - | 12498 | ` *  This function will return FALSE and generate an error if handle` |
|        - | 12499 | ` *  is not a resource.` |
|        - | 12500 | ` */` |
|        2 | 12501 | `static int vm_builtin_get_resource_type(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12502 |  |
|        3 | 12503 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|        - | 12504 | `		/* Missing/Invalid arguments,return FALSE*/` |
|      ! 0 | 12505 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12506 | `		return PH7_OK;` |
|        - | 12507 | `	}` |
|        3 | 12508 | `	ph7_result_string_format(pCtx,"resID_%#x",apArg[0]->x.pOther);` |
|        3 | 12509 | `	return SXRET_OK;` |
|        2 | 12510 |  |
|        - | 12511 | `/*` |
|        - | 12512 | ` * void var_dump(expression,....)` |
|        - | 12513 | ` *   var_dump � Dumps information about a variable` |
|        - | 12514 | ` * Parameters` |
|        - | 12515 | ` *   One or more expression to dump.` |
|        - | 12516 | ` * Returns` |
|        - | 12517 | ` *  Nothing.` |
|        - | 12518 | ` */` |
|      218 | 12519 | `static int vm_builtin_var_dump(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12520 |  |
|        - | 12521 | `	SyBlob sDump; /* Generated dump is stored here */` |
|        - | 12522 | `	int i;` |
|      220 | 12523 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        - | 12524 | `	/* Dump one or more expressions */` |
|      444 | 12525 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      226 | 12526 | `		ph7_value *pObj = apArg[i];` |
|        - | 12527 | `		/* Reset the working buffer */` |
|      226 | 12528 | `		SyBlobReset(&sDump);` |
|        - | 12529 | `		/* Dump the given expression */` |
|      226 | 12530 | `		PH7_MemObjDump(&sDump,pObj,TRUE,0,0,0);` |
|        - | 12531 | `		/* Output */` |
|      226 | 12532 | `		if( SyBlobLength(&sDump) > 0 ){` |
|      226 | 12533 | `			ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|      112 | 12534 | `		}` |
|      114 | 12535 | `	}` |
|        - | 12536 | `	/* Release the working buffer */` |
|      220 | 12537 | `	SyBlobRelease(&sDump);` |
|      220 | 12538 | `	return SXRET_OK;` |
|        2 | 12539 |  |
|        - | 12540 | `/*` |
|        - | 12541 | ` * string/bool print_r(expression,[bool $return = FALSE])` |
|        - | 12542 | ` *   print-r - Prints human-readable information about a variable` |
|        - | 12543 | ` * Parameters` |
|        - | 12544 | ` *   expression: Expression to dump` |
|        - | 12545 | ` *   return : If you would like to capture the output of print_r() use` |
|        - | 12546 | ` *            the return parameter. When this parameter is set to TRUE` |
|        - | 12547 | ` *            print_r() will return the information rather than print it.` |
|        - | 12548 | ` * Return` |
|        - | 12549 | ` *  When the return parameter is TRUE, this function will return a string.` |
|        - | 12550 | ` *  Otherwise, the return value is TRUE.` |
|        - | 12551 | ` */` |
|       16 | 12552 | `static int vm_builtin_print_r(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12553 |  |
|       17 | 12554 | `	int ret_string = 0;` |
|        - | 12555 | `	SyBlob sDump;` |
|       17 | 12556 | `	if( nArg < 1 ){` |
|        - | 12557 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12558 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12559 | `		return SXRET_OK;` |
|        - | 12560 | `	}` |
|       17 | 12561 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|       17 | 12562 | `	if ( nArg > 1 ){` |
|        - | 12563 | `		/* Where to redirect output */` |
|       11 | 12564 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        5 | 12565 | `	}` |
|        - | 12566 | `	/* Generate dump */` |
|       17 | 12567 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|       17 | 12568 | `	if( !ret_string ){` |
|        - | 12569 | `		/* Output dump */` |
|        7 | 12570 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12571 | `		/* Return true */` |
|        7 | 12572 | `		ph7_result_bool(pCtx,1);` |
|        4 | 12573 | `	}else{` |
|        - | 12574 | `		/* Generated dump as return value */` |
|       11 | 12575 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12576 | `	}` |
|        - | 12577 | `	/* Release the working buffer */` |
|       17 | 12578 | `	SyBlobRelease(&sDump);` |
|       17 | 12579 | `	return SXRET_OK;` |
|        9 | 12580 |  |
|        - | 12581 | `/*` |
|        - | 12582 | ` * string/null var_export(expression,[bool $return = FALSE])` |
|        - | 12583 | ` * Same job as print_r. (see coment above)` |
|        - | 12584 | ` */` |
|        2 | 12585 | `static int vm_builtin_var_export(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12586 |  |
|        3 | 12587 | `	int ret_string = 0;` |
|        - | 12588 | `	SyBlob sDump;      /* Dump is stored in this BLOB */` |
|        3 | 12589 | `	if( nArg < 1 ){` |
|        - | 12590 | `		/* Nothing to output,return FALSE */` |
|      ! 0 | 12591 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12592 | `		return SXRET_OK;` |
|        - | 12593 | `	}` |
|        3 | 12594 | `	SyBlobInit(&sDump,&pCtx->pVm->sAllocator);` |
|        3 | 12595 | `	if ( nArg > 1 ){` |
|        - | 12596 | `		/* Where to redirect output */` |
|        3 | 12597 | `		ret_string = ph7_value_to_bool(apArg[1]);` |
|        1 | 12598 | `	}` |
|        - | 12599 | `	/* Generate dump */` |
|        3 | 12600 | `	PH7_MemObjDump(&sDump,apArg[0],FALSE,0,0,0);` |
|        3 | 12601 | `	if( !ret_string ){` |
|        - | 12602 | `		/* Output dump */` |
|      ! 0 | 12603 | `		ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12604 | `		/* Return NULL */` |
|      ! 0 | 12605 | `		ph7_result_null(pCtx);` |
|      ! 0 | 12606 | `	}else{` |
|        - | 12607 | `		/* Generated dump as return value */` |
|        3 | 12608 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 12609 | `	}` |
|        - | 12610 | `	/* Release the working buffer */` |
|        3 | 12611 | `	SyBlobRelease(&sDump);` |
|        3 | 12612 | `	return SXRET_OK;` |
|        2 | 12613 |  |
|        - | 12614 | `/*` |
|        - | 12615 | ` * int/bool assert_options(int $what [, mixed $value ])` |
|        - | 12616 | ` *  Set/get the various assert flags.` |
|        - | 12617 | ` * Parameter` |
|        - | 12618 | ` * $what` |
|        - | 12619 | ` *   ASSERT_ACTIVE          Enable assert() evaluation` |
|        - | 12620 | ` *   ASSERT_WARNING         Deprecated, accepted as no-op` |
|        - | 12621 | ` *   ASSERT_BAIL            Terminate execution on failed assertions` |
|        - | 12622 | ` *   ASSERT_QUIET_EVAL      Deprecated, accepted as no-op (removed in PHP 8.0)` |
|        - | 12623 | ` *   ASSERT_CALLBACK        Callback to call on failed assertions` |
|        - | 12624 | ` *   ASSERT_EXCEPTION       Always enabled in PHP 8` |
|        - | 12625 | ` * $value` |
|        - | 12626 | ` *   An optional new value for the option.` |
|        - | 12627 | ` * Return` |
|        - | 12628 | ` *  Old setting on success or FALSE on failure.` |
|        - | 12629 | ` */` |
|       28 | 12630 | `static int vm_builtin_assert_options(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12631 |  |
|       30 | 12632 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12633 | `	int iOption;` |
|        - | 12634 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       30 | 12635 | `	if( nArg < 1 ){` |
|        3 | 12636 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12637 | `			"ArgumentCountError",` |
|        - | 12638 | `			"assert_options() expects at least 1 argument, 0 given"` |
|        - | 12639 | `			);` |
|        - | 12640 | `	}` |
|        - | 12641 | `	/* PHP 8: TypeError for non-scalar option types */` |
|       26 | 12642 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0])` |
|       28 | 12643 | `		\|\| ph7_value_is_resource(apArg[0]) ){` |
|      ! 0 | 12644 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12645 | `			"TypeError",` |
|        - | 12646 | `			"assert_options(): Argument #1 ($option) must be of type int, %s given",` |
|      ! 0 | 12647 | `			ph7_value_is_array(apArg[0]) ? "array" :` |
|      ! 0 | 12648 | `			ph7_value_is_object(apArg[0]) ? "object" : "resource"` |
|        - | 12649 | `			);` |
|        - | 12650 | `	}` |
|       28 | 12651 | `	iOption = ph7_value_to_int(apArg[0]);` |
|        - | 12652 | `	/* PHP constant values: ASSERT_ACTIVE=1, ASSERT_CALLBACK=2,` |
|        - | 12653 | `	 * ASSERT_BAIL=3, ASSERT_WARNING=4, ASSERT_EXCEPTION=5,` |
|        - | 12654 | `	 * ASSERT_QUIET_EVAL=6 (deprecated) */` |
|       28 | 12655 | `	switch( iOption ){` |
|        5 | 12656 | `	case 1: /* ASSERT_ACTIVE */` |
|        - | 12657 | `		/* Return old value: 1 if active (not disabled), 0 if disabled */` |
|       12 | 12658 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_DISABLE) ? 0 : 1);` |
|       12 | 12659 | `		if( nArg > 1 ){` |
|        5 | 12660 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12661 | `				pVm->iAssertFlags &= ~PH7_ASSERT_DISABLE;` |
|        2 | 12662 | `			}else{` |
|        3 | 12663 | `				pVm->iAssertFlags \|= PH7_ASSERT_DISABLE;` |
|        - | 12664 | `			}` |
|        2 | 12665 | `		}` |
|       12 | 12666 | `		break;` |
|        1 | 12667 | `	case 2: /* ASSERT_CALLBACK */` |
|        - | 12668 | `		/* Return old callback or null */` |
|        3 | 12669 | `		if( pVm->iAssertFlags & PH7_ASSERT_CALLBACK ){` |
|      ! 0 | 12670 | `			ph7_result_value(pCtx, &pVm->sAssertCallback);` |
|      ! 0 | 12671 | `		}else{` |
|        3 | 12672 | `			ph7_result_null(pCtx);` |
|        - | 12673 | `		}` |
|        3 | 12674 | `		if( nArg > 1 ){` |
|      ! 0 | 12675 | `			if( ph7_value_is_callable(apArg[1]) ){` |
|      ! 0 | 12676 | `				PH7_MemObjStore(apArg[1],&pVm->sAssertCallback);` |
|      ! 0 | 12677 | `				pVm->iAssertFlags \|= PH7_ASSERT_CALLBACK;` |
|      ! 0 | 12678 | `			}else{` |
|      ! 0 | 12679 | `				pVm->iAssertFlags &= ~PH7_ASSERT_CALLBACK;` |
|        - | 12680 | `			}` |
|      ! 0 | 12681 | `		}` |
|        3 | 12682 | `		break;` |
|        5 | 12683 | `	case 3: /* ASSERT_BAIL */` |
|       11 | 12684 | `		ph7_result_int(pCtx, (pVm->iAssertFlags & PH7_ASSERT_BAIL) ? 1 : 0);` |
|       11 | 12685 | `		if( nArg > 1 ){` |
|        5 | 12686 | `			if( ph7_value_to_bool(apArg[1]) ){` |
|        3 | 12687 | `				pVm->iAssertFlags \|= PH7_ASSERT_BAIL;` |
|        2 | 12688 | `			}else{` |
|        3 | 12689 | `				pVm->iAssertFlags &= ~PH7_ASSERT_BAIL;` |
|        - | 12690 | `			}` |
|        2 | 12691 | `		}` |
|       11 | 12692 | `		break;` |
|      ! 0 | 12693 | `	case 4: /* ASSERT_WARNING — deprecated, accept but no-op */` |
|      ! 0 | 12694 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12695 | `		break;` |
|        1 | 12696 | `	case 5: /* ASSERT_EXCEPTION — always enabled in PHP 8 */` |
|        3 | 12697 | `		ph7_result_int(pCtx, 1);` |
|        3 | 12698 | `		break;` |
|      ! 0 | 12699 | `	case 6: /* ASSERT_QUIET_EVAL — removed in PHP 8.0, accept as no-op */` |
|      ! 0 | 12700 | `		ph7_result_int(pCtx, 0);` |
|      ! 0 | 12701 | `		break;` |
|        1 | 12702 | `	default:` |
|        - | 12703 | `		/* PHP 8: ValueError for invalid option */` |
|        3 | 12704 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12705 | `			"ValueError",` |
|        - | 12706 | `			"assert_options(): Argument #1 ($option) must be an ASSERT_* constant"` |
|        - | 12707 | `			);` |
|        - | 12708 | `	}` |
|       26 | 12709 | `	return PH7_OK;` |
|       16 | 12710 |  |
|        - | 12711 | `/*` |
|        - | 12712 | ` * bool assert(mixed $assertion)` |
|        - | 12713 | ` *  Checks if assertion is FALSE.` |
|        - | 12714 | ` * Parameter` |
|        - | 12715 | ` *  $assertion` |
|        - | 12716 | ` *    The assertion to test.` |
|        - | 12717 | ` * Return` |
|        - | 12718 | ` *  FALSE if the assertion is false, TRUE otherwise.` |
|        - | 12719 | ` */` |
|       24 | 12720 | `static int vm_builtin_assert(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12721 |  |
|       26 | 12722 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12723 | `	int iFlags,iResult;` |
|        - | 12724 | `	const char *zDesc;` |
|        - | 12725 | `	/* PHP 8: ArgumentCountError if no arguments */` |
|       26 | 12726 | `	if( nArg < 1 ){` |
|        3 | 12727 | `		return PH7_VmThrowException(pCtx,` |
|        - | 12728 | `			"ArgumentCountError",` |
|        - | 12729 | `			"assert() expects at least 1 argument, 0 given"` |
|        - | 12730 | `			);` |
|        - | 12731 | `	}` |
|       24 | 12732 | `	iFlags = pVm->iAssertFlags;` |
|       24 | 12733 | `	if( iFlags & PH7_ASSERT_DISABLE ){` |
|        - | 12734 | `		/* Assertion is disabled,return TRUE (PHP 8 behavior) */` |
|      ! 0 | 12735 | `		ph7_result_bool(pCtx,1);` |
|      ! 0 | 12736 | `		return PH7_OK;` |
|        - | 12737 | `	}` |
|        - | 12738 | `	/* PHP 8: No string evaluation.  All values are cast to boolean. */` |
|       24 | 12739 | `	iResult = ph7_value_to_bool(apArg[0]);` |
|       24 | 12740 | `	if( !iResult ){` |
|        - | 12741 | `		/* Assertion failed */` |
|        - | 12742 | `		/* Extract optional description */` |
|       13 | 12743 | `		zDesc = 0;` |
|       13 | 12744 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 12745 | `			zDesc = ph7_value_to_string(apArg[1],0);` |
|        1 | 12746 | `		}` |
|       13 | 12747 | `		if( iFlags & PH7_ASSERT_CALLBACK ){` |
|        - | 12748 | `			static const SyString sFileName = { ":Memory", sizeof(":Memory") - 1};` |
|        - | 12749 | `			ph7_value sFile,sLine;` |
|        - | 12750 | `			ph7_value *apCbArg[3];` |
|        - | 12751 | `			SyString *pFile;` |
|        - | 12752 | `			/* Extract the processed script */` |
|      ! 0 | 12753 | `			pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      ! 0 | 12754 | `			if( pFile == 0 ){` |
|      ! 0 | 12755 | `				pFile = (SyString *)&sFileName;` |
|      ! 0 | 12756 | `			}` |
|        - | 12757 | `			/* Invoke the callback */` |
|      ! 0 | 12758 | `			PH7_MemObjInitFromString(pVm,&sFile,pFile);` |
|      ! 0 | 12759 | `			PH7_MemObjInitFromInt(pVm,&sLine,0);` |
|      ! 0 | 12760 | `			apCbArg[0] = &sFile;` |
|      ! 0 | 12761 | `			apCbArg[1] = &sLine;` |
|      ! 0 | 12762 | `			apCbArg[2] = apArg[0];` |
|      ! 0 | 12763 | `			PH7_VmCallUserFunction(pVm,&pVm->sAssertCallback,3,apCbArg,0);` |
|        - | 12764 | `			/* Clean-up the mess left behind */` |
|      ! 0 | 12765 | `			PH7_MemObjRelease(&sFile);` |
|      ! 0 | 12766 | `			PH7_MemObjRelease(&sLine);` |
|      ! 0 | 12767 | `		}` |
|       13 | 12768 | `		if( iFlags & PH7_ASSERT_BAIL ){` |
|        - | 12769 | `			/* Abort VM execution immediately */` |
|      ! 0 | 12770 | `			return PH7_ABORT;` |
|        - | 12771 | `		}` |
|        - | 12772 | `		/* PHP 8: throw AssertionError by default */` |
|       13 | 12773 | `		if( zDesc && zDesc[0] != '\0' ){` |
|        4 | 12774 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12775 | `				"AssertionError",` |
|        - | 12776 | `				"%s",` |
|        1 | 12777 | `				zDesc` |
|        - | 12778 | `				);` |
|      ! 0 | 12779 | `		}else{` |
|       11 | 12780 | `			return PH7_VmThrowException(pCtx,` |
|        - | 12781 | `				"AssertionError",` |
|        - | 12782 | `				"assert(false)"` |
|        - | 12783 | `				);` |
|        - | 12784 | `		}` |
|        - | 12785 | `	}` |
|        - | 12786 | `	/* Assertion passed */` |
|       11 | 12787 | `	ph7_result_bool(pCtx,1);` |
|       11 | 12788 | `	return PH7_OK;` |
|       14 | 12789 |  |
|        - | 12790 | `/*` |
|        - | 12791 | ` * Section:` |
|        - | 12792 | ` *  Error reporting functions.` |
|        - | 12793 | ` * Status:` |
|        - | 12794 | ` *    Stable.` |
|        - | 12795 | ` */` |
|        - | 12796 | `/*` |
|        - | 12797 | ` * bool trigger_error(string $error_msg[,int $error_type = E_USER_NOTICE ])` |
|        - | 12798 | ` *  Generates a user-level error/warning/notice message.` |
|        - | 12799 | ` * Parameters` |
|        - | 12800 | ` *  $error_msg` |
|        - | 12801 | ` *   The designated error message for this error. It's limited to 1024 characters` |
|        - | 12802 | ` *   in length. Any additional characters beyond 1024 will be truncated.` |
|        - | 12803 | ` * $error_type` |
|        - | 12804 | ` *  The designated error type for this error. It only works with the E_USER family` |
|        - | 12805 | ` *  of constants, and will default to E_USER_NOTICE.` |
|        - | 12806 | ` * Return` |
|        - | 12807 | ` *  This function returns FALSE if wrong error_type is specified, TRUE otherwise.` |
|        - | 12808 | ` */` |
|       12 | 12809 | `static int vm_builtin_trigger_error(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12810 |  |
|       14 | 12811 | `	int nErr = PH7_CTX_NOTICE;` |
|       14 | 12812 | `	int rc = PH7_OK;` |
|       14 | 12813 | `	if( nArg > 0 ){` |
|        - | 12814 | `		const char *zErr;` |
|        - | 12815 | `		int nLen;` |
|        - | 12816 | `		/* Extract the error message */` |
|       12 | 12817 | `		zErr = ph7_value_to_string(apArg[0],&nLen);` |
|       12 | 12818 | `		if( nArg > 1 ){` |
|        - | 12819 | `			/* Extract the error type */` |
|       12 | 12820 | `			nErr = ph7_value_to_int(apArg[1]);` |
|       12 | 12821 | `			switch( nErr ){` |
|        1 | 12822 | `			case 1:   /* E_ERROR */` |
|        - | 12823 | `			case 16:  /* E_CORE_ERROR */` |
|        - | 12824 | `			case 64:  /* E_COMPILE_ERROR */` |
|        - | 12825 | `			case 256: /* E_USER_ERROR */` |
|        3 | 12826 | `				nErr = PH7_CTX_ERR;` |
|        3 | 12827 | `				rc = PH7_ABORT; /* Abort processing immediately */` |
|        3 | 12828 | `				break;` |
|        1 | 12829 | `			case 2:   /* E_WARNING */` |
|        - | 12830 | `			case 32:  /* E_CORE_WARNING */` |
|        - | 12831 | `			case 123: /* E_COMPILE_WARNING */` |
|        - | 12832 | `			case 512: /* E_USER_WARNING */` |
|        3 | 12833 | `				nErr = PH7_CTX_WARNING;` |
|        3 | 12834 | `				break;` |
|        3 | 12835 | `			default:` |
|        8 | 12836 | `				nErr = PH7_CTX_NOTICE;` |
|        6 | 12837 | `				break;` |
|        - | 12838 | `			}` |
|        5 | 12839 | `		}` |
|        - | 12840 | `		/* Report error */` |
|       12 | 12841 | `		rc = PH7_VmThrowError(pCtx->pVm, NULL, nErr, zErr);` |
|       12 | 12842 | `		if( rc == PH7_ABORT ){` |
|      ! 0 | 12843 | `			return rc;` |
|        - | 12844 | `		}` |
|        - | 12845 | `		/* Return true */` |
|       12 | 12846 | `		ph7_result_bool(pCtx,1);` |
|        7 | 12847 | `	}else{` |
|        - | 12848 | `		/* Missing arguments,return FALSE */` |
|        3 | 12849 | `		ph7_result_bool(pCtx,0);` |
|        - | 12850 | `	}` |
|       14 | 12851 | `	return rc;` |
|        8 | 12852 |  |
|        - | 12853 | `/*` |
|        - | 12854 | ` * int error_reporting([int $level])` |
|        - | 12855 | ` *  Sets which PHP errors are reported.` |
|        - | 12856 | ` * Parameters` |
|        - | 12857 | ` *  $level` |
|        - | 12858 | ` *   The new error_reporting level. It takes on either a bitmask, or named constants.` |
|        - | 12859 | ` *   Using named constants is strongly encouraged to ensure compatibility for future versions.` |
|        - | 12860 | ` *   As error levels are added, the range of integers increases, so older integer-based error` |
|        - | 12861 | ` *   levels will not always behave as expected.` |
|        - | 12862 | ` *   The available error level constants and the actual meanings of these error levels are described` |
|        - | 12863 | ` *   in the predefined constants.` |
|        - | 12864 | ` * Return` |
|        - | 12865 | ` *   Returns the old error_reporting level or the current level if no level` |
|        - | 12866 | ` *   parameter is given.` |
|        - | 12867 | ` */` |
|       38 | 12868 | `static int vm_builtin_error_reporting(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12869 |  |
|       40 | 12870 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12871 | `	int nOld;` |
|        - | 12872 | `	/* Extract the old reporting level */` |
|       40 | 12873 | `	nOld = pVm->bErrReport ? 32767 /* E_ALL */ : 0;` |
|       40 | 12874 | `	if( nArg > 0 ){` |
|        - | 12875 | `		int nNew;` |
|        - | 12876 | `		/* Extract the desired error reporting level */` |
|       32 | 12877 | `		nNew = ph7_value_to_int(apArg[0]);` |
|       32 | 12878 | `		if( !nNew ){` |
|        - | 12879 | `			/* Do not report errors at all */` |
|        5 | 12880 | `			pVm->bErrReport = 0;` |
|        3 | 12881 | `		}else{` |
|        - | 12882 | `			/* Report all errors */` |
|       28 | 12883 | `			pVm->bErrReport = 1;` |
|        - | 12884 | `		}` |
|       15 | 12885 | `	}` |
|        - | 12886 | `	/* Return the old level */` |
|       40 | 12887 | `	ph7_result_int(pCtx,nOld);` |
|       40 | 12888 | `	return PH7_OK;` |
|        2 | 12889 |  |
|        - | 12890 | `/*` |
|        - | 12891 | ` * bool error_log(string $message[,int $message_type = 0 [,string $destination[,string $extra_headers]]])` |
|        - | 12892 | ` *  Send an error message somewhere.` |
|        - | 12893 | ` * Parameter` |
|        - | 12894 | ` *  $message` |
|        - | 12895 | ` *   The error message that should be logged.` |
|        - | 12896 | ` *  $message_type` |
|        - | 12897 | ` *   Says where the error should go. The possible message types are as follows:` |
|        - | 12898 | ` *    0  message is sent to PHP's system logger, using the Operating System's system logging mechanism` |
|        - | 12899 | ` *       or a file, depending on what the error_log configuration directive is set to.` |
|        - | 12900 | ` *       This is the default option.` |
|        - | 12901 | ` *    1 message is sent by email to the address in the destination parameter.` |
|        - | 12902 | ` *      This is the only message type where the fourth parameter, extra_headers is used.` |
|        - | 12903 | ` *    2  No longer an option.` |
|        - | 12904 | ` *    3  message is appended to the file destination. A newline is not automatically added` |
|        - | 12905 | ` *       to the end of the message string.` |
|        - | 12906 | ` *    4  message is sent directly to the SAPI logging handler.` |
|        - | 12907 | ` *  $destination` |
|        - | 12908 | ` *   The destination. Its meaning depends on the message_type parameter as described above.` |
|        - | 12909 | ` *  $extra_headers` |
|        - | 12910 | ` *   The extra headers. It's used when the message_type parameter is set to 1` |
|        - | 12911 | ` * Return` |
|        - | 12912 | ` *  TRUE on success or FALSE on failure.` |
|        - | 12913 | ` * NOTE:` |
|        - | 12914 | ` *  Actually,PH7 does not care about the given parameters,all this function does` |
|        - | 12915 | ` *  is to invoke any user callback registered using the PH7_VM_CONFIG_ERR_LOG_HANDLER` |
|        - | 12916 | ` *  configuration directive (refer to the official documentation for more information).` |
|        - | 12917 | ` *  Otherwise this function is no-op.` |
|        - | 12918 | ` */` |
|        4 | 12919 | `static int vm_builtin_error_log(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12920 |  |
|        - | 12921 | `	const char *zMessage,*zDest,*zHeader;` |
|        5 | 12922 | `	ph7_vm *pVm = pCtx->pVm;` |
|        5 | 12923 | `	int iType = 0;` |
|        5 | 12924 | `	if( nArg < 1 ){` |
|        - | 12925 | `		/* Missing log message,return FALSE */` |
|      ! 0 | 12926 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 12927 | `		return PH7_OK;` |
|        - | 12928 | `	}` |
|        5 | 12929 | `	if( pVm->xErrLog  ){` |
|        - | 12930 | `		/* Invoke the user callback */` |
|      ! 0 | 12931 | `		zMessage = ph7_value_to_string(apArg[0],0);` |
|      ! 0 | 12932 | `		zDest = zHeader = ""; /* Empty string */` |
|      ! 0 | 12933 | `		if( nArg > 1 ){` |
|      ! 0 | 12934 | `			iType = ph7_value_to_int(apArg[1]);` |
|      ! 0 | 12935 | `			if( nArg > 2 ){` |
|      ! 0 | 12936 | `				zDest = ph7_value_to_string(apArg[2],0);` |
|      ! 0 | 12937 | `				if( nArg > 3 ){` |
|      ! 0 | 12938 | `					zHeader = ph7_value_to_string(apArg[3],0);` |
|      ! 0 | 12939 | `				}` |
|      ! 0 | 12940 | `			}` |
|      ! 0 | 12941 | `		}` |
|      ! 0 | 12942 | `		pVm->xErrLog(zMessage,iType,zDest,zHeader);` |
|      ! 0 | 12943 | `	}` |
|        - | 12944 | `	/* Retun TRUE */` |
|        5 | 12945 | `	ph7_result_bool(pCtx,1);` |
|        5 | 12946 | `	return PH7_OK;` |
|        3 | 12947 |  |
|        - | 12948 | `/*` |
|        - | 12949 | ` * bool restore_exception_handler(void)` |
|        - | 12950 | ` *  Restores the previously defined exception handler function.` |
|        - | 12951 | ` * Parameter` |
|        - | 12952 | ` *  None` |
|        - | 12953 | ` * Return` |
|        - | 12954 | ` *  TRUE if the exception handler is restored.FALSE otherwise` |
|        - | 12955 | ` */` |
|        4 | 12956 | `static int vm_builtin_restore_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 12957 |  |
|        5 | 12958 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12959 | `	ph7_value *pOld,*pNew;` |
|        - | 12960 | `	/* Point to the old and the new handler */` |
|        5 | 12961 | `	pOld = &pVm->aExceptionCB[0];` |
|        5 | 12962 | `	pNew = &pVm->aExceptionCB[1];` |
|        5 | 12963 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        2 | 12964 | `		SXUNUSED(nArg); /* cc warning */` |
|        2 | 12965 | `		SXUNUSED(apArg);` |
|        - | 12966 | `		/* No installed handler,return FALSE */` |
|        5 | 12967 | `		ph7_result_bool(pCtx,0);` |
|        5 | 12968 | `		return PH7_OK;` |
|        - | 12969 | `	}` |
|        - | 12970 | `	/* Copy the old handler */` |
|      ! 0 | 12971 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 12972 | `	PH7_MemObjRelease(pOld);` |
|        - | 12973 | `	/* Return TRUE */` |
|      ! 0 | 12974 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 12975 | `	return PH7_OK;` |
|        3 | 12976 |  |
|        - | 12977 | `/*` |
|        - | 12978 | ` * callable set_exception_handler(callable $exception_handler)` |
|        - | 12979 | ` *  Sets a user-defined exception handler function.` |
|        - | 12980 | ` *  Sets the default exception handler if an exception is not caught within a try/catch block.` |
|        - | 12981 | ` * NOTE` |
|        - | 12982 | ` *  Execution will NOT stop after the exception_handler calls for example die/exit unlike` |
|        - | 12983 | ` *  the satndard PHP engine.` |
|        - | 12984 | ` * Parameters` |
|        - | 12985 | ` *  $exception_handler` |
|        - | 12986 | ` *   Name of the function to be called when an uncaught exception occurs.` |
|        - | 12987 | ` *   This handler function needs to accept one parameter, which will be the exception object` |
|        - | 12988 | ` *   that was thrown.` |
|        - | 12989 | ` *  Note:` |
|        - | 12990 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 12991 | ` * Return` |
|        - | 12992 | ` *  Returns the name of the previously defined exception handler, or NULL on error.` |
|        - | 12993 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 12994 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 12995 | ` */` |
|        4 | 12996 | `static int vm_builtin_set_exception_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 12997 |  |
|        6 | 12998 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 12999 | `	ph7_value *pOld,*pNew;` |
|        - | 13000 | `	/* Point to the old and the new handler */` |
|        6 | 13001 | `	pOld = &pVm->aExceptionCB[0];` |
|        6 | 13002 | `	pNew = &pVm->aExceptionCB[1];` |
|        - | 13003 | `	/* Return the old handler */` |
|        6 | 13004 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|        6 | 13005 | `	if( nArg > 0 ){` |
|        6 | 13006 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13007 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|      ! 0 | 13008 | `			PH7_MemObjRelease(pNew);` |
|      ! 0 | 13009 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 13010 | `		}else{` |
|        6 | 13011 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13012 | `			/* Install the new handler */` |
|        6 | 13013 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13014 | `		}` |
|        2 | 13015 | `	}` |
|        6 | 13016 | `	return PH7_OK;` |
|        2 | 13017 |  |
|        - | 13018 | `/*` |
|        - | 13019 | ` * bool restore_error_handler(void)` |
|        - | 13020 | ` *  THIS FUNCTION IS A NO-OP IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13021 | ` * Parameters:` |
|        - | 13022 | ` *  None.` |
|        - | 13023 | ` * Return` |
|        - | 13024 | ` *  Always TRUE.` |
|        - | 13025 | ` */` |
|        6 | 13026 | `static int vm_builtin_restore_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13027 |  |
|        7 | 13028 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13029 | `	ph7_value *pOld,*pNew;` |
|        - | 13030 | `	/* Point to the old and the new handler */` |
|        7 | 13031 | `	pOld = &pVm->aErrCB[0];` |
|        7 | 13032 | `	pNew = &pVm->aErrCB[1];` |
|        7 | 13033 | `	if( pOld->iFlags & MEMOBJ_NULL ){` |
|        3 | 13034 | `		SXUNUSED(nArg); /* cc warning */` |
|        3 | 13035 | `		SXUNUSED(apArg);` |
|        - | 13036 | `		/* No installed callback,return FALSE */` |
|        7 | 13037 | `		ph7_result_bool(pCtx,0);` |
|        7 | 13038 | `		return PH7_OK;` |
|        - | 13039 | `	}` |
|        - | 13040 | `	/* Copy the old callback */` |
|      ! 0 | 13041 | `	PH7_MemObjStore(pOld,pNew);` |
|      ! 0 | 13042 | `	PH7_MemObjRelease(pOld);` |
|        - | 13043 | `	/* Return TRUE */` |
|      ! 0 | 13044 | `	ph7_result_bool(pCtx,1);` |
|      ! 0 | 13045 | `	return PH7_OK;` |
|        4 | 13046 |  |
|        - | 13047 | `/*` |
|        - | 13048 | ` * value set_error_handler(callable $error_handler)` |
|        - | 13049 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13050 | ` *   THIS FUNCTION IS DISABLED IN THE CURRENT RELEASE OF THE PH7 ENGINE.` |
|        - | 13051 | ` *  +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++` |
|        - | 13052 | ` *  Sets a user-defined error handler function.` |
|        - | 13053 | ` *  This function can be used for defining your own way of handling errors during` |
|        - | 13054 | ` *  runtime, for example in applications in which you need to do cleanup of data/files` |
|        - | 13055 | ` *  when a critical error happens, or when you need to trigger an error under certain` |
|        - | 13056 | ` *  conditions (using trigger_error()).` |
|        - | 13057 | ` * Parameters` |
|        - | 13058 | ` *  $error_handler` |
|        - | 13059 | ` *   The user function needs to accept two parameters: the error code, and a string` |
|        - | 13060 | ` *   describing the error.` |
|        - | 13061 | ` *   Then there are three optional parameters that may be supplied: the filename in which` |
|        - | 13062 | ` *   the error occurred, the line number in which the error occurred, and the context in which` |
|        - | 13063 | ` *   the error occurred (an array that points to the active symbol table at the point the error occurred).` |
|        - | 13064 | ` *   The function can be shown as:` |
|        - | 13065 | ` *    handler ( int $errno , string $errstr [, string $errfile])` |
|        - | 13066 | ` *     errno` |
|        - | 13067 | ` *       The first parameter, errno, contains the level of the error raised, as an integer.` |
|        - | 13068 | ` *   errstr` |
|        - | 13069 | ` *      The second parameter, errstr, contains the error message, as a string.` |
|        - | 13070 | ` *   errfile` |
|        - | 13071 | ` *      The third parameter is optional, errfile, which contains the filename that the error` |
|        - | 13072 | ` *     was raised in, as a string.` |
|        - | 13073 | ` *  Note:` |
|        - | 13074 | ` *   NULL may be passed instead, to reset this handler to its default state.` |
|        - | 13075 | ` * Return` |
|        - | 13076 | ` *  Returns the name of the previously defined error handler, or NULL on error.` |
|        - | 13077 | ` *  If no previous handler was defined, NULL is also returned. If NULL is passed` |
|        - | 13078 | ` *  resetting the handler to its default state, TRUE is returned.` |
|        - | 13079 | ` */` |
|    10608 | 13080 | `static int vm_builtin_set_error_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13081 |  |
|    10610 | 13082 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13083 | `	ph7_value *pOld,*pNew;` |
|        - | 13084 | `	/* Point to the old and the new handler */` |
|    10610 | 13085 | `	pOld = &pVm->aErrCB[0];` |
|    10610 | 13086 | `	pNew = &pVm->aErrCB[1];` |
|        - | 13087 | `	/* Return the old handler */` |
|    10610 | 13088 | `	ph7_result_value(pCtx,pOld); /* Will make it's own copy */` |
|    10610 | 13089 | `	if( nArg > 0 ){` |
|    10610 | 13090 | `		if( !ph7_value_is_callable(apArg[0])) {` |
|        - | 13091 | `			/* Not callable,return TRUE (As requested by the PHP specification) */` |
|     5299 | 13092 | `			PH7_MemObjRelease(pNew);` |
|     5299 | 13093 | `			ph7_result_bool(pCtx,1);` |
|     2650 | 13094 | `		}else{` |
|     5312 | 13095 | `			PH7_MemObjStore(pNew,pOld);` |
|        - | 13096 | `			/* Install the new handler */` |
|     5312 | 13097 | `			PH7_MemObjStore(apArg[0],pNew);` |
|        - | 13098 | `		}` |
|     5304 | 13099 | `	}` |
|    10610 | 13100 | `	return PH7_OK;` |
|        2 | 13101 |  |
|        - | 13102 | `/*` |
|        - | 13103 | ` * array debug_backtrace([ int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT [, int $limit = 0 ]] )` |
|        - | 13104 | ` *  Generates a backtrace.` |
|        - | 13105 | ` * Paramaeter` |
|        - | 13106 | ` *  $options` |
|        - | 13107 | ` *   DEBUG_BACKTRACE_PROVIDE_OBJECT: Whether or not to populate the "object" index.` |
|        - | 13108 | ` *   DEBUG_BACKTRACE_IGNORE_ARGS 	Whether or not to omit the "args" index, and thus` |
|        - | 13109 | ` *   all the function/method arguments, to save memory.` |
|        - | 13110 | ` * $limit` |
|        - | 13111 | ` *   (Not Used)` |
|        - | 13112 | ` * Return` |
|        - | 13113 | ` *  An array.The possible returned elements are as follows:` |
|        - | 13114 | ` *          Possible returned elements from debug_backtrace()` |
|        - | 13115 | ` *          Name        Type      Description` |
|        - | 13116 | ` *          ------      ------     -----------` |
|        - | 13117 | ` *          function    string    The current function name. See also __FUNCTION__.` |
|        - | 13118 | ` *          line        integer   The current line number. See also __LINE__.` |
|        - | 13119 | ` *          file 	    string 	  The current file name. See also __FILE__.` |
|        - | 13120 | ` *          class       string    The current class name. See also __CLASS__` |
|        - | 13121 | ` *          object      object    The current object.` |
|        - | 13122 | ` *          args        array     If inside a function, this lists the functions arguments.` |
|        - | 13123 | ` *                                If inside an included file, this lists the included file name(s).` |
|        - | 13124 | ` */` |
|      868 | 13125 | `static int vm_builtin_debug_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 13126 |  |
|      870 | 13127 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13128 | `	ph7_value *pArray;` |
|        - | 13129 | `	ph7_class *pClass;` |
|        - | 13130 | `	ph7_value *pValue;` |
|        - | 13131 | `	SyString *pFile;` |
|        - | 13132 | `	/* Create a new array */` |
|      870 | 13133 | `	pArray = ph7_context_new_array(pCtx);` |
|      870 | 13134 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      870 | 13135 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13136 | `		/* Out of memory,return NULL */` |
|      ! 0 | 13137 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|      ! 0 | 13138 | `		ph7_result_null(pCtx);` |
|      ! 0 | 13139 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 13140 | `		SXUNUSED(apArg);` |
|      ! 0 | 13141 | `		return PH7_OK;` |
|        - | 13142 | `	}` |
|        - | 13143 | `	/* Dump running function name and it's arguments  */` |
|      870 | 13144 | `	if( pVm->pFrame->pParent ){` |
|      870 | 13145 | `		VmFrame *pFrame = pVm->pFrame;` |
|        - | 13146 | `		ph7_vm_func *pFunc;` |
|        - | 13147 | `		ph7_value *pArg;` |
|      870 | 13148 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      870 | 13149 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|      870 | 13150 | `		if( pFrame->pParent && pFunc ){` |
|      870 | 13151 | `			ph7_value_string(pValue,pFunc->sName.zString,(int)pFunc->sName.nByte);` |
|      870 | 13152 | `			ph7_array_add_strkey_elem(pArray,"function",pValue);` |
|      870 | 13153 | `			ph7_value_reset_string_cursor(pValue);` |
|      434 | 13154 | `		}` |
|        - | 13155 | `		/* Function arguments */` |
|      870 | 13156 | `		pArg = ph7_context_new_array(pCtx);` |
|      870 | 13157 | `		if( pArg  ){` |
|        - | 13158 | `			ph7_value *pObj;` |
|        - | 13159 | `			VmSlot *aSlot;` |
|        - | 13160 | `			sxu32 n;` |
|        - | 13161 | `			/* Start filling the array with the given arguments */` |
|      870 | 13162 | `			aSlot = (VmSlot *)SySetBasePtr(&pFrame->sArg);` |
|     3478 | 13163 | `			for( n = 0;  n < SySetUsed(&pFrame->sArg) ; n++ ){` |
|     2610 | 13164 | `				pObj = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,aSlot[n].nIdx);` |
|     2610 | 13165 | `				if( pObj ){` |
|     2610 | 13166 | `					ph7_array_add_elem(pArg,0/* Automatic index assign*/,pObj);` |
|     1304 | 13167 | `				}` |
|     1306 | 13168 | `			}` |
|        - | 13169 | `			/* Save the array */` |
|      870 | 13170 | `			ph7_array_add_strkey_elem(pArray,"args",pArg);` |
|      434 | 13171 | `		}` |
|      434 | 13172 | `	}` |
|      870 | 13173 | `	ph7_value_int(pValue,1);` |
|        - | 13174 | `	/* Append the current line (which is always 1 since PH7 does not track` |
|        - | 13175 | `	 * line numbers at run-time. )` |
|        - | 13176 | `	 */` |
|      870 | 13177 | `	ph7_array_add_strkey_elem(pArray,"line",pValue);` |
|        - | 13178 | `	/* Current processed script */` |
|      870 | 13179 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      870 | 13180 | `	if( pFile ){` |
|      870 | 13181 | `		ph7_value_string(pValue,pFile->zString,(int)pFile->nByte);` |
|      870 | 13182 | `		ph7_array_add_strkey_elem(pArray,"file",pValue);` |
|      870 | 13183 | `		ph7_value_reset_string_cursor(pValue);` |
|      434 | 13184 | `	}` |
|        - | 13185 | `	/* Top class */` |
|      870 | 13186 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|      870 | 13187 | `	if( pClass ){` |
|      866 | 13188 | `		ph7_value_reset_string_cursor(pValue);` |
|      866 | 13189 | `		ph7_value_string(pValue,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      866 | 13190 | `		ph7_array_add_strkey_elem(pArray,"class",pValue);` |
|      432 | 13191 | `	}` |
|        - | 13192 | `	/* Return the freshly created array */` |
|      870 | 13193 | `	ph7_result_value(pCtx,pArray);` |
|        - | 13194 | `	/*` |
|        - | 13195 | `	 * Don't worry about freeing memory, everything will be released automatically` |
|        - | 13196 | `	 * as soon we return from this function.` |
|        - | 13197 | `	 */` |
|      870 | 13198 | `	return PH7_OK;` |
|      436 | 13199 |  |
|        - | 13200 | `/*` |
|        - | 13201 | ` * Generate a small backtrace.` |
|        - | 13202 | ` * Store the generated dump in the given BLOB` |
|        - | 13203 | ` */` |
|        4 | 13204 | `static int VmMiniBacktrace(` |
|        - | 13205 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13206 | `	SyBlob *pOut /* Store Dump here */` |
|        - | 13207 | `	)` |
|        1 | 13208 |  |
|        5 | 13209 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 13210 | `	ph7_vm_func *pFunc;` |
|        - | 13211 | `	ph7_class *pClass;` |
|        - | 13212 | `	SyString *pFile;` |
|        - | 13213 | `	/* Called function */` |
|        5 | 13214 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|        5 | 13215 | `	pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|        5 | 13216 | `	SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13217 | `	if( pFrame->pParent && pFunc ){` |
|        5 | 13218 | `		SyBlobAppend(pOut,"Called function: ",sizeof("Called function: ")-1);` |
|        5 | 13219 | `		SyBlobAppend(pOut,pFunc->sName.zString,pFunc->sName.nByte);` |
|        3 | 13220 | `	}else{` |
|      ! 0 | 13221 | `		SyBlobAppend(pOut,"Global scope",sizeof("Global scope") - 1);` |
|        - | 13222 | `	}` |
|        5 | 13223 | `	SyBlobAppend(pOut,"]",sizeof(char));` |
|        - | 13224 | `	/* Current processed script */` |
|        5 | 13225 | `	pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|        5 | 13226 | `	if( pFile ){` |
|        5 | 13227 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|        5 | 13228 | `		SyBlobAppend(pOut,"Processed file: ",sizeof("Processed file: ")-1);` |
|        5 | 13229 | `		SyBlobAppend(pOut,pFile->zString,pFile->nByte);` |
|        5 | 13230 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|        2 | 13231 | `	}` |
|        - | 13232 | `	/* Top class */` |
|        5 | 13233 | `	pClass = PH7_VmPeekTopClass(pVm);` |
|        5 | 13234 | `	if( pClass ){` |
|      ! 0 | 13235 | `		SyBlobAppend(pOut,"[",sizeof(char));` |
|      ! 0 | 13236 | `		SyBlobAppend(pOut,"Class: ",sizeof("Class: ")-1);` |
|      ! 0 | 13237 | `		SyBlobAppend(pOut,pClass->sName.zString,pClass->sName.nByte);` |
|      ! 0 | 13238 | `		SyBlobAppend(pOut,"]",sizeof(char));` |
|      ! 0 | 13239 | `	}` |
|        5 | 13240 | `	SyBlobAppend(pOut,"\n",sizeof(char));` |
|        - | 13241 | `	/* All done */` |
|        5 | 13242 | `	return SXRET_OK;` |
|        1 | 13243 |  |
|        - | 13244 | `/*` |
|        - | 13245 | ` * void debug_print_backtrace()` |
|        - | 13246 | ` *  Prints a backtrace` |
|        - | 13247 | ` * Parameters` |
|        - | 13248 | ` * None` |
|        - | 13249 | ` * Return` |
|        - | 13250 | ` * NULL` |
|        - | 13251 | ` */` |
|        2 | 13252 | `static int vm_builtin_debug_print_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13253 |  |
|        3 | 13254 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13255 | `	SyBlob sDump;` |
|        3 | 13256 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13257 | `	/* Generate the backtrace */` |
|        3 | 13258 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13259 | `	/* Output backtrace */` |
|        3 | 13260 | `	ph7_context_output(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump));` |
|        - | 13261 | `	/* All done,cleanup */` |
|        3 | 13262 | `	SyBlobRelease(&sDump);` |
|        1 | 13263 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13264 | `	SXUNUSED(apArg);` |
|        3 | 13265 | `	return PH7_OK;` |
|        1 | 13266 |  |
|        - | 13267 | `/*` |
|        - | 13268 | ` * string debug_string_backtrace()` |
|        - | 13269 | ` *  Generate a backtrace` |
|        - | 13270 | ` * Parameters` |
|        - | 13271 | ` * None` |
|        - | 13272 | ` * Return` |
|        - | 13273 | ` *  A mini backtrace().` |
|        - | 13274 | ` * Note that this is a symisc extension.` |
|        - | 13275 | ` */` |
|        2 | 13276 | `static int vm_builtin_debug_string_backtrace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13277 |  |
|        3 | 13278 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 13279 | `	SyBlob sDump;` |
|        3 | 13280 | `	SyBlobInit(&sDump,&pVm->sAllocator);` |
|        - | 13281 | `	/* Generate the backtrace */` |
|        3 | 13282 | `	VmMiniBacktrace(pVm,&sDump);` |
|        - | 13283 | `	/* Return the backtrace */` |
|        3 | 13284 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sDump),(int)SyBlobLength(&sDump)); /* Will make it's own copy */` |
|        - | 13285 | `	/* All done,cleanup */` |
|        3 | 13286 | `	SyBlobRelease(&sDump);` |
|        1 | 13287 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13288 | `	SXUNUSED(apArg);` |
|        3 | 13289 | `	return PH7_OK;` |
|        1 | 13290 |  |
|        - | 13291 | `/*` |
|        - | 13292 | ` * The following routine is invoked by the engine when an uncaught` |
|        - | 13293 | ` * exception is triggered.` |
|        - | 13294 | ` */` |
|      512 | 13295 | `static sxi32 VmUncaughtException(` |
|        - | 13296 | `	ph7_vm *pVm, /* Target VM */` |
|        - | 13297 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13298 | `	)` |
|        1 | 13299 |  |
|        - | 13300 | `	ph7_value *apArg[2],sArg;` |
|      513 | 13301 | `	int nArg = 1;` |
|        - | 13302 | `	sxi32 rc;` |
|      513 | 13303 | `	if( pVm->nExceptDepth > 15 ){` |
|        - | 13304 | `		/* Nesting limit reached */` |
|      ! 0 | 13305 | `		return SXRET_OK;` |
|        - | 13306 | `	}` |
|        - | 13307 | `	/* Call any exception handler if available */` |
|      513 | 13308 | `	PH7_MemObjInit(pVm,&sArg);` |
|      513 | 13309 | `	if( pThis ){` |
|        - | 13310 | `		/* Load the exception instance */` |
|      513 | 13311 | `		sArg.x.pOther = pThis;` |
|      513 | 13312 | `		pThis->iRef++;` |
|      513 | 13313 | `		MemObjSetType(&sArg,MEMOBJ_OBJ);` |
|      257 | 13314 | `	}else{` |
|      ! 0 | 13315 | `		nArg = 0;` |
|        - | 13316 | `	}` |
|      513 | 13317 | `	apArg[0] = &sArg;` |
|        - | 13318 | `	/* Call the exception handler if available */` |
|      513 | 13319 | `	pVm->nExceptDepth++;` |
|      513 | 13320 | `	rc = PH7_VmCallUserFunction(&(*pVm),&pVm->aExceptionCB[1],nArg,apArg,0);` |
|      513 | 13321 | `	pVm->nExceptDepth--;` |
|      513 | 13322 | `	if( rc != SXRET_OK ){` |
|        - | 13323 | `		SyBlob sMsgBuf;` |
|      511 | 13324 | `		const char *zClass = "Exception";` |
|      511 | 13325 | `		sxu32 nClass = (sxu32)sizeof("Exception") - 1;` |
|        - | 13326 | `		const char *zMsg;` |
|        - | 13327 | `		sxu32 nMsg;` |
|        - | 13328 | `		const char *zFuncName;` |
|        - | 13329 | `		int nFuncLen;` |
|      511 | 13330 | `		SyBlobInit(&sMsgBuf,&pVm->sAllocator);` |
|      511 | 13331 | `		if( pThis ){` |
|        - | 13332 | `			ph7_class_method *pGetMessage;` |
|        - | 13333 | `			ph7_value sMsg;` |
|        - | 13334 | `			const char *zTmp;` |
|        - | 13335 | `			int nTmp;` |
|      511 | 13336 | `			zClass = pThis->pClass->sName.zString;` |
|      511 | 13337 | `			nClass = pThis->pClass->sName.nByte;` |
|      511 | 13338 | `			pGetMessage = PH7_ClassExtractMethod(pThis->pClass,"getMessage",sizeof("getMessage")-1);` |
|      511 | 13339 | `			if( pGetMessage ){` |
|      511 | 13340 | `				PH7_MemObjInit(pVm,&sMsg);` |
|      511 | 13341 | `				if( PH7_VmCallClassMethod(&(*pVm),pThis,pGetMessage,&sMsg,0,0) == SXRET_OK ){` |
|      511 | 13342 | `					zTmp = ph7_value_to_string(&sMsg,&nTmp);` |
|      511 | 13343 | `					if( zTmp && nTmp > 0 ){` |
|      511 | 13344 | `						SyBlobAppend(&sMsgBuf,zTmp,(sxu32)nTmp);` |
|      255 | 13345 | `					}` |
|      255 | 13346 | `				}` |
|      511 | 13347 | `				PH7_MemObjRelease(&sMsg);` |
|      255 | 13348 | `			}` |
|      255 | 13349 | `		}` |
|      511 | 13350 | `		zMsg = (const char *)SyBlobData(&sMsgBuf);` |
|      511 | 13351 | `		nMsg = (sxu32)SyBlobLength(&sMsgBuf);` |
|      511 | 13352 | `		VmGetFrameContext(pVm,&zFuncName,&nFuncLen);` |
|      511 | 13353 | `		VmReportUncaughtException(pVm,zClass,nClass,zMsg,nMsg,zFuncName,nFuncLen);` |
|      511 | 13354 | `		SyBlobRelease(&sMsgBuf);` |
|        - | 13355 | `		/* Tell the upper layer to stop VM execution immediately  */` |
|      511 | 13356 | `		rc = SXERR_ABORT;` |
|      255 | 13357 | `	}` |
|      513 | 13358 | `	PH7_MemObjRelease(&sArg);` |
|      513 | 13359 | `	return rc;` |
|      257 | 13360 |  |
|        - | 13361 | `/*` |
|        - | 13362 | ` * Throw a user exception.` |
|        - | 13363 | ` *` |
|        - | 13364 | ` * Exception dispatch follows this sequence:` |
|        - | 13365 | ` *` |
|        - | 13366 | ` * 1. Walk the exception stack (pVm->aException) from top to find a` |
|        - | 13367 | ` *    try/catch whose catch block matches the exception class.` |
|        - | 13368 | ` *` |
|        - | 13369 | ` * 2. If NO catch matches:` |
|        - | 13370 | ` *    a. Run finally (if present) for the current try block.` |
|        - | 13371 | ` *    b. If outer handlers exist on the stack, re-throw recursively.` |
|        - | 13372 | ` *    c. If we're inside a catch body (VM_FRAME_CATCH on frame stack)` |
|        - | 13373 | ` *       whose outer handlers were temporarily hidden, DEFER the` |
|        - | 13374 | ` *       exception in pVm->pPendingException instead of reporting it` |
|        - | 13375 | ` *       uncaught. It will be re-thrown after finally runs (step 3d).` |
|        - | 13376 | ` *    d. Otherwise, report as truly uncaught.` |
|        - | 13377 | ` *` |
|        - | 13378 | ` * 3. If a catch DOES match:` |
|        - | 13379 | ` *    a. Temporarily HIDE all outer exception handlers by saving the` |
|        - | 13380 | ` *       aException stack and resetting it. This prevents a re-throw` |
|        - | 13381 | ` *       inside the catch body from immediately propagating past our` |
|        - | 13382 | ` *       finally block.` |
|        - | 13383 | ` *    b. Execute the catch body via VmLocalExec in a VM_FRAME_CATCH` |
|        - | 13384 | ` *       frame. If the catch body throws, dispatch recurses but finds` |
|        - | 13385 | ` *       no handlers (they're hidden), so the exception is deferred` |
|        - | 13386 | ` *       in pPendingException (step 2c).` |
|        - | 13387 | ` *    c. Restore outer handlers from the saved copy.` |
|        - | 13388 | ` *    d. Run finally (if present).` |
|        - | 13389 | ` *    e. If pPendingException is set (catch re-threw), re-throw it now` |
|        - | 13390 | ` *       that handlers are restored and finally has run.` |
|        - | 13391 | ` */` |
|      816 | 13392 | `static sxi32 VmThrowException(` |
|        - | 13393 | `	ph7_vm *pVm,              /* Target VM */` |
|        - | 13394 | `	ph7_class_instance *pThis /* Exception class instance [i.e: Exception $e] */` |
|        - | 13395 | `	)` |
|        2 | 13396 |  |
|        - | 13397 | `	ph7_exception_block *pCatch; /* Catch block to execute */` |
|        - | 13398 | `	ph7_exception **apException;` |
|        - | 13399 | `	ph7_exception *pException;` |
|        - | 13400 | `	/* An in-flight throw abandons any pending null-coalesce-assign store:` |
|        - | 13401 | `	 * disarm so the RHS-evaluation throw can't leave the slot live for a` |
|        - | 13402 | `	 * later unrelated NULLC_STORE (stale offsetSet) or leak the instance ref. */` |
|      818 | 13403 | `	VmCoalesceDisarm(pVm);` |
|        - | 13404 | `	/* Point to the stack of loaded exceptions */` |
|      818 | 13405 | `	apException = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      818 | 13406 | `	pException = 0;` |
|      818 | 13407 | `	pCatch = 0;` |
|      818 | 13408 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13409 | `		ph7_exception_block *aCatch;` |
|        - | 13410 | `		ph7_class *pClass;` |
|        - | 13411 | `		SyString *aNames;` |
|        - | 13412 | `		sxu32 nNames;` |
|        - | 13413 | `		int matched;` |
|        - | 13414 | `		sxu32 j,k;` |
|        - | 13415 | `		/* Locate the appropriate block to execute */` |
|      298 | 13416 | `		pException = apException[SySetUsed(&pVm->aException) - 1];` |
|      298 | 13417 | `		(void)SySetPop(&pVm->aException);` |
|      298 | 13418 | `		aCatch = (ph7_exception_block *)SySetBasePtr(&pException->sEntry);` |
|      306 | 13419 | `		for( j = 0 ; j < SySetUsed(&pException->sEntry) ; ++j ){` |
|        - | 13420 | `			/* Iterate over all class names in this catch block (multi-catch support) */` |
|      304 | 13421 | `			aNames = (SyString *)SySetBasePtr(&aCatch[j].aClasses);` |
|      304 | 13422 | `			nNames = SySetUsed(&aCatch[j].aClasses);` |
|      304 | 13423 | `			matched = 0;` |
|      330 | 13424 | `			for( k = 0 ; k < nNames ; ++k ){` |
|        - | 13425 | `				/* Extract the target class or interface (iLoadable=FALSE so` |
|        - | 13426 | `				 * interfaces like Throwable are resolvable as catch targets).` |
|        - | 13427 | `				 * Traits are never instance-compatible, so skip them explicitly. */` |
|      322 | 13428 | `				pClass = PH7_VmExtractClass(&(*pVm),aNames[k].zString,aNames[k].nByte,FALSE,0);` |
|      322 | 13429 | `				if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|        - | 13430 | `					/* No such class, or trait — cannot match */` |
|      ! 0 | 13431 | `					continue;` |
|        - | 13432 | `				}` |
|      322 | 13433 | `				if( PH7_VmInstanceOf(pThis->pClass,pClass) ){` |
|      296 | 13434 | `					matched = 1;` |
|      296 | 13435 | `					break;` |
|        - | 13436 | `				}` |
|       14 | 13437 | `			}` |
|      304 | 13438 | `			if( matched ){` |
|        - | 13439 | `				/* Catch block found,break immediately */` |
|      296 | 13440 | `				pCatch = &aCatch[j];` |
|      296 | 13441 | `				break;` |
|        - | 13442 | `			}` |
|        5 | 13443 | `		}` |
|      148 | 13444 | `	}` |
|        - | 13445 | `	/* Execute the cached block if available */` |
|      818 | 13446 | `	if( pCatch == 0 ){` |
|        - | 13447 | `		sxi32 rc;` |
|        - | 13448 | `		/* No catch matched. Execute finally, then propagate to outer try/catch. */` |
|      524 | 13449 | `		if( pException && pException->iHasFinally ){` |
|        3 | 13450 | `			pException->iFinallyDone = 1;` |
|        3 | 13451 | `			rc = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|        3 | 13452 | `			if( rc == SXERR_ABORT ){` |
|      ! 0 | 13453 | `				return SXERR_ABORT;` |
|        - | 13454 | `			}` |
|        1 | 13455 | `		}` |
|        - | 13456 | `		/* Check if there is an outer exception handler on the stack */` |
|      524 | 13457 | `		if( SySetUsed(&pVm->aException) > 0 ){` |
|        - | 13458 | `			/* Re-throw to the outer handler */` |
|        3 | 13459 | `			return VmThrowException(&(*pVm),pThis);` |
|        - | 13460 | `		}` |
|        - | 13461 | `		/* No outer handler. If the handlers were temporarily hidden` |
|        - | 13462 | `		 * (catch body re-throw with finally pending), defer the` |
|        - | 13463 | `		 * exception instead of reporting it uncaught.` |
|        - | 13464 | `		 */` |
|      522 | 13465 | `		if( pVm->pPendingException == 0 && pThis ){` |
|        - | 13466 | `			/* Check if we are inside a catch execution with hidden handlers` |
|        - | 13467 | `			 * by looking for a catch frame on the stack.` |
|        - | 13468 | `			 */` |
|      522 | 13469 | `			VmFrame *pF = pVm->pFrame;` |
|      522 | 13470 | `			int inCatch = 0;` |
|     1050 | 13471 | `			while( pF ){` |
|      538 | 13472 | `				if( pF->iFlags & VM_FRAME_CATCH ){` |
|        9 | 13473 | `					inCatch = 1;` |
|        9 | 13474 | `					break;` |
|        - | 13475 | `				}` |
|      529 | 13476 | `				pF = pF->pParent;` |
|        1 | 13477 | `			}` |
|      522 | 13478 | `			if( inCatch ){` |
|        - | 13479 | `				/* Defer — will be re-thrown after finally runs */` |
|        9 | 13480 | `				pThis->iRef++;` |
|        9 | 13481 | `				pVm->pPendingException = pThis;` |
|        9 | 13482 | `				return SXRET_OK;` |
|        - | 13483 | `			}` |
|      256 | 13484 | `		}` |
|        - | 13485 | `		/* Truly uncaught */` |
|      513 | 13486 | `		rc = VmUncaughtException(&(*pVm),pThis);` |
|      513 | 13487 | `		if( rc == SXRET_OK && pException ){` |
|      ! 0 | 13488 | `			VmFrame *pFrame = pVm->pFrame;` |
|      ! 0 | 13489 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|      ! 0 | 13490 | `			if( pException->pFrame == pFrame ){` |
|      ! 0 | 13491 | `				pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      ! 0 | 13492 | `			}` |
|      ! 0 | 13493 | `		}` |
|      513 | 13494 | `		return rc;` |
|      ! 0 | 13495 | `	}else{` |
|      296 | 13496 | `		VmFrame *pFrame = pVm->pFrame;` |
|      296 | 13497 | `		ph7_exception **apSaved = 0;` |
|        - | 13498 | `		sxu32 nSavedCount;` |
|        - | 13499 | `		sxi32 rc;` |
|      296 | 13500 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      296 | 13501 | `		if( pException->pFrame == pFrame ){` |
|      230 | 13502 | `			pFrame->iFlags &= ~VM_FRAME_THROW;` |
|      114 | 13503 | `		}` |
|        - | 13504 | `		/* Temporarily hide outer exception handlers so that if the catch` |
|        - | 13505 | `		 * body re-throws, the exception does not immediately propagate past` |
|        - | 13506 | `		 * our finally block. We save the stack contents and restore after.` |
|        - | 13507 | `		 */` |
|      296 | 13508 | `		nSavedCount = SySetUsed(&pVm->aException);` |
|      296 | 13509 | `		if( nSavedCount > 0 ){` |
|       16 | 13510 | `			apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        5 | 13511 | `				nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13512 | `			if( apSaved ){` |
|       16 | 13513 | `				SyMemcpy(SySetBasePtr(&pVm->aException),apSaved,` |
|        5 | 13514 | `					nSavedCount * sizeof(ph7_exception *));` |
|       11 | 13515 | `				SySetReset(&pVm->aException);` |
|        5 | 13516 | `			}` |
|        5 | 13517 | `		}` |
|        - | 13518 | `		/* Create a private frame first */` |
|      296 | 13519 | `		rc = VmEnterFrame(&(*pVm),0,0,&pFrame);` |
|      296 | 13520 | `		if( rc == SXRET_OK ){` |
|      296 | 13521 | `			ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      296 | 13522 | `			pFrame->iFlags \|= VM_FRAME_CATCH;` |
|      296 | 13523 | `			if( pObj ){` |
|      296 | 13524 | `				pThis->iRef++;` |
|      296 | 13525 | `				pObj->x.pOther = pThis;` |
|      296 | 13526 | `				MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      147 | 13527 | `			}` |
|        - | 13528 | `			/* Execute the catch block */` |
|      296 | 13529 | `			rc = VmLocalExec(&(*pVm),&pCatch->sByteCode,0);` |
|        - | 13530 | `			/* Leave the frame */` |
|      296 | 13531 | `			VmLeaveFrame(&(*pVm));` |
|      147 | 13532 | `		}` |
|        - | 13533 | `		/* Restore the outer exception handlers */` |
|      296 | 13534 | `		if( apSaved ){` |
|        - | 13535 | `			sxu32 k;` |
|        - | 13536 | `			/* Any new entries pushed during catch execution (from nested` |
|        - | 13537 | `			 * try blocks inside the catch body) are already consumed.` |
|        - | 13538 | `			 * Restore the original outer entries.` |
|        - | 13539 | `			 */` |
|       11 | 13540 | `			SySetReset(&pVm->aException);` |
|       21 | 13541 | `			for(k = 0; k < nSavedCount; k++){` |
|       11 | 13542 | `				SySetPut(&pVm->aException,(const void *)&apSaved[k]);` |
|        6 | 13543 | `			}` |
|       11 | 13544 | `			SyMemBackendFree(&pVm->sAllocator,apSaved);` |
|        5 | 13545 | `		}` |
|        - | 13546 | `		/* Execute the finally block after catch */` |
|      296 | 13547 | `		if( pException->iHasFinally ){` |
|       16 | 13548 | `			pException->iFinallyDone = 1;` |
|        - | 13549 | `			{` |
|       16 | 13550 | `				sxi32 rcf = VmLocalExec(&(*pVm),&pException->sFinally,0);` |
|       16 | 13551 | `				if( rcf == SXERR_ABORT ){` |
|      ! 0 | 13552 | `					return SXERR_ABORT;` |
|        - | 13553 | `				}` |
|        - | 13554 | `			}` |
|        7 | 13555 | `		}` |
|      296 | 13556 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 | 13557 | `			return SXERR_ABORT;` |
|        - | 13558 | `		}` |
|        - | 13559 | `		/* If the catch body re-threw, the exception was deferred in` |
|        - | 13560 | `		 * pPendingException (because outer handlers were hidden).` |
|        - | 13561 | `		 * Now that finally has run and handlers are restored, re-throw.` |
|        - | 13562 | `		 */` |
|      296 | 13563 | `		if( pVm->pPendingException ){` |
|        9 | 13564 | `			ph7_class_instance *pReThrow = pVm->pPendingException;` |
|        9 | 13565 | `			pVm->pPendingException = 0;` |
|        9 | 13566 | `			return VmThrowException(&(*pVm),pReThrow);` |
|        - | 13567 | `		}` |
|        - | 13568 | `	}` |
|        - | 13569 | `	/* TICKET 1433-60: Do not release the 'pException' pointer since it may` |
|        - | 13570 | `	 * be used again if a 'goto' statement is executed.` |
|        - | 13571 | `	 */` |
|      288 | 13572 | `	return SXRET_OK;` |
|      410 | 13573 |  |
|        - | 13574 | `/*` |
|        - | 13575 | ` * Section:` |
|        - | 13576 | ` *  Version,Credits and Copyright related functions.` |
|        - | 13577 | ` * Status:` |
|        - | 13578 | ` *    Stable.` |
|        - | 13579 | ` */` |
|        - | 13580 | `/*` |
|        - | 13581 | ` * string ph7version(void)` |
|        - | 13582 | ` *  Returns the running version of the PH7 version.` |
|        - | 13583 | ` * Parameters` |
|        - | 13584 | ` *  None` |
|        - | 13585 | ` * Return` |
|        - | 13586 | ` * Current PH7 version.` |
|        - | 13587 | ` */` |
|        2 | 13588 | `static int vm_builtin_ph7_version(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13589 |  |
|        1 | 13590 | `	SXUNUSED(nArg);` |
|        1 | 13591 | `	SXUNUSED(apArg); /* cc warning */` |
|        - | 13592 | `	/* Current engine version */` |
|        3 | 13593 | `	ph7_result_string(pCtx,PH7_VERSION,sizeof(PH7_VERSION) - 1);` |
|        3 | 13594 | `	return PH7_OK;` |
|        1 | 13595 |  |
|        - | 13596 | `/*` |
|        - | 13597 | ` * PH7 release information HTML page used by the ph7info() and ph7credits() functions.` |
|        - | 13598 | ` */` |
|        - | 13599 | ` #define PH7_HTML_PAGE_HEADER "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"\` |
|        - | 13600 | ` "<html><head>"\` |
|        - | 13601 | ` "<meta content=\"text/html; charset=UTF-8\" http-equiv=\"content-type\"><title>PH7 engine credits</title>"\` |
|        - | 13602 | ` "<style type=\"text/css\">"\` |
|        - | 13603 | ` "div {"\` |
|        - | 13604 | `     "border: 1px solid #cccccc;"\` |
|        - | 13605 | `     "-moz-border-radius-topleft: 10px;"\` |
|        - | 13606 | `     "-moz-border-radius-bottomright: 10px;"\` |
|        - | 13607 | `     "-moz-border-radius-bottomleft: 10px;"\` |
|        - | 13608 | `     "-moz-border-radius-topright: 10px;"\` |
|        - | 13609 | `     "-webkit-border-radius: 10px;"\` |
|        - | 13610 | `     "-o-border-radius: 10px;"\` |
|        - | 13611 | `     "border-radius: 10px;"\` |
|        - | 13612 | `     "padding-left: 2em;"\` |
|        - | 13613 | `     "background-color: white;"\` |
|        - | 13614 | `     "margin-left: auto;"\` |
|        - | 13615 | `     "font-family: verdana;"\` |
|        - | 13616 | `     "padding-right: 2em;"\` |
|        - | 13617 | `     "margin-right: auto;"\` |
|        - | 13618 | `     "}"\` |
|        - | 13619 | `     "body {"\` |
|        - | 13620 | `     "padding: 0.2em;"\` |
|        - | 13621 | `     "font-style: normal;"\` |
|        - | 13622 | `     "font-size: medium;"\` |
|        - | 13623 | `     "background-color: #f2f2f2;"\` |
|        - | 13624 | `     "}"\` |
|        - | 13625 | `     "hr {"\` |
|        - | 13626 | `     "border-style: solid none none;"\` |
|        - | 13627 | `     "border-width: 1px medium medium;"\` |
|        - | 13628 | `     "border-top: 1px solid #cccccc;"\` |
|        - | 13629 | `     "height: 1px;"\` |
|        - | 13630 | `     "}"\` |
|        - | 13631 | `     "a {"\` |
|        - | 13632 | `     "color: #3366cc;"\` |
|        - | 13633 | `     "text-decoration: none;"\` |
|        - | 13634 | `     "}"\` |
|        - | 13635 | `     "a:hover {"\` |
|        - | 13636 | `     "color: #999999;"\` |
|        - | 13637 | `     "}"\` |
|        - | 13638 | `     "a:active {"\` |
|        - | 13639 | `     "color: #663399;"\` |
|        - | 13640 | `     "}"\` |
|        - | 13641 | `     "h1 {"\` |
|        - | 13642 | `     "margin: 0;"\` |
|        - | 13643 | `     "padding: 0;"\` |
|        - | 13644 | `     "font-family: Verdana;"\` |
|        - | 13645 | `     "font-weight: bold;"\` |
|        - | 13646 | `     "font-style: normal;"\` |
|        - | 13647 | `     "font-size: medium;"\` |
|        - | 13648 | `     "text-transform: capitalize;"\` |
|        - | 13649 | `     "color: #0a328c;"\` |
|        - | 13650 | `     "}"\` |
|        - | 13651 | `     "p {"\` |
|        - | 13652 | `     "margin: 0 auto;"\` |
|        - | 13653 | `     "font-size: medium;"\` |
|        - | 13654 | `     "font-style: normal;"\` |
|        - | 13655 | `     "font-family: verdana;"\` |
|        - | 13656 | `     "}"\` |
|        - | 13657 | `"</style></head><body>"\` |
|        - | 13658 | `"<div style=\"background-color: white; width: 699px;\">"\` |
|        - | 13659 | `"<h1 style=\"font-family: Verdana; text-align: right;\"><small><small>PH7 Engine Credits</small></small></h1>"\` |
|        - | 13660 | `"<hr style=\"margin-left: auto; margin-right: auto;\">"\` |
|        - | 13661 | `"<p><small><small><span style=\"font-weight: bold;\">"\` |
|        - | 13662 | `"PH7 Engine</span></small><small>&nbsp;</small></small></p>"\` |
|        - | 13663 | `"<p style=\"text-align: left;\"><small><small>"\` |
|        - | 13664 | `"A highly efficient embeddable bytecode compiler and a Virtual Machine for the PHP Programming Language.</small></small></p>"\` |
|        - | 13665 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Symisc Systems.<br></small></small></p>"\` |
|        - | 13666 | `"<p style=\"text-align: left;\"><small><small>Copyright (C) Alexandre Gomes Gaigalas.<br></small></small></p>"\` |
|        - | 13667 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine Version:</small></small></p>"\` |
|        - | 13668 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\">"` |
|        - | 13669 |  |
|        - | 13670 | `#define PH7_HTML_PAGE_FORMAT "<small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13671 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Engine ID:</small></small></p>"\` |
|        - | 13672 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s %s</span></small></small></p>"\` |
|        - | 13673 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Underlying VFS:</small></small></p>"\` |
|        - | 13674 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13675 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Functions:</small></small></p>"\` |
|        - | 13676 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13677 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Total Built-in Classes:</small></small></p>"\` |
|        - | 13678 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%d</span></small></small></p>"\` |
|        - | 13679 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Host Operating System:</small></small></p>"\` |
|        - | 13680 | `"<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">%s</span></small></small></p>"\` |
|        - | 13681 | `"<p style=\"text-align: left; font-weight: bold;\"><small style=\"font-weight: bold;\"><small><small></small></small></small></p>"\` |
|        - | 13682 | `"<p style=\"text-align: left; font-weight: bold;\"><small><small>Licensed To: &lt;Public Release Under The <a href=\"http://www.symisc.net/spl.txt\">"\` |
|        - | 13683 | ` "Symisc Public License (SPL)</a>&gt;</small></small></p>"` |
|        - | 13684 |  |
|        - | 13685 | `#define PH7_HTML_PAGE_FOOTER "<p style=\"text-align: left; font-weight: bold; margin-left: 40px;\"><small><small><span style=\"font-weight: normal;\">/*<br>"\` |
|        - | 13686 | `"&nbsp;* Copyright (C) 2011, 2012, 2013, 2014 Symisc Systems. All rights reserved.<br>"\` |
|        - | 13687 | `"&nbsp;* Copyright (C) 2025 Alexandre Gomes Gaigalas. All rights reserved.<br>"\` |
|        - | 13688 | `"&nbsp;*<br>"\` |
|        - | 13689 | `"&nbsp;* Redistribution and use in source and binary forms, with or without<br>"\` |
|        - | 13690 | `"&nbsp;* modification, are permitted provided that the following conditions<br>"\` |
|        - | 13691 | `"&nbsp;* are met:<br>"\` |
|        - | 13692 | `"&nbsp;* 1. Redistributions of source code must retain the above copyright<br>"\` |
|        - | 13693 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer.<br>"\` |
|        - | 13694 | `"&nbsp;* 2. Redistributions in binary form must reproduce the above copyright<br>"\` |
|        - | 13695 | `"&nbsp;*&nbsp;&nbsp;&nbsp; notice, this list of conditions and the following disclaimer in the<br>"\` |
|        - | 13696 | `"&nbsp;*&nbsp;&nbsp;&nbsp; documentation and/or other materials provided with the distribution.<br>"\` |
|        - | 13697 | `"&nbsp;* 3. Redistributions in any form must be accompanied by information on<br>"\` |
|        - | 13698 | `"&nbsp;*&nbsp;&nbsp;&nbsp; how to obtain complete source code for the PH7 engine and any <br>"\` |
|        - | 13699 | `"&nbsp;*&nbsp;&nbsp;&nbsp; accompanying software that uses the PH7 engine software.<br>"\` |
|        - | 13700 | `"&nbsp;*&nbsp;&nbsp;&nbsp; The source code must either be included in the distribution<br>"\` |
|        - | 13701 | `"&nbsp;*&nbsp;&nbsp;&nbsp; or be available for no more than the cost of distribution plus<br>"\` |
|        - | 13702 | `"&nbsp;*&nbsp;&nbsp;&nbsp; a nominal fee, and must be freely redistributable under reasonable<br>"\` |
|        - | 13703 | `"&nbsp;*&nbsp;&nbsp;&nbsp; conditions. For an executable file, complete source code means<br>"\` |
|        - | 13704 | `"&nbsp;*&nbsp;&nbsp;&nbsp; the source code for all modules it contains.It does not include<br>"\` |
|        - | 13705 | `"&nbsp;*&nbsp;&nbsp;&nbsp; source code for modules or files that typically accompany the major<br>"\` |
|        - | 13706 | `"&nbsp;*&nbsp;&nbsp;&nbsp; components of the operating system on which the executable file runs.<br>"\` |
|        - | 13707 | `"&nbsp;*<br>"\` |
|        - | 13708 | ```"&nbsp;* THIS SOFTWARE IS PROVIDED BY SYMISC SYSTEMS ``AS IS'' AND ANY EXPRESS<br>"\``` |
|        - | 13709 | `"&nbsp;* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED<br>"\` |
|        - | 13710 | `"&nbsp;* WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR<br>"\` |
|        - | 13711 | `"&nbsp;* NON-INFRINGEMENT, ARE DISCLAIMED.&nbsp; IN NO EVENT SHALL SYMISC SYSTEMS<br>"\` |
|        - | 13712 | `"&nbsp;* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR<br>"\` |
|        - | 13713 | `"&nbsp;* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF<br>"\` |
|        - | 13714 | `"&nbsp;* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR<br>"\` |
|        - | 13715 | `"&nbsp;* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,<br>"\` |
|        - | 13716 | `"&nbsp;* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE<br>"\` |
|        - | 13717 | `"&nbsp;* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN<br>"\` |
|        - | 13718 | `"&nbsp;* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.<br>"\` |
|        - | 13719 | `"&nbsp;*/<br>"\` |
|        - | 13720 | `"</span></small></small></p>"\` |
|        - | 13721 | `"</div></body></html>"` |
|        - | 13722 | `/*` |
|        - | 13723 | ` * bool ph7credits(void)` |
|        - | 13724 | ` * bool ph7info(void)` |
|        - | 13725 | ` * bool ph7copyright(void)` |
|        - | 13726 | ` *  Prints out the credits for PH7 engine` |
|        - | 13727 | ` * Parameters` |
|        - | 13728 | ` *  None` |
|        - | 13729 | ` * Return` |
|        - | 13730 | ` *  Always TRUE` |
|        - | 13731 | ` */` |
|        2 | 13732 | `static int vm_builtin_ph7_credits(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13733 |  |
|        3 | 13734 | `	ph7_vm *pVm = pCtx->pVm; /* Point to the underlying VM */` |
|        - | 13735 | `	/* Expand the HTML page above*/` |
|        3 | 13736 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_HEADER,(int)sizeof(PH7_HTML_PAGE_HEADER)-1);` |
|        2 | 13737 | `	ph7_context_output_format(` |
|        1 | 13738 | `		pCtx,` |
|        - | 13739 | `		PH7_HTML_PAGE_FORMAT,` |
|        1 | 13740 | `		ph7_lib_version(),   /* Engine version */` |
|        1 | 13741 | `		ph7_lib_signature(), /* Engine signature */` |
|        1 | 13742 | `		ph7_lib_ident(),     /* Engine ID */` |
|        2 | 13743 | `		pVm->pEngine->pVfs ? pVm->pEngine->pVfs->zName : "null_vfs",` |
|        2 | 13744 | `		SyHashTotalEntry(&pVm->hFunction) + SyHashTotalEntry(&pVm->hHostFunction),/* # built-in functions */` |
|        1 | 13745 | `		SyHashTotalEntry(&pVm->hClass),` |
|        - | 13746 | `#ifdef __WINNT__` |
|        - | 13747 | `		"Windows NT"` |
|        - | 13748 | `#elif defined(__UNIXES__)` |
|        - | 13749 | `		"UNIX-Like"` |
|        - | 13750 | `#else` |
|        - | 13751 | `		"Other OS"` |
|        - | 13752 | `#endif` |
|        - | 13753 | `		);` |
|        3 | 13754 | `	ph7_context_output(pCtx,PH7_HTML_PAGE_FOOTER,(int)sizeof(PH7_HTML_PAGE_FOOTER)-1);` |
|        1 | 13755 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 13756 | `	SXUNUSED(apArg);` |
|        - | 13757 | `	/* Return TRUE */` |
|        - | 13758 | `	//ph7_result_bool(pCtx,1);` |
|        3 | 13759 | `	return PH7_OK;` |
|        1 | 13760 |  |
|        - | 13761 | `/*` |
|        - | 13762 | ` * Section:` |
|        - | 13763 | ` *    URL related routines.` |
|        - | 13764 | ` * Status:` |
|        - | 13765 | ` *    Stable.` |
|        - | 13766 | ` */` |
|        - | 13767 | `/*` |
|        - | 13768 | ` * value parse_url(string $url [, int $component = -1 ])` |
|        - | 13769 | ` *  Parse a URL and return its fields.` |
|        - | 13770 | ` * Parameters` |
|        - | 13771 | ` *  $url` |
|        - | 13772 | ` *   The URL to parse.` |
|        - | 13773 | ` * $component` |
|        - | 13774 | ` *  Specify one of PHP_URL_SCHEME, PHP_URL_HOST, PHP_URL_PORT, PHP_URL_USER` |
|        - | 13775 | ` *  PHP_URL_PASS, PHP_URL_PATH, PHP_URL_QUERY or PHP_URL_FRAGMENT to retrieve` |
|        - | 13776 | ` *  just a specific URL component as a string (except when PHP_URL_PORT is given` |
|        - | 13777 | ` *  in which case the return value will be an integer).` |
|        - | 13778 | ` * Return` |
|        - | 13779 | ` *  If the component parameter is omitted, an associative array is returned.` |
|        - | 13780 | ` *  At least one element will be present within the array. Potential keys within` |
|        - | 13781 | ` *  this array are:` |
|        - | 13782 | ` *   scheme - e.g. http` |
|        - | 13783 | ` *   host` |
|        - | 13784 | ` *   port` |
|        - | 13785 | ` *   user` |
|        - | 13786 | ` *   pass` |
|        - | 13787 | ` *   path` |
|        - | 13788 | ` *   query - after the question mark ?` |
|        - | 13789 | ` *   fragment - after the hashmark #` |
|        - | 13790 | ` * Note:` |
|        - | 13791 | ` *  FALSE is returned on failure.` |
|        - | 13792 | ` *  This function work with relative URL unlike the one shipped` |
|        - | 13793 | ` *  with the standard PHP engine.` |
|        - | 13794 | ` */` |
|       28 | 13795 | `static int vm_builtin_parse_url(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 13796 |  |
|        - | 13797 | `	const char *zStr; /* Input string */` |
|        - | 13798 | `	SyString *pComp;  /* Pointer to the URI component */` |
|        - | 13799 | `	SyhttpUri sURI;   /* Parse of the given URI */` |
|        - | 13800 | `	int nLen;` |
|        - | 13801 | `	sxi32 rc;` |
|       29 | 13802 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|        - | 13803 | `		/* Missing/Invalid arguments,return FALSE */` |
|      ! 0 | 13804 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13805 | `		return PH7_OK;` |
|        - | 13806 | `	}` |
|        - | 13807 | `	/* Extract the given URI */` |
|       29 | 13808 | `	zStr = ph7_value_to_string(apArg[0],&nLen);` |
|       29 | 13809 | `	if( nLen < 1 ){` |
|        - | 13810 | `		/* Nothing to process,return FALSE */` |
|        3 | 13811 | `		ph7_result_bool(pCtx,0);` |
|        3 | 13812 | `		return PH7_OK;` |
|        - | 13813 | `	}` |
|        - | 13814 | `	/* Get a parse */` |
|       27 | 13815 | `	rc = PH7_VmHttpSplitURI(&sURI,zStr,(sxu32)nLen);` |
|       27 | 13816 | `	if( rc != SXRET_OK ){` |
|        - | 13817 | `		/* Malformed input,return FALSE */` |
|      ! 0 | 13818 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 13819 | `		return PH7_OK;` |
|        - | 13820 | `	}` |
|       27 | 13821 | `	if( nArg > 1 ){` |
|      ! 0 | 13822 | `		int nComponent = ph7_value_to_int(apArg[1]);` |
|        - | 13823 | `		/* Refer to constant.c for constants values */` |
|      ! 0 | 13824 | `		switch(nComponent){` |
|      ! 0 | 13825 | `		case 1: /* PHP_URL_SCHEME */` |
|      ! 0 | 13826 | `			pComp = &sURI.sScheme;` |
|      ! 0 | 13827 | `			if( pComp->nByte < 1 ){` |
|        - | 13828 | `				/* No available value,return NULL */` |
|      ! 0 | 13829 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13830 | `			}else{` |
|      ! 0 | 13831 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13832 | `			}` |
|      ! 0 | 13833 | `			break;` |
|      ! 0 | 13834 | `		case 2: /* PHP_URL_HOST */` |
|      ! 0 | 13835 | `			pComp = &sURI.sHost;` |
|      ! 0 | 13836 | `			if( pComp->nByte < 1 ){` |
|        - | 13837 | `				/* No available value,return NULL */` |
|      ! 0 | 13838 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13839 | `			}else{` |
|      ! 0 | 13840 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13841 | `			}` |
|      ! 0 | 13842 | `			break;` |
|      ! 0 | 13843 | `		case 3: /* PHP_URL_PORT */` |
|      ! 0 | 13844 | `			pComp = &sURI.sPort;` |
|      ! 0 | 13845 | `			if( pComp->nByte < 1 ){` |
|        - | 13846 | `				/* No available value,return NULL */` |
|      ! 0 | 13847 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13848 | `			}else{` |
|      ! 0 | 13849 | `				int iPort = 0;` |
|        - | 13850 | `				/* Cast the value to integer */` |
|      ! 0 | 13851 | `				SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|      ! 0 | 13852 | `				ph7_result_int(pCtx,iPort);` |
|        - | 13853 | `			}` |
|      ! 0 | 13854 | `			break;` |
|      ! 0 | 13855 | `		case 4: /* PHP_URL_USER */` |
|      ! 0 | 13856 | `			pComp = &sURI.sUser;` |
|      ! 0 | 13857 | `			if( pComp->nByte < 1 ){` |
|        - | 13858 | `				/* No available value,return NULL */` |
|      ! 0 | 13859 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13860 | `			}else{` |
|      ! 0 | 13861 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13862 | `			}` |
|      ! 0 | 13863 | `			break;` |
|      ! 0 | 13864 | `		case 5: /* PHP_URL_PASS */` |
|      ! 0 | 13865 | `			pComp = &sURI.sPass;` |
|      ! 0 | 13866 | `			if( pComp->nByte < 1 ){` |
|        - | 13867 | `				/* No available value,return NULL */` |
|      ! 0 | 13868 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13869 | `			}else{` |
|      ! 0 | 13870 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13871 | `			}` |
|      ! 0 | 13872 | `			break;` |
|      ! 0 | 13873 | `		case 7: /* PHP_URL_QUERY */` |
|      ! 0 | 13874 | `			pComp = &sURI.sQuery;` |
|      ! 0 | 13875 | `			if( pComp->nByte < 1 ){` |
|        - | 13876 | `				/* No available value,return NULL */` |
|      ! 0 | 13877 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13878 | `			}else{` |
|      ! 0 | 13879 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13880 | `			}` |
|      ! 0 | 13881 | `			break;` |
|      ! 0 | 13882 | `		case 8: /* PHP_URL_FRAGMENT */` |
|      ! 0 | 13883 | `			pComp = &sURI.sFragment;` |
|      ! 0 | 13884 | `			if( pComp->nByte < 1 ){` |
|        - | 13885 | `				/* No available value,return NULL */` |
|      ! 0 | 13886 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13887 | `			}else{` |
|      ! 0 | 13888 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13889 | `			}` |
|      ! 0 | 13890 | `			break;` |
|      ! 0 | 13891 | `		case 6: /*  PHP_URL_PATH */` |
|      ! 0 | 13892 | `			pComp = &sURI.sPath;` |
|      ! 0 | 13893 | `			if( pComp->nByte < 1 ){` |
|        - | 13894 | `				/* No available value,return NULL */` |
|      ! 0 | 13895 | `				ph7_result_null(pCtx);` |
|      ! 0 | 13896 | `			}else{` |
|      ! 0 | 13897 | `				ph7_result_string(pCtx,pComp->zString,(int)pComp->nByte);` |
|        - | 13898 | `			}` |
|      ! 0 | 13899 | `			break;` |
|      ! 0 | 13900 | `		default:` |
|        - | 13901 | `			/* No such entry,return NULL */` |
|      ! 0 | 13902 | `			ph7_result_null(pCtx);` |
|      ! 0 | 13903 | `			break;` |
|        - | 13904 | `		}` |
|      ! 0 | 13905 | `	}else{` |
|        - | 13906 | `		ph7_value *pArray,*pValue;` |
|        - | 13907 | `		/* Return an associative array */` |
|       27 | 13908 | `		pArray = ph7_context_new_array(pCtx);  /* Empty array */` |
|       27 | 13909 | `		pValue = ph7_context_new_scalar(pCtx); /* Array value */` |
|       27 | 13910 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|        - | 13911 | `			/* Out of memory */` |
|      ! 0 | 13912 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 13913 | `			/* Return false */` |
|      ! 0 | 13914 | `			ph7_result_bool(pCtx,0);` |
|      ! 0 | 13915 | `			return PH7_OK;` |
|        - | 13916 | `		}` |
|        - | 13917 | `		/* Fill the array */` |
|       27 | 13918 | `		pComp = &sURI.sScheme;` |
|       27 | 13919 | `		if( pComp->nByte > 0 ){` |
|       19 | 13920 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       19 | 13921 | `			ph7_array_add_strkey_elem(pArray,"scheme",pValue); /* Will make it's own copy */` |
|        9 | 13922 | `		}` |
|        - | 13923 | `		/* Reset the string cursor */` |
|       27 | 13924 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13925 | `		pComp = &sURI.sHost;` |
|       27 | 13926 | `		if( pComp->nByte > 0 ){` |
|       25 | 13927 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       25 | 13928 | `			ph7_array_add_strkey_elem(pArray,"host",pValue); /* Will make it's own copy */` |
|       12 | 13929 | `		}` |
|        - | 13930 | `		/* Reset the string cursor */` |
|       27 | 13931 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13932 | `		pComp = &sURI.sPort;` |
|       27 | 13933 | `		if( pComp->nByte > 0 ){` |
|       11 | 13934 | `			int iPort = 0;/* cc warning */` |
|        - | 13935 | `			/* Convert to integer */` |
|       11 | 13936 | `			SyStrToInt32(pComp->zString,pComp->nByte,(void *)&iPort,0);` |
|       11 | 13937 | `			ph7_value_int(pValue,iPort);` |
|       11 | 13938 | `			ph7_array_add_strkey_elem(pArray,"port",pValue); /* Will make it's own copy */` |
|        5 | 13939 | `		}` |
|        - | 13940 | `		/* Reset the string cursor */` |
|       27 | 13941 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13942 | `		pComp = &sURI.sUser;` |
|       27 | 13943 | `		if( pComp->nByte > 0 ){` |
|        7 | 13944 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13945 | `			ph7_array_add_strkey_elem(pArray,"user",pValue); /* Will make it's own copy */` |
|        3 | 13946 | `		}` |
|        - | 13947 | `		/* Reset the string cursor */` |
|       27 | 13948 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13949 | `		pComp = &sURI.sPass;` |
|       27 | 13950 | `		if( pComp->nByte > 0 ){` |
|        7 | 13951 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        7 | 13952 | `			ph7_array_add_strkey_elem(pArray,"pass",pValue); /* Will make it's own copy */` |
|        3 | 13953 | `		}` |
|        - | 13954 | `		/* Reset the string cursor */` |
|       27 | 13955 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13956 | `		pComp = &sURI.sPath;` |
|       27 | 13957 | `		if( pComp->nByte > 0 ){` |
|       17 | 13958 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|       17 | 13959 | `			ph7_array_add_strkey_elem(pArray,"path",pValue); /* Will make it's own copy */` |
|        8 | 13960 | `		}` |
|        - | 13961 | `		/* Reset the string cursor */` |
|       27 | 13962 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13963 | `		pComp = &sURI.sQuery;` |
|       27 | 13964 | `		if( pComp->nByte > 0 ){` |
|        5 | 13965 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13966 | `			ph7_array_add_strkey_elem(pArray,"query",pValue); /* Will make it's own copy */` |
|        2 | 13967 | `		}` |
|        - | 13968 | `		/* Reset the string cursor */` |
|       27 | 13969 | `		ph7_value_reset_string_cursor(pValue);` |
|       27 | 13970 | `		pComp = &sURI.sFragment;` |
|       27 | 13971 | `		if( pComp->nByte > 0 ){` |
|        5 | 13972 | `			ph7_value_string(pValue,pComp->zString,(int)pComp->nByte);` |
|        5 | 13973 | `			ph7_array_add_strkey_elem(pArray,"fragment",pValue); /* Will make it's own copy */` |
|        2 | 13974 | `		}` |
|        - | 13975 | `		/* Return the created array */` |
|       27 | 13976 | `		ph7_result_value(pCtx,pArray);` |
|        - | 13977 | `		/* NOTE:` |
|        - | 13978 | `		 * Don't worry about freeing 'pValue',everything will be released` |
|        - | 13979 | `		 * automatically as soon we return from this function.` |
|        - | 13980 | `		 */` |
|        - | 13981 | `	}` |
|        - | 13982 | `	/* All done */` |
|       27 | 13983 | `	return PH7_OK;` |
|       15 | 13984 |  |
|        - | 13985 | `/*` |
|        - | 13986 | ` * Section:` |
|        - | 13987 | ` *   Array related routines.` |
|        - | 13988 | ` * Status:` |
|        - | 13989 | ` *    Stable.` |
|        - | 13990 | ` * Note 2012-5-21 01:04:15:` |
|        - | 13991 | ` *  Array related functions that need access to the underlying` |
|        - | 13992 | ` *  virtual machine are implemented here rather than 'hashmap.c'` |
|        - | 13993 | ` */` |
|        - | 13994 | `/*` |
|        - | 13995 | ` * The [compact()] function store it's state information in an instance` |
|        - | 13996 | ` * of the following structure.` |
|        - | 13997 | ` */` |
|        - | 13998 | `struct compact_data` |
|        - | 13999 |  |
|        - | 14000 | `	ph7_value *pArray;  /* Target array */` |
|        - | 14001 | `	int nRecCount;      /* Recursion count */` |
|        - | 14002 | `};` |
|        - | 14003 | `/*` |
|        - | 14004 | ` * Walker callback for the [compact()] function defined below.` |
|        - | 14005 | ` */` |
|      ! 0 | 14006 | `static int VmCompactCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      ! 0 | 14007 |  |
|      ! 0 | 14008 | `	struct compact_data *pData = (struct compact_data *)pUserData;` |
|      ! 0 | 14009 | `	ph7_value *pArray = (ph7_value *)pData->pArray;` |
|      ! 0 | 14010 | `	ph7_vm *pVm = pArray->pVm;` |
|        - | 14011 | `	/* Act according to the hashmap value */` |
|      ! 0 | 14012 | `	if( ph7_value_is_string(pValue) ){` |
|        - | 14013 | `		SyString sVar;` |
|      ! 0 | 14014 | `		SyStringInitFromBuf(&sVar,SyBlobData(&pValue->sBlob),SyBlobLength(&pValue->sBlob));` |
|      ! 0 | 14015 | `		if( sVar.nByte > 0 ){` |
|        - | 14016 | `			/* Query the current frame */` |
|      ! 0 | 14017 | `			pKey = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        - | 14018 | `			/* ^` |
|        - | 14019 | `			 * \| Avoid wasting variable and use 'pKey' instead` |
|        - | 14020 | `			 */` |
|      ! 0 | 14021 | `			if( pKey ){` |
|        - | 14022 | `				/* Perform the insertion */` |
|      ! 0 | 14023 | `				ph7_array_add_elem(pArray,pValue/* Variable name*/,pKey/* Variable value */);` |
|      ! 0 | 14024 | `			}` |
|      ! 0 | 14025 | `		}` |
|      ! 0 | 14026 | `	}else if( ph7_value_is_array(pValue) && pData->nRecCount < 32) {` |
|        - | 14027 | `		int rc;` |
|        - | 14028 | `		/* Recursively traverse this array */` |
|      ! 0 | 14029 | `		pData->nRecCount++;` |
|      ! 0 | 14030 | `		rc = PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,VmCompactCallback,pUserData);` |
|      ! 0 | 14031 | `		pData->nRecCount--;` |
|      ! 0 | 14032 | `		return rc;` |
|        - | 14033 | `	}` |
|      ! 0 | 14034 | `	return SXRET_OK;` |
|      ! 0 | 14035 |  |
|        - | 14036 | `/*` |
|        - | 14037 | ` * array compact(mixed $varname [, mixed $... ])` |
|        - | 14038 | ` *  Create array containing variables and their values.` |
|        - | 14039 | ` *  For each of these, compact() looks for a variable with that name` |
|        - | 14040 | ` *  in the current symbol table and adds it to the output array such` |
|        - | 14041 | ` *  that the variable name becomes the key and the contents of the variable` |
|        - | 14042 | ` *  become the value for that key. In short, it does the opposite of extract().` |
|        - | 14043 | ` *  Any strings that are not set will simply be skipped.` |
|        - | 14044 | ` * Parameters` |
|        - | 14045 | ` *  $varname` |
|        - | 14046 | ` *   compact() takes a variable number of parameters. Each parameter can be either` |
|        - | 14047 | ` *   a string containing the name of the variable, or an array of variable names.` |
|        - | 14048 | ` *   The array can contain other arrays of variable names inside it; compact() handles` |
|        - | 14049 | ` *   it recursively.` |
|        - | 14050 | ` * Return` |
|        - | 14051 | ` *  The output array with all the variables added to it or NULL on failure` |
|        - | 14052 | ` */` |
|        2 | 14053 | `static int vm_builtin_compact(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14054 |  |
|        - | 14055 | `	ph7_value *pArray,*pObj;` |
|        3 | 14056 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14057 | `	const char *zName;` |
|        - | 14058 | `	SyString sVar;` |
|        - | 14059 | `	int i,nLen;` |
|        3 | 14060 | `	if( nArg < 1 ){` |
|        - | 14061 | `		/* Missing arguments,return NULL */` |
|      ! 0 | 14062 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14063 | `		return PH7_OK;` |
|        - | 14064 | `	}` |
|        - | 14065 | `	/* Create the array */` |
|        3 | 14066 | `	pArray = ph7_context_new_array(pCtx);` |
|        3 | 14067 | `	if( pArray == 0 ){` |
|        - | 14068 | `		/* Out of memory */` |
|      ! 0 | 14069 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 engine is running out of memory");` |
|        - | 14070 | `		/* Return NULL */` |
|      ! 0 | 14071 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14072 | `		return PH7_OK;` |
|        - | 14073 | `	}` |
|        - | 14074 | `	/* Perform the requested operation */` |
|        7 | 14075 | `	for( i = 0 ; i < nArg ; i++ ){` |
|        5 | 14076 | `		if( !ph7_value_is_string(apArg[i]) ){` |
|      ! 0 | 14077 | `			if( ph7_value_is_array(apArg[i]) ){` |
|        - | 14078 | `				struct compact_data sData;` |
|      ! 0 | 14079 | `				ph7_hashmap *pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|        - | 14080 | `				/* Recursively walk the array */` |
|      ! 0 | 14081 | `				sData.nRecCount = 0;` |
|      ! 0 | 14082 | `				sData.pArray = pArray;` |
|      ! 0 | 14083 | `				PH7_HashmapWalk(pMap,VmCompactCallback,&sData);` |
|      ! 0 | 14084 | `			}` |
|      ! 0 | 14085 | `		}else{` |
|        - | 14086 | `			/* Extract variable name */` |
|        5 | 14087 | `			zName = ph7_value_to_string(apArg[i],&nLen);` |
|        5 | 14088 | `			if( nLen > 0 ){` |
|        5 | 14089 | `				SyStringInitFromBuf(&sVar,zName,nLen);` |
|        - | 14090 | `				/* Check if the variable is available in the current frame */` |
|        5 | 14091 | `				pObj = VmExtractMemObj(pVm,&sVar,FALSE,FALSE);` |
|        5 | 14092 | `				if( pObj ){` |
|        5 | 14093 | `					ph7_array_add_elem(pArray,apArg[i]/*Variable name*/,pObj/* Variable value */);` |
|        2 | 14094 | `				}` |
|        2 | 14095 | `			}` |
|        - | 14096 | `		}` |
|        3 | 14097 | `	}` |
|        - | 14098 | `	/* Return the array */` |
|        3 | 14099 | `	ph7_result_value(pCtx,pArray);` |
|        3 | 14100 | `	return PH7_OK;` |
|        2 | 14101 |  |
|        - | 14102 | `/*` |
|        - | 14103 | ` * The [extract()] function store it's state information in an instance` |
|        - | 14104 | ` * of the following structure.` |
|        - | 14105 | ` */` |
|        - | 14106 | `typedef struct extract_aux_data extract_aux_data;` |
|        - | 14107 | `struct extract_aux_data` |
|        - | 14108 |  |
|        - | 14109 | `	ph7_vm *pVm;          /* VM that own this instance */` |
|        - | 14110 | `	int iCount;           /* Number of variables successfully imported  */` |
|        - | 14111 | `	const char *zPrefix;  /* Prefix name */` |
|        - | 14112 | `	int Prefixlen;        /* Prefix  length */` |
|        - | 14113 | `	int iFlags;           /* Control flags */` |
|        - | 14114 | `	char zWorker[1024];   /* Working buffer */` |
|        - | 14115 | `};` |
|        - | 14116 | `/* Forward declaration */` |
|        - | 14117 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData);` |
|        - | 14118 | `/*` |
|        - | 14119 | ` * int extract(array &$var_array[,int $extract_type = EXTR_OVERWRITE[,string $prefix = NULL ]])` |
|        - | 14120 | ` *   Import variables into the current symbol table from an array.` |
|        - | 14121 | ` * Parameters` |
|        - | 14122 | ` * $var_array` |
|        - | 14123 | ` *  An associative array. This function treats keys as variable names and values` |
|        - | 14124 | ` *  as variable values. For each key/value pair it will create a variable in the current symbol` |
|        - | 14125 | ` *  table, subject to extract_type and prefix parameters.` |
|        - | 14126 | ` *  You must use an associative array; a numerically indexed array will not produce results` |
|        - | 14127 | ` *  unless you use EXTR_PREFIX_ALL or EXTR_PREFIX_INVALID.` |
|        - | 14128 | ` * $extract_type` |
|        - | 14129 | ` *  The way invalid/numeric keys and collisions are treated is determined by the extract_type.` |
|        - | 14130 | ` *  It can be one of the following values:` |
|        - | 14131 | ` *   EXTR_OVERWRITE` |
|        - | 14132 | ` *       If there is a collision, overwrite the existing variable.` |
|        - | 14133 | ` *   EXTR_SKIP` |
|        - | 14134 | ` *       If there is a collision, don't overwrite the existing variable.` |
|        - | 14135 | ` *   EXTR_PREFIX_SAME` |
|        - | 14136 | ` *       If there is a collision, prefix the variable name with prefix.` |
|        - | 14137 | ` *   EXTR_PREFIX_ALL` |
|        - | 14138 | ` *       Prefix all variable names with prefix.` |
|        - | 14139 | ` *   EXTR_PREFIX_INVALID` |
|        - | 14140 | ` *       Only prefix invalid/numeric variable names with prefix.` |
|        - | 14141 | ` *   EXTR_IF_EXISTS` |
|        - | 14142 | ` *       Only overwrite the variable if it already exists in the current symbol table` |
|        - | 14143 | ` *       otherwise do nothing.` |
|        - | 14144 | ` *       This is useful for defining a list of valid variables and then extracting only those` |
|        - | 14145 | ` *       variables you have defined out of $_REQUEST, for example.` |
|        - | 14146 | ` *   EXTR_PREFIX_IF_EXISTS` |
|        - | 14147 | ` *       Only create prefixed variable names if the non-prefixed version of the same variable exists in` |
|        - | 14148 | ` *      the current symbol table.` |
|        - | 14149 | ` * $prefix` |
|        - | 14150 | ` *  Note that prefix is only required if extract_type is EXTR_PREFIX_SAME, EXTR_PREFIX_ALL` |
|        - | 14151 | ` *  EXTR_PREFIX_INVALID or EXTR_PREFIX_IF_EXISTS. If the prefixed result is not a valid variable name` |
|        - | 14152 | ` *  it is not imported into the symbol table. Prefixes are automatically separated from the array key by an` |
|        - | 14153 | ` *  underscore character.` |
|        - | 14154 | ` * Return` |
|        - | 14155 | ` *   Returns the number of variables successfully imported into the symbol table.` |
|        - | 14156 | ` */` |
|        4 | 14157 | `static int vm_builtin_extract(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14158 |  |
|        - | 14159 | `	extract_aux_data sAux;` |
|        - | 14160 | `	ph7_hashmap *pMap;` |
|        5 | 14161 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|        - | 14162 | `		/* Missing/Invalid arguments,return 0 */` |
|      ! 0 | 14163 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14164 | `		return PH7_OK;` |
|        - | 14165 | `	}` |
|        - | 14166 | `	/* Point to the target hashmap */` |
|        5 | 14167 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|        5 | 14168 | `	if( pMap->nEntry < 1 ){` |
|        - | 14169 | `		/* Empty map,return  0 */` |
|      ! 0 | 14170 | `		ph7_result_int(pCtx,0);` |
|      ! 0 | 14171 | `		return PH7_OK;` |
|        - | 14172 | `	}` |
|        - | 14173 | `	/* Prepare the aux data */` |
|        5 | 14174 | `	SyZero(&sAux,sizeof(extract_aux_data)-sizeof(sAux.zWorker));` |
|        5 | 14175 | `	if( nArg > 1 ){` |
|        3 | 14176 | `		sAux.iFlags = ph7_value_to_int(apArg[1]);` |
|        3 | 14177 | `		if( nArg > 2 ){` |
|      ! 0 | 14178 | `			sAux.zPrefix = ph7_value_to_string(apArg[2],&sAux.Prefixlen);` |
|      ! 0 | 14179 | `		}` |
|        1 | 14180 | `	}` |
|        5 | 14181 | `	sAux.pVm = pCtx->pVm;` |
|        - | 14182 | `	/* Invoke the worker callback */` |
|        5 | 14183 | `	PH7_HashmapWalk(pMap,VmExtractCallback,&sAux);` |
|        - | 14184 | `	/* Number of variables successfully imported */` |
|        5 | 14185 | `	ph7_result_int(pCtx,sAux.iCount);` |
|        5 | 14186 | `	return PH7_OK;` |
|        3 | 14187 |  |
|        - | 14188 | `/*` |
|        - | 14189 | ` * Worker callback for the [extract()] function defined` |
|        - | 14190 | ` * below.` |
|        - | 14191 | ` */` |
|        8 | 14192 | `static int VmExtractCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14193 |  |
|        9 | 14194 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        9 | 14195 | `	int iFlags = pAux->iFlags;` |
|        9 | 14196 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14197 | `	ph7_value *pObj;` |
|        - | 14198 | `	SyString sVar;` |
|        9 | 14199 | `	if( (iFlags & 0x10/* EXTR_PREFIX_INVALID */) && (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL\|MEMOBJ_REAL))){` |
|      ! 0 | 14200 | `		iFlags \|= 0x08; /*EXTR_PREFIX_ALL*/` |
|      ! 0 | 14201 | `	}` |
|        - | 14202 | `	/* Perform a string cast */` |
|        9 | 14203 | `	PH7_MemObjToString(pKey);` |
|        9 | 14204 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14205 | `		/* Unavailable variable name */` |
|      ! 0 | 14206 | `		return SXRET_OK;` |
|        - | 14207 | `	}` |
|        9 | 14208 | `	sVar.nByte = 0; /* cc warning */` |
|        9 | 14209 | `	if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/ ) && pAux->Prefixlen > 0 ){` |
|      ! 0 | 14210 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14211 | `			pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14212 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14213 | `			);` |
|      ! 0 | 14214 | `	}else{` |
|       13 | 14215 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|        8 | 14216 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14217 | `	}` |
|        9 | 14218 | `	sVar.zString = pAux->zWorker;` |
|        - | 14219 | `	/* Try to extract the variable */` |
|        9 | 14220 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,FALSE);` |
|        9 | 14221 | `	if( pObj ){` |
|        - | 14222 | `		/* Collision */` |
|        5 | 14223 | `		if( iFlags & 0x02 /* EXTR_SKIP */ ){` |
|      ! 0 | 14224 | `			return SXRET_OK;` |
|        - | 14225 | `		}` |
|        5 | 14226 | `		if( iFlags & 0x04 /* EXTR_PREFIX_SAME */ ){` |
|      ! 0 | 14227 | `			if( (iFlags & 0x08/*EXTR_PREFIX_ALL*/) \|\| pAux->Prefixlen < 1){` |
|        - | 14228 | `				/* Already prefixed */` |
|      ! 0 | 14229 | `				return SXRET_OK;` |
|        - | 14230 | `			}` |
|      ! 0 | 14231 | `			sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s_%.*s",` |
|      ! 0 | 14232 | `				pAux->Prefixlen,pAux->zPrefix,` |
|      ! 0 | 14233 | `				SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14234 | `				);` |
|      ! 0 | 14235 | `			pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|      ! 0 | 14236 | `		}` |
|        3 | 14237 | `	}else{` |
|        - | 14238 | `		/* Create the variable */` |
|        5 | 14239 | `		pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        - | 14240 | `	}` |
|        9 | 14241 | `	if( pObj ){` |
|        - | 14242 | `		/* Overwrite the old value */` |
|        9 | 14243 | `		PH7_MemObjStore(pValue,pObj);` |
|        - | 14244 | `		/* Increment counter */` |
|        9 | 14245 | `		pAux->iCount++;` |
|        4 | 14246 | `	}` |
|        9 | 14247 | `	return SXRET_OK;` |
|        5 | 14248 |  |
|        - | 14249 | `/*` |
|        - | 14250 | ` * Worker callback for the [import_request_variables()] function` |
|        - | 14251 | ` * defined below.` |
|        - | 14252 | ` */` |
|        2 | 14253 | `static int VmImportRequestCallback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|        1 | 14254 |  |
|        3 | 14255 | `	extract_aux_data *pAux = (extract_aux_data *)pUserData;` |
|        3 | 14256 | `	ph7_vm *pVm = pAux->pVm;` |
|        - | 14257 | `	ph7_value *pObj;` |
|        - | 14258 | `	SyString sVar;` |
|        - | 14259 | `	/* Perform a string cast */` |
|        3 | 14260 | `	PH7_MemObjToString(pKey);` |
|        3 | 14261 | `	if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|        - | 14262 | `		/* Unavailable variable name */` |
|      ! 0 | 14263 | `		return SXRET_OK;` |
|        - | 14264 | `	}` |
|        3 | 14265 | `	sVar.nByte = 0; /* cc warning */` |
|        3 | 14266 | `	if( pAux->Prefixlen > 0 ){` |
|        4 | 14267 | `		sVar.nByte = (sxu32)SyBufferFormat(pAux->zWorker,sizeof(pAux->zWorker),"%.*s%.*s",` |
|        1 | 14268 | `			pAux->Prefixlen,pAux->zPrefix,` |
|        1 | 14269 | `			SyBlobLength(&pKey->sBlob),SyBlobData(&pKey->sBlob)` |
|        - | 14270 | `			);` |
|        2 | 14271 | `	}else{` |
|      ! 0 | 14272 | `		sVar.nByte = (sxu32) SyMemcpy(SyBlobData(&pKey->sBlob),pAux->zWorker,` |
|      ! 0 | 14273 | `			SXMIN(SyBlobLength(&pKey->sBlob),sizeof(pAux->zWorker)));` |
|        - | 14274 | `	}` |
|        3 | 14275 | `	sVar.zString = pAux->zWorker;` |
|        - | 14276 | `	/* Extract the variable */` |
|        3 | 14277 | `	pObj = VmExtractMemObj(pVm,&sVar,TRUE,TRUE);` |
|        3 | 14278 | `	if( pObj ){` |
|        3 | 14279 | `		PH7_MemObjStore(pValue,pObj);` |
|        1 | 14280 | `	}` |
|        3 | 14281 | `	return SXRET_OK;` |
|        2 | 14282 |  |
|        - | 14283 | `/*` |
|        - | 14284 | ` * bool import_request_variables(string $types[,string $prefix])` |
|        - | 14285 | ` *  Import GET/POST/Cookie variables into the global scope.` |
|        - | 14286 | ` * Parameters` |
|        - | 14287 | ` * $types` |
|        - | 14288 | ` *  Using the types parameter, you can specify which request variables to import.` |
|        - | 14289 | ` *  You can use 'G', 'P' and 'C' characters respectively for GET, POST and Cookie.` |
|        - | 14290 | ` *  These characters are not case sensitive, so you can also use any combination of 'g', 'p' and 'c'.` |
|        - | 14291 | ` *  POST includes the POST uploaded file information.` |
|        - | 14292 | ` *  Note:` |
|        - | 14293 | ` *  Note that the order of the letters matters, as when using "GP", the POST variables will overwrite` |
|        - | 14294 | ` *  GET variables with the same name. Any other letters than GPC are discarded.` |
|        - | 14295 | ` * $prefix` |
|        - | 14296 | ` *  Variable name prefix, prepended before all variable's name imported into the global scope.` |
|        - | 14297 | ` *  So if you have a GET value named "userid", and provide a prefix "pref_", then you'll get a global` |
|        - | 14298 | ` *  variable named $pref_userid.` |
|        - | 14299 | ` * Return` |
|        - | 14300 | ` *  TRUE on success or FALSE on failure.` |
|        - | 14301 | ` */` |
|        2 | 14302 | `static int vm_builtin_import_request_variables(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14303 |  |
|        - | 14304 | `	const char *zPrefix,*zEnd,*zImport;` |
|        - | 14305 | `	extract_aux_data sAux;` |
|        - | 14306 | `	int nLen,nPrefixLen;` |
|        - | 14307 | `	ph7_value *pSuper;` |
|        - | 14308 | `	ph7_vm *pVm;` |
|        - | 14309 | `	/* By default import only $_GET variables  */` |
|        3 | 14310 | `	zImport = "G";` |
|        3 | 14311 | `	nLen = (int)sizeof(char);` |
|        3 | 14312 | `	zPrefix = 0;` |
|        3 | 14313 | `	nPrefixLen = 0;` |
|        3 | 14314 | `	if( nArg > 0 ){` |
|        3 | 14315 | `		if( ph7_value_is_string(apArg[0]) ){` |
|        3 | 14316 | `			zImport = ph7_value_to_string(apArg[0],&nLen);` |
|        1 | 14317 | `		}` |
|        3 | 14318 | `		if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|        3 | 14319 | `			zPrefix = ph7_value_to_string(apArg[1],&nPrefixLen);` |
|        1 | 14320 | `		}` |
|        1 | 14321 | `	}` |
|        - | 14322 | `	/* Point to the underlying VM */` |
|        3 | 14323 | `	pVm = pCtx->pVm;` |
|        - | 14324 | `	/* Initialize the aux data */` |
|        3 | 14325 | `	SyZero(&sAux,sizeof(sAux)-sizeof(sAux.zWorker));` |
|        3 | 14326 | `	sAux.zPrefix = zPrefix;` |
|        3 | 14327 | `	sAux.Prefixlen = nPrefixLen;` |
|        3 | 14328 | `	sAux.pVm = pVm;` |
|        - | 14329 | `	/* Extract */` |
|        3 | 14330 | `	zEnd = &zImport[nLen];` |
|        5 | 14331 | `	while( zImport < zEnd ){` |
|        3 | 14332 | `		int c = zImport[0];` |
|        3 | 14333 | `		pSuper = 0;` |
|        3 | 14334 | `		if( c == 'G' \|\| c == 'g' ){` |
|        - | 14335 | `			/* Import $_GET variables */` |
|        3 | 14336 | `			pSuper = PH7_VmExtractSuper(pVm,"_GET",sizeof("_GET")-1);` |
|        1 | 14337 | `		}else if( c == 'P' \|\| c == 'p' ){` |
|        - | 14338 | `			/* Import $_POST variables */` |
|      ! 0 | 14339 | `			pSuper = PH7_VmExtractSuper(pVm,"_POST",sizeof("_POST")-1);` |
|      ! 0 | 14340 | `		}else if( c == 'c' \|\| c == 'C' ){` |
|        - | 14341 | `			/* Import $_COOKIE variables */` |
|      ! 0 | 14342 | `			pSuper = PH7_VmExtractSuper(pVm,"_COOKIE",sizeof("_COOKIE")-1);` |
|      ! 0 | 14343 | `		}` |
|        3 | 14344 | `		if( pSuper ){` |
|        - | 14345 | `			/* Iterate throw array entries */` |
|        3 | 14346 | `			ph7_array_walk(pSuper,VmImportRequestCallback,&sAux);` |
|        1 | 14347 | `		}` |
|        - | 14348 | `		/* Advance the cursor */` |
|        3 | 14349 | `		zImport++;` |
|        1 | 14350 | `	}` |
|        - | 14351 | `	/* All done,return TRUE*/` |
|        3 | 14352 | `	ph7_result_bool(pCtx,0);` |
|        3 | 14353 | `	return PH7_OK;` |
|        1 | 14354 |  |
|        - | 14355 | `/*` |
|        - | 14356 | ` * Compile and evaluate a PHP chunk at run-time.` |
|        - | 14357 | ` * Refer to the eval() language construct implementation for more` |
|        - | 14358 | ` * information.` |
|        - | 14359 | ` */` |
|    12488 | 14360 | `static sxi32 VmEvalChunk(` |
|        - | 14361 | `	ph7_vm *pVm,        /* Underlying Virtual Machine */` |
|        - | 14362 | `	ph7_context *pCtx,  /* Call Context */` |
|        - | 14363 | `	SyString *pChunk,   /* PHP chunk to evaluate */` |
|        - | 14364 | `	int iFlags,         /* Compile flag */` |
|        - | 14365 | `	int bTrueReturn     /* TRUE to return execution result */` |
|        - | 14366 | `	)` |
|        2 | 14367 |  |
|        - | 14368 | `	SySet *pByteCode,aByteCode;` |
|        - | 14369 | `	SyBlob sSavedNs;` |
|    12490 | 14370 | `	ProcConsumer xErr = 0;` |
|    12490 | 14371 | `	void *pErrData = 0;` |
|        - | 14372 | `	/* Initialize bytecode container */` |
|    12490 | 14373 | `	SySetInit(&aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|    12490 | 14374 | `	SySetAlloc(&aByteCode,0x20);` |
|        - | 14375 | `	/* Reset the code generator */` |
|    12490 | 14376 | `	if( bTrueReturn ){` |
|        - | 14377 | `		/* Included file,log compile-time errors */` |
|     9348 | 14378 | `		xErr = pVm->pEngine->xConf.xErr;` |
|     9348 | 14379 | `		pErrData = pVm->pEngine->xConf.pErrData;` |
|     4673 | 14380 | `	}` |
|    12490 | 14381 | `	PH7_ResetCodeGenerator(pVm,xErr,pErrData);` |
|        - | 14382 | `	/* Save and reset VM namespace state for the new compilation unit.` |
|        - | 14383 | `	 * Each included file has its own namespace scope; after execution,` |
|        - | 14384 | `	 * the caller's namespace is restored. */` |
|    12490 | 14385 | `	SyBlobInit(&sSavedNs,&pVm->sAllocator);` |
|    12490 | 14386 | `	SyBlobDup(&pVm->sNamespace,&sSavedNs);` |
|    12490 | 14387 | `	if( bTrueReturn ){` |
|        - | 14388 | `		/* Include/require: start in a fresh (global) namespace scope. */` |
|     9348 | 14389 | `		SyBlobReset(&pVm->sNamespace);` |
|     4673 | 14390 | `	}` |
|        - | 14391 | `	/* Swap bytecode container */` |
|    12490 | 14392 | `	pByteCode = pVm->pByteContainer;` |
|    12490 | 14393 | `	pVm->pByteContainer = &aByteCode;` |
|        - | 14394 | `	/* Compile the chunk */` |
|    12490 | 14395 | `	PH7_CompileScript(pVm,pChunk,iFlags);` |
|    18734 | 14396 | `	if( pVm->sCodeGen.nErr > 0 ){` |
|        - | 14397 | `		/* Compilation error,return false */` |
|        3 | 14398 | `		if( pCtx ){` |
|        3 | 14399 | `			ph7_result_bool(pCtx,0);` |
|        1 | 14400 | `		}` |
|        2 | 14401 | `	}else{` |
|        - | 14402 | `		/* Mount any newly defined classes */` |
|        - | 14403 | `		SyHashEntry *pEntry;` |
|        - | 14404 | `		ph7_class *pClass;` |
|        - | 14405 | `		ph7_value sResult; /* Return value */` |
|        - | 14406 | `		sxi32 rc;` |
|    12488 | 14407 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|   722595 | 14408 | `		while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   703866 | 14409 | `			pClass = (ph7_class *)pEntry->pUserData;` |
|        - | 14410 | `			/* Only mount classes that haven't been mounted yet */` |
|   703866 | 14411 | `			if( !pClass->bMounted ){` |
|   189268 | 14412 | `				rc = VmMountUserClass(pVm,pClass);` |
|   189268 | 14413 | `				if( rc != SXRET_OK ){` |
|        - | 14414 | `					/* Mount failure (likely memory error) */` |
|      ! 0 | 14415 | `					if( pCtx ){` |
|      ! 0 | 14416 | `						ph7_result_bool(pCtx,0);` |
|      ! 0 | 14417 | `					}` |
|      ! 0 | 14418 | `					goto Cleanup;` |
|        - | 14419 | `				}` |
|    94633 | 14420 | `			}` |
|        2 | 14421 | `		}` |
|    12488 | 14422 | `		if( SXRET_OK != PH7_VmEmitInstr(pVm,PH7_OP_DONE,0,0,0,0) ){` |
|        - | 14423 | `			/* Out of memory */` |
|      ! 0 | 14424 | `			if( pCtx ){` |
|      ! 0 | 14425 | `				ph7_result_bool(pCtx,0);` |
|      ! 0 | 14426 | `			}` |
|      ! 0 | 14427 | `			goto Cleanup;` |
|        - | 14428 | `		}` |
|    12488 | 14429 | `		if( bTrueReturn ){` |
|        - | 14430 | `			/* Assume a boolean true return value */` |
|     9348 | 14431 | `			PH7_MemObjInitFromBool(pVm,&sResult,1);` |
|     4675 | 14432 | `		}else{` |
|        - | 14433 | `			/* Assume a null return value */` |
|     3142 | 14434 | `			PH7_MemObjInit(pVm,&sResult);` |
|        - | 14435 | `		}` |
|        - | 14436 | `		/* Execute the compiled chunk */` |
|    12488 | 14437 | `		VmLocalExec(pVm,&aByteCode,&sResult);` |
|    12488 | 14438 | `		if( pCtx ){` |
|        - | 14439 | `			/* Set the execution result */` |
|     9366 | 14440 | `			ph7_result_value(pCtx,&sResult);` |
|     4682 | 14441 | `		}` |
|    12488 | 14442 | `		PH7_MemObjRelease(&sResult);` |
|        - | 14443 | `	}` |
|     6244 | 14444 | `Cleanup:` |
|        - | 14445 | `	/* Cleanup the mess left behind */` |
|    12490 | 14446 | `	pVm->pByteContainer = pByteCode;` |
|    12490 | 14447 | `	SySetRelease(&aByteCode);` |
|        - | 14448 | `	/* Restore caller's namespace state */` |
|    12490 | 14449 | `	SyBlobReset(&pVm->sNamespace);` |
|    12490 | 14450 | `	SyBlobDup(&sSavedNs,&pVm->sNamespace);` |
|    12490 | 14451 | `	SyBlobRelease(&sSavedNs);` |
|    12490 | 14452 | `	return SXRET_OK;` |
|        2 | 14453 |  |
|        - | 14454 | `/*` |
|        - | 14455 | ` * value eval(string $code)` |
|        - | 14456 | ` *   Evaluate a string as PHP code.` |
|        - | 14457 | ` * Parameter` |
|        - | 14458 | ` *  code: PHP code to evaluate.` |
|        - | 14459 | ` * Return` |
|        - | 14460 | ` *  eval() returns NULL unless return is called in the evaluated code, in which case` |
|        - | 14461 | ` *  the value passed to return is returned. If there is a parse error in the evaluated` |
|        - | 14462 | ` *  code, eval() returns FALSE and execution of the following code continues normally.` |
|        - | 14463 | ` */` |
|       22 | 14464 | `static int vm_builtin_eval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14465 |  |
|        - | 14466 | `	SyString sChunk;    /* Chunk to evaluate */` |
|       24 | 14467 | `	if( nArg < 1 ){` |
|        - | 14468 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14469 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14470 | `		return SXRET_OK;` |
|        - | 14471 | `	}` |
|        - | 14472 | `	/* Chunk to evaluate */` |
|       24 | 14473 | `	sChunk.zString = ph7_value_to_string(apArg[0],(int *)&sChunk.nByte);` |
|       24 | 14474 | `	if( sChunk.nByte < 1 ){` |
|        - | 14475 | `		/* Empty string,return NULL */` |
|        3 | 14476 | `		ph7_result_null(pCtx);` |
|        3 | 14477 | `		return SXRET_OK;` |
|        - | 14478 | `	}` |
|        - | 14479 | `	/* Eval the chunk */` |
|       22 | 14480 | `	VmEvalChunk(pCtx->pVm,&(*pCtx),&sChunk,PH7_PHP_ONLY,FALSE);` |
|       22 | 14481 | `	return SXRET_OK;` |
|       13 | 14482 |  |
|        - | 14483 | `/*` |
|        - | 14484 | ` * Check if a file path is already included.` |
|        - | 14485 | ` */` |
|    18688 | 14486 | `static int VmIsIncludedFile(ph7_vm *pVm,SyString *pFile)` |
|        2 | 14487 |  |
|        - | 14488 | `	SyString *aEntries;` |
|        - | 14489 | `	sxu32 n;` |
|    18690 | 14490 | `	aEntries = (SyString *)SySetBasePtr(&pVm->aIncluded);` |
|        - | 14491 | `	/* Perform a linear search */` |
| 87255076 | 14492 | `	for( n = 0 ; n < SySetUsed(&pVm->aIncluded) ; ++n ){` |
| 87236394 | 14493 | `		if( SyStringCmp(pFile,&aEntries[n],SyMemcmp) == 0 ){` |
|        - | 14494 | `			/* Already included */` |
|        7 | 14495 | `			return TRUE;` |
|        - | 14496 | `		}` |
| 43618195 | 14497 | `	}` |
|    18684 | 14498 | `	return FALSE;` |
|     9346 | 14499 |  |
|        - | 14500 | `/*` |
|        - | 14501 | ` * Push a file path in the appropriate VM container.` |
|        - | 14502 | ` */` |
|    21802 | 14503 | `PH7_PRIVATE sxi32 PH7_VmPushFilePath(ph7_vm *pVm,const char *zPath,int nLen,sxu8 bMain,sxi32 *pNew)` |
|        2 | 14504 |  |
|        - | 14505 | `	SyString sPath;` |
|        - | 14506 | `	char *zDup;` |
|        - | 14507 | `#ifdef __WINNT__` |
|        - | 14508 | `	char *zCur;` |
|        - | 14509 | `#endif` |
|        - | 14510 | `	sxi32 rc;` |
|    21804 | 14511 | `	if( nLen < 0 ){` |
|     3116 | 14512 | `		nLen = SyStrlen(zPath);` |
|     1557 | 14513 | `	}` |
|        - | 14514 | `	/* Duplicate the file path first */` |
|    21804 | 14515 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zPath,nLen);` |
|    21804 | 14516 | `	if( zDup == 0 ){` |
|      ! 0 | 14517 | `		return SXERR_MEM;` |
|        - | 14518 | `	}` |
|        - | 14519 | `#ifdef __WINNT__` |
|        - | 14520 | `	/* Normalize path on windows` |
|        - | 14521 | `	 * Example:` |
|        - | 14522 | `	 *    Path/To/File.php` |
|        - | 14523 | `	 * becomes` |
|        - | 14524 | `	 *   path\to\file.php` |
|        - | 14525 | `	 */` |
|        2 | 14526 | `	zCur = zDup;` |
|        2 | 14527 | `	while( zCur[0] != 0 ){` |
|        2 | 14528 | `		if( zCur[0] == '/' ){` |
|        2 | 14529 | `			zCur[0] = '\\';` |
|        2 | 14530 | `		}else if( (unsigned char)zCur[0] < 0xc0 && SyisUpper(zCur[0]) ){` |
|        1 | 14531 | `			int c = SyToLower(zCur[0]);` |
|        1 | 14532 | `			zCur[0] = (char)c; /* MSVC stupidity */` |
|        - | 14533 | `		}` |
|        2 | 14534 | `		zCur++;` |
|        2 | 14535 | `	}` |
|        - | 14536 | `#endif` |
|        - | 14537 | `	/* Install the file path */` |
|    21804 | 14538 | `	SyStringInitFromBuf(&sPath,zDup,nLen);` |
|    21804 | 14539 | `	if( !bMain ){` |
|    18690 | 14540 | `		if( VmIsIncludedFile(&(*pVm),&sPath) ){` |
|        - | 14541 | `			/* Already included */` |
|        7 | 14542 | `			*pNew = 0;` |
|        4 | 14543 | `		}else{` |
|        - | 14544 | `			/* Insert in the corresponding container */` |
|    18684 | 14545 | `			rc = SySetPut(&pVm->aIncluded,(const void *)&sPath);` |
|    18684 | 14546 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 14547 | `				SyMemBackendFree(&pVm->sAllocator,zDup);` |
|      ! 0 | 14548 | `				return rc;` |
|        - | 14549 | `			}` |
|    18684 | 14550 | `			*pNew = 1;` |
|        - | 14551 | `		}` |
|     9344 | 14552 | `	}` |
|    21804 | 14553 | `	SySetPut(&pVm->aFiles,(const void *)&sPath);` |
|    21804 | 14554 | `	return SXRET_OK;` |
|    10903 | 14555 |  |
|        - | 14556 | `/*` |
|        - | 14557 | ` * Compile and Execute a PHP script at run-time.` |
|        - | 14558 | ` * SXRET_OK is returned on sucessful evaluation.Any other return values` |
|        - | 14559 | ` * indicates failure.` |
|        - | 14560 | ` * Note that the PHP script to evaluate can be a local or remote file.In` |
|        - | 14561 | ` * either cases the [PH7_StreamReadWholeFile()] function handle all the underlying` |
|        - | 14562 | ` * operations.` |
|        - | 14563 | ` * If the [PH7_DISABLE_BUILTIN_FUNC] compile-time directive is defined,then` |
|        - | 14564 | ` * this function is a no-op.` |
|        - | 14565 | ` * Refer to the implementation of the include(),include_once() language` |
|        - | 14566 | ` * constructs for more information.` |
|        - | 14567 | ` */` |
|     9356 | 14568 | `static sxi32 VmExecIncludedFile(` |
|        - | 14569 | `	 ph7_context *pCtx, /* Call Context */` |
|        - | 14570 | `	 SyString *pPath,   /* Script path or URL*/` |
|        - | 14571 | `	 int IncludeOnce    /* TRUE if called from include_once() or require_once() */` |
|        - | 14572 | `	 )` |
|        2 | 14573 |  |
|        - | 14574 | `	sxi32 rc;` |
|        - | 14575 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 14576 | `	const ph7_io_stream *pStream;` |
|        - | 14577 | `	SyBlob sContents;` |
|        - | 14578 | `	void *pHandle;` |
|        - | 14579 | `	ph7_vm *pVm;` |
|        - | 14580 | `	int isNew;` |
|        - | 14581 | `	/* Initialize fields */` |
|     9358 | 14582 | `	pVm = pCtx->pVm;` |
|     9358 | 14583 | `	SyBlobInit(&sContents,&pVm->sAllocator);` |
|     9358 | 14584 | `	isNew = 0;` |
|        - | 14585 | `	/* Extract the associated stream */` |
|     9358 | 14586 | `	pStream = PH7_VmGetStreamDevice(pVm,&pPath->zString,pPath->nByte);` |
|        - | 14587 | `	/*` |
|        - | 14588 | `	 * Open the file or the URL [i.e: http://ph7.symisc.net/example/hello.php"]` |
|        - | 14589 | `	 * in a read-only mode.` |
|        - | 14590 | `	 */` |
|     9358 | 14591 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,pPath->zString,PH7_IO_OPEN_RDONLY,TRUE,0,TRUE,&isNew);` |
|     9358 | 14592 | `	if( pHandle == 0 ){` |
|        8 | 14593 | `		return SXERR_IO;` |
|        - | 14594 | `	}` |
|     9352 | 14595 | `	rc = SXRET_OK; /* Stupid cc warning */` |
|     9352 | 14596 | `	if( IncludeOnce && !isNew ){` |
|        - | 14597 | `		/* Already included */` |
|        5 | 14598 | `		rc = SXERR_EXISTS;` |
|        3 | 14599 | `	}else{` |
|        - | 14600 | `		/* Read the whole file contents */` |
|     9348 | 14601 | `		rc = PH7_StreamReadWholeFile(pHandle,pStream,&sContents);` |
|     9348 | 14602 | `		if( rc == SXRET_OK ){` |
|        - | 14603 | `			SyString sScript;` |
|        - | 14604 | `			/* Compile and execute the script */` |
|     9348 | 14605 | `			SyStringInitFromBuf(&sScript,SyBlobData(&sContents),SyBlobLength(&sContents));` |
|     9348 | 14606 | `			VmEvalChunk(pCtx->pVm,&(*pCtx),&sScript,0,TRUE);` |
|     4673 | 14607 | `		}` |
|        - | 14608 | `	}` |
|        - | 14609 | `	/* Pop from the set of included file */` |
|     9352 | 14610 | `	(void)SySetPop(&pVm->aFiles);` |
|        - | 14611 | `	/* Close the handle */` |
|     9352 | 14612 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|        - | 14613 | `	/* Release the working buffer */` |
|     9352 | 14614 | `	SyBlobRelease(&sContents);` |
|        - | 14615 | `#else` |
|        - | 14616 | `	SXUNUSED(pCtx); /* cc warning */` |
|        - | 14617 | `	SXUNUSED(pPath);` |
|        - | 14618 | `	SXUNUSED(IncludeOnce);` |
|        - | 14619 | `	rc = SXERR_IO;` |
|        - | 14620 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     9352 | 14621 | `	return rc;` |
|     4680 | 14622 |  |
|        - | 14623 | `/*` |
|        - | 14624 | ` * string get_include_path(void)` |
|        - | 14625 | ` *  Gets the current include_path configuration option.` |
|        - | 14626 | ` * Parameter` |
|        - | 14627 | ` *  None` |
|        - | 14628 | ` * Return` |
|        - | 14629 | ` *  Included paths as a string` |
|        - | 14630 | ` */` |
|        2 | 14631 | `static int vm_builtin_get_include_path(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14632 |  |
|        3 | 14633 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14634 | `	SyString *aEntry;` |
|        - | 14635 | `	int dir_sep;` |
|        - | 14636 | `	sxu32 n;` |
|        - | 14637 | `#ifdef __WINNT__` |
|        1 | 14638 | `	dir_sep = ';';` |
|        - | 14639 | `#else` |
|        - | 14640 | `	/* Assume UNIX path separator */` |
|        2 | 14641 | `	dir_sep = ':';` |
|        - | 14642 | `#endif` |
|        1 | 14643 | `	SXUNUSED(nArg); /* cc warning */` |
|        1 | 14644 | `	SXUNUSED(apArg);` |
|        - | 14645 | `	/* Point to the list of import paths */` |
|        3 | 14646 | `	aEntry = (SyString *)SySetBasePtr(&pVm->aPaths);` |
|        5 | 14647 | `	for( n = 0 ; n < SySetUsed(&pVm->aPaths) ; n++ ){` |
|        3 | 14648 | `		SyString *pEntry = &aEntry[n];` |
|        3 | 14649 | `		if( n > 0 ){` |
|        - | 14650 | `			/* Append dir seprator */` |
|      ! 0 | 14651 | `			ph7_result_string(pCtx,(const char *)&dir_sep,sizeof(char));` |
|      ! 0 | 14652 | `		}` |
|        - | 14653 | `		/* Append path */` |
|        3 | 14654 | `		ph7_result_string(pCtx,pEntry->zString,(int)pEntry->nByte);` |
|        2 | 14655 | `	}` |
|        3 | 14656 | `	return PH7_OK;` |
|        1 | 14657 |  |
|        - | 14658 | `/*` |
|        - | 14659 | ` * string get_get_included_files(void)` |
|        - | 14660 | ` *  Gets the current include_path configuration option.` |
|        - | 14661 | ` * Parameter` |
|        - | 14662 | ` *  None` |
|        - | 14663 | ` * Return` |
|        - | 14664 | ` *  Included paths as a string` |
|        - | 14665 | ` */` |
|        2 | 14666 | `static int vm_builtin_get_included_files(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14667 |  |
|        3 | 14668 | `	SySet *pFiles = &pCtx->pVm->aFiles;` |
|        - | 14669 | `	ph7_value *pArray,*pWorker;` |
|        - | 14670 | `	SyString *pEntry;` |
|        - | 14671 | `	int c,d;` |
|        - | 14672 | `	/* Create an array and a working value */` |
|        3 | 14673 | `	pArray  = ph7_context_new_array(pCtx);` |
|        3 | 14674 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|        3 | 14675 | `	if( pArray == 0 \|\| pWorker == 0 ){` |
|        - | 14676 | `		/* Out of memory,return null */` |
|      ! 0 | 14677 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14678 | `		SXUNUSED(nArg); /* cc warning */` |
|      ! 0 | 14679 | `		SXUNUSED(apArg);` |
|      ! 0 | 14680 | `		return PH7_OK;` |
|        - | 14681 | `	}` |
|        3 | 14682 | `	c = d = '/';` |
|        - | 14683 | `#ifdef __WINNT__` |
|        1 | 14684 | `	d = '\\';` |
|        - | 14685 | `#endif` |
|        - | 14686 | `	/* Iterate throw entries */` |
|        3 | 14687 | `	SySetResetCursor(pFiles);` |
|     3839 | 14688 | `	while( SXRET_OK == SySetGetNextEntry(pFiles,(void **)&pEntry) ){` |
|        - | 14689 | `		const char *zBase,*zEnd;` |
|        - | 14690 | `		int iLen;` |
|        - | 14691 | `		/* reset the string cursor */` |
|     3837 | 14692 | `		ph7_value_reset_string_cursor(pWorker);` |
|        - | 14693 | `		/* Extract base name */` |
|     3837 | 14694 | `		zEnd = &pEntry->zString[pEntry->nByte - 1];` |
|        - | 14695 | `		/* Ignore trailing '/' */` |
|     5755 | 14696 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] == c \|\| (int)zEnd[0] == d ) ){` |
|      ! 0 | 14697 | `			zEnd--;` |
|      ! 0 | 14698 | `		}` |
|     3837 | 14699 | `		iLen = (int)(&zEnd[1]-pEntry->zString);` |
|   118297 | 14700 | `		while( zEnd > pEntry->zString && ( (int)zEnd[0] != c && (int)zEnd[0] != d ) ){` |
|   112543 | 14701 | `			zEnd--;` |
|        1 | 14702 | `		}` |
|     3837 | 14703 | `		zBase = (zEnd > pEntry->zString) ? &zEnd[1] : pEntry->zString;` |
|     3837 | 14704 | `		zEnd = &pEntry->zString[iLen];` |
|        - | 14705 | `		/* Copy entry name */` |
|     3837 | 14706 | `		ph7_value_string(pWorker,zBase,(int)(zEnd-zBase));` |
|        - | 14707 | `		/* Perform the insertion */` |
|     3837 | 14708 | `		ph7_array_add_elem(pArray,0/* Automatic index assign*/,pWorker); /* Will make it's own copy */` |
|        1 | 14709 | `	}` |
|        - | 14710 | `	/* All done,return the created array */` |
|        3 | 14711 | `	ph7_result_value(pCtx,pArray);` |
|        - | 14712 | `	/* Note that 'pWorker' will be automatically destroyed` |
|        - | 14713 | `	 * by the engine as soon we return from this foreign` |
|        - | 14714 | `	 * function.` |
|        - | 14715 | `	 */` |
|        3 | 14716 | `	return PH7_OK;` |
|        2 | 14717 |  |
|        - | 14718 | `/*` |
|        - | 14719 | ` * include:` |
|        - | 14720 | ` * According to the PHP reference manual.` |
|        - | 14721 | ` *  The include() function includes and evaluates the specified file.` |
|        - | 14722 | ` *  Files are included based on the file path given or, if none is given` |
|        - | 14723 | ` *  the include_path specified.If the file isn't found in the include_path` |
|        - | 14724 | ` *  include() will finally check in the calling script's own directory` |
|        - | 14725 | ` *  and the current working directory before failing. The include()` |
|        - | 14726 | ` *  construct will emit a warning if it cannot find a file; this is different` |
|        - | 14727 | ` *  behavior from require(), which will emit a fatal error.` |
|        - | 14728 | ` *  If a path is defined � whether absolute (starting with a drive letter` |
|        - | 14729 | ` *  or \ on Windows, or / on Unix/Linux systems) or relative to the current` |
|        - | 14730 | ` *  directory (starting with . or ..) � the include_path will be ignored altogether.` |
|        - | 14731 | ` *  For example, if a filename begins with ../, the parser will look in the parent` |
|        - | 14732 | ` *  directory to find the requested file.` |
|        - | 14733 | ` *  When a file is included, the code it contains inherits the variable scope` |
|        - | 14734 | ` *  of the line on which the include occurs. Any variables available at that line` |
|        - | 14735 | ` *  in the calling file will be available within the called file, from that point forward.` |
|        - | 14736 | ` *  However, all functions and classes defined in the included file have the global scope.` |
|        - | 14737 | ` */` |
|     9338 | 14738 | `static int vm_builtin_include(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14739 |  |
|        - | 14740 | `	SyString sFile;` |
|        - | 14741 | `	sxi32 rc;` |
|     9340 | 14742 | `	if( nArg < 1 ){` |
|        - | 14743 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14744 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14745 | `		return SXRET_OK;` |
|        - | 14746 | `	}` |
|        - | 14747 | `	/* File to include */` |
|     9340 | 14748 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|     9340 | 14749 | `	if( sFile.nByte < 1 ){` |
|        - | 14750 | `		/* Empty string,return NULL */` |
|      ! 0 | 14751 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14752 | `		return SXRET_OK;` |
|        - | 14753 | `	}` |
|        - | 14754 | `	/* Open,compile and execute the desired script */` |
|     9340 | 14755 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|     9340 | 14756 | `	if( rc != SXRET_OK ){` |
|        - | 14757 | `		/* Emit a warning and return false */` |
|        3 | 14758 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|        3 | 14759 | `		ph7_result_bool(pCtx,0);` |
|        1 | 14760 | `	}` |
|     9340 | 14761 | `	return SXRET_OK;` |
|     4671 | 14762 |  |
|        - | 14763 | `/*` |
|        - | 14764 | ` * include_once:` |
|        - | 14765 | ` *  According to the PHP reference manual.` |
|        - | 14766 | ` *   The include_once() statement includes and evaluates the specified file during` |
|        - | 14767 | ` *   the execution of the script. This is a behavior similar to the include()` |
|        - | 14768 | ` *   statement, with the only difference being that if the code from a file has already` |
|        - | 14769 | ` *   been included, it will not be included again. As the name suggests, it will be included` |
|        - | 14770 | ` *   just once.` |
|        - | 14771 | ` */` |
|        4 | 14772 | `static int vm_builtin_include_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14773 |  |
|        - | 14774 | `	SyString sFile;` |
|        - | 14775 | `	sxi32 rc;` |
|        5 | 14776 | `	if( nArg < 1 ){` |
|        - | 14777 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14778 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14779 | `		return SXRET_OK;` |
|        - | 14780 | `	}` |
|        - | 14781 | `	/* File to include */` |
|        5 | 14782 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14783 | `	if( sFile.nByte < 1 ){` |
|        - | 14784 | `		/* Empty string,return NULL */` |
|      ! 0 | 14785 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14786 | `		return SXRET_OK;` |
|        - | 14787 | `	}` |
|        - | 14788 | `	/* Open,compile and execute the desired script */` |
|        5 | 14789 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14790 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14791 | `		/* File already included,return TRUE */` |
|        3 | 14792 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14793 | `		return SXRET_OK;` |
|        - | 14794 | `	}` |
|        3 | 14795 | `	if( rc != SXRET_OK ){` |
|        - | 14796 | `		/* Emit a warning and return false */` |
|      ! 0 | 14797 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14798 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14799 | ` 	}` |
|        3 | 14800 | `	return SXRET_OK;` |
|        3 | 14801 |  |
|        - | 14802 | `/*` |
|        - | 14803 | ` * require.` |
|        - | 14804 | ` *  According to the PHP reference manual.` |
|        - | 14805 | ` *   require() is identical to include() except upon failure it will` |
|        - | 14806 | ` *   also produce a fatal level error.` |
|        - | 14807 | ` *   In other words, it will halt the script whereas include() only` |
|        - | 14808 | ` *   emits a warning  which allows the script to continue.` |
|        - | 14809 | ` */` |
|        6 | 14810 | `static int vm_builtin_require(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14811 |  |
|        - | 14812 | `	SyString sFile;` |
|        - | 14813 | `	sxi32 rc;` |
|        8 | 14814 | `	if( nArg < 1 ){` |
|        - | 14815 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14816 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14817 | `		return SXRET_OK;` |
|        - | 14818 | `	}` |
|        - | 14819 | `	/* File to include */` |
|        8 | 14820 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        8 | 14821 | `	if( sFile.nByte < 1 ){` |
|        - | 14822 | `		/* Empty string,return NULL */` |
|      ! 0 | 14823 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14824 | `		return SXRET_OK;` |
|        - | 14825 | `	}` |
|        - | 14826 | `	/* Open,compile and execute the desired script */` |
|        8 | 14827 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,FALSE);` |
|        8 | 14828 | `	if( rc != SXRET_OK ){` |
|        - | 14829 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14830 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14831 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14832 | `		return PH7_ABORT;` |
|        - | 14833 | `	}` |
|        8 | 14834 | `	return SXRET_OK;` |
|        5 | 14835 |  |
|        - | 14836 | `/*` |
|        - | 14837 | ` * require_once:` |
|        - | 14838 | ` *  According to the PHP reference manual.` |
|        - | 14839 | ` *   The require_once() statement is identical to require() except PHP will check` |
|        - | 14840 | ` *   if the file has already been included, and if so, not include (require) it again.` |
|        - | 14841 | ` *   See the include_once() documentation for information about the _once behaviour` |
|        - | 14842 | ` *   and how it differs from its non _once siblings.` |
|        - | 14843 | ` */` |
|        4 | 14844 | `static int vm_builtin_require_once(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 14845 |  |
|        - | 14846 | `	SyString sFile;` |
|        - | 14847 | `	sxi32 rc;` |
|        5 | 14848 | `	if( nArg < 1 ){` |
|        - | 14849 | `		/* Nothing to evaluate,return NULL */` |
|      ! 0 | 14850 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14851 | `		return SXRET_OK;` |
|        - | 14852 | `	}` |
|        - | 14853 | `	/* File to include */` |
|        5 | 14854 | `	sFile.zString = ph7_value_to_string(apArg[0],(int *)&sFile.nByte);` |
|        5 | 14855 | `	if( sFile.nByte < 1 ){` |
|        - | 14856 | `		/* Empty string,return NULL */` |
|      ! 0 | 14857 | `		ph7_result_null(pCtx);` |
|      ! 0 | 14858 | `		return SXRET_OK;` |
|        - | 14859 | `	}` |
|        - | 14860 | `	/* Open,compile and execute the desired script */` |
|        5 | 14861 | `	rc = VmExecIncludedFile(&(*pCtx),&sFile,TRUE);` |
|        5 | 14862 | `	if( rc == SXERR_EXISTS ){` |
|        - | 14863 | `		/* File already included,return TRUE */` |
|        3 | 14864 | `		ph7_result_bool(pCtx,1);` |
|        3 | 14865 | `		return SXRET_OK;` |
|        - | 14866 | `	}` |
|        3 | 14867 | `	if( rc != SXRET_OK ){` |
|        - | 14868 | `		/* Fatal,abort VM execution immediately */` |
|      ! 0 | 14869 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,"Fatal IO error while importing: '%z'",&sFile);` |
|      ! 0 | 14870 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14871 | `		return PH7_ABORT;` |
|        - | 14872 | `	}` |
|        3 | 14873 | `	return SXRET_OK;` |
|        3 | 14874 |  |
|        - | 14875 | `/* Getopt builtins moved to vm_builtin_getopt.c */` |
|        - | 14876 | `/* JSON encoding/decoding routines moved to vm_json.c */` |
|        - | 14877 | `/* XML processing and UTF-8 routines moved to vm_xml.c */` |
|        - | 14878 | `/*` |
|        - | 14879 | ` * Section:` |
|        - | 14880 | ` *  SPL Autoloading functions.` |
|        - | 14881 | ` * Status:` |
|        - | 14882 | ` *  Stable.` |
|        - | 14883 | ` */` |
|        - | 14884 | `/*` |
|        - | 14885 | ` * bool spl_autoload_register([ callable $callback [, bool $throw = true [, bool $prepend = false ]]])` |
|        - | 14886 | ` *  Register given function as __autoload() implementation.` |
|        - | 14887 | ` * Parameters` |
|        - | 14888 | ` *  callback` |
|        - | 14889 | ` *   The autoload function being registered. If no parameter is provided,` |
|        - | 14890 | ` *   then the default implementation of spl_autoload() will be registered.` |
|        - | 14891 | ` *  throw` |
|        - | 14892 | ` *   This parameter specifies whether spl_autoload_register() should throw` |
|        - | 14893 | ` *   exceptions on error. (Ignored in this implementation — always succeeds.)` |
|        - | 14894 | ` *  prepend` |
|        - | 14895 | ` *   If true, spl_autoload_register() will prepend the autoloader on the` |
|        - | 14896 | ` *   autoload stack instead of appending it.` |
|        - | 14897 | ` * Return` |
|        - | 14898 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14899 | ` */` |
|       34 | 14900 | `static int vm_builtin_spl_autoload_register(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14901 |  |
|        - | 14902 | `	VmAutoloadCB sEntry;` |
|       36 | 14903 | `	ph7_vm *pVm = pCtx->pVm;` |
|       36 | 14904 | `	int iPrepend = 0;` |
|        - | 14905 | `	sxu32 n;` |
|       36 | 14906 | `	if( nArg < 1 ){` |
|        - | 14907 | `		/* No callback provided — register default spl_autoload.` |
|        - | 14908 | `		 * Store the string "spl_autoload" as the callback. */` |
|        - | 14909 | `		/* Check for duplicates first */` |
|        9 | 14910 | `		for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|        5 | 14911 | `			VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|        6 | 14912 | `			if( pExisting && (pExisting->sCallback.iFlags & MEMOBJ_STRING)` |
|        4 | 14913 | `				&& SyBlobLength(&pExisting->sCallback.sBlob) == sizeof("spl_autoload")-1` |
|        5 | 14914 | `				&& SyMemcmp(SyBlobData(&pExisting->sCallback.sBlob),"spl_autoload",sizeof("spl_autoload")-1) == 0 ){` |
|        5 | 14915 | `				ph7_result_bool(pCtx,1);` |
|        5 | 14916 | `				return SXRET_OK;` |
|        - | 14917 | `			}` |
|      ! 0 | 14918 | `		}` |
|        5 | 14919 | `		SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|        5 | 14920 | `		PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|        5 | 14921 | `		PH7_MemObjStringAppend(&sEntry.sCallback,"spl_autoload",sizeof("spl_autoload")-1);` |
|        5 | 14922 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        5 | 14923 | `		ph7_result_bool(pCtx,1);` |
|        5 | 14924 | `		return SXRET_OK;` |
|        - | 14925 | `	}` |
|        - | 14926 | `	/* Validate that the callback is callable */` |
|       28 | 14927 | `	if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      ! 0 | 14928 | `		int iThrow = 1; /* Default: throw on error */` |
|      ! 0 | 14929 | `		if( nArg >= 2 ){` |
|      ! 0 | 14930 | `			iThrow = ph7_value_to_bool(apArg[1]);` |
|      ! 0 | 14931 | `		}` |
|      ! 0 | 14932 | `		if( iThrow ){` |
|      ! 0 | 14933 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|        - | 14934 | `				"Argument is not callable");` |
|      ! 0 | 14935 | `		}` |
|      ! 0 | 14936 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14937 | `		return SXRET_OK;` |
|        - | 14938 | `	}` |
|        - | 14939 | `	/* Check for duplicates */` |
|       46 | 14940 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       20 | 14941 | `		VmAutoloadCB *pExisting = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       20 | 14942 | `		if( pExisting && PH7_MemObjCmp(&pExisting->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 14943 | `			/* Already registered */` |
|      ! 0 | 14944 | `			ph7_result_bool(pCtx,1);` |
|      ! 0 | 14945 | `			return SXRET_OK;` |
|        - | 14946 | `		}` |
|       11 | 14947 | `	}` |
|        - | 14948 | `	/* Check prepend flag */` |
|       28 | 14949 | `	if( nArg >= 3 ){` |
|        3 | 14950 | `		iPrepend = ph7_value_to_bool(apArg[2]);` |
|        1 | 14951 | `	}` |
|        - | 14952 | `	/* Store the callback */` |
|       28 | 14953 | `	SyZero(&sEntry,sizeof(VmAutoloadCB));` |
|       28 | 14954 | `	PH7_MemObjInit(pVm,&sEntry.sCallback);` |
|       28 | 14955 | `	PH7_MemObjStore(apArg[0],&sEntry.sCallback);` |
|       29 | 14956 | `	if( iPrepend && SySetUsed(&pVm->aAutoload) > 0 ){` |
|        - | 14957 | `		/* Prepend: shift existing entries and insert at position 0.` |
|        - | 14958 | `		 * We do this by appending first, then rotating the array. */` |
|        3 | 14959 | `		sxu32 nTotal = SySetUsed(&pVm->aAutoload);` |
|        - | 14960 | `		VmAutoloadCB *aBase;` |
|        3 | 14961 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14962 | `		/* Rotate: move last entry to front */` |
|        3 | 14963 | `		aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        3 | 14964 | `		if( aBase ){` |
|        - | 14965 | `			VmAutoloadCB sTemp;` |
|        - | 14966 | `			sxu32 i;` |
|        3 | 14967 | `			SyMemcpy(&aBase[nTotal],&sTemp,sizeof(VmAutoloadCB));` |
|        7 | 14968 | `			for( i = nTotal ; i > 0 ; i-- ){` |
|        5 | 14969 | `				SyMemcpy(&aBase[i-1],&aBase[i],sizeof(VmAutoloadCB));` |
|        3 | 14970 | `			}` |
|        3 | 14971 | `			SyMemcpy(&sTemp,&aBase[0],sizeof(VmAutoloadCB));` |
|        1 | 14972 | `		}` |
|        2 | 14973 | `	}else{` |
|       26 | 14974 | `		SySetPut(&pVm->aAutoload,(const void *)&sEntry);` |
|        - | 14975 | `	}` |
|       28 | 14976 | `	ph7_result_bool(pCtx,1);` |
|       28 | 14977 | `	return SXRET_OK;` |
|       19 | 14978 |  |
|        - | 14979 | `/*` |
|        - | 14980 | ` * bool spl_autoload_unregister(callable $callback)` |
|        - | 14981 | ` *  Unregister a given function as __autoload() implementation.` |
|        - | 14982 | ` * Parameters` |
|        - | 14983 | ` *  callback` |
|        - | 14984 | ` *   The autoload function being unregistered.` |
|        - | 14985 | ` * Return` |
|        - | 14986 | ` *  TRUE on success, FALSE on failure.` |
|        - | 14987 | ` */` |
|       32 | 14988 | `static int vm_builtin_spl_autoload_unregister(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        2 | 14989 |  |
|       34 | 14990 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 14991 | `	sxu32 n,nEntry;` |
|       34 | 14992 | `	if( nArg < 1 ){` |
|      ! 0 | 14993 | `		ph7_result_bool(pCtx,0);` |
|      ! 0 | 14994 | `		return SXRET_OK;` |
|        - | 14995 | `	}` |
|       34 | 14996 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       38 | 14997 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       36 | 14998 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       36 | 14999 | `		if( pEntry && PH7_MemObjCmp(&pEntry->sCallback,apArg[0],TRUE,0) == 0 ){` |
|        - | 15000 | `			/* Found — remove by shifting remaining entries down */` |
|       32 | 15001 | `			VmAutoloadCB *aBase = (VmAutoloadCB *)SySetBasePtr(&pVm->aAutoload);` |
|        - | 15002 | `			sxu32 i;` |
|       32 | 15003 | `			PH7_MemObjRelease(&pEntry->sCallback);` |
|       46 | 15004 | `			for( i = n ; i + 1 < nEntry ; i++ ){` |
|       16 | 15005 | `				SyMemcpy(&aBase[i+1],&aBase[i],sizeof(VmAutoloadCB));` |
|        9 | 15006 | `			}` |
|        - | 15007 | `			/* Pop the now-duplicate tail entry via the SySet API */` |
|       32 | 15008 | `			SySetPop(&pVm->aAutoload);` |
|       32 | 15009 | `			ph7_result_bool(pCtx,1);` |
|       32 | 15010 | `			return SXRET_OK;` |
|        - | 15011 | `		}` |
|        3 | 15012 | `	}` |
|        3 | 15013 | `	ph7_result_bool(pCtx,0);` |
|        3 | 15014 | `	return SXRET_OK;` |
|       18 | 15015 |  |
|        - | 15016 | `/*` |
|        - | 15017 | ` * array spl_autoload_functions(void)` |
|        - | 15018 | ` *  Return all registered __autoload() functions.` |
|        - | 15019 | ` * Return` |
|        - | 15020 | ` *  An array of all registered autoload functions. If no function is registered,` |
|        - | 15021 | ` *  an empty array is returned.` |
|        - | 15022 | ` */` |
|       20 | 15023 | `static int vm_builtin_spl_autoload_functions(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15024 |  |
|       21 | 15025 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 15026 | `	ph7_value *pArray;` |
|        - | 15027 | `	sxu32 n,nEntry;` |
|       10 | 15028 | `	SXUNUSED(nArg);` |
|       10 | 15029 | `	SXUNUSED(apArg);` |
|       21 | 15030 | `	pArray = ph7_context_new_array(pCtx);` |
|       21 | 15031 | `	if( pArray == 0 ){` |
|      ! 0 | 15032 | `		ph7_result_null(pCtx);` |
|      ! 0 | 15033 | `		return SXRET_OK;` |
|        - | 15034 | `	}` |
|       21 | 15035 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       35 | 15036 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       15 | 15037 | `		VmAutoloadCB *pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       15 | 15038 | `		if( pEntry ){` |
|       15 | 15039 | `			ph7_array_add_elem(pArray,0/* Automatic index */,&pEntry->sCallback);` |
|        7 | 15040 | `		}` |
|        8 | 15041 | `	}` |
|       21 | 15042 | `	ph7_result_value(pCtx,pArray);` |
|       21 | 15043 | `	return SXRET_OK;` |
|       11 | 15044 |  |
|        - | 15045 | `/*` |
|        - | 15046 | ` * void spl_autoload(string $class [, string $file_extensions = ".php,.inc" ])` |
|        - | 15047 | ` *  Default implementation of __autoload().` |
|        - | 15048 | ` *  Converts namespace separators to directory separators, lowercases the class` |
|        - | 15049 | ` *  name, and tries to include a file with each of the given extensions.` |
|        - | 15050 | ` * Parameters` |
|        - | 15051 | ` *  class` |
|        - | 15052 | ` *   The class name being searched.` |
|        - | 15053 | ` *  file_extensions` |
|        - | 15054 | ` *   Comma-separated list of file extensions to try.` |
|        - | 15055 | ` */` |
|        2 | 15056 | `static int vm_builtin_spl_autoload(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|        1 | 15057 |  |
|        - | 15058 | `	const char *zClass,*zExt,*zEnd,*zCur;` |
|        - | 15059 | `	SyBlob sPath;` |
|        - | 15060 | `	int nClass;` |
|        - | 15061 | `	sxi32 rc;` |
|        3 | 15062 | `	if( nArg < 1 ){` |
|      ! 0 | 15063 | `		return SXRET_OK;` |
|        - | 15064 | `	}` |
|        3 | 15065 | `	zClass = ph7_value_to_string(apArg[0],&nClass);` |
|        3 | 15066 | `	if( nClass < 1 ){` |
|      ! 0 | 15067 | `		return SXRET_OK;` |
|        - | 15068 | `	}` |
|        - | 15069 | `	/* Default extensions */` |
|        3 | 15070 | `	zExt = ".php,.inc";` |
|        3 | 15071 | `	if( nArg >= 2 ){` |
|        - | 15072 | `		int nExt;` |
|      ! 0 | 15073 | `		zExt = ph7_value_to_string(apArg[1],&nExt);` |
|      ! 0 | 15074 | `		if( nExt < 1 ){` |
|      ! 0 | 15075 | `			zExt = ".php,.inc";` |
|      ! 0 | 15076 | `		}` |
|      ! 0 | 15077 | `	}` |
|        3 | 15078 | `	SyBlobInit(&sPath,&pCtx->pVm->sAllocator);` |
|        - | 15079 | `	/* Iterate over comma-separated extensions */` |
|        3 | 15080 | `	zEnd = zExt + SyStrlen(zExt);` |
|        3 | 15081 | `	zCur = zExt;` |
|        7 | 15082 | `	while( zCur < zEnd ){` |
|        - | 15083 | `		const char *zComma;` |
|        - | 15084 | `		SyString sFile;` |
|        - | 15085 | `		int i;` |
|        - | 15086 | `		/* Find next comma or end */` |
|        5 | 15087 | `		zComma = zCur;` |
|       21 | 15088 | `		while( zComma < zEnd && *zComma != ',' ){` |
|       17 | 15089 | `			zComma++;` |
|        1 | 15090 | `		}` |
|        - | 15091 | `		/* Build path: lowercase class name with \ -> / , then append extension */` |
|        5 | 15092 | `		SyBlobReset(&sPath);` |
|       69 | 15093 | `		for( i = 0 ; i < nClass ; i++ ){` |
|       65 | 15094 | `			char c = zClass[i];` |
|       65 | 15095 | `			if( c == '\\' ){` |
|      ! 0 | 15096 | `				c = '/';` |
|       65 | 15097 | `			}else if( c >= 'A' && c <= 'Z' ){` |
|       13 | 15098 | `				c = c + ('a' - 'A');` |
|        6 | 15099 | `			}` |
|       65 | 15100 | `			SyBlobAppend(&sPath,(const void *)&c,1);` |
|       33 | 15101 | `		}` |
|        - | 15102 | `		/* Append extension */` |
|        5 | 15103 | `		SyBlobAppend(&sPath,(const void *)zCur,(sxu32)(zComma - zCur));` |
|        - | 15104 | `		/* Try to include the file */` |
|        5 | 15105 | `		SyStringInitFromBuf(&sFile,(const char *)SyBlobData(&sPath),SyBlobLength(&sPath));` |
|        5 | 15106 | `		rc = VmExecIncludedFile(pCtx,&sFile,FALSE);` |
|        5 | 15107 | `		if( rc == SXRET_OK ){` |
|        - | 15108 | `			/* File included successfully */` |
|      ! 0 | 15109 | `			SyBlobRelease(&sPath);` |
|      ! 0 | 15110 | `			return SXRET_OK;` |
|        - | 15111 | `		}` |
|        - | 15112 | `		/* Move past the comma */` |
|        5 | 15113 | `		zCur = zComma;` |
|        5 | 15114 | `		if( zCur < zEnd && *zCur == ',' ){` |
|        3 | 15115 | `			zCur++;` |
|        1 | 15116 | `		}` |
|        1 | 15117 | `	}` |
|        3 | 15118 | `	SyBlobRelease(&sPath);` |
|        3 | 15119 | `	return SXRET_OK;` |
|        2 | 15120 |  |
|        - | 15121 | `/* Table of built-in VM functions. */` |
|        - | 15122 | `static const ph7_builtin_func aVmFunc[] = {` |
|        - | 15123 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|        - | 15124 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|        - | 15125 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|        - | 15126 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|        - | 15127 | `	{ "function_exists", vm_builtin_func_exists   },` |
|        - | 15128 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|        - | 15129 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|        - | 15130 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|        - | 15131 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|        - | 15132 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|        - | 15133 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|        - | 15134 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|        - | 15135 | `	    /* Constants management */` |
|        - | 15136 | `	{ "defined",  vm_builtin_defined              },` |
|        - | 15137 | `	{ "define",   vm_builtin_define               },` |
|        - | 15138 | `	{ "constant", vm_builtin_constant             },` |
|        - | 15139 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|        - | 15140 | `	   /* Class/Object functions */` |
|        - | 15141 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|        - | 15142 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|        - | 15143 | `	{ "property_exists", vm_builtin_property_exists   },` |
|        - | 15144 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|        - | 15145 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|        - | 15146 | `	{ "get_class",       vm_builtin_get_class         },` |
|        - | 15147 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|        - | 15148 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|        - | 15149 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|        - | 15150 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|        - | 15151 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|        - | 15152 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|        - | 15153 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|        - | 15154 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|        - | 15155 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|        - | 15156 | `	{ "is_a", vm_builtin_is_a },` |
|        - | 15157 | `	   /* SPL Autoloading */` |
|        - | 15158 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|        - | 15159 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|        - | 15160 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|        - | 15161 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|        - | 15162 | `	   /* Random numbers/strings generators */` |
|        - | 15163 | `	{ "rand",          vm_builtin_rand            },` |
|        - | 15164 | `	{ "mt_rand",       vm_builtin_rand            },` |
|        - | 15165 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|        - | 15166 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|        - | 15167 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|        - | 15168 | `	{ "random_int",    vm_builtin_random_int      },` |
|        - | 15169 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|        - | 15170 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15171 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|        - | 15172 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|        - | 15173 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|        - | 15174 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15175 | `	   /* Language constructs functions */` |
|        - | 15176 | `	{ "echo",  vm_builtin_echo                    },` |
|        - | 15177 | `	{ "print", vm_builtin_print                   },` |
|        - | 15178 | `	{ "exit",  vm_builtin_exit                    },` |
|        - | 15179 | `	{ "die",   vm_builtin_exit                    },` |
|        - | 15180 | `	{ "eval",  vm_builtin_eval                    },` |
|        - | 15181 | `	  /* Variable handling functions */` |
|        - | 15182 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|        - | 15183 | `	{ "gettype",   vm_builtin_gettype              },` |
|        - | 15184 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|        - | 15185 | `	{ "isset",     vm_builtin_isset                },` |
|        - | 15186 | `	{ "unset",     vm_builtin_unset                },` |
|        - | 15187 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|        - | 15188 | `	{ "print_r",   vm_builtin_print_r              },` |
|        - | 15189 | `	{ "var_export",vm_builtin_var_export           },` |
|        - | 15190 | `	  /* Ouput control functions */` |
|        - | 15191 | `	{ "flush",        vm_builtin_ob_flush          },` |
|        - | 15192 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|        - | 15193 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|        - | 15194 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|        - | 15195 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|        - | 15196 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|        - | 15197 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|        - | 15198 | `	{ "ob_get_flush",    vm_builtin_ob_get_clean   },` |
|        - | 15199 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|        - | 15200 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|        - | 15201 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|        - | 15202 | `	{ "ob_get_level",      vm_builtin_ob_get_level },` |
|        - | 15203 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|        - | 15204 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|        - | 15205 | `	  /* Assertion functions */` |
|        - | 15206 | `	{ "assert_options",  vm_builtin_assert_options },` |
|        - | 15207 | `	{ "assert",          vm_builtin_assert         },` |
|        - | 15208 | `	  /* Error reporting functions */` |
|        - | 15209 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|        - | 15210 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|        - | 15211 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|        - | 15212 | `	{ "error_log",       vm_builtin_error_log      },` |
|        - | 15213 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|        - | 15214 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|        - | 15215 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|        - | 15216 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|        - | 15217 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|        - | 15218 | `	{ "error_get_last" ,  vm_builtin_debug_backtrace },` |
|        - | 15219 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|        - | 15220 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|        - | 15221 | `	  /* Release info */` |
|        - | 15222 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|        - | 15223 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|        - | 15224 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|        - | 15225 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|        - | 15226 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|        - | 15227 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|        - | 15228 | `	  /* hashmap */` |
|        - | 15229 | `	{"compact",          vm_builtin_compact       },` |
|        - | 15230 | `	{"extract",          vm_builtin_extract       },` |
|        - | 15231 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|        - | 15232 | `	  /* URL related function */` |
|        - | 15233 | `	{"parse_url",        vm_builtin_parse_url     },` |
|        - | 15234 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|        - | 15235 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|        - | 15236 | `	   /* XML processing functions */` |
|        - | 15237 | `	{"xml_parser_create",        vm_builtin_xml_parser_create   },` |
|        - | 15238 | `	{"xml_parser_create_ns",     vm_builtin_xml_parser_create_ns},` |
|        - | 15239 | `	{"xml_parser_free",          vm_builtin_xml_parser_free     },` |
|        - | 15240 | `	{"xml_set_element_handler",  vm_builtin_xml_set_element_handler},` |
|        - | 15241 | `	{"xml_set_character_data_handler", vm_builtin_xml_set_character_data_handler},` |
|        - | 15242 | `	{"xml_set_default_handler",  vm_builtin_xml_set_default_handler },` |
|        - | 15243 | `	{"xml_set_end_namespace_decl_handler", vm_builtin_xml_set_end_namespace_decl_handler},` |
|        - | 15244 | `	{"xml_set_start_namespace_decl_handler",vm_builtin_xml_set_start_namespace_decl_handler},` |
|        - | 15245 | `	{"xml_set_processing_instruction_handler",vm_builtin_xml_set_processing_instruction_handler},` |
|        - | 15246 | `	{"xml_set_unparsed_entity_decl_handler",vm_builtin_xml_set_unparsed_entity_decl_handler},` |
|        - | 15247 | `	{"xml_set_notation_decl_handler",vm_builtin_xml_set_notation_decl_handler},` |
|        - | 15248 | `	{"xml_set_external_entity_ref_handler",vm_builtin_xml_set_external_entity_ref_handler},` |
|        - | 15249 | `	{"xml_get_current_line_number",  vm_builtin_xml_get_current_line_number},` |
|        - | 15250 | `	{"xml_get_current_byte_index",   vm_builtin_xml_get_current_byte_index },` |
|        - | 15251 | `	{"xml_set_object",               vm_builtin_xml_set_object},` |
|        - | 15252 | `	{"xml_get_current_column_number",vm_builtin_xml_get_current_column_number},` |
|        - | 15253 | `	{"xml_get_error_code",           vm_builtin_xml_get_error_code },` |
|        - | 15254 | `	{"xml_parse",                    vm_builtin_xml_parse },` |
|        - | 15255 | `	{"xml_parser_set_option",        vm_builtin_xml_parser_set_option},` |
|        - | 15256 | `	{"xml_parser_get_option",        vm_builtin_xml_parser_get_option},` |
|        - | 15257 | `	{"xml_error_string",             vm_builtin_xml_error_string     },` |
|        - | 15258 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|        - | 15259 | `	   /* UTF-8 encoding/decoding */` |
|        - | 15260 | `	{"utf8_encode",    vm_builtin_utf8_encode},` |
|        - | 15261 | `	{"utf8_decode",    vm_builtin_utf8_decode},` |
|        - | 15262 | `	   /* Command line processing */` |
|        - | 15263 | `	{"getopt",         vm_builtin_getopt     },` |
|        - | 15264 | `	   /* JSON encoding/decoding */` |
|        - | 15265 | `	{"json_encode",    vm_builtin_json_encode },` |
|        - | 15266 | `	{"json_last_error",vm_builtin_json_last_error},` |
|        - | 15267 | `	{"json_decode",    vm_builtin_json_decode },` |
|        - | 15268 | `	{"serialize",      vm_builtin_json_encode },` |
|        - | 15269 | `	{"unserialize",    vm_builtin_json_decode },` |
|        - | 15270 | `	   /* Files/URI inclusion facility */` |
|        - | 15271 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|        - | 15272 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|        - | 15273 | `	{ "include",      vm_builtin_include          },` |
|        - | 15274 | `	{ "include_once", vm_builtin_include_once     },` |
|        - | 15275 | `	{ "require",      vm_builtin_require          },` |
|        - | 15276 | `	{ "require_once", vm_builtin_require_once     },` |
|        - | 15277 | `};` |
|        - | 15278 | `/*` |
|        - | 15279 | ` * Register the built-in VM functions defined above.` |
|        - | 15280 | ` */` |
|     2808 | 15281 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|        2 | 15282 |  |
|        - | 15283 | `	sxi32 rc;` |
|        - | 15284 | `	sxu32 n;` |
|   367850 | 15285 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|        - | 15286 | `		/* Note that these special functions have access` |
|        - | 15287 | `		 * to the underlying virtual machine as their` |
|        - | 15288 | `		 * private data.` |
|        - | 15289 | `		 */` |
|   365042 | 15290 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   365042 | 15291 | `		if( rc != SXRET_OK ){` |
|      ! 0 | 15292 | `			return rc;` |
|        - | 15293 | `		}` |
|   182522 | 15294 | `	}` |
|     2810 | 15295 | `	return SXRET_OK;` |
|     1406 | 15296 |  |
|        - | 15297 | `/*` |
|        - | 15298 | ` * Helper: Apply loadable filter to a class pointer.` |
|        - | 15299 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|        - | 15300 | ` * in the name collision chain, or NULL if none qualifies.` |
|        - | 15301 | ` */` |
|    96832 | 15302 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|        2 | 15303 |  |
|    96834 | 15304 | `	if( !iLoadable ){` |
|    94850 | 15305 | `		return pClass;` |
|        - | 15306 | `	}` |
|     1990 | 15307 | `	while(pClass){` |
|     1986 | 15308 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|     1982 | 15309 | `			return pClass;` |
|        - | 15310 | `		}` |
|        5 | 15311 | `		pClass = pClass->pNextName;` |
|        1 | 15312 | `	}` |
|        5 | 15313 | `	return 0;` |
|    48418 | 15314 |  |
|        - | 15315 | `/*` |
|        - | 15316 | ` * Trigger the autoload mechanism for a class that was not found.` |
|        - | 15317 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|        - | 15318 | ` * with the class name. After each callback, checks if the class is now` |
|        - | 15319 | ` * registered in the VM's class table.` |
|        - | 15320 | ` * Returns a pointer to the class on success, NULL on failure.` |
|        - | 15321 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|        - | 15322 | ` */` |
|       38 | 15323 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15324 |  |
|        - | 15325 | `	VmAutoloadCB *pEntry;` |
|        - | 15326 | `	ph7_value sArg,sResult;` |
|        - | 15327 | `	SyHashEntry *pHashEntry;` |
|        - | 15328 | `	ph7_class *pClass;` |
|        - | 15329 | `	sxu32 n,nEntry;` |
|       40 | 15330 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       40 | 15331 | `	if( nEntry < 1 ){` |
|       26 | 15332 | `		return 0;` |
|        - | 15333 | `	}` |
|        - | 15334 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       16 | 15335 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|        3 | 15336 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|        - | 15337 | `	}` |
|        - | 15338 | `	/* Mark this class as being autoloaded */` |
|       14 | 15339 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|        - | 15340 | `	/* Prepare the class name argument */` |
|       14 | 15341 | `	PH7_MemObjInit(pVm,&sArg);` |
|       14 | 15342 | `	PH7_MemObjInit(pVm,&sResult);` |
|       14 | 15343 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       14 | 15344 | `	pClass = 0;` |
|       28 | 15345 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        - | 15346 | `		ph7_value *apArg[1];` |
|       24 | 15347 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       24 | 15348 | `		if( pEntry == 0 ){` |
|      ! 0 | 15349 | `			continue;` |
|        - | 15350 | `		}` |
|       24 | 15351 | `		apArg[0] = &sArg;` |
|       24 | 15352 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|        - | 15353 | `			/* Callback could not be invoked — skip to next autoloader */` |
|      ! 0 | 15354 | `			continue;` |
|        - | 15355 | `		}` |
|        - | 15356 | `		/* Check if the class is now available */` |
|       24 | 15357 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       24 | 15358 | `		if( pHashEntry ){` |
|       10 | 15359 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       10 | 15360 | `			if( pClass ){` |
|       10 | 15361 | `				break;` |
|        - | 15362 | `			}` |
|      ! 0 | 15363 | `		}` |
|        9 | 15364 | `	}` |
|       14 | 15365 | `	PH7_MemObjRelease(&sArg);` |
|       14 | 15366 | `	PH7_MemObjRelease(&sResult);` |
|        - | 15367 | `	/* Remove reentrancy guard */` |
|       14 | 15368 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       14 | 15369 | `	return pClass;` |
|       21 | 15370 |  |
|        - | 15371 | `/*` |
|        - | 15372 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|        - | 15373 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|        - | 15374 | ` */` |
|       18 | 15375 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|        2 | 15376 |  |
|       20 | 15377 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        2 | 15378 |  |
|        - | 15379 | `/*` |
|        - | 15380 | ` * Check if the given name refer to an installed class.` |
|        - | 15381 | ` * Return a pointer to that class on success. NULL on failure.` |
|        - | 15382 | ` */` |
|    96844 | 15383 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|        - | 15384 | `	ph7_vm *pVm,        /* Target VM */` |
|        - | 15385 | `	const char *zName,  /* Name of the target class */` |
|        - | 15386 | `	sxu32 nByte,        /* zName length */` |
|        - | 15387 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|        - | 15388 | `						 * [i.e: no abstract classes or interfaces]` |
|        - | 15389 | `						 */` |
|        - | 15390 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|        - | 15391 | `	)` |
|        2 | 15392 |  |
|        - | 15393 | `	SyHashEntry *pEntry;` |
|        - | 15394 | `	ph7_class *pClass;` |
|    48422 | 15395 | `	SXUNUSED(iNest);` |
|        - | 15396 | `	/* Exact class lookup.` |
|        - | 15397 | `	 * Static names are already namespace-qualified by the compiler.` |
|        - | 15398 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */` |
|    96846 | 15399 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|    96846 | 15400 | `	if( pEntry == 0 ){` |
|        - | 15401 | `		/* Class not found in hash table — try autoload before giving up */` |
|       22 | 15402 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|        - | 15403 | `	}` |
|    96826 | 15404 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|    96826 | 15405 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|    48424 | 15406 |  |
|        - | 15407 | `/*` |
|        - | 15408 | ` * Reference Table Implementation` |
|        - | 15409 | ` * Status: stable <chm@symisc.net>` |
|        - | 15410 | ` * Intro` |
|        - | 15411 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|        - | 15412 | ` *  differ greatly from the one used by the zend engine. That is,` |
|        - | 15413 | ` *  the reference implementation is consistent,solid and it's` |
|        - | 15414 | ` *  behavior resemble the C++ reference mechanism.` |
|        - | 15415 | ` *  Refer to the official for more information on this powerful` |
|        - | 15416 | ` *  extension.` |
|        - | 15417 | ` */` |
|        - | 15418 | `/*` |
|        - | 15419 | ` * Allocate a new reference entry.` |
|        - | 15420 | ` */` |
|  3169878 | 15421 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|        2 | 15422 |  |
|        - | 15423 | `	VmRefObj *pRef;` |
|        - | 15424 | `	/* Allocate a new instance */` |
|  3169880 | 15425 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  3169880 | 15426 | `	if( pRef == 0 ){` |
|      ! 0 | 15427 | `		return 0;` |
|        - | 15428 | `	}` |
|        - | 15429 | `	/* Zero the structure */` |
|  3169880 | 15430 | `	SyZero(pRef,sizeof(VmRefObj));` |
|        - | 15431 | `	/* Initialize fields */` |
|  3169880 | 15432 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  3169880 | 15433 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  3169880 | 15434 | `	pRef->nIdx = nIdx;` |
|  3169880 | 15435 | `	return pRef;` |
|  1584941 | 15436 |  |
|        - | 15437 | `/*` |
|        - | 15438 | ` * Default hash function used by the reference table` |
|        - | 15439 | ` * for lookup/insertion operations.` |
|        - | 15440 | ` */` |
| 17401030 | 15441 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|        2 | 15442 |  |
|        - | 15443 | `	/* Calculate the hash based on the memory object index */` |
| 17401032 | 15444 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|        2 | 15445 |  |
|        - | 15446 | `/*` |
|        - | 15447 | ` * Check if a memory object [i.e: a variable] is already installed` |
|        - | 15448 | ` * in the reference table.` |
|        - | 15449 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|        - | 15450 | ` * otherwise.` |
|        - | 15451 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15452 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15453 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15454 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15455 | ` * Refer to the official for more information on this powerful` |
|        - | 15456 | ` * extension.` |
|        - | 15457 | ` */` |
|  9450920 | 15458 | `static VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|        2 | 15459 |  |
|        - | 15460 | `	VmRefObj *pRef;` |
|        - | 15461 | `	sxu32 nBucket;` |
|        - | 15462 | `	/* Point to the appropriate bucket */` |
|  9450922 | 15463 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|        - | 15464 | `	/* Perform the lookup */` |
|  9450922 | 15465 | `	pRef = pVm->apRefObj[nBucket];` |
| 20616777 | 15466 | `	for(;;){` |
| 41228000 | 15467 | `		if( pRef == 0 ){` |
|  3273144 | 15468 | `			break;` |
|        - | 15469 | `		}` |
| 37954858 | 15470 | `		if( pRef->nIdx == nObjIdx ){` |
|        - | 15471 | `			/* Entry found */` |
|  6177780 | 15472 | `			return pRef;` |
|        - | 15473 | `		}` |
|        - | 15474 | `		/* Point to the next entry */` |
| 31777080 | 15475 | `		pRef = pRef->pNextCollide;` |
|        2 | 15476 | `	}` |
|        - | 15477 | `	/* No such entry,return NULL */` |
|  3273144 | 15478 | `	return 0;` |
|  4725462 | 15479 |  |
|        - | 15480 | `/*` |
|        - | 15481 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15482 | ` *` |
|        - | 15483 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15484 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15485 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15486 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15487 | ` * Refer to the official for more information on this powerful` |
|        - | 15488 | ` * extension.` |
|        - | 15489 | ` */` |
|  3169878 | 15490 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15491 |  |
|        - | 15492 | `	sxu32 nBucket;` |
|  3169880 | 15493 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|        - | 15494 | `		VmRefObj **apNew;` |
|        - | 15495 | `		sxu32 nNew;` |
|        - | 15496 | `		/* Allocate a larger table */` |
|     4456 | 15497 | `		nNew = pVm->nRefSize << 1;` |
|     4456 | 15498 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     4456 | 15499 | `		if( apNew ){` |
|     4456 | 15500 | `			VmRefObj *pEntry = pVm->pRefList;` |
|        - | 15501 | `			sxu32 n;` |
|        - | 15502 | `			/* Zero the structure */` |
|     4456 | 15503 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|        - | 15504 | `			/* Rehash all referenced entries */` |
|  2847842 | 15505 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|        - | 15506 | `				/* Remove old collision links */` |
|  2843388 | 15507 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|        - | 15508 | `				/* Point to the appropriate bucket */` |
|  2843388 | 15509 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|        - | 15510 | `				/* Insert the entry  */` |
|  2843388 | 15511 | `				pEntry->pNextCollide = apNew[nBucket];` |
|  2843388 | 15512 | `				if( apNew[nBucket] ){` |
|  2301116 | 15513 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|  1150557 | 15514 | `				}` |
|  2843388 | 15515 | `				apNew[nBucket] = pEntry;` |
|        - | 15516 | `				/* Point to the next entry */` |
|  2843388 | 15517 | `				pEntry = pEntry->pNext;` |
|  1421695 | 15518 | `			}` |
|        - | 15519 | `			/* Release the old table */` |
|     4456 | 15520 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|        - | 15521 | `			/* Install the new one */` |
|     4456 | 15522 | `			pVm->apRefObj = apNew;` |
|     4456 | 15523 | `			pVm->nRefSize = nNew;` |
|     2227 | 15524 | `		}` |
|     2227 | 15525 | `	}` |
|        - | 15526 | `	/* Point to the appropriate bucket */` |
|  3169880 | 15527 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|        - | 15528 | `	/* Insert the entry */` |
|  3169880 | 15529 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  3169880 | 15530 | `	if( pVm->apRefObj[nBucket] ){` |
|  2590266 | 15531 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|  1295077 | 15532 | `	}` |
|  3169880 | 15533 | `	pVm->apRefObj[nBucket] = pRef;` |
|  3169880 | 15534 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  3169880 | 15535 | `	pVm->nRefUsed++;` |
|  3169880 | 15536 | `	return SXRET_OK;` |
|        2 | 15537 |  |
|        - | 15538 | `/*` |
|        - | 15539 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|        - | 15540 | ` * the reference table.` |
|        - | 15541 | ` * This function is invoked when the user perform an unset` |
|        - | 15542 | ` * call [i.e: unset($var); ].` |
|        - | 15543 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15544 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15545 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15546 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15547 | ` * Refer to the official for more information on this powerful` |
|        - | 15548 | ` * extension.` |
|        - | 15549 | ` */` |
|  3129012 | 15550 | `static sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|        2 | 15551 |  |
|        - | 15552 | `	ph7_hashmap_node **apNode;` |
|        - | 15553 | `	SyHashEntry **apEntry;` |
|        - | 15554 | `	sxu32 n;` |
|        - | 15555 | `	/* Point to the reference table */` |
|  3129014 | 15556 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  3129014 | 15557 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|        - | 15558 | `	/* Unlink the entry from the reference table */` |
|  3238080 | 15559 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   109068 | 15560 | `		if( apEntry[n] ){` |
|   109018 | 15561 | `			SyHashDeleteEntry2(apEntry[n]);` |
|    54508 | 15562 | `		}` |
|    54535 | 15563 | `	}` |
|  6149258 | 15564 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|  3020246 | 15565 | `		if( apNode[n] ){` |
|     6738 | 15566 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|     3368 | 15567 | `		}` |
|  1510124 | 15568 | `	}` |
|  3129014 | 15569 | `	if( pRef->pPrevCollide ){` |
|  1192168 | 15570 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|   596213 | 15571 | `	}else{` |
|  1936848 | 15572 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|        - | 15573 | `	}` |
|  3129014 | 15574 | `	if( pRef->pNextCollide ){` |
|  1777428 | 15575 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   888709 | 15576 | `	}` |
|  3129014 | 15577 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|        - | 15578 | `	/* Release the node */` |
|  3129014 | 15579 | `	SySetRelease(&pRef->aReference);` |
|  3129014 | 15580 | `	SySetRelease(&pRef->aArrEntries);` |
|  3129014 | 15581 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  3129014 | 15582 | `	pVm->nRefUsed--;` |
|  3129014 | 15583 | `	return SXRET_OK;` |
|        2 | 15584 |  |
|        - | 15585 | `/*` |
|        - | 15586 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|        - | 15587 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15588 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15589 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15590 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15591 | ` * Refer to the official for more information on this powerful` |
|        - | 15592 | ` * extension.` |
|        - | 15593 | ` */` |
|  3205070 | 15594 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|        - | 15595 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15596 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15597 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15598 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|        - | 15599 | `	sxi32 iFlags                 /* Control flags */` |
|        - | 15600 | `	)` |
|        2 | 15601 |  |
|  3205072 | 15602 | `	VmFrame *pFrame = pVm->pFrame;` |
|        - | 15603 | `	VmRefObj *pRef;` |
|        - | 15604 | `	/* Check if the referenced object already exists */` |
|  3205072 | 15605 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3205072 | 15606 | `	if( pRef == 0 ){` |
|        - | 15607 | `		/* Create a new entry */` |
|  3169880 | 15608 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  3169880 | 15609 | `		if( pRef == 0 ){` |
|      ! 0 | 15610 | `			return SXERR_MEM;` |
|        - | 15611 | `		}` |
|  3169880 | 15612 | `		pRef->iFlags = iFlags;` |
|        - | 15613 | `		/* Install the entry */` |
|  3169880 | 15614 | `		VmRefObjInsert(&(*pVm),pRef);` |
|  1584939 | 15615 | `	}` |
|  3205072 | 15616 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  3205072 | 15617 | `	if( pFrame->pParent != 0 && pEntry ){` |
|        - | 15618 | `		VmSlot sRef;` |
|        - | 15619 | `		/* Local frame,record referenced entry so that it can` |
|        - | 15620 | `		 * be deleted when we leave this frame.` |
|        - | 15621 | `		 */` |
|   103362 | 15622 | `		sRef.nIdx = nIdx;` |
|   103362 | 15623 | `		sRef.pUserData = pEntry;` |
|   103362 | 15624 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|      ! 0 | 15625 | `			pEntry = 0; /* Do not record this entry */` |
|      ! 0 | 15626 | `		}` |
|    51680 | 15627 | `	}` |
|  3205072 | 15628 | `	if( pEntry ){` |
|        - | 15629 | `		/* Address of the hash-entry */` |
|   138354 | 15630 | `		SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|    69176 | 15631 | `	}` |
|  3205072 | 15632 | `	if( pMapEntry ){` |
|        - | 15633 | `		/* Address of the hashmap node [i.e: Array entry] */` |
|  3058740 | 15634 | `		SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|  1529369 | 15635 | `	}` |
|  3205072 | 15636 | `	return SXRET_OK;` |
|  1602537 | 15637 |  |
|        - | 15638 | `/*` |
|        - | 15639 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|        - | 15640 | ` * The implementation of the reference mechanism in the PH7 engine` |
|        - | 15641 | ` * differ greatly from the one used by the zend engine. That is,` |
|        - | 15642 | ` * the reference implementation is consistent,solid and it's` |
|        - | 15643 | ` * behavior resemble the C++ reference mechanism.` |
|        - | 15644 | ` * Refer to the official for more information on this powerful` |
|        - | 15645 | ` * extension.` |
|        - | 15646 | ` */` |
|  3116832 | 15647 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|        - | 15648 | `	ph7_vm *pVm,                 /* Target VM */` |
|        - | 15649 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|        - | 15650 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|        - | 15651 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|        - | 15652 | `	)` |
|        2 | 15653 |  |
|        - | 15654 | `	VmRefObj *pRef;` |
|        - | 15655 | `	sxu32 n;` |
|        - | 15656 | `	/* Check if the referenced object already exists */` |
|  3116834 | 15657 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  3116834 | 15658 | `	if( pRef == 0 ){` |
|        - | 15659 | `		/* Not such entry */` |
|   103260 | 15660 | `		return SXERR_NOTFOUND;` |
|        - | 15661 | `	}` |
|        - | 15662 | `	/* Remove the desired entry */` |
|  3013576 | 15663 | `	if( pEntry ){` |
|        - | 15664 | `		SyHashEntry **apEntry;` |
|       62 | 15665 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|      228 | 15666 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|      168 | 15667 | `			if( apEntry[n] == pEntry ){` |
|        - | 15668 | `				/* Nullify the entry */` |
|       62 | 15669 | `				apEntry[n] = 0;` |
|        - | 15670 | `				/*` |
|        - | 15671 | `				 * NOTE:` |
|        - | 15672 | `				 * In future releases,think to add a free pool of entries,so that` |
|        - | 15673 | `				 * we avoid wasting spaces.` |
|        - | 15674 | `				 */` |
|       30 | 15675 | `			}` |
|       85 | 15676 | `		}` |
|       30 | 15677 | `	}` |
|  3013576 | 15678 | `	if( pMapEntry ){` |
|        - | 15679 | `		ph7_hashmap_node **apNode;` |
|  3013516 | 15680 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  6027124 | 15681 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|  3013610 | 15682 | `			if( apNode[n] == pMapEntry ){` |
|        - | 15683 | `				/* nullify the entry */` |
|  3013516 | 15684 | `				apNode[n] = 0;` |
|  1506757 | 15685 | `			}` |
|  1506806 | 15686 | `		}` |
|  1506757 | 15687 | `	}` |
|  3013576 | 15688 | `	return SXRET_OK;` |
|  1558418 | 15689 |  |
|        - | 15690 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|        - | 15691 | `/*` |
|        - | 15692 | ` * Extract the IO stream device associated with a given scheme.` |
|        - | 15693 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|        - | 15694 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|        - | 15695 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|        - | 15696 | ` * For more information on how to register IO stream devices,please` |
|        - | 15697 | ` * refer to the official documentation.` |
|        - | 15698 | ` */` |
|    28506 | 15699 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|        - | 15700 | `	ph7_vm *pVm,           /* Target VM */` |
|        - | 15701 | `	const char **pzDevice, /* Full path,URI,... */` |
|        - | 15702 | `	int nByte              /* *pzDevice length*/` |
|        - | 15703 | `	)` |
|        2 | 15704 |  |
|        - | 15705 | `	const char *zIn,*zEnd,*zCur,*zNext;` |
|        - | 15706 | `	ph7_io_stream **apStream,*pStream;` |
|        - | 15707 | `	SyString sDev,sCur;` |
|        - | 15708 | `	sxu32 n,nEntry;` |
|        - | 15709 | `	int rc;` |
|        - | 15710 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    28508 | 15711 | `	zNext = zCur = zIn = *pzDevice;` |
|    28508 | 15712 | `	zEnd = &zIn[nByte];` |
|  1818442 | 15713 | `	while( zIn < zEnd ){` |
|  1789938 | 15714 | `		if( zIn < &zEnd[-3]/*://*/ && zIn[0] == ':' && zIn[1] == '/' && zIn[2] == '/' ){` |
|        - | 15715 | `			/* Got one */` |
|        3 | 15716 | `			zNext = &zIn[sizeof("://")-1];` |
|        3 | 15717 | `			break;` |
|        - | 15718 | `		}` |
|        - | 15719 | `		/* Advance the cursor */` |
|  1789936 | 15720 | `		zIn++;` |
|        2 | 15721 | `	}` |
|    28508 | 15722 | `	if( zIn >= zEnd ){` |
|        - | 15723 | `		/* No such scheme,return the default stream */` |
|    28506 | 15724 | `		return pVm->pDefStream;` |
|        - | 15725 | `	}` |
|        3 | 15726 | `	SyStringInitFromBuf(&sDev,zCur,zIn-zCur);` |
|        - | 15727 | `	/* Remove leading and trailing white spaces */` |
|        3 | 15728 | `	SyStringFullTrim(&sDev);` |
|        - | 15729 | `	/* Perform a linear lookup on the installed stream devices */` |
|        3 | 15730 | `	apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        3 | 15731 | `	nEntry = SySetUsed(&pVm->aIOstream);` |
|        3 | 15732 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        3 | 15733 | `		pStream = apStream[n];` |
|        3 | 15734 | `		SyStringInitFromBuf(&sCur,pStream->zName,SyStrlen(pStream->zName));` |
|        - | 15735 | `		/* Perfrom a case-insensitive comparison */` |
|        3 | 15736 | `		rc = SyStringCmp(&sDev,&sCur,SyStrnicmp);` |
|        3 | 15737 | `		if( rc == 0 ){` |
|        - | 15738 | `			/* Stream device found */` |
|        3 | 15739 | `			*pzDevice = zNext;` |
|        3 | 15740 | `			return pStream;` |
|        - | 15741 | `		}` |
|      ! 0 | 15742 | `	}` |
|        - | 15743 | `	/* No such stream,return NULL */` |
|      ! 0 | 15744 | `	return 0;` |
|    14255 | 15745 |  |
|        - | 15746 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|        - | 15747 | `/* HTTP/URI routines moved to vm_http.c */` |
|        - | 15748 |  |
