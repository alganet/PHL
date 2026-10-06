# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5025/5730 lines (87.70%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits |  Line | Source |
| --------: | ----: | :--- |
|         - |     1 | `/**` |
|         - |     2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |     3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |     4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |     5 | ` */` |
|         - |     6 | `#include "ph7int.h"` |
|         - |     7 | `#include <stddef.h>` |
|         - |     8 | `#include <stdlib.h>` |
|         - |     9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |    10 | `#include <math.h>` |
|         - |    11 | `#endif` |
|         - |    12 | `/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared` |
|         - |    13 | ` * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in` |
|         - |    14 | ` * memobj.c). The executor uses them to promote an overflowing integer` |
|         - |    15 | ` * operation to a float, matching PHP. */` |
|         - |    16 | `/*` |
|         - |    17 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|         - |    18 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|         - |    19 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|         - |    20 | ` * statements.` |
|         - |    21 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|         - |    22 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|         - |    23 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|         - |    24 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|         - |    25 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|         - |    26 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|         - |    27 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|         - |    28 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|         - |    29 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|         - |    30 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|         - |    31 | ` * and so on.` |
|         - |    32 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|         - |    33 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|         - |    34 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|         - |    35 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|         - |    36 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|         - |    37 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|         - |    38 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|         - |    39 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|         - |    40 | ` */` |
|         - |    41 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|         - |    42 | `/*` |
|         - |    43 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|         - |    44 | ` * memory object index is stored in an instance of the following structure and put` |
|         - |    45 | ` * in the free object table so that it can be reused again without allocating` |
|         - |    46 | ` * a new memory object.` |
|         - |    47 | ` */` |
|         - |    48 | `/* VmSlot struct moved to ph7int.h */` |
|         - |    49 | `/*` |
|         - |    50 | ` * An entry in the reference table is represented by an instance of the` |
|         - |    51 | ` * follwoing table.` |
|         - |    52 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - |    53 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - |    54 | ` * the reference implementation is consistent,solid and it's` |
|         - |    55 | ` * behavior resemble the C++ reference mechanism.` |
|         - |    56 | ` * Refer to the official for more information on this powerful` |
|         - |    57 | ` * extension.` |
|         - |    58 | ` */` |
|         - |    59 | `/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */` |
|         - |    60 | `/*` |
|         - |    61 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|         - |    62 | ` * is stored in an instance of the following structure.` |
|         - |    63 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|         - |    64 | ` */` |
|         - |    65 | `/* VmShutdownCB struct moved to ph7int.h */` |
|         - |    66 | `/*` |
|         - |    67 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|         - |    68 | ` * is stored in an instance of the following structure.` |
|         - |    69 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|         - |    70 | ` */` |
|         - |    71 | `/* VmAutoloadCB struct moved to ph7int.h */` |
|         - |    72 |  |
|         - |    73 | `/*` |
|         - |    74 | ` * TRUE when php compares these two operands as UNORDERED -- a NaN against` |
|         - |    75 | ` * something php reads as a NUMBER or as a STRING. php answers 1 for that` |
|         - |    76 | `` * comparison in BOTH directions, which is what makes `==`, `<`, `>`, `<=` and`` |
|         - |    77 | `` * `>=` all false at once while `<=>` is 1 either way round.`` |
|         - |    78 | ` *` |
|         - |    79 | ` * Two rules ride on this predicate and both were wrong without them.` |
|         - |    80 | ` *` |
|         - |    81 | ` * It must be asked BEFORE PH7_MemObjCmp runs: the comparator converts its` |
|         - |    82 | ` * operands IN PLACE, so a NaN that took the string path is a MEMOBJ_STRING by` |
|         - |    83 | ` * the time the answer comes back and the float is gone. That is how` |
|         - |    84 | `` * `NAN == "NAN"` was TRUE here (php: false) and `NAN < "abc"` was TRUE`` |
|         - |    85 | ` * (php: false) -- the screen ran on two strings and saw no NaN at all.` |
|         - |    86 | ` *` |
|         - |    87 | ` * And php's own precedence comes FIRST: a comparison against null, a bool, an` |
|         - |    88 | ``  * array or an object never reaches the numeric/string rule, so `NAN == true` `` |
|         - |    89 | `` * is TRUE (both truthy) and `NAN < []` is TRUE (an array is greater). Those`` |
|         - |    90 | ` * flags are exactly the branches PH7_MemObjCmp answers ahead of its numeric` |
|         - |    91 | ` * one. A RESOURCE is not among them: php reads it as its ID there, so a NaN` |
|         - |    92 | ` * against one is as unordered as a NaN against any other number.` |
|         - |    93 | ` */` |
|  76839764 |    94 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |    95 | `{` |
|  76839764 |    96 | `	if( (pLeft->iFlags \| pRight->iFlags)` |
|  76839769 |    97 | `	  & (MEMOBJ_NULL\|MEMOBJ_BOOL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ) ){` |
|       ! 0 |    98 | `		return FALSE;` |
|         - |    99 | `	}` |
|  76839769 |   100 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       382 |   101 | `		return TRUE;` |
|         - |   102 | `	}` |
|  76839389 |   103 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       174 |   104 | `		return TRUE;` |
|         - |   105 | `	}` |
|  76839217 |   106 | `	return FALSE;` |
|  38429134 |   107 | `}` |
|         - |   108 | `/*` |
|         - |   109 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |   110 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |   111 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |   112 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |   113 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |   114 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |   115 | ` * still go through the existing numeric coercion.` |
|         - |   116 | ` */` |
|  12862804 |   117 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |   118 | `{` |
|         - |   119 | `	SyString sStr;` |
|  12862809 |   120 | `	sxu8 bReal = FALSE;` |
|  12862809 |   121 | `	const char *zTail = 0;` |
|         - |   122 | `	const char *zEnd;` |
|  12862809 |   123 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|  12862791 |   124 | `		return FALSE;` |
|         - |   125 | `	}` |
|        21 |   126 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        21 |   127 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |   128 | `		return TRUE;` |
|         - |   129 | `	}` |
|        21 |   130 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |   131 | `		return TRUE;` |
|         - |   132 | `	}` |
|         - |   133 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |   134 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |   135 | `	 * still takes the Perl path. */` |
|        17 |   136 | `	zEnd = sStr.zString + sStr.nByte;` |
|        17 |   137 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |   138 | `		zTail++;` |
|       ! 0 |   139 | `	}` |
|        17 |   140 | `	return zTail < zEnd;` |
|   6432486 |   141 | `}` |
|         - |   142 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|         - |   143 | `/* Constant expander used by define(); used below to recognise user-defined` |
|         - |   144 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|         - |   145 | ` * a define() overwrites them. */` |
|         - |   146 | `/*` |
|         - |   147 | ` * Register a constant and it's associated expansion callback so that` |
|         - |   148 | ` * it can be expanded from the target PHP program.` |
|         - |   149 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|         - |   150 | ` * simple and work as follows:` |
|         - |   151 | ` * Each registered constant have a C procedure associated with it.` |
|         - |   152 | ` * This procedure known as the constant expansion callback is responsible` |
|         - |   153 | ` * of expanding the invoked constant to the desired value,for example:` |
|         - |   154 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|         - |   155 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|         - |   156 | ` * (Windows,Linux,...) and so on.` |
|         - |   157 | ` * Please refer to the official documentation for additional information.` |
|         - |   158 | ` */` |
|         - |   159 | `/*` |
|         - |   160 | ` * php compares a constant name's NAMESPACE part without regard to case and its` |
|         - |   161 | `` * final segment byte for byte: with `Aa\Bb\DEE` defined, `aa\BB\DEE` answers`` |
|         - |   162 | `` * and `Aa\Bb\dee` does not. A LOOKUP also drops one leading backslash, so`` |
|         - |   163 | `` * `\Aa\Bb\DEE` answers -- while define() keeps whatever it was handed, which is`` |
|         - |   164 | `` * why the constant `define('\X\Y')` makes can never be named again.`` |
|         - |   165 | ` *` |
|         - |   166 | ` * hConstant is a plain byte hash, so a name is folded to that canonical key` |
|         - |   167 | ` * before it reaches the table -- everything up to and including the LAST` |
|         - |   168 | ` * backslash lowercased, the rest untouched -- at the insert door as well as at` |
|         - |   169 | ` * every read door, or the two sides would stop meeting. The declared spelling` |
|         - |   170 | ` * survives in ph7_constant.sName, which is what get_defined_constants() and` |
|         - |   171 | ` * Reflection report.` |
|         - |   172 | ` *` |
|         - |   173 | ` * Length of the leading run a name folds: past its last backslash, or 0 when it` |
|         - |   174 | ` * has none -- in which case the name IS its own key and nothing is copied.` |
|         - |   175 | ` */` |
|  13821131 |   176 | `static sxu32 VmConstantFoldLen(const char *zName,sxu32 nName)` |
|         5 |   177 | `{` |
|  13821136 |   178 | `	sxu32 n = nName;` |
| 247207695 |   179 | `	while( n > 0 && zName[n-1] != '\\' ){` |
| 233386564 |   180 | `		--n;` |
|         5 |   181 | `	}` |
|  13821136 |   182 | `	return n;` |
|         5 |   183 | `}` |
|         - |   184 | `/*` |
|         - |   185 | ` * Write zName's canonical key into zOut (nName bytes): ASCII-lowercase the` |
|         - |   186 | ` * first nFold, copy the rest. php folds with the ASCII table only.` |
|         - |   187 | ` */` |
|    112842 |   188 | `static void VmConstantFoldKey(char *zOut,const char *zName,sxu32 nName,sxu32 nFold)` |
|         5 |   189 | `{` |
|         - |   190 | `	sxu32 i;` |
|    564841 |   191 | `	for( i = 0 ; i < nFold ; ++i ){` |
|    451999 |   192 | `		int c = zName[i];` |
|    451999 |   193 | `		zOut[i] = (char)( (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c );` |
|    225650 |   194 | `	}` |
|   2082993 |   195 | `	for( ; i < nName ; ++i ){` |
|   1970151 |   196 | `		zOut[i] = zName[i];` |
|    983538 |   197 | `	}` |
|    112847 |   198 | `}` |
|         - |   199 | `/*` |
|         - |   200 | ` * The hConstant entry a name resolves to under that rule, or NULL. Every read` |
|         - |   201 | ` * door goes through here. bStripLead drops one leading backslash, which is what` |
|         - |   202 | ` * a lookup does and an insert does not.` |
|         - |   203 | ` */` |
|    137573 |   204 | `PH7_PRIVATE SyHashEntry * PH7_VmConstantFetch(ph7_vm *pVm,const char *zName,sxu32 nName,int bStripLead)` |
|         5 |   205 | `{` |
|         - |   206 | `	char zStack[128];` |
|         - |   207 | `	char *zKey;` |
|         - |   208 | `	sxu32 nFold;` |
|         - |   209 | `	SyHashEntry *pEntry;` |
|    137578 |   210 | `	if( bStripLead && nName > 0 && zName[0] == '\\' ){` |
|        11 |   211 | `		zName++;` |
|        11 |   212 | `		nName--;` |
|         5 |   213 | `	}` |
|    137578 |   214 | `	if( nName < 1 ){` |
|       ! 0 |   215 | `		return 0;` |
|         - |   216 | `	}` |
|    137578 |   217 | `	nFold = VmConstantFoldLen(zName,nName);` |
|    137578 |   218 | `	if( nFold < 1 ){` |
|         - |   219 | `		/* No namespace part: the common case, matched byte for byte. */` |
|    136570 |   220 | `		return SyHashGet(&pVm->hConstant,(const void *)zName,nName);` |
|         - |   221 | `	}` |
|      1013 |   222 | `	zKey = zStack;` |
|      1013 |   223 | `	if( nName > (sxu32)sizeof(zStack) ){` |
|       ! 0 |   224 | `		zKey = (char *)SyMemBackendAlloc(&pVm->sAllocator,nName);` |
|       ! 0 |   225 | `		if( zKey == 0 ){` |
|       ! 0 |   226 | `			return 0;` |
|         - |   227 | `		}` |
|       ! 0 |   228 | `	}` |
|      1013 |   229 | `	VmConstantFoldKey(zKey,zName,nName,nFold);` |
|      1013 |   230 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)zKey,nName);` |
|      1013 |   231 | `	if( zKey != zStack ){` |
|       ! 0 |   232 | `		SyMemBackendFree(&pVm->sAllocator,zKey);` |
|       ! 0 |   233 | `	}` |
|      1013 |   234 | `	return pEntry;` |
|     67934 |   235 | `}` |
|  13683132 |   236 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |   237 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |   238 | `	const SyString *pName,  /* Constant name */` |
|         - |   239 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |   240 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |   241 | `	)` |
|         5 |   242 | `{` |
|  13683137 |   243 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |   244 | `}` |
|         - |   245 | `/*` |
|         - |   246 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |   247 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |   248 | ` */` |
|  13683558 |   249 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|         - |   250 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |   251 | `	const SyString *pName,  /* Constant name */` |
|         - |   252 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |   253 | `	void *pUserData,        /* Last argument to xExpand() */` |
|         - |   254 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|         - |   255 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|         - |   256 | `	int bUser               /* 1 when defined by user code */` |
|         - |   257 | `	)` |
|         5 |   258 | `{` |
|         - |   259 | `	ph7_constant *pCons;` |
|         - |   260 | `	SyHashEntry *pEntry;` |
|         - |   261 | `	char *zDupName;` |
|  13683563 |   262 | `	char *zKeyBuf = 0;   /* folded key, when the name has a namespace part */` |
|         - |   263 | `	sxi32 rc;` |
|         - |   264 | ``	/* Install under the canonical key, so a later `aa\bb\NAME` finds what`` |
|         - |   265 | ``	 * `Aa\Bb\NAME` declared -- and so php's refusal to redefine it fires. */`` |
|         - |   266 | `	{` |
|  13683563 |   267 | `		sxu32 nFold = VmConstantFoldLen(pName->zString,pName->nByte);` |
|  13683563 |   268 | `		if( nFold > 0 ){` |
|    111839 |   269 | `			zKeyBuf = (char *)SyMemBackendAlloc(&pVm->sAllocator,pName->nByte);` |
|    111839 |   270 | `			if( zKeyBuf == 0 ){` |
|       ! 0 |   271 | `				return 0;` |
|         - |   272 | `			}` |
|    111839 |   273 | `			VmConstantFoldKey(zKeyBuf,pName->zString,pName->nByte,nFold);` |
|     55829 |   274 | `		}` |
|         - |   275 | `	}` |
|  20317288 |   276 | `	pEntry = SyHashGet(&pVm->hConstant,` |
|  13683558 |   277 | `		(const void *)(zKeyBuf ? zKeyBuf : pName->zString),pName->nByte);` |
|  13683563 |   278 | `	if( pEntry ){` |
|         - |   279 | `		/* Overwrite the old definition and return immediately */` |
|       ! 0 |   280 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |   281 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|         - |   282 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|         - |   283 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|       ! 0 |   284 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|       ! 0 |   285 | `		 && pCons->pUserData != pUserData ){` |
|       ! 0 |   286 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|       ! 0 |   287 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|       ! 0 |   288 | `		}` |
|       ! 0 |   289 | `		pCons->xExpand = xExpand;` |
|       ! 0 |   290 | `		pCons->pUserData = pUserData;` |
|       ! 0 |   291 | `		if( pFile ){` |
|       ! 0 |   292 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|       ! 0 |   293 | `		}else{` |
|       ! 0 |   294 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|         - |   295 | `		}` |
|       ! 0 |   296 | `		pCons->nLine = nLine;` |
|       ! 0 |   297 | `		pCons->nRunGen = pVm->nRunGen;` |
|       ! 0 |   298 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|       ! 0 |   299 | `		pCons->zDeprecated = 0;     /* ...and its deprecation, which was the old symbol's */` |
|       ! 0 |   300 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|       ! 0 |   301 | `		if( zKeyBuf ){` |
|       ! 0 |   302 | `			SyMemBackendFree(&pVm->sAllocator,zKeyBuf);` |
|       ! 0 |   303 | `		}` |
|       ! 0 |   304 | `		return SXRET_OK;` |
|         - |   305 | `	}` |
|         - |   306 | `	/* Allocate a new constant instance */` |
|  13683563 |   307 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  13683563 |   308 | `	if( pCons == 0 ){` |
|       ! 0 |   309 | `		if( zKeyBuf ){` |
|       ! 0 |   310 | `			SyMemBackendFree(&pVm->sAllocator,zKeyBuf);` |
|       ! 0 |   311 | `		}` |
|       ! 0 |   312 | `		return 0;` |
|         - |   313 | `	}` |
|         - |   314 | `	/* Duplicate constant name */` |
|  13683563 |   315 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  13683563 |   316 | `	if( zDupName == 0 ){` |
|       ! 0 |   317 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |   318 | `		if( zKeyBuf ){` |
|       ! 0 |   319 | `			SyMemBackendFree(&pVm->sAllocator,zKeyBuf);` |
|       ! 0 |   320 | `		}` |
|       ! 0 |   321 | `		return 0;` |
|         - |   322 | `	}` |
|  13683563 |   323 | `	pCons->zKey = zKeyBuf;` |
|  13683563 |   324 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  13683563 |   325 | `	if( pFile ){` |
|       431 |   326 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       213 |   327 | `	}` |
|  13683563 |   328 | `	pCons->nLine = nLine;` |
|  13683563 |   329 | `	pCons->nRunGen = pVm->nRunGen;` |
|  13683563 |   330 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|  13683563 |   331 | `	pCons->zDeprecated = 0;` |
|         - |   332 | `	/* Install the constant */` |
|  13683563 |   333 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  13683563 |   334 | `	pCons->xExpand = xExpand;` |
|  13683563 |   335 | `	pCons->pUserData = pUserData;` |
|  13683563 |   336 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  13683563 |   337 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)(zKeyBuf ? zKeyBuf : zDupName),` |
|   6633725 |   338 | `		SyStringLength(&pCons->sName),pCons);` |
|         - |   339 | `	/* A name that was not a constant is one now, so every PH7_OP_LOADC site that` |
|         - |   340 | `	 * remembers what its name resolved to has to ask again -- including one whose` |
|         - |   341 | `	 * namespaced candidate used to MISS and fall through to the global literal. */` |
|  13683563 |   342 | `	pVm->nConstGen++;` |
|  13683563 |   343 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   344 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |   345 | `		if( zKeyBuf ){` |
|       ! 0 |   346 | `			SyMemBackendFree(&pVm->sAllocator,zKeyBuf);` |
|       ! 0 |   347 | `		}` |
|       ! 0 |   348 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |   349 | `		return rc;` |
|         - |   350 | `	}` |
|         - |   351 | `	/* All done,constant can be invoked from PHP code */` |
|  13683563 |   352 | `	return SXRET_OK;` |
|   6633730 |   353 | `}` |
|         - |   354 | `/*` |
|         - |   355 | ` * Allocate a new foreign function instance.` |
|         - |   356 | ` * This function return SXRET_OK on success. Any other` |
|         - |   357 | ` * return value indicates failure.` |
|         - |   358 | ` * Please refer to the official documentation for an introduction to` |
|         - |   359 | ` * the foreign function mechanism.` |
|         - |   360 | ` */` |
|  22894726 |   361 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|         - |   362 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |   363 | `	const SyString *pName,    /* Foreign function name */` |
|         - |   364 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |   365 | `	void *pUserData,          /* Foreign function private data */` |
|         - |   366 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|         - |   367 | `	)` |
|         5 |   368 | `{` |
|         - |   369 | `	ph7_user_func *pFunc;` |
|         - |   370 | `	char *zDup;` |
|         - |   371 | `	/* Allocate a new user function */` |
|  22894731 |   372 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  22894731 |   373 | `	if( pFunc == 0 ){` |
|       ! 0 |   374 | `		return SXERR_MEM;` |
|         - |   375 | `	}` |
|         - |   376 | `	/* Duplicate function name */` |
|  22894731 |   377 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  22894731 |   378 | `	if( zDup == 0 ){` |
|       ! 0 |   379 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |   380 | `		return SXERR_MEM;` |
|         - |   381 | `	}` |
|         - |   382 | `	/* Zero the structure */` |
|  22894731 |   383 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |   384 | `	/* Initialize structure fields */` |
|  22894731 |   385 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  22894731 |   386 | `	pFunc->pVm   = pVm;` |
|  22894731 |   387 | `	pFunc->xFunc = xFunc;` |
|  22894731 |   388 | `	pFunc->pUserData = pUserData;` |
|  22894731 |   389 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |   390 | `	/* Write a pointer to the new function */` |
|  22894731 |   391 | `	*ppOut = pFunc;` |
|  22894731 |   392 | `	return SXRET_OK;` |
|  11419790 |   393 | `}` |
|         - |   394 | `/*` |
|         - |   395 | ` * Install a foreign function and it's associated callback so that` |
|         - |   396 | ` * it can be invoked from the target PHP code.` |
|         - |   397 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |   398 | ` * return value indicates failure.` |
|         - |   399 | ` * Please refer to the official documentation for an introduction to` |
|         - |   400 | ` * the foreign function mechanism.` |
|         - |   401 | ` */` |
|   9534438 |   402 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|         - |   403 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |   404 | `	const SyString *pName,    /* Foreign function name */` |
|         - |   405 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |   406 | `	void *pUserData           /* Foreign function private data */` |
|         - |   407 | `	)` |
|         5 |   408 | `{` |
|         - |   409 | `	ph7_user_func *pFunc;` |
|         - |   410 | `	SyHashEntry *pEntry;` |
|         - |   411 | `	sxi32 rc;` |
|         - |   412 | `	/* Overwrite any previously registered function with the same name */` |
|   9534443 |   413 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   9534443 |   414 | `	if( pEntry ){` |
|       ! 0 |   415 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|       ! 0 |   416 | `		pFunc->pUserData = pUserData;` |
|       ! 0 |   417 | `		pFunc->xFunc = xFunc;` |
|       ! 0 |   418 | `		SySetReset(&pFunc->aAux);` |
|         - |   419 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |   420 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |   421 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |   422 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|       ! 0 |   423 | `		pFunc->nMinArg  = 0;` |
|       ! 0 |   424 | `		pFunc->nMaxArg  = 0;` |
|       ! 0 |   425 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|       ! 0 |   426 | `		pFunc->bAtLeast = 0;` |
|       ! 0 |   427 | `		return SXRET_OK;` |
|         - |   428 | `	}` |
|         - |   429 | `	/* Create a new user function */` |
|   9534443 |   430 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   9534443 |   431 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   432 | `		return rc;` |
|         - |   433 | `	}` |
|         - |   434 | `	/* Install the function in the corresponding hashtable */` |
|   9534443 |   435 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   9534443 |   436 | `	pVm->nCallableGen++; /* a name that was not callable may be now (OP_CALL_INIT) */` |
|   9534443 |   437 | `	if( rc != SXRET_OK ){` |
|       ! 0 |   438 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |   439 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |   440 | `		return rc;` |
|         - |   441 | `	}` |
|         - |   442 | `	/* User function successfully installed */` |
|   9534443 |   443 | `	return SXRET_OK;` |
|   4748347 |   444 | `}` |
|         - |   445 | `/*` |
|         - |   446 | ` * Initialize a VM function.` |
|         - |   447 | ` */` |
|  13567288 |   448 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |   449 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |   450 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |   451 | `	const char *zName,  /* Function name */` |
|         - |   452 | `	sxu32 nByte,        /* zName length */` |
|         - |   453 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |   454 | `	void *pUserData     /* Function private data */` |
|         - |   455 | `	)` |
|         5 |   456 | `{` |
|         - |   457 | `	/* Zero the structure */` |
|  13567293 |   458 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |   459 | `	/* Initialize structure fields */` |
|         - |   460 | `	/* Arguments container */` |
|  13567293 |   461 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |   462 | `	/* Static variable container */` |
|  13567293 |   463 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |   464 | `	/* Bytecode container */` |
|  13567293 |   465 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |   466 | `    /* Preallocate some instruction slots */` |
|  13567293 |   467 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |   468 | `	/* Closure environment */` |
|  13567293 |   469 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |   470 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|  13567293 |   471 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |   472 | `	/* Declared #[...] attributes */` |
|  13567293 |   473 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  13567293 |   474 | `	pFunc->iFlags = iFlags;` |
|  13567293 |   475 | `	pFunc->pUserData = pUserData;` |
|         - |   476 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |   477 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|  13567293 |   478 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|  13567293 |   479 | `	if( pVm->bCompilingBuiltin ){` |
|         - |   480 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|  13537340 |   481 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   6759856 |   482 | `	}else{` |
|         - |   483 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |   484 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |   485 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     29958 |   486 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     29958 |   487 | `		if( pFile ){` |
|     29958 |   488 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     14855 |   489 | `		}` |
|         - |   490 | `	}` |
|  13567293 |   491 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|  13567293 |   492 | `	return SXRET_OK;` |
|         5 |   493 | `}` |
|         - |   494 | `/*` |
|         - |   495 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|         - |   496 | ` *` |
|         - |   497 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|         - |   498 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|         - |   499 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|         - |   500 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|         - |   501 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|         - |   502 | ` * one.` |
|         - |   503 | ` *` |
|         - |   504 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|         - |   505 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|         - |   506 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|         - |   507 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|         - |   508 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|         - |   509 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|         - |   510 | ` * READ in VmByteCodeExecBody).` |
|         - |   511 | ` *` |
|         - |   512 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|         - |   513 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|         - |   514 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|         - |   515 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|         - |   516 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|         - |   517 | ` * wrote ever passes 1.` |
|         - |   518 | ` */` |
|   4176675 |   519 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|         - |   520 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |   521 | `	const void *pName,  /* Function name */` |
|         - |   522 | `	sxu32 nByte,        /* Name length */` |
|         - |   523 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |   524 | `	)` |
|         5 |   525 | `{` |
|   4176680 |   526 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|   4176680 |   527 | `	if( pEntry && !bEngineName ){` |
|     57617 |   528 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     57617 |   529 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|        25 |   530 | `			return 0;` |
|         - |   531 | `		}` |
|     28505 |   532 | `	}` |
|   4176656 |   533 | `	return pEntry;` |
|   2086923 |   534 | `}` |
|         - |   535 | `/*` |
|         - |   536 | ` * Look a name up in the HOST function table as a script spells it -- the twin of` |
|         - |   537 | ` * PH7_VmGetUserFunction above, and for the same reason.` |
|         - |   538 | ` *` |
|         - |   539 | `` * Nine of the engine's host functions are php LANGUAGE CONSTRUCTS: `empty`, `isset`,`` |
|         - |   540 | `` * `unset`, `eval`, `print`, `include`, `include_once`, `require` and `require_once`.`` |
|         - |   541 | ` * php has none of them in its function table -- they are grammar, and the compiler emits` |
|         - |   542 | `` * an opcode -- so `function_exists('empty')` is false there, `is_callable('isset')` is`` |
|         - |   543 | `` * false, `get_defined_functions()` lists neither, and `$f = 'include'; $f($p);` is`` |
|         - |   544 | `` * `Call to undefined function include()`. Here the construct's codegen dispatches each`` |
|         - |   545 | ` * one as an ordinary call to a host function of the same name, so a plain SyHashGet` |
|         - |   546 | ` * answered every one of those doors YES: phpstan's bundled better-reflection enumerates` |
|         - |   547 | `` * the internal function list, wrote `function empty() {}` into a stub, and its own parser`` |
|         - |   548 | ` * refused the file.` |
|         - |   549 | ` *` |
|         - |   550 | ` * bEngineName is the construct codegen's own dispatch, marked at the call SITE with` |
|         - |   551 | ` * PH7_CALL_CONSTRUCT. Nothing a program wrote ever passes 1: all nine names are lexer` |
|         - |   552 | ` * keywords, so the construct compiler is the only thing that can emit an OP_CALL naming` |
|         - |   553 | `` * one. (`exit`, `die` and `clone` are NOT here -- php 8.5 really does have those three as`` |
|         - |   554 | ` * functions.)` |
|         - |   555 | ` */` |
|   2204338 |   556 | `PH7_PRIVATE SyHashEntry * PH7_VmGetHostFunction(` |
|         - |   557 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |   558 | `	const void *pName,  /* Function name */` |
|         - |   559 | `	sxu32 nByte,        /* Name length */` |
|         - |   560 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |   561 | `	)` |
|         5 |   562 | `{` |
|   2204343 |   563 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,pName,nByte);` |
|   2204343 |   564 | `	if( pEntry && !bEngineName ){` |
|   1932379 |   565 | `		ph7_user_func *pFunc = (ph7_user_func *)pEntry->pUserData;` |
|   1932379 |   566 | `		if( pFunc == 0 \|\| pFunc->bConstruct ){` |
|        71 |   567 | `			return 0;` |
|         - |   568 | `		}` |
|    965359 |   569 | `	}` |
|   2204273 |   570 | `	return pEntry;` |
|   1101381 |   571 | `}` |
|         - |   572 | `/*` |
|         - |   573 | ` * The one copy of a callee name that every call site spelling it shares, made on first` |
|         - |   574 | ` * demand. 0 when it cannot be made, which just costs the caller its cache.` |
|         - |   575 | ` */` |
|     24377 |   576 | `static const char * VmCallNameIntern(ph7_vm *pVm,const SyString *pName)` |
|         5 |   577 | `{` |
|     24382 |   578 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hCallName,pName->zString,pName->nByte);` |
|         - |   579 | `	char *zCopy;` |
|     24382 |   580 | `	if( pEntry ){` |
|     14389 |   581 | `		return (const char *)pEntry->pKey;` |
|         - |   582 | `	}` |
|      9998 |   583 | `	zCopy = (char *)SyMemBackendDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      9998 |   584 | `	if( zCopy == 0 ){` |
|       ! 0 |   585 | `		return 0;` |
|         - |   586 | `	}` |
|      9998 |   587 | `	if( SyHashInsert(&pVm->hCallName,zCopy,pName->nByte,zCopy) != SXRET_OK ){` |
|       ! 0 |   588 | `		SyMemBackendFree(&pVm->sAllocator,zCopy);` |
|       ! 0 |   589 | `		return 0;` |
|         - |   590 | `	}` |
|      9998 |   591 | `	return zCopy;` |
|     12123 |   592 | `}` |
|         - |   593 | `/*` |
|         - |   594 | ` * Are these two names the same bytes? Equality only -- the guard below never orders.` |
|         - |   595 | ` *` |
|         - |   596 | ` * SyMemcmp is a byte loop (SX_MACRO_FAST_CMP, four bytes unrolled with a branch each)` |
|         - |   597 | ` * behind a call, and THIS ONE SITE walked 1,737,378,747 bytes of callee name on the` |
|         - |   598 | ` * ecosystem gate's phpcs step: 42,994,248 guards averaging forty bytes, because a` |
|         - |   599 | ` * namespaced function name is long. It was the second-largest SyMemcmp caller in the` |
|         - |   600 | ` * engine, above the one inside SyHashGetHashed. Eight bytes at a time turns forty` |
|         - |   601 | ` * comparisons into five and drops the call.` |
|         - |   602 | ` *` |
|         - |   603 | ` * The load is through memcpy rather than a cast: an unaligned sxu64 read through a` |
|         - |   604 | ` * char pointer is what UBSan exists to catch, and every compiler in the matrix folds a` |
|         - |   605 | ` * constant-size memcpy into the one load anyway.` |
|         - |   606 | ` *` |
|         - |   607 | ` * This is NOT a case for widening SyMemcmp itself. It was measured, and widening it read` |
|         - |   608 | ` * neutral, because most of its callers compare short property names where a word loop` |
|         - |   609 | ` * never gets going -- the same reason glibc's memcmp measured 1.3% SLOWER there.` |
|         - |   610 | ` */` |
|  16972633 |   611 | `static int VmCallNameEq(const char *zA,const char *zB,sxu32 nByte)` |
|         5 |   612 | `{` |
|         - |   613 | `	/* Declared and seeded out here for MSVC: /WX turns C4701 ("potentially` |
|         - |   614 | `	 * uninitialized local variable used") into an error, and cl cannot see that the` |
|         - |   615 | `	 * memmove below is what writes them. Both compilers drop the two stores. */` |
|  16972638 |   616 | `	sxu64 a = 0,b = 0;` |
|  20202489 |   617 | `	while( nByte >= sizeof(sxu64) ){` |
|   3229886 |   618 | `		SX_MACRO_FAST_MEMCPY(zA,&a,sizeof(a));` |
|   3229886 |   619 | `		SX_MACRO_FAST_MEMCPY(zB,&b,sizeof(b));` |
|   3229886 |   620 | `		if( a != b ){` |
|        31 |   621 | `			return 0;` |
|         - |   622 | `		}` |
|   3229856 |   623 | `		zA += sizeof(sxu64);` |
|   3229856 |   624 | `		zB += sizeof(sxu64);` |
|   3229856 |   625 | `		nByte -= (sxu32)sizeof(sxu64);` |
|         5 |   626 | `	}` |
| 108383274 |   627 | `	while( nByte-- > 0 ){` |
|  91411549 |   628 | `		if( *zA++ != *zB++ ){` |
|       883 |   629 | `			return 0;` |
|         - |   630 | `		}` |
|         5 |   631 | `	}` |
|  16971730 |   632 | `	return 1;` |
|   8488595 |   633 | `}` |
|         - |   634 | `/*` |
|         - |   635 | ` * The VmCallSite record a PH7_OP_CALL site owns. bClaim says which of the two doors is` |
|         - |   636 | ` * asking: the RECORDING one may create the record, the ASKING one only reads it.` |
|         - |   637 | ` *` |
|         - |   638 | ` * Answers 0 whenever the site has to resolve the long way -- it has no record yet, it` |
|         - |   639 | ` * has been marked dead, it is asking about a name it did not ask about before (which is` |
|         - |   640 | ` * what marks it dead), or the bookkeeping could not be allocated.` |
|         - |   641 | ` */` |
|  23633917 |   642 | `static VmCallSite * VmCallSiteFor(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,` |
|         - |   643 | `	int bEngineName,int bClaim)` |
|         5 |   644 | `{` |
|         - |   645 | `	VmCallSite *pSite;` |
|  23633922 |   646 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|       ! 0 |   647 | `		return 0;` |
|         - |   648 | `	}` |
|  23633922 |   649 | `	if( pInstr->nSite == 0 ){` |
|         - |   650 | `		/* No record yet. Claim one only on this site's SECOND execution, because a` |
|         - |   651 | `		 * record costs more memory than a site that runs once can ever save -- a` |
|         - |   652 | `		 * bootstrap, a one-shot branch, a sniff that matches nothing. pInstr->nAux is` |
|         - |   653 | `		 * free on a PH7_OP_CALL (OP_LOAD and OP_CALL_INIT are the only opcodes that` |
|         - |   654 | `		 * use it), so the site counts its own first two executions there.` |
|         - |   655 | `		 *` |
|         - |   656 | `		 * Only the ASKING door counts, and only the RECORDING door claims: both run on` |
|         - |   657 | `		 * one dispatch, so a shared counter would reach two before the first call has` |
|         - |   658 | `		 * finished and the site would pay on its first execution after all. */` |
|         - |   659 | `		VmCallSite sNew;` |
|         - |   660 | `		const char *zCopy;` |
|   6624823 |   661 | `		if( !bClaim ){` |
|   3432487 |   662 | `			if( pInstr->nAux < 2 ){` |
|   3192489 |   663 | `				pInstr->nAux++;` |
|   1595494 |   664 | `			}` |
|   3432487 |   665 | `			return 0;` |
|         - |   666 | `		}` |
|   3192341 |   667 | `		if( pInstr->nAux < 2 ){` |
|   3167964 |   668 | `			return 0;` |
|         - |   669 | `		}` |
|     24382 |   670 | `		zCopy = VmCallNameIntern(&(*pVm),pName);` |
|     24382 |   671 | `		if( zCopy == 0 ){` |
|       ! 0 |   672 | `			return 0;` |
|         - |   673 | `		}` |
|     24382 |   674 | `		sNew.zName = zCopy;` |
|     24382 |   675 | `		sNew.nName = pName->nByte;` |
|     24382 |   676 | `		sNew.pEntry = 0;` |
|     24382 |   677 | `		sNew.nGen = 0;` |
|     24382 |   678 | `		sNew.nNextFree = 0;` |
|     24382 |   679 | `		sNew.bHost = 0;` |
|     24382 |   680 | `		sNew.bEngine = (sxu8)(bEngineName ? 1 : 0);` |
|     24382 |   681 | `		sNew.bDead = 0;` |
|     24382 |   682 | `		if( pVm->nFreeCallSite ){` |
|      6403 |   683 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|      6403 |   684 | `			if( pSite ){` |
|      6403 |   685 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|      6403 |   686 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|      6403 |   687 | `				*pSite = sNew;` |
|      6403 |   688 | `				return pSite;` |
|         - |   689 | `			}` |
|       ! 0 |   690 | `			pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse rather than on the cache */` |
|       ! 0 |   691 | `		}` |
|     17980 |   692 | `		if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|       ! 0 |   693 | `			return 0; /* the interned name stays; another site may still want it */` |
|         - |   694 | `		}` |
|     17980 |   695 | `		pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|     17980 |   696 | `		return (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|         - |   697 | `	}` |
|  17009104 |   698 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|  17009104 |   699 | `	if( pSite == 0 \|\| pSite->bDead ){` |
|     36091 |   700 | `		return 0;` |
|         - |   701 | `	}` |
|  16973013 |   702 | `	if( pSite->nName != pName->nByte` |
|  16972823 |   703 | `	 \|\| pSite->bEngine != (sxu8)(bEngineName ? 1 : 0)` |
|  16972638 |   704 | `	 \|\| !VmCallNameEq(pSite->zName,pName->zString,pSite->nName) ){` |
|         - |   705 | `		/* A second name at one site: the callee is a variable (or a closure key), and` |
|         - |   706 | `		 * re-interning it on every call would cost more than the lookup it saves. */` |
|      1293 |   707 | `		pSite->zName = 0;` |
|      1293 |   708 | `		pSite->nName = 0;` |
|      1293 |   709 | `		pSite->pEntry = 0;` |
|      1293 |   710 | `		pSite->nGen = 0;` |
|      1293 |   711 | `		pSite->bDead = 1;` |
|      1293 |   712 | `		return 0;` |
|         - |   713 | `	}` |
|  16971730 |   714 | `	return pSite;` |
|  11817470 |   715 | `}` |
|         - |   716 | `/*` |
|         - |   717 | ` * The function-table entry this call site resolved its callee to last time, or 0 if it` |
|         - |   718 | ` * has to be resolved again. *pbHost says which table the answer is in.` |
|         - |   719 | ` */` |
|  20352688 |   720 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(` |
|         - |   721 | `	ph7_vm *pVm,          /* Target VM */` |
|         - |   722 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|         - |   723 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|         - |   724 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|         - |   725 | `	int *pbHost           /* OUT: 1 when the answer lives in hHostFunction */` |
|         - |   726 | `	)` |
|         5 |   727 | `{` |
|  20352693 |   728 | `	VmCallSite *pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,0);` |
|  20352693 |   729 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nCallableGen ){` |
|   3521380 |   730 | `		return 0;` |
|         - |   731 | `	}` |
|  16831318 |   732 | `	*pbHost = pSite->bHost;` |
|  16831318 |   733 | `	return pSite->pEntry;` |
|  10177741 |   734 | `}` |
|         - |   735 | `/*` |
|         - |   736 | ` * Remember what this call site's callee name resolved to, so the next execution can` |
|         - |   737 | ` * skip the lookups. Silently does nothing for a site that has no record to write to.` |
|         - |   738 | ` */` |
|   5102253 |   739 | `PH7_PRIVATE void PH7_VmCallSiteRecord(` |
|         - |   740 | `	ph7_vm *pVm,          /* Target VM */` |
|         - |   741 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|         - |   742 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|         - |   743 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|         - |   744 | `	int bHost,            /* TRUE when pEntry lives in hHostFunction */` |
|         - |   745 | `	SyHashEntry *pEntry   /* The entry the name resolved to */` |
|         - |   746 | `	)` |
|         5 |   747 | `{` |
|         - |   748 | `	VmCallSite *pSite;` |
|   5102258 |   749 | `	if( pEntry == 0 ){` |
|   1821029 |   750 | `		return;` |
|         - |   751 | `	}` |
|   3281234 |   752 | `	pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,1);` |
|   3281234 |   753 | `	if( pSite == 0 ){` |
|   3186651 |   754 | `		return;` |
|         - |   755 | `	}` |
|     94588 |   756 | `	pSite->pEntry = pEntry;` |
|     94588 |   757 | `	pSite->bHost = (sxu8)(bHost ? 1 : 0);` |
|     94588 |   758 | `	pSite->nGen = pVm->nCallableGen;` |
|   2549845 |   759 | `}` |
|         - |   760 | `/*` |
|         - |   761 | ` * The hConstant entry a PH7_OP_LOADC site resolved its constant to last time, or 0 when` |
|         - |   762 | ` * it has to be resolved the long way.` |
|         - |   763 | ` *` |
|         - |   764 | ` * A LOADC site looks up as many as TWO names on every execution -- the compile-time` |
|         - |   765 | `` * candidate in p3 (a `use const` import's FQN, or `current-namespace\NAME`) and then the`` |
|         - |   766 | ` * bare literal -- and on the ecosystem gate's phpcs step those two were 104M of the` |
|         - |   767 | ` * engine's 724M hash lookups and 3.0 GB of its 7.5 GB of hashed key bytes. The candidate` |
|         - |   768 | ` * alone hashed 2.3 GB to miss 91% of the time, because a namespace-qualified name is` |
|         - |   769 | ` * long and usually is not a constant.` |
|         - |   770 | ` *` |
|         - |   771 | ` * Which of the two wins, and what it resolves to, can only change when a name enters or` |
|         - |   772 | ` * leaves hConstant. So the site stamps the pVm->nConstGen it resolved at, and a site` |
|         - |   773 | ` * whose stamp is current answers without hashing anything.` |
|         - |   774 | ` *` |
|         - |   775 | ` * The record is claimed on the site's SECOND execution, for VmCallSiteFor's reason: a` |
|         - |   776 | ` * bootstrap or a one-shot branch would pay for bookkeeping it never reads. pInstr->nAux` |
|         - |   777 | ` * is free on a PH7_OP_LOADC, so the site counts its first two executions there.` |
|         - |   778 | ` */` |
|    179180 |   779 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr)` |
|         5 |   780 | `{` |
|         - |   781 | `	VmCallSite *pSite;` |
|    179185 |   782 | `	if( pInstr->nSite == 0 ){` |
|     10932 |   783 | `		if( pInstr->nAux < 2 ){` |
|     10890 |   784 | `			pInstr->nAux++;` |
|      5354 |   785 | `		}` |
|     10932 |   786 | `		return 0;` |
|         - |   787 | `	}` |
|    168258 |   788 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|    168258 |   789 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nConstGen ){` |
|       129 |   790 | `		return 0;` |
|         - |   791 | `	}` |
|    168130 |   792 | `	return pSite->pEntry;` |
|     89457 |   793 | `}` |
|         - |   794 | `/*` |
|         - |   795 | ` * Remember what this PH7_OP_LOADC site's constant name resolved to. Silently does` |
|         - |   796 | ` * nothing when the site has not earned a record yet or one cannot be allocated -- the` |
|         - |   797 | ` * lookup path above is always correct on its own.` |
|         - |   798 | ` */` |
|     11055 |   799 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry)` |
|         5 |   800 | `{` |
|         - |   801 | `	VmCallSite *pSite;` |
|     11060 |   802 | `	if( pEntry == 0 ){` |
|       211 |   803 | `		return;` |
|         - |   804 | `	}` |
|     10854 |   805 | `	if( pInstr->nSite == 0 ){` |
|         - |   806 | `		VmCallSite sNew;` |
|     10726 |   807 | `		if( pInstr->nAux < 2 ){` |
|      9752 |   808 | `			return;` |
|         - |   809 | `		}` |
|       979 |   810 | `		sNew.zName = 0;   /* a LOADC site's name is its instruction; nothing to guard */` |
|       979 |   811 | `		sNew.nName = 0;` |
|       979 |   812 | `		sNew.pEntry = 0;` |
|       979 |   813 | `		sNew.nGen = 0;` |
|       979 |   814 | `		sNew.nNextFree = 0;` |
|       979 |   815 | `		sNew.bHost = 0;` |
|       979 |   816 | `		sNew.bEngine = 0;` |
|       979 |   817 | `		sNew.bDead = 0;` |
|       979 |   818 | `		if( pVm->nFreeCallSite ){` |
|       189 |   819 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|       189 |   820 | `			if( pSite ){` |
|       189 |   821 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|       189 |   822 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|       189 |   823 | `				*pSite = sNew;` |
|        95 |   824 | `			}else{` |
|       ! 0 |   825 | `				pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse, not on the cache */` |
|         - |   826 | `			}` |
|        94 |   827 | `		}` |
|       979 |   828 | `		if( pInstr->nSite == 0 ){` |
|       791 |   829 | `			if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|       ! 0 |   830 | `				return;` |
|         - |   831 | `			}` |
|       791 |   832 | `			pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|       386 |   833 | `		}` |
|       480 |   834 | `	}` |
|      1107 |   835 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|      1107 |   836 | `	if( pSite == 0 ){` |
|       ! 0 |   837 | `		return;` |
|         - |   838 | `	}` |
|      1107 |   839 | `	pSite->pEntry = pEntry;` |
|      1107 |   840 | `	pSite->nGen = pVm->nConstGen;` |
|      5444 |   841 | `}` |
|         - |   842 | `/*` |
|         - |   843 | ` * Give back every site record the instructions in a bytecode container claimed -- a` |
|         - |   844 | ` * PH7_OP_CALL's callee answer and a PH7_OP_LOADC's constant answer alike. Called just` |
|         - |   845 | ` * before the container itself is released -- which happens exactly once, for the chunk an` |
|         - |   846 | ` * eval() or an include compiles -- so that a program evaluating chunks in a loop reuses` |
|         - |   847 | ` * the records instead of accumulating one per chunk for ever.` |
|         - |   848 | ` */` |
|     31317 |   849 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode)` |
|         5 |   850 | `{` |
|     31322 |   851 | `	VmInstr *aInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|     31322 |   852 | `	sxu32 n = SySetUsed(pByteCode);` |
|         - |   853 | `	sxu32 i;` |
|     31322 |   854 | `	if( aInstr == 0 ){` |
|       ! 0 |   855 | `		return;` |
|         - |   856 | `	}` |
|    827161 |   857 | `	for( i = 0 ; i < n ; ++i ){` |
|         - |   858 | `		VmCallSite *pSite;` |
|    795844 |   859 | `		sxu32 nSite = aInstr[i].nSite;` |
|    795839 |   860 | `		if( nSite == 0` |
|    401217 |   861 | `		 \|\| (aInstr[i].iOp != PH7_OP_CALL && aInstr[i].iOp != PH7_OP_LOADC) ){` |
|    789248 |   862 | `			continue;` |
|         - |   863 | `		}` |
|      6597 |   864 | `		aInstr[i].nSite = 0;` |
|      6597 |   865 | `		pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,nSite - 1);` |
|      6597 |   866 | `		if( pSite == 0 ){` |
|       ! 0 |   867 | `			continue;` |
|         - |   868 | `		}` |
|      6597 |   869 | `		pSite->zName = 0; /* the name itself is hCallName's, and other sites may share it */` |
|      6597 |   870 | `		pSite->nName = 0;` |
|      6597 |   871 | `		pSite->pEntry = 0;` |
|      6597 |   872 | `		pSite->nGen = 0;` |
|      6597 |   873 | `		pSite->bDead = 0;` |
|      6597 |   874 | `		pSite->nNextFree = pVm->nFreeCallSite;` |
|      6597 |   875 | `		pVm->nFreeCallSite = nSite;` |
|      3299 |   876 | `	}` |
|     15658 |   877 | `}` |
|         - |   878 | `/*` |
|         - |   879 | ` * Namespace-aware function lookup.` |
|         - |   880 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |   881 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |   882 | ` */` |
|         - |   883 | `/*` |
|         - |   884 | ` * Record one declaration the unit being compiled installed (see VmUnitDecl).` |
|         - |   885 | ` */` |
|    183899 |   886 | `static void VmUnitDeclLog(ph7_vm *pVm,void *pDecl,const SyString *pName,int bClass)` |
|         5 |   887 | `{` |
|         - |   888 | `	VmUnitDecl sDecl;` |
|    183904 |   889 | `	sDecl.pDecl = pDecl;` |
|    183904 |   890 | `	sDecl.sName = *pName;` |
|    183904 |   891 | `	sDecl.bClass = (sxu8)bClass;` |
|    183904 |   892 | `	SySetPut(&pVm->aUnitDecl,(const void *)&sDecl);` |
|    183904 |   893 | `}` |
|         - |   894 | `/*` |
|         - |   895 | ` * Take one declaration back out of its table: the name answers whatever it answered` |
|         - |   896 | ` * before the install, or nothing.` |
|         - |   897 | ` */` |
|        78 |   898 | `static void VmUnitDeclUnlink(ph7_vm *pVm,const VmUnitDecl *pDecl)` |
|         4 |   899 | `{` |
|        82 |   900 | `	SyHash *pTable = pDecl->bClass ? &pVm->hClass : &pVm->hFunction;` |
|        82 |   901 | `	SyHashEntry *pEntry = SyHashGet(pTable,(const void *)pDecl->sName.zString,pDecl->sName.nByte);` |
|        82 |   902 | `	if( pEntry == 0 ){` |
|       ! 0 |   903 | `		return;` |
|         - |   904 | `	}` |
|        82 |   905 | `	if( pDecl->bClass ){` |
|        34 |   906 | `		ph7_class *pClass = (ph7_class *)pDecl->pDecl;` |
|        34 |   907 | `		ph7_class **ppLink = (ph7_class **)&pEntry->pUserData;` |
|        34 |   908 | `		while( *ppLink && *ppLink != pClass ){` |
|       ! 0 |   909 | `			ppLink = &(*ppLink)->pNextName;` |
|       ! 0 |   910 | `		}` |
|        34 |   911 | `		if( *ppLink == 0 ){` |
|       ! 0 |   912 | `			return;` |
|         - |   913 | `		}` |
|        34 |   914 | `		*ppLink = pClass->pNextName;` |
|        34 |   915 | `		pClass->pNextName = 0;` |
|        34 |   916 | `		if( pClass->pBase ){` |
|         - |   917 | `			/* ...and out of its parent's subclass table, which is keyed by NAME: a` |
|         - |   918 | `			 * later, unrelated class spelled the same would answer is_subclass_of(). */` |
|         7 |   919 | `			SyHashEntry *pSub = SyHashGet(&pClass->pBase->hDerived,` |
|         4 |   920 | `				(const void *)SyStringData(&pClass->sName),SyStringLength(&pClass->sName));` |
|         5 |   921 | `			if( pSub && pSub->pUserData == (void *)pClass ){` |
|         5 |   922 | `				SyHashDeleteEntry2(pSub);` |
|         2 |   923 | `			}` |
|         2 |   924 | `		}` |
|        19 |   925 | `	}else{` |
|        51 |   926 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pDecl->pDecl;` |
|        51 |   927 | `		ph7_vm_func **ppLink = (ph7_vm_func **)&pEntry->pUserData;` |
|        51 |   928 | `		while( *ppLink && *ppLink != pFunc ){` |
|       ! 0 |   929 | `			ppLink = &(*ppLink)->pNextName;` |
|       ! 0 |   930 | `		}` |
|        51 |   931 | `		if( *ppLink == 0 ){` |
|       ! 0 |   932 | `			return;` |
|         - |   933 | `		}` |
|        51 |   934 | `		*ppLink = pFunc->pNextName;` |
|        51 |   935 | `		pFunc->pNextName = 0;` |
|         - |   936 | `	}` |
|        82 |   937 | `	if( pEntry->pUserData == 0 ){` |
|        82 |   938 | `		SyHashDeleteEntry2(pEntry);` |
|        39 |   939 | `	}` |
|        43 |   940 | `}` |
|         - |   941 | `/*` |
|         - |   942 | ` * The unit VmEvalChunk compiled from aUnitDecl[nMark] on is finished: a failed one` |
|         - |   943 | ` * takes everything it installed back out, newest first; either way the log drops it.` |
|         - |   944 | ` */` |
|     31317 |   945 | `PH7_PRIVATE void PH7_VmUnitDeclEnd(ph7_vm *pVm,sxu32 nMark,int bFailed)` |
|         5 |   946 | `{` |
|     31322 |   947 | `	VmUnitDecl *aDecl = (VmUnitDecl *)SySetBasePtr(&pVm->aUnitDecl);` |
|     31322 |   948 | `	sxu32 n = SySetUsed(&pVm->aUnitDecl);` |
|     31322 |   949 | `	if( bFailed && n > nMark ){` |
|       150 |   950 | `		while( n > nMark ){` |
|        82 |   951 | `			n--;` |
|        82 |   952 | `			VmUnitDeclUnlink(pVm,&aDecl[n]);` |
|         4 |   953 | `		}` |
|         - |   954 | `		/* A call site may hold the entry just deleted. */` |
|        72 |   955 | `		pVm->nCallableGen++;` |
|        34 |   956 | `	}` |
|     31322 |   957 | `	SySetTruncate(&pVm->aUnitDecl,nMark);` |
|     31322 |   958 | `}` |
|         - |   959 | `/*` |
|         - |   960 | ` * Hide every class aHiddenClass lists from nMark on: its unit has compiled it,` |
|         - |   961 | ` * and php declares it only where its statement runs (PH7_OP_CLASS_DECLARE). It` |
|         - |   962 | ` * keeps its slot in hClass -- php files it at compile time too, under a key no` |
|         - |   963 | ` * lookup matches, so get_declared_classes() lists it in FILE order -- and every` |
|         - |   964 | ` * lookup by name passes over it (PH7_VmClassEntry) until then.` |
|         - |   965 | ` */` |
|     37382 |   966 | `PH7_PRIVATE void PH7_VmHideClasses(ph7_vm *pVm,sxu32 nMark)` |
|         5 |   967 | `{` |
|     37387 |   968 | `	ph7_class **apClass = (ph7_class **)SySetBasePtr(&pVm->aHiddenClass);` |
|         - |   969 | `	sxu32 n;` |
|     38643 |   970 | `	for( n = nMark ; n < SySetUsed(&pVm->aHiddenClass) ; n++ ){` |
|      1261 |   971 | `		apClass[n]->iFlags \|= PH7_CLASS_HIDDEN;` |
|       633 |   972 | `	}` |
|     37387 |   973 | `	SySetTruncate(&pVm->aHiddenClass,nMark);` |
|     37387 |   974 | `}` |
|         - |   975 | `/*` |
|         - |   976 | ` * The hClass entry a lookup BY NAME finds: none when every class filed under the` |
|         - |   977 | ` * name still waits for its statement.` |
|         - |   978 | ` */` |
|  10560943 |   979 | `PH7_PRIVATE SyHashEntry * PH7_VmClassEntry(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 |   980 | `{` |
|  10560948 |   981 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|         - |   982 | `	ph7_class *pClass;` |
|  10560948 |   983 | `	if( pEntry == 0 ){` |
|      1131 |   984 | `		return 0;` |
|         - |   985 | `	}` |
|  10559840 |   986 | `	for( pClass = (ph7_class *)pEntry->pUserData ; pClass ; pClass = pClass->pNextName ){` |
|  10559822 |   987 | `		if( (pClass->iFlags & PH7_CLASS_HIDDEN) == 0 ){` |
|  10559804 |   988 | `			return pEntry;` |
|         - |   989 | `		}` |
|        11 |   990 | `	}` |
|        20 |   991 | `	return 0;` |
|   5278070 |   992 | `}` |
|         - |   993 | `/*` |
|         - |   994 | ` * PH7_OP_CLASS_DECLARE: the statement of a class php did not early-bind runs, and` |
|         - |   995 | ` * the class takes its name only now. A name something else took meanwhile (an` |
|         - |   996 | ` * autoloader asked for it above the statement) is php's redeclaration fatal.` |
|         - |   997 | ` */` |
|      1238 |   998 | `PH7_PRIVATE sxi32 PH7_VmDeclareHiddenClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 |   999 | `{` |
|      1243 |  1000 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  1001 | `	SyHashEntry *pEntry;` |
|         - |  1002 | `	ph7_class *pPrev;` |
|      1243 |  1003 | `	if( (pClass->iFlags & PH7_CLASS_HIDDEN) == 0 ){` |
|         8 |  1004 | `		return SXRET_OK;` |
|         - |  1005 | `	}` |
|      1237 |  1006 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pClass->sName.zString,pClass->sName.nByte);` |
|      2467 |  1007 | `	for( pPrev = pEntry ? (ph7_class *)pEntry->pUserData : 0 ; pPrev ; pPrev = pPrev->pNextName ){` |
|         - |  1008 | `		const char *zKind;` |
|      1237 |  1009 | `		if( pPrev == pClass \|\| (pPrev->iFlags & PH7_CLASS_HIDDEN) ){` |
|      1235 |  1010 | `			continue;` |
|         - |  1011 | `		}` |
|         5 |  1012 | `		zKind = (pPrev->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|         3 |  1013 | `			: (pPrev->iFlags & PH7_CLASS_TRAIT) ? "trait"` |
|         2 |  1014 | `			: (pPrev->iFlags & PH7_CLASS_ENUM) ? "enum" : "class";` |
|         - |  1015 | `		/* php's compile fatal, raised from the declaration (PH7_ClassSettleObligations` |
|         - |  1016 | `		 * borrows the generator the same way). */` |
|         - |  1017 | `		{` |
|         3 |  1018 | `			ProcConsumer xSavedErr = pGen->xErr;` |
|         3 |  1019 | `			void *pSavedErrData = pGen->pErrData;` |
|         3 |  1020 | `			sxu32 nErr = pGen->nErr;` |
|         3 |  1021 | `			pGen->xErr = pVm->pEngine->xConf.xErr;` |
|         3 |  1022 | `			pGen->pErrData = pVm->pEngine->xConf.pErrData;` |
|         3 |  1023 | `			pGen->iFatalTrace = PH7_FATAL_TRACE_RUNTIME;` |
|         3 |  1024 | `			if( pPrev->sFile.nByte > 0 ){` |
|         4 |  1025 | `				PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|         - |  1026 | `					"Cannot redeclare %s %z (previously declared in %.*s:%u)",` |
|         1 |  1027 | `					zKind,&pClass->sDisp,pPrev->sFile.nByte,pPrev->sFile.zString,pPrev->nLine);` |
|         2 |  1028 | `			}else{` |
|       ! 0 |  1029 | `				PH7_GenCompileError(pGen,E_ERROR,pClass->nLine,` |
|       ! 0 |  1030 | `					"Cannot redeclare %s %z",zKind,&pClass->sDisp);` |
|         - |  1031 | `			}` |
|         3 |  1032 | `			pGen->iFatalTrace = PH7_FATAL_TRACE_COMPILE;` |
|         3 |  1033 | `			pGen->nErr = nErr;` |
|         3 |  1034 | `			pGen->xErr = xSavedErr;` |
|         3 |  1035 | `			pGen->pErrData = pSavedErrData;` |
|         - |  1036 | `		}` |
|         3 |  1037 | `		pVm->iExitStatus = 255;` |
|         3 |  1038 | `		pVm->bHaltRequested = 1;` |
|         3 |  1039 | `		return SXERR_ABORT;` |
|       ! 0 |  1040 | `	}` |
|      1235 |  1041 | `	pClass->iFlags &= ~(PH7_CLASS_HIDDEN\|PH7_CLASS_LATEBIND);` |
|      1235 |  1042 | `	return SXRET_OK;` |
|       624 |  1043 | `}` |
|         - |  1044 | `/*` |
|         - |  1045 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  1046 | ` */` |
|  55893537 |  1047 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  1048 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  1049 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  1050 | `	SyString *pName     /* Function name */` |
|         - |  1051 | `	)` |
|         5 |  1052 | `{` |
|         - |  1053 | `	SyHashEntry *pEntry;` |
|         - |  1054 | `	sxi32 rc;` |
|  55893542 |  1055 | `	if( pName == 0 ){` |
|         - |  1056 | `		/* Use the built-in name */` |
|    205793 |  1057 | `		pName = &pFunc->sName;` |
|    102651 |  1058 | `	}` |
|         - |  1059 | `	/* Check for duplicates (functions with the same name) first */` |
|  55893542 |  1060 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  55893542 |  1061 | `	if( pEntry ){` |
|  42912894 |  1062 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  42912894 |  1063 | `		if( pLink != pFunc ){` |
|         - |  1064 | `			/* Link */` |
|        72 |  1065 | `			pFunc->pNextName = pLink;` |
|        72 |  1066 | `			pEntry->pUserData = pFunc;` |
|        72 |  1067 | `			if( pVm->bUnitDecl ){` |
|       ! 0 |  1068 | `				VmUnitDeclLog(pVm,(void *)pFunc,pName,0);` |
|       ! 0 |  1069 | `			}` |
|        34 |  1070 | `		}` |
|  42912894 |  1071 | `		return SXRET_OK;` |
|         - |  1072 | `	}` |
|  12980653 |  1073 | `	if( pVm->bUnitDecl ){` |
|    180132 |  1074 | `		VmUnitDeclLog(pVm,(void *)pFunc,pName,0);` |
|     89948 |  1075 | `	}` |
|         - |  1076 | `	/* First time seen */` |
|  12980653 |  1077 | `	pFunc->pNextName = 0;` |
|  12980653 |  1078 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|  12980653 |  1079 | `	if( (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) == 0 ){` |
|         - |  1080 | `		/* A name a SCRIPT can now call. This table also holds every method and every` |
|         - |  1081 | `		 * per-instantiation closure copy -- names PH7_VmGetUserFunction refuses to a` |
|         - |  1082 | `		 * script -- and counting those would retire OP_CALL_INIT's screened-at stamps` |
|         - |  1083 | `		 * on every closure EXPRESSION a program evaluates, which is most of them. */` |
|    182612 |  1084 | `		pVm->nCallableGen++;` |
|     91182 |  1085 | `	}` |
|  12980653 |  1086 | `	return rc;` |
|  27906837 |  1087 | `}` |
|         - |  1088 | `/*` |
|         - |  1089 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  1090 | ` */` |
|   2093103 |  1091 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  1092 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  1093 | `	ph7_class *pClass /* Target Class */` |
|         - |  1094 | `	)` |
|         5 |  1095 | `{` |
|   2093108 |  1096 | `	SyString *pName = &pClass->sName;` |
|         - |  1097 | `	SyHashEntry *pEntry;` |
|         - |  1098 | `	sxi32 rc;` |
|   2093108 |  1099 | `	if( pVm->sCodeGen.bDeclCheck ){` |
|         - |  1100 | `		/* A deferred declaration compiled only for its refusals: it, and any` |
|         - |  1101 | `		 * anonymous class in its methods, is declared when the real compile runs. */` |
|       209 |  1102 | `		return SXRET_OK;` |
|         - |  1103 | `	}` |
|   2092904 |  1104 | `	if( pVm->bUnitDecl ){` |
|      3777 |  1105 | `		VmUnitDeclLog(pVm,(void *)pClass,pName,1);` |
|      1886 |  1106 | `	}` |
|         - |  1107 | `	/* Check for duplicates */` |
|   2092904 |  1108 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   2092904 |  1109 | `	if( pEntry ){` |
|         9 |  1110 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  1111 | `		/* Link entry with the same name */` |
|         9 |  1112 | `		pClass->pNextName = pLink;` |
|         9 |  1113 | `		pEntry->pUserData = pClass;` |
|         9 |  1114 | `		return SXRET_OK;` |
|         - |  1115 | `	}` |
|   2092896 |  1116 | `	pClass->pNextName = 0;` |
|         - |  1117 | `	/* Perform a simple hashtable insertion */` |
|   2092896 |  1118 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   2092896 |  1119 | `	return rc;` |
|   1045198 |  1120 | `}` |
|         - |  1121 | `/*` |
|         - |  1122 | ` * Instruction builder interface.` |
|         - |  1123 | ` */` |
|  18991039 |  1124 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  1125 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  1126 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  1127 | `	sxi32 iP1,    /* First operand */` |
|         - |  1128 | `	sxu32 iP2,    /* Second operand */` |
|         - |  1129 | `	void *p3,     /* Third operand */` |
|         - |  1130 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  1131 | `	)` |
|         5 |  1132 | `{` |
|         - |  1133 | `	VmInstr sInstr;` |
|  18991044 |  1134 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  1135 | `	sxi32 rc;` |
|         - |  1136 | `	/* Fill the VM instruction */` |
|  18991044 |  1137 | `	sInstr.iOp = (sxu8)iOp;` |
|  18991044 |  1138 | `	sInstr.iP1 = iP1;` |
|  18991044 |  1139 | `	sInstr.iP2 = iP2;` |
|  18991044 |  1140 | `	sInstr.p3  = p3;` |
|         - |  1141 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  1142 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  1143 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  1144 | `	 * between statements, hence the range check. */` |
|  18991044 |  1145 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|         - |  1146 | `	/* Nothing is discarded until the statement that owns this call says so` |
|         - |  1147 | `	 * (GenStateMarkDiscardedCall, after the fact) — but the field must not be` |
|         - |  1148 | `	 * this stack frame's leftovers in the meantime. */` |
|  18991044 |  1149 | `	sInstr.bDiscard = 0;` |
|         - |  1150 | `	/* ...and neither must the reference-source marker: the codegen stamps it on the` |
|         - |  1151 | `	 * one OP_MEMBER it belongs to, AFTER this returns. */` |
|  18991044 |  1152 | `	sInstr.bRefSrc = 0;` |
|  18991044 |  1153 | `	sInstr.nAux = 0;` |
|         - |  1154 | `	/* ...nor the call site's cache index: a stale one would point this site at` |
|         - |  1155 | `	 * another site's remembered callee. */` |
|  18991044 |  1156 | `	sInstr.nSite = 0;` |
|  18991044 |  1157 | `	sInstr.nLine = 0;` |
|  18991044 |  1158 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   6788637 |  1159 | `		sInstr.nLine = pGen->pIn->nLine;` |
|  15591231 |  1160 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  1161 | `		/* Past the end (statement tail): blame the last real token. */` |
|  12103852 |  1162 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   6042255 |  1163 | `	}` |
|  18991044 |  1164 | `	if( pIndex ){` |
|         - |  1165 | `		/* Instruction index in the bytecode array */` |
|   1560200 |  1166 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    779067 |  1167 | `	}` |
|         - |  1168 | `	/* Finally,record the instruction */` |
|  18991044 |  1169 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  18991044 |  1170 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  1171 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  1172 | `		/* Fall throw */` |
|       ! 0 |  1173 | `	}` |
|  18991044 |  1174 | `	return rc;` |
|         5 |  1175 | `}` |
|         - |  1176 | `/*` |
|         - |  1177 | ` * Swap the current bytecode container with the given one.` |
|         - |  1178 | ` */` |
|    537560 |  1179 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  1180 | `{` |
|    537565 |  1181 | `	if( pContainer == 0 ){` |
|         - |  1182 | `		/* Point to the default container */` |
|       ! 0 |  1183 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  1184 | `	}else{` |
|         - |  1185 | `		/* Change container */` |
|    537565 |  1186 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  1187 | `	}` |
|    537565 |  1188 | `	return SXRET_OK;` |
|         5 |  1189 | `}` |
|         - |  1190 | `/*` |
|         - |  1191 | ` * Return the current bytecode container.` |
|         - |  1192 | ` */` |
|   1460156 |  1193 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  1194 | `{` |
|   1460161 |  1195 | `	return pVm->pByteContainer;` |
|         5 |  1196 | `}` |
|         - |  1197 | `/*` |
|         - |  1198 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  1199 | ` */` |
|   1630225 |  1200 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  1201 | `{` |
|         - |  1202 | `	VmInstr *pInstr;` |
|   1630230 |  1203 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   1630230 |  1204 | `	return pInstr;` |
|         5 |  1205 | `}` |
|         - |  1206 | `/*` |
|         - |  1207 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  1208 | ` */` |
|  15318130 |  1209 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  1210 | `{` |
|  15318135 |  1211 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  1212 | `}` |
|         - |  1213 | `/*` |
|         - |  1214 | ` * Pop the last VM instruction.` |
|         - |  1215 | ` */` |
|   1192860 |  1216 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  1217 | `{` |
|   1192865 |  1218 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  1219 | `}` |
|         - |  1220 | `/*` |
|         - |  1221 | ` * Peek the last VM instruction.` |
|         - |  1222 | ` */` |
|   6352073 |  1223 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  1224 | `{` |
|   6352078 |  1225 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  1226 | `}` |
|    122589 |  1227 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  1228 | `{` |
|         - |  1229 | `	VmInstr *aInstr;` |
|         - |  1230 | `	sxu32 n;` |
|    122594 |  1231 | `	n = SySetUsed(pVm->pByteContainer);` |
|    122594 |  1232 | `	if( n < 2 ){` |
|       ! 0 |  1233 | `		return 0;` |
|         - |  1234 | `	}` |
|    122594 |  1235 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|    122594 |  1236 | `	return &aInstr[n - 2];` |
|     61216 |  1237 | `}` |
|         - |  1238 | `/*` |
|         - |  1239 | ` * Allocate a new virtual machine frame.` |
|         - |  1240 | ` */` |
|   3837174 |  1241 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  1242 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  1243 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  1244 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  1245 | `	)` |
|         5 |  1246 | `{` |
|         - |  1247 | `	VmFrame *pFrame;` |
|         - |  1248 | `	/* Allocate a new vm frame */` |
|   3837179 |  1249 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3837179 |  1250 | `	if( pFrame == 0 ){` |
|       ! 0 |  1251 | `		return 0;` |
|         - |  1252 | `	}` |
|         - |  1253 | `	/* Zero the structure */` |
|   3837179 |  1254 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  1255 | `	/* Initialize frame fields */` |
|   3837179 |  1256 | `	pFrame->pUserData = pUserData;` |
|   3837179 |  1257 | `	pFrame->pThis = pThis;` |
|   3837179 |  1258 | `	pFrame->pVm = pVm;` |
|   3837179 |  1259 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3837179 |  1260 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3837179 |  1261 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3837179 |  1262 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3837179 |  1263 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  1264 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  1265 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3837179 |  1266 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3837179 |  1267 | `	return pFrame;` |
|   1918642 |  1268 | `}` |
|         - |  1269 | `/* Forward declaration */` |
|         - |  1270 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  1271 | `/*` |
|         - |  1272 | ` * The file the code RUNNING RIGHT NOW is written in -- what php would call the` |
|         - |  1273 | ` * executing op array's filename, and what a trace frame records as its call site.` |
|         - |  1274 | ` *` |
|         - |  1275 | ` * Three answers, in order. An include/require/eval started from the current frame` |
|         - |  1276 | ` * means that unit's own top-level code is what is running, so the include stack's` |
|         - |  1277 | ` * top is the file (a frame is SHARED with the unit it includes: php gives the unit` |
|         - |  1278 | ` * an op array of its own, this engine does not). Otherwise it is the defining file` |
|         - |  1279 | ` * of the function whose frame this is -- the include stack is no help there, since` |
|         - |  1280 | ` * a call chain spanning files leaves it pointing at the outermost unit. And for` |
|         - |  1281 | ` * top-level code with no function at all, the include stack's top again.` |
|         - |  1282 | ` *` |
|         - |  1283 | `` * A `try` block pushes a frame of its own carrying no function, so the search for`` |
|         - |  1284 | ` * the running function looks past those.` |
|         - |  1285 | ` */` |
|   5400121 |  1286 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm)` |
|         5 |  1287 | `{` |
|   5400126 |  1288 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - |  1289 | `	sxu32 nInc;` |
|  12974861 |  1290 | `	while( pFrame && pFrame->pParent` |
|  10274648 |  1291 | `	    && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|   2751099 |  1292 | `		pFrame = pFrame->pParent;` |
|         5 |  1293 | `	}` |
|   5400126 |  1294 | `	nInc = SySetUsed(&pVm->aIncFrame);` |
|   5400126 |  1295 | `	if( nInc > 0 ){` |
|    316949 |  1296 | `		VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|    316949 |  1297 | `		if( pInc && pInc->pFrame == (void *)pFrame ){` |
|    185643 |  1298 | `			return (SyString *)SySetPeek(&pVm->aFiles);` |
|         - |  1299 | `		}` |
|     65653 |  1300 | `	}` |
|   5214488 |  1301 | `	if( pFrame && pFrame->pUserData ){` |
|   1494980 |  1302 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   1494980 |  1303 | `		if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   1493621 |  1304 | `			return &pFunc->sFile;` |
|         - |  1305 | `		}` |
|       679 |  1306 | `	}` |
|   3720872 |  1307 | `	return (SyString *)SySetPeek(&pVm->aFiles);` |
|   2700042 |  1308 | `}` |
|         - |  1309 | `/*` |
|         - |  1310 | ` * The name a diagnostic raised from inside this builtin belongs to, php's way:` |
|         - |  1311 | ` * the prelude builtin under whose body it is running when there is one, and` |
|         - |  1312 | `` * otherwise the builtin's own name. `zBuf` is scratch the caller owns -- a`` |
|         - |  1313 | ` * compiled function's name aliases the chunk it was parsed from and is not` |
|         - |  1314 | `` * NUL-terminated, and every one of these messages is built with `%s`.`` |
|         - |  1315 | ` *` |
|         - |  1316 | ` * See PH7_VmPreludeBuiltinFrame for why the outer name is the right one.` |
|         - |  1317 | ` */` |
|       502 |  1318 | `PH7_PRIVATE const char * PH7_CtxDiagFuncName(ph7_context *pCtx,char *zBuf,int nBuf)` |
|         5 |  1319 | `{` |
|       507 |  1320 | `	ph7_vm_func *pFunc = PH7_VmPreludeBuiltinFrame(pCtx->pVm,0,0);` |
|         - |  1321 | `	int nName;` |
|       507 |  1322 | `	if( pFunc == 0 ){` |
|       481 |  1323 | `		return ph7_function_name(pCtx);` |
|         - |  1324 | `	}` |
|        28 |  1325 | `	nName = (int)SyStringLength(&pFunc->sName);` |
|        28 |  1326 | `	if( nName < 1 \|\| nName >= nBuf ){` |
|       ! 0 |  1327 | `		return ph7_function_name(pCtx);` |
|         - |  1328 | `	}` |
|        28 |  1329 | `	SyMemcpy(SyStringData(&pFunc->sName),zBuf,(sxu32)nName);` |
|        28 |  1330 | `	zBuf[nName] = 0;` |
|        28 |  1331 | `	return zBuf;` |
|       255 |  1332 | `}` |
|         - |  1333 | `/*` |
|         - |  1334 | ` * The prelude builtin whose body is RUNNING, or 0 -- and the call site the` |
|         - |  1335 | ` * program wrote for it.` |
|         - |  1336 | ` *` |
|         - |  1337 | ` * ~24 builtins (scandir, glob, tempnam, tmpfile, hex2bin, checkdate, ...) are` |
|         - |  1338 | ` * written as embedded PHP in the builtin chunk, so each one runs in a VM frame` |
|         - |  1339 | ` * of its own. php has no such frame: every one of them is an INTERNAL function` |
|         - |  1340 | ` * there, whose C body is not PHP code and carries no line at all. So everything` |
|         - |  1341 | ` * a diagnostic asks about position has to be answered from the frame BELOW --` |
|         - |  1342 | ` * the file and line of the call the program wrote -- and the builtin has to` |
|         - |  1343 | ` * name ITSELF rather than whichever host builtin it reached for.` |
|         - |  1344 | ` *` |
|         - |  1345 | ` * Neither held. The whole chunk is one source line, so every diagnostic raised` |
|         - |  1346 | `` * anywhere under one of these reported `on line 1`, and so did the getFile()/`` |
|         - |  1347 | `` * getLine() of every exception they throw: `scandir('')` said line 1 where php`` |
|         - |  1348 | `` * says the caller's. And scandir()'s failed open said `opendir(/nope): Failed`` |
|         - |  1349 | `` * to open directory` where php says `scandir(...)`, because the message names`` |
|         - |  1350 | ` * the host builtin that raised it.` |
|         - |  1351 | ` *` |
|         - |  1352 | `` * The walk skips `try`/`catch` frames, which carry no function, and stops at`` |
|         - |  1353 | ` * the first frame that is not a prelude builtin -- a user callback reached from` |
|         - |  1354 | ` * one is ordinary user code and keeps its own position. It walks past a RUN of` |
|         - |  1355 | ` * them (a prelude builtin calling another) so the site is always userland's,` |
|         - |  1356 | ` * while the name reported is the innermost, which is the internal function php` |
|         - |  1357 | ` * would have been inside.` |
|         - |  1358 | ` *` |
|         - |  1359 | ` * Only plain functions qualify. A method declared in a builtin chunk carries` |
|         - |  1360 | ` * VM_FUNC_INTERNAL too, and so does a native class's method shell, but those` |
|         - |  1361 | ` * have a receiver and php gives several of them real PHP frames.` |
|         - |  1362 | ` */` |
|   1515190 |  1363 | `PH7_PRIVATE ph7_vm_func * PH7_VmPreludeBuiltinFrame(` |
|         - |  1364 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  1365 | `	SyString **ppFile,  /* OUT: file of the call the program wrote, or untouched */` |
|         - |  1366 | `	sxu32 *pnLine       /* OUT: its line, or untouched */` |
|         - |  1367 | `	)` |
|         5 |  1368 | `{` |
|   1515195 |  1369 | `	VmFrame *pFrame = pVm->pFrame;` |
|   1515195 |  1370 | `	ph7_vm_func *pInner = 0;` |
|    757840 |  1371 | `	for(;;){` |
|         - |  1372 | `		ph7_vm_func *pFunc;` |
|   3990457 |  1373 | `		while( pFrame && pFrame->pParent` |
|   3276657 |  1374 | `		    && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|    959494 |  1375 | `			pFrame = pFrame->pParent;` |
|         5 |  1376 | `		}` |
|   1515489 |  1377 | `		if( pFrame == 0 \|\| pFrame->pUserData == 0 ){` |
|    435835 |  1378 | `			break;` |
|         - |  1379 | `		}` |
|    643802 |  1380 | `		pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|    643797 |  1381 | `		if( (pFunc->iFlags & (VM_FUNC_INTERNAL\|VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE))` |
|    321868 |  1382 | `			!= VM_FUNC_INTERNAL ){` |
|    643508 |  1383 | `			break;` |
|         - |  1384 | `		}` |
|       299 |  1385 | `		if( pInner == 0 ){` |
|       299 |  1386 | `			pInner = pFunc;` |
|       147 |  1387 | `		}` |
|       299 |  1388 | `		if( ppFile && SyStringLength(&pFrame->sCallFile) > 0 ){` |
|       209 |  1389 | `			*ppFile = &pFrame->sCallFile;` |
|       102 |  1390 | `		}` |
|       299 |  1391 | `		if( pnLine ){` |
|       209 |  1392 | `			*pnLine = pFrame->nCallLine;` |
|       102 |  1393 | `		}` |
|       299 |  1394 | `		pFrame = pFrame->pParent;` |
|         5 |  1395 | `	}` |
|   1515195 |  1396 | `	return pInner;` |
|         5 |  1397 | `}` |
|         - |  1398 | `/* Defined with the variable-slot machinery below; VmEnterFrame is what arms it. */` |
|         - |  1399 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName);` |
|         - |  1400 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc);` |
|         - |  1401 | `/*` |
|         - |  1402 | ` * Enter a VM frame.` |
|         - |  1403 | ` */` |
|   3835882 |  1404 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  1405 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  1406 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  1407 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  1408 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  1409 | `	)` |
|         5 |  1410 | `{` |
|         - |  1411 | `	VmFrame *pFrame;` |
|         - |  1412 | `	/* Allocate a new frame */` |
|   3835887 |  1413 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3835887 |  1414 | `	if( pFrame == 0 ){` |
|       ! 0 |  1415 | `		return SXERR_MEM;` |
|         - |  1416 | `	}` |
|   3835887 |  1417 | `	pFrame->pSelfClass = pThis ? pThis->pClass : 0; /* the caller overwrites it for a static call */` |
|   3835887 |  1418 | `	if( pUserData ){` |
|         - |  1419 | `		/* A function frame runs a body whose variables can be numbered; do it once,` |
|         - |  1420 | `		 * here, so every push site inherits it (the OP_CALL trampoline, a generator` |
|         - |  1421 | `		 * or fiber resume, a closure, an engine-dispatched magic method). A frame` |
|         - |  1422 | `		 * with no function -- the global one, a try's -- leaves pCodeBase 0 and its` |
|         - |  1423 | `		 * variables take the hash path. */` |
|    890199 |  1424 | `		VmFrameNumberBody(pFrame,(ph7_vm_func *)pUserData);` |
|    445289 |  1425 | `	}` |
|         - |  1426 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3835887 |  1427 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  1428 | `	{` |
|         - |  1429 | `		/* ...and the file that line is in, which has to be read NOW: the include` |
|         - |  1430 | `		 * stack has moved on by the time a backtrace is taken. */` |
|   3835887 |  1431 | `		SyString *pCallFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|   3835887 |  1432 | `		if( pCallFile ){` |
|   3827442 |  1433 | `			pFrame->sCallFile = *pCallFile;` |
|   1913774 |  1434 | `		}` |
|         - |  1435 | `	}` |
|         - |  1436 | `	/* Link to the list of active VM frame */` |
|   3835887 |  1437 | `	pFrame->pParent = pVm->pFrame;` |
|   3835887 |  1438 | `	pVm->pFrame = pFrame;` |
|   3835887 |  1439 | `	if( ppFrame ){` |
|         - |  1440 | `		/* Write a pointer to the new VM frame */` |
|   3827426 |  1441 | `		*ppFrame = pFrame;` |
|   1913766 |  1442 | `	}` |
|   3835887 |  1443 | `	return SXRET_OK;` |
|   1917996 |  1444 | `}` |
|         - |  1445 | `/*` |
|         - |  1446 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  1447 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  1448 | ` * information.` |
|         - |  1449 | ` */` |
|      2640 |  1450 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  1451 | `{` |
|         - |  1452 | `	VmFrame *pTarget,*pGlobal;` |
|         - |  1453 | `	SyHashEntry *pEntry;` |
|         - |  1454 | `	sxi32 rc;` |
|      2645 |  1455 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - |  1456 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - |  1457 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - |  1458 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - |  1459 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - |  1460 | `	 * writes never reached the real global. */` |
|      2645 |  1461 | `	pGlobal = pTarget;` |
|      6229 |  1462 | `	while( pGlobal->pParent ){` |
|      3589 |  1463 | `		pGlobal = pGlobal->pParent;` |
|         5 |  1464 | `	}` |
|      2645 |  1465 | `	if( pGlobal == pTarget ){` |
|         - |  1466 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 |  1467 | `		return SXRET_OK;` |
|         - |  1468 | `	}` |
|         - |  1469 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - |  1470 | `	 * plain global that would shadow it. */` |
|      2645 |  1471 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|      2645 |  1472 | `	if( pEntry == 0 ){` |
|      2643 |  1473 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|      1317 |  1474 | `	}` |
|      2645 |  1475 | `	if( pEntry == 0 ){` |
|         - |  1476 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - |  1477 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - |  1478 | `		 * the assignment went to a local nobody could read. */` |
|        14 |  1479 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        14 |  1480 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  1481 | `			return rc;` |
|         - |  1482 | `		}` |
|        14 |  1483 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        14 |  1484 | `		if( pEntry == 0 ){` |
|       ! 0 |  1485 | `			return SXERR_NOTFOUND;` |
|         - |  1486 | `		}` |
|         6 |  1487 | `	}` |
|         - |  1488 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - |  1489 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|      3963 |  1490 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|      2640 |  1491 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|      2645 |  1492 | `	return SXRET_OK;` |
|      1323 |  1493 | `}` |
|         - |  1494 | `/*` |
|         - |  1495 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  1496 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  1497 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  1498 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  1499 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  1500 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  1501 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  1502 | ` */` |
|   3828687 |  1503 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  1504 | `{` |
|   3828692 |  1505 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  1506 | `		VmClearResumeTarget(&(*pVm));` |
|       ! 0 |  1507 | `	}` |
|   3828692 |  1508 | `}` |
|         - |  1509 | `/*` |
|         - |  1510 | ` * The four resume fields are ONE record: a frame, the landing pad inside it, the` |
|         - |  1511 | ` * bytecode array that pad indexes, and the operand-stack base to drain to. They` |
|         - |  1512 | ` * were written together but cleared, saved and restored INDIVIDUALLY (only the` |
|         - |  1513 | ` * frame), so a live frame could end up paired with a dead try's pad and depth —` |
|         - |  1514 | ` * which drains the operand stack to a foreign base and lands mid-statement, one` |
|         - |  1515 | ` * slot below the stack. These four functions are the only writers.` |
|         - |  1516 | ` */` |
|   4318710 |  1517 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth)` |
|         5 |  1518 | `{` |
|   4318715 |  1519 | `	pVm->pResumeFrame = pFrame;` |
|   4318715 |  1520 | `	pVm->iResumePc = iPc;` |
|   4318715 |  1521 | `	pVm->pResumeInstr = pInstr;` |
|   4318715 |  1522 | `	pVm->iResumeStackDepth = iStackDepth;` |
|   4318715 |  1523 | `}` |
|   2948516 |  1524 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm)` |
|         5 |  1525 | `{` |
|   2948521 |  1526 | `	VmSetResumeTarget(&(*pVm),0,0,0,0);` |
|   2948521 |  1527 | `}` |
|   1397067 |  1528 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave)` |
|         5 |  1529 | `{` |
|   1397072 |  1530 | `	pSave->pFrame = pVm->pResumeFrame;` |
|   1397072 |  1531 | `	pSave->iPc = pVm->iResumePc;` |
|   1397072 |  1532 | `	pSave->pInstr = pVm->pResumeInstr;` |
|   1397072 |  1533 | `	pSave->iStackDepth = pVm->iResumeStackDepth;` |
|   1397072 |  1534 | `}` |
|      1503 |  1535 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave)` |
|         5 |  1536 | `{` |
|      1508 |  1537 | `	VmSetResumeTarget(&(*pVm),pSave->pFrame,pSave->iPc,pSave->pInstr,pSave->iStackDepth);` |
|      1508 |  1538 | `}` |
|         - |  1539 | `/*` |
|         - |  1540 | ` * Leave the top-most active frame.` |
|         - |  1541 | ` */` |
|   3827397 |  1542 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  1543 | `{` |
|   3827402 |  1544 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3827402 |  1545 | `	if( pCurFrame ){` |
|         - |  1546 | `		/* Unlink from the list of active VM frame */` |
|   3827402 |  1547 | `		pVm->pFrame = pCurFrame->pParent;` |
|         - |  1548 | `		/* End the foreach walks this activation never finished, before its locals go:` |
|         - |  1549 | `		 * a step retains its subject, and an object walk holds a cursor registered on` |
|         - |  1550 | `		 * the instance. */` |
|   3827402 |  1551 | `		VmReleaseFrameForeachSteps(&(*pVm),pCurFrame);` |
|   3827402 |  1552 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  1553 | `			VmSlot  *aSlot;` |
|         - |  1554 | `			sxu32 n;` |
|         - |  1555 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - |  1556 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - |  1557 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - |  1558 | `			 * is about to free). */` |
|    890193 |  1559 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   2464687 |  1560 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1574499 |  1561 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    790342 |  1562 | `			}` |
|         - |  1563 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    890193 |  1564 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   2460431 |  1565 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1570243 |  1566 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - |  1567 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - |  1568 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - |  1569 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - |  1570 | `					 * down the slot and the reference table took the holders with it, so` |
|         - |  1571 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - |  1572 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        33 |  1573 | `					continue;` |
|         - |  1574 | `				}` |
|         - |  1575 | `				/* Unset the local variable */` |
|   1570213 |  1576 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    788201 |  1577 | `			}` |
|    445286 |  1578 | `		}` |
|         - |  1579 | `		/* Release internal containers */` |
|   3827402 |  1580 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3827402 |  1581 | `		SySetRelease(&pCurFrame->sArg);` |
|   3827402 |  1582 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3827402 |  1583 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  1584 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  1585 | `		 * containers above — released for every frame, including transparent` |
|         - |  1586 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3827402 |  1587 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  1588 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3827402 |  1589 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  1590 | `		/* This activation no longer needs the function it was running. For a` |
|         - |  1591 | `		 * run-time closure that is one of the two holds on its per-instantiation` |
|         - |  1592 | `		 * copy -- the other is the Closure object -- and the copy goes when both` |
|         - |  1593 | `		 * are gone. pUserData is a ph7_vm_func for a user-function frame and 0 for` |
|         - |  1594 | `		 * every other kind (the global frame, an exception wrapper, a local exec). */` |
|   3827402 |  1595 | `		if( pCurFrame->pUserData ){` |
|    890193 |  1596 | `			PH7_VmClosureFuncUnref(&(*pVm),(ph7_vm_func *)pCurFrame->pUserData);` |
|    445286 |  1597 | `		}` |
|         - |  1598 | `		/* Release the whole structure */` |
|   3827402 |  1599 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1913754 |  1600 | `	}` |
|   3827402 |  1601 | `}` |
|         - |  1602 | `/*` |
|         - |  1603 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  1604 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  1605 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  1606 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  1607 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  1608 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  1609 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  1610 | ` */` |
|         - |  1611 | `/*` |
|         - |  1612 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  1613 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  1614 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  1615 | ` *` |
|         - |  1616 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  1617 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  1618 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  1619 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  1620 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  1621 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  1622 | ` */` |
|  16722663 |  1623 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  1624 | `{` |
|         - |  1625 | `	VmFrame *pFrame;` |
|  43049038 |  1626 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  26327859 |  1627 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  1628 | `		sxu32 n;` |
|  61742207 |  1629 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  35415837 |  1630 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  1631 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|      1488 |  1632 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|      1488 |  1633 | `				(void)SySetPop(&pFrame->sLocal);` |
|      1488 |  1634 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  1635 | `			}` |
|  17705382 |  1636 | `		}` |
|  13159550 |  1637 | `	}` |
|  16721184 |  1638 | `	return FALSE;` |
|   8358125 |  1639 | `}` |
|         - |  1640 | `/*` |
|         - |  1641 | ` * The superglobal table, asked the cheap question first.` |
|         - |  1642 | ` *` |
|         - |  1643 | ` * Every variable access consults hSuper before the frame -- php resolves $_SERVER` |
|         - |  1644 | ` * the same in every scope, so the order is the semantics and cannot change -- and` |
|         - |  1645 | ` * for the ~9 names that are superglobals ($GLOBALS and the $_* set) the answer is` |
|         - |  1646 | ` * no. Hashing a whole variable name to learn that was, measured on the ecosystem` |
|         - |  1647 | ` * gate's phpcs step, 101M of the engine's 325M hash-table lookups.` |
|         - |  1648 | ` *` |
|         - |  1649 | ` * aSuperFirst is the set of first bytes any INSTALLED superglobal name starts` |
|         - |  1650 | ` * with, so a name whose first byte is not in it cannot be one and never reaches` |
|         - |  1651 | ` * the table. It is a set and not a fixed 'G'/'_' test because an embedder may` |
|         - |  1652 | ` * install a superglobal of its own (PH7_VM_CONFIG_CREATE_SUPER).` |
|         - |  1653 | ` */` |
|  98019807 |  1654 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 |  1655 | `{` |
|         - |  1656 | `	unsigned char c;` |
|  98019812 |  1657 | `	if( nByte < 1 \|\| zName == 0 ){` |
|         5 |  1658 | `		return 0;` |
|         - |  1659 | `	}` |
|  98019808 |  1660 | `	c = (unsigned char)zName[0];` |
|  98019808 |  1661 | `	if( (pVm->aSuperFirst[c >> 5] & (1u << (c & 31))) == 0 ){` |
|  97613719 |  1662 | `		return 0;` |
|         - |  1663 | `	}` |
|    406094 |  1664 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|  49012430 |  1665 | `}` |
|         - |  1666 | `/* Record a name just installed in hSuper. Every insertion into that table must` |
|         - |  1667 | ` * come through here, or the lookup above stops finding it. */` |
|     77011 |  1668 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 |  1669 | `{` |
|         - |  1670 | `	VmFrame *pFrame;` |
|         - |  1671 | `	unsigned char c;` |
|     77016 |  1672 | `	if( nByte < 1 \|\| zName == 0 ){` |
|       ! 0 |  1673 | `		return;` |
|         - |  1674 | `	}` |
|     77016 |  1675 | `	c = (unsigned char)zName[0];` |
|     77016 |  1676 | `	pVm->aSuperFirst[c >> 5] \|= (1u << (c & 31));` |
|         - |  1677 | `	/* A name the frames may already have memoized as an ordinary variable now` |
|         - |  1678 | `	 * resolves through hSuper instead, and hSuper is consulted FIRST. Installing a` |
|         - |  1679 | `	 * superglobal is a VM-configuration act with only the global frame live, so the` |
|         - |  1680 | `	 * active chain is every frame there is to correct. */` |
|    154027 |  1681 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     77016 |  1682 | `		VmVarMemoFlush(pFrame);` |
|     38450 |  1683 | `	}` |
|     38450 |  1684 | `}` |
|         - |  1685 | `/*` |
|         - |  1686 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  1687 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  1688 | ` * should be skipped when looking for the real execution context.` |
|         - |  1689 | ` */` |
| 247221900 |  1690 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  1691 | `{` |
| 258978771 |  1692 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  11756871 |  1693 | `		pFrame = pFrame->pParent;` |
|         5 |  1694 | `	}` |
| 247221905 |  1695 | `	return pFrame;` |
|         5 |  1696 | `}` |
|         - |  1697 | `/*` |
|         - |  1698 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  1699 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  1700 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  1701 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  1702 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  1703 | ` *` |
|         - |  1704 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  1705 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  1706 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  1707 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  1708 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  1709 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  1710 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  1711 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  1712 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  1713 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  1714 | ` *` |
|         - |  1715 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  1716 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  1717 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  1718 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  1719 | ` */` |
|   1982746 |  1720 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  1721 | `{` |
|   1982751 |  1722 | `	if( pVm->pResumeFrame == 0 ){` |
|       152 |  1723 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  1724 | `	}` |
|   1982603 |  1725 | `	if( pEntryFrame == 0 ){` |
|         - |  1726 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  1727 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  1728 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  1729 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  1730 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  1731 | `		 * NULL and the process dies. */` |
|        23 |  1732 | `		return FALSE;` |
|         - |  1733 | `	}` |
|         - |  1734 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  1735 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  1736 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  1737 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  1738 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  1739 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  1740 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  1741 | `	 * exec's resume site match and land. */` |
|   1982578 |  1742 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1677863 |  1743 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1370952 |  1744 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  1745 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  1746 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  1747 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  1748 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  1749 | `		 * never match a real frame. */` |
|    613892 |  1750 | `		return FALSE;` |
|         - |  1751 | `	}` |
|         - |  1752 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  1753 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  1754 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  1755 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  1756 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  1757 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  1758 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  1759 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  1760 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  1761 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  1762 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  1763 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  1764 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  1765 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  1766 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  1767 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  1768 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  1769 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  1770 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  1771 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  1772 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly.` |
|         - |  1773 | `	 *` |
|         - |  1774 | `	 * The record is CONSUMED FIRST — snapshotted whole and cleared — because the pop` |
|         - |  1775 | `	 * loop below runs USER CODE: leaving a frame releases its locals, and a local's` |
|         - |  1776 | `	 * last reference dying runs that object's __destruct(). A destructor allocates,` |
|         - |  1777 | `	 * calls, and may throw; a throw re-enters VmThrowException, whose first act is to` |
|         - |  1778 | `	 * invalidate the in-flight resume record. Reading pVm->iResumePc AFTER the loop` |
|         - |  1779 | ``	 * therefore read a ZERO the destructor had left behind, and `iResumePc - 1` handed`` |
|         - |  1780 | `	 * the dispatcher -1, which its pc++ turned into a re-run of the whole body from` |
|         - |  1781 | `	 * index 0: monolog's suite restarted its top-level script forever. The loop's own` |
|         - |  1782 | `	 * landing-pad test has to read the snapshot for the same reason. */` |
|         - |  1783 | `	{` |
|         - |  1784 | `		VmResumeTarget sTarget;` |
|   1368696 |  1785 | `		VmSaveResumeTarget(&(*pVm),&sTarget);` |
|   1368696 |  1786 | `		VmClearResumeTarget(&(*pVm)); /* one-shot consume: the whole record, before any teardown */` |
|   2259054 |  1787 | `		while( pVm->pFrame != pEntryFrame` |
|   2515201 |  1788 | `		    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1624782 |  1789 | `		         && pVm->pFrame->iExceptionJump == sTarget.iPc) ){` |
|    412146 |  1790 | `			VmLeaveFrame(&(*pVm));` |
|         5 |  1791 | `		}` |
|   1368696 |  1792 | `		*pResumePc = (sxi32)sTarget.iPc - 1;` |
|         - |  1793 | `	}` |
|         - |  1794 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  1795 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  1796 | `	 * point router must not re-fire it after this resume. */` |
|   1368696 |  1797 | `	pVm->nBoundaryRc = 0;` |
|   1368696 |  1798 | `	return TRUE;` |
|    991323 |  1799 | `}` |
|         - |  1800 | `/*` |
|         - |  1801 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  1802 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  1803 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  1804 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  1805 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  1806 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  1807 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  1808 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  1809 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  1810 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  1811 | ` */` |
|         - |  1812 | `/*` |
|         - |  1813 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  1814 | ` *` |
|         - |  1815 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  1816 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  1817 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  1818 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  1819 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  1820 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  1821 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  1822 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  1823 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  1824 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  1825 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  1826 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  1827 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  1828 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  1829 | ` * activation.` |
|         - |  1830 | ` */` |
|   1488350 |  1831 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  1832 | `{` |
|   1488355 |  1833 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1488355 |  1834 | `	if( pClone == 0 ){` |
|       ! 0 |  1835 | `		return 0;` |
|         - |  1836 | `	}` |
|   1488355 |  1837 | `	*pClone = *pCompiled;` |
|   1488355 |  1838 | `	pClone->pCompiled = pCompiled;` |
|   1488355 |  1839 | `	pClone->iFinallyDone = 0;` |
|   1488355 |  1840 | `	pClone->iInCatch = 0;` |
|   1488355 |  1841 | `	pClone->pInflight = 0;` |
|   1488355 |  1842 | `	pClone->pFrame = 0;` |
|   1488355 |  1843 | `	return pClone;` |
|    744071 |  1844 | `}` |
|   2957959 |  1845 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  1846 | `{` |
|   2957964 |  1847 | `	if( pExc && pExc->pCompiled ){` |
|         - |  1848 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  1849 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  1850 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  1851 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1488341 |  1852 | `		if( pExc->pInflight ){` |
|       ! 0 |  1853 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 |  1854 | `			pExc->pInflight = 0;` |
|       ! 0 |  1855 | `		}` |
|   1488341 |  1856 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    744059 |  1857 | `	}` |
|   2957964 |  1858 | `}` |
|         - |  1859 | `/*` |
|         - |  1860 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - |  1861 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - |  1862 | ` */` |
|     13295 |  1863 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 |  1864 | `{` |
|     13300 |  1865 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 |  1866 | `}` |
|         - |  1867 | `/*` |
|         - |  1868 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - |  1869 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - |  1870 | ` * handlers). The set itself is reset by the caller.` |
|         - |  1871 | ` */` |
|    130321 |  1872 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 |  1873 | `{` |
|    130326 |  1874 | `	sxu32 n = SySetUsed(pSet);` |
|    130326 |  1875 | `	if( n > 0 ){` |
|       ! 0 |  1876 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - |  1877 | `		sxu32 i;` |
|       ! 0 |  1878 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 |  1879 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 |  1880 | `		}` |
|       ! 0 |  1881 | `	}` |
|    130326 |  1882 | `}` |
|         - |  1883 | `/*` |
|         - |  1884 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - |  1885 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - |  1886 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - |  1887 | ` * down a try belonging to the caller.` |
|         - |  1888 | ` */` |
|        18 |  1889 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         4 |  1890 | `{` |
|        22 |  1891 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        22 |  1892 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        22 |  1893 | `	if( nBase < nFloor ){` |
|       ! 0 |  1894 | `		nBase = nFloor;` |
|       ! 0 |  1895 | `	}` |
|        22 |  1896 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         4 |  1897 | `}` |
|         - |  1898 | `/*` |
|         - |  1899 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - |  1900 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - |  1901 | ` * only carries the compiled pointer.` |
|         - |  1902 | ` */` |
|        88 |  1903 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  1904 | `{` |
|        93 |  1905 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        93 |  1906 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        93 |  1907 | `	while( n > 0 ){` |
|        93 |  1908 | `		n--;` |
|        93 |  1909 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        93 |  1910 | `			return ap[n];` |
|         - |  1911 | `		}` |
|       ! 0 |  1912 | `	}` |
|       ! 0 |  1913 | `	return 0;` |
|        49 |  1914 | `}` |
|   4560796 |  1915 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 |  1916 | `{` |
|         - |  1917 | `	sxu32 nUsed;` |
|   4560801 |  1918 | `	sxi32 rcOut = SXRET_OK;` |
|   4567147 |  1919 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|      6351 |  1920 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      6351 |  1921 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|      6351 |  1922 | `		(void)SySetPop(&pVm->aException);` |
|      6351 |  1923 | `		pExc->pFrame = 0;` |
|         - |  1924 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - |  1925 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - |  1926 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - |  1927 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - |  1928 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|      6351 |  1929 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      6351 |  1930 | `			VmLeaveFrame(&(*pVm));` |
|      3173 |  1931 | `		}` |
|      6385 |  1932 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - |  1933 | `			sxi32 rcF;` |
|        73 |  1934 | `			pExc->iFinallyDone = 1;` |
|        73 |  1935 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        73 |  1936 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        73 |  1937 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 |  1938 | `				return SXERR_ABORT;` |
|         - |  1939 | `			}` |
|        73 |  1940 | `			if( rcF == PH7_EXCEPTION ){` |
|         - |  1941 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - |  1942 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - |  1943 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 |  1944 | `				rcOut = PH7_EXCEPTION;` |
|         2 |  1945 | `			}` |
|        39 |  1946 | `		}else{` |
|      6283 |  1947 | `			VmExcRelease(&(*pVm),pExc);` |
|         - |  1948 | `		}` |
|         5 |  1949 | `	}` |
|   4560801 |  1950 | `	return rcOut;` |
|   2280360 |  1951 | `}` |
|         - |  1952 | `/*` |
|         - |  1953 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - |  1954 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - |  1955 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - |  1956 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - |  1957 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - |  1958 | ` */` |
|   3757268 |  1959 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 |  1960 | `{` |
|   3757273 |  1961 | `	pFrame->bHasRet = 0;` |
|   3757273 |  1962 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   3757273 |  1963 | `	pFrame->nCatchJmpPc = 0;` |
|   3757273 |  1964 | `}` |
|         - |  1965 | `/*` |
|         - |  1966 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - |  1967 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - |  1968 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - |  1969 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - |  1970 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - |  1971 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - |  1972 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - |  1973 | ` */` |
|     24296 |  1974 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 |  1975 | `{` |
|     24301 |  1976 | `	if( pResult ){` |
|     24301 |  1977 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     12148 |  1978 | `	}` |
|     24301 |  1979 | `	VmClearFramePending(pEntryFrame);` |
|     24301 |  1980 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|       ! 0 |  1981 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 |  1982 | `	}` |
|     24301 |  1983 | `}` |
|         - |  1984 | `/*` |
|         - |  1985 | ` * Compare two functions signature and return the comparison result.` |
|         - |  1986 | ` */` |
|         4 |  1987 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         1 |  1988 | `{` |
|         5 |  1989 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|         5 |  1990 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|         5 |  1991 | `	const char *zSin = pSecond->zString;` |
|         5 |  1992 | `	const char *zFin = pFirst->zString;` |
|         5 |  1993 | `	const char *zPtr = zFin;` |
|         2 |  1994 | `	for(;;){` |
|         5 |  1995 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|         3 |  1996 | `			break;` |
|         - |  1997 | `		}` |
|       ! 0 |  1998 | `		if( zFin[0] != zSin[0] ){` |
|         - |  1999 | `			/* mismatch */` |
|       ! 0 |  2000 | `			break;` |
|         - |  2001 | `		}` |
|       ! 0 |  2002 | `		zFin++;` |
|       ! 0 |  2003 | `		zSin++;` |
|       ! 0 |  2004 | `	}` |
|         5 |  2005 | `	return (int)(zFin-zPtr);` |
|         1 |  2006 | `}` |
|         - |  2007 | `/*` |
|         - |  2008 | ` * Select the appropriate VM function for the current call context.` |
|         - |  2009 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - |  2010 | ` * introduced by the version 2 of the PH7 engine.` |
|         - |  2011 | ` * Refer to the official documentation for more information.` |
|         - |  2012 | ` */` |
|        72 |  2013 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - |  2014 | `	ph7_vm *pVm,         /* Target VM */` |
|         - |  2015 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - |  2016 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - |  2017 | `	int nArg             /* Total number of passed arguments  */` |
|         - |  2018 | `	)` |
|         4 |  2019 | `{` |
|         - |  2020 | `	int iTarget,i,j,iCur,iMax;` |
|         - |  2021 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - |  2022 | `	ph7_vm_func *pLink;` |
|         - |  2023 | `	SyString sArgSig;` |
|         - |  2024 | `	SyBlob sSig;` |
|         - |  2025 |  |
|        76 |  2026 | `	pLink = pList;` |
|        76 |  2027 | `	i = 0;` |
|         - |  2028 | `	/* Put functions expecting the same number of passed arguments */` |
|       636 |  2029 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|       584 |  2030 | `		if( pLink == 0 ){` |
|        23 |  2031 | `			break;` |
|         - |  2032 | `		}` |
|       564 |  2033 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - |  2034 | `			/* Candidate for overloading */` |
|       564 |  2035 | `			apSet[i++] = pLink;` |
|       280 |  2036 | `		}` |
|         - |  2037 | `		/* Point to the next entry */` |
|       564 |  2038 | `		pLink = pLink->pNextName;` |
|         4 |  2039 | `	}` |
|        76 |  2040 | `	if( i < 1 ){` |
|         - |  2041 | `		/* No candidates,return the head of the list */` |
|       ! 0 |  2042 | `		return pList;` |
|         - |  2043 | `	}` |
|        76 |  2044 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - |  2045 | `		/* Return the only candidate */` |
|        74 |  2046 | `		return apSet[0];` |
|         - |  2047 | `	}` |
|         - |  2048 | `	/* Calculate function signature */` |
|         3 |  2049 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|         5 |  2050 | `	for( j = 0 ; j < nArg ; j++ ){` |
|         3 |  2051 | `		int c = 'n'; /* null */` |
|         3 |  2052 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - |  2053 | `			/* Hashmap */` |
|       ! 0 |  2054 | `			c = 'h';` |
|         3 |  2055 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - |  2056 | `			/* bool */` |
|       ! 0 |  2057 | `			c = 'b';` |
|         3 |  2058 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - |  2059 | `			/* int */` |
|         3 |  2060 | `			c = 'i';` |
|         1 |  2061 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - |  2062 | `			/* String */` |
|       ! 0 |  2063 | `			c = 's';` |
|       ! 0 |  2064 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - |  2065 | `			/* Float */` |
|       ! 0 |  2066 | `			c = 'f';` |
|       ! 0 |  2067 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - |  2068 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 |  2069 | `			int marker = 'o';` |
|       ! 0 |  2070 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 |  2071 | `			SyString *pName = &pClass->sName;` |
|       ! 0 |  2072 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 |  2073 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 |  2074 | `			c = -1;` |
|       ! 0 |  2075 | `		}` |
|         3 |  2076 | `		if( c > 0 ){` |
|         3 |  2077 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         1 |  2078 | `		}` |
|         2 |  2079 | `	}` |
|         3 |  2080 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|         3 |  2081 | `	iTarget = 0;` |
|         3 |  2082 | `	iMax = -1;` |
|         - |  2083 | `	/* Select the appropriate function */` |
|         7 |  2084 | `	for( j = 0 ; j < i ; j++ ){` |
|         - |  2085 | `		/* Compare the two signatures */` |
|         5 |  2086 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|         5 |  2087 | `		if( iCur > iMax ){` |
|         3 |  2088 | `			iMax = iCur;` |
|         3 |  2089 | `			iTarget = j;` |
|         1 |  2090 | `		}` |
|         3 |  2091 | `	}` |
|         3 |  2092 | `	SyBlobRelease(&sSig);` |
|         - |  2093 | `	/* Appropriate function for the current call context */` |
|         3 |  2094 | `	return apSet[iTarget];` |
|        40 |  2095 | `}` |
|         - |  2096 | `/* Forward declaration */` |
|         - |  2097 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - |  2098 | `/*` |
|         - |  2099 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot.` |
|         - |  2100 | ` *` |
|         - |  2101 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - |  2102 | ` * end-of-exec. When the pool was one doubling buffer, an initializer that` |
|         - |  2103 | ` * allocated pool memobjs — a large array literal reserves one per element —` |
|         - |  2104 | ` * reallocated and FREED that buffer, so a pResult pointing into it dangled and` |
|         - |  2105 | ` * the final store was a heap use-after-free (confirmed via ASan on a >=~227` |
|         - |  2106 | ` * element class-const array; it is what blocked Composer's autoload class-map).` |
|         - |  2107 | ` * The answer was to evaluate into a stable local and store into the slot` |
|         - |  2108 | ` * re-fetched by its index, which is what this does. Redundant since P1 -- the` |
|         - |  2109 | ` * pool's segments are fixed, so a slot's address never moves. Left for the` |
|         - |  2110 | ` * harvest sweep; the history above is why it was ever needed.` |
|         - |  2111 | ` *` |
|         - |  2112 | ` * On return *ppMemObj points at the valid post-eval slot. PH7_MemObjStore` |
|         - |  2113 | ` * preserves the destination slot's nIdx (excluded from its memcpy), so the slot` |
|         - |  2114 | ` * identity is kept. Mirrors the enum-case backing path.` |
|         - |  2115 | ` */` |
|      4698 |  2116 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 |  2117 | `{` |
|         - |  2118 | `	ph7_value sVal;` |
|      4703 |  2119 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - |  2120 | `	sxi32 rc;` |
|      4703 |  2121 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      4703 |  2122 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - |  2123 | `	/* Re-fetch by the reserved index (redundant since P1: see above). */` |
|      4703 |  2124 | `	*ppMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|      4703 |  2125 | `	if( *ppMemObj ){` |
|      4703 |  2126 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      2349 |  2127 | `	}` |
|      4703 |  2128 | `	PH7_MemObjRelease(&sVal);` |
|      4703 |  2129 | `	return rc;` |
|         5 |  2130 | `}` |
|         - |  2131 | `/*` |
|         - |  2132 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - |  2133 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - |  2134 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - |  2135 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - |  2136 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - |  2137 | ` * C caller ever regains control.` |
|         - |  2138 | ` */` |
|         - |  2139 | `typedef struct VmMuteState {` |
|         - |  2140 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - |  2141 | `	sxu32 nSaved;` |
|         - |  2142 | `	sxi32 iSaveStatus;` |
|         - |  2143 | `	sxi32 iSaveBoundary;` |
|         - |  2144 | `	VmResumeTarget sSaveResume;` |
|         - |  2145 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - |  2146 | `	ph7_class *pSaveCycleClass;` |
|         - |  2147 | `} VmMuteState;` |
|     26953 |  2148 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 |  2149 | `{` |
|     26958 |  2150 | `	pSave->apSaved = 0;` |
|     26958 |  2151 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|     26958 |  2152 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|     26958 |  2153 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|     26958 |  2154 | `	VmSaveResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|     26958 |  2155 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|     26958 |  2156 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|     26958 |  2157 | `	if( pSave->nSaved > 0 ){` |
|       410 |  2158 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       204 |  2159 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|       206 |  2160 | `		if( pSave->apSaved ){` |
|       308 |  2161 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|       204 |  2162 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|       206 |  2163 | `			SySetReset(&pVm->aException);` |
|       102 |  2164 | `		}` |
|       102 |  2165 | `	}` |
|     26958 |  2166 | `	pVm->nMuteThrow++;` |
|     26958 |  2167 | `}` |
|         - |  2168 | `/*` |
|         - |  2169 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - |  2170 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - |  2171 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - |  2172 | ` * TRUE when a throw was actually swallowed.` |
|         - |  2173 | ` */` |
|     26953 |  2174 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 |  2175 | `{` |
|         - |  2176 | `	VmFrame *pFrame;` |
|     26958 |  2177 | `	pVm->nMuteThrow--;` |
|         - |  2178 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - |  2179 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - |  2180 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - |  2181 | `	 * hidden. */` |
|     26958 |  2182 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|     26958 |  2183 | `	SySetReset(&pVm->aException);` |
|     26958 |  2184 | `	if( pSave->apSaved ){` |
|         - |  2185 | `		sxu32 k;` |
|       410 |  2186 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|       206 |  2187 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|       104 |  2188 | `		}` |
|       206 |  2189 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|       206 |  2190 | `		pSave->apSaved = 0;` |
|       102 |  2191 | `	}` |
|     26958 |  2192 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|     26876 |  2193 | `		return FALSE;` |
|         - |  2194 | `	}` |
|        86 |  2195 | `	pFrame = pVm->pFrame;` |
|        86 |  2196 | `	if( pFrame ){` |
|        86 |  2197 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        86 |  2198 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        41 |  2199 | `	}` |
|        86 |  2200 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        86 |  2201 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        86 |  2202 | `	VmRestoreResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|        86 |  2203 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        86 |  2204 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        86 |  2205 | `	return TRUE;` |
|     13464 |  2206 | `}` |
|         - |  2207 | `/*` |
|         - |  2208 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - |  2209 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - |  2210 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - |  2211 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - |  2212 | ` *` |
|         - |  2213 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - |  2214 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - |  2215 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - |  2216 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - |  2217 | ` */` |
|       198 |  2218 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - |  2219 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - |  2220 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - |  2221 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - |  2222 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - |  2223 | `	int nArg,                    /* Argument count */` |
|         - |  2224 | `	ph7_value **apArg,           /* Arguments */` |
|         - |  2225 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - |  2226 | `	)` |
|         1 |  2227 | `{` |
|         - |  2228 | `	VmMuteState sSave;` |
|         - |  2229 | `	sxi32 rc;` |
|       199 |  2230 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       199 |  2231 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       199 |  2232 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         7 |  2233 | `		if( pbThrew ){` |
|         7 |  2234 | `			*pbThrew = TRUE;` |
|         3 |  2235 | `		}` |
|         7 |  2236 | `		return SXRET_OK;` |
|         - |  2237 | `	}` |
|       193 |  2238 | `	if( pbThrew ){` |
|       193 |  2239 | `		*pbThrew = FALSE;` |
|        96 |  2240 | `	}` |
|       193 |  2241 | `	return rc;` |
|       100 |  2242 | `}` |
|         - |  2243 | `/*` |
|         - |  2244 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - |  2245 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - |  2246 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - |  2247 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - |  2248 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - |  2249 | ` *` |
|         - |  2250 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - |  2251 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - |  2252 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - |  2253 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - |  2254 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - |  2255 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - |  2256 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - |  2257 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - |  2258 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - |  2259 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - |  2260 | ` */` |
|       472 |  2261 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 |  2262 | `{` |
|         - |  2263 | `	VmMuteState sSave;` |
|         - |  2264 | `	sxi32 rc;` |
|       477 |  2265 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       477 |  2266 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       477 |  2267 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - |  2268 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - |  2269 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - |  2270 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - |  2271 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - |  2272 | `		 * records the cycle again and raises it there. */` |
|       ! 0 |  2273 | `		rc = PH7_EXCEPTION;` |
|       ! 0 |  2274 | `	}` |
|         - |  2275 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - |  2276 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - |  2277 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - |  2278 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - |  2279 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - |  2280 | `	 * detects the cycle again. */` |
|       477 |  2281 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       477 |  2282 | `	return rc;` |
|         5 |  2283 | `}` |
|         - |  2284 | `/*` |
|         - |  2285 | ` * Run a compiled constant expression only to LOOK at the value it produces, and` |
|         - |  2286 | ` * report whether php's own compiler would have FOLDED it.` |
|         - |  2287 | ` *` |
|         - |  2288 | ` * php folds a parameter default at compile time and keeps the folded zval; what` |
|         - |  2289 | `` * it cannot reduce stays an AST and prints as `<expression>` in the declaration`` |
|         - |  2290 | ` * php renders for an incompatible-override fatal (see PH7_ClassRenderDecl).` |
|         - |  2291 | ``  * "Cannot reduce" is not a syntactic property -- `2 * 1024` folds and `1 / 0` `` |
|         - |  2292 | ` * does not -- so the question is asked by RUNNING the program and watching for` |
|         - |  2293 | ` * anything php's folder would have refused on.` |
|         - |  2294 | ` *` |
|         - |  2295 | ` * The window is doubly sealed, because this runs at CLASS-LINK time: a program` |
|         - |  2296 | ` * php has not reached, in the middle of compiling one it has. VmMuteEnter hides` |
|         - |  2297 | `` * the live try activations and swallows a throw (`1/0`, `"a"+1`), and`` |
|         - |  2298 | ` * nSpeculative drops every diagnostic without running a user error handler or` |
|         - |  2299 | `` * touching error_get_last() (`[1,2][5]`). Either one having happened is exactly`` |
|         - |  2300 | ` * php's "did not fold".` |
|         - |  2301 | ` *` |
|         - |  2302 | ` * Returns TRUE with *pOut holding the value, or FALSE (caller prints` |
|         - |  2303 | `` * `<expression>`). The caller must have SCREENED the program first: only a run`` |
|         - |  2304 | ` * built from literal loads and pure value operators belongs here -- a constant` |
|         - |  2305 | `` * NAME, a class constant and a `new` are all things php keeps unfolded and this`` |
|         - |  2306 | ` * would happily evaluate (or construct).` |
|         - |  2307 | ` */` |
|     26283 |  2308 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut)` |
|         5 |  2309 | `{` |
|         - |  2310 | `	VmMuteState sSave;` |
|         - |  2311 | `	sxu32 nDiag;` |
|         - |  2312 | `	sxi32 rc;` |
|         - |  2313 | `	int bFolded;` |
|     26288 |  2314 | `	int bFramePushed = 0;` |
|     26288 |  2315 | `	if( pVm->pFrame == 0 ){` |
|         - |  2316 | `		/* CLASS-LINK time: the script has not started, so there is no frame at all --` |
|         - |  2317 | `		 * and the reference table, the array builder and the throw path all read one.` |
|         - |  2318 | `		 * Stand a global-shaped frame up for the duration (pParent == 0, so its` |
|         - |  2319 | `		 * teardown is the global frame's: nothing of the caller's is torn down with` |
|         - |  2320 | `		 * it). Without this an array default segfaulted the compiler. */` |
|       ! 0 |  2321 | `		if( VmEnterFrame(&(*pVm),0,0,0) != SXRET_OK ){` |
|       ! 0 |  2322 | `			return 0;` |
|         - |  2323 | `		}` |
|       ! 0 |  2324 | `		bFramePushed = 1;` |
|       ! 0 |  2325 | `	}` |
|     26288 |  2326 | `	VmMuteEnter(&(*pVm),&sSave);` |
|     26288 |  2327 | `	pVm->nSpeculative++;` |
|     26288 |  2328 | `	nDiag = pVm->nSpecDiag;` |
|     26288 |  2329 | `	rc = VmLocalExec(&(*pVm),pByteCode,pOut,FALSE);` |
|     26288 |  2330 | `	pVm->nSpeculative--;` |
|     26288 |  2331 | `	bFolded = ( rc == SXRET_OK && pVm->nSpecDiag == nDiag );` |
|     26288 |  2332 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         3 |  2333 | `		bFolded = 0; /* a throw was swallowed */` |
|         1 |  2334 | `	}` |
|     26288 |  2335 | `	if( bFramePushed ){` |
|       ! 0 |  2336 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 |  2337 | `	}` |
|     26288 |  2338 | `	return bFolded;` |
|     13129 |  2339 | `}` |
|         - |  2340 | `/*` |
|         - |  2341 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - |  2342 | ` * it can be instanciated from the executed PHP script.` |
|         - |  2343 | ` */` |
|         - |  2344 | `/*` |
|         - |  2345 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - |  2346 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - |  2347 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - |  2348 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - |  2349 | ` * properties register their enforcement slot. It is factored out of` |
|         - |  2350 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - |  2351 | ` * reuse without re-installing the (compile-time) methods.` |
|         - |  2352 | ` */` |
|   4040907 |  2353 | `static sxi32 VmMountUserClassAttrs(` |
|         - |  2354 | `	ph7_vm *pVm,      /* Target VM */` |
|         - |  2355 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - |  2356 | `	)` |
|         5 |  2357 | `{` |
|         - |  2358 | `	ph7_class_attr *pAttr;` |
|         - |  2359 | `	SyHashEntry *pEntry;` |
|         - |  2360 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - |  2361 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - |  2362 | `	 * over both tables. */` |
|         - |  2363 | `	SyHash *apMount[2];` |
|         - |  2364 | `	int iMount;` |
|   4040912 |  2365 | `	apMount[0] = &pClass->hAttr;` |
|   4040912 |  2366 | `	apMount[1] = &pClass->hConst;` |
|  12122718 |  2367 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - |  2368 | `	/* Reset the loop cursor */` |
|   8081819 |  2369 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - |  2370 | `	/* Process only static and constant attribute */` |
|  46189110 |  2371 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - |  2372 | `		/* Extract the current attribute */` |
|  38107304 |  2373 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  38107299 |  2374 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  25223540 |  2375 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   6180077 |  2376 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - |  2377 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - |  2378 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - |  2379 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - |  2380 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - |  2381 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - |  2382 | `			 * user catch, and initializers referencing constants of a class` |
|         - |  2383 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - |  2384 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - |  2385 | `			 * %s as value for class constant" fatal without any access). */` |
|  12375009 |  2386 | `			continue;` |
|         - |  2387 | `		}` |
|  25732295 |  2388 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|  12847838 |  2389 | `				== PH7_CLASS_ATTR_TYPED` |
|  20828157 |  2390 | `		 && pAttr->pNativeValue == 0 && SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - |  2391 | `			/* A typed INSTANCE default: php holds it to its type when the class` |
|         - |  2392 | `			 * first resolves, so the class owes PH7_VmMaterializeClassStatics a` |
|         - |  2393 | `			 * pass even with a whole static table. */` |
|      9497 |  2394 | `			pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|      9497 |  2395 | `			continue;` |
|         - |  2396 | `		}` |
|  25722808 |  2397 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - |  2398 | `			ph7_value *pMemObj;` |
|      7485 |  2399 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - |  2400 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - |  2401 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - |  2402 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - |  2403 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - |  2404 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - |  2405 | `				 * access / instantiation throws like php's. */` |
|      6950 |  2406 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      5642 |  2407 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - |  2408 | `						/* Its default threw at the other class's mount and is` |
|         - |  2409 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - |  2410 | `						 * both static tables). */` |
|         5 |  2411 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|      5640 |  2412 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        20 |  2413 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|        12 |  2414 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|        14 |  2415 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|       ! 0 |  2416 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|       ! 0 |  2417 | `						}` |
|         6 |  2418 | `					}` |
|      2819 |  2419 | `				}` |
|      6953 |  2420 | `				continue;` |
|         - |  2421 | `			}` |
|         - |  2422 | `			/* Reserve a memory object for this constant/static attribute */` |
|       539 |  2423 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       539 |  2424 | `			if( pMemObj == 0 ){` |
|       ! 0 |  2425 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - |  2426 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 |  2427 | `					&pClass->sDisp,&pAttr->sName` |
|         - |  2428 | `					);` |
|       ! 0 |  2429 | `				return SXERR_MEM;` |
|         - |  2430 | `			}` |
|       539 |  2431 | `			if( pAttr->pNativeValue ){` |
|         - |  2432 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 |  2433 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       539 |  2434 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - |  2435 | `				/* Initialize attribute default value (any complex expression).` |
|         - |  2436 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - |  2437 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       477 |  2438 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       477 |  2439 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|       477 |  2440 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - |  2441 | `				sxi32 rcExec;` |
|       477 |  2442 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|         - |  2443 | `				/* ...and the frame marker is what makes that fallback reachable when a` |
|         - |  2444 | `				 * frame IS current: a class declared inside a METHOD mounts here, and` |
|         - |  2445 | `				 * without the marker PH7_VmPeekDeclaringClass answers that method's` |
|         - |  2446 | ``				 * class instead (`class G { function go(){ eval('class Q { const K=5;`` |
|         - |  2447 | ``				 * public static $s = self::K; }'); } }` read G::K). */`` |
|       477 |  2448 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       477 |  2449 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       477 |  2450 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       477 |  2451 | `				pVm->nConstEvalDepth++;` |
|         - |  2452 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - |  2453 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - |  2454 | `				 * not something it can see. What reaches this line is a static` |
|         - |  2455 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - |  2456 | `				 * which php does validate here, but only when the initializer` |
|         - |  2457 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - |  2458 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - |  2459 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       477 |  2460 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       477 |  2461 | `				pVm->nConstEvalDepth--;` |
|       477 |  2462 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       477 |  2463 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       477 |  2464 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|       477 |  2465 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - |  2466 | `					/* php has not reached this initializer: defer it whole to the` |
|         - |  2467 | `					 * first USE, where the throw is raised at the access site and is` |
|         - |  2468 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - |  2469 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - |  2470 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - |  2471 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - |  2472 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - |  2473 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - |  2474 | `					 * unset nIdx. */` |
|        77 |  2475 | `					if( bStaticProp ){` |
|        71 |  2476 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        71 |  2477 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        37 |  2478 | `					}else{` |
|         - |  2479 | `						/* Release before recycling: the value a muted eval that` |
|         - |  2480 | `						 * only recorded a CYCLE left here must go before the` |
|         - |  2481 | `						 * slot's dead nIdx word becomes the free-list link. */` |
|         8 |  2482 | `						PH7_MemObjRelease(pMemObj);` |
|         8 |  2483 | `						VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|         8 |  2484 | `						continue;` |
|         - |  2485 | `					}` |
|       435 |  2486 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|       204 |  2487 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - |  2488 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - |  2489 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - |  2490 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        77 |  2491 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        77 |  2492 | `					if( rcType != SXRET_OK ){` |
|        11 |  2493 | `						return rcType;` |
|         - |  2494 | `					}` |
|        32 |  2495 | `				}` |
|       229 |  2496 | `			}` |
|         - |  2497 | `			/* Record attribute index */` |
|       525 |  2498 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - |  2499 | `			/* Install static attribute in the reference table */` |
|       525 |  2500 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  2501 | `			/* If this is a typed static property, register the slot so the` |
|         - |  2502 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - |  2503 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - |  2504 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - |  2505 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - |  2506 | `			 * already enforced above, so they need no store-time slot. */` |
|       520 |  2507 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       351 |  2508 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|       113 |  2509 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       113 |  2510 | `				if( pVmAttrS == 0 ){` |
|       ! 0 |  2511 | `					return SXERR_MEM;` |
|         - |  2512 | `				}` |
|       113 |  2513 | `				pVmAttrS->pAttr = pAttr;` |
|       113 |  2514 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|       113 |  2515 | `				pVmAttrS->iState = 0;` |
|       113 |  2516 | `				PH7_VmAttrSetClass(pVmAttrS,pClass);   /* the class's own slot: no instance behind it */` |
|         - |  2517 | `				/* Static typed property with no default starts uninitialized` |
|         - |  2518 | `				 * (constants are already excluded by the enclosing condition). */` |
|       113 |  2519 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        37 |  2520 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        97 |  2521 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - |  2522 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - |  2523 | `					 * typed static default LAZILY at the first static-property` |
|         - |  2524 | `					 * access / instantiation (a never-touched bad default is` |
|         - |  2525 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - |  2526 | `					 * place (int -> float widening, whole-real materialization,` |
|         - |  2527 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - |  2528 | `					 * the slot and the class are flagged, and the access sites` |
|         - |  2529 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - |  2530 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - |  2531 | `					 * the materializer checks it after the re-run. */` |
|        53 |  2532 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|       ! 0 |  2533 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|       ! 0 |  2534 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|       ! 0 |  2535 | `					}` |
|        24 |  2536 | `				}` |
|       113 |  2537 | `				if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttrS) != SXRET_OK ){` |
|       ! 0 |  2538 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 |  2539 | `					return SXERR_MEM;` |
|         - |  2540 | `				}` |
|        54 |  2541 | `			}` |
|       260 |  2542 | `		}` |
|         5 |  2543 | `	}` |
|   4035463 |  2544 | `	} /* for iMount */` |
|   4040904 |  2545 | `	return SXRET_OK;` |
|   2017736 |  2546 | `}` |
|         - |  2547 | `/*` |
|         - |  2548 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - |  2549 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - |  2550 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - |  2551 | ` */` |
|   4038423 |  2552 | `static sxi32 VmMountUserClassMethods(` |
|         - |  2553 | `	ph7_vm *pVm,      /* Target VM */` |
|         - |  2554 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - |  2555 | `	)` |
|         5 |  2556 | `{` |
|         - |  2557 | `	ph7_class_method *pMeth;` |
|         - |  2558 | `	SyHashEntry *pEntry;` |
|         - |  2559 | `	sxi32 rc;` |
|         - |  2560 | `	/* Install class methods */` |
|   4038428 |  2561 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|         - |  2562 | `		/* Do not mount trait methods since they are not directly invocable.` |
|         - |  2563 | `		 */` |
|    214423 |  2564 | `		return SXRET_OK;` |
|         - |  2565 | `	}` |
|         - |  2566 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - |  2567 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - |  2568 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - |  2569 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - |  2570 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - |  2571 | `	/* Install the methods now */` |
|   3824010 |  2572 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  60916033 |  2573 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  57092028 |  2574 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|         - |  2575 | `		/* A DECLARED abstract method (an interface's included) is installed too: php` |
|         - |  2576 | `		 * runs its EMPTY body through the two doors that do not refuse it, and every` |
|         - |  2577 | `		 * other door refuses on the flag first. The compiler gave it that body; one the` |
|         - |  2578 | `		 * engine synthesized has none and stays uninstalled. */` |
|  57092023 |  2579 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0` |
|  29208561 |  2580 | `		 \|\| SySetUsed(&pMeth->sFunc.aByteCode) > 0 ){` |
|  55687754 |  2581 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  55687754 |  2582 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2583 | `				return rc;` |
|         - |  2584 | `			}` |
|  27804181 |  2585 | `		}` |
|         5 |  2586 | `	}` |
|         - |  2587 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   3824010 |  2588 | `	pClass->bMounted = TRUE;` |
|   3824010 |  2589 | `	return SXRET_OK;` |
|   2016494 |  2590 | `}` |
|   2303731 |  2591 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - |  2592 | `	ph7_vm *pVm,      /* Target VM */` |
|         - |  2593 | `	ph7_class *pClass /* Class to be mounted */` |
|         - |  2594 | `	)` |
|         5 |  2595 | `{` |
|         - |  2596 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - |  2597 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - |  2598 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - |  2599 | `	 * initializer that throws finds the exception classes ready. */` |
|   2303736 |  2600 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   2303736 |  2601 | `	if( rc != SXRET_OK ){` |
|         3 |  2602 | `		return rc;` |
|         - |  2603 | `	}` |
|   2303734 |  2604 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|   1150512 |  2605 | `}` |
|         - |  2606 | `/*` |
|         - |  2607 | ` * Allocate a private frame for attributes of the given` |
|         - |  2608 | ` * class instance (Object in the PHP jargon).` |
|         - |  2609 | ` */` |
|   1680820 |  2610 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - |  2611 | `	ph7_vm *pVm, /* Target VM */` |
|         - |  2612 | `	ph7_class_instance *pObj /* Class instance */` |
|         - |  2613 | `	)` |
|         5 |  2614 | `{` |
|   1680825 |  2615 | `	ph7_class *pClass = pObj->pClass;` |
|         - |  2616 | `	ph7_class_attr *pAttr;` |
|         - |  2617 | `	SyHashEntry *pEntry;` |
|         - |  2618 | `	sxi32 rc;` |
|   1680825 |  2619 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - |  2620 | `	                    * at the FIRST bad default; a second registered throw` |
|         - |  2621 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - |  2622 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1680825 |  2623 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  13611084 |  2624 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - |  2625 | `		VmClassAttr *pVmAttr;` |
|         - |  2626 | `		/* The KEY the class filed it under, not the attribute's own name: an` |
|         - |  2627 | `		 * inherited PRIVATE instance property lives under php's mangled storage` |
|         - |  2628 | ``		 * name (PH7_ClassAttrStorageName), which is what keeps a base's `$q` and a`` |
|         - |  2629 | ``		 * child's `$q` two slots on one object instead of one. */`` |
|  11930264 |  2630 | `		const void *pKey = pEntry->pKey;` |
|  11930264 |  2631 | `		sxu32 nKeyLen = pEntry->nKeyLen;` |
|         - |  2632 | `		/* Extract the current attribute */` |
|  11930264 |  2633 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  11930264 |  2634 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT ){` |
|         - |  2635 | `			/* php's VIRTUAL property: the class declares the name and answers it` |
|         - |  2636 | `			 * from its own state, and the OBJECT has no slot for it at all -- so` |
|         - |  2637 | `			 * every table walk (the (array) cast, get_object_vars, foreach,` |
|         - |  2638 | `			 * json_encode, var_export, serialize) finds nothing, and a read, a` |
|         - |  2639 | `			 * write or an isset() takes the miss path to the class's magic trio.` |
|         - |  2640 | `			 * Nothing ever installs one; unlike the LAZY kind there is no C body` |
|         - |  2641 | `			 * behind it that would. */` |
|   1101091 |  2642 | `			continue;` |
|         - |  2643 | `		}` |
|  10829178 |  2644 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY ){` |
|         - |  2645 | `			/* A property php's own object does not HOLD until its constructor` |
|         - |  2646 | `			 * fills it: no slot, no hAttr entry, nothing for a read, an isset()` |
|         - |  2647 | `			 * or a property walk to find. PH7_NativeMaterializeLazy installs the` |
|         - |  2648 | `			 * whole set the first time a C body writes one. */` |
|      9384 |  2649 | `			continue;` |
|         - |  2650 | `		}` |
|  10819798 |  2651 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10819798 |  2652 | `		if( pVmAttr == 0 ){` |
|       ! 0 |  2653 | `			return SXERR_MEM;` |
|         - |  2654 | `		}` |
|  10819798 |  2655 | `		pVmAttr->pAttr = pAttr;` |
|  10819798 |  2656 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - |  2657 | `			ph7_value *pMemObj;` |
|         - |  2658 | `			/* Reserve a memory object for this attribute */` |
|  10818420 |  2659 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10818420 |  2660 | `			if( pMemObj == 0 ){` |
|       ! 0 |  2661 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2662 | `				return SXERR_MEM;` |
|         - |  2663 | `			}` |
|  10818420 |  2664 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10818420 |  2665 | `			pVmAttr->iState = 0;` |
|  10818420 |  2666 | `			PH7_VmAttrSetInst(pVmAttr,pObj);` |
|  10818420 |  2667 | `			if( pAttr->pNativeValue ){` |
|         - |  2668 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - |  2669 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - |  2670 | `				 * literal cannot throw and the builder states the type itself. */` |
|  10267073 |  2671 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5684196 |  2672 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - |  2673 | `				/* Initialize attribute default value (any complex expression).` |
|         - |  2674 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - |  2675 | ``				 * against the declaring class. This runs at `new`, i.e. at an`` |
|         - |  2676 | `				 * arbitrary point in execution -- typically inside some OTHER` |
|         - |  2677 | `				 * class's method, whose frame is still current because` |
|         - |  2678 | `				 * VmLocalExec pushes none of its own. Mark that frame, or` |
|         - |  2679 | `				 * PH7_VmPeekDeclaringClass answers the caller's class and` |
|         - |  2680 | ``				 * `public $v = self::K` read F::K when `new` ran in F::make(). */`` |
|      3649 |  2681 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      3649 |  2682 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - |  2683 | `				sxi32 rcExec;` |
|      3649 |  2684 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      3649 |  2685 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      3649 |  2686 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      3649 |  2687 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      3649 |  2688 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|      3649 |  2689 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - |  2690 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - |  2691 | `					 * enum case): its exception is already registered — do NOT` |
|         - |  2692 | `					 * also type-check the leftover value (a spurious second` |
|         - |  2693 | `					 * TypeError would escape the user's catch), and throw` |
|         - |  2694 | `					 * nothing further for the remaining attributes.` |
|         - |  2695 | `					 *` |
|         - |  2696 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - |  2697 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - |  2698 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - |  2699 | `					 * and the throw came back as a status nobody read: this` |
|         - |  2700 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - |  2701 | `					 * and the whole statement RESUMED after the catch — php` |
|         - |  2702 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - |  2703 | `					 * existing construction-aborted route. */` |
|        21 |  2704 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        21 |  2705 | `					bDefThrew = 1;` |
|      3639 |  2706 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - |  2707 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - |  2708 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - |  2709 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - |  2710 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - |  2711 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - |  2712 | `					 * aborts construction) / the fetch-point router. */` |
|       673 |  2713 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       673 |  2714 | `					if( rcDef != SXRET_OK ){` |
|       ! 0 |  2715 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|       ! 0 |  2716 | `						bDefThrew = 1;` |
|       ! 0 |  2717 | `					}` |
|       339 |  2718 | `				}` |
|    549530 |  2719 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - |  2720 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - |  2721 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    544748 |  2722 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    272331 |  2723 | `			}` |
|  10818420 |  2724 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
|  10818420 |  2725 | `			if( rc != SXRET_OK ){` |
|         - |  2726 | `				/* Restore the reserved (NULL-valued) slot to the free list */` |
|       ! 0 |  2727 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 |  2728 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2729 | `				return SXERR_MEM;` |
|         - |  2730 | `			}` |
|         - |  2731 | `			/* Install attribute in the reference table */` |
|  10818420 |  2732 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  2733 | `			/* Register the slot with the store filter -- a declared TYPE to` |
|         - |  2734 | `			 * enforce, a native class's write handler, or both. On failure roll` |
|         - |  2735 | `			 * back the just-installed hAttr entry and the reserved memobj so the` |
|         - |  2736 | `			 * caller sees a consistent instance. */` |
|  10818420 |  2737 | `			rc = PH7_VmStoreFilterRegister(&(*pVm),pVmAttr);` |
|  10818420 |  2738 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2739 | `				SyHashDeleteEntry(&pObj->hAttr,pKey,nKeyLen,0);` |
|       ! 0 |  2740 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 |  2741 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2742 | `				return SXERR_MEM;` |
|         - |  2743 | `			}` |
|   5408482 |  2744 | `		}else{` |
|         - |  2745 | `			/* Install static/constant attribute */` |
|      1383 |  2746 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|      1383 |  2747 | `			pVmAttr->iState = 0;` |
|      1383 |  2748 | `			PH7_VmAttrSetClass(pVmAttr,pClass);   /* a static slot belongs to the class, not to this object */` |
|      1383 |  2749 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
|      1383 |  2750 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  2751 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2752 | `				return SXERR_MEM;` |
|         - |  2753 | `			}` |
|         - |  2754 | `		}` |
|         5 |  2755 | `	}` |
|   1680825 |  2756 | `	return SXRET_OK;` |
|    840247 |  2757 | `}` |
|         - |  2758 | `/*` |
|         - |  2759 | ` * Whether [pClass] permits runtime-created (dynamic) properties: stdClass, and` |
|         - |  2760 | `` * any class php's own `#[AllowDynamicProperties]` opts in (the attribute is`` |
|         - |  2761 | ` * inherited, so the ancestry is walked -- VmClassHasAttributeNamed does that).` |
|         - |  2762 | ` *` |
|         - |  2763 | ` * The attribute half used to be spelled out at each caller, and one of the three` |
|         - |  2764 | ` * did not have it: the by-REFERENCE binder (VmBindPropByRef) asked this alone, so` |
|         - |  2765 | `` * an opted-in class refused `f($o->undeclared)` with the scope policy's `Cannot create dynamic`` |
|         - |  2766 | `` * property` on a write php performs -- while `$o->undeclared = 1` next to it`` |
|         - |  2767 | ` * worked. One decision, one site.` |
|         - |  2768 | ` */` |
|       340 |  2769 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 |  2770 | `{` |
|       345 |  2771 | `	if( pVm->pStdClass != 0 ){` |
|         - |  2772 | `		/* ...and stdClass's own permission is INHERITED, exactly as the attribute` |
|         - |  2773 | ``		 * is: php deprecates nothing for `class C extends stdClass`, so the scope policy must`` |
|         - |  2774 | `		 * refuse nothing there either. Only the class ITSELF was recognized, so a` |
|         - |  2775 | `		 * subclass of the one class php lets a script build freely could not take a` |
|         - |  2776 | `		 * property at all. */` |
|       345 |  2777 | `		ph7_class *pAncestor = pClass;` |
|       437 |  2778 | `		while( pAncestor ){` |
|       357 |  2779 | `			if( pAncestor == pVm->pStdClass ){` |
|       265 |  2780 | `				return TRUE;` |
|         - |  2781 | `			}` |
|        97 |  2782 | `			pAncestor = pAncestor->pBase;` |
|         5 |  2783 | `		}` |
|        40 |  2784 | `	}` |
|        85 |  2785 | `	return VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|         - |  2786 | `		sizeof("AllowDynamicProperties")-1);` |
|       175 |  2787 | `}` |
|         - |  2788 | `/*` |
|         - |  2789 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - |  2790 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - |  2791 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - |  2792 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - |  2793 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - |  2794 | ` */` |
|        80 |  2795 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         5 |  2796 | `{` |
|       141 |  2797 | `	while( pClass ){` |
|        91 |  2798 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - |  2799 | `		sxu32 n;` |
|        91 |  2800 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        30 |  2801 | `			if( aAttr[n].sName.nByte == nName` |
|        32 |  2802 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        32 |  2803 | `				return TRUE;` |
|         - |  2804 | `			}` |
|       ! 0 |  2805 | `		}` |
|        61 |  2806 | `		pClass = pClass->pBase;` |
|         5 |  2807 | `	}` |
|        55 |  2808 | `	return FALSE;` |
|        45 |  2809 | `}` |
|         - |  2810 | `/*` |
|         - |  2811 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - |  2812 | ` * access or method call on such an instance is php's incomplete-object` |
|         - |  2813 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - |  2814 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - |  2815 | ` */` |
|    339033 |  2816 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 |  2817 | `{` |
|    339038 |  2818 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 |  2819 | `}` |
|         - |  2820 | `/*` |
|         - |  2821 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - |  2822 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - |  2823 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - |  2824 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - |  2825 | ` * had one says "unknown", like php.` |
|         - |  2826 | ` */` |
|        44 |  2827 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 |  2828 | `{` |
|        45 |  2829 | `	const char *zName = "unknown";` |
|        45 |  2830 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 |  2831 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - |  2832 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 |  2833 | `	if( pEntry ){` |
|        45 |  2834 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 |  2835 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 |  2836 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 |  2837 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 |  2838 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 |  2839 | `		}` |
|        22 |  2840 | `	}` |
|        67 |  2841 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - |  2842 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - |  2843 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 |  2844 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 |  2845 | `}` |
|         - |  2846 | `/*` |
|         - |  2847 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - |  2848 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - |  2849 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - |  2850 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - |  2851 | ` * (property_exists() passes its name in pFuncName).` |
|         - |  2852 | ` */` |
|        16 |  2853 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 |  2854 | `{` |
|         - |  2855 | `	SyBlob sMsg;` |
|        17 |  2856 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 |  2857 | `	if( pFuncName ){` |
|       ! 0 |  2858 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 |  2859 | `	}else{` |
|        17 |  2860 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 |  2861 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 |  2862 | `		if( pFunc == 0 ){` |
|       ! 0 |  2863 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 |  2864 | `		}else{` |
|        17 |  2865 | `			const char *zDisp = 0;` |
|         - |  2866 | `			int nDisp;` |
|        17 |  2867 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 |  2868 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 |  2869 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 |  2870 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 |  2871 | `			}` |
|        17 |  2872 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 |  2873 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - |  2874 | `		}` |
|         - |  2875 | `	}` |
|        17 |  2876 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 |  2877 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 |  2878 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 |  2879 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 |  2880 | `	SyBlobRelease(&sMsg);` |
|        17 |  2881 | `}` |
|         - |  2882 | `/*` |
|         - |  2883 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - |  2884 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - |  2885 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - |  2886 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - |  2887 | ` *` |
|         - |  2888 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - |  2889 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - |  2890 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - |  2891 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - |  2892 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - |  2893 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - |  2894 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - |  2895 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - |  2896 | ` */` |
|       638 |  2897 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         5 |  2898 | `{` |
|         - |  2899 | `	ph7_class_attr *pAttr;` |
|       643 |  2900 | `	VmClassAttr *pVmAttr = 0;` |
|       643 |  2901 | `	ph7_value *pMemObj = 0;` |
|         - |  2902 | `	char *zCopy;` |
|         - |  2903 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       643 |  2904 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       643 |  2905 | `	if( pAttr == 0 ){` |
|       ! 0 |  2906 | `		return 0;` |
|         - |  2907 | `	}` |
|       643 |  2908 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       643 |  2909 | `	zCopy = (char *)&pAttr[1];` |
|       643 |  2910 | `	if( nName > 0 ){` |
|       631 |  2911 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       313 |  2912 | `	}` |
|       643 |  2913 | `	zCopy[nName] = 0;` |
|       643 |  2914 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       643 |  2915 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       643 |  2916 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       643 |  2917 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - |  2918 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - |  2919 | `	 * value, never a union. */` |
|       643 |  2920 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       643 |  2921 | `	if( pVmAttr == 0 ){` |
|       ! 0 |  2922 | `		goto fail_attr;` |
|         - |  2923 | `	}` |
|       643 |  2924 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       643 |  2925 | `	if( pMemObj == 0 ){` |
|       ! 0 |  2926 | `		goto fail_vmattr;` |
|         - |  2927 | `	}` |
|       643 |  2928 | `	pVmAttr->pAttr = pAttr;` |
|       643 |  2929 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       643 |  2930 | `	pVmAttr->iState = 0;` |
|       643 |  2931 | `	PH7_VmAttrSetInst(pVmAttr,pThis);` |
|         - |  2932 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - |  2933 | `	 * property-creation order, matching PHP. */` |
|       643 |  2934 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 |  2935 | `		goto fail_slot;` |
|         - |  2936 | `	}` |
|         - |  2937 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       643 |  2938 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  2939 | ``	/* php walks the LIVE property table: a `foreach`/`array_walk` that has run off`` |
|         - |  2940 | `	 * the end re-arms onto a property the body just created. */` |
|       962 |  2941 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|       638 |  2942 | `		SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|       643 |  2943 | `	if( ppAttr ){` |
|       371 |  2944 | `		*ppAttr = pVmAttr;` |
|       183 |  2945 | `	}` |
|       643 |  2946 | `	return pMemObj;` |
|       ! 0 |  2947 | `fail_slot:` |
|       ! 0 |  2948 | `	VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 |  2949 | `fail_vmattr:` |
|       ! 0 |  2950 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2951 | `fail_attr:` |
|       ! 0 |  2952 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 |  2953 | `	return 0;` |
|       324 |  2954 | `}` |
|         - |  2955 | `/*` |
|         - |  2956 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - |  2957 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - |  2958 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - |  2959 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - |  2960 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - |  2961 | ` */` |
|      7398 |  2962 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         4 |  2963 | `{` |
|         - |  2964 | `	VmClassAttr *pVmAttr;` |
|         - |  2965 | `	ph7_value *pMemObj;` |
|         - |  2966 | `	/* php's storage name: a base's private goes back into the slot it came out` |
|         - |  2967 | `	 * of, beside (not over) a same-named property of the object's own class. */` |
|      7402 |  2968 | `	const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pAttr);` |
|      7402 |  2969 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      7402 |  2970 | `	if( pVmAttr == 0 ){` |
|       ! 0 |  2971 | `		return;` |
|         - |  2972 | `	}` |
|      7402 |  2973 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      7402 |  2974 | `	if( pMemObj == 0 ){` |
|       ! 0 |  2975 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2976 | `		return;` |
|         - |  2977 | `	}` |
|      7402 |  2978 | `	pVmAttr->pAttr = pAttr;` |
|      7402 |  2979 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      7402 |  2980 | `	pVmAttr->iState = 0;` |
|      7402 |  2981 | `	PH7_VmAttrSetInst(pVmAttr,pThis);` |
|         - |  2982 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - |  2983 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - |  2984 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - |  2985 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - |  2986 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|      7402 |  2987 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      1138 |  2988 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       568 |  2989 | `	}` |
|         - |  2990 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - |  2991 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - |  2992 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - |  2993 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|      7402 |  2994 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),pVmAttr) != SXRET_OK ){` |
|       ! 0 |  2995 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 |  2996 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  2997 | `		return;` |
|         - |  2998 | `	}` |
|      7402 |  2999 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      7402 |  3000 | `	if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttr) != SXRET_OK ){` |
|       ! 0 |  3001 | `		SyHashDeleteEntry(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),0);` |
|       ! 0 |  3002 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 |  3003 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 |  3004 | `		return;` |
|         - |  3005 | `	}` |
|         - |  3006 | `	/* Re-armed only once the entry is here to stay: the rollback above deletes it. */` |
|     11101 |  3007 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|      7398 |  3008 | `		SyHashGet(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey)));` |
|      7402 |  3009 | `	if( ppAttr ){` |
|      7402 |  3010 | `		*ppAttr = pVmAttr;` |
|      3699 |  3011 | `	}` |
|      3703 |  3012 | `}` |
|         - |  3013 | `/* Forward declaration */` |
|         - |  3014 | `/*` |
|         - |  3015 | ` * Dummy read-only buffer used for slot reservation.` |
|         - |  3016 | ` */` |
|         - |  3017 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - |  3018 | `/*` |
|         - |  3019 | ` * Reserve a constant memory object.` |
|         - |  3020 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - |  3021 | ` */` |
|   2109492 |  3022 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 |  3023 | `{` |
|         - |  3024 | `	ph7_value *pObj;` |
|         - |  3025 | `	sxi32 rc;` |
|   2109497 |  3026 | `	if( pIndex ){` |
|         - |  3027 | `		/* Object index in the object table */` |
|   2084162 |  3028 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|   1040334 |  3029 | `	}` |
|         - |  3030 | `	/* Reserve a slot for the new object */` |
|   2109497 |  3031 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   2109497 |  3032 | `	if( rc != SXRET_OK ){` |
|         - |  3033 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - |  3034 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - |  3035 | `		 */` |
|       ! 0 |  3036 | `		return 0;` |
|         - |  3037 | `	}` |
|   2109497 |  3038 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   2109497 |  3039 | `	return pObj;` |
|   1052990 |  3040 | `}` |
|         - |  3041 | `/*` |
|         - |  3042 | ` * The segmented memory-object pool. See VmMemPool in ph7int.h.` |
|         - |  3043 | ` * A slot's ADDRESS never moves once it exists, which is the whole point: the` |
|         - |  3044 | ` * engine's standing "a pointer into aMemObj dangles across a reserve" hazard` |
|         - |  3045 | ` * (written down at pointers-die-across-a-user-callback) exists only because the` |
|         - |  3046 | ` * old table reallocated. Growth here appends a VM_MEMPOOL_SEG_SLOTS segment --` |
|         - |  3047 | ` * one allocation, no copy -- and a fully-free trailing segment is returned on` |
|         - |  3048 | ` * truncate, so a reused VM (-S server, in-process .phpt runner) hands the pool` |
|         - |  3049 | ` * back most of what a large run grew.` |
|         - |  3050 | ` */` |
|      8445 |  3051 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator)` |
|         5 |  3052 | `{` |
|         - |  3053 | `	ph7_value *pSeg;` |
|      8450 |  3054 | `	SyZero(pPool,sizeof(VmMemPool));` |
|      8450 |  3055 | `	pPool->pAllocator = pAllocator;` |
|         - |  3056 | `	/* The first segment up front, mirroring the SySetAlloc(&pVm->aMemObj,0xFF)` |
|         - |  3057 | `	 * the pool replaced: an aMemObj exists the moment the VM does, and at 256` |
|         - |  3058 | `	 * slots it costs the same 16 KB that opening bid did. This allocation IS the` |
|         - |  3059 | `	 * per-VM floor -- see the segment-size note on VmMemPool in ph7int.h before` |
|         - |  3060 | `	 * raising VM_MEMPOOL_SEG_SHIFT. */` |
|      8450 |  3061 | `	pSeg = (ph7_value *)SyMemBackendAlloc(pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|      8450 |  3062 | `	if( pSeg == 0 ){` |
|       ! 0 |  3063 | `		return SXERR_MEM;` |
|         - |  3064 | `	}` |
|      8450 |  3065 | `	pPool->apSeg = (ph7_value **)SyMemBackendAlloc(pAllocator,sizeof(ph7_value *) * 16);` |
|      8450 |  3066 | `	if( pPool->apSeg == 0 ){` |
|       ! 0 |  3067 | `		SyMemBackendFree(pAllocator,pSeg);` |
|       ! 0 |  3068 | `		return SXERR_MEM;` |
|         - |  3069 | `	}` |
|      8450 |  3070 | `	pPool->apSeg[0] = pSeg;` |
|      8450 |  3071 | `	pPool->nSeg = 1;` |
|      8450 |  3072 | `	pPool->nCap = 16;` |
|      8450 |  3073 | `	pPool->nFreeHead = SXU32_HIGH; /* 0 is a valid slot; the empty-list mark cannot be it */` |
|      8450 |  3074 | `	return SXRET_OK;` |
|      4222 |  3075 | `}` |
|         - |  3076 | `/*` |
|         - |  3077 | ` * Return a freed slot to the pool's intrusive free list. The slot's value must` |
|         - |  3078 | ` * already have been RELEASED by the caller (the sites that push NULL-valued` |
|         - |  3079 | ` * freshly-reserved slots on error have nothing to release): the link is written` |
|         - |  3080 | ` * into the slot's own dead nIdx word, so a slot in the list must be a dead slot.` |
|         - |  3081 | ` * O(1), and zero memory beyond the pool's single head word.` |
|         - |  3082 | ` *` |
|         - |  3083 | ` * Freeing an index that is already on the list is a NO-OP, not a corruption:` |
|         - |  3084 | ` * see MEMOBJ_POOLFREE. That is the one behaviour the old aFreeObj stack had for` |
|         - |  3085 | ` * free and this list does not, so it is bought back explicitly.` |
|         - |  3086 | ` */` |
|  28402757 |  3087 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx)` |
|         5 |  3088 | `{` |
|  28402762 |  3089 | `	ph7_value *pObj = PH7_MemObjAt(pPool,nIdx);` |
|  28402762 |  3090 | `	if( pObj == 0 ){` |
|       ! 0 |  3091 | `		return;   /* stale index -- the caller's own contract, and truncate's */` |
|         - |  3092 | `	}` |
|  28402762 |  3093 | `	if( pObj->iFlags & MEMOBJ_POOLFREE ){` |
|         - |  3094 | `		/* Already on the list. The old aFreeObj stack tolerated a double free by` |
|         - |  3095 | `		 * handing the index out twice and draining; this list would write the head` |
|         - |  3096 | `		 * into the slot the head already names, and every reserve after it would` |
|         - |  3097 | `		 * return that one slot forever. Refusing leaks nothing -- the slot stays` |
|         - |  3098 | `		 * exactly where it already is, on the list. */` |
|       ! 0 |  3099 | `		return;` |
|         - |  3100 | `	}` |
|  28402762 |  3101 | `	pObj->iFlags \|= MEMOBJ_POOLFREE;` |
|  28402762 |  3102 | `	pObj->nIdx = pPool->nFreeHead;` |
|  28402762 |  3103 | `	pPool->nFreeHead = nIdx;` |
|  14200549 |  3104 | `}` |
|         - |  3105 | `/*` |
|         - |  3106 | ` * Reserve a slot at the end of the pool. Returns the raw slot (uninitialized --` |
|         - |  3107 | ` * callers PH7_MemObjInit it, as they did the SySetPeek of the set this replaced)` |
|         - |  3108 | ` * and stores its index. Appending a slot never relocates an existing one.` |
|         - |  3109 | ` */` |
|   3156230 |  3110 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex)` |
|         5 |  3111 | `{` |
|         - |  3112 | `	sxu32 nIdx;` |
|         - |  3113 | `	sxu32 nSeg;` |
|   3156235 |  3114 | `	if( pPool->nUsed >= (pPool->nSeg << VM_MEMPOOL_SEG_SHIFT) ){` |
|         - |  3115 | `		ph7_value *pSeg;` |
|     11470 |  3116 | `		if( pPool->nSeg >= pPool->nCap ){` |
|         - |  3117 | `			/* The segment table itself doubles. It is a few hundred pointers at` |
|         - |  3118 | `			 * the engine's real peaks, so this is cheap and does not touch the` |
|         - |  3119 | `			 * values. */` |
|         - |  3120 | `			ph7_value **apNew;` |
|        47 |  3121 | `			sxu32 nNew = pPool->nCap ? pPool->nCap * 2 : 16;` |
|        47 |  3122 | `			apNew = (ph7_value **)SyMemBackendRealloc(pPool->pAllocator,pPool->apSeg,sizeof(ph7_value *) * nNew);` |
|        47 |  3123 | `			if( apNew == 0 ){` |
|       ! 0 |  3124 | `				return 0;` |
|         - |  3125 | `			}` |
|        47 |  3126 | `			pPool->apSeg = apNew;` |
|        47 |  3127 | `			pPool->nCap = nNew;` |
|        21 |  3128 | `		}` |
|     11470 |  3129 | `		pSeg = (ph7_value *)SyMemBackendAlloc(pPool->pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|     11470 |  3130 | `		if( pSeg == 0 ){` |
|       ! 0 |  3131 | `			return 0;` |
|         - |  3132 | `		}` |
|     11470 |  3133 | `		pPool->apSeg[pPool->nSeg++] = pSeg;` |
|      5730 |  3134 | `	}` |
|   3156235 |  3135 | `	nIdx = pPool->nUsed;` |
|   3156235 |  3136 | `	pPool->nUsed++;` |
|   3156235 |  3137 | `	nSeg = nIdx >> VM_MEMPOOL_SEG_SHIFT;` |
|   3156235 |  3138 | `	if( pIndex ){` |
|   3156235 |  3139 | `		*pIndex = nIdx;` |
|   1577092 |  3140 | `	}` |
|   3156235 |  3141 | `	return &pPool->apSeg[nSeg][nIdx & VM_MEMPOOL_SEG_MASK];` |
|   1577097 |  3142 | `}` |
|         - |  3143 | `/*` |
|         - |  3144 | ` * Shrink the pool's logical size. Fully-free trailing segments are RETURNED to` |
|         - |  3145 | ` * the allocator; the segment table itself keeps its capacity (a few hundred` |
|         - |  3146 | ` * pointers), so a reset does not realloc the table that describes the pool.` |
|         - |  3147 | ` */` |
|        16 |  3148 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize)` |
|       ! 0 |  3149 | `{` |
|         - |  3150 | `	sxu32 nSegNeed;` |
|         - |  3151 | `	sxu32 nGuard;` |
|         - |  3152 | `	sxu32 nCur;` |
|         - |  3153 | `	sxu32 n;` |
|         - |  3154 | `	/* Abandon the free list: its chain threads through slots that are about to be` |
|         - |  3155 | `	 * truncated away (and through segments about to be freed). The retained` |
|         - |  3156 | `	 * segments' free slots are simply forgotten -- they become fresh reserves,` |
|         - |  3157 | `	 * exactly as the SySetReset(&pVm->aFreeObj) this replaces emptied the old` |
|         - |  3158 | `	 * stack. Walk it FIRST, while nUsed still resolves every link, to take` |
|         - |  3159 | `	 * MEMOBJ_POOLFREE back off the slots that survive: the bit means "on the` |
|         - |  3160 | `	 * list", and a slot still wearing it after the list is gone would refuse the` |
|         - |  3161 | `	 * next legitimate free of that index. nGuard bounds the walk by the slot` |
|         - |  3162 | `	 * count so a chain corrupted from outside cannot spin here. */` |
|        16 |  3163 | `	nCur = pPool->nFreeHead;` |
|        40 |  3164 | `	for( nGuard = pPool->nUsed ; nGuard > 0 && nCur != SXU32_HIGH ; --nGuard ){` |
|        24 |  3165 | `		ph7_value *pFree = PH7_MemObjAt(pPool,nCur);` |
|        24 |  3166 | `		if( pFree == 0 ){` |
|       ! 0 |  3167 | `			break;` |
|         - |  3168 | `		}` |
|        24 |  3169 | `		pFree->iFlags &= ~MEMOBJ_POOLFREE;` |
|        24 |  3170 | `		nCur = pFree->nIdx;` |
|        12 |  3171 | `	}` |
|        16 |  3172 | `	pPool->nFreeHead = SXU32_HIGH;` |
|        16 |  3173 | `	if( nNewSize < pPool->nUsed ){` |
|        16 |  3174 | `		pPool->nUsed = nNewSize;` |
|         8 |  3175 | `	}` |
|         - |  3176 | `	/* Return every segment past the one that still holds a slot. nSeg is kept` |
|         - |  3177 | `	 * rounded UP to cover nUsed, so a slot index already handed out never stops` |
|         - |  3178 | `	 * resolving. */` |
|        16 |  3179 | `	nSegNeed = (pPool->nUsed + VM_MEMPOOL_SEG_SLOTS - 1) >> VM_MEMPOOL_SEG_SHIFT;` |
|        16 |  3180 | `	if( nSegNeed < pPool->nSeg ){` |
|        32 |  3181 | `		for( n = nSegNeed ; n < pPool->nSeg ; ++n ){` |
|        16 |  3182 | `			SyMemBackendFree(pPool->pAllocator,pPool->apSeg[n]);` |
|        16 |  3183 | `			pPool->apSeg[n] = 0;` |
|         8 |  3184 | `		}` |
|        16 |  3185 | `		pPool->nSeg = nSegNeed;` |
|         8 |  3186 | `	}` |
|        16 |  3187 | `	return SXRET_OK;` |
|       ! 0 |  3188 | `}` |
|         - |  3189 | `/*` |
|         - |  3190 | `` * Point an instance property at somebody else's value slot -- php's `$o->p =& $x`,`` |
|         - |  3191 | `` * and the same bind an `R:` back-reference asks for when unserialize() reads a`` |
|         - |  3192 | ` * payload whose two properties shared one reference.` |
|         - |  3193 | ` *` |
|         - |  3194 | ` * The property gives back whatever it was holding first: a slot it was already` |
|         - |  3195 | ` * bound to gets its pin returned (which frees it when this property was the last` |
|         - |  3196 | ` * holder), and its OWN slot is unset outright. Either way the typed-slot` |
|         - |  3197 | ` * enforcement entry goes with the old slot -- a reference-bound property bypasses` |
|         - |  3198 | ` * php's typed coercion -- and the new slot takes a counted pin so no frame` |
|         - |  3199 | ` * teardown recycles it underneath the property.` |
|         - |  3200 | ` */` |
|        58 |  3201 | `PH7_PRIVATE void PH7_VmBindAttrRef(ph7_vm *pVm,VmClassAttr *pVmAttr,sxu32 nSrcIdx)` |
|         1 |  3202 | `{` |
|        59 |  3203 | `	sxu32 nOldIdx = pVmAttr->nIdx;` |
|        59 |  3204 | `	if( nOldIdx == nSrcIdx ){` |
|       ! 0 |  3205 | `		return;` |
|         - |  3206 | `	}` |
|        59 |  3207 | `	if( pVmAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN) ){` |
|         - |  3208 | ``		/* A SOURCE (`$r =& $o->p; $o->p =& $y;`) still owns its declaration, so`` |
|         - |  3209 | `		 * its enforcement entry goes with the repoint. */` |
|         9 |  3210 | `		if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|         3 |  3211 | `			PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|         1 |  3212 | `		}` |
|         9 |  3213 | `		VmUnpinMemObjSlot(&(*pVm),nOldIdx);` |
|         5 |  3214 | `	}else{` |
|        51 |  3215 | `		PH7_VmStoreFilterDrop(&(*pVm),pVmAttr->pAttr,nOldIdx);` |
|        51 |  3216 | `		PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|         - |  3217 | `	}` |
|        59 |  3218 | `	pVmAttr->nIdx = nSrcIdx;` |
|        59 |  3219 | `	pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|        59 |  3220 | `	pVmAttr->iState &= ~(VM_CLASS_ATTR_UNINIT\|VM_CLASS_ATTR_REFSRCPIN);` |
|        59 |  3221 | `	VmPinMemObjSlotCounted(&(*pVm),nSrcIdx);` |
|        30 |  3222 | `}` |
|         - |  3223 | `/*` |
|         - |  3224 | ` * Reserve a memory object.` |
|         - |  3225 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - |  3226 | ` */` |
|   3156230 |  3227 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 |  3228 | `{` |
|         - |  3229 | `	ph7_value *pObj;` |
|   3156235 |  3230 | `	pObj = VmMemPoolReserve(&pVm->aMemObj,pIndex);` |
|   3156235 |  3231 | `	if( pObj == 0 ){` |
|       ! 0 |  3232 | `		return 0;` |
|         - |  3233 | `	}` |
|         - |  3234 | `	/* The slot this replaced came from a SySetPut of a zeroed filler, so a slot` |
|         - |  3235 | `	 * the caller leaves untouched (a static without an initializer, say) reads as` |
|         - |  3236 | `	 * a null value. Keep that: zero the fresh slot. */` |
|   3156235 |  3237 | `	SyZero(pObj,sizeof(ph7_value));` |
|   3156235 |  3238 | `	return pObj;` |
|   1577097 |  3239 | `}` |
|         - |  3240 | `/* Forward declaration */` |
|         - |  3241 | `/* Forward declarations for Fiber C functions */` |
|         - |  3242 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - |  3243 | `/* Forward declarations for Generator helpers and C functions */` |
|         - |  3244 | `/*` |
|         - |  3245 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - |  3246 | ` * directly as foreign functions.` |
|         - |  3247 | ` */` |
|         - |  3248 |  |
|         - |  3249 | `/*` |
|         - |  3250 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - |  3251 | ` * start compiling the target PHP program.` |
|         - |  3252 | ` */` |
|         - |  3253 | `/* Forward declaration */` |
|         - |  3254 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|      8445 |  3255 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - |  3256 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - |  3257 | `	 ph7 *pEngine /* Master engine */` |
|         - |  3258 | `	 )` |
|         5 |  3259 | `{` |
|         - |  3260 | `	ph7_value *pObj;` |
|         - |  3261 | `	sxi32 rc;` |
|         - |  3262 | `	/* Zero the structure */` |
|      8450 |  3263 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - |  3264 | `	/* Initialize VM fields */` |
|      8450 |  3265 | `	pVm->pEngine = &(*pEngine);` |
|      8450 |  3266 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|      8450 |  3267 | `	PH7_GcInit(&(*pVm));` |
|      8450 |  3268 | `	SySetInit(&pVm->aDeadClosure,&pVm->sAllocator,sizeof(ph7_vm_func *));` |
|         - |  3269 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - |  3270 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - |  3271 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      8450 |  3272 | `	pVm->iDisplayErrors = PH7_DISPLAY_ERRORS_OFF;` |
|      8450 |  3273 | `	pVm->bLogErrors = 1;` |
|         - |  3274 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|         - |  3275 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|      8450 |  3276 | `	pVm->iMbSubstitute = '?';` |
|      8450 |  3277 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - |  3278 | `	/* Instructions containers */` |
|      8450 |  3279 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      8450 |  3280 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      8450 |  3281 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - |  3282 | `	/* Object containers */` |
|      8450 |  3283 | `	rc = VmMemPoolInit(&pVm->aMemObj,&pVm->sAllocator);` |
|      8450 |  3284 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3285 | `		return rc;` |
|         - |  3286 | `	}` |
|         - |  3287 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      8450 |  3288 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      8450 |  3289 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      8450 |  3290 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      8450 |  3291 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|      8450 |  3292 | `	SySetInit(&pVm->aEffArgRun,&pVm->sAllocator,sizeof(sxu32));` |
|         - |  3293 | `	/* Virtual machine internal containers */` |
|      8450 |  3294 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      8450 |  3295 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      8450 |  3296 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      8450 |  3297 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|         - |  3298 | `	/* The http:// wrapper's last response headers (see PH7_HttpPublishHeaders). */` |
|      8450 |  3299 | `	SyBlobInit(&pVm->sHttpRespHdrs,&pVm->sAllocator);` |
|      8450 |  3300 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      8450 |  3301 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - |  3302 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - |  3303 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - |  3304 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - |  3305 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - |  3306 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - |  3307 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - |  3308 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - |  3309 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      8450 |  3310 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      8450 |  3311 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|         - |  3312 | `	/* 0 means "this call site has never been screened", so the first generation is 1. */` |
|      8450 |  3313 | `	pVm->nCallableGen = 1;` |
|      8450 |  3314 | `	pVm->nConstGen = 1; /* likewise for a PH7_OP_LOADC site (PH7_VmConstSiteAnswer) */` |
|      8450 |  3315 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      8450 |  3316 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      8450 |  3317 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      8450 |  3318 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|      8450 |  3319 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      8450 |  3320 | `	SySetInit(&pVm->aCallSite,&pVm->sAllocator,sizeof(VmCallSite));` |
|      8450 |  3321 | `	SyHashInit(&pVm->hCallName,&pVm->sAllocator,0,0);` |
|      8450 |  3322 | `	SySetInit(&pVm->aUnitDecl,&pVm->sAllocator,sizeof(VmUnitDecl));` |
|      8450 |  3323 | `	SySetInit(&pVm->aHiddenClass,&pVm->sAllocator,sizeof(ph7_class *));` |
|      8450 |  3324 | `	pVm->bUnitDecl = 0;` |
|      8450 |  3325 | `	pVm->nFreeCallSite = 0;` |
|      8450 |  3326 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      8450 |  3327 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      8450 |  3328 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      8450 |  3329 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      8450 |  3330 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|      8450 |  3331 | `	pVm->bIniSeeded = 0;` |
|      8450 |  3332 | `	pVm->pGettext = 0;   /* ext/gettext binds its first domain lazily */` |
|      8450 |  3333 | `	pVm->pPcntl = 0;     /* ext/pcntl allocates its handler table on the first call */` |
|      8450 |  3334 | `	pVm->pSyslog = 0;    /* openlog() allocates the prefix it has to keep alive */` |
|      8450 |  3335 | `	pVm->iPosixErr = 0;  /* ext/posix has seen no failure yet */` |
|      8450 |  3336 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      8450 |  3337 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|      8450 |  3338 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|      8450 |  3339 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      8450 |  3340 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      8450 |  3341 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      8450 |  3342 | `	SyBlobInit(&pVm->sErrLogPath,&pVm->sAllocator);` |
|      8450 |  3343 | `	SyBlobInit(&pVm->sOutStartFile,&pVm->sAllocator);` |
|      8450 |  3344 | `	SyBlobInit(&pVm->sSessStartFile,&pVm->sAllocator);` |
|      8450 |  3345 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      8450 |  3346 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      8450 |  3347 | `	SyBlobInit(&pVm->sAutoloadExt,&pVm->sAllocator);` |
|      8450 |  3348 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|      8450 |  3349 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      8450 |  3350 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      8450 |  3351 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      8450 |  3352 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      8450 |  3353 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      8450 |  3354 | `	pVm->nResourceIdNext = 1;` |
|      8450 |  3355 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      8450 |  3356 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      8450 |  3357 | `	pVm->pMagicSetThis = 0;` |
|      8450 |  3358 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      8450 |  3359 | `	pVm->pHookSetThis = 0;` |
|      8450 |  3360 | `	pVm->pHookSetAttr = 0;` |
|      8450 |  3361 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      8450 |  3362 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      8450 |  3363 | `	pVm->pMagicCallThis = 0;` |
|      8450 |  3364 | `	pVm->pMagicCallClass = 0;` |
|      8450 |  3365 | `	pVm->pMagicCallLsb = 0;` |
|      8450 |  3366 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      8450 |  3367 | `	pVm->pIdleCallFrames = 0;` |
|      8450 |  3368 | `	SyZero(pVm->apIdleOperandStack,sizeof(pVm->apIdleOperandStack));` |
|      8450 |  3369 | `	pVm->nIdleOperandStacks = 0;` |
|      8450 |  3370 | `	pVm->nIdleOperandSlots = 0;` |
|      8450 |  3371 | `	pVm->pIdleStackNodes = 0;` |
|      8450 |  3372 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      8450 |  3373 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      8450 |  3374 | `	pVm->pPendingException = 0;` |
|      8450 |  3375 | `	pVm->pInflightException = 0;` |
|      8450 |  3376 | `	pVm->nInflightExcBase = 0;` |
|      8450 |  3377 | `	VmClearResumeTarget(&(*pVm));` |
|      8450 |  3378 | `	pVm->nBoundaryRc = 0;` |
|      8450 |  3379 | `	PH7_CmpRefusalClear(&(*pVm));` |
|      8450 |  3380 | `	pVm->pConstEvalClass = 0;` |
|      8450 |  3381 | `	pVm->nConstEvalDepth = 0;` |
|      8450 |  3382 | `	pVm->pConstCycleAttr = 0;` |
|      8450 |  3383 | `	pVm->pConstCycleClass = 0;` |
|      8450 |  3384 | `	SySetReset(&pVm->aMagicGuard);` |
|      8450 |  3385 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 |  3386 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 |  3387 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 |  3388 | `	}` |
|      8450 |  3389 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      8450 |  3390 | `	if( pVm->pHookSetThis ){` |
|       ! 0 |  3391 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 |  3392 | `		pVm->pHookSetThis = 0;` |
|       ! 0 |  3393 | `	}` |
|      8450 |  3394 | `	pVm->pHookSetAttr = 0;` |
|      8450 |  3395 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      8450 |  3396 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 |  3397 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 |  3398 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 |  3399 | `	}` |
|      8450 |  3400 | `	pVm->pMagicCallClass = 0;` |
|      8450 |  3401 | `	pVm->pMagicCallLsb = 0;` |
|      8450 |  3402 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - |  3403 | `	/* Configuration containers */` |
|      8450 |  3404 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      8450 |  3405 | `	SySetInit(&pVm->aIncFrame,&pVm->sAllocator,sizeof(VmIncFrame));` |
|      8450 |  3406 | `	SyBlobInit(&pVm->sReflectConstName,&pVm->sAllocator);` |
|      8450 |  3407 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      8450 |  3408 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      8450 |  3409 | `	SySetInit(&pVm->aEvalFile,&pVm->sAllocator,sizeof(SyString));` |
|      8450 |  3410 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      8450 |  3411 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|         - |  3412 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|         - |  3413 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|      8450 |  3414 | `	pVm->iResponseStatus = 0;` |
|      8450 |  3415 | `	pVm->bHeadersSent = 0;` |
|      8450 |  3416 | `	SyBlobReset(&pVm->sOutStartFile);` |
|      8450 |  3417 | `	pVm->nOutStartLine = 0;` |
|      8450 |  3418 | `	SyBlobReset(&pVm->sSessStartFile);` |
|      8450 |  3419 | `	pVm->nSessStartLine = 0;` |
|      8450 |  3420 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|      8450 |  3421 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - |  3422 | `	/* Error callbacks containers */` |
|      8450 |  3423 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|      8450 |  3424 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|      8450 |  3425 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|      8450 |  3426 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      8450 |  3427 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      8450 |  3428 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - |  3429 | `	/* Recursion policy. PHP call depth is heap-bound since` |
|         - |  3430 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - |  3431 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - |  3432 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - |  3433 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - |  3434 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - |  3435 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      8450 |  3436 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      8450 |  3437 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - |  3438 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - |  3439 | `#else` |
|         - |  3440 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - |  3441 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - |  3442 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - |  3443 | `	 * at max depth). The embedder tunes both via config verbs. */` |
|         - |  3444 | `	pVm->nMaxDepth = 512;` |
|         - |  3445 | `	pVm->nMaxNativeDepth = 16;` |
|         - |  3446 | `#endif` |
|         - |  3447 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - |  3448 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - |  3449 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      8450 |  3450 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - |  3451 | `	/* JSON return status */` |
|      8450 |  3452 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - |  3453 | `	/* PRNG context */` |
|      8450 |  3454 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - |  3455 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - |  3456 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      8450 |  3457 | `	pVm->mtSeeded = FALSE;` |
|         - |  3458 | `	/* Install the null constant */` |
|      8450 |  3459 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      8450 |  3460 | `	if( pObj == 0 ){` |
|       ! 0 |  3461 | `		rc = SXERR_MEM;` |
|       ! 0 |  3462 | `		goto Err;` |
|         - |  3463 | `	}` |
|      8450 |  3464 | `	PH7_MemObjInit(pVm,pObj);` |
|         - |  3465 | `	/* Install the boolean TRUE constant */` |
|      8450 |  3466 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      8450 |  3467 | `	if( pObj == 0 ){` |
|       ! 0 |  3468 | `		rc = SXERR_MEM;` |
|       ! 0 |  3469 | `		goto Err;` |
|         - |  3470 | `	}` |
|      8450 |  3471 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - |  3472 | `	/* Install the boolean FALSE constant */` |
|      8450 |  3473 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      8450 |  3474 | `	if( pObj == 0 ){` |
|       ! 0 |  3475 | `		rc = SXERR_MEM;` |
|       ! 0 |  3476 | `		goto Err;` |
|         - |  3477 | `	}` |
|      8450 |  3478 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - |  3479 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - |  3480 | `	 * reuse the same slot rather than allocating a new one.` |
|         - |  3481 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      8450 |  3482 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      8450 |  3483 | `	if( pObj == 0 ){` |
|       ! 0 |  3484 | `		rc = SXERR_MEM;` |
|       ! 0 |  3485 | `		goto Err;` |
|         - |  3486 | `	}` |
|      8450 |  3487 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - |  3488 | `	/* Allocate the reference table. It belongs to VM INIT rather than to` |
|         - |  3489 | `	 * PH7_VmMakeReady because the COMPILER now runs bytecode of its own: the` |
|         - |  3490 | `	 * constant-expression evaluation behind a declaration message` |
|         - |  3491 | `	 * (PH7_VmEvalConstExpr) builds the array a parameter defaults to, and every` |
|         - |  3492 | `	 * hashmap insert installs a reference-table entry. (While the table was a` |
|         - |  3493 | `` 	 * HASH, an unallocated one made that lookup index `apRefObj[hash & (0 - 1)]` `` |
|         - |  3494 | `	 * and segfaulted the compiler; the slot-indexed table answers "no record"` |
|         - |  3495 | `	 * for an out-of-range index instead, so this is now a head start rather than` |
|         - |  3496 | `	 * the thing standing between the compiler and a crash.) */` |
|      8450 |  3497 | `	pVm->nRefSize = 0x10;` |
|      8450 |  3498 | `	pVm->apRefObj = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * pVm->nRefSize);` |
|      8450 |  3499 | `	if( pVm->apRefObj == 0 ){` |
|       ! 0 |  3500 | `		rc = SXERR_MEM;` |
|       ! 0 |  3501 | `		goto Err;` |
|         - |  3502 | `	}` |
|         - |  3503 | `	/* Zero the reference table */` |
|      8450 |  3504 | `	SyZero(pVm->apRefObj,sizeof(void *) * pVm->nRefSize);` |
|         - |  3505 | `	/* Create the global frame */` |
|      8450 |  3506 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      8450 |  3507 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3508 | `		goto Err;` |
|         - |  3509 | `	}` |
|         - |  3510 | `	/* Initialize the code generator */` |
|      8450 |  3511 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      8450 |  3512 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3513 | `		goto Err;` |
|         - |  3514 | `	}` |
|         - |  3515 | `	/* VM correctly initialized,set the magic number */` |
|      8450 |  3516 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - |  3517 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - |  3518 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      8450 |  3519 | `	pVm->bCompilingBuiltin = 1;` |
|         - |  3520 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      8450 |  3521 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - |  3522 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - |  3523 | `	 * compiled — its classes are internal too. */` |
|         - |  3524 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      8450 |  3525 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      8450 |  3526 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      8450 |  3527 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      8450 |  3528 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      8450 |  3529 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - |  3530 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      8450 |  3531 | `	pVm->pCoalesceObj = 0;` |
|      8450 |  3532 | `	pVm->bCoalesceArmed = 0;` |
|      8450 |  3533 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - |  3534 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - |  3535 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - |  3536 | ``	 * segfaults the first `new Fiber`. */`` |
|      8450 |  3537 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      8450 |  3538 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - |  3539 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - |  3540 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - |  3541 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - |  3542 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - |  3543 | `	 * rather than being stamped on afterwards. */` |
|      8450 |  3544 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      8450 |  3545 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      8450 |  3546 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      8450 |  3547 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|      8450 |  3548 | `	pVm->bClosureUnbound = 0;` |
|      8450 |  3549 | `	pVm->pClosureMethodCls = 0;` |
|         - |  3550 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      8450 |  3551 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - |  3552 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      8450 |  3553 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - |  3554 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - |  3555 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - |  3556 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - |  3557 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - |  3558 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      8450 |  3559 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      8450 |  3560 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - |  3561 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - |  3562 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - |  3563 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      8450 |  3564 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - |  3565 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - |  3566 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - |  3567 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      8450 |  3568 | `	PH7_VmInstallReflection(&(*pVm));` |
|      8450 |  3569 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      8450 |  3570 | `	PH7_VmInstallSpl(&(*pVm));` |
|         - |  3571 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      8450 |  3572 | `	PH7_VmInstallHashContext(&(*pVm));` |
|         - |  3573 | `#endif` |
|         - |  3574 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - |  3575 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|         - |  3576 | `	 * builds its filters out of. */` |
|      8450 |  3577 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|         - |  3578 | `#endif` |
|      8450 |  3579 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|         - |  3580 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  3581 | `	/* php 8.4's RoundingMode, round()'s declared third argument. It rides the` |
|         - |  3582 | `	 * builtin guard because round() -- and bcround() -- do: a build with no` |
|         - |  3583 | `	 * consumer for the symbol does not ship the symbol. */` |
|      8450 |  3584 | `	PH7_VmInstallRoundingMode(&(*pVm));` |
|         - |  3585 | `	/* Pcntl\QosClass: php registers this pure enum on every platform, even the` |
|         - |  3586 | `	 * ones whose build has no function that reads it. */` |
|      8450 |  3587 | `	PH7_VmInstallPcntl(&(*pVm));` |
|         - |  3588 | `	/* BcMath\Number: after RoundingMode, whose cases its round() reads. */` |
|      8450 |  3589 | `	PH7_VmInstallBcMath(&(*pVm));` |
|         - |  3590 | `	/* php's ext/random object surface. It rides the builtin guard for the same` |
|         - |  3591 | `	 * reason bcmath does: the tiny build ships no consumer for it. */` |
|      8450 |  3592 | `	PH7_VmInstallRandom(&(*pVm));` |
|         - |  3593 | `	/* php's ext/fileinfo: the finfo class. It rides the builtin guard with the` |
|         - |  3594 | `	 * two above -- the tiny build ships none of its six functions. */` |
|      8450 |  3595 | `	PH7_VmInstallFileinfo(&(*pVm));` |
|         - |  3596 | `	/* ext/phar stands on SPL's directory iterators (a Phar IS one) and on` |
|         - |  3597 | `	 * ext/zlib for a compressed entry, so it mounts after both. */` |
|      8450 |  3598 | `	PH7_VmInstallPhar(&(*pVm));` |
|         - |  3599 | `#endif` |
|      8450 |  3600 | `	PH7_VmInstallSession(&(*pVm));` |
|      8450 |  3601 | `	PH7_VmInstallIni(&(*pVm));` |
|         - |  3602 | `#ifdef PH7_ENABLE_LIBXML` |
|         - |  3603 | `	/* libxml2-backed surfaces: shared plumbing first, then the ext/xml push` |
|         - |  3604 | `	 * parser and the DOM and XMLWriter class libraries that build on it. */` |
|      8450 |  3605 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      8450 |  3606 | `	PH7_VmInstallXml(&(*pVm));` |
|      8450 |  3607 | `	PH7_VmInstallDom(&(*pVm));` |
|      8450 |  3608 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - |  3609 | `	/* ext/simplexml stands on ext/dom's document shells and hands nodes back to` |
|         - |  3610 | `	 * it (dom_import_simplexml), so it mounts after DOMDocument exists. */` |
|      8450 |  3611 | `	PH7_VmInstallSimpleXml(&(*pVm));` |
|         - |  3612 | `#endif` |
|         - |  3613 | `#ifdef PH7_ENABLE_SQLITE` |
|         - |  3614 | ``	/* ext/pdo's class library first: `Pdo\Sqlite` extends PDO, so the driver's`` |
|         - |  3615 | `	 * installer needs the parent already mounted. */` |
|      8450 |  3616 | `	PH7_VmInstallPdo(&(*pVm));` |
|      8450 |  3617 | `	PH7_VmInstallPdoSqlite(&(*pVm));` |
|         - |  3618 | `	/* ext/sqlite3: php's other sqlite surface, independent of both. */` |
|      8450 |  3619 | `	PH7_VmInstallSqlite3(&(*pVm));` |
|         - |  3620 | `#endif` |
|         - |  3621 | `#ifdef PH7_ENABLE_CURL` |
|         - |  3622 | `	/* ext/curl: the libcurl binding. */` |
|      8450 |  3623 | `	PH7_VmInstallCurl(&(*pVm));` |
|         - |  3624 | `#endif` |
|         - |  3625 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - |  3626 | `	/* ext/sockets: the Socket/AddressInfo handle classes and php's BSD socket` |
|         - |  3627 | `	 * API over the descriptors net.c already drives for the stream wrappers. */` |
|         - |  3628 | `	{` |
|         - |  3629 | `		const ph7_builtin_func *aSock;` |
|      8450 |  3630 | `		sxu32 nSock = 0,n;` |
|      8450 |  3631 | `		PH7_VmInstallSockets(&(*pVm));` |
|      8450 |  3632 | `		aSock = PH7_SocketsFuncTable(&nSock);` |
|    320915 |  3633 | `		for( n = 0 ; n < nSock ; ++n ){` |
|    312470 |  3634 | `			ph7_create_function(&(*pVm),aSock[n].zName,aSock[n].xFunc,pVm);` |
|    156034 |  3635 | `		}` |
|         - |  3636 | `	}` |
|         - |  3637 | `#endif` |
|         - |  3638 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - |  3639 | `	/* ext/openssl: the three opaque handle classes and the extension's own` |
|         - |  3640 | `	 * functions, in two units -- the library-wide/cipher half and the` |
|         - |  3641 | `	 * certificate half. */` |
|         - |  3642 | `	{` |
|         - |  3643 | `		const ph7_builtin_func *aSsl;` |
|      8450 |  3644 | `		sxu32 nSsl = 0,n;` |
|      8450 |  3645 | `		PH7_VmInstallOpenSsl(&(*pVm));` |
|      8450 |  3646 | `		aSsl = PH7_OpenSslFuncTable(&nSsl);` |
|    295580 |  3647 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|    287135 |  3648 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|    143383 |  3649 | `		}` |
|      8450 |  3650 | `		aSsl = PH7_OpenSslX509FuncTable(&nSsl);` |
|    236465 |  3651 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|    228020 |  3652 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|    113864 |  3653 | `		}` |
|         - |  3654 | `	}` |
|         - |  3655 | `#endif` |
|         - |  3656 | `#ifdef PH7_ENABLE_ZLIB` |
|         - |  3657 | `	/* ext/zlib: the two context classes and the extension's own functions.` |
|         - |  3658 | `	 * Its gz* handle verbs are aliases registered beside the stream functions` |
|         - |  3659 | `	 * they are (vfs.c), and its device beside the other wrappers. */` |
|         - |  3660 | `	{` |
|         - |  3661 | `		const ph7_builtin_func *aZlib;` |
|      8450 |  3662 | `		sxu32 nZlib = 0,n;` |
|      8450 |  3663 | `		PH7_VmInstallZlib(&(*pVm));` |
|      8450 |  3664 | `		aZlib = PH7_ZlibFuncTable(&nZlib);` |
|    168905 |  3665 | `		for( n = 0 ; n < nZlib ; ++n ){` |
|    160460 |  3666 | `			ph7_create_function(&(*pVm),aZlib[n].zName,aZlib[n].xFunc,pVm);` |
|     80128 |  3667 | `		}` |
|         - |  3668 | `	}` |
|         - |  3669 | `	/* ext/zip: the ZipArchive class and the ten deprecated procedural verbs.` |
|         - |  3670 | `	 * It stands ON ext/zlib -- a deflated member is the format's normal case,` |
|         - |  3671 | `	 * and php's own build requires the library for the same reason -- so it` |
|         - |  3672 | `	 * mounts inside that guard and after it. */` |
|         - |  3673 | `	{` |
|         - |  3674 | `		const ph7_builtin_func *aZip;` |
|      8450 |  3675 | `		sxu32 nZip = 0,n;` |
|      8450 |  3676 | `		PH7_VmInstallZip(&(*pVm));` |
|      8450 |  3677 | `		aZip = PH7_ZipFuncTable(&nZip);` |
|     92900 |  3678 | `		for( n = 0 ; n < nZip ; ++n ){` |
|     84455 |  3679 | `			ph7_create_function(&(*pVm),aZip[n].zName,aZip[n].xFunc,pVm);` |
|     42175 |  3680 | `		}` |
|         - |  3681 | `	}` |
|         - |  3682 | `#endif` |
|         - |  3683 | `	/* Register the C builtins, LAST so that the order every name was inserted in` |
|         - |  3684 | `	 * is the order it was inserted in when these ran from PH7_VmMakeReady -- a hash` |
|         - |  3685 | `	 * walk is reverse-insertion, and get_defined_functions() and Reflection's` |
|         - |  3686 | `	 * extension surfaces both read one.` |
|         - |  3687 | `	 *` |
|         - |  3688 | `	 * They run HERE, and not after compilation as they used to, because the` |
|         - |  3689 | `	 * declaration guard has to be able to see them: a program is compiled between` |
|         - |  3690 | ``	 * VM init and PH7_VmMakeReady, and `function strlen(){}` in it silently WON`` |
|         - |  3691 | `	 * while the ~650 core names arrived too late to be found. php has its whole` |
|         - |  3692 | `	 * internal function table before it compiles anything, and now so does this.` |
|         - |  3693 | `	 * Nothing else the compiler does reads hHostFunction.` |
|         - |  3694 | `	 *` |
|         - |  3695 | `	 * What still belongs to PH7_VmMakeReady is everything STAMPED on top of these` |
|         - |  3696 | `	 * registrations -- arity, signatures, the deprecation marks and the` |
|         - |  3697 | `	 * language-construct mark -- which needs every extension's own pass to have run` |
|         - |  3698 | `	 * first, and the constants, which a compile-time constant expression resolves` |
|         - |  3699 | `	 * through its own table. */` |
|      8450 |  3700 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      8450 |  3701 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  3702 | `		goto Err;` |
|         - |  3703 | `	}` |
|      8450 |  3704 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|      8450 |  3705 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - |  3706 | `#ifdef PH7_ENABLE_PCRE` |
|      8450 |  3707 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|         - |  3708 | `#endif` |
|      8450 |  3709 | `	pVm->bCompilingBuiltin = 0;` |
|         - |  3710 | `	/* Reset the code generator */` |
|      8450 |  3711 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      8450 |  3712 | `	return SXRET_OK;` |
|       ! 0 |  3713 | `Err:` |
|       ! 0 |  3714 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 |  3715 | `	return rc;` |
|      4222 |  3716 | `}` |
|         - |  3717 | `/*` |
|         - |  3718 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - |  3719 | ` * routine which store the output in an internal blob.` |
|         - |  3720 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - |  3721 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - |  3722 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - |  3723 | ` * Refer to the official docurmentation for additional information.` |
|         - |  3724 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - |  3725 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - |  3726 | ` * to finish executing and extracting the output.` |
|         - |  3727 | ` */` |
|       358 |  3728 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - |  3729 | `	const void *pOut,   /* VM Generated output*/` |
|         - |  3730 | `	unsigned int nLen,  /* Generated output length */` |
|         - |  3731 | `	void *pUserData     /* User private data */` |
|         - |  3732 | `	)` |
|         4 |  3733 | `{` |
|         - |  3734 | `	 sxi32 rc;` |
|         - |  3735 | `	 /* Store the output in an internal BLOB */` |
|       362 |  3736 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       362 |  3737 | `	 return rc;` |
|         4 |  3738 | `}` |
|         - |  3739 | `/*` |
|         - |  3740 | ` * WHERE the response body began -- the file and the line php names in every` |
|         - |  3741 | ` * headers-already-sent diagnostic and hands back through headers_sent()'s two` |
|         - |  3742 | ` * by-ref out-params. Answers 0 while nothing has been emitted, which is php's` |
|         - |  3743 | ` * "" and 0 rather than a missing answer.` |
|         - |  3744 | ` */` |
|        30 |  3745 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|         4 |  3746 | `{` |
|        34 |  3747 | `	if( pFile ){` |
|        34 |  3748 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sOutStartFile),` |
|         - |  3749 | `			SyBlobLength(&pVm->sOutStartFile));` |
|        15 |  3750 | `	}` |
|        34 |  3751 | `	if( pnLine ){` |
|        34 |  3752 | `		*pnLine = pVm->nOutStartLine;` |
|        15 |  3753 | `	}` |
|        34 |  3754 | `	return SyBlobLength(&pVm->sOutStartFile) > 0;` |
|         4 |  3755 | `}` |
|         - |  3756 | `/*` |
|         - |  3757 | ` * WHERE the active session was started, which is the other half php names --` |
|         - |  3758 | ` * a session directive refused because a session is ACTIVE points at the` |
|         - |  3759 | ` * session_start() that opened it.` |
|         - |  3760 | ` */` |
|        12 |  3761 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|         2 |  3762 | `{` |
|        14 |  3763 | `	if( pFile ){` |
|        14 |  3764 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sSessStartFile),` |
|         - |  3765 | `			SyBlobLength(&pVm->sSessStartFile));` |
|         6 |  3766 | `	}` |
|        14 |  3767 | `	if( pnLine ){` |
|        14 |  3768 | `		*pnLine = pVm->nSessStartLine;` |
|         6 |  3769 | `	}` |
|        14 |  3770 | `	return SyBlobLength(&pVm->sSessStartFile) > 0;` |
|         2 |  3771 | `}` |
|         - |  3772 | `/*` |
|         - |  3773 | ` * php's provenance clause, appended to a session refusal: a session that is` |
|         - |  3774 | ` * ACTIVE points at the session_start() that opened it, and a response that has` |
|         - |  3775 | ` * already begun points at the output. Appends nothing when the place is not` |
|         - |  3776 | ` * known, which is the message php prints for a session no script started.` |
|         - |  3777 | ` */` |
|        18 |  3778 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive)` |
|         3 |  3779 | `{` |
|         - |  3780 | `	SyString sFile;` |
|        21 |  3781 | `	sxu32 nLine = 0;` |
|         - |  3782 | `	char zTail[64];` |
|        18 |  3783 | `	int bHave = bSessionActive ? PH7_VmSessionOrigin(pVm,&sFile,&nLine)` |
|        12 |  3784 | `	                           : PH7_VmOutputOrigin(pVm,&sFile,&nLine);` |
|        21 |  3785 | `	if( !bHave ){` |
|       ! 0 |  3786 | `		return;` |
|         - |  3787 | `	}` |
|         - |  3788 | `	{` |
|        21 |  3789 | `		const char *zOpen = bSessionActive ? " (started from " : " (sent from ";` |
|        21 |  3790 | `		SyBlobAppend(pMsg,zOpen,(sxu32)SyStrlen(zOpen));` |
|         - |  3791 | `	}` |
|        21 |  3792 | `	SyBlobAppend(pMsg,sFile.zString,sFile.nByte);` |
|        21 |  3793 | `	SyBufferFormat(zTail,sizeof(zTail)," on line %u)",nLine);` |
|        21 |  3794 | `	SyBlobAppend(pMsg,zTail,(sxu32)SyStrlen(zTail));` |
|        12 |  3795 | `}` |
|         - |  3796 | `/*` |
|         - |  3797 | ` * Record where the session now going ACTIVE was started.` |
|         - |  3798 | ` */` |
|        96 |  3799 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm)` |
|         4 |  3800 | `{` |
|       100 |  3801 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       100 |  3802 | `	SyBlobReset(&pVm->sSessStartFile);` |
|       100 |  3803 | `	if( pFile && pFile->nByte > 0 ){` |
|       100 |  3804 | `		SyBlobAppend(&pVm->sSessStartFile,pFile->zString,pFile->nByte);` |
|        48 |  3805 | `	}` |
|       100 |  3806 | `	pVm->nSessStartLine = pVm->nCurLine;` |
|       100 |  3807 | `}` |
|         - |  3808 | `/*` |
|         - |  3809 | ` * Track output length and mark headers as sent when output reaches` |
|         - |  3810 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - |  3811 | ` */` |
|    386054 |  3812 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 |  3813 | `{` |
|    386059 |  3814 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    386059 |  3815 | `	if( xCons != VmObConsumer ){` |
|    128899 |  3816 | `		pVm->nOutputLen += nLen;` |
|    128899 |  3817 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|         - |  3818 | `			/* The ORIGIN of the output is recorded with the flag, once: php's` |
|         - |  3819 | `			 * four headers-sent diagnostics all name the place the response` |
|         - |  3820 | `			 * body began, and headers_sent() hands the same pair back through` |
|         - |  3821 | `			 * its two by-ref out-params. */` |
|      3103 |  3822 | `			SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      3103 |  3823 | `			pVm->bHeadersSent = 1;` |
|      3103 |  3824 | `			SyBlobReset(&pVm->sOutStartFile);` |
|      3103 |  3825 | `			if( pFile && pFile->nByte > 0 ){` |
|      3103 |  3826 | `				SyBlobAppend(&pVm->sOutStartFile,pFile->zString,pFile->nByte);` |
|      1549 |  3827 | `			}` |
|      3103 |  3828 | `			pVm->nOutStartLine = pVm->nCurLine;` |
|      1549 |  3829 | `		}` |
|     63482 |  3830 | `	}` |
|    386059 |  3831 | `}` |
|         - |  3832 | `/*` |
|         - |  3833 | ` * Static operand-stack depth analysis.` |
|         - |  3834 | ` *` |
|         - |  3835 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - |  3836 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - |  3837 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - |  3838 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - |  3839 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - |  3840 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - |  3841 | ` *` |
|         - |  3842 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - |  3843 | ` * conservative BY CONSTRUCTION:` |
|         - |  3844 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - |  3845 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - |  3846 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - |  3847 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - |  3848 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - |  3849 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - |  3850 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - |  3851 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - |  3852 | ` *     bound. There is no partial/unsafe middle.` |
|         - |  3853 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - |  3854 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - |  3855 | ` *     instruction-count bound -> fallback.` |
|         - |  3856 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - |  3857 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - |  3858 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - |  3859 | ` *` |
|         - |  3860 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - |  3861 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - |  3862 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - |  3863 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - |  3864 | ` *` |
|         - |  3865 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - |  3866 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - |  3867 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - |  3868 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - |  3869 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - |  3870 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - |  3871 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - |  3872 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - |  3873 | ` * entry here.` |
|         - |  3874 | ` */` |
|         - |  3875 | `/*` |
|         - |  3876 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - |  3877 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - |  3878 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - |  3879 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - |  3880 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - |  3881 | ` */` |
|     99200 |  3882 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 |  3883 | `{` |
|     99205 |  3884 | `	int push = 0, n = 0;` |
|         - |  3885 | `	sxi32 d;` |
|     99205 |  3886 | `	switch( pI->iOp ){` |
|         - |  3887 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - |  3888 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     19006 |  3889 | `	case PH7_OP_LOADC:` |
|         - |  3890 | `	case PH7_OP_DUP:` |
|         - |  3891 | `	case PH7_OP_PICK:` |
|     37720 |  3892 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      7272 |  3893 | `	case PH7_OP_LOAD:` |
|     14521 |  3894 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|     14521 |  3895 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|        10 |  3896 | `	case PH7_OP_LOAD_REF:` |
|        21 |  3897 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - |  3898 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       692 |  3899 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - |  3900 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - |  3901 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1388 |  3902 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - |  3903 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - |  3904 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       334 |  3905 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - |  3906 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       649 |  3907 | `		if( pI->iP2 == 0 ){` |
|       649 |  3908 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       315 |  3909 | `		}else{` |
|       ! 0 |  3910 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 |  3911 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - |  3912 | `		}` |
|       649 |  3913 | `		break;` |
|         - |  3914 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - |  3915 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       217 |  3916 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - |  3917 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - |  3918 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - |  3919 | `	case PH7_OP_NOOP:` |
|         - |  3920 | `	case PH7_OP_SNAPSHOT:` |
|       437 |  3921 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - |  3922 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - |  3923 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|      1045 |  3924 | `	case PH7_OP_STORE:` |
|      2092 |  3925 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      2092 |  3926 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - |  3927 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      2080 |  3928 | `	case PH7_OP_POP:` |
|         - |  3929 | `	case PH7_OP_CONSUME:` |
|      4142 |  3930 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - |  3931 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - |  3932 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 |  3933 | `	case PH7_OP_ROT_CALLEE:` |
|         - |  3934 | `	case PH7_OP_NAMED_SEND:` |
|       ! 0 |  3935 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - |  3936 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - |  3937 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - |  3938 | `	 * true pop count is a runtime value — never reaches here. */` |
|       891 |  3939 | `	case PH7_OP_CALL:` |
|      1732 |  3940 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - |  3941 | `	/* Jumps. */` |
|       100 |  3942 | `	case PH7_OP_JMP:` |
|       205 |  3943 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       257 |  3944 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       516 |  3945 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       516 |  3946 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - |  3947 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      5016 |  3948 | `	case PH7_OP_DONE:` |
|      9994 |  3949 | `		n = 0; break;` |
|     13041 |  3950 | `	default:` |
|     25844 |  3951 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - |  3952 | `	}` |
|     73366 |  3953 | `	*pPush = push; *pN = n;` |
|     73366 |  3954 | `	return 1;` |
|     49244 |  3955 | `}` |
|         - |  3956 | `/*` |
|         - |  3957 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - |  3958 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - |  3959 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - |  3960 | ` */` |
|     29311 |  3961 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 |  3962 | `{` |
|         - |  3963 | `	void *pScratch;` |
|         - |  3964 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - |  3965 | `	sxu32 nQ, i, nIter, nCap;` |
|         - |  3966 | `	sxi32 iMax;` |
|         - |  3967 | `	int push, n, k;` |
|         - |  3968 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     29316 |  3969 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - |  3970 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 |  3971 | `		return VM_STACK_UNMODELED;` |
|         - |  3972 | `	}` |
|         - |  3973 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     91812 |  3974 | `	for( i = 0; i < nInstr; i++ ){` |
|     88340 |  3975 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     25844 |  3976 | `			return VM_STACK_UNMODELED;` |
|         - |  3977 | `		}` |
|     31056 |  3978 | `	}` |
|         - |  3979 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - |  3980 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - |  3981 | `	 * first (the byte array last needs no alignment). */` |
|      3477 |  3982 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      3477 |  3983 | `	if( pScratch == 0 ){` |
|       ! 0 |  3984 | `		return VM_STACK_UNMODELED;` |
|         - |  3985 | `	}` |
|      3477 |  3986 | `	aH  = (sxi32 *)pScratch;` |
|      3477 |  3987 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      3477 |  3988 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     17324 |  3989 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      3477 |  3990 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      3477 |  3991 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|     14342 |  3992 | `	while( nQ > 0 ){` |
|     10870 |  3993 | `		sxu32 pc = aQ[--nQ];` |
|         - |  3994 | `		sxi32 h;` |
|     10870 |  3995 | `		aIn[pc] = 0;` |
|     10870 |  3996 | `		h = aH[pc];` |
|     10870 |  3997 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|     10870 |  3998 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|     10870 |  3999 | `		if( h + push > iMax ){ iMax = h + push; }` |
|     10870 |  4000 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     18299 |  4001 | `		for( k = 0; k < n; k++ ){` |
|      7434 |  4002 | `			sxi32 hn = h + delta[k];` |
|      7434 |  4003 | `			sxu32 t = succ[k];` |
|      7434 |  4004 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      7434 |  4005 | `			if( hn > aH[t] ){` |
|      7398 |  4006 | `				aH[t] = hn;` |
|      7398 |  4007 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      3661 |  4008 | `			}` |
|      3684 |  4009 | `		}` |
|     10870 |  4010 | `		if( iMax < 0 ){ break; }` |
|         5 |  4011 | `	}` |
|      3477 |  4012 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      3477 |  4013 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     14532 |  4014 | `}` |
|         - |  4015 | `/*` |
|         - |  4016 | ` * Allocate a new operand stack so that we can start executing` |
|         - |  4017 | ` * our compiled PHP program.` |
|         - |  4018 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - |  4019 | ` * on success. NULL (Fatal error) on failure.` |
|         - |  4020 | ` *` |
|         - |  4021 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - |  4022 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - |  4023 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - |  4024 | ` * eval, coroutine, callbacks) call this directly.` |
|         - |  4025 | ` */` |
|   4608468 |  4026 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - |  4027 | `	ph7_vm *pVm, /* Target VM */` |
|         - |  4028 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - |  4029 | `	)` |
|         5 |  4030 | `{` |
|         - |  4031 | `	ph7_value *pStack;` |
|         - |  4032 | `  /* No instruction ever pushes more than a single element onto the` |
|         - |  4033 | `  ** stack and the stack never grows on successive executions of the` |
|         - |  4034 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - |  4035 | `  ** on the maximum stack depth required.` |
|         - |  4036 | `  **` |
|         - |  4037 | `  ** Allocation all the stack space we will ever need.` |
|         - |  4038 | `  */` |
|   4608473 |  4039 | `	nInstr += VM_STACK_GUARD;` |
|   4608473 |  4040 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   4608473 |  4041 | `	if( pStack == 0 ){` |
|       ! 0 |  4042 | `		return 0;` |
|         - |  4043 | `	}` |
|         - |  4044 | `	/* Initialize the operand stack. PH7_MemObjInit per slot is one call, one` |
|         - |  4045 | `	 * sizeof(ph7_value) SyZero and one SyBlobInit each; the whole buffer is` |
|         - |  4046 | `	 * contiguous, so zero it ONCE and fill in only the three fields a null value` |
|         - |  4047 | `	 * needs that are not zero. This is a hot path in its own right -- an OP_CALL` |
|         - |  4048 | `	 * that misses the recycling pool inits the callee's whole stack, and the fill` |
|         - |  4049 | `	 * loop below was 11% of a phpcs run. */` |
|   4608473 |  4050 | `	SyZero(pStack,nInstr * sizeof(ph7_value));` |
|  90187432 |  4051 | `	while( nInstr > 0 ){` |
|  85578964 |  4052 | `		ph7_value *pSlot = &pStack[--nInstr];` |
|  85578964 |  4053 | `		pSlot->pVm = pVm;` |
|  85578964 |  4054 | `		pSlot->sBlob.pAllocator = &pVm->sAllocator;` |
|  85578964 |  4055 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|         5 |  4056 | `	}` |
|         - |  4057 | `	/* Ready for bytecode execution */` |
|   4608473 |  4058 | `	return pStack;` |
|   2303892 |  4059 | `}` |
|         - |  4060 | `/*` |
|         - |  4061 | ` * Operand-stack recycling.` |
|         - |  4062 | ` *` |
|         - |  4063 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - |  4064 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - |  4065 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - |  4066 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - |  4067 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - |  4068 | ` * the per-slot PH7_MemObjInit.` |
|         - |  4069 | ` *` |
|         - |  4070 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - |  4071 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - |  4072 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - |  4073 | ` * cold/suspend/abort paths that never route through here. A buffer is reused` |
|         - |  4074 | ` * only for a request of exactly its size (never over-allocated), and the pool is` |
|         - |  4075 | ` * bounded three ways so it can't grow without end: entry count, per-buffer size,` |
|         - |  4076 | ` * and -- the one that actually bounds the MEMORY -- a total parked-slot budget.` |
|         - |  4077 | ` *` |
|         - |  4078 | ` * Only an EXACT size is reusable, so the size picks the chain: the pool is` |
|         - |  4079 | `` * PH7_STACK_POOL_BUCKETS separate LIFO lists indexed by `nCap & (BUCKETS-1)`, with`` |
|         - |  4080 | ` * the size still checked per node. It used to be ONE list walked end to end. That` |
|         - |  4081 | ` * replaced a head-only match, which was tuned for the design target -- recursion, or` |
|         - |  4082 | ` * a hot loop calling one function: one size, near-total reuse -- and which real code` |
|         - |  4083 | ` * (a dozen differently-sized functions in turn) missed on nearly every call, falling` |
|         - |  4084 | ` * back to a fresh buffer whose every slot had to be initialized: 11% of a phpcs run` |
|         - |  4085 | ` * sat in that init. Walking the whole list fixed the misses and bought its own` |
|         - |  4086 | ` * problem, because the cap that makes the reuse work is 256 buffers and a call` |
|         - |  4087 | ` * compared itself against all of them: 2.2% of the run, nearly all of it against` |
|         - |  4088 | ` * sizes it could never take. Keying by size keeps the hit rate and drops the walk.` |
|         - |  4089 | ` */` |
|         - |  4090 | `typedef struct VmIdleStack VmIdleStack;` |
|         - |  4091 | `struct VmIdleStack {` |
|         - |  4092 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - |  4093 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - |  4094 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - |  4095 | `};` |
|         - |  4096 | `#define VM_STACK_POOL_MAX 256      /* max buffers parked at once. A program calls far more` |
|         - |  4097 | `                                    * than 64 distinct-sized functions in its hot loop, and a` |
|         - |  4098 | `                                    * pool that is FULL turns every recycle into a free and` |
|         - |  4099 | `                                    * every call after it into a fresh, freshly-initialized` |
|         - |  4100 | `                                    * buffer: at 64 entries a phpcs run refused 86k of its` |
|         - |  4101 | `                                    * 400k recycles for being full and missed 24% of its` |
|         - |  4102 | `                                    * allocations. The memory this could cost is bounded by` |
|         - |  4103 | `                                    * the slot budget below, not by this count. */` |
|         - |  4104 | `#define VM_STACK_POOL_MAXSLOTS 4096 /* never pool a buffer bigger than this: one outlier` |
|         - |  4105 | `                                    * (an unmodelable body falls back to its whole` |
|         - |  4106 | `                                    * instruction count — 8000+ slots is real) must not sit` |
|         - |  4107 | `                                    * in the pool holding half a megabyte for a size nothing` |
|         - |  4108 | `                                    * asks for again */` |
|         - |  4109 | `#define VM_STACK_POOL_SLOTS 65536  /* total slots parked across the pool: ~4 MB of ph7_values,` |
|         - |  4110 | `                                    * the real bound on what recycling costs. Entry count and` |
|         - |  4111 | `                                    * per-buffer size are shape limits; this is the budget. */` |
|         - |  4112 | `/*` |
|         - |  4113 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - |  4114 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - |  4115 | ` */` |
|    885358 |  4116 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 |  4117 | `{` |
|    885363 |  4118 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|         - |  4119 | `	/* Only this size's own chain can hold a buffer this call can take. */` |
|    885363 |  4120 | `	VmIdleStack **ppIdle =` |
|    885358 |  4121 | `		(VmIdleStack **)&pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|    900225 |  4122 | `	while( *ppIdle ){` |
|    884692 |  4123 | `		VmIdleStack *pIdle = *ppIdle;` |
|    884692 |  4124 | `		if( pIdle->nCap == nCap ){` |
|    869830 |  4125 | `			ph7_value *pStack = pIdle->pStack;` |
|    869830 |  4126 | `			*ppIdle = pIdle->pNext;` |
|    869830 |  4127 | `			pVm->nIdleOperandStacks--;` |
|    869830 |  4128 | `			pVm->nIdleOperandSlots -= nCap;` |
|         - |  4129 | `			/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - |  4130 | `			 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    869830 |  4131 | `			pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    869830 |  4132 | `			pVm->pIdleStackNodes = pIdle;` |
|    869830 |  4133 | `			return pStack; /* slots already released -> reusable without re-init */` |
|         - |  4134 | `		}` |
|     14866 |  4135 | `		ppIdle = &pIdle->pNext;` |
|         4 |  4136 | `	}` |
|     15538 |  4137 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    442876 |  4138 | `}` |
|         - |  4139 | `/*` |
|         - |  4140 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - |  4141 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size); a parked` |
|         - |  4142 | ` * buffer must leave here with every slot released, so it can be handed to the next` |
|         - |  4143 | ` * call without re-initialization and can never retain a live value.` |
|         - |  4144 | ` *` |
|         - |  4145 | ` * nLive is how many slots the finishing activation could have touched -- its` |
|         - |  4146 | ` * operand-stack WATERMARK, the deepest its top ever reached (VmByteCodeExecBody` |
|         - |  4147 | ` * keeps it; the callee's record carries it here). Above that the buffer is still` |
|         - |  4148 | ` * exactly as it was handed out: clean. Releasing the whole capacity instead was` |
|         - |  4149 | ` * 1,044,870,548 releases on the phpcs step of record -- 38% of every release the` |
|         - |  4150 | ` * engine makes -- of which 34,354 found a value and 1,044,836,194 found a slot` |
|         - |  4151 | ` * that owned nothing. 11.85 million recycles walking 88 slots each, to free 34` |
|         - |  4152 | ` * thousand values, all of which live in the first handful of slots. The watermark` |
|         - |  4153 | ` * bounds the same sweep at 44.5 million slots (-95.7%) and finds every one of` |
|         - |  4154 | ` * those values; both corpora and the phpcs step agree that NOTHING above it is` |
|         - |  4155 | ` * ever dirty (the value-primitive census is what found it).` |
|         - |  4156 | ` *` |
|         - |  4157 | ` * The bound has to be the watermark and not the final top: a call abandons its` |
|         - |  4158 | `` * argument slots by lowering the top past them (`pTos = &pTos[-nCallArgs]`), so a`` |
|         - |  4159 | ` * body's own stack routinely carries dirt ABOVE where its top ends up -- 28,343` |
|         - |  4160 | ` * of those 11.85 million recycles. The watermark is above both by construction.` |
|         - |  4161 | ` */` |
|    885358 |  4162 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive)` |
|         5 |  4163 | `{` |
|         - |  4164 | `	VmIdleStack *pIdle;` |
|         - |  4165 | `	sxu32 i;` |
|    885363 |  4166 | `	if( pStack == 0 ){` |
|       ! 0 |  4167 | `		return;` |
|         - |  4168 | `	}` |
|    885358 |  4169 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX` |
|    881173 |  4170 | `	 \|\| nCap > VM_STACK_POOL_MAXSLOTS` |
|    876993 |  4171 | `	 \|\| pVm->nIdleOperandSlots + nCap > VM_STACK_POOL_SLOTS ){` |
|      8371 |  4172 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|      8371 |  4173 | `		return;` |
|         - |  4174 | `	}` |
|         - |  4175 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - |  4176 | `	 * pool-allocate only when the spare list is empty. */` |
|    876993 |  4177 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    876993 |  4178 | `	if( pIdle ){` |
|    869830 |  4179 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    435196 |  4180 | `	}else{` |
|      7168 |  4181 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      7168 |  4182 | `		if( pIdle == 0 ){` |
|       ! 0 |  4183 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 |  4184 | `			return;` |
|         - |  4185 | `		}` |
|         - |  4186 | `	}` |
|    876993 |  4187 | `	if( nLive > nCap ){` |
|       ! 0 |  4188 | `		nLive = nCap;   /* defensive: never walk past the buffer */` |
|       ! 0 |  4189 | `	}` |
|   3116680 |  4190 | `	for( i = 0; i < nLive; i++ ){` |
|   2239692 |  4191 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - |  4192 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - |  4193 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - |  4194 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - |  4195 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - |  4196 | `		 * across invocations — cheap defense in depth. The slots ABOVE nLive keep` |
|         - |  4197 | `		 * whatever index they had, which is exactly what a FRESH buffer looks like:` |
|         - |  4198 | `		 * VmNewOperandStack zeroes the array and never writes nIdx, so slot 0's` |
|         - |  4199 | `		 * index is what an untouched slot has always carried. */` |
|   2239692 |  4200 | `		pStack[i].nIdx = SXU32_HIGH;` |
|   1117701 |  4201 | `	}` |
|    876993 |  4202 | `	pIdle->pStack = pStack;` |
|    876993 |  4203 | `	pIdle->nCap = nCap;` |
|    876993 |  4204 | `	pIdle->pNext = (VmIdleStack *)pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|    876993 |  4205 | `	pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)] = pIdle;` |
|    876993 |  4206 | `	pVm->nIdleOperandStacks++;` |
|    876993 |  4207 | `	pVm->nIdleOperandSlots += nCap;` |
|    442876 |  4208 | `}` |
|         - |  4209 | `/*` |
|         - |  4210 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - |  4211 | ` * This routine gets called by the PH7 engine after` |
|         - |  4212 | ` * successful compilation of the target PHP program.` |
|         - |  4213 | ` */` |
|      6985 |  4214 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - |  4215 | `	ph7_vm *pVm /* Target VM */` |
|         - |  4216 | `	)` |
|         5 |  4217 | `{` |
|         - |  4218 | `	SyHashEntry *pEntry;` |
|         - |  4219 | `	sxi32 rc;` |
|      6990 |  4220 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - |  4221 | `		/* Initialize your VM first */` |
|       ! 0 |  4222 | `		return SXERR_CORRUPT;` |
|         - |  4223 | `	}` |
|         - |  4224 | `	/* Mark the VM ready for byte-code execution */` |
|      6990 |  4225 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - |  4226 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - |  4227 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - |  4228 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - |  4229 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      6990 |  4230 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - |  4231 | `	/* Emit the DONE instruction */` |
|      6990 |  4232 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      6990 |  4233 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  4234 | `		return SXERR_MEM;` |
|         - |  4235 | `	}` |
|         - |  4236 | `	/* Script return value */` |
|      6990 |  4237 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - |  4238 | `	/* Allocate a new operand stack */` |
|      6990 |  4239 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      6990 |  4240 | `	if( pVm->aOps == 0 ){` |
|       ! 0 |  4241 | `		return SXERR_MEM;` |
|         - |  4242 | `	}` |
|         - |  4243 | `	/* Set the default VM output consumer callback and it's` |
|         - |  4244 | `	 * private data. */` |
|      6990 |  4245 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      6990 |  4246 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - |  4247 | `	/* The host functions themselves -- the special ones, the core builtins, the HTTP` |
|         - |  4248 | `	 * response verbs and PCRE's -- were registered by PH7_VmInit, which runs before` |
|         - |  4249 | `	 * the program compiles; a declaration that shadows one has to be refused where` |
|         - |  4250 | `	 * php refuses it, and that is at compile time. Their constants and the metadata` |
|         - |  4251 | `	 * stamped onto them are still this routine's, below. */` |
|         - |  4252 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - |  4253 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - |  4254 | `	 * every object/variable created during execution) is per-exec state that` |
|         - |  4255 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - |  4256 | `	 * below it is compile-time/init state that survives a reset. */` |
|      6990 |  4257 | `	pVm->nSuperBaseline = pVm->aMemObj.nUsed;` |
|         - |  4258 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      6990 |  4259 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      6990 |  4260 | `	if( rc != SXRET_OK ){` |
|         - |  4261 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 |  4262 | `		return rc;` |
|         - |  4263 | `	}` |
|         - |  4264 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      6990 |  4265 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - |  4266 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      6990 |  4267 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - |  4268 | `	/* Register ext/pcntl's signal, priority and namespace constants (none of` |
|         - |  4269 | `	 * which exist on Windows, where php builds no ext/pcntl either) */` |
|      6990 |  4270 | `	PH7_RegisterPcntlConstants(&(*pVm));` |
|         - |  4271 | `	/* Register the LOG_* constants ext/standard's syslog trio reads */` |
|      6990 |  4272 | `	PH7_RegisterSyslogConstants(&(*pVm));` |
|         - |  4273 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - |  4274 | `	/* Register ext/sockets' AF_, SOCK_, SO_ and SOCKET_E family. php builds this` |
|         - |  4275 | `	 * extension on every platform, so unlike pcntl's these are not #ifdef'd` |
|         - |  4276 | `	 * away on Windows -- only the names that platform has no macro for are. */` |
|      6990 |  4277 | `	PH7_RegisterSocketsConstants(&(*pVm));` |
|         - |  4278 | `#endif` |
|         - |  4279 | `#ifdef PH7_ENABLE_PCRE` |
|         - |  4280 | `	/* Register PCRE constants [i.e: PREG_SPLIT_NO_EMPTY, PREG_PATTERN_ORDER, etc.] */` |
|      6990 |  4281 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - |  4282 | `#endif` |
|         - |  4283 | `#ifdef PH7_ENABLE_LIBXML` |
|         - |  4284 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      6990 |  4285 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - |  4286 | `#endif` |
|         - |  4287 | `#ifdef PH7_ENABLE_CURL` |
|         - |  4288 | `	/* Register the CURLOPT_* / CURLINFO_* / CURLE_* family */` |
|      6990 |  4289 | `	PH7_RegisterCurlConstants(&(*pVm));` |
|         - |  4290 | `#endif` |
|         - |  4291 | `	/* Every extension has registered its own constants by now, so the` |
|         - |  4292 | `	 * deprecation marks can be stamped on the names they cover wherever those` |
|         - |  4293 | `	 * were installed -- a name a build does not carry is simply skipped. */` |
|      6990 |  4294 | `	PH7_MarkDeprecatedConstants(&(*pVm));` |
|         - |  4295 | `	/* Same stamp for the FUNCTIONS and native methods php deprecated; the` |
|         - |  4296 | `	 * classes were installed by PH7_VmInit, well before this runs. */` |
|      6990 |  4297 | `	PH7_MarkDeprecatedFunctions(&(*pVm));` |
|         - |  4298 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - |  4299 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      6990 |  4300 | `	VmSetBuiltinArity(&(*pVm));` |
|         - |  4301 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      6990 |  4302 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - |  4303 | `	/* ...and hide the nine registrations that are language CONSTRUCTS rather than` |
|         - |  4304 | `	 * functions, now every table that could hold one has been filled. */` |
|      6990 |  4305 | `	PH7_VmMarkLanguageConstructs(&(*pVm));` |
|         - |  4306 | `	/* Initialize and install static and constants class attributes.` |
|         - |  4307 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - |  4308 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - |  4309 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - |  4310 | `	 * that function in sync when changing what is reserved here. */` |
|         - |  4311 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - |  4312 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - |  4313 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - |  4314 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - |  4315 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - |  4316 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - |  4317 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - |  4318 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - |  4319 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - |  4320 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      6990 |  4321 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   1741684 |  4322 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   1734699 |  4323 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   1734699 |  4324 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4325 | `			return rc;` |
|         - |  4326 | `		}` |
|         5 |  4327 | `	}` |
|      6990 |  4328 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   1740188 |  4329 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   1733209 |  4330 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   1733209 |  4331 | `		if( rc != SXRET_OK ){` |
|         8 |  4332 | `			return rc;` |
|         - |  4333 | `		}` |
|         5 |  4334 | `	}` |
|         - |  4335 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|         - |  4336 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      6984 |  4337 | `	pVm->nNextObjId = 1;` |
|         - |  4338 | `	/* VM is ready for bytecode execution */` |
|      6984 |  4339 | `	return SXRET_OK;` |
|      3492 |  4340 | `}` |
|         - |  4341 | `/*` |
|         - |  4342 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - |  4343 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - |  4344 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - |  4345 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - |  4346 | ` * a dangling node pointer in some other object's reference record.` |
|         - |  4347 | ` */` |
|        16 |  4348 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 |  4349 | `{` |
|         - |  4350 | `	/* VmRefSlotUnlink empties the cell it is given and decrements nRefUsed, so a` |
|         - |  4351 | `	 * sweep of the table leaves it empty and nRefUsed at 0 — no extra clearing` |
|         - |  4352 | `	 * needed. The cell array and nRefSize survive.` |
|         - |  4353 | `	 *` |
|         - |  4354 | `	 * Unlinking one cell deletes the names and array nodes it holds, which` |
|         - |  4355 | `	 * releases values, which can unlink OTHER cells — including ones this sweep` |
|         - |  4356 | `	 * has already passed, and (through a destructor) ones it has not created yet.` |
|         - |  4357 | `	 * So the sweep repeats while it is still making progress rather than trusting` |
|         - |  4358 | `	 * one pass, and stops the moment a pass frees nothing so it cannot spin. */` |
|        24 |  4359 | `	for(;;){` |
|        32 |  4360 | `		sxu32 n, nBefore = pVm->nRefUsed;` |
|        32 |  4361 | `		if( nBefore == 0 ){` |
|        16 |  4362 | `			break;` |
|         - |  4363 | `		}` |
|       912 |  4364 | `		for( n = 0 ; n < pVm->nRefSize ; ++n ){` |
|      1404 |  4365 | `			while( pVm->apRefObj[n] ){` |
|       508 |  4366 | `				void *pWord = pVm->apRefObj[n];` |
|       508 |  4367 | `				PH7_VmSlotUnlink(&(*pVm),n);` |
|       508 |  4368 | `				if( pVm->apRefObj[n] == pWord ){` |
|       ! 0 |  4369 | `					break; /* unlink did not clear it: do not spin on this cell */` |
|         - |  4370 | `				}` |
|       ! 0 |  4371 | `			}` |
|       448 |  4372 | `		}` |
|        16 |  4373 | `		if( pVm->nRefUsed >= nBefore ){` |
|       ! 0 |  4374 | `			break;` |
|         - |  4375 | `		}` |
|       ! 0 |  4376 | `	}` |
|        16 |  4377 | `}` |
|         - |  4378 | `/*` |
|         - |  4379 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - |  4380 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - |  4381 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - |  4382 | ` */` |
|        96 |  4383 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 |  4384 | `{` |
|        96 |  4385 | `	PH7_MemObjRelease(pObj);` |
|        96 |  4386 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        96 |  4387 | `}` |
|         - |  4388 | `/*` |
|         - |  4389 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|         - |  4390 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|         - |  4391 | ` * whole thing dies with the VM allocator either way.` |
|         - |  4392 | ` */` |
|        32 |  4393 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|       ! 0 |  4394 | `{` |
|        32 |  4395 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|         - |  4396 | `	sxu32 n;` |
|        32 |  4397 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|       ! 0 |  4398 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|       ! 0 |  4399 | `	}` |
|        32 |  4400 | `	SySetReset(pStack);` |
|        32 |  4401 | `}` |
|         - |  4402 | `/*` |
|         - |  4403 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - |  4404 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - |  4405 | ` * of statics).` |
|         - |  4406 | ` */` |
|     46081 |  4407 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|         5 |  4408 | `{` |
|     46086 |  4409 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - |  4410 | `	sxu32 k;` |
|     46094 |  4411 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         9 |  4412 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         5 |  4413 | `	}` |
|     46086 |  4414 | `}` |
|         - |  4415 | `/*` |
|         - |  4416 | ` * Tear down one run-time closure's per-instantiation ph7_vm_func: reset its` |
|         - |  4417 | ` * (template-shared) statics, release its captured-by-value environment, then free` |
|         - |  4418 | ` * the name buffer and the structure. The caller owns unlinking the hFunction row.` |
|         - |  4419 | ` */` |
|     21549 |  4420 | `static void VmFreeRuntimeClosure(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|         5 |  4421 | `{` |
|     21554 |  4422 | `	ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     21554 |  4423 | `	const char *zName = SyStringData(&pFunc->sName);` |
|         - |  4424 | `	sxu32 k;` |
|     21554 |  4425 | `	VmResetFuncStatics(pFunc);` |
|     54675 |  4426 | `	for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|     33126 |  4427 | `		PH7_MemObjRelease(&aEnv[k].sValue);` |
|     16424 |  4428 | `	}` |
|     21554 |  4429 | `	SySetRelease(&pFunc->aClosureEnv);` |
|     21554 |  4430 | `	if( zName ){` |
|     21554 |  4431 | `		SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|     10655 |  4432 | `	}` |
|     21554 |  4433 | `	SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     21554 |  4434 | `}` |
|         - |  4435 | `/*` |
|         - |  4436 | ` * The run-time closure this name belongs to, or NULL when the name is anything` |
|         - |  4437 | ` * else -- a named user function, a host function, a method. Only a` |
|         - |  4438 | ` * per-instantiation copy (VM_FUNC_CLOSURE, minted by OP_LOAD_CLOSURE) is owned by` |
|         - |  4439 | ` * the Closure objects that name it; everything else in hFunction outlives them.` |
|         - |  4440 | ` */` |
|     48033 |  4441 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 |  4442 | `{` |
|         - |  4443 | `	SyHashEntry *pEntry;` |
|         - |  4444 | `	ph7_vm_func *pFunc;` |
|     48038 |  4445 | `	if( zName == 0 \|\| nByte < 1 ){` |
|       ! 0 |  4446 | `		return 0;` |
|         - |  4447 | `	}` |
|     48038 |  4448 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nByte);` |
|     48038 |  4449 | `	if( pEntry == 0 ){` |
|      1395 |  4450 | `		return 0;` |
|         - |  4451 | `	}` |
|     46648 |  4452 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     46648 |  4453 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       927 |  4454 | `		return 0;` |
|         - |  4455 | `	}` |
|     45726 |  4456 | `	return pFunc;` |
|     23780 |  4457 | `}` |
|    916193 |  4458 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc)` |
|         5 |  4459 | `{` |
|    916198 |  4460 | `	if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|     88871 |  4461 | `		pFunc->nRef++;` |
|     44026 |  4462 | `	}` |
|    916198 |  4463 | `}` |
|         - |  4464 | `/*` |
|         - |  4465 | ` * Give back one hold on a run-time closure. At zero nothing can reach it any more` |
|         - |  4466 | ` * -- no Closure object names it and no activation is running it -- so it leaves` |
|         - |  4467 | ` * the function table and the memory goes back.` |
|         - |  4468 | ` */` |
|    915074 |  4469 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|         5 |  4470 | `{` |
|    915079 |  4471 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|    827322 |  4472 | `		return;` |
|         - |  4473 | `	}` |
|     87762 |  4474 | `	pFunc->nRef--;` |
|     87762 |  4475 | `	if( pFunc->nRef > 0 \|\| pVm->bInReset \|\| pFunc->bQueued ){` |
|     65247 |  4476 | `		return;` |
|         - |  4477 | `	}` |
|         - |  4478 | `	/* Queued, not freed on the spot. The drop that brings a closure to zero is` |
|         - |  4479 | `	 * usually OP_CALL releasing the Closure OBJECT it just unwrapped -- and the` |
|         - |  4480 | `	 * dispatch that did it is about to look the function up BY NAME. Freeing here` |
|         - |  4481 | ``	 * turned `(function(){ yield 1; })()` into "Call to undefined function`` |
|         - |  4482 | `	 * [closure_6]()". The list is drained at the VM's fetch point, where no dispatch` |
|         - |  4483 | `	 * is half-done; anything that took the function up again in between (the call's` |
|         - |  4484 | `	 * own frame does, one instruction later) is simply dropped from the list. */` |
|     22520 |  4485 | `	pFunc->bQueued = 1;` |
|     22520 |  4486 | `	if( SySetPut(&pVm->aDeadClosure,(const void *)&pFunc) != SXRET_OK ){` |
|       ! 0 |  4487 | `		pFunc->bQueued = 0;` |
|       ! 0 |  4488 | `		return; /* no room to remember it: it goes with the wholesale teardown */` |
|         - |  4489 | `	}` |
|     22520 |  4490 | `	pVm->bClosurePurge = 1;` |
|    457614 |  4491 | `}` |
|         - |  4492 | `/*` |
|         - |  4493 | ` * Free the run-time closures nothing needs any more. Called only from the VM's` |
|         - |  4494 | ` * fetch point, between two instructions, where no dispatch is half-resolved.` |
|         - |  4495 | ` *` |
|         - |  4496 | ` * Drained by POPPING: freeing one releases its captured environment, which can` |
|         - |  4497 | ` * release the last Closure object naming ANOTHER one and queue it mid-drain.` |
|         - |  4498 | ` */` |
|     20612 |  4499 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm)` |
|         5 |  4500 | `{` |
|     20617 |  4501 | `	pVm->bClosurePurge = 0;` |
|     31953 |  4502 | `	for(;;){` |
|     42605 |  4503 | `		ph7_vm_func **ppFunc = (ph7_vm_func **)SySetPop(&pVm->aDeadClosure);` |
|         - |  4504 | `		ph7_vm_func *pFunc;` |
|         - |  4505 | `		SyHashEntry *pEntry;` |
|     42605 |  4506 | `		if( ppFunc == 0 ){` |
|     20617 |  4507 | `			break;` |
|         - |  4508 | `		}` |
|     21993 |  4509 | `		pFunc = *ppFunc;` |
|     21993 |  4510 | `		pFunc->bQueued = 0;` |
|     21993 |  4511 | `		if( pFunc->nRef > 0 \|\| pVm->bInReset ){` |
|       448 |  4512 | `			continue; /* taken up again between the drop and here */` |
|         - |  4513 | `		}` |
|     32203 |  4514 | `		pEntry = SyHashGet(&pVm->hFunction,(const void *)SyStringData(&pFunc->sName),` |
|     10653 |  4515 | `			SyStringLength(&pFunc->sName));` |
|     21550 |  4516 | `		if( pEntry == 0 \|\| pEntry->pUserData != (void *)pFunc ){` |
|         - |  4517 | `			/* The name is not this copy's any more (an overload chain, a reset in` |
|         - |  4518 | `			 * flight): leave it to the wholesale teardown rather than guess. */` |
|       ! 0 |  4519 | `			continue;` |
|         - |  4520 | `		}` |
|     21550 |  4521 | `		SyHashDeleteEntry2(pEntry);` |
|     21550 |  4522 | `		VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|         5 |  4523 | `	}` |
|     20617 |  4524 | `}` |
|         - |  4525 | `/*` |
|         - |  4526 | `` * A Closure OBJECT taking or giving back its hold on the function `$__fn` names.`` |
|         - |  4527 | `` * One door for all of them: `function(){}` (OP_LOAD_CLOSURE via VmCreateClosure),`` |
|         - |  4528 | `` * `clone`, and `bindTo`/`bind`, which clones.`` |
|         - |  4529 | ` */` |
|     49179 |  4530 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta)` |
|         5 |  4531 | `{` |
|         - |  4532 | `	ph7_value *pFn;` |
|         - |  4533 | `	ph7_vm_func *pFunc;` |
|         - |  4534 | `	SyString sAttr;` |
|     49184 |  4535 | `	if( pObj == 0 \|\| pVm->pClosureClass == 0 \|\| pObj->pClass != pVm->pClosureClass ){` |
|      2307 |  4536 | `		return;` |
|         - |  4537 | `	}` |
|     48038 |  4538 | `	SyStringInitFromBuf(&sAttr,"__fn",4);` |
|     48038 |  4539 | `	pFn = PH7_ClassInstanceFetchAttr(pObj,&sAttr);` |
|     48038 |  4540 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 |  4541 | `		return;` |
|         - |  4542 | `	}` |
|     71813 |  4543 | `	pFunc = PH7_VmRuntimeClosure(&(*pVm),(const char *)SyBlobData(&pFn->sBlob),` |
|     23775 |  4544 | `		SyBlobLength(&pFn->sBlob));` |
|     48038 |  4545 | `	if( pFunc == 0 ){` |
|      2317 |  4546 | `		return;` |
|         - |  4547 | `	}` |
|     45726 |  4548 | `	if( iDelta > 0 ){` |
|     23420 |  4549 | `		PH7_VmClosureFuncRef(pFunc);` |
|     11591 |  4550 | `	}else{` |
|     22311 |  4551 | `		PH7_VmClosureFuncUnref(&(*pVm),pFunc);` |
|         - |  4552 | `	}` |
|     24353 |  4553 | `}` |
|         - |  4554 | `/*` |
|         - |  4555 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - |  4556 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - |  4557 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - |  4558 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - |  4559 | ` *    captured environment values, its name buffer and its structure (the` |
|         - |  4560 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - |  4561 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - |  4562 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - |  4563 | ` *    has its static sentinels reset.` |
|         - |  4564 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - |  4565 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - |  4566 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - |  4567 | ` */` |
|        16 |  4568 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 |  4569 | `{` |
|         - |  4570 | `	SyHashEntry *pEntry;` |
|        16 |  4571 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     24552 |  4572 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     24536 |  4573 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     24536 |  4574 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - |  4575 | `			/* Whatever run-time closures outlived their objects (one being executed` |
|         - |  4576 | `			 * when its last holder went, one the engine still names) go here. */` |
|         - |  4577 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 |  4578 | `			SyHashDeleteEntry2(pEntry);` |
|         4 |  4579 | `			VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|         4 |  4580 | `			continue;` |
|         - |  4581 | `		}` |
|         - |  4582 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     49064 |  4583 | `		while( pFunc ){` |
|     24532 |  4584 | `			VmResetFuncStatics(pFunc);` |
|     24532 |  4585 | `			pFunc = pFunc->pNextName;` |
|       ! 0 |  4586 | `		}` |
|       ! 0 |  4587 | `	}` |
|        16 |  4588 | `	pVm->closure_cnt = 0;` |
|        16 |  4589 | `}` |
|         - |  4590 | `/*` |
|         - |  4591 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - |  4592 | ` * are already gone (each object's destructor removed its own during the object` |
|         - |  4593 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - |  4594 | ` * the class re-mount registers fresh ones.` |
|         - |  4595 | ` */` |
|        16 |  4596 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 |  4597 | `{` |
|         - |  4598 | `	SyHashEntry *pEntry;` |
|         - |  4599 | `	/* The bitmap in front of the table goes with it, whether or not the table has` |
|         - |  4600 | `	 * anything left in it: a bit that outlived its entry would send a store into a` |
|         - |  4601 | `	 * lookup that answers nothing, and the class re-mount registers fresh ones. */` |
|        16 |  4602 | `	if( pVm->pFilterBits ){` |
|         4 |  4603 | `		SyZero(pVm->pFilterBits,pVm->nFilterBits >> 3);` |
|         2 |  4604 | `	}` |
|         - |  4605 | `	/* The table is emptied with it, so a bitmap that had been switched off can be` |
|         - |  4606 | `	 * trusted again from here. */` |
|        16 |  4607 | `	pVm->bFilterBitsOff = 0;` |
|         - |  4608 | `	/* Common case: no class static typed properties — table already empty. */` |
|        16 |  4609 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        12 |  4610 | `		return;` |
|         - |  4611 | `	}` |
|         - |  4612 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - |  4613 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 |  4614 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 |  4615 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 |  4616 | `		if( pEntry->pUserData ){` |
|         4 |  4617 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 |  4618 | `		}` |
|       ! 0 |  4619 | `	}` |
|         4 |  4620 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 |  4621 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 |  4622 | `	pVm->nNativeSetSlot = 0;` |
|         8 |  4623 | `}` |
|         - |  4624 | `/*` |
|         - |  4625 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - |  4626 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - |  4627 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - |  4628 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - |  4629 | ` * comparing equal — both used to cast to 1.` |
|         - |  4630 | ` *` |
|         - |  4631 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - |  4632 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - |  4633 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - |  4634 | ` * aborting a cast.` |
|         - |  4635 | ` */` |
|       466 |  4636 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         5 |  4637 | `{` |
|         - |  4638 | `	SyHashEntry *pEntry;` |
|         - |  4639 | `	phl_res_id *pRec;` |
|       471 |  4640 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 |  4641 | `		return 0;` |
|         - |  4642 | `	}` |
|       471 |  4643 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       471 |  4644 | `	if( pEntry ){` |
|       417 |  4645 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - |  4646 | `	}` |
|        59 |  4647 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        59 |  4648 | `	if( pRec == 0 ){` |
|       ! 0 |  4649 | `		return 0;` |
|         - |  4650 | `	}` |
|        59 |  4651 | `	pRec->pRes = pRes;` |
|        59 |  4652 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        59 |  4653 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 |  4654 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 |  4655 | `		return 0;` |
|         - |  4656 | `	}` |
|        59 |  4657 | `	return pRec->nId;` |
|       238 |  4658 | `}` |
|         - |  4659 | `/*` |
|         - |  4660 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - |  4661 | ` * 1 for the next run, matching a fresh php process.` |
|         - |  4662 | ` */` |
|        16 |  4663 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 |  4664 | `{` |
|         - |  4665 | `	SyHashEntry *pEntry;` |
|        16 |  4666 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        16 |  4667 | `		pVm->nResourceIdNext = 1;` |
|        16 |  4668 | `		return;` |
|         - |  4669 | `	}` |
|       ! 0 |  4670 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 |  4671 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 |  4672 | `		if( pEntry->pUserData ){` |
|       ! 0 |  4673 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 |  4674 | `		}` |
|       ! 0 |  4675 | `	}` |
|       ! 0 |  4676 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 |  4677 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 |  4678 | `	pVm->nResourceIdNext = 1;` |
|         8 |  4679 | `}` |
|         - |  4680 | `/*` |
|         - |  4681 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - |  4682 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - |  4683 | ` *` |
|         - |  4684 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - |  4685 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - |  4686 | ` * (a re-run define() overwrites the value in place: nRunGen tells it from a` |
|         - |  4687 | ` * redefinition inside one run, which php refuses), included-file markers` |
|         - |  4688 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - |  4689 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - |  4690 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - |  4691 | ` *` |
|         - |  4692 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - |  4693 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - |  4694 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - |  4695 | ` * exception/error-handler state, the reference table and every object/array` |
|         - |  4696 | ` * reserved during the run.` |
|         - |  4697 | ` *` |
|         - |  4698 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - |  4699 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - |  4700 | ` * global-scope destructors never fired.` |
|         - |  4701 | ` */` |
|        16 |  4702 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 |  4703 | `{` |
|         - |  4704 | `	sxu32 nWater,n;` |
|        16 |  4705 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 |  4706 | `		return SXERR_CORRUPT;` |
|         - |  4707 | `	}` |
|        16 |  4708 | `	nWater = pVm->nSuperBaseline;` |
|         - |  4709 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - |  4710 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        16 |  4711 | `	pVm->pGlobal = 0;` |
|         - |  4712 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - |  4713 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - |  4714 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - |  4715 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - |  4716 | `	 * object); unref'ing here would race the teardown below. */` |
|        16 |  4717 | `	pVm->pClosureThis = 0;` |
|        16 |  4718 | `	pVm->pClosureScope = 0;` |
|        16 |  4719 | `	pVm->bClosureUnbound = 0;` |
|        16 |  4720 | `	pVm->pClosureMethodCls = 0;` |
|         - |  4721 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - |  4722 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - |  4723 | `	 * here is unsafe. Engine memory is still reclaimed. Mirrors prior behaviour` |
|         - |  4724 | `	 * (global destructors never ran). */` |
|        16 |  4725 | `	pVm->bInReset = 1;` |
|         - |  4726 | `	/* (0) Forget every buffered cycle root. The object pool is about to go, and a` |
|         - |  4727 | `	 * row that outlived it would name freed memory on the next run. */` |
|        16 |  4728 | `	PH7_GcResetBuffer(&(*pVm));` |
|         - |  4729 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        16 |  4730 | `	VmResetRefTable(&(*pVm));` |
|         - |  4731 | `	/* (1b) The pending-free list names functions the wholesale teardown below is` |
|         - |  4732 | `	 * about to free anyway; forget it rather than leave rows pointing at them. */` |
|        16 |  4733 | `	SySetReset(&pVm->aDeadClosure);` |
|        16 |  4734 | `	pVm->bClosurePurge = 0;` |
|         - |  4735 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - |  4736 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - |  4737 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - |  4738 | `	 * re-run define() overwrites the value in place -- see nRunGen). */` |
|        16 |  4739 | `	VmResetFunctionState(&(*pVm));` |
|         - |  4740 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - |  4741 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       564 |  4742 | `	for( n = nWater ; n < pVm->aMemObj.nUsed ; ++n ){` |
|       548 |  4743 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|       548 |  4744 | `		if( pObj ){` |
|       548 |  4745 | `			PH7_MemObjRelease(pObj);` |
|       274 |  4746 | `		}` |
|       274 |  4747 | `	}` |
|         - |  4748 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - |  4749 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        16 |  4750 | `	VmResetTypedSlots(&(*pVm));` |
|         - |  4751 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - |  4752 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        16 |  4753 | `	VmResetResourceIds(&(*pVm));` |
|         - |  4754 | `	/* (5) Unwind any active frames back to none. */` |
|        32 |  4755 | `	while( pVm->pFrame ){` |
|        16 |  4756 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 |  4757 | `	}` |
|         - |  4758 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        16 |  4759 | `	pVm->bInReset = 0;` |
|         - |  4760 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - |  4761 | `	 * slots (their indices no longer exist). Fully-free trailing segments are` |
|         - |  4762 | `	 * returned by VmMemPoolTruncate. */` |
|        16 |  4763 | `	VmMemPoolTruncate(&pVm->aMemObj,nWater);` |
|         - |  4764 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        16 |  4765 | `	SyHashRelease(&pVm->hSuper);` |
|        16 |  4766 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        16 |  4767 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|         - |  4768 | `	/* A reused VM (the -S server, the in-process .phpt runner) starts the next run` |
|         - |  4769 | `	 * with the bytecode of the last one still holding its screened-at stamps. */` |
|        16 |  4770 | `	pVm->nCallableGen++;` |
|        16 |  4771 | `	pVm->nConstGen++;` |
|        16 |  4772 | `	pVm->nRunGen++;` |
|         - |  4773 | `	/* (8) Drain remaining per-exec containers. */` |
|        16 |  4774 | `	SySetReset(&pVm->aSelf);` |
|         - |  4775 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - |  4776 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - |  4777 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        16 |  4778 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 |  4779 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 |  4780 | `		if( pCB ){` |
|         - |  4781 | `			int iArg;` |
|       ! 0 |  4782 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 |  4783 | `			PH7_MemObjRelease(&pCB->sInvoke);` |
|       ! 0 |  4784 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 |  4785 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 |  4786 | `			}` |
|       ! 0 |  4787 | `		}` |
|       ! 0 |  4788 | `	}` |
|        16 |  4789 | `	SySetReset(&pVm->aShutdown);` |
|         - |  4790 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - |  4791 | `	 * aborted program can leave entries behind). */` |
|        16 |  4792 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        16 |  4793 | `	SySetReset(&pVm->aException);` |
|        16 |  4794 | `	SySetReset(&pVm->aFinallyAction);` |
|        16 |  4795 | `	pVm->pPendingException = 0;` |
|        16 |  4796 | `	pVm->pInflightException = 0;` |
|        16 |  4797 | `	pVm->nInflightExcBase = 0;` |
|        16 |  4798 | `	VmClearResumeTarget(&(*pVm));` |
|        16 |  4799 | `	pVm->nBoundaryRc = 0;` |
|        16 |  4800 | `	PH7_CmpRefusalClear(&(*pVm));` |
|        16 |  4801 | `	pVm->pConstEvalClass = 0;` |
|        16 |  4802 | `	pVm->nConstEvalDepth = 0;` |
|        16 |  4803 | `	pVm->pConstCycleAttr = 0;` |
|        16 |  4804 | `	pVm->pConstCycleClass = 0;` |
|        16 |  4805 | `	SySetReset(&pVm->aMagicGuard);` |
|         - |  4806 | `	{` |
|         - |  4807 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - |  4808 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        16 |  4809 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        16 |  4810 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - |  4811 | `		sxu32 iRmw;` |
|        16 |  4812 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 |  4813 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 |  4814 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 |  4815 | `		}` |
|        16 |  4816 | `		SySetReset(&pVm->aHookRmw);` |
|         - |  4817 | `	}` |
|        16 |  4818 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 |  4819 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 |  4820 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 |  4821 | `	}` |
|        16 |  4822 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        16 |  4823 | `	if( pVm->pHookSetThis ){` |
|       ! 0 |  4824 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 |  4825 | `		pVm->pHookSetThis = 0;` |
|       ! 0 |  4826 | `	}` |
|        16 |  4827 | `	pVm->pHookSetAttr = 0;` |
|        16 |  4828 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        16 |  4829 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 |  4830 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 |  4831 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 |  4832 | `	}` |
|        16 |  4833 | `	pVm->pMagicCallClass = 0;` |
|        16 |  4834 | `	pVm->pMagicCallLsb = 0;` |
|        16 |  4835 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        16 |  4836 | `	pVm->nExceptDepth = 0;` |
|         - |  4837 | `	/* spl_autoload_register() callbacks are per request */` |
|        16 |  4838 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 |  4839 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 |  4840 | `		if( pCB ){` |
|       ! 0 |  4841 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 |  4842 | `			PH7_MemObjRelease(&pCB->sInvoke);` |
|       ! 0 |  4843 | `		}` |
|       ! 0 |  4844 | `	}` |
|        16 |  4845 | `	SySetReset(&pVm->aAutoload);` |
|         - |  4846 | `	/* ...and so is the extension list they are searched with. */` |
|        16 |  4847 | `	SyBlobReset(&pVm->sAutoloadExt);` |
|        16 |  4848 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|         - |  4849 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - |  4850 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        16 |  4851 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 |  4852 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 |  4853 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 |  4854 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 |  4855 | `	}` |
|         - |  4856 | `	/* Output buffers */` |
|        16 |  4857 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 |  4858 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 |  4859 | `		if( pOb ){` |
|       ! 0 |  4860 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 |  4861 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 |  4862 | `		}` |
|       ! 0 |  4863 | `	}` |
|        16 |  4864 | `	SySetReset(&pVm->aOB);` |
|        16 |  4865 | `	pVm->nObDepth = 0;` |
|         - |  4866 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - |  4867 | `	{` |
|        16 |  4868 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        16 |  4869 | `		if( rc == SXRET_OK ){` |
|        16 |  4870 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         8 |  4871 | `		}` |
|        16 |  4872 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  4873 | `			return rc;` |
|         - |  4874 | `		}` |
|         - |  4875 | `	}` |
|         - |  4876 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - |  4877 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - |  4878 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - |  4879 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - |  4880 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - |  4881 | `	{` |
|         - |  4882 | `		SyHashEntry *pEntry;` |
|        16 |  4883 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3988 |  4884 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3972 |  4885 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - |  4886 | `			ph7_class_attr *pAttr;` |
|         - |  4887 | `			SyHashEntry *pAttrEntry;` |
|      3972 |  4888 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     32650 |  4889 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     26692 |  4890 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     26692 |  4891 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 |  4892 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 |  4893 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 |  4894 | `				}` |
|       ! 0 |  4895 | `			}` |
|         - |  4896 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - |  4897 | `			 * slots too so VM reuse re-evaluates them. */` |
|      3972 |  4898 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|     16788 |  4899 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|     12816 |  4900 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     12816 |  4901 | `				pAttr->nIdx = SXU32_HIGH;` |
|     12816 |  4902 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 |  4903 | `			}` |
|       ! 0 |  4904 | `		}` |
|        16 |  4905 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3988 |  4906 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3972 |  4907 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      3972 |  4908 | `			if( rc != SXRET_OK ){` |
|       ! 0 |  4909 | `				return rc;` |
|         - |  4910 | `			}` |
|       ! 0 |  4911 | `		}` |
|         - |  4912 | `	}` |
|         - |  4913 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        16 |  4914 | `	SyBlobReset(&pVm->sConsumer);` |
|        16 |  4915 | `	pVm->nOutputLen = 0;` |
|        16 |  4916 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        16 |  4917 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|         - |  4918 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|         - |  4919 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|        16 |  4920 | `	pVm->iResponseStatus = 0;` |
|        16 |  4921 | `	pVm->bHeadersSent = 0;` |
|        16 |  4922 | `	SyBlobReset(&pVm->sOutStartFile);` |
|        16 |  4923 | `	pVm->nOutStartLine = 0;` |
|        16 |  4924 | `	SyBlobReset(&pVm->sSessStartFile);` |
|        16 |  4925 | `	pVm->nSessStartLine = 0;` |
|        16 |  4926 | `	pVm->bHttpContext = 0;` |
|        16 |  4927 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|        16 |  4928 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|        16 |  4929 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|        16 |  4930 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|        16 |  4931 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|        16 |  4932 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         - |  4933 | `	/* The session's userland save handler belongs to the request that installed` |
|         - |  4934 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|         - |  4935 | `	 * through the previous script's object. */` |
|        16 |  4936 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|        16 |  4937 | `	pVm->bSessOpened = 0;` |
|        16 |  4938 | `	SyBlobReset(&pVm->sSessData);` |
|        16 |  4939 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - |  4940 | `#ifdef PH7_ENABLE_PCRE` |
|        16 |  4941 | `	pVm->iPcreLastError = 0;` |
|         - |  4942 | `#endif` |
|         - |  4943 | `#ifdef PH7_ENABLE_LIBXML` |
|         - |  4944 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        16 |  4945 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - |  4946 | `#endif` |
|         - |  4947 | `#ifdef PH7_ENABLE_SQLITE` |
|         - |  4948 | `	/* Close the previous request's databases: a reused VM (the -S server's)` |
|         - |  4949 | `	 * must not answer the next request through a handle that request opened. */` |
|        16 |  4950 | `	PH7_PdoVmReset(&(*pVm));` |
|        16 |  4951 | `	PH7_Sqlite3VmReset(&(*pVm));` |
|         - |  4952 | `#endif` |
|         - |  4953 | `#ifdef PH7_ENABLE_CURL` |
|         - |  4954 | `	/* Same rule for the previous request's curl handles, which hold sockets` |
|         - |  4955 | `	 * and a connection cache of their own. */` |
|        16 |  4956 | `	PH7_CurlVmReset(&(*pVm));` |
|         - |  4957 | `#endif` |
|         - |  4958 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - |  4959 | `	/* And every ext/sockets descriptor: a reused VM must not leave the previous` |
|         - |  4960 | `	 * request's listener bound to its port. */` |
|        16 |  4961 | `	PH7_SocketsVmReset(&(*pVm));` |
|         - |  4962 | `#endif` |
|         - |  4963 | `	/* php's "last opened directory stream" is per REQUEST: a reused VM must not` |
|         - |  4964 | `	 * let readdir() with no argument reach the previous one's handle. */` |
|        16 |  4965 | `	pVm->pLastDir = 0;` |
|         - |  4966 | `#ifdef PH7_ENABLE_ZLIB` |
|         - |  4967 | `	/* And its deflate/inflate contexts: a z_stream's window is libz's own` |
|         - |  4968 | `	 * allocation, which the wholesale release below would not reach. */` |
|        16 |  4969 | `	PH7_ZlibVmReset(&(*pVm));` |
|        16 |  4970 | `	PH7_ZipVmReset(&(*pVm));` |
|         - |  4971 | `#endif` |
|         - |  4972 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - |  4973 | `	/* And every certificate, key and signing request still held: each is` |
|         - |  4974 | `	 * OpenSSL's own allocation, outside the backend the release below wipes. */` |
|        16 |  4975 | `	PH7_SslVmReset(&(*pVm));` |
|         - |  4976 | `#endif` |
|         - |  4977 | `	/* And every archive it opened: php's phar cache is per-request too. */` |
|        16 |  4978 | `	PH7_PharVmReset(&(*pVm));` |
|         - |  4979 | `	/* Drop the stream contexts this run created, the default one included: a` |
|         - |  4980 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|         - |  4981 | `	 * previous one's stream_context_set_default(). */` |
|        16 |  4982 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|         - |  4983 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|         - |  4984 | `	 * still owns memory the next request must not inherit. */` |
|        16 |  4985 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|         - |  4986 | `	/* And the last http:// exchange's response headers, for the same reason:` |
|         - |  4987 | `	 * http_get_last_response_headers() must not answer the previous request's. */` |
|        16 |  4988 | `	PH7_HttpClearResponseHeaders(&(*pVm));` |
|        16 |  4989 | `	pVm->iCmpCallbackExc = 0;` |
|        16 |  4990 | `	pVm->bHaltRequested = 0;` |
|        16 |  4991 | `	pVm->iExitStatus = 0;` |
|        16 |  4992 | `	pVm->nSpreadCallBase = 0;` |
|        16 |  4993 | `	VmSpreadCaptureReset(pVm);` |
|        16 |  4994 | `	pVm->nRecursionDepth = 0;` |
|        16 |  4995 | `	pVm->pActiveCtx = 0;` |
|        16 |  4996 | `	pVm->pCurFiber = 0;` |
|        16 |  4997 | `	pVm->pCoalesceObj = 0;` |
|        16 |  4998 | `	pVm->bCoalesceArmed = 0;` |
|        16 |  4999 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - |  5000 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - |  5001 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        16 |  5002 | `	pVm->nNextObjId = 1;` |
|         - |  5003 | `	/* Set the ready flag */` |
|        16 |  5004 | `	pVm->nMagic = PH7_VM_RUN;` |
|        16 |  5005 | `	return SXRET_OK;` |
|         8 |  5006 | `}` |
|         - |  5007 | `/*` |
|         - |  5008 | ` * Release a Virtual Machine.` |
|         - |  5009 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - |  5010 | ` */` |
|      6995 |  5011 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 |  5012 | `{` |
|         - |  5013 | `	/* Set the stale magic number */` |
|      7000 |  5014 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - |  5015 | `#ifdef PH7_ENABLE_LIBXML` |
|         - |  5016 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - |  5017 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      7000 |  5018 | `	PH7_LibxmlVmRelease(pVm);` |
|         - |  5019 | `#endif` |
|         - |  5020 | `#ifdef PH7_ENABLE_SQLITE` |
|         - |  5021 | `	/* Same rule for the sqlite3 handles behind still-open PDO objects. */` |
|      7000 |  5022 | `	PH7_PdoVmRelease(pVm);` |
|      7000 |  5023 | `	PH7_Sqlite3VmRelease(pVm);` |
|         - |  5024 | `#endif` |
|         - |  5025 | `#ifdef PH7_ENABLE_CURL` |
|         - |  5026 | `	/* Same rule for the libcurl handles behind still-open CurlHandle objects. */` |
|      7000 |  5027 | `	PH7_CurlVmRelease(pVm);` |
|         - |  5028 | `#endif` |
|         - |  5029 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - |  5030 | `	/* Same rule for the descriptors behind still-open Socket objects. */` |
|      7000 |  5031 | `	PH7_SocketsVmRelease(pVm);` |
|         - |  5032 | `#endif` |
|         - |  5033 | `#ifdef PH7_ENABLE_ZLIB` |
|         - |  5034 | `	/* Same rule for the z_streams behind still-open Deflate/InflateContexts. */` |
|      7000 |  5035 | `	PH7_ZlibVmRelease(pVm);` |
|         - |  5036 | `	/* ...and for the archives behind still-open ZipArchives, whose entry` |
|         - |  5037 | `	 * tables are this allocator's but whose lifetime is not the object's. */` |
|      7000 |  5038 | `	PH7_ZipVmRelease(pVm);` |
|         - |  5039 | `#endif` |
|         - |  5040 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - |  5041 | `	/* Same rule for the X509/EVP_PKEY handles behind still-open objects. */` |
|      7000 |  5042 | `	PH7_SslVmRelease(pVm);` |
|         - |  5043 | `#endif` |
|         - |  5044 | `	/* ext/pcntl put a C signal handler in front of this VM's handler table;` |
|         - |  5045 | `	 * every disposition it took over goes back to SIG_DFL before the allocator` |
|         - |  5046 | `	 * that table lives in disappears. */` |
|      7000 |  5047 | `	PH7_PcntlVmRelease(pVm);` |
|         - |  5048 | `	/* ...and for the syslog prefix a still-open openlog() points at. */` |
|      7000 |  5049 | `	PH7_SyslogVmRelease(pVm);` |
|      7000 |  5050 | `	PH7_PharVmRelease(pVm);` |
|         - |  5051 | `	/* Same rule for the OS directory streams behind still-open directory` |
|         - |  5052 | `	 * iterators: the DIR lives outside the backend. */` |
|      7000 |  5053 | `	PH7_SplDirVmRelease(pVm);` |
|      7000 |  5054 | `	SySetRelease(&pVm->aDeadClosure);` |
|      7000 |  5055 | `	PH7_GcRelease(pVm);` |
|         - |  5056 | `	/* Release the private memory subsystem */` |
|      7000 |  5057 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      7000 |  5058 | `	return SXRET_OK;` |
|         5 |  5059 | `}` |
|         - |  5060 | `/*` |
|         - |  5061 | ` * Initialize a foreign function call context.` |
|         - |  5062 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - |  5063 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - |  5064 | ` * functions.` |
|         - |  5065 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - |  5066 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - |  5067 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - |  5068 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - |  5069 | ` */` |
|  19258975 |  5070 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - |  5071 | `	ph7_context *pOut,    /* Call Context */` |
|         - |  5072 | `	ph7_vm *pVm,          /* Target VM */` |
|         - |  5073 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - |  5074 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - |  5075 | `	sxi32 iFlags          /* Control flags */` |
|         - |  5076 | `	)` |
|         5 |  5077 | `{` |
|  19258980 |  5078 | `	pOut->pFunc = pFunc;` |
|  19258980 |  5079 | `	pOut->pVm   = pVm;` |
|  19258980 |  5080 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|  19258980 |  5081 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  5082 | `	/* Assume a null return value */` |
|  19258980 |  5083 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|  19258980 |  5084 | `	pOut->pRet = pRet;` |
|  19258980 |  5085 | `	pOut->iFlags = iFlags;` |
|  19258980 |  5086 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|  19258980 |  5087 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - |  5088 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - |  5089 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - |  5090 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - |  5091 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|  19258980 |  5092 | `	pOut->pThis = 0;` |
|  19258980 |  5093 | `	pOut->pCalledClass = 0;` |
|  19258980 |  5094 | `	pOut->bThisInit = 0;` |
|         - |  5095 | `	/* Only the scratch context a native PROPERTY handler runs on carries one; every` |
|         - |  5096 | `	 * ordinary call leaves it empty, so a refusal there throws as it always did. */` |
|  19258980 |  5097 | `	pOut->pPropCtx = 0;` |
|  19258980 |  5098 | `	return SXRET_OK;` |
|         5 |  5099 | `}` |
|         - |  5100 | `/*` |
|         - |  5101 | ` * Release a foreign function call context and cleanup the mess` |
|         - |  5102 | ` * left behind.` |
|         - |  5103 | ` */` |
|  19258991 |  5104 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 |  5105 | `{` |
|         - |  5106 | `	sxu32 n;` |
|  19258996 |  5107 | `	if( pCtx->bThisInit ){` |
|         - |  5108 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - |  5109 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - |  5110 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - |  5111 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|     10673 |  5112 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|     10673 |  5113 | `		pCtx->sThis.x.pOther = 0;` |
|     10673 |  5114 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|     10673 |  5115 | `		pCtx->bThisInit = 0;` |
|      5334 |  5116 | `	}` |
|  19258996 |  5117 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    856268 |  5118 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|   2168006 |  5119 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|   1311743 |  5120 | `			if( apObj[n] == 0 ){` |
|         - |  5121 | `				/* Already released */` |
|     59328 |  5122 | `				continue;` |
|         - |  5123 | `			}` |
|   1252420 |  5124 | `			PH7_MemObjRelease(apObj[n]);` |
|   1252420 |  5125 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    626111 |  5126 | `		}` |
|    856268 |  5127 | `		SySetRelease(&pCtx->sVar);` |
|    428092 |  5128 | `	}` |
|  19258996 |  5129 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - |  5130 | `		ph7_aux_data *aAux;` |
|         - |  5131 | `		void *pChunk;` |
|         - |  5132 | `		/* Automatic release of dynamically allocated chunk` |
|         - |  5133 | `		 * using [ph7_context_alloc_chunk()].` |
|         - |  5134 | `		 */` |
|      8987 |  5135 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     34354 |  5136 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     25372 |  5137 | `			pChunk = aAux[n].pAuxData;` |
|         - |  5138 | `			/* Release the chunk */` |
|     25372 |  5139 | `			if( pChunk ){` |
|     24668 |  5140 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|     12188 |  5141 | `			}` |
|     12545 |  5142 | `		}` |
|      8987 |  5143 | `		SySetRelease(&pCtx->sChunk);` |
|      4443 |  5144 | `	}` |
|  19258996 |  5145 | `}` |
|         - |  5146 | `/*` |
|         - |  5147 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - |  5148 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - |  5149 | ` */` |
|     59323 |  5150 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - |  5151 | `	ph7_context *pCtx, /* Call context */` |
|         - |  5152 | `	ph7_value *pValue  /* Release this value */` |
|         - |  5153 | `	)` |
|         5 |  5154 | `{` |
|     59328 |  5155 | `	if( pValue == 0 ){` |
|         - |  5156 | `		/* NULL value is a harmless operation */` |
|       ! 0 |  5157 | `		return;` |
|         - |  5158 | `	}` |
|     59328 |  5159 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     59328 |  5160 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - |  5161 | `		sxu32 n;` |
|    952760 |  5162 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    952760 |  5163 | `			if( apObj[n] == pValue ){` |
|     59328 |  5164 | `				PH7_MemObjRelease(pValue);` |
|     59328 |  5165 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - |  5166 | `				/* Mark as released */` |
|     59328 |  5167 | `				apObj[n] = 0;` |
|     59328 |  5168 | `				break;` |
|         - |  5169 | `			}` |
|    446045 |  5170 | `		}` |
|     29620 |  5171 | `	}` |
|     29625 |  5172 | `}` |
|         - |  5173 | `/*` |
|         - |  5174 | ` * Pop and release as many memory object from the operand stack.` |
|         - |  5175 | ` */` |
| 144862199 |  5176 | `PH7_PRIVATE void VmPopOperand(` |
|         - |  5177 | `	ph7_value **ppTos, /* Operand stack */` |
|         - |  5178 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - |  5179 | `	)` |
|         5 |  5180 | `{` |
| 144862204 |  5181 | `	ph7_value *pTos = *ppTos;` |
| 301663455 |  5182 | `	while( nPop > 0 ){` |
| 156801256 |  5183 | `		PH7_MemObjRelease(pTos);` |
| 156801256 |  5184 | `		pTos--;` |
| 156801256 |  5185 | `		nPop--;` |
|         5 |  5186 | `	}` |
|         - |  5187 | `	/* Top of the stack */` |
| 144862204 |  5188 | `	*ppTos = pTos;` |
| 144862204 |  5189 | `}` |
|         - |  5190 | `/*` |
|         - |  5191 | ` * Reserve a memory object.` |
|         - |  5192 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - |  5193 | ` */` |
|  29391413 |  5194 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 |  5195 | `{` |
|  29391418 |  5196 | `	ph7_value *pObj = 0;` |
|         - |  5197 | `	sxu32 nIdx;` |
|         - |  5198 | `	/* Check for a free slot. The head is a slot index, and the freed slot's own` |
|         - |  5199 | `	 * (dead) nIdx word holds the next one -- one load past the bounds test, the` |
|         - |  5200 | `	 * same shape as the SySetPop of the stack this replaced. The PH7_MemObjInit` |
|         - |  5201 | `	 * below is what takes MEMOBJ_POOLFREE back off: every acquire runs it, so the` |
|         - |  5202 | `	 * bit means "on the list" and nothing else. */` |
|  29391418 |  5203 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  29391418 |  5204 | `	if( pVm->aMemObj.nFreeHead != SXU32_HIGH ){` |
|  26235288 |  5205 | `		nIdx = pVm->aMemObj.nFreeHead;` |
|  26235288 |  5206 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  26235288 |  5207 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_POOLFREE) == 0 ){` |
|         - |  5208 | `			/* Stale or corrupted chain (defensive -- truncate clears the head and` |
|         - |  5209 | `			 * clears the bit): abandon it rather than hand out a LIVE slot. */` |
|       ! 0 |  5210 | `			pVm->aMemObj.nFreeHead = SXU32_HIGH;` |
|       ! 0 |  5211 | `			pObj = 0;` |
|       ! 0 |  5212 | `		}else{` |
|  26235288 |  5213 | `			pVm->aMemObj.nFreeHead = pObj->nIdx;` |
|         - |  5214 | `		}` |
|  13117059 |  5215 | `	}` |
|  29391418 |  5216 | `	if( pObj == 0 ){` |
|         - |  5217 | `		/* Reserve a new memory object */` |
|   3156135 |  5218 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   3156135 |  5219 | `		if( pObj == 0 ){` |
|       ! 0 |  5220 | `			return 0;` |
|         - |  5221 | `		}` |
|   1577042 |  5222 | `	}` |
|         - |  5223 | `	/* Set a null default value */` |
|  29391418 |  5224 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  29391418 |  5225 | `	pObj->nIdx = nIdx;` |
|  29391418 |  5226 | `	return pObj;` |
|  14694106 |  5227 | `}` |
|         - |  5228 | `/*` |
|         - |  5229 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - |  5230 | ` */` |
|     97721 |  5231 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - |  5232 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - |  5233 | `	const char *zKey,  /* Entry key */` |
|         - |  5234 | `	sxu32 nByte,       /* Key length */` |
|         - |  5235 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - |  5236 | `	)` |
|         5 |  5237 | `{` |
|         - |  5238 | `	ph7_value sKey;` |
|         - |  5239 | `	sxi32 rc;` |
|     97726 |  5240 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     97726 |  5241 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - |  5242 | `	/* Perform the insertion */` |
|     97726 |  5243 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     97726 |  5244 | `	PH7_MemObjRelease(&sKey);` |
|     97726 |  5245 | `	return rc;` |
|         5 |  5246 | `}` |
|         - |  5247 | `/*` |
|         - |  5248 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - |  5249 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - |  5250 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - |  5251 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - |  5252 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - |  5253 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - |  5254 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - |  5255 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - |  5256 | ` *     superglobal in place.` |
|         - |  5257 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - |  5258 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - |  5259 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - |  5260 | ` *     a plain variable.` |
|         - |  5261 | ` */` |
|       262 |  5262 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         4 |  5263 | `{` |
|       266 |  5264 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - |  5265 | `	SyHashEntry *pEntry;` |
|         - |  5266 | `	ph7_value *pObj;` |
|         - |  5267 | `	char *zDup;` |
|         - |  5268 | `	sxu32 nIdx;` |
|         - |  5269 | `	sxi32 rc;` |
|         - |  5270 | `	/* Walk down to the global frame */` |
|       334 |  5271 | `	while( pFrame->pParent ){` |
|        71 |  5272 | `		pFrame = pFrame->pParent;` |
|         3 |  5273 | `	}` |
|         - |  5274 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       266 |  5275 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|       266 |  5276 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - |  5277 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - |  5278 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - |  5279 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - |  5280 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - |  5281 | `		 * of $GLOBALS itself). */` |
|         5 |  5282 | `		pEntry = 0;` |
|         2 |  5283 | `	}` |
|       266 |  5284 | `	if( pEntry == 0 ){` |
|       266 |  5285 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       131 |  5286 | `	}` |
|       266 |  5287 | `	if( pEntry ){` |
|        42 |  5288 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - |  5289 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - |  5290 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|        40 |  5291 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|        40 |  5292 | `			return SXRET_OK;` |
|         - |  5293 | `		}` |
|         3 |  5294 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 |  5295 | `		if( pObj == 0 ){` |
|       ! 0 |  5296 | `			return SXERR_NOTFOUND;` |
|         - |  5297 | `		}` |
|         3 |  5298 | `		if( pValue ){` |
|         3 |  5299 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 |  5300 | `		}else{` |
|       ! 0 |  5301 | `			PH7_MemObjToNull(pObj);` |
|         - |  5302 | `		}` |
|         3 |  5303 | `		return SXRET_OK;` |
|         - |  5304 | `	}` |
|       226 |  5305 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - |  5306 | `		/* Reserve a fresh slot for the new global */` |
|       220 |  5307 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       220 |  5308 | `		if( pObj == 0 ){` |
|       ! 0 |  5309 | `			return SXERR_MEM;` |
|         - |  5310 | `		}` |
|       220 |  5311 | `		nIdx = pObj->nIdx;` |
|       112 |  5312 | `	}else{` |
|         - |  5313 | `		/* Reference assignment: bind the name to the existing slot */` |
|         7 |  5314 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nRefIdx);` |
|         7 |  5315 | `		if( pObj == 0 ){` |
|       ! 0 |  5316 | `			return SXERR_NOTFOUND;` |
|         - |  5317 | `		}` |
|         7 |  5318 | `		nIdx = nRefIdx;` |
|         - |  5319 | `	}` |
|       226 |  5320 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       226 |  5321 | `	if( zDup == 0 ){` |
|       ! 0 |  5322 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - |  5323 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - |  5324 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|       ! 0 |  5325 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 |  5326 | `		}` |
|       ! 0 |  5327 | `		return SXERR_MEM;` |
|         - |  5328 | `	}` |
|       226 |  5329 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       226 |  5330 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  5331 | `		if( nRefIdx == SXU32_HIGH ){` |
|       ! 0 |  5332 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 |  5333 | `		}` |
|       ! 0 |  5334 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 |  5335 | `		return rc;` |
|         - |  5336 | `	}` |
|         - |  5337 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       226 |  5338 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       226 |  5339 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       226 |  5340 | `	if( nRefIdx == SXU32_HIGH ){` |
|       220 |  5341 | `		pObj->nIdx = nIdx;` |
|       220 |  5342 | `		if( pValue ){` |
|       203 |  5343 | `			PH7_MemObjStore(pValue,pObj);` |
|       100 |  5344 | `		}` |
|       108 |  5345 | `	}` |
|       226 |  5346 | `	return SXRET_OK;` |
|       135 |  5347 | `}` |
|         - |  5348 | `/*` |
|         - |  5349 | ` * Extract a variable value from the top active VM frame.` |
|         - |  5350 | ` * Return a pointer to the variable value on success.` |
|         - |  5351 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - |  5352 | ` *` |
|         - |  5353 | ` * pnIdx, when given, receives the SLOT the name resolved to -- which the value's own` |
|         - |  5354 | ` * nIdx does not always carry (a superglobal's does not), and which the caller cannot` |
|         - |  5355 | ` * ask for afterwards without repeating the lookup this function just did.` |
|         - |  5356 | ` */` |
|  97884028 |  5357 | `static ph7_value * VmExtractMemObjEx(` |
|         - |  5358 | `	ph7_vm *pVm,           /* Target VM */` |
|         - |  5359 | `	const SyString *pName, /* Variable name */` |
|         - |  5360 | `	int bDup,              /* True to duplicate variable name */` |
|         - |  5361 | `	int bCreate,           /* True to create the variable if non-existent */` |
|         - |  5362 | `	sxu32 *pnIdx           /* OUT: the slot the name is bound to (may be NULL) */` |
|         - |  5363 | `	)` |
|         5 |  5364 | `{` |
|  97884033 |  5365 | `	int bNullify = FALSE;` |
|         - |  5366 | `	SyHashEntry *pEntry;` |
|         - |  5367 | `	VmFrame *pFrame;` |
|         - |  5368 | `	ph7_value *pObj;` |
|         - |  5369 | `	sxu32 nIdx;` |
|         - |  5370 | `	sxi32 rc;` |
|         - |  5371 | `	/* Point to the top active frame */` |
|  97884033 |  5372 | `	pFrame = pVm->pFrame;` |
|  97884033 |  5373 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - |  5374 | `	/* Perform the lookup */` |
|  97884033 |  5375 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - |  5376 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 |  5377 | `		pName = &sAnnon;` |
|         - |  5378 | `		/* Always nullify the object */` |
|        18 |  5379 | `		bNullify = TRUE;` |
|        18 |  5380 | `		bDup = FALSE;` |
|         8 |  5381 | `	}` |
|         - |  5382 | `	/* Check the superglobals table first */` |
|  97884033 |  5383 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|  97884033 |  5384 | `	if( pEntry == 0 ){` |
|         - |  5385 | `		/* Query the top active frame */` |
|  97882381 |  5386 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  97882381 |  5387 | `		if( pEntry == 0 ){` |
|   1616731 |  5388 | `			char *zName = (char *)pName->zString;` |
|         - |  5389 | `			VmSlot sLocal;` |
|   1616731 |  5390 | `			if( !bCreate ){` |
|         - |  5391 | `				/* Do not create the variable,return NULL instead */` |
|     23058 |  5392 | `				return 0;` |
|         - |  5393 | `			}` |
|         - |  5394 | `			/* No such variable,automatically create a new one and install` |
|         - |  5395 | `			 * it in the current frame.` |
|         - |  5396 | `			 */` |
|   1593678 |  5397 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1593678 |  5398 | `			if( pObj == 0 ){` |
|       ! 0 |  5399 | `				return 0;` |
|         - |  5400 | `			}` |
|   1593678 |  5401 | `			nIdx = pObj->nIdx;` |
|   1593678 |  5402 | `			if( bDup ){` |
|         - |  5403 | `				/* Duplicate name */` |
|     16859 |  5404 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|     16859 |  5405 | `				if( zName == 0 ){` |
|       ! 0 |  5406 | `					return 0;` |
|         - |  5407 | `				}` |
|      8384 |  5408 | `			}` |
|         - |  5409 | `			/* Link to the top active VM frame */` |
|   1593678 |  5410 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1593678 |  5411 | `			if( rc != SXRET_OK ){` |
|         - |  5412 | `				/* Return the slot to the free pool */` |
|       ! 0 |  5413 | `				VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 |  5414 | `				return 0;` |
|         - |  5415 | `			}` |
|   1593678 |  5416 | `			if( pFrame->pParent != 0 ){` |
|         - |  5417 | `				/* Local variable */` |
|   1572907 |  5418 | `				sLocal.nIdx = nIdx;` |
|   1572907 |  5419 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    810319 |  5420 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - |  5421 | `				/* Register in the $GLOBALS array */` |
|     20461 |  5422 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|     10192 |  5423 | `			}` |
|         - |  5424 | `			/* Install in the reference table */` |
|   1593678 |  5425 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - |  5426 | `			/* Save object index */` |
|   1593678 |  5427 | `			pObj->nIdx = nIdx;` |
|    799897 |  5428 | `		}else{` |
|         - |  5429 | `			/* Extract variable contents */` |
|  96265655 |  5430 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  96265655 |  5431 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  96265655 |  5432 | `			if( bNullify && pObj ){` |
|         3 |  5433 | `				PH7_MemObjRelease(pObj);` |
|         1 |  5434 | `			}` |
|         - |  5435 | `		}` |
|  48932419 |  5436 | `	}else{` |
|         - |  5437 | `		/* Superglobal */` |
|      1657 |  5438 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      1657 |  5439 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|         - |  5440 | `	}` |
|  97860980 |  5441 | `	if( pnIdx ){` |
|  90231617 |  5442 | `		*pnIdx = nIdx;` |
|  45118712 |  5443 | `	}` |
|  97860980 |  5444 | `	return pObj;` |
|  48944647 |  5445 | `}` |
|   7651714 |  5446 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - |  5447 | `	ph7_vm *pVm,           /* Target VM */` |
|         - |  5448 | `	const SyString *pName, /* Variable name */` |
|         - |  5449 | `	int bDup,              /* True to duplicate variable name */` |
|         - |  5450 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - |  5451 | `	)` |
|         5 |  5452 | `{` |
|   7651719 |  5453 | `	return VmExtractMemObjEx(&(*pVm),pName,bDup,bCreate,0);` |
|         5 |  5454 | `}` |
|         - |  5455 | `/*` |
|         - |  5456 | ` * Number a body's variables, once, from the body itself.` |
|         - |  5457 | ` *` |
|         - |  5458 | ` * Every instruction that names a variable the compiler wrote down -- OP_LOAD and` |
|         - |  5459 | ` * OP_STORE with a p3 -- gets a small NUMBER in its nSite, and the running frame then` |
|         - |  5460 | ` * answers that number out of an array instead of hashing the name (see VmFrame's` |
|         - |  5461 | ` * aLocalSlot). Two instructions naming the same variable get the same number, so the` |
|         - |  5462 | ` * frame holds one entry per NAME and not one per site.` |
|         - |  5463 | ` *` |
|         - |  5464 | ` * Names are compared by ADDRESS, which is exact and not an approximation: the compiler` |
|         - |  5465 | ` * interns every variable name it emits into one VM-lifetime buffer (pGen->hVar), so` |
|         - |  5466 | ` * within a body the same spelling is the same pointer. Two pointers for one spelling` |
|         - |  5467 | ` * would only cost a body two numbers for one name, which stays correct -- both entries` |
|         - |  5468 | ` * hold the same slot and both are emptied together.` |
|         - |  5469 | ` *` |
|         - |  5470 | ` * A body with more distinct names than PH7_VAR_SLOT_MAX numbers its most REFERENCED` |
|         - |  5471 | ` * ones: the pass counts static references first and hands the numbers out in that` |
|         - |  5472 | ` * order, so what a hot loop reads is what fits. The rest keep nSite = 0 and take the` |
|         - |  5473 | ` * hash path, exactly as every site did before this existed.` |
|         - |  5474 | ` *` |
|         - |  5475 | ` * Lazy and self-computing, like nMaxStack: a body that has not been walked yet just` |
|         - |  5476 | ` * walks. There is no path that can produce a WRONG number -- an unwalked body has 0` |
|         - |  5477 | ` * everywhere, which means "ask the table".` |
|         - |  5478 | ` */` |
|         - |  5479 | `#define VM_LOCAL_SCAN_MAX 32   /* distinct names the pass will rank; past this it stops` |
|         - |  5480 | `                                * counting and numbers what it has. A body naming more` |
|         - |  5481 | `                                * than this has long since stopped fitting the frame, and` |
|         - |  5482 | `                                * the three arrays below are C STACK -- 352 bytes at this` |
|         - |  5483 | `                                * width, which matters on a 16-frame embedded target. */` |
|   1607455 |  5484 | `static int VmInstrNamesVar(const VmInstr *pInstr)` |
|         5 |  5485 | `{` |
|   1607460 |  5486 | `	return ( pInstr->iOp == PH7_OP_LOAD \|\| pInstr->iOp == PH7_OP_STORE ) && pInstr->p3 != 0;` |
|         5 |  5487 | `}` |
|     36484 |  5488 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName)` |
|         5 |  5489 | `{` |
|         - |  5490 | `	const char *azName[VM_LOCAL_SCAN_MAX];` |
|         - |  5491 | `	sxu16 aRef[VM_LOCAL_SCAN_MAX];   /* references, then re-used as name -> number+1 */` |
|         - |  5492 | `	sxu8 aRank[VM_LOCAL_SCAN_MAX];` |
|     36489 |  5493 | `	sxu32 nName = 0;` |
|         - |  5494 | `	sxu32 i,j,n;` |
|     36489 |  5495 | `	*pnName = 0;` |
|     36489 |  5496 | `	if( aInstr == 0 ){` |
|       ! 0 |  5497 | `		return;` |
|         - |  5498 | `	}` |
|         - |  5499 | `	/* Pass one: the distinct names, and how many instructions reach for each. */` |
|    907647 |  5500 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|         - |  5501 | `		const char *zName;` |
|    871163 |  5502 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|    764133 |  5503 | `			continue;` |
|         - |  5504 | `		}` |
|    107035 |  5505 | `		zName = (const char *)aInstr[i].p3;` |
|         - |  5506 | `		/* The length belongs to the name and not to the execution: measure it here,` |
|         - |  5507 | `		 * once, for the handlers that used to call SyStrlen on every pass. */` |
|    107035 |  5508 | `		if( aInstr[i].nAux == 0 ){` |
|     99531 |  5509 | `			aInstr[i].nAux = (sxu32)SyStrlen(zName);` |
|     49423 |  5510 | `		}` |
|    474946 |  5511 | `		for( j = 0 ; j < nName ; ++j ){` |
|    428320 |  5512 | `			if( azName[j] == zName ){` |
|     60409 |  5513 | `				if( aRef[j] < SXU16_HIGH ){` |
|     60409 |  5514 | `					aRef[j]++;   /* a count that saturates still ranks first */` |
|     29975 |  5515 | `				}` |
|     60409 |  5516 | `				break;` |
|         - |  5517 | `			}` |
|    182226 |  5518 | `		}` |
|    107035 |  5519 | `		if( j == nName ){` |
|     46631 |  5520 | `			if( nName >= VM_LOCAL_SCAN_MAX ){` |
|      1203 |  5521 | `				continue;` |
|         - |  5522 | `			}` |
|     45433 |  5523 | `			azName[nName] = zName;` |
|     45433 |  5524 | `			aRef[nName] = 1;` |
|     45433 |  5525 | `			nName++;` |
|     22589 |  5526 | `		}` |
|     52569 |  5527 | `	}` |
|     36489 |  5528 | `	if( nName < 1 ){` |
|     13811 |  5529 | `		return;` |
|         - |  5530 | `	}` |
|         - |  5531 | `	/* Rank by static reference count, first appearance breaking ties -- an insertion` |
|         - |  5532 | `	 * sort over at most VM_LOCAL_SCAN_MAX entries, run once per body. aRank[k] is the` |
|         - |  5533 | `	 * name that gets number k. */` |
|     68111 |  5534 | `	for( i = 0 ; i < nName ; ++i ){` |
|     74180 |  5535 | `		for( j = i ; j > 0 && aRef[aRank[j-1]] < aRef[i] ; --j ){` |
|     28752 |  5536 | `			aRank[j] = aRank[j-1];` |
|     14239 |  5537 | `		}` |
|         - |  5538 | `		/* aRank holds name INDICES, so VM_LOCAL_SCAN_MAX must fit an sxu8. */` |
|     45433 |  5539 | `		aRank[j] = (sxu8)i;` |
|     22594 |  5540 | `	}` |
|     22683 |  5541 | `	n = nName > PH7_VAR_SLOT_MAX ? PH7_VAR_SLOT_MAX : nName;` |
|         - |  5542 | `	/* aRef is re-used as name -> number+1, so pass two is a single lookup. */` |
|     68111 |  5543 | `	for( i = 0 ; i < nName ; ++i ){` |
|     45433 |  5544 | `		aRef[i] = 0;` |
|     22594 |  5545 | `	}` |
|     68053 |  5546 | `	for( i = 0 ; i < n ; ++i ){` |
|     45375 |  5547 | `		aRef[aRank[i]] = (sxu16)(i + 1);` |
|     22565 |  5548 | `	}` |
|         - |  5549 | `	/* Pass two: stamp the number on every instruction that names one. */` |
|    758980 |  5550 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|         - |  5551 | `		const char *zName;` |
|    736302 |  5552 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|    629272 |  5553 | `			continue;` |
|         - |  5554 | `		}` |
|    107035 |  5555 | `		zName = (const char *)aInstr[i].p3;` |
|    474946 |  5556 | `		for( j = 0 ; j < nName ; ++j ){` |
|    473748 |  5557 | `			if( azName[j] == zName ){` |
|    105837 |  5558 | `				aInstr[i].nSite = aRef[j];` |
|    105837 |  5559 | `				break;` |
|         - |  5560 | `			}` |
|    182226 |  5561 | `		}` |
|     53168 |  5562 | `	}` |
|     22683 |  5563 | `	*pnName = (sxu16)n;` |
|     18113 |  5564 | `}` |
|         - |  5565 | `/*` |
|         - |  5566 | ` * Number a function body if it has not been numbered, and tell the frame about to run` |
|         - |  5567 | ` * it which body its numbers belong to. One branch per activation; the walk itself` |
|         - |  5568 | ` * happens once per function for the life of the VM.` |
|         - |  5569 | ` */` |
|    890194 |  5570 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc)` |
|         5 |  5571 | `{` |
|    890199 |  5572 | `	if( !pFunc->bNumbered ){` |
|     44299 |  5573 | `		VmNumberLocals((VmInstr *)SySetBasePtr(&pFunc->aByteCode),` |
|     14679 |  5574 | `			SySetUsed(&pFunc->aByteCode),&pFunc->nLocalName);` |
|     29620 |  5575 | `		pFunc->bNumbered = 1;` |
|     14679 |  5576 | `	}` |
|    890199 |  5577 | `	if( pFunc->nLocalName > 0 ){` |
|    256345 |  5578 | `		pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|    128458 |  5579 | `	}` |
|    890199 |  5580 | `}` |
|         - |  5581 | `/*` |
|         - |  5582 | ` * Forget where this frame's variables live. Called from the doors that can move a NAME` |
|         - |  5583 | ` * to a different slot; a door that only ever INSTALLS a name the frame did not have` |
|         - |  5584 | ` * does not need it, because a lookup that found nothing is never filed.` |
|         - |  5585 | ` */` |
|     96499 |  5586 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame)` |
|         5 |  5587 | `{` |
|         - |  5588 | `	sxu32 i;` |
|   2798476 |  5589 | `	for( i = 0 ; i < PH7_VAR_SLOT_MAX ; ++i ){` |
|   2701977 |  5590 | `		pFrame->aLocalSlot[i] = 0;` |
|   1349101 |  5591 | `	}` |
|     96504 |  5592 | `}` |
|         - |  5593 | `/*` |
|         - |  5594 | ` * VmExtractMemObj for a name the CALLER guarantees outlives the lookup -- a variable` |
|         - |  5595 | ` * name the compiler interned into the bytecode, and nothing else.` |
|         - |  5596 | ` *` |
|         - |  5597 | ` * Every variable access consults the superglobal table and then hashes the name into` |
|         - |  5598 | ` * the frame's symbol table; measured on the ecosystem gate's phpcs step that was the` |
|         - |  5599 | ` * largest single row in the engine's whole name-lookup census, and the hash is over a` |
|         - |  5600 | ` * name whose answer cannot change between two accesses in the same frame unless` |
|         - |  5601 | ` * something re-binds it. So the answer is remembered on the frame, BY NUMBER (see` |
|         - |  5602 | ` * VmFrame's aLocalSlot), and the second and later reads of a variable inside one` |
|         - |  5603 | ` * activation cost an array index.` |
|         - |  5604 | ` *` |
|         - |  5605 | ` * nSlot is the number the body gave this name plus one, and aCode the instruction` |
|         - |  5606 | ` * array it was numbered in -- 0 for a caller that has neither, which then pays the` |
|         - |  5607 | ` * lookup it always did. The aCode compare is what keeps an included unit, an eval and` |
|         - |  5608 | ` * a default-argument mini-program from reading numbers that are not theirs: they share` |
|         - |  5609 | ` * the frame, so their instructions must not index its array.` |
|         - |  5610 | ` *` |
|         - |  5611 | ` * bDup is deliberately absent: a name that has to be COPIED to become a symbol-table` |
|         - |  5612 | ` * key is by definition not one that outlives the lookup.` |
|         - |  5613 | ` */` |
| 106558742 |  5614 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(` |
|         - |  5615 | `	ph7_vm *pVm,           /* Target VM */` |
|         - |  5616 | `	const SyString *pName, /* Variable name -- interned, NUL-terminated, VM-lifetime */` |
|         - |  5617 | `	int bCreate,           /* True to create the variable if non-existent */` |
|         - |  5618 | `	sxu32 nSlot,           /* The body's number for this name, plus one (0 = none) */` |
|         - |  5619 | `	const VmInstr *aCode   /* The instruction array nSlot was numbered in */` |
|         - |  5620 | `	)` |
|         5 |  5621 | `{` |
|         - |  5622 | `	VmFrame *pFrame;` |
|         - |  5623 | `	ph7_value *pObj;` |
|         - |  5624 | `	sxu32 nIdx;` |
| 106558747 |  5625 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|       ! 0 |  5626 | `		return VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,0);` |
|         - |  5627 | `	}` |
| 106558747 |  5628 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
| 107141118 |  5629 | `	if( nSlot > 0 && pFrame->pCodeBase == aCode ){` |
|  17497407 |  5630 | `		sxu32 nCached = pFrame->aLocalSlot[nSlot - 1];` |
|  17497407 |  5631 | `		if( nCached > 0 ){` |
|  16326433 |  5632 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCached - 1);` |
|  16326433 |  5633 | `			if( pObj ){` |
|  16326433 |  5634 | `				return pObj;` |
|         - |  5635 | `			}` |
|       ! 0 |  5636 | `		}` |
|    588608 |  5637 | `	}else{` |
|  89061345 |  5638 | `		nSlot = 0;` |
|         - |  5639 | `	}` |
|  90232319 |  5640 | `	nIdx = SXU32_HIGH;` |
|  90232319 |  5641 | `	pObj = VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,&nIdx);` |
|  90232319 |  5642 | `	if( pObj && nSlot > 0 && nIdx != SXU32_HIGH ){` |
|         - |  5643 | `		/* VmExtractMemObjEx may have grown the frame chain's tables, but never the` |
|         - |  5644 | `		 * chain itself, so the frame the answer belongs to is still this one. */` |
|   1170409 |  5645 | `		pFrame->aLocalSlot[nSlot - 1] = nIdx + 1;` |
|    588318 |  5646 | `	}` |
|  90232319 |  5647 | `	return pObj;` |
|  53296937 |  5648 | `}` |
|         - |  5649 | `/*` |
|         - |  5650 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - |  5651 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - |  5652 | ` */` |
|     62743 |  5653 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - |  5654 | `	ph7_vm *pVm,       /* Target VM */` |
|         - |  5655 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - |  5656 | `	sxu32 nByte        /* zName length */` |
|         - |  5657 | `	)` |
|         5 |  5658 | `{` |
|         - |  5659 | `	SyHashEntry *pEntry;` |
|         - |  5660 | `	ph7_value *pValue;` |
|         - |  5661 | `	sxu32 nIdx;` |
|         - |  5662 | `	/* Query the superglobal table */` |
|     62748 |  5663 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|     62748 |  5664 | `	if( pEntry == 0 ){` |
|         - |  5665 | `		/* No such entry */` |
|       ! 0 |  5666 | `		return 0;` |
|         - |  5667 | `	}` |
|         - |  5668 | `	/* Extract the superglobal index in the global object pool */` |
|     62748 |  5669 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - |  5670 | `	/* Extract the variable value  */` |
|     62748 |  5671 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     62748 |  5672 | `	return pValue;` |
|     31327 |  5673 | `}` |
|         - |  5674 | `/*` |
|         - |  5675 | ` * Perform a raw hashmap insertion.` |
|         - |  5676 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - |  5677 | ` */` |
|     48925 |  5678 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - |  5679 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - |  5680 | `	const char *zKey,   /* Entry key */` |
|         - |  5681 | `	int nKeylen,        /* zKey length*/` |
|         - |  5682 | `	const char *zData,  /* Entry data */` |
|         - |  5683 | `	int nLen            /* zData length */` |
|         - |  5684 | `	)` |
|         5 |  5685 | `{` |
|         - |  5686 | `	ph7_value sKey,sValue;` |
|         - |  5687 | `	sxi32 rc;` |
|     48930 |  5688 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     48930 |  5689 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     48930 |  5690 | `	if( zKey ){` |
|     42031 |  5691 | `		if( nKeylen < 0 ){` |
|     41857 |  5692 | `			nKeylen = (int)SyStrlen(zKey);` |
|     20893 |  5693 | `		}` |
|     42031 |  5694 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     20980 |  5695 | `	}` |
|     48930 |  5696 | `	if( zData ){` |
|     48930 |  5697 | `		if( nLen < 0 ){` |
|         - |  5698 | `			/* Compute length automatically */` |
|     27709 |  5699 | `			nLen = (int)SyStrlen(zData);` |
|     13830 |  5700 | `		}` |
|     48930 |  5701 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     24424 |  5702 | `	}` |
|         - |  5703 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - |  5704 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - |  5705 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - |  5706 | `	 * every argument under "". */` |
|     48930 |  5707 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     48930 |  5708 | `	PH7_MemObjRelease(&sKey);` |
|     48930 |  5709 | `	PH7_MemObjRelease(&sValue);` |
|     48930 |  5710 | `	return rc;` |
|         5 |  5711 | `}` |
|         - |  5712 | `/*` |
|         - |  5713 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - |  5714 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - |  5715 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - |  5716 | ` */` |
|       230 |  5717 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 |  5718 | `{` |
|       234 |  5719 | `	sxi64 iVal = 0;` |
|       234 |  5720 | `	if( nValue == 0 ){` |
|       ! 0 |  5721 | `		return 0;` |
|         - |  5722 | `	}` |
|       230 |  5723 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|       230 |  5724 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|       234 |  5725 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 |  5726 | `		return 1;` |
|         - |  5727 | `	}` |
|       234 |  5728 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|       234 |  5729 | `	return iVal != 0;` |
|       119 |  5730 | `}` |
|         - |  5731 | `/*` |
|         - |  5732 | ` * php's php.ini VALUE grammar, over one directive's value.` |
|         - |  5733 | ` *` |
|         - |  5734 | ` * An ini value is not a literal. zend_ini_parser reads it as an expression over` |
|         - |  5735 | `` * `\|`, `&`, `^`, unary `~` and `!` and parentheses, in which a bare identifier`` |
|         - |  5736 | ` * stands for the constant of that name -- which is what makes` |
|         - |  5737 | `` * `error_reporting = E_ALL & ~E_DEPRECATED` mean 22527 rather than 0. Three`` |
|         - |  5738 | ` * facts about it are not the C ones and were all read off php 8.5:` |
|         - |  5739 | ` *` |
|         - |  5740 | ` *   . The three binary operators share ONE precedence and associate left, so` |
|         - |  5741 | `` *     `1 \| 2 & 4` is `(1\|2) & 4` = 0 where C reads it as 1. The unary pair binds`` |
|         - |  5742 | `` *     tighter and nests (`~~2` is 2).`` |
|         - |  5743 | ` *   . Operands are STRINGS. zend_ini_do_op() runs each side through atoi(),` |
|         - |  5744 | ` *     computes in a 32-bit int, and writes the decimal text of the result back` |
|         - |  5745 | ` *     as the value -- so an undefined constant is its own name (and therefore` |
|         - |  5746 | ` *     0), a quoted "E_ALL" is four letters and not 30719, and adjacent pieces` |
|         - |  5747 | `` *     concatenate: `E_NOTICE E_WARNING` is the string "8 2", which is 8.`` |
|         - |  5748 | ` *   . A value carrying no operator at all keeps its substituted TEXT. That text` |
|         - |  5749 | ` *     is what ini_get() shows and what ini_restore() re-applies, so it is what` |
|         - |  5750 | ` *     gets stored, not the number it happens to read as.` |
|         - |  5751 | ` *` |
|         - |  5752 | `` * The boolean words are a whole-value shape rather than an operand: `On` is 1`` |
|         - |  5753 | `` * even in `On\|E_NOTICE`, where the word itself still commits and the `\|` is`` |
|         - |  5754 | ` * a separate syntax error over the leftover text (see VmIniEvalValue).` |
|         - |  5755 | ` */` |
|         - |  5756 | `#define VM_INI_EXPR_MAX_DEPTH 32` |
|         - |  5757 | `/*` |
|         - |  5758 | ` * What php's ini parser would report over one value, in the pieces its` |
|         - |  5759 | ` * ini_error() prints. php names the token it could not take, and it has three` |
|         - |  5760 | ` * ways of naming one: zTok is the SYMBOL its grammar declares for a token made` |
|         - |  5761 | `` * of more than one shape (`TC_CONSTANT`, `TC_NUMBER`, `TC_STRING`, `TC_RAW`,`` |
|         - |  5762 | `` * `TC_DOLLAR_CURLY`), cChar is the byte itself for the tokens bison prints as`` |
|         - |  5763 | ` * a quoted character, and neither set means the value ran out -- which php` |
|         - |  5764 | ` * names END_OF_LINE and dates to the line after the directive. bExpect adds` |
|         - |  5765 | ` * the token list it appends when the stop happened inside an unclosed '('.` |
|         - |  5766 | ` * bSet is the whole question "was this value a syntax error", which is what` |
|         - |  5767 | ` * decides both the warning and php's abort of the rest of the source.` |
|         - |  5768 | ` */` |
|         - |  5769 | `typedef struct VmIniBad VmIniBad;` |
|         - |  5770 | `struct VmIniBad {` |
|         - |  5771 | `	int bSet;` |
|         - |  5772 | `	const char *zTok;` |
|         - |  5773 | `	int cChar;` |
|         - |  5774 | `	int bExpect;` |
|         - |  5775 | ``	/* An expect-list that is not the value grammar's own: `${` has two of its`` |
|         - |  5776 | `	 * own (TC_VARNAME, and "TC_FALLBACK or '}'"), so the list is text here` |
|         - |  5777 | `	 * rather than the single flag bExpect still carries. */` |
|         - |  5778 | `	const char *zExpect;` |
|         - |  5779 | `	/* How many lines below the directive's own the stop happened. A value is` |
|         - |  5780 | `	 * one scanner input and a double-quoted run inside it crosses newlines,` |
|         - |  5781 | ``	 * so `x = "a\nb")` is dated to the line the `)` is written on and not to`` |
|         - |  5782 | ``	 * the line `x` is. Only the double-quoted run moves it: php's raw-string`` |
|         - |  5783 | `	 * rule is a single match that never touches the line counter. */` |
|         - |  5784 | `	sxu32 nLine;` |
|         - |  5785 | `};` |
|         - |  5786 | `typedef struct VmIniExpr VmIniExpr;` |
|         - |  5787 | `struct VmIniExpr {` |
|         - |  5788 | `	ph7_vm *pVm;` |
|         - |  5789 | `	const char *zCur;` |
|         - |  5790 | `	const char *zEnd;` |
|         - |  5791 | `	/* Newlines a double-quoted run carried the scanner over before the stop --` |
|         - |  5792 | `	 * php's ST_DOUBLE_QUOTES counts every one of them, and dates whatever it` |
|         - |  5793 | `	 * reports next that many lines below the directive. */` |
|         - |  5794 | `	sxu32 nLine;` |
|         - |  5795 | ``	/* A raw string with no closing quote: php's `['][^']*[']` is ONE match, so`` |
|         - |  5796 | `	 * it does not match at all and the scanner runs off the end of the source.` |
|         - |  5797 | `	 * Whatever the value already holds still commits; only a value that had` |
|         - |  5798 | `	 * nothing yet becomes an error, and php names that one end of file. */` |
|         - |  5799 | `	int bRawEof;` |
|         - |  5800 | `	/* Where a failed parse stopped, in the two pieces php's ini_error() prints.` |
|         - |  5801 | `	 * cStop is the byte the grammar could not take, or 0 when the value simply` |
|         - |  5802 | `	 * ran out -- php calls that one END_OF_LINE and dates it to the line AFTER` |
|         - |  5803 | `	 * the directive, because its scanner has already eaten the newline. bExpect` |
|         - |  5804 | `	 * is set at the one place php's parser has a complete expression in hand and` |
|         - |  5805 | `	 * an unclosed '(' behind it, which is the whole of when it appends its` |
|         - |  5806 | ``	 * "expecting '^' or '\|' or '&' or ')'" list -- `(E_ALL` and `(1~2)` both get`` |
|         - |  5807 | ``	 * it, `~(` and `1 ^` do not. Only the FIRST stop is kept: the recursive`` |
|         - |  5808 | `	 * descent unwinds through every caller and php reports one error per parse. */` |
|         - |  5809 | `	int bStop;` |
|         - |  5810 | `	int cStop;` |
|         - |  5811 | `	int bExpect;` |
|         - |  5812 | ``	/* The `${` substitution names its own token and its own expect-list; every`` |
|         - |  5813 | `	 * other stop leaves both 0 and is described by cStop/bExpect alone. */` |
|         - |  5814 | `	const char *zTok;` |
|         - |  5815 | `	const char *zExpect;` |
|         - |  5816 | `};` |
|       108 |  5817 | `static void VmIniExprStop(VmIniExpr *p,int cStop,int bExpect)` |
|       ! 0 |  5818 | `{` |
|       108 |  5819 | `	if( p->bStop ){` |
|        28 |  5820 | `		return;` |
|         - |  5821 | `	}` |
|        80 |  5822 | `	p->bStop = 1;` |
|         - |  5823 | ``	/* `;` opens a comment rather than a token, so a value that stops there ran`` |
|         - |  5824 | `	 * out as far as the grammar is concerned. */` |
|        80 |  5825 | `	p->cStop = cStop == ';' ? 0 : cStop;` |
|        80 |  5826 | `	p->bExpect = bExpect;` |
|        54 |  5827 | `}` |
|         - |  5828 | `/*` |
|         - |  5829 | ` * A stop php names itself rather than by the byte it choked on. zTok is the` |
|         - |  5830 | ` * symbol printed with no quotes ("end of file", "TC_FALLBACK"), cStop the byte` |
|         - |  5831 | ` * when there is one, and zExpect the list appended behind either.` |
|         - |  5832 | ` */` |
|        66 |  5833 | `static void VmIniExprStopAt(VmIniExpr *p,const char *zTok,int cStop,const char *zExpect)` |
|       ! 0 |  5834 | `{` |
|        66 |  5835 | `	if( p->bStop ){` |
|       ! 0 |  5836 | `		return;` |
|         - |  5837 | `	}` |
|        66 |  5838 | `	p->bStop = 1;` |
|        66 |  5839 | `	p->cStop = cStop;` |
|        66 |  5840 | `	p->bExpect = 0;` |
|        66 |  5841 | `	p->zTok = zTok;` |
|        66 |  5842 | `	p->zExpect = zExpect;` |
|        33 |  5843 | `}` |
|      4253 |  5844 | `static void VmIniExprSpace(VmIniExpr *p)` |
|         4 |  5845 | `{` |
|      5849 |  5846 | `	while( p->zCur < p->zEnd && (p->zCur[0] == ' ' \|\| p->zCur[0] == '\t') ){` |
|        48 |  5847 | `		p->zCur++;` |
|         4 |  5848 | `	}` |
|      4257 |  5849 | `}` |
|      2033 |  5850 | `static int VmIniExprIsOp(int c)` |
|         4 |  5851 | `{` |
|      2037 |  5852 | `	return c == '\|' \|\| c == '&' \|\| c == '^';` |
|         4 |  5853 | `}` |
|         - |  5854 | `/*` |
|         - |  5855 | ` * One VALUE_CHARS unit: php's value scanner takes any byte that is not one of` |
|         - |  5856 | `` * its own delimiters, plus a `$` that does NOT open `${` -- which carries the`` |
|         - |  5857 | ` * byte behind it, and one more when that byte is a backslash. Answers how many` |
|         - |  5858 | `` * bytes the unit spends, or 0 when the run stops here. A `$` with NOTHING`` |
|         - |  5859 | ` * behind it matches nothing at all, so it is not a unit and the run stops in` |
|         - |  5860 | `` * front of it -- a value that ends on `$` keeps the newline that followed it,`` |
|         - |  5861 | ` * because the reader hands the run-on over whole rather than trimming it.` |
|         - |  5862 | ` */` |
|      1049 |  5863 | `static int VmIniValueCharLen(const char *zCur,const char *zEnd)` |
|         4 |  5864 | `{` |
|      1053 |  5865 | `	int c = (unsigned char)zCur[0];` |
|      1053 |  5866 | `	if( c == '$' ){` |
|        30 |  5867 | `		if( &zCur[1] >= zEnd \|\| zCur[1] == '{' ){` |
|       ! 0 |  5868 | `			return 0;` |
|         - |  5869 | `		}` |
|        30 |  5870 | `		return zCur[1] == '\\' && &zCur[2] < zEnd ? 3 : 2;` |
|         - |  5871 | `	}` |
|      1019 |  5872 | `	if( c == '=' \|\| c == ' ' \|\| c == '\t' \|\| c == '\n' \|\| c == '\r' \|\| c == ';'` |
|      1009 |  5873 | `	 \|\| c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '~' \|\| c == '(' \|\| c == ')'` |
|      1013 |  5874 | `	 \|\| c == '!' \|\| c == '"' \|\| c == '\'' \|\| c == 0 ){` |
|        10 |  5875 | `		return 0;` |
|         - |  5876 | `	}` |
|      1013 |  5877 | `	return 1;` |
|       528 |  5878 | `}` |
|         - |  5879 | `/*` |
|         - |  5880 | ` * Name the token php's value scanner would make of the text left standing` |
|         - |  5881 | ` * where the grammar cannot take it -- the two shapes VmIniEvalValue hands back` |
|         - |  5882 | ` * as "committed, but here is what php would separately warn about": a leftover` |
|         - |  5883 | `` * right after a boolean word short-circuits (`On X`, `On\|E_NOTICE`) and one`` |
|         - |  5884 | `` * right after a parenthesised expression closes (`(1)x`, `(1)=`). Blanks are`` |
|         - |  5885 | `` * already behind zCur, and `;` opens a comment rather than a token.`` |
|         - |  5886 | ` *` |
|         - |  5887 | ` * php's ST_VALUE rules compete by longest match and, on a tie, by the order` |
|         - |  5888 | ` * they are written in: TC_CONSTANT, then TC_NUMBER, then the catch-all` |
|         - |  5889 | ` * TC_STRING. Every byte those first two can take is also a VALUE_CHARS byte,` |
|         - |  5890 | ` * so the longest run is always the catch-all's and the tie IS the test --` |
|         - |  5891 | `` * `x1` is a constant, `1x` is a string, `-1` is a number and `-1x` is not.`` |
|         - |  5892 | ` */` |
|      1227 |  5893 | `static void VmIniBadToken(const char *zCur,const char *zEnd,VmIniBad *pBad)` |
|         4 |  5894 | `{` |
|         - |  5895 | `	const char *zWalk;` |
|         - |  5896 | `	sxu32 nVal,nCons,nNum;` |
|         - |  5897 | `	int c;` |
|      1231 |  5898 | `	if( zCur >= zEnd ){` |
|      1137 |  5899 | `		return;` |
|         - |  5900 | `	}` |
|        97 |  5901 | `	c = (unsigned char)zCur[0];` |
|        97 |  5902 | `	if( c == ';' ){` |
|       ! 0 |  5903 | `		return;   /* a comment, not a token */` |
|         - |  5904 | `	}` |
|         - |  5905 | `	/* The tokens bison prints as a quoted character: the operators and` |
|         - |  5906 | ``	 * parentheses of the value grammar itself, the `"` that opens a double`` |
|         - |  5907 | ``	 * quoted run, and the `=` php hands back to statement position. */`` |
|        94 |  5908 | `	if( c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '~' \|\| c == '('` |
|        85 |  5909 | `	 \|\| c == ')' \|\| c == '!' \|\| c == '"' \|\| c == '=' ){` |
|        41 |  5910 | `		pBad->bSet = 1;` |
|        41 |  5911 | `		pBad->cChar = c;` |
|        41 |  5912 | `		return;` |
|         - |  5913 | `	}` |
|        56 |  5914 | `	if( c == '\'' ){` |
|         - |  5915 | `		/* A raw string is one token only when it closes, and it needs at least` |
|         - |  5916 | ``		 * one byte inside: `''` matches nothing at all and php reports no`` |
|         - |  5917 | `		 * error over it. */` |
|       110 |  5918 | `		for( zWalk = &zCur[1] ; zWalk < zEnd ; zWalk++ ){` |
|       106 |  5919 | `			if( zWalk[0] == '\'' ){` |
|         6 |  5920 | `				if( zWalk > &zCur[1] ){` |
|         6 |  5921 | `					pBad->bSet = 1;` |
|         6 |  5922 | `					pBad->zTok = "TC_RAW";` |
|         3 |  5923 | `				}` |
|         6 |  5924 | `				return;` |
|         - |  5925 | `			}` |
|        50 |  5926 | `		}` |
|         4 |  5927 | `		return;` |
|         - |  5928 | `	}` |
|        46 |  5929 | `	if( c == '$' && &zCur[1] < zEnd && zCur[1] == '{' ){` |
|         4 |  5930 | `		pBad->bSet = 1;` |
|         4 |  5931 | `		pBad->zTok = "TC_DOLLAR_CURLY";` |
|         4 |  5932 | `		return;` |
|         - |  5933 | `	}` |
|        42 |  5934 | `	nVal = 0;` |
|       130 |  5935 | `	for( zWalk = zCur ; zWalk < zEnd ; ){` |
|        90 |  5936 | `		int nUnit = VmIniValueCharLen(zWalk,zEnd);` |
|        90 |  5937 | `		if( nUnit < 1 ){` |
|         2 |  5938 | `			break;` |
|         - |  5939 | `		}` |
|        88 |  5940 | `		zWalk += nUnit;` |
|        88 |  5941 | `		nVal += (sxu32)nUnit;` |
|       ! 0 |  5942 | `	}` |
|        42 |  5943 | `	if( nVal < 1 ){` |
|       ! 0 |  5944 | `		return;` |
|         - |  5945 | `	}` |
|        42 |  5946 | `	nCons = 0;` |
|        42 |  5947 | `	if( c < 0xc0 && (SyisAlpha(c) \|\| c == '_') ){` |
|        24 |  5948 | `		for( zWalk = zCur ; zWalk < zEnd ; zWalk++ ){` |
|        14 |  5949 | `			if( (unsigned char)zWalk[0] >= 0xc0` |
|        14 |  5950 | `			 \|\| (!SyisAlphaNum((unsigned char)zWalk[0]) && zWalk[0] != '_') ){` |
|       ! 0 |  5951 | `				break;` |
|         - |  5952 | `			}` |
|        14 |  5953 | `			nCons++;` |
|         7 |  5954 | `		}` |
|         5 |  5955 | `	}` |
|         - |  5956 | ``	/* php's NUMBER is `[-]?[0-9]+` or a decimal run with a dot on either side`` |
|         - |  5957 | `	 * of it; the sign belongs to the integer form alone. */` |
|        42 |  5958 | `	nNum = 0;` |
|        58 |  5959 | `	for( zWalk = c == '-' ? &zCur[1] : zCur ; zWalk < zEnd ; zWalk++ ){` |
|        50 |  5960 | `		if( !SyisDigit((unsigned char)zWalk[0]) ){` |
|        34 |  5961 | `			break;` |
|         - |  5962 | `		}` |
|        16 |  5963 | `		nNum++;` |
|         8 |  5964 | `	}` |
|        42 |  5965 | `	if( nNum > 0 && c == '-' ){` |
|         2 |  5966 | `		nNum++;` |
|        41 |  5967 | `	}else if( c != '-' && zWalk < zEnd && zWalk[0] == '.' ){` |
|         6 |  5968 | `		sxu32 nFrac = 0;` |
|         - |  5969 | `		const char *zFrac;` |
|        10 |  5970 | `		for( zFrac = &zWalk[1] ; zFrac < zEnd ; zFrac++ ){` |
|         4 |  5971 | `			if( !SyisDigit((unsigned char)zFrac[0]) ){` |
|       ! 0 |  5972 | `				break;` |
|         - |  5973 | `			}` |
|         4 |  5974 | `			nFrac++;` |
|         2 |  5975 | `		}` |
|         6 |  5976 | `		if( nNum > 0 \|\| nFrac > 0 ){` |
|         6 |  5977 | `			nNum += nFrac + 1;` |
|         3 |  5978 | `		}` |
|         3 |  5979 | `	}` |
|        42 |  5980 | `	pBad->bSet = 1;` |
|        42 |  5981 | `	pBad->zTok = nCons == nVal ? "TC_CONSTANT"` |
|        37 |  5982 | `	           : (nNum == nVal ? "TC_NUMBER" : "TC_STRING");` |
|       617 |  5983 | `}` |
|         - |  5984 | `/*` |
|         - |  5985 | ` * atoi() over an operand: php stops at the first byte that is not part of a` |
|         - |  5986 | ` * number and answers 0 when there is none.` |
|         - |  5987 | ` */` |
|       122 |  5988 | `static sxi32 VmIniExprInt(SyBlob *pVal)` |
|         4 |  5989 | `{` |
|       126 |  5990 | `	sxi32 iVal = 0;` |
|       126 |  5991 | `	if( SyBlobLength(pVal) > 0 ){` |
|       126 |  5992 | `		SyStrToInt32((const char *)SyBlobData(pVal),SyBlobLength(pVal),(void *)&iVal,0);` |
|        61 |  5993 | `	}` |
|       126 |  5994 | `	return iVal;` |
|         4 |  5995 | `}` |
|        42 |  5996 | `static void VmIniExprSetInt(SyBlob *pOut,sxi32 iVal)` |
|         4 |  5997 | `{` |
|         - |  5998 | `	char zBuf[32];` |
|        46 |  5999 | `	int nBuf = SyBufferFormat(zBuf,sizeof(zBuf),"%d",(int)iVal);` |
|        46 |  6000 | `	SyBlobReset(pOut);` |
|        46 |  6001 | `	SyBlobAppend(pOut,zBuf,(sxu32)nBuf);` |
|        46 |  6002 | `}` |
|         - |  6003 | `/*` |
|         - |  6004 | `` * php's `${NAME}` substitution, and php 8.5's `${NAME:-fallback}`. The name is`` |
|         - |  6005 | ` * answered in three steps (zend_ini_get_var): a directive ALREADY SET in this` |
|         - |  6006 | `` * source wins -- `precision=77` then `${precision}` is "77" whatever the`` |
|         - |  6007 | ` * environment says -- then the environment, then the fallback, then the empty` |
|         - |  6008 | ` * string. Only names that were set are visible: a directive left at its` |
|         - |  6009 | `` * built-in default is not in the table, so `${memory_limit}` is empty.`` |
|         - |  6010 | ` *` |
|         - |  6011 | ` * The name run is php's LABEL_CHAR: any byte that is not one of the value` |
|         - |  6012 | `` * grammar's own delimiters, `{`/`}`/`[`/`]`, or the `:` of a `:-`. Blanks are`` |
|         - |  6013 | `` * IN the run and trimmed off both ends afterwards, so `${ FOO }` is FOO and`` |
|         - |  6014 | `` * `${F OO}` is the three-word name "F OO".`` |
|         - |  6015 | ` */` |
|     16309 |  6016 | `static int VmIniVarNameStop(int c)` |
|         4 |  6017 | `{` |
|     24455 |  6018 | `	return c == '=' \|\| c == '\n' \|\| c == '\r' \|\| c == '\t' \|\| c == ';'` |
|     16301 |  6019 | `	    \|\| c == '&' \|\| c == '\|' \|\| c == '^' \|\| c == '$' \|\| c == '~'` |
|     16287 |  6020 | `	    \|\| c == '(' \|\| c == ')' \|\| c == '{' \|\| c == '}' \|\| c == '!'` |
|     24468 |  6021 | `	    \|\| c == '"' \|\| c == '[' \|\| c == ']' \|\| c == 0;` |
|         4 |  6022 | `}` |
|         - |  6023 | `/*` |
|         - |  6024 | ` * php's eight bool words. Its ini scanner has a rule for each of them, and` |
|         - |  6025 | ` * that rule stands AHEAD of the one that reads a LABEL -- so the same word is` |
|         - |  6026 | `` * a value's `1`/`""`/null and, at a statement position, a token no statement`` |
|         - |  6027 | `` * of php's grammar starts with. `on = 1` is `syntax error, unexpected`` |
|         - |  6028 | `` * BOOL_TRUE` where `onx = 1` is the entry "onx".`` |
|         - |  6029 | ` */` |
|         - |  6030 | `static const struct {` |
|         - |  6031 | `	const char *zWord;` |
|         - |  6032 | `	sxu32 nWord;` |
|         - |  6033 | `	const char *zText;   /* what the word reduces to inside a value */` |
|         - |  6034 | `	const char *zTok;    /* how php's parser names it when it refuses one */` |
|         - |  6035 | `} aVmIniBool[] = {` |
|         - |  6036 | `	{ "on"  ,2,"1","BOOL_TRUE"  }, { "yes"  ,3,"1","BOOL_TRUE"  },` |
|         - |  6037 | `	{ "true",4,"1","BOOL_TRUE"  }, { "off"  ,3,"" ,"BOOL_FALSE" },` |
|         - |  6038 | `	{ "no"  ,2,"" ,"BOOL_FALSE" }, { "false",5,"" ,"BOOL_FALSE" },` |
|         - |  6039 | `	{ "none",4,"" ,"BOOL_FALSE" }, { "null" ,4,"" ,"NULL_NULL"  }` |
|         - |  6040 | `};` |
|         - |  6041 | `/*` |
|         - |  6042 | ` * Screen the text standing where php's scanner reads a directive NAME, for a` |
|         - |  6043 | ` * host that walks a php.ini source itself (PH7_INI_STOP_STMT).` |
|         - |  6044 | ` *` |
|         - |  6045 | `` * php's INITIAL is not "everything up to the `=`": a `{LABEL}` run stops at`` |
|         - |  6046 | ` * every byte its operators, brackets and line ends are made of, and each of` |
|         - |  6047 | `` * those has a rule of its own behind it. A TAB, a `;` and a `[` open another`` |
|         - |  6048 | ` * statement, a comment and an offset, so a name may legitimately be several` |
|         - |  6049 | ` * runs -- but the twelve bytes below are tokens the grammar has no statement` |
|         - |  6050 | ` * for, and meeting one refuses the source from here down whether it opens the` |
|         - |  6051 | ` * name or sits in the middle of it. Three rules compete for the first run and` |
|         - |  6052 | ` * flex ranks them by length, then by the order they are written in:` |
|         - |  6053 | `` * `{LABEL}"["` outruns everything, the bool words come next, and `{LABEL}` is`` |
|         - |  6054 | `` * last. A word has to OPEN the run, and its `{TABS_AND_SPACES}*` tail is what`` |
|         - |  6055 | `` * lets it outrun a LABEL, which stops dead at a TAB -- so `on\t= 1` is`` |
|         - |  6056 | `` * BOOL_TRUE, `on x = 1` is the entry "on x", and `none = 1` is BOOL_FALSE`` |
|         - |  6057 | ` * only because the four-byte word outruns the two-byte one inside it.` |
|         - |  6058 | ` *` |
|         - |  6059 | ` * Answers 0 when php reads a name here, and otherwise the token it refuses` |
|         - |  6060 | ` * the statement under: a bool word by name, or the byte itself in quotes,` |
|         - |  6061 | ` * written into zBuf.` |
|         - |  6062 | ` */` |
|      1481 |  6063 | `static const char * VmIniStmtToken(const char *z,sxu32 nByte,char *zBuf)` |
|         4 |  6064 | `{` |
|      1485 |  6065 | `	const char *zEnd = &z[nByte];` |
|      1485 |  6066 | `	int bFirst = 1;` |
|       740 |  6067 | `	for(;;){` |
|      1485 |  6068 | `		const char *zRun = z;` |
|     16924 |  6069 | `		while( zRun < zEnd && !VmIniVarNameStop((unsigned char)zRun[0]) ){` |
|     15443 |  6070 | `			zRun++;` |
|         4 |  6071 | `		}` |
|      1485 |  6072 | `		if( bFirst && zRun > z && (zRun >= zEnd \|\| zRun[0] != '[') ){` |
|      1403 |  6073 | `			const char *zTok = 0;` |
|      1403 |  6074 | `			int nBest = 0;` |
|         - |  6075 | `			sxu32 i;` |
|     12595 |  6076 | `			for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; ++i ){` |
|         - |  6077 | `				const char *zTail;` |
|         - |  6078 | `				int nMatch;` |
|     11192 |  6079 | `				if( (sxu32)(zEnd - z) < aVmIniBool[i].nWord` |
|     11076 |  6080 | `				 \|\| SyStrnicmp(z,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|     11172 |  6081 | `					continue;` |
|         - |  6082 | `				}` |
|        24 |  6083 | `				zTail = &z[aVmIniBool[i].nWord];` |
|        30 |  6084 | `				while( zTail < zEnd && (zTail[0] == ' ' \|\| zTail[0] == '\t') ){` |
|         2 |  6085 | `					zTail++;` |
|       ! 0 |  6086 | `				}` |
|        24 |  6087 | `				nMatch = (int)(zTail - z);` |
|        24 |  6088 | `				if( nMatch > nBest ){` |
|        24 |  6089 | `					nBest = nMatch;` |
|        24 |  6090 | `					zTok = aVmIniBool[i].zTok;` |
|        12 |  6091 | `				}` |
|        12 |  6092 | `			}` |
|      1403 |  6093 | `			if( zTok && nBest >= (int)(zRun - z) ){` |
|        18 |  6094 | `				return zTok;` |
|         - |  6095 | `			}` |
|       690 |  6096 | `		}` |
|      1467 |  6097 | `		bFirst = 0;` |
|      1467 |  6098 | `		z = zRun;` |
|      1467 |  6099 | `		if( z >= zEnd \|\| z[0] == 0 ){` |
|      1377 |  6100 | `			return 0;   /* the statement ran out: nothing left to refuse */` |
|         - |  6101 | `		}` |
|        90 |  6102 | `		if( z[0] == '\t' \|\| z[0] == ' ' ){` |
|       ! 0 |  6103 | `			z++;        /* blanks the LABEL rule left behind are thrown away */` |
|       ! 0 |  6104 | `			continue;` |
|         - |  6105 | `		}` |
|        90 |  6106 | `		if( z[0] == ';' \|\| z[0] == '\n' \|\| z[0] == '\r' \|\| z[0] == '[' ){` |
|        50 |  6107 | `			return 0;   /* the comment, the newline and the offset own these */` |
|         - |  6108 | `		}` |
|        40 |  6109 | `		zBuf[0] = '\'';` |
|        40 |  6110 | `		zBuf[1] = z[0];` |
|        40 |  6111 | `		zBuf[2] = '\'';` |
|        40 |  6112 | `		zBuf[3] = 0;` |
|        40 |  6113 | `		return zBuf;` |
|       ! 0 |  6114 | `	}` |
|       744 |  6115 | `}` |
|         - |  6116 | `/*` |
|         - |  6117 | `` * Name the token php's parser finds where an offset statement's `=` has to be.`` |
|         - |  6118 | `` * The `]` that closes an offset pops the scanner back to INITIAL, and every`` |
|         - |  6119 | ` * token INITIAL can make there is a refusal because the grammar has no other` |
|         - |  6120 | `` * statement to build: `foo[bar]]` is `']'`, `foo[bar]x` is TC_LABEL, and`` |
|         - |  6121 | `` * `foo[bar]on` is BOOL_TRUE even though the same word inside a value is a 1.`` |
|         - |  6122 | ` * The three INITIAL runs rank as they do at any other statement position --` |
|         - |  6123 | `` * `{LABEL}"["` first, then the bool words, then `{LABEL}` -- so a second`` |
|         - |  6124 | ` * offset behind the first is TC_OFFSET rather than the label inside it.` |
|         - |  6125 | ` *` |
|         - |  6126 | ` * Answers 0 when the text names nothing, which is php's END_OF_LINE, and` |
|         - |  6127 | ` * otherwise the token: a symbol by name, or the byte in quotes in zBuf.` |
|         - |  6128 | ` */` |
|        24 |  6129 | `static const char * VmIniOffsetToken(const char *z,sxu32 nByte,char *zBuf)` |
|       ! 0 |  6130 | `{` |
|        24 |  6131 | `	const char *zEnd = &z[nByte];` |
|        24 |  6132 | `	const char *zRun,*zTok = 0;` |
|        24 |  6133 | `	int nBest = 0;` |
|         - |  6134 | `	sxu32 i;` |
|        38 |  6135 | `	while( z < zEnd && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|         4 |  6136 | `		z++;   /* every INITIAL rule that can match here eats its own blanks */` |
|       ! 0 |  6137 | `	}` |
|        24 |  6138 | `	if( z >= zEnd \|\| z[0] == '\n' \|\| z[0] == '\r' ){` |
|         8 |  6139 | `		return 0;   /* the newline rule, which is an END_OF_LINE */` |
|         - |  6140 | `	}` |
|        16 |  6141 | `	if( z[0] == ';' ){` |
|         2 |  6142 | `		return 0;   /* so is the comment rule: it ends on the same newline */` |
|         - |  6143 | `	}` |
|        14 |  6144 | `	if( z[0] == '[' ){` |
|         2 |  6145 | `		return "TC_SECTION";` |
|         - |  6146 | `	}` |
|        12 |  6147 | `	zRun = z;` |
|        22 |  6148 | `	while( zRun < zEnd && !VmIniVarNameStop((unsigned char)zRun[0]) ){` |
|        10 |  6149 | `		zRun++;` |
|       ! 0 |  6150 | `	}` |
|        12 |  6151 | `	if( zRun == z ){` |
|         - |  6152 | `		/* one of the bytes no run of php's takes: it reaches the parser as` |
|         - |  6153 | `		 * itself, and bison prints it as a quoted character */` |
|         6 |  6154 | `		zBuf[0] = '\'';` |
|         6 |  6155 | `		zBuf[1] = z[0];` |
|         6 |  6156 | `		zBuf[2] = '\'';` |
|         6 |  6157 | `		zBuf[3] = 0;` |
|         6 |  6158 | `		return zBuf;` |
|         - |  6159 | `	}` |
|         6 |  6160 | `	if( zRun < zEnd && zRun[0] == '[' ){` |
|         2 |  6161 | `		return "TC_OFFSET";` |
|         - |  6162 | `	}` |
|        36 |  6163 | `	for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; ++i ){` |
|         - |  6164 | `		const char *zTail;` |
|         - |  6165 | `		int nMatch;` |
|        32 |  6166 | `		if( (sxu32)(zEnd - z) < aVmIniBool[i].nWord` |
|        18 |  6167 | `		 \|\| SyStrnicmp(z,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|        30 |  6168 | `			continue;` |
|         - |  6169 | `		}` |
|         2 |  6170 | `		zTail = &z[aVmIniBool[i].nWord];` |
|         2 |  6171 | `		while( zTail < zEnd && (zTail[0] == ' ' \|\| zTail[0] == '\t') ){` |
|       ! 0 |  6172 | `			zTail++;` |
|       ! 0 |  6173 | `		}` |
|         2 |  6174 | `		nMatch = (int)(zTail - z);` |
|         2 |  6175 | `		if( nMatch > nBest ){` |
|         2 |  6176 | `			nBest = nMatch;` |
|         2 |  6177 | `			zTok = aVmIniBool[i].zTok;` |
|         1 |  6178 | `		}` |
|         1 |  6179 | `	}` |
|         4 |  6180 | `	if( zTok && nBest >= (int)(zRun - z) ){` |
|         2 |  6181 | `		return zTok;` |
|         - |  6182 | `	}` |
|         2 |  6183 | `	return "TC_LABEL";` |
|        12 |  6184 | `}` |
|         - |  6185 | `/*` |
|         - |  6186 | ` * The first two steps of that lookup. FALSE means the name is nowhere, which` |
|         - |  6187 | ` * is what hands the question on to a fallback.` |
|         - |  6188 | ` */` |
|        86 |  6189 | `static int VmIniVarLookup(ph7_vm *pVm,const char *zName,sxu32 nName,SyBlob *pOut)` |
|       ! 0 |  6190 | `{` |
|         - |  6191 | `	VmIniEntry *aEntry;` |
|         - |  6192 | `	const char *zEnv;` |
|         - |  6193 | `	char zName0[256];` |
|         - |  6194 | `	sxu32 n;` |
|        86 |  6195 | `	if( nName < 1 ){` |
|         2 |  6196 | `		return 0;` |
|         - |  6197 | `	}` |
|        84 |  6198 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pVm->aIniCli);` |
|         - |  6199 | `	/* Backwards: a directive written twice answers with the last one, which is` |
|         - |  6200 | `	 * also the one that stands. */` |
|        84 |  6201 | `	for( n = SySetUsed(&pVm->aIniCli) ; n > 0 ; n-- ){` |
|         4 |  6202 | `		SyString *pName = &aEntry[n-1].sName;` |
|         4 |  6203 | `		if( pName->nByte == nName && SyMemcmp(pName->zString,zName,nName) == 0 ){` |
|         4 |  6204 | `			SyBlobAppend(pOut,aEntry[n-1].sValue.zString,aEntry[n-1].sValue.nByte);` |
|         4 |  6205 | `			return 1;` |
|         - |  6206 | `		}` |
|       ! 0 |  6207 | `	}` |
|         - |  6208 | `	/* getenv() wants a C string and the name is a slice of the ini source. A` |
|         - |  6209 | `	 * name longer than any real environment variable is simply absent.` |
|         - |  6210 | `	 *` |
|         - |  6211 | `	 * It also trips MSVC's C4996 "may be unsafe" deprecation under /WX -- the` |
|         - |  6212 | `	 * same standard function used deliberately, suppressed the same way vfs.c` |
|         - |  6213 | `	 * suppresses it for strerror(), and _MSC_VER-guarded so the GCC build never` |
|         - |  6214 | `	 * sees an unknown pragma. */` |
|        80 |  6215 | `	if( nName >= sizeof(zName0) ){` |
|       ! 0 |  6216 | `		return 0;` |
|         - |  6217 | `	}` |
|        80 |  6218 | `	SyMemcpy(zName,zName0,nName);` |
|        80 |  6219 | `	zName0[nName] = 0;` |
|         - |  6220 | `#if defined(_MSC_VER)` |
|         - |  6221 | `#pragma warning(push)` |
|         - |  6222 | `#pragma warning(disable:4996)` |
|         - |  6223 | `#endif` |
|        80 |  6224 | `	zEnv = getenv(zName0);` |
|         - |  6225 | `#if defined(_MSC_VER)` |
|         - |  6226 | `#pragma warning(pop)` |
|         - |  6227 | `#endif` |
|        80 |  6228 | `	if( zEnv == 0 ){` |
|        62 |  6229 | `		return 0;` |
|         - |  6230 | `	}` |
|        18 |  6231 | `	SyBlobAppend(pOut,zEnv,(sxu32)SyStrlen(zEnv));` |
|        18 |  6232 | `	return 1;` |
|        43 |  6233 | `}` |
|         - |  6234 | `static int VmIniExprVar(VmIniExpr *p,SyBlob *pOut,int nDepth);` |
|         - |  6235 | `/*` |
|         - |  6236 | ` * One quoted run. php collapses exactly three escapes inside a double-quoted` |
|         - |  6237 | ` * run and keeps both bytes of every other one; a single-quoted run carries no` |
|         - |  6238 | `` * escapes at all and no substitution either, so `'${FOO}'` is its own seven`` |
|         - |  6239 | `` * bytes. A tool that has to put a quote in an ini value writes `\"` -- PHPUnit's`` |
|         - |  6240 | ` * job runner does -- and reading that as the end of the run cut the value in` |
|         - |  6241 | ` * half.` |
|         - |  6242 | ` */` |
|        80 |  6243 | `static int VmIniExprQuoted(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|         3 |  6244 | `{` |
|        83 |  6245 | `	int c = (unsigned char)p->zCur[0];` |
|        83 |  6246 | `	p->zCur++;` |
|       639 |  6247 | `	while( p->zCur < p->zEnd && (unsigned char)p->zCur[0] != c ){` |
|       559 |  6248 | `		if( c == '"' && (p->zCur[0] == '\n' \|\| p->zCur[0] == '\r') ){` |
|         - |  6249 | `			/* ST_DOUBLE_QUOTES is the only quoted run that counts what it eats. */` |
|        42 |  6250 | `			if( p->zCur[0] == '\r' && &p->zCur[1] < p->zEnd && p->zCur[1] == '\n' ){` |
|       ! 0 |  6251 | `				SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|       ! 0 |  6252 | `				p->zCur++;` |
|       ! 0 |  6253 | `			}` |
|        42 |  6254 | `			p->nLine++;` |
|        42 |  6255 | `			SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|        42 |  6256 | `			p->zCur++;` |
|        42 |  6257 | `			continue;` |
|         - |  6258 | `		}` |
|       517 |  6259 | `		if( c == '"' && p->zCur[0] == '\\' && &p->zCur[1] < p->zEnd ){` |
|       ! 0 |  6260 | `			int e = (unsigned char)p->zCur[1];` |
|       ! 0 |  6261 | `			if( e == '"' \|\| e == '\\' \|\| e == '$' ){` |
|       ! 0 |  6262 | `				SyBlobAppend(pOut,&p->zCur[1],sizeof(char));` |
|       ! 0 |  6263 | `			}else{` |
|       ! 0 |  6264 | `				SyBlobAppend(pOut,p->zCur,2*sizeof(char));` |
|         - |  6265 | `			}` |
|       ! 0 |  6266 | `			p->zCur += 2;` |
|       ! 0 |  6267 | `			continue;` |
|         - |  6268 | `		}` |
|       517 |  6269 | `		if( c == '"' && p->zCur[0] == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|         - |  6270 | ``			/* ST_DOUBLE_QUOTES takes `${` too. */`` |
|         2 |  6271 | `			if( !VmIniExprVar(p,pOut,nDepth+1) ){` |
|       ! 0 |  6272 | `				return 0;` |
|         - |  6273 | `			}` |
|         2 |  6274 | `			continue;` |
|         - |  6275 | `		}` |
|       515 |  6276 | `		SyBlobAppend(pOut,p->zCur,sizeof(char));` |
|       515 |  6277 | `		p->zCur++;` |
|         3 |  6278 | `	}` |
|        83 |  6279 | `	if( p->zCur >= p->zEnd ){` |
|         - |  6280 | `		/* php's ST_DOUBLE_QUOTES runs to the end of the SOURCE, not to the end` |
|         - |  6281 | `		 * of the line, so a quote with no partner is the end of file and the` |
|         - |  6282 | `		 * whole value is refused. Dropping the quote and keeping the letters` |
|         - |  6283 | ``		 * made `a"b` the two bytes "ab". The caller names the token: this one is`` |
|         - |  6284 | ``		 * also reached from inside `${NAME:-...}`, which has an expect-list of`` |
|         - |  6285 | `		 * its own. */` |
|        14 |  6286 | `		return 0;` |
|         - |  6287 | `	}` |
|        69 |  6288 | `	p->zCur++;    /* the closing quote */` |
|        69 |  6289 | `	return 1;` |
|        43 |  6290 | `}` |
|         - |  6291 | `/*` |
|         - |  6292 | `` * `${` ... `}`, with p->zCur on the `$`. Everything it can refuse refuses the`` |
|         - |  6293 | ` * whole VALUE -- php's parser never reduces the directive -- so the directive` |
|         - |  6294 | ` * keeps its default and the rest of the source is dropped, exactly as any other` |
|         - |  6295 | ` * ini syntax error is.` |
|         - |  6296 | ` *` |
|         - |  6297 | ` * The three refusals are php's own, and the middle one is php's scanner being` |
|         - |  6298 | `` * literal about a one-byte name: `<ST_VARNAME>{LABEL_CHAR}` matches ONE byte and`` |
|         - |  6299 | `` * then looks ahead, and when what follows is `:-` it jumps straight to the`` |
|         - |  6300 | `` * fallback state WITHOUT returning the name it just read. So `${NN:-x}` is`` |
|         - |  6301 | `` * "x" and `${N:-x}` is a syntax error over a token the parser never got.`` |
|         - |  6302 | ` */` |
|       146 |  6303 | `static int VmIniExprVar(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|       ! 0 |  6304 | `{` |
|         - |  6305 | `	const char *zName,*zRaw;` |
|         - |  6306 | `	SyBlob sFallback;` |
|         - |  6307 | `	sxu32 nName,nRaw;` |
|       146 |  6308 | `	int bFallback = 0;` |
|       146 |  6309 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|       ! 0 |  6310 | `		VmIniExprStopAt(p,"end of file",0,"'}'");` |
|       ! 0 |  6311 | `		return 0;` |
|         - |  6312 | `	}` |
|         - |  6313 | ``	/* Seeded here rather than under the `:-` branch that fills it: MSVC cannot`` |
|         - |  6314 | `	 * prove bFallback gates every use and rejects the blob as possibly` |
|         - |  6315 | `	 * uninitialised under /WX. */` |
|       146 |  6316 | `	SyBlobInit(&sFallback,&p->pVm->sAllocator);` |
|       146 |  6317 | ``	p->zCur += 2;    /* `${` */`` |
|       146 |  6318 | `	zRaw = p->zCur;` |
|       768 |  6319 | `	while( p->zCur < p->zEnd ){` |
|       762 |  6320 | `		int c = (unsigned char)p->zCur[0];` |
|       762 |  6321 | `		if( VmIniVarNameStop(c) ){` |
|        66 |  6322 | `			break;` |
|         - |  6323 | `		}` |
|       696 |  6324 | `		if( c == ':' && &p->zCur[1] < p->zEnd && p->zCur[1] == '-' ){` |
|        74 |  6325 | `			break;` |
|         - |  6326 | `		}` |
|       622 |  6327 | `		p->zCur++;` |
|       ! 0 |  6328 | `	}` |
|       146 |  6329 | `	nRaw = (sxu32)(p->zCur - zRaw);` |
|       146 |  6330 | `	if( nRaw < 1 ){` |
|         - |  6331 | ``		/* Nothing the name rule could take at all. `${}` names the brace;`` |
|         - |  6332 | `		 * anything else -- a delimiter, or the source running out -- reaches` |
|         - |  6333 | `		 * php's scanner with no rule left and reads as the end of file. */` |
|        16 |  6334 | `		if( p->zCur < p->zEnd && p->zCur[0] == '}' ){` |
|         8 |  6335 | `			VmIniExprStopAt(p,0,'}',"TC_VARNAME");` |
|         4 |  6336 | `		}else{` |
|         8 |  6337 | `			VmIniExprStopAt(p,"end of file",0,"TC_VARNAME");` |
|         - |  6338 | `		}` |
|        16 |  6339 | `		SyBlobRelease(&sFallback);` |
|        16 |  6340 | `		return 0;` |
|         - |  6341 | `	}` |
|       130 |  6342 | `	if( nRaw == 1 && p->zCur < p->zEnd && p->zCur[0] == ':' ){` |
|        10 |  6343 | `		VmIniExprStopAt(p,"TC_FALLBACK",0,"TC_VARNAME");` |
|        10 |  6344 | `		SyBlobRelease(&sFallback);` |
|        10 |  6345 | `		return 0;` |
|         - |  6346 | `	}` |
|       120 |  6347 | `	zName = zRaw;` |
|       120 |  6348 | `	nName = nRaw;` |
|       184 |  6349 | `	while( nName > 0 && (zName[0] == ' ' \|\| zName[0] == '\t') ){` |
|         4 |  6350 | `		zName++;` |
|         4 |  6351 | `		nName--;` |
|       ! 0 |  6352 | `	}` |
|       182 |  6353 | `	while( nName > 0 && (zName[nName-1] == ' ' \|\| zName[nName-1] == '\t') ){` |
|         2 |  6354 | `		nName--;` |
|       ! 0 |  6355 | `	}` |
|       120 |  6356 | `	if( p->zCur < p->zEnd && p->zCur[0] == ':' ){` |
|         - |  6357 | ``		/* The fallback state takes text, `\<byte>` pairs kept WHOLE, nested`` |
|         - |  6358 | `		 * substitutions and double-quoted runs -- and nothing else: a newline,` |
|         - |  6359 | ``		 * a `;` or a raw quote leaves php with no rule and it reports the end`` |
|         - |  6360 | ``		 * of file. Blanks here are content, so `${NN:- }` is one space. */`` |
|        64 |  6361 | `		p->zCur += 2;` |
|        64 |  6362 | `		bFallback = 1;` |
|       180 |  6363 | `		while( p->zCur < p->zEnd ){` |
|       178 |  6364 | `			int c = (unsigned char)p->zCur[0];` |
|       178 |  6365 | `			if( c == '}' ){` |
|        40 |  6366 | `				break;` |
|         - |  6367 | `			}` |
|       138 |  6368 | `			if( c == '\\' && &p->zCur[1] < p->zEnd ){` |
|         6 |  6369 | `				SyBlobAppend(&sFallback,p->zCur,2*sizeof(char));` |
|         6 |  6370 | `				p->zCur += 2;` |
|         6 |  6371 | `				continue;` |
|         - |  6372 | `			}` |
|       132 |  6373 | `			if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|         8 |  6374 | `				if( !VmIniExprVar(p,&sFallback,nDepth+1) ){` |
|         2 |  6375 | `					SyBlobRelease(&sFallback);` |
|         2 |  6376 | `					return 0;` |
|         - |  6377 | `				}` |
|         6 |  6378 | `				continue;` |
|         - |  6379 | `			}` |
|       124 |  6380 | `			if( c == '"' ){` |
|        16 |  6381 | `				if( !VmIniExprQuoted(p,&sFallback,nDepth) ){` |
|         - |  6382 | `					/* The run reaches the end of the SOURCE, and what php` |
|         - |  6383 | `					 * names there is its double-quote state's own expect-list` |
|         - |  6384 | ``					 * rather than the `}` the fallback around it still wants:`` |
|         - |  6385 | ``					 * `${A:-"x` and `${A:-x"` both stop under it. */`` |
|         8 |  6386 | `					SyBlobRelease(&sFallback);` |
|         8 |  6387 | `					VmIniExprStopAt(p,"end of file",0,` |
|         - |  6388 | `						"TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'");` |
|         8 |  6389 | `					return 0;` |
|         - |  6390 | `				}` |
|         8 |  6391 | `				continue;` |
|         - |  6392 | `			}` |
|       108 |  6393 | `			if( c == '\n' \|\| c == '\r' \|\| c == ';' \|\| c == '\'' ){` |
|         6 |  6394 | `				break;` |
|         - |  6395 | `			}` |
|        96 |  6396 | `			SyBlobAppend(&sFallback,p->zCur,sizeof(char));` |
|        96 |  6397 | `			p->zCur++;` |
|       ! 0 |  6398 | `		}` |
|        54 |  6399 | `		if( p->zCur >= p->zEnd \|\| p->zCur[0] != '}' ){` |
|        14 |  6400 | `			SyBlobRelease(&sFallback);` |
|        14 |  6401 | `			VmIniExprStopAt(p,"end of file",0,"'}'");` |
|        14 |  6402 | `			return 0;` |
|         - |  6403 | `		}` |
|        20 |  6404 | `	}` |
|        96 |  6405 | `	if( p->zCur >= p->zEnd \|\| p->zCur[0] != '}' ){` |
|        10 |  6406 | `		SyBlobRelease(&sFallback);` |
|        10 |  6407 | `		VmIniExprStopAt(p,"end of file",0,"TC_FALLBACK or '}'");` |
|        10 |  6408 | `		return 0;` |
|         - |  6409 | `	}` |
|        86 |  6410 | ``	p->zCur++;    /* `}` */`` |
|        86 |  6411 | `	if( !VmIniVarLookup(p->pVm,zName,nName,pOut) && bFallback ){` |
|        38 |  6412 | `		SyBlobAppend(pOut,SyBlobData(&sFallback),SyBlobLength(&sFallback));` |
|        19 |  6413 | `	}` |
|        86 |  6414 | `	SyBlobRelease(&sFallback);` |
|        86 |  6415 | `	return 1;` |
|        73 |  6416 | `}` |
|         - |  6417 | `/*` |
|         - |  6418 | `` * One operand: everything up to the next operator, parenthesis or `;` comment,`` |
|         - |  6419 | ` * with each bare identifier replaced by the constant of that name when one is` |
|         - |  6420 | ` * defined and left standing as its own text when none is, and each quoted run` |
|         - |  6421 | ``  * taken literally. Trailing blanks are not part of the token, so `E_ALL ; x` `` |
|         - |  6422 | ` * stores "30719" and not "30719 ".` |
|         - |  6423 | ` */` |
|      1347 |  6424 | `static int VmIniExprOperand(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|         4 |  6425 | `{` |
|      1351 |  6426 | `	const char *zPend = 0;   /* blanks held back: trailing ones are not part of the` |
|         - |  6427 | `	                          * token, interior ones are ("E_NOTICE E_WARNING" is "8 2") */` |
|      1351 |  6428 | `	sxu32 nPend = 0;` |
|      1351 |  6429 | `	int bAny = 0;` |
|      1351 |  6430 | `	SyBlobReset(pOut);` |
|      1351 |  6431 | `	VmIniExprSpace(p);` |
|      2942 |  6432 | `	while( p->zCur < p->zEnd ){` |
|      1805 |  6433 | `		int c = (unsigned char)p->zCur[0];` |
|      1801 |  6434 | `		if( VmIniExprIsOp(c) \|\| c == '(' \|\| c == ')'` |
|      1681 |  6435 | `		 \|\| c == '~' \|\| c == '!' \|\| c == ';' ){` |
|        89 |  6436 | `			break;` |
|         - |  6437 | `		}` |
|      1635 |  6438 | `		if( c == ' ' \|\| c == '\t' ){` |
|        90 |  6439 | `			if( nPend == 0 ){` |
|        90 |  6440 | `				zPend = p->zCur;` |
|        43 |  6441 | `			}` |
|        90 |  6442 | `			nPend++;` |
|        90 |  6443 | `			p->zCur++;` |
|        90 |  6444 | `			continue;` |
|         - |  6445 | `		}` |
|      1549 |  6446 | `		if( nPend > 0 ){` |
|        12 |  6447 | `			SyBlobAppend(pOut,zPend,nPend);` |
|        12 |  6448 | `			nPend = 0;` |
|         5 |  6449 | `		}` |
|      1549 |  6450 | `		if( c == '$' && &p->zCur[1] < p->zEnd && p->zCur[1] == '{' ){` |
|        84 |  6451 | `			if( !VmIniExprVar(p,pOut,nDepth) ){` |
|        22 |  6452 | `				return 0;` |
|         - |  6453 | `			}` |
|         - |  6454 | `			/* A substitution that answered nothing still MADE a value: the` |
|         - |  6455 | `			 * directive lands as the empty string rather than keeping its` |
|         - |  6456 | `			 * default. */` |
|        62 |  6457 | `			bAny = 1;` |
|        62 |  6458 | `			continue;` |
|         - |  6459 | `		}` |
|      1465 |  6460 | `		if( c == '\'' ){` |
|        12 |  6461 | `			const char *zQ = &p->zCur[1];` |
|       148 |  6462 | `			while( zQ < p->zEnd && zQ[0] != '\'' ){` |
|       136 |  6463 | `				zQ++;` |
|       ! 0 |  6464 | `			}` |
|        12 |  6465 | `			if( zQ >= p->zEnd ){` |
|         - |  6466 | `				/* php's raw-string rule is one match spanning as many newlines` |
|         - |  6467 | `				 * as it needs; with no closing quote it matches nothing and the` |
|         - |  6468 | `				 * scanner leaves for the end of the source. The operand ends` |
|         - |  6469 | `` 				 * here, and what it already holds is still a value: `x = A'b` `` |
|         - |  6470 | `				 * stores "A" and reports nothing at all. */` |
|         4 |  6471 | `				p->bRawEof = 1;` |
|         4 |  6472 | `				break;` |
|         - |  6473 | `			}` |
|         4 |  6474 | `		}` |
|      1461 |  6475 | `		if( c == '"' \|\| c == '\'' ){` |
|        67 |  6476 | `			if( !VmIniExprQuoted(p,pOut,nDepth) ){` |
|         - |  6477 | `				/* Only the double-quoted run can get here now, and php names its` |
|         - |  6478 | `				 * refusal with the tokens ST_DOUBLE_QUOTES was still willing to` |
|         - |  6479 | `				 * take. */` |
|         6 |  6480 | `				VmIniExprStopAt(p,"end of file",0,` |
|         - |  6481 | `					"TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'");` |
|         6 |  6482 | `				return 0;` |
|         - |  6483 | `			}` |
|        61 |  6484 | `			bAny = 1;` |
|        61 |  6485 | `			continue;` |
|         - |  6486 | `		}` |
|      1397 |  6487 | `		if( c < 0xc0 && (SyisAlpha(c) \|\| c == '_') ){` |
|       438 |  6488 | `			const char *zTok = p->zCur;` |
|         - |  6489 | `			ph7_value sCons;` |
|      1923 |  6490 | `			while( p->zCur < p->zEnd` |
|      2802 |  6491 | `			 && (unsigned char)p->zCur[0] < 0xc0` |
|      4119 |  6492 | `			 && (SyisAlphaNum((unsigned char)p->zCur[0]) \|\| p->zCur[0] == '_') ){` |
|      2548 |  6493 | `				p->zCur++;` |
|         4 |  6494 | `			}` |
|       438 |  6495 | `			PH7_MemObjInit(p->pVm,&sCons);` |
|         - |  6496 | `			/* php reads php.ini before a single extension has registered a` |
|         - |  6497 | ``			 * constant, so only the ENGINE's own answer here: `M_PI`,`` |
|         - |  6498 | ``			 * `SORT_ASC` and `DIRECTORY_SEPARATOR` are ext/standard's and`` |
|         - |  6499 | `			 * store their own NAMES, while the same text through` |
|         - |  6500 | `			 * parse_ini_file() at runtime stores the constant. */` |
|       434 |  6501 | `			if( PH7_VmExtOfConstant(zTok,(int)(p->zCur - zTok)) == PH7_EXT_CORE` |
|       437 |  6502 | `			 && (PH7_ExpandBuiltinConstant(p->pVm,zTok,(sxu32)(p->zCur - zTok),&sCons)` |
|       432 |  6503 | `			  \|\| PH7_VmQueryConstant(p->pVm,zTok,(sxu32)(p->zCur - zTok),&sCons)) ){` |
|        52 |  6504 | `				int nCons = 0;` |
|        52 |  6505 | `				const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|        52 |  6506 | `				SyBlobAppend(pOut,zCons,(sxu32)nCons);` |
|        28 |  6507 | `			}else{` |
|       390 |  6508 | `				SyBlobAppend(pOut,zTok,(sxu32)(p->zCur - zTok));` |
|         - |  6509 | `			}` |
|       438 |  6510 | `			PH7_MemObjRelease(&sCons);` |
|       438 |  6511 | `			bAny = 1;` |
|       438 |  6512 | `			continue;` |
|         - |  6513 | `		}` |
|         - |  6514 | `		{` |
|         - |  6515 | `			/* Everything else is VALUE_CHARS, and the run spends whole UNITS` |
|         - |  6516 | `			 * of it: a byte that opens no unit is not part of the operand and` |
|         - |  6517 | ``			 * ends it. `=` and a bare newline are two of those, so a value`` |
|         - |  6518 | `			 * cannot walk down into the statement below on its own -- the one` |
|         - |  6519 | ``			 * way through is the `$` unit, which carries whatever byte follows`` |
|         - |  6520 | `			 * it and hands the run-on the next line's text. */` |
|       963 |  6521 | `			int nUnit = VmIniValueCharLen(p->zCur,p->zEnd);` |
|       963 |  6522 | `			if( nUnit < 1 ){` |
|         8 |  6523 | `				break;` |
|         - |  6524 | `			}` |
|       955 |  6525 | `			SyBlobAppend(pOut,p->zCur,(sxu32)nUnit);` |
|       955 |  6526 | `			p->zCur += nUnit;` |
|       955 |  6527 | `			bAny = 1;` |
|         - |  6528 | `		}` |
|         4 |  6529 | `	}` |
|      1323 |  6530 | `	return bAny;` |
|       677 |  6531 | `}` |
|         - |  6532 | `static int VmIniExprEval(VmIniExpr *p,SyBlob *pOut,int nDepth);` |
|      1491 |  6533 | `static int VmIniExprUnary(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|         4 |  6534 | `{` |
|         - |  6535 | `	int c;` |
|      1495 |  6536 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|       ! 0 |  6537 | `		return 0;` |
|         - |  6538 | `	}` |
|      1495 |  6539 | `	VmIniExprSpace(p);` |
|      1495 |  6540 | `	if( p->zCur >= p->zEnd ){` |
|        46 |  6541 | `		VmIniExprStop(p,0,0);` |
|        46 |  6542 | `		return 0;` |
|         - |  6543 | `	}` |
|      1449 |  6544 | `	c = p->zCur[0];` |
|      1449 |  6545 | `	if( c == '~' \|\| c == '!' ){` |
|        32 |  6546 | `		p->zCur++;` |
|        32 |  6547 | `		if( !VmIniExprUnary(p,pOut,nDepth+1) ){` |
|        12 |  6548 | `			return 0;` |
|         - |  6549 | `		}` |
|        20 |  6550 | `		VmIniExprSetInt(pOut,c == '~' ? ~VmIniExprInt(pOut)` |
|       ! 0 |  6551 | `		                              : (sxi32)(VmIniExprInt(pOut) == 0));` |
|        20 |  6552 | `		return 1;` |
|         - |  6553 | `	}` |
|      1421 |  6554 | `	if( c == '(' ){` |
|        71 |  6555 | `		p->zCur++;` |
|        71 |  6556 | `		if( !VmIniExprEval(p,pOut,nDepth+1) ){` |
|         4 |  6557 | `			return 0;` |
|         - |  6558 | `		}` |
|        67 |  6559 | `		VmIniExprSpace(p);` |
|        67 |  6560 | `		if( p->zCur >= p->zEnd \|\| p->zCur[0] != ')' ){` |
|         - |  6561 | `			/* A complete expression with an unclosed '(' behind it: php's parser` |
|         - |  6562 | `			 * can still take an operator or the ')', and says so. */` |
|        12 |  6563 | `			VmIniExprStop(p,p->zCur >= p->zEnd ? 0 : (unsigned char)p->zCur[0],1);` |
|        12 |  6564 | `			return 0;` |
|         - |  6565 | `		}` |
|        55 |  6566 | `		p->zCur++;` |
|        55 |  6567 | `		return 1;` |
|         - |  6568 | `	}` |
|      1351 |  6569 | `	if( !VmIniExprOperand(p,pOut,nDepth) ){` |
|        52 |  6570 | `		if( p->bRawEof ){` |
|         - |  6571 | `			/* Nothing had reduced yet, so the scanner's leap to the end of the` |
|         - |  6572 | `			 * source is the token php's parser chokes on -- and it is the end of` |
|         - |  6573 | `			 * the FILE, not the end of a line, so it does not move the line the` |
|         - |  6574 | `			 * refusal is dated to. */` |
|         2 |  6575 | `			VmIniExprStopAt(p,"end of file",0,0);` |
|         1 |  6576 | `		}else{` |
|        50 |  6577 | `			VmIniExprStop(p,(unsigned char)p->zCur[0],0);` |
|         - |  6578 | `		}` |
|        52 |  6579 | `		return 0;` |
|         - |  6580 | `	}` |
|      1299 |  6581 | `	return 1;` |
|       749 |  6582 | `}` |
|      1383 |  6583 | `static int VmIniExprEval(VmIniExpr *p,SyBlob *pOut,int nDepth)` |
|         4 |  6584 | `{` |
|      1387 |  6585 | `	if( nDepth > VM_INI_EXPR_MAX_DEPTH ){` |
|       ! 0 |  6586 | `		return 0;` |
|         - |  6587 | `	}` |
|      1387 |  6588 | `	if( !VmIniExprUnary(p,pOut,nDepth) ){` |
|        60 |  6589 | `		return 0;` |
|         - |  6590 | `	}` |
|       687 |  6591 | `	for(;;){` |
|         - |  6592 | `		SyBlob sRhs;` |
|         - |  6593 | `		sxi32 iLhs,iRhs,iRes;` |
|         - |  6594 | `		int c;` |
|      1353 |  6595 | `		VmIniExprSpace(p);` |
|      1353 |  6596 | `		if( p->zCur >= p->zEnd \|\| !VmIniExprIsOp(p->zCur[0]) ){` |
|       638 |  6597 | `			break;` |
|         - |  6598 | `		}` |
|        84 |  6599 | `		c = p->zCur[0];` |
|        84 |  6600 | `		p->zCur++;` |
|        84 |  6601 | `		iLhs = VmIniExprInt(pOut);` |
|        84 |  6602 | `		SyBlobInit(&sRhs,&p->pVm->sAllocator);` |
|        84 |  6603 | `		if( !VmIniExprUnary(p,&sRhs,nDepth+1) ){` |
|        54 |  6604 | `			SyBlobRelease(&sRhs);` |
|        54 |  6605 | `			return 0;` |
|         - |  6606 | `		}` |
|        30 |  6607 | `		iRhs = VmIniExprInt(&sRhs);` |
|        30 |  6608 | `		SyBlobRelease(&sRhs);` |
|        30 |  6609 | `		iRes = c == '\|' ? (iLhs \| iRhs) : (c == '&' ? (iLhs & iRhs) : (iLhs ^ iRhs));` |
|        30 |  6610 | `		VmIniExprSetInt(pOut,iRes);` |
|         4 |  6611 | `	}` |
|      1273 |  6612 | `	return 1;` |
|       695 |  6613 | `}` |
|         - |  6614 | `/*` |
|         - |  6615 | ` * Evaluate one php.ini value into the text php would store for it. FALSE means` |
|         - |  6616 | ` * nothing ever reduced to a complete value at all -- a dangling operator or an` |
|         - |  6617 | ` * unmatched '(' -- and php leaves the directive at its default rather than at` |
|         - |  6618 | `` * zero: `error_reporting = E_ALL &` keeps whatever was there before.`` |
|         - |  6619 | ` *` |
|         - |  6620 | ` * A syntax error elsewhere does NOT mean FALSE: php's yacc grammar reduces` |
|         - |  6621 | `` * `string_or_value` (and fires the assignment) as soon as the lookahead byte`` |
|         - |  6622 | ` * cannot extend it further, and only THEN discovers that byte cannot start` |
|         - |  6623 | `` * anything either. `error_reporting = E_ALL)` stores 30719 and separately`` |
|         - |  6624 | ` * warns about the ')' -- the value most be committed even though the whole` |
|         - |  6625 | ` * directive text was not clean.` |
|         - |  6626 | ` *` |
|         - |  6627 | ` * *pBad is what php would REPORT over this value, and it is filled on both` |
|         - |  6628 | ` * paths: a leftover byte over a value that committed, and, when the parse` |
|         - |  6629 | ` * failed outright, wherever the expression walker stopped. Its bSet is the` |
|         - |  6630 | ` * question "was this a syntax error at all" -- the caller prints php's` |
|         - |  6631 | ` * warning over it AND stops feeding the rest of that ini source, which php` |
|         - |  6632 | ` * does because it hands each source to its parser whole (see` |
|         - |  6633 | ` * VmSetIniEntry and PH7_VmApplyEngineIni).` |
|         - |  6634 | ` */` |
|      1373 |  6635 | `static int VmIniEvalValue(ph7_vm *pVm,const char *zVal,sxu32 nVal,SyBlob *pOut,VmIniBad *pBad)` |
|         4 |  6636 | `{` |
|         - |  6637 | `	VmIniExpr sIn;` |
|         - |  6638 | `	sxu32 i;` |
|      1377 |  6639 | `	pBad->bSet = 0;` |
|      1377 |  6640 | `	pBad->zTok = 0;` |
|      1377 |  6641 | `	pBad->cChar = 0;` |
|      1377 |  6642 | `	pBad->bExpect = 0;` |
|      1377 |  6643 | `	pBad->zExpect = 0;` |
|      1377 |  6644 | `	pBad->nLine = 0;` |
|      1377 |  6645 | `	SyBlobReset(pOut);` |
|      2299 |  6646 | `	while( nVal > 0 && (zVal[0] == ' ' \|\| zVal[0] == '\t') ){` |
|       236 |  6647 | `		zVal++;` |
|       236 |  6648 | `		nVal--;` |
|       ! 0 |  6649 | `	}` |
|      2081 |  6650 | `	while( nVal > 0 && (zVal[nVal-1] == ' ' \|\| zVal[nVal-1] == '\t') ){` |
|        18 |  6651 | `		nVal--;` |
|       ! 0 |  6652 | `	}` |
|      1377 |  6653 | `	if( nVal == 0 ){` |
|         4 |  6654 | ``		return 1;   /* `-d name=` carries the empty value, not an expression */`` |
|         - |  6655 | `	}` |
|         - |  6656 | `	/* A boolean word is its own token, taken whenever it is the longest match` |
|         - |  6657 | `	 * AT THE FRONT of the value -- i.e. whenever the byte right behind it is` |
|         - |  6658 | `` 	 * not one php's scanner would fold into the same run. `onx`/`ontology` `` |
|         - |  6659 | `	 * are not "on" at all (VALUE_CHARS+ outruns the word there and reads the` |
|         - |  6660 | `` 	 * whole run as one unresolved identifier instead), while `On\|E_NOTICE` `` |
|         - |  6661 | `	 * IS "on" followed by a token the boolean production cannot take -- the` |
|         - |  6662 | `	 * word still commits, the '\|' is a separate, later syntax error. */` |
|     11937 |  6663 | `	for( i = 0 ; i < SX_ARRAYSIZE(aVmIniBool) ; i++ ){` |
|         - |  6664 | `		int c;` |
|     10620 |  6665 | `		if( nVal < aVmIniBool[i].nWord` |
|      6850 |  6666 | `		 \|\| SyStrnicmp(zVal,aVmIniBool[i].zWord,aVmIniBool[i].nWord) != 0 ){` |
|     10566 |  6667 | `			continue;` |
|         - |  6668 | `		}` |
|        61 |  6669 | `		c = nVal == aVmIniBool[i].nWord ? -1 : (unsigned char)zVal[aVmIniBool[i].nWord];` |
|        58 |  6670 | `		if( c < 0 \|\| VmIniExprIsOp(c) \|\| c == '(' \|\| c == ')' \|\| c == '~'` |
|        20 |  6671 | `		 \|\| c == '!' \|\| c == '"' \|\| c == '\'' \|\| c == '$' \|\| c == ';'` |
|        21 |  6672 | `		 \|\| c == ' ' \|\| c == '\t' ){` |
|        59 |  6673 | `			if( c > 0 ){` |
|         - |  6674 | `				/* The word production eats the blanks behind it, so what php` |
|         - |  6675 | `				 * scans next starts at the first non-blank. */` |
|        26 |  6676 | `				const char *zRest = &zVal[aVmIniBool[i].nWord];` |
|        26 |  6677 | `				const char *zStop = &zVal[nVal];` |
|        54 |  6678 | `				while( zRest < zStop && (zRest[0] == ' ' \|\| zRest[0] == '\t') ){` |
|        16 |  6679 | `					zRest++;` |
|       ! 0 |  6680 | `				}` |
|        26 |  6681 | `				VmIniBadToken(zRest,zStop,pBad);` |
|        12 |  6682 | `			}` |
|        59 |  6683 | `			SyBlobAppend(pOut,aVmIniBool[i].zText,(sxu32)SyStrlen(aVmIniBool[i].zText));` |
|        59 |  6684 | `			return 1;` |
|         - |  6685 | `		}` |
|         2 |  6686 | `	}` |
|      1317 |  6687 | `	sIn.pVm = pVm;` |
|      1317 |  6688 | `	sIn.zCur = zVal;` |
|      1317 |  6689 | `	sIn.zEnd = &zVal[nVal];` |
|      1317 |  6690 | `	sIn.nLine = 0;` |
|      1317 |  6691 | `	sIn.bRawEof = 0;` |
|      1317 |  6692 | `	sIn.bStop = 0;` |
|      1317 |  6693 | `	sIn.cStop = 0;` |
|      1317 |  6694 | `	sIn.bExpect = 0;` |
|      1317 |  6695 | `	sIn.zTok = 0;` |
|      1317 |  6696 | `	sIn.zExpect = 0;` |
|      1317 |  6697 | `	if( !VmIniExprEval(&sIn,pOut,0) ){` |
|       110 |  6698 | `		pBad->bSet = 1;` |
|       110 |  6699 | `		pBad->zTok = sIn.zTok;` |
|       110 |  6700 | `		pBad->cChar = sIn.cStop;` |
|       110 |  6701 | `		pBad->bExpect = sIn.bExpect;` |
|       110 |  6702 | `		pBad->zExpect = sIn.zExpect;` |
|       110 |  6703 | `		pBad->nLine = sIn.nLine;` |
|       110 |  6704 | `		return 0;` |
|         - |  6705 | `	}` |
|         - |  6706 | ``	/* Whatever is left -- `)`, `1&&2`'s second `&`, an unresolved `~`, and`` |
|         - |  6707 | ``	 * after a closing paren anything at all (`(1)x`, `(1)0`, `(1)'a'`) -- is a`` |
|         - |  6708 | `	 * separate token the grammar cannot take from here, but the expr already` |
|         - |  6709 | `	 * reduced and its value already stands. */` |
|      1207 |  6710 | `	VmIniBadToken(sIn.zCur,sIn.zEnd,pBad);` |
|      1207 |  6711 | `	pBad->nLine = sIn.nLine;` |
|      1207 |  6712 | `	return 1;` |
|       690 |  6713 | `}` |
|         - |  6714 | `/*` |
|         - |  6715 | `` * php's own unbuffered ini-parser warning: `PHP:  syntax error, unexpected`` |
|         - |  6716 | `` * '<c>' in <file> on line <N>\n`, written straight to the engine's error`` |
|         - |  6717 | ` * consumer with none of error_reporting/display_errors/log_errors in the` |
|         - |  6718 | ` * way. That is php's own rule (zend_ini_parser.c's ini_error(), the` |
|         - |  6719 | ` * ini_parser_unbuffered_errors branch): those three knobs are not` |
|         - |  6720 | ` * trustworthy gates here because the refusal may be setting one of them.` |
|         - |  6721 | ` * Silent when the host never wired PH7_CONFIG_ERR_OUTPUT, or never supplied` |
|         - |  6722 | ` * a file for this entry (an embedder that does not pass one gets nothing,` |
|         - |  6723 | ` * same as it gets nothing from any other early diagnostic).` |
|         - |  6724 | ` */` |
|       360 |  6725 | `static void VmIniSyntaxWarning(ph7_vm *pVm,SyString *pFile,sxu32 nLine,int iStop,const VmIniBad *pBad)` |
|         3 |  6726 | `{` |
|         - |  6727 | `	SyBlob sMsg;` |
|       363 |  6728 | `	ProcConsumer xErr = pVm->pEngine->xConf.xErr;` |
|       363 |  6729 | `	if( xErr == 0 \|\| pFile == 0 \|\| pFile->nByte == 0 ){` |
|       ! 0 |  6730 | `		return;` |
|         - |  6731 | `	}` |
|       363 |  6732 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|         - |  6733 | `	/* A value is one scanner input: a double-quoted run inside it may have` |
|         - |  6734 | `	 * carried php's line counter past several newlines before the stop. */` |
|       363 |  6735 | `	nLine += pBad->nLine;` |
|       363 |  6736 | `	SyBlobAppend(&sMsg,"PHP:  syntax error, unexpected ",sizeof("PHP:  syntax error, unexpected ")-1);` |
|       363 |  6737 | `	if( pBad->zTok ){` |
|         - |  6738 | `		/* A token php's grammar declares under a symbol of its own prints as` |
|         - |  6739 | `		 * that symbol, with no quotes around it. */` |
|       220 |  6740 | `		SyBlobAppend(&sMsg,pBad->zTok,(sxu32)SyStrlen(pBad->zTok));` |
|       253 |  6741 | `	}else if( pBad->cChar != 0 ){` |
|        79 |  6742 | `		SyBlobFormat(&sMsg,"'%c'",pBad->cChar);` |
|       102 |  6743 | `	}else if( iStop == PH7_INI_STOP_COMMENT ){` |
|         - |  6744 | ``		/* A value that ran out inside a `;` comment no newline ever closed: the`` |
|         - |  6745 | `		 * comment rule wants that newline, so nothing matches at all and what` |
|         - |  6746 | `		 * the parser is handed is the end of the input rather than a line's end.` |
|         - |  6747 | `		 * It stays on the directive's own line. */` |
|         6 |  6748 | `		SyBlobAppend(&sMsg,"end of file",sizeof("end of file")-1);` |
|         3 |  6749 | `	}else{` |
|         - |  6750 | `		/* A value that ran out: php's scanner has already taken the newline, so` |
|         - |  6751 | `		 * its token is END_OF_LINE and it is dated to the line after the one the` |
|         - |  6752 | `		 * directive was written on -- but only when there WAS a newline to take.` |
|         - |  6753 | `		 * The last line of a source that ends without one is still refused under` |
|         - |  6754 | `		 * END_OF_LINE, and stays where it was written. */` |
|        58 |  6755 | `		SyBlobAppend(&sMsg,"END_OF_LINE",sizeof("END_OF_LINE")-1);` |
|        58 |  6756 | `		if( iStop == PH7_INI_STOP_EOL ){` |
|        42 |  6757 | `			nLine++;` |
|        21 |  6758 | `		}` |
|         - |  6759 | `	}` |
|       363 |  6760 | `	if( pBad->bExpect ){` |
|        12 |  6761 | `		SyBlobAppend(&sMsg,", expecting '^' or '\|' or '&' or ')'",` |
|         - |  6762 | `			sizeof(", expecting '^' or '\|' or '&' or ')'")-1);` |
|       357 |  6763 | `	}else if( pBad->zExpect ){` |
|       130 |  6764 | `		SyBlobFormat(&sMsg,", expecting %s",pBad->zExpect);` |
|        65 |  6765 | `	}` |
|       363 |  6766 | `	SyBlobFormat(&sMsg," in %.*s on line %u\n",` |
|       360 |  6767 | `		(int)pFile->nByte,pFile->zString,(unsigned)nLine);` |
|       363 |  6768 | `	xErr(SyBlobData(&sMsg),SyBlobLength(&sMsg),pVm->pEngine->xConf.pErrData);` |
|       363 |  6769 | `	SyBlobRelease(&sMsg);` |
|       183 |  6770 | `}` |
|         - |  6771 | `/*` |
|         - |  6772 | ` * Apply one php.ini directive to a VM: queue an allocator-owned copy for the INI` |
|         - |  6773 | ` * chunk's lazy seed, then arm the C-side knobs that have to hold whether or not` |
|         - |  6774 | ` * the script ever touches the INI API. Shared by PH7_VM_CONFIG_INI_ENTRY, which` |
|         - |  6775 | ` * hands a directive to a VM that already exists, and by the engine-level replay a` |
|         - |  6776 | ` * fresh VM runs before it compiles anything (PH7_VmApplyEngineIni). zFile/nLine` |
|         - |  6777 | ` * locate the directive for VmIniSyntaxWarning; an empty zFile leaves a refusal` |
|         - |  6778 | ` * silent, matching an embedder that never supplied one.` |
|         - |  6779 | ` */` |
|      2972 |  6780 | `static sxi32 VmSetIniEntry(ph7_vm *pVm,const char *zName,const char *zValue,` |
|         - |  6781 | `	const char *zFile,sxu32 nLine,int iStop,int *pbBad)` |
|         4 |  6782 | `{` |
|      2976 |  6783 | `	sxi32 rc = SXRET_OK;` |
|         - |  6784 | `	VmIniEntry sEntry;` |
|         - |  6785 | `	SyBlob sEval;` |
|         - |  6786 | `	SyString sFile;` |
|      2976 |  6787 | `	int bLevel = 0;` |
|         - |  6788 | `	VmIniBad sBad;` |
|         - |  6789 | `	char *zDupN,*zDupV;` |
|         - |  6790 | `	sxu32 nName,nValue;` |
|      2976 |  6791 | `	if( pbBad ){` |
|      2976 |  6792 | `		*pbBad = 0;` |
|      1485 |  6793 | `	}` |
|      2976 |  6794 | `	if( iStop == PH7_INI_STOP_STMT ){` |
|         - |  6795 | `		/* Not a directive either: the text a host's scanner found where php` |
|         - |  6796 | `		 * reads a directive NAME. php reads a name out of most of it and` |
|         - |  6797 | `		 * needs nothing done -- its php.ini callback ignores a statement` |
|         - |  6798 | `		 * carrying no value, and one carrying a value arrives on its own --` |
|         - |  6799 | `		 * but where its scanner hands the parser a token instead, the source` |
|         - |  6800 | `		 * is refused from here down. The table that decides is the ini value` |
|         - |  6801 | `		 * grammar's own, one file away from here rather than copied into` |
|         - |  6802 | `		 * every host that walks a source. */` |
|         - |  6803 | `		char zTok[8];` |
|      1485 |  6804 | `		const char *zBad = VmIniStmtToken(zName,(sxu32)SyStrlen(zName),zTok);` |
|      1485 |  6805 | `		if( zBad == 0 ){` |
|      1427 |  6806 | `			return SXRET_OK;` |
|         - |  6807 | `		}` |
|        58 |  6808 | `		SyZero(&sBad,sizeof(sBad));` |
|        58 |  6809 | `		sBad.zTok = zBad;` |
|        58 |  6810 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|        58 |  6811 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|        58 |  6812 | `		if( pbBad ){` |
|        58 |  6813 | `			*pbBad = 1;` |
|        29 |  6814 | `		}` |
|        58 |  6815 | `		return SXRET_OK;` |
|         - |  6816 | `	}` |
|      1495 |  6817 | `	if( iStop == PH7_INI_STOP_OFFSET \|\| iStop == PH7_INI_STOP_OFFSET_EOF ){` |
|         - |  6818 | ``		/* Not a directive: an offset statement whose `]` was not followed by`` |
|         - |  6819 | ``		 * the `=` its grammar demands. php refuses the source from here down`` |
|         - |  6820 | `		 * the way any other statement-position token does, and appends the one` |
|         - |  6821 | `		 * thing it could have taken. */` |
|         - |  6822 | `		char zTok[8];` |
|        24 |  6823 | `		const char *zBad = VmIniOffsetToken(zName,(sxu32)SyStrlen(zName),zTok);` |
|        24 |  6824 | `		SyZero(&sBad,sizeof(sBad));` |
|        24 |  6825 | `		if( zBad ){` |
|        14 |  6826 | `			if( zBad == zTok ){` |
|         6 |  6827 | `				sBad.cChar = (unsigned char)zTok[1];` |
|         3 |  6828 | `			}else{` |
|         8 |  6829 | `				sBad.zTok = zBad;` |
|       ! 0 |  6830 | `			}` |
|        17 |  6831 | `		}else if( iStop == PH7_INI_STOP_OFFSET_EOF ){` |
|         - |  6832 | `			/* nothing left to read at all: the newline rule the END_OF_LINE` |
|         - |  6833 | `			 * comes from needs a newline, and there is none */` |
|         2 |  6834 | `			sBad.zTok = "end of file";` |
|         1 |  6835 | `		}else{` |
|         - |  6836 | `			/* END_OF_LINE, and php's newline rule has already counted it */` |
|         8 |  6837 | `			sBad.nLine = 1;` |
|         - |  6838 | `		}` |
|        24 |  6839 | `		sBad.zExpect = "'='";` |
|        24 |  6840 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|        24 |  6841 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|        24 |  6842 | `		if( pbBad ){` |
|        24 |  6843 | `			*pbBad = 1;` |
|        12 |  6844 | `		}` |
|        24 |  6845 | `		return SXRET_OK;` |
|         - |  6846 | `	}` |
|      1471 |  6847 | `	if( iStop == PH7_INI_STOP_SECTION_VAR ){` |
|         - |  6848 | ``		/* A `${` run written inside a section name or an offset. php's`` |
|         - |  6849 | `		 * ST_VARNAME and ST_VAR_FALLBACK are pushed from its section, offset` |
|         - |  6850 | `		 * and value states alike, so the run is read by the value grammar's` |
|         - |  6851 | `		 * own variable rule and refused under the same tokens -- a source that` |
|         - |  6852 | ``		 * writes `[${A:-x}]` is refused for the one-letter name php's scanner`` |
|         - |  6853 | ``		 * loses to its fallback rule, exactly as `p = ${A:-x}` is. Where php`` |
|         - |  6854 | `		 * reads the substitution there is nothing to say and nothing to store:` |
|         - |  6855 | `		 * the host walks a section header for its extent, not for a directive. */` |
|         - |  6856 | `		VmIniExpr sVar;` |
|         - |  6857 | `		SyBlob sOut;` |
|         - |  6858 | `		int bRead;` |
|        52 |  6859 | `		nName = (sxu32)SyStrlen(zName);` |
|        52 |  6860 | `		if( nName < 2 ){` |
|       ! 0 |  6861 | `			return SXRET_OK;` |
|         - |  6862 | `		}` |
|        52 |  6863 | `		SyZero(&sVar,sizeof(sVar));` |
|        52 |  6864 | `		sVar.pVm = pVm;` |
|        52 |  6865 | `		sVar.zCur = zName;` |
|        52 |  6866 | `		sVar.zEnd = &zName[nName];` |
|        52 |  6867 | `		SyBlobInit(&sOut,&pVm->sAllocator);` |
|        52 |  6868 | `		bRead = VmIniExprVar(&sVar,&sOut,0);` |
|        52 |  6869 | `		SyBlobRelease(&sOut);` |
|        52 |  6870 | `		if( bRead ){` |
|        16 |  6871 | `			return SXRET_OK;` |
|         - |  6872 | `		}` |
|        36 |  6873 | `		SyZero(&sBad,sizeof(sBad));` |
|        36 |  6874 | `		sBad.zTok = sVar.zTok;` |
|        36 |  6875 | `		sBad.cChar = sVar.cStop;` |
|        36 |  6876 | `		sBad.bExpect = sVar.bExpect;` |
|        36 |  6877 | `		sBad.zExpect = sVar.zExpect;` |
|        36 |  6878 | `		sBad.nLine = sVar.nLine;` |
|        36 |  6879 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|        36 |  6880 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|        36 |  6881 | `		if( pbBad ){` |
|        36 |  6882 | `			*pbBad = 1;` |
|        18 |  6883 | `		}` |
|        36 |  6884 | `		return SXRET_OK;` |
|         - |  6885 | `	}` |
|      1419 |  6886 | `	if( iStop >= PH7_INI_STOP_SECTION ){` |
|         - |  6887 | ``		/* Not a directive: a `[` the host's scanner never found a `]` for. php's`` |
|         - |  6888 | ``		 * section name is one scanner run that ends at the `]` -- a newline is`` |
|         - |  6889 | `		 * not an end, so the run reaches the end of the FILE and refuses it` |
|         - |  6890 | `		 * there, dated to where the run stopped counting. The directives above` |
|         - |  6891 | `		 * it stand, and everything below is inside a section header that never` |
|         - |  6892 | `		 * closed, so nothing below is ever seen. */` |
|        42 |  6893 | `		SyZero(&sBad,sizeof(sBad));` |
|        42 |  6894 | `		sBad.zTok = "end of file";` |
|        42 |  6895 | `		sBad.zExpect = iStop == PH7_INI_STOP_SECTION_STR` |
|         - |  6896 | `			? "TC_DOLLAR_CURLY or TC_QUOTED_STRING or '\"'"` |
|        21 |  6897 | `			: "']'";` |
|        42 |  6898 | `		SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|        42 |  6899 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|        42 |  6900 | `		if( pbBad ){` |
|        42 |  6901 | `			*pbBad = 1;` |
|        21 |  6902 | `		}` |
|        42 |  6903 | `		return SXRET_OK;` |
|         - |  6904 | `	}` |
|      1377 |  6905 | `	if( SX_EMPTY_STR(zName) ){` |
|       ! 0 |  6906 | `		return SXERR_EMPTY;` |
|         - |  6907 | `	}` |
|      1377 |  6908 | `	if( zValue == 0 ){` |
|       ! 0 |  6909 | `		zValue = "";` |
|       ! 0 |  6910 | `	}` |
|      1377 |  6911 | `	SyStringInitFromBuf(&sFile,zFile,zFile ? SyStrlen(zFile) : 0);` |
|      1377 |  6912 | `	nName = (sxu32)SyStrlen(zName);` |
|      1377 |  6913 | `	nValue = (sxu32)SyStrlen(zValue);` |
|      1377 |  6914 | `	SyBlobInit(&sEval,&pVm->sAllocator);` |
|      1447 |  6915 | `	bLevel = nName == sizeof("error_reporting")-1` |
|      1373 |  6916 | `	      && SyMemcmp(zName,"error_reporting",nName) == 0;` |
|         - |  6917 | `	/* php stores what its ini parser MADE of the value, not what was typed, and` |
|         - |  6918 | `	 * it runs that parser over EVERY directive: ini_get() shows "30711" for` |
|         - |  6919 | ``	 * `E_ALL & ~E_NOTICE` and "1" for `On` whatever the name in front of it, and`` |
|         - |  6920 | `	 * ini_restore() then has that text to re-apply rather than an expression the` |
|         - |  6921 | `	 * runtime setter would read as 0. */` |
|      1377 |  6922 | `	if( !VmIniEvalValue(pVm,zValue,nValue,&sEval,&sBad) ){` |
|         - |  6923 | `		/* php's ini parser calls this a syntax error, and a refused directive` |
|         - |  6924 | `		 * never lands at all -- its default stands, rather than the raw text or` |
|         - |  6925 | `		 * a zero standing in for it. It is still an error it REPORTS, and one` |
|         - |  6926 | ``		 * that stops the rest of the source: `precision = 1 & )` leaves`` |
|         - |  6927 | `		 * precision alone, warns, and takes every later line down with it. */` |
|       110 |  6928 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|       110 |  6929 | `		if( pbBad ){` |
|       110 |  6930 | `			*pbBad = 1;` |
|        55 |  6931 | `		}` |
|       110 |  6932 | `		SyBlobRelease(&sEval);` |
|       110 |  6933 | `		return SXRET_OK;` |
|         - |  6934 | `	}` |
|      1267 |  6935 | `	if( sBad.bSet ){` |
|        93 |  6936 | `		VmIniSyntaxWarning(pVm,&sFile,nLine,iStop,&sBad);` |
|        93 |  6937 | `		if( pbBad ){` |
|        93 |  6938 | `			*pbBad = 1;` |
|        45 |  6939 | `		}` |
|        45 |  6940 | `	}` |
|      1267 |  6941 | `	nValue = SyBlobLength(&sEval);` |
|      1267 |  6942 | `	zValue = nValue > 0 ? (const char *)SyBlobData(&sEval) : "";` |
|      1267 |  6943 | `	zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|      1267 |  6944 | `	zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|      1267 |  6945 | `	SyBlobRelease(&sEval);` |
|         - |  6946 | `	/* From here on the evaluated TEXT is the allocator's copy: the blob it was` |
|         - |  6947 | `	 * built in is gone, and every knob armed below reads this value. */` |
|      1267 |  6948 | `	zValue = zDupV ? zDupV : "";` |
|      1267 |  6949 | `	if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 |  6950 | `		return SXERR_MEM;` |
|         - |  6951 | `	}` |
|      1267 |  6952 | `	SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|      1267 |  6953 | `	SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|      1267 |  6954 | `	sEntry.sFile.zString = 0;` |
|      1267 |  6955 | `	sEntry.sFile.nByte = 0;` |
|      1267 |  6956 | `	sEntry.nLine = 0;` |
|      1267 |  6957 | `	rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|      1267 |  6958 | `	if( rc == SXRET_OK ){` |
|      1267 |  6959 | `		if( bLevel ){` |
|         - |  6960 | `			/* The LEVEL, not an on/off gate. It used to move bErrReport alone, so` |
|         - |  6961 | ``			 * `-d error_reporting=2` left the mask at E_ALL and printed every`` |
|         - |  6962 | `			 * severity it was set to hide. The text read here is the grammar's,` |
|         - |  6963 | `			 * so it is always a number: a value php's ini parser refuses never` |
|         - |  6964 | `			 * reaches this far, and reading one as 0 silenced the whole run over` |
|         - |  6965 | `			 * one stray operator. */` |
|       102 |  6966 | `			sxi64 iLevel = 0;` |
|       102 |  6967 | `			if( nValue > 0 ){` |
|       102 |  6968 | `				SyStrToInt64(zDupV,nValue,(void *)&iLevel,0);` |
|        49 |  6969 | `			}` |
|       102 |  6970 | `			pVm->iErrMask = (sxi32)iLevel;` |
|       102 |  6971 | `			pVm->bErrReport = pVm->iErrMask != 0;` |
|       102 |  6972 | `			pVm->bErrMaskSet = 1;` |
|         - |  6973 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      1218 |  6974 | `		}else if( nName == sizeof("memory_limit")-1` |
|       596 |  6975 | `		 && SyMemcmp(zName,"memory_limit",nName) == 0 ){` |
|         - |  6976 | ``			/* Arm the allocator ceiling now: `-d memory_limit=32M` has to hold`` |
|         - |  6977 | `			 * for the whole run, and the INI chunk that would otherwise carry it` |
|         - |  6978 | `			 * is seeded lazily -- by which time a runaway script has already` |
|         - |  6979 | `			 * taken the box.` |
|         - |  6980 | `			 *` |
|         - |  6981 | `			 * Guarded because the applier lives in vm_builtin_ini.c, which the` |
|         - |  6982 | `			 * tiny build compiles away wholesale: an unguarded call here links` |
|         - |  6983 | ``			 * fine in `full` and fails ONLY in tiny, which is the one build the`` |
|         - |  6984 | `			 * ASan and Windows gates do not cover. */` |
|         4 |  6985 | `			PH7_VmApplyMemoryLimit(&(*pVm),zValue,nValue);` |
|         - |  6986 | `#endif` |
|      1163 |  6987 | `		}else if( nName == sizeof("date.timezone")-1` |
|       596 |  6988 | `		 && SyMemcmp(zName,"date.timezone",nName) == 0 ){` |
|         - |  6989 | ``			/* `-d date.timezone=...` takes what the directive takes: a`` |
|         - |  6990 | `			 * tz-database identifier, or UTC/GMT when there is no database.` |
|         - |  6991 | `			 * Anything else leaves the default at UTC, silently, which is` |
|         - |  6992 | `			 * php's answer for an ini value it cannot resolve at startup. */` |
|         - |  6993 | `#ifdef PH7_ENABLE_TZDB` |
|         6 |  6994 | `			if( nValue > 0 && (sxu32)nValue < sizeof(pVm->zDefTz)` |
|         8 |  6995 | `			 && PH7_TzFind(zValue,nValue) >= 0 ){` |
|         8 |  6996 | `				SyMemcpy(zValue,pVm->zDefTz,(sxu32)nValue);` |
|         8 |  6997 | `				pVm->zDefTz[nValue] = 0;` |
|         8 |  6998 | `				pVm->nDefTz = (sxu32)nValue;` |
|         5 |  6999 | `			}else` |
|         - |  7000 | `#endif` |
|       ! 0 |  7001 | `			if( nValue == 3` |
|       ! 0 |  7002 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 |  7003 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 |  7004 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 |  7005 | `				pVm->nDefTz = 3;` |
|       ! 0 |  7006 | `			}` |
|      1160 |  7007 | `		}else if( nName == sizeof("zend.assertions")-1` |
|       620 |  7008 | `		 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - |  7009 | `			/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - |  7010 | `			 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - |  7011 | `			 * effect even before the INI chunk is seeded. */` |
|        40 |  7012 | `			sxi64 iZend = 0;` |
|        40 |  7013 | `			SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 |  7014 | `			if( iZend >= 1 ){` |
|        40 |  7015 | `				pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 |  7016 | `			}else{` |
|       ! 0 |  7017 | `				pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - |  7018 | `			}` |
|      1141 |  7019 | `		}else if( nName == sizeof("display_errors")-1` |
|       681 |  7020 | `		 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - |  7021 | `			/* Mirror the display_errors DESTINATION C-side so it takes effect` |
|         - |  7022 | `			 * even if the script never touches the INI API (ini_set keeps it in` |
|         - |  7023 | `			 * sync at runtime via __ini_apply_err). */` |
|       240 |  7024 | `			pVm->iDisplayErrors = PH7_VmDisplayErrorsMode(zValue,nValue);` |
|      1005 |  7025 | `		}else if( nName == sizeof("log_errors")-1` |
|       673 |  7026 | `		 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|       234 |  7027 | `			pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|       772 |  7028 | `		}else if( nName == sizeof("error_log")-1` |
|       429 |  7029 | `		 && SyMemcmp(zName,"error_log",nName) == 0 ){` |
|         - |  7030 | `			/* The destination has to reach the VM here and not only through the` |
|         - |  7031 | ``			 * INI chunk, whose seed is lazy: `-d error_log=…` is set precisely so`` |
|         - |  7032 | `			 * that the diagnostics of a run that never touches the INI API land` |
|         - |  7033 | `			 * in the file. Empty clears it back to the SAPI logger, which is what` |
|         - |  7034 | `			 * php's unset destination means. */` |
|        68 |  7035 | `			SyBlobReset(&pVm->sErrLogPath);` |
|        68 |  7036 | `			if( nValue > 0 ){` |
|        64 |  7037 | `				SyBlobAppend(&pVm->sErrLogPath,zValue,nValue);` |
|        32 |  7038 | `			}` |
|        68 |  7039 | `			SyBlobNullAppend(&pVm->sErrLogPath);` |
|       619 |  7040 | `		}else if( nName == sizeof("include_path")-1` |
|       300 |  7041 | `		 && SyMemcmp(zName,"include_path",nName) == 0` |
|        16 |  7042 | `		 && nValue > 0 ){` |
|         - |  7043 | `			/* The path SET is the store this directive names, and the INI` |
|         - |  7044 | ``			 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|         - |  7045 | `			 * here or a script that never touches the INI API keeps looking` |
|         - |  7046 | `			 * in the default directory. Empty is refused, as php's` |
|         - |  7047 | `			 * OnUpdateStringUnempty refuses it. */` |
|        10 |  7048 | `			PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|         4 |  7049 | `		}` |
|       631 |  7050 | `	}` |
|      1267 |  7051 | `	return rc;` |
|      1489 |  7052 | `}` |
|         - |  7053 | `/*` |
|         - |  7054 | ` * Seed a brand-new VM from the ENGINE's configuration: the reporting level, then` |
|         - |  7055 | ` * the php.ini directives the host handed over with PH7_CONFIG_INI_ENTRY. Called` |
|         - |  7056 | ` * before the unit is compiled, because a compile diagnostic owes the same three` |
|         - |  7057 | ` * gates a runtime one does and ph7_compile_file is what creates the VM -- a` |
|         - |  7058 | ` * directive that only ever reached the FINISHED VM arrived after every diagnostic` |
|         - |  7059 | ` * the unit's own compile could raise. The ini replay runs last so` |
|         - |  7060 | `` * `-d error_reporting=0` still lowers what PH7_CONFIG_ERR_REPORT raised.`` |
|         - |  7061 | ` */` |
|      8445 |  7062 | `PH7_PRIVATE void PH7_VmApplyEngineIni(ph7_vm *pVm)` |
|         5 |  7063 | `{` |
|      8450 |  7064 | `	ph7_conf *pConf = &pVm->pEngine->xConf;` |
|         - |  7065 | `	VmIniEntry *aEntry;` |
|         - |  7066 | `	SyString sSkip;` |
|         - |  7067 | `	sxu32 i;` |
|      8450 |  7068 | `	if( pConf->bErrReport ){` |
|      8420 |  7069 | `		pVm->bErrReport = 1;` |
|      8420 |  7070 | `		pVm->iErrMask = PH7_E_ALL_MASK;` |
|      8420 |  7071 | `		pVm->bErrMaskSet = 1;` |
|      4202 |  7072 | `	}` |
|         - |  7073 | `	/* php hands each ini SOURCE to its parser whole -- the php.ini file is one` |
|         - |  7074 | `	 * parse and the CLI's joined -d buffer is another -- so a syntax error does` |
|         - |  7075 | `	 * not just drop its own directive: bison stops, and every directive still to` |
|         - |  7076 | `	 * come in that source is never seen. A bad php.ini line therefore takes the` |
|         - |  7077 | `	 * lines under it down while leaving -d alone, and a bad -d takes the -d's` |
|         - |  7078 | `	 * behind it while leaving php.ini alone. The queue here is per directive, so` |
|         - |  7079 | `	 * the source is the file it came from; an entry with no file is an embedder` |
|         - |  7080 | `	 * handing over one directive at a time rather than a parsed source, and` |
|         - |  7081 | `	 * nothing follows it into the skip. */` |
|      8450 |  7082 | `	sSkip.zString = 0;` |
|      8450 |  7083 | `	sSkip.nByte = 0;` |
|      8450 |  7084 | `	aEntry = (VmIniEntry *)SySetBasePtr(&pConf->aIniEntry);` |
|     11802 |  7085 | `	for( i = 0 ; i < SySetUsed(&pConf->aIniEntry) ; ++i ){` |
|      3356 |  7086 | `		int bBad = 0;` |
|      3352 |  7087 | `		if( sSkip.nByte > 0 && aEntry[i].sFile.nByte == sSkip.nByte` |
|       386 |  7088 | `		 && SyMemcmp(aEntry[i].sFile.zString,sSkip.zString,sSkip.nByte) == 0 ){` |
|       380 |  7089 | `			continue;` |
|         - |  7090 | `		}` |
|      4461 |  7091 | `		VmSetIniEntry(pVm,aEntry[i].sName.zString,aEntry[i].sValue.zString,` |
|      2972 |  7092 | `			aEntry[i].sFile.zString,aEntry[i].nLine,aEntry[i].iStop,&bBad);` |
|      2976 |  7093 | `		if( bBad && aEntry[i].sFile.nByte > 0 ){` |
|       363 |  7094 | `			sSkip = aEntry[i].sFile;` |
|       180 |  7095 | `		}` |
|      1489 |  7096 | `	}` |
|      8450 |  7097 | `}` |
|         - |  7098 | `/*` |
|         - |  7099 | ` * Configure a working virtual machine instance.` |
|         - |  7100 | ` *` |
|         - |  7101 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - |  7102 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - |  7103 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - |  7104 | ` * The second argument to this function is an integer configuration option` |
|         - |  7105 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - |  7106 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - |  7107 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - |  7108 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - |  7109 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - |  7110 | ` */` |
|    237754 |  7111 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - |  7112 | `	ph7_vm *pVm, /* Target VM */` |
|         - |  7113 | `	sxi32 nOp,   /* Configuration verb */` |
|         - |  7114 | `	va_list ap   /* Subsequent option arguments */` |
|         - |  7115 | `	)` |
|         5 |  7116 | `{` |
|    237759 |  7117 | `	sxi32 rc = SXRET_OK;` |
|    237759 |  7118 | `	switch(nOp){` |
|      3417 |  7119 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      6828 |  7120 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      6828 |  7121 | `		void *pUserData = va_arg(ap,void *);` |
|         - |  7122 | `		/* VM output consumer callback */` |
|         - |  7123 | `#ifdef UNTRUST` |
|         - |  7124 | `		if( xConsumer == 0 ){` |
|         - |  7125 | `			rc = SXERR_CORRUPT;` |
|         - |  7126 | `			break;` |
|         - |  7127 | `		}` |
|         - |  7128 | `#endif` |
|         - |  7129 | `		/* Install the output consumer */` |
|      6828 |  7130 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      6828 |  7131 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      6828 |  7132 | `		break;` |
|         - |  7133 | `							   }` |
|      3417 |  7134 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      6828 |  7135 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      6828 |  7136 | `		void *pUserData = va_arg(ap,void *);` |
|         - |  7137 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - |  7138 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - |  7139 | `		 * instead of the program-output stream. */` |
|         - |  7140 | `#ifdef UNTRUST` |
|         - |  7141 | `		if( xConsumer == 0 ){` |
|         - |  7142 | `			rc = SXERR_CORRUPT;` |
|         - |  7143 | `			break;` |
|         - |  7144 | `		}` |
|         - |  7145 | `#endif` |
|      6828 |  7146 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      6828 |  7147 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      6828 |  7148 | `		break;` |
|         - |  7149 | `								   }` |
|      3491 |  7150 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - |  7151 | `		/* Import path */` |
|         - |  7152 | `		  const char *zPath;` |
|         - |  7153 | `		  SyString sPath;` |
|      6976 |  7154 | `		  zPath = va_arg(ap,const char *);` |
|         - |  7155 | `#if defined(UNTRUST)` |
|         - |  7156 | `		  if( zPath == 0 ){` |
|         - |  7157 | `			  rc = SXERR_EMPTY;` |
|         - |  7158 | `			  break;` |
|         - |  7159 | `		  }` |
|         - |  7160 | `#endif` |
|      6976 |  7161 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - |  7162 | `		  /* Remove trailing slashes and backslashes */` |
|         - |  7163 | `#ifdef __WINNT__` |
|         5 |  7164 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - |  7165 | `#endif` |
|     13947 |  7166 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - |  7167 | `		  /* Remove leading and trailing white spaces */` |
|      6976 |  7168 | `		  SyStringFullTrim(&sPath);` |
|      6976 |  7169 | `		  if( sPath.nByte > 0 ){` |
|         - |  7170 | `			  /* Store the path in the corresponding conatiner */` |
|      6976 |  7171 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      3480 |  7172 | `		  }` |
|      6976 |  7173 | `		  break;` |
|         - |  7174 | `									 }` |
|        23 |  7175 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - |  7176 | `		/* Run-Time Error report */` |
|        46 |  7177 | `		pVm->bErrReport = 1;` |
|        46 |  7178 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|        46 |  7179 | `		pVm->bErrMaskSet = 1;` |
|        46 |  7180 | `		break;` |
|         2 |  7181 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - |  7182 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - |  7183 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - |  7184 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - |  7185 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - |  7186 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - |  7187 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - |  7188 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - |  7189 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 |  7190 | `		int nDepth = va_arg(ap,int);` |
|         5 |  7191 | `		if( nDepth >= 0 ){` |
|         5 |  7192 | `			pVm->nMaxDepth = nDepth;` |
|         2 |  7193 | `		}` |
|         5 |  7194 | `		break;` |
|         - |  7195 | `									   }` |
|         5 |  7196 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - |  7197 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - |  7198 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - |  7199 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - |  7200 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - |  7201 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - |  7202 | `		 * so it is rejected as a footgun). */` |
|        11 |  7203 | `		int nDepth = va_arg(ap,int);` |
|        11 |  7204 | `		if( nDepth > 1 ){` |
|        11 |  7205 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 |  7206 | `		}` |
|        11 |  7207 | `		break;` |
|         - |  7208 | `									   }` |
|       ! 0 |  7209 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - |  7210 | `		/* VM output length in bytes */` |
|       ! 0 |  7211 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - |  7212 | `#ifdef UNTRUST` |
|         - |  7213 | `		if( pOut == 0 ){` |
|         - |  7214 | `			rc = SXERR_CORRUPT;` |
|         - |  7215 | `			break;` |
|         - |  7216 | `		}` |
|         - |  7217 | `#endif` |
|       ! 0 |  7218 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 |  7219 | `		break;` |
|         - |  7220 | `							   }` |
|         - |  7221 |  |
|     38477 |  7222 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - |  7223 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - |  7224 | `		/* Create a new superglobal/global variable */` |
|     76838 |  7225 | `		const char *zName = va_arg(ap,const char *);` |
|     76838 |  7226 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - |  7227 | `		SyHashEntry *pEntry;` |
|         - |  7228 | `		ph7_value *pObj;` |
|         - |  7229 | `		sxu32 nByte;` |
|         - |  7230 | `		sxu32 nIdx;` |
|         - |  7231 | `#ifdef UNTRUST` |
|         - |  7232 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - |  7233 | `			rc = SXERR_CORRUPT;` |
|         - |  7234 | `			break;` |
|         - |  7235 | `		}` |
|         - |  7236 | `#endif` |
|     76838 |  7237 | `		nByte = SyStrlen(zName);` |
|     76838 |  7238 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - |  7239 | `			/* Check if the superglobal is already installed */` |
|     70015 |  7240 | `			pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|     34955 |  7241 | `		}else{` |
|         - |  7242 | `			/* Query the top active VM frame */` |
|      6828 |  7243 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - |  7244 | `		}` |
|     76838 |  7245 | `		if( pEntry ){` |
|         - |  7246 | `			/* Variable already installed */` |
|       ! 0 |  7247 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - |  7248 | `			/* Extract contents */` |
|       ! 0 |  7249 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       ! 0 |  7250 | `			if( pObj ){` |
|         - |  7251 | `				/* Overwrite old contents */` |
|       ! 0 |  7252 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 |  7253 | `			}` |
|       ! 0 |  7254 | `		}else{` |
|         - |  7255 | `			/* Install a new variable */` |
|     76838 |  7256 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     76838 |  7257 | `			if( pObj == 0 ){` |
|       ! 0 |  7258 | `				rc = SXERR_MEM;` |
|       ! 0 |  7259 | `				break;` |
|         - |  7260 | `			}` |
|     76838 |  7261 | `			nIdx = pObj->nIdx;` |
|         - |  7262 | `			/* Copy value */` |
|     76838 |  7263 | `			PH7_MemObjStore(pValue,pObj);` |
|     76838 |  7264 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - |  7265 | `				/* Install the superglobal */` |
|     70015 |  7266 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     70015 |  7267 | `				if( rc == SXRET_OK ){` |
|     70015 |  7268 | `					PH7_VmSuperNote(&(*pVm),zName,nByte);` |
|     34950 |  7269 | `				}` |
|     34955 |  7270 | `			}else{` |
|         - |  7271 | `				/* Install in the current frame */` |
|      6828 |  7272 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - |  7273 | `			}` |
|     76838 |  7274 | `			if( rc == SXRET_OK ){` |
|         - |  7275 | `				SyHashEntry *pRef;` |
|     76838 |  7276 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     70015 |  7277 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     34955 |  7278 | `				}else{` |
|      6828 |  7279 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - |  7280 | `				}` |
|         - |  7281 | `				/* Install in the reference table */` |
|     76838 |  7282 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     76838 |  7283 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - |  7284 | `					/* Register in the $GLOBALS array */` |
|     76838 |  7285 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     38356 |  7286 | `				}` |
|     38356 |  7287 | `			}` |
|         - |  7288 | `		}` |
|     76838 |  7289 | `		break;` |
|         - |  7290 | `									}` |
|     20959 |  7291 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - |  7292 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - |  7293 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - |  7294 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - |  7295 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - |  7296 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - |  7297 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     41857 |  7298 | `		const char *zKey   = va_arg(ap,const char *);` |
|     41857 |  7299 | `		const char *zValue = va_arg(ap,const char *);` |
|     41857 |  7300 | `		int nLen = va_arg(ap,int);` |
|         - |  7301 | `		ph7_hashmap *pMap;` |
|         - |  7302 | `		ph7_value *pValue;` |
|     41857 |  7303 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - |  7304 | `			/* Extract the $_ENV superglobal */` |
|       ! 0 |  7305 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     41857 |  7306 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - |  7307 | `			/* Extract the $_POST superglobal */` |
|       ! 0 |  7308 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     41857 |  7309 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - |  7310 | `			/* Extract the $_GET superglobal */` |
|       ! 0 |  7311 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     41857 |  7312 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - |  7313 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 |  7314 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     41857 |  7315 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - |  7316 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 |  7317 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     41857 |  7318 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - |  7319 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 |  7320 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 |  7321 | `		}else{` |
|         - |  7322 | `			/* Extract the $_SERVER superglobal */` |
|     41857 |  7323 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - |  7324 | `		}` |
|     41857 |  7325 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  7326 | `			/* No such entry */` |
|       ! 0 |  7327 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 |  7328 | `			break;` |
|         - |  7329 | `		}` |
|         - |  7330 | `		/* Point to the hashmap */` |
|     41857 |  7331 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - |  7332 | `		/* Perform the insertion */` |
|     41857 |  7333 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     41857 |  7334 | `		break;` |
|         - |  7335 | `								   }` |
|      3455 |  7336 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - |  7337 | `		/* Script arguments */` |
|      6904 |  7338 | `		const char *zValue = va_arg(ap,const char *);` |
|         - |  7339 | `		ph7_hashmap *pMap;` |
|         - |  7340 | `		ph7_value *pValue;` |
|         - |  7341 | `		sxu32 n;` |
|         - |  7342 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - |  7343 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - |  7344 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - |  7345 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      6904 |  7346 | `		if( zValue == 0 ){` |
|       ! 0 |  7347 | `			rc = SXERR_EMPTY;` |
|       ! 0 |  7348 | `			break;` |
|         - |  7349 | `		}` |
|         - |  7350 | `		/* Extract the $argv array */` |
|      6904 |  7351 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      6904 |  7352 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - |  7353 | `			/* No such entry */` |
|       ! 0 |  7354 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 |  7355 | `			break;` |
|         - |  7356 | `		}` |
|         - |  7357 | `		/* Point to the hashmap */` |
|      6904 |  7358 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - |  7359 | `		/* Perform the insertion */` |
|      6904 |  7360 | `		n = (sxu32)SyStrlen(zValue);` |
|      6904 |  7361 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      6904 |  7362 | `		break;` |
|         - |  7363 | `								  }` |
|      3417 |  7364 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - |  7365 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - |  7366 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - |  7367 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - |  7368 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - |  7369 | `		ph7_value *pArgv,*pServer;` |
|         - |  7370 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - |  7371 | `		ph7_value sArgvVal,sKey,sCount;` |
|      6828 |  7372 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      6828 |  7373 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      6823 |  7374 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      6828 |  7375 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 |  7376 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 |  7377 | `			break;` |
|         - |  7378 | `		}` |
|      6828 |  7379 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      6828 |  7380 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - |  7381 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      6828 |  7382 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      6828 |  7383 | `		if( pDup == 0 ){` |
|       ! 0 |  7384 | `			rc = SXERR_MEM;` |
|       ! 0 |  7385 | `			break;` |
|         - |  7386 | `		}` |
|      6828 |  7387 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      6828 |  7388 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      6828 |  7389 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      6828 |  7390 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      6828 |  7391 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      6828 |  7392 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      6828 |  7393 | `		PH7_MemObjRelease(&sKey);` |
|         - |  7394 | `		/* $_SERVER['argc'] = count($argv). */` |
|      6828 |  7395 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      6828 |  7396 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      6828 |  7397 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      6828 |  7398 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      6828 |  7399 | `		PH7_MemObjRelease(&sCount);` |
|      6828 |  7400 | `		PH7_MemObjRelease(&sKey);` |
|      6828 |  7401 | `		rc = SXRET_OK;` |
|      6828 |  7402 | `		break;` |
|         - |  7403 | `								  }` |
|       ! 0 |  7404 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - |  7405 | `		/* A php.ini directive handed to a VM that already exists (the CLI's -d/-c` |
|         - |  7406 | `		 * queue reaches a fresh one through PH7_CONFIG_INI_ENTRY instead). */` |
|       ! 0 |  7407 | `		const char *zName = va_arg(ap,const char *);` |
|       ! 0 |  7408 | `		const char *zValue = va_arg(ap,const char *);` |
|       ! 0 |  7409 | `		const char *zFile = va_arg(ap,const char *);` |
|       ! 0 |  7410 | `		unsigned int nLine = va_arg(ap,unsigned int);` |
|       ! 0 |  7411 | `		int iStop = va_arg(ap,int);` |
|       ! 0 |  7412 | `		rc = VmSetIniEntry(pVm,zName,zValue,zFile,(sxu32)nLine,iStop,0);` |
|       ! 0 |  7413 | `		break;` |
|         - |  7414 | `								  }` |
|       ! 0 |  7415 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - |  7416 | `		/* error_log() consumer */` |
|       ! 0 |  7417 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 |  7418 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 |  7419 | `		break;` |
|         - |  7420 | `										}` |
|       ! 0 |  7421 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - |  7422 | `		/* Script return value */` |
|       ! 0 |  7423 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - |  7424 | `#ifdef UNTRUST` |
|         - |  7425 | `		if( ppValue == 0 ){` |
|         - |  7426 | `			rc = SXERR_CORRUPT;` |
|         - |  7427 | `			break;` |
|         - |  7428 | `		}` |
|         - |  7429 | `#endif` |
|       ! 0 |  7430 | `		*ppValue = &pVm->sExec;` |
|       ! 0 |  7431 | `		break;` |
|         - |  7432 | `								   }` |
|     42298 |  7433 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - |  7434 | `		/* Register an IO stream device */` |
|     84491 |  7435 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - |  7436 | `		/* Make sure we are dealing with a valid IO stream. A wrapper has to be` |
|         - |  7437 | `		 * able to do ONE of the two things a wrapper does -- open a byte stream` |
|         - |  7438 | `		 * or open a directory. php's glob:// is a dir_opener and nothing else,` |
|         - |  7439 | ``		 * and demanding xOpen here would leave `opendir('glob://…')` with no`` |
|         - |  7440 | `		 * device to reach. */` |
|     88708 |  7441 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     84486 |  7442 | `			((pStream->xOpen == 0 \|\| pStream->xRead == 0) && pStream->xOpenDir == 0) ){` |
|         - |  7443 | `				/* Invalid stream */` |
|       ! 0 |  7444 | `				rc = SXERR_INVALID;` |
|       ! 0 |  7445 | `				break;` |
|         - |  7446 | `		}` |
|     84491 |  7447 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - |  7448 | `			/* Make the 'file://' stream the defaut stream device */` |
|      8450 |  7449 | `			pVm->pDefStream = pStream;` |
|      4217 |  7450 | `		}` |
|         - |  7451 | `		/* Insert in the appropriate container */` |
|     84491 |  7452 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     84491 |  7453 | `		break;` |
|         - |  7454 | `								  }` |
|        23 |  7455 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - |  7456 | `		/* Point to the VM internal output consumer buffer */` |
|        46 |  7457 | `		const void **ppOut = va_arg(ap,const void **);` |
|        46 |  7458 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - |  7459 | `#ifdef UNTRUST` |
|         - |  7460 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - |  7461 | `			rc = SXERR_CORRUPT;` |
|         - |  7462 | `			break;` |
|         - |  7463 | `		}` |
|         - |  7464 | `#endif` |
|        46 |  7465 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        46 |  7466 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        46 |  7467 | `		break;` |
|         - |  7468 | `									   }` |
|        23 |  7469 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - |  7470 | `		/* Raw HTTP request*/` |
|        46 |  7471 | `		const char *zRequest = va_arg(ap,const char *);` |
|        46 |  7472 | `		int nByte = va_arg(ap,int);` |
|        46 |  7473 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 |  7474 | `			rc = SXERR_EMPTY;` |
|       ! 0 |  7475 | `			break;` |
|         - |  7476 | `		}` |
|        46 |  7477 | `		if( nByte < 0 ){` |
|         - |  7478 | `			/* Compute length automatically */` |
|       ! 0 |  7479 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 |  7480 | `		}` |
|         - |  7481 | `		/* Process the request */` |
|        46 |  7482 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - |  7483 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        46 |  7484 | `		if( rc == SXRET_OK ){` |
|        44 |  7485 | `			pVm->bHttpContext = 1;` |
|        44 |  7486 | `			if( pVm->iResponseStatus == 0 ){` |
|         - |  7487 | `				/* A request-driven run starts at 200, which is what` |
|         - |  7488 | `				 * http_response_code() reads back before anything sets one. */` |
|        44 |  7489 | `				pVm->iResponseStatus = 200;` |
|        22 |  7490 | `			}` |
|        22 |  7491 | `		}` |
|        46 |  7492 | `		break;` |
|         - |  7493 | `									}` |
|        23 |  7494 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - |  7495 | `		/* Extract HTTP response status code */` |
|        46 |  7496 | `		int *pStatus = va_arg(ap, int *);` |
|        46 |  7497 | `		if( pStatus ){` |
|         - |  7498 | `			/* A response nothing set a code for goes out as 200. */` |
|        46 |  7499 | `			*pStatus = pVm->iResponseStatus ? pVm->iResponseStatus : 200;` |
|        23 |  7500 | `		}` |
|        46 |  7501 | `		break;` |
|         - |  7502 | `										}` |
|        23 |  7503 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - |  7504 | `		/* Iterate response headers via callback */` |
|         - |  7505 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        46 |  7506 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        46 |  7507 | `		void *pUserData = va_arg(ap, void *);` |
|        46 |  7508 | `		if( xCallback ){` |
|        46 |  7509 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        46 |  7510 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       112 |  7511 | `			for( k = 0; k < nHdr; k++ ){` |
|        99 |  7512 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        66 |  7513 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        33 |  7514 | `							   pUserData);` |
|        66 |  7515 | `				if( rc != PH7_OK ){` |
|       ! 0 |  7516 | `					break;` |
|         - |  7517 | `				}` |
|        33 |  7518 | `			}` |
|        23 |  7519 | `		}` |
|        46 |  7520 | `		break;` |
|         - |  7521 | `										 }` |
|       ! 0 |  7522 | `	default:` |
|         - |  7523 | `		/* Unknown configuration option */` |
|       ! 0 |  7524 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 |  7525 | `		break;` |
|         - |  7526 | `	}` |
|    237759 |  7527 | `	return rc;` |
|         5 |  7528 | `}` |
|         - |  7529 | `/* Forward declaration */` |
|         - |  7530 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - |  7531 | `/*` |
|         - |  7532 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - |  7533 | ` * format.` |
|         - |  7534 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - |  7535 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - |  7536 | ` * (STDOUT).` |
|         - |  7537 | ` */` |
|         2 |  7538 | `static sxi32 VmByteCodeDump(` |
|         - |  7539 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - |  7540 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - |  7541 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - |  7542 | `	)` |
|         1 |  7543 | `{` |
|         - |  7544 | `	static const char zDump[] = {` |
|         - |  7545 | `		"====================================================\n"` |
|         - |  7546 | `		"PH7 VM Dump\n"` |
|         - |  7547 | `		"====================================================\n"` |
|         - |  7548 | `	};` |
|         - |  7549 | `	VmInstr *pInstr,*pEnd;` |
|         3 |  7550 | `	sxi32 rc = SXRET_OK;` |
|         - |  7551 | `	sxu32 n;` |
|         - |  7552 | `	/* Point to the PH7 instructions */` |
|         3 |  7553 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 |  7554 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 |  7555 | `	n = 0;` |
|         3 |  7556 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - |  7557 | `	/* Dump instructions */` |
|         6 |  7558 | `	for(;;){` |
|        13 |  7559 | `		if( pInstr >= pEnd ){` |
|         - |  7560 | `			/* No more instructions */` |
|         3 |  7561 | `			break;` |
|         - |  7562 | `		}` |
|         - |  7563 | `		/* Format and call the consumer callback */` |
|        16 |  7564 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 |  7565 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 |  7566 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 |  7567 | `		if( rc != SXRET_OK ){` |
|         - |  7568 | `			/* Consumer routine request an operation abort */` |
|       ! 0 |  7569 | `			return rc;` |
|         - |  7570 | `		}` |
|        11 |  7571 | `		++n;` |
|        11 |  7572 | `		pInstr++; /* Next instruction in the stream */` |
|         1 |  7573 | `	}` |
|         3 |  7574 | `	return rc;` |
|         2 |  7575 | `}` |
|         - |  7576 | `/*` |
|         - |  7577 | ` * Save the execution state of a fiber/generator context.` |
|         - |  7578 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - |  7579 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - |  7580 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - |  7581 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - |  7582 | ` * when VmByteCodeExec returns.` |
|         - |  7583 | ` */` |
|      1772 |  7584 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - |  7585 | `	ph7_vm *pVm,` |
|         - |  7586 | `	ph7_exec_ctx *pCtx,` |
|         - |  7587 | `	sxi32 pc,` |
|         - |  7588 | `	sxi32 nTos` |
|         - |  7589 | `	)` |
|         5 |  7590 | `{` |
|       886 |  7591 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1777 |  7592 | `	pCtx->pc = pc;` |
|      1777 |  7593 | `	pCtx->nTos = nTos;` |
|      1777 |  7594 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1777 |  7595 | `	return PH7_SUSPEND;` |
|         5 |  7596 | `}` |
|         - |  7597 | `/*` |
|         - |  7598 | ` * Resolve named-argument mapping.` |
|         - |  7599 | ` *` |
|         - |  7600 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - |  7601 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - |  7602 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - |  7603 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - |  7604 | ` * every formal parameter that received a value.` |
|         - |  7605 | ` *` |
|         - |  7606 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - |  7607 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - |  7608 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - |  7609 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - |  7610 | ` */` |
|       832 |  7611 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - |  7612 | `	ph7_vm *pVm,` |
|         - |  7613 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - |  7614 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - |  7615 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - |  7616 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - |  7617 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - |  7618 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - |  7619 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - |  7620 | `)` |
|         5 |  7621 | `{` |
|       837 |  7622 | `	sxi32 posIdx = 0;` |
|       837 |  7623 | `	sxi32 nNamedHigh = 0; /* php's num_args as the names left it: highest formal + 1 */` |
|         - |  7624 | `	sxu32 i;` |
|       837 |  7625 | `	int bSeenNamed = 0;` |
|       837 |  7626 | `	sxu32 nNamedRun = 0;  /* the written argument the last named slot came from */` |
|         - |  7627 | `	char zErrMsg[256];` |
|       837 |  7628 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      2571 |  7629 | `	for( i = 0; i < nActual; i++ ){` |
|      1739 |  7630 | `		aSlot[i] = -2;` |
|       872 |  7631 | `	}` |
|      2507 |  7632 | `	for( i = 0; i < nActual; i++ ){` |
|      2302 |  7633 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - |  7634 | `			/* Named argument — find formal by name */` |
|      1189 |  7635 | `			int found = 0;` |
|      1189 |  7636 | `			bSeenNamed = 1;` |
|      1189 |  7637 | `			nNamedRun = pMap->aRun ? pMap->aRun[i] : 0;` |
|         - |  7638 | `			sxu32 k;` |
|      1931 |  7639 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|      1560 |  7640 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|      1497 |  7641 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|      1424 |  7642 | `						pMap->aNames[i].zString,` |
|      2136 |  7643 | `						pMap->aNames[i].nByte) == 0 ){` |
|       823 |  7644 | `					if( aUsed[k] ){` |
|        43 |  7645 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - |  7646 | `							"Named parameter $%.*s overwrites previous argument",` |
|        26 |  7647 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        30 |  7648 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - |  7649 | `					}` |
|       797 |  7650 | `					aSlot[i] = (sxi32)k;` |
|       797 |  7651 | `					aUsed[k] = 1;` |
|       797 |  7652 | `					if( (sxi32)k >= nNamedHigh ){` |
|       667 |  7653 | `						nNamedHigh = (sxi32)k + 1;` |
|       331 |  7654 | `					}` |
|       797 |  7655 | `					found = 1;` |
|       797 |  7656 | `					break;` |
|         - |  7657 | `				}` |
|       376 |  7658 | `			}` |
|      1163 |  7659 | `			if( !found ){` |
|       371 |  7660 | `				if( iVariadicIdx >= 0 ){` |
|         - |  7661 | `					/* An extra the variadic collects is keyed by its name, so a second` |
|         - |  7662 | `					 * one under the same name would overwrite the first in the collected` |
|         - |  7663 | `					 * array: php refuses it with the same Error a named formal gets.` |
|         - |  7664 | `					 * Only a NAMED earlier extra can collide -- a positional overflow` |
|         - |  7665 | `					 * is keyed by index. */` |
|         - |  7666 | `					sxu32 j;` |
|      1497 |  7667 | `					for( j = 0; j < i; j++ ){` |
|      1154 |  7668 | `						if( aSlot[j] == -1 && j < pMap->nTotal` |
|      1010 |  7669 | `							&& pMap->aNames[j].nByte == pMap->aNames[i].nByte` |
|       636 |  7670 | `							&& SyMemcmp(pMap->aNames[j].zString,pMap->aNames[i].zString,` |
|       378 |  7671 | `								pMap->aNames[i].nByte) == 0 ){` |
|        19 |  7672 | `							SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - |  7673 | `								"Named parameter $%.*s overwrites previous argument",` |
|        12 |  7674 | `								(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        13 |  7675 | `							return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - |  7676 | `						}` |
|       576 |  7677 | `					}` |
|       343 |  7678 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       174 |  7679 | `				}else{` |
|        27 |  7680 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - |  7681 | `						"Unknown named parameter $%.*s",` |
|        16 |  7682 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        19 |  7683 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - |  7684 | `				}` |
|       169 |  7685 | `			}` |
|       570 |  7686 | `		}else{` |
|         - |  7687 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - |  7688 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - |  7689 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - |  7690 | `			 * can, so enforce PHP's rule at this shared choke point. php holds the` |
|         - |  7691 | ``			 * rule per UNPACK: `f(...['a'=>1], ...[2])` is legal, and its 2 binds`` |
|         - |  7692 | `			 * after the highest parameter a name filled (zend's num_args), so a` |
|         - |  7693 | `			 * hole a name jumped over stays a hole. */` |
|       553 |  7694 | `			if( bSeenNamed && (pMap->aRun == 0 \|\| pMap->aRun[i] == nNamedRun) ){` |
|         - |  7695 | `				/* php has two sentences for the one rule: the argument list an` |
|         - |  7696 | ``				 * UNPACK produced ends with ` during unpacking`, and the one`` |
|         - |  7697 | `				 * call_user_func_array() rebuilt from an array does not. */` |
|        10 |  7698 | `				if( pMap->bFromUnpack ){` |
|         5 |  7699 | `					return VmThrowNamedArgError(&(*pVm),` |
|         - |  7700 | `						"Cannot use positional argument after named argument during unpacking",` |
|         - |  7701 | `						sizeof("Cannot use positional argument after named argument during unpacking") - 1);` |
|         - |  7702 | `				}` |
|         6 |  7703 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - |  7704 | `					"Cannot use positional argument after named argument",` |
|         - |  7705 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - |  7706 | `			}` |
|       545 |  7707 | `			if( posIdx < nNamedHigh ){` |
|        21 |  7708 | `				posIdx = nNamedHigh;` |
|        10 |  7709 | `			}` |
|       545 |  7710 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|       183 |  7711 | `				if( aUsed[posIdx] ){` |
|       ! 0 |  7712 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - |  7713 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 |  7714 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 |  7715 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - |  7716 | `				}` |
|       183 |  7717 | `				aSlot[i] = posIdx;` |
|       183 |  7718 | `				aUsed[posIdx] = 1;` |
|       455 |  7719 | `			}else if( iVariadicIdx >= 0 ){` |
|       356 |  7720 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       176 |  7721 | `			}` |
|       545 |  7722 | `			posIdx++;` |
|         - |  7723 | `		}` |
|       840 |  7724 | `	}` |
|       775 |  7725 | `	return SXRET_OK;` |
|       421 |  7726 | `}` |
|         - |  7727 | `/*` |
|         - |  7728 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - |  7729 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - |  7730 | ` */` |
|      1783 |  7731 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         5 |  7732 | `{` |
|      1788 |  7733 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      1682 |  7734 | `		return 0;` |
|         - |  7735 | `	}` |
|       111 |  7736 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       849 |  7737 | `}` |
|         - |  7738 | `/*` |
|         - |  7739 | ` * Shared body of the two Traversable-spread steps. An ARRAY source can only hand` |
|         - |  7740 | ` * over the two key types an array holds, but an ITERATOR may answer key() with` |
|         - |  7741 | `` * anything at all, so php screens it: `Keys must be of type int\|string during`` |
|         - |  7742 | `` * {array,argument} unpacking` is an Error raised at the offending element, and it`` |
|         - |  7743 | ` * refuses a float (a WHOLE one included), a bool, a null and a resource as well as` |
|         - |  7744 | ` * the two containers — this is not the offset rule set, which folds all four.` |
|         - |  7745 | ` *` |
|         - |  7746 | ` * What survives the screen follows php's ordinary 8.1 unpack rules, which are the` |
|         - |  7747 | ` * ones an array source already gets: a key that stays a STRING is kept, a key that` |
|         - |  7748 | ` * FOLDS to an integer — a canonical numeric string like "7" among them — is` |
|         - |  7749 | ` * renumbered. PH7_HashmapKeyIsInt answers that fold, and asking it is what keeps` |
|         - |  7750 | `` * `yield "7" => v` off the integer key 7 the raw insert would have written.`` |
|         - |  7751 | ` *` |
|         - |  7752 | ` * On the ARGUMENT path the kept string key is what makes the element a NAMED` |
|         - |  7753 | ` * argument: VmSpreadCaptureRun reads the temp map's node keys, so binding, the` |
|         - |  7754 | ` * unknown-name Error and the duplicate-name Error all come for free. Before this,` |
|         - |  7755 | `` * both steps threw the key away — `[...$gen]` silently renumbered a key php`` |
|         - |  7756 | `` * refuses, and `f(...$gen)` passed a named argument positionally, which is a`` |
|         - |  7757 | ` * DIFFERENT parameter's value with no diagnostic at all.` |
|         - |  7758 | ` */` |
|       112 |  7759 | `static sxi32 VmSpreadKeyedStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue,` |
|         - |  7760 | `                               ph7_hashmap *pMap, int bArgs)` |
|         5 |  7761 | `{` |
|         - |  7762 | `	SyBlob sMsg;` |
|         - |  7763 | `	sxi32 rc;` |
|         - |  7764 | `	int bKeep;` |
|       112 |  7765 | `	if( (pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT)) == 0` |
|       103 |  7766 | `	 \|\| (pKey->iFlags & (MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) ){` |
|        35 |  7767 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        35 |  7768 | `		SyBlobFormat(&sMsg,"Keys must be of type int\|string during %s unpacking",` |
|        16 |  7769 | `			bArgs ? "argument" : "array");` |
|        35 |  7770 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        35 |  7771 | `		SyBlobRelease(&sMsg);` |
|        35 |  7772 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |  7773 | `	}` |
|         - |  7774 | `	/* PH7_HashmapKeyIsInt may cast pKey to a string to answer; pKey is the walk's` |
|         - |  7775 | `	 * own temporary, released the moment this step returns. */` |
|        85 |  7776 | `	bKeep = (pKey->iFlags & MEMOBJ_STRING) && !PH7_HashmapKeyIsInt(pKey);` |
|        85 |  7777 | `	if( bKeep && bArgs && PH7_HashmapLookup(pMap,pKey,0) == SXRET_OK ){` |
|         - |  7778 | `		/* Two elements under the same string key are two NAMED arguments with the` |
|         - |  7779 | `		 * same name, which php refuses. The array path lets the later one win (that` |
|         - |  7780 | `		 * IS php's array-unpack rule), but here the collision would silently drop an` |
|         - |  7781 | `		 * argument: the temp map keeps one element, so the callee would be handed a` |
|         - |  7782 | `		 * shorter list with no diagnostic. The message is the binder's own — a` |
|         - |  7783 | `		 * duplicate spread ACROSS two sources still reaches it there. */` |
|         6 |  7784 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|         6 |  7785 | `		SyBlobFormat(&sMsg,"Named parameter $%.*s overwrites previous argument",` |
|         4 |  7786 | `			(int)SyBlobLength(&pKey->sBlob),(const char *)SyBlobData(&pKey->sBlob));` |
|         6 |  7787 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|         6 |  7788 | `		SyBlobRelease(&sMsg);` |
|         6 |  7789 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - |  7790 | `	}` |
|        81 |  7791 | `	PH7_HashmapInsert(pMap, bKeep ? pKey : 0 /* auto-index */, pValue);` |
|        81 |  7792 | `	return SXRET_OK;` |
|        61 |  7793 | `}` |
|         - |  7794 | `/*` |
|         - |  7795 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - |  7796 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - |  7797 | ` * preserved (later wins), integer keys are renumbered.` |
|         - |  7798 | ` */` |
|        50 |  7799 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         3 |  7800 | `{` |
|        53 |  7801 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,0);` |
|         3 |  7802 | `}` |
|         - |  7803 | `/*` |
|         - |  7804 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - |  7805 | ` * collect the elements into a temp array, keeping a string key so the CALL` |
|         - |  7806 | ` * replays it as a named argument.` |
|         - |  7807 | ` */` |
|        62 |  7808 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         4 |  7809 | `{` |
|        66 |  7810 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,1);` |
|         4 |  7811 | `}` |
|         - |  7812 | `/*` |
|         - |  7813 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - |  7814 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - |  7815 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - |  7816 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - |  7817 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - |  7818 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - |  7819 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - |  7820 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - |  7821 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - |  7822 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - |  7823 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - |  7824 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - |  7825 | ` */` |
|         - |  7826 | `/*` |
|         - |  7827 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - |  7828 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - |  7829 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - |  7830 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - |  7831 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - |  7832 | ` */` |
|      1633 |  7833 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         5 |  7834 | `{` |
|         - |  7835 | `	VmSpreadRun sRun;` |
|         - |  7836 | `	ph7_hashmap_node *pNode;` |
|         - |  7837 | `	sxu32 i;` |
|      1638 |  7838 | `	sRun.pStart = pFirst;` |
|      1638 |  7839 | `	sRun.nCount = nCount;` |
|      1638 |  7840 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      1638 |  7841 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      1638 |  7842 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 |  7843 | `		return;` |
|         - |  7844 | `	}` |
|      1638 |  7845 | `	pNode = pMap->pFirst;` |
|      5243 |  7846 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - |  7847 | `		VmSpreadKey sKey;` |
|      3610 |  7848 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - |  7849 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - |  7850 | `			 * the source map's release before CALL replays them. */` |
|       239 |  7851 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       239 |  7852 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       239 |  7853 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|       121 |  7854 | `		}else{` |
|         - |  7855 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      3374 |  7856 | `			sKey.nOff = 0;` |
|      3374 |  7857 | `			sKey.nLen = 0;` |
|         - |  7858 | `		}` |
|      3610 |  7859 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      3610 |  7860 | `		pNode = pNode->pPrev; /* forward link */` |
|      1695 |  7861 | `	}` |
|       774 |  7862 | `}` |
|         - |  7863 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - |  7864 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        16 |  7865 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 |  7866 | `{` |
|        16 |  7867 | `	SySetReset(&pVm->aSpreadRun);` |
|        16 |  7868 | `	SySetReset(&pVm->aSpreadKey);` |
|        16 |  7869 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        16 |  7870 | `}` |
|         - |  7871 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - |  7872 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - |  7873 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - |  7874 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - |  7875 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - |  7876 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - |  7877 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - |  7878 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - |  7879 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - |  7880 | ` * slot — from being consumed by that nested call. */` |
|    278530 |  7881 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         5 |  7882 | `{` |
|    278535 |  7883 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|    278535 |  7884 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - |  7885 | `	VmSpreadRun *aRun;` |
|    278535 |  7886 | `	if( rStart >= nRun ){` |
|    276980 |  7887 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - |  7888 | `	}` |
|      1560 |  7889 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1560 |  7890 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      1560 |  7891 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      1560 |  7892 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       730 |  7893 | `	}` |
|      1560 |  7894 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|    139138 |  7895 | `}` |
|         - |  7896 | `/*` |
|         - |  7897 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - |  7898 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - |  7899 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - |  7900 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - |  7901 | ` *` |
|         - |  7902 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - |  7903 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - |  7904 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - |  7905 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - |  7906 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - |  7907 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - |  7908 | ` * they are counted only by that call. This replaces the old shared` |
|         - |  7909 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - |  7910 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - |  7911 | ` *` |
|         - |  7912 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - |  7913 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - |  7914 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - |  7915 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - |  7916 | ` */` |
|      3682 |  7917 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         5 |  7918 | `{` |
|      3687 |  7919 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - |  7920 | `	VmSpreadRun *aRun;` |
|      3687 |  7921 | `	ph7_value *pEnd = pTos;` |
|      3687 |  7922 | `	sxi32 nPos = iP1;` |
|      3687 |  7923 | `	sxi32 ri, extra = 0;` |
|      3687 |  7924 | `	if( nRun == 0 ){` |
|        19 |  7925 | `		pVm->nSpreadCallBase = 0;` |
|        19 |  7926 | `		return 0;` |
|         - |  7927 | `	}` |
|      3669 |  7928 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      3669 |  7929 | `	ri = (sxi32)nRun - 1;` |
|     10687 |  7930 | `	while( nPos > 0 ){` |
|      7023 |  7931 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - |  7932 | `			/* A non-empty unpack occupying nCount slots. */` |
|      3135 |  7933 | `			pEnd = aRun[ri].pStart;` |
|      3135 |  7934 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      3135 |  7935 | `			ri--;` |
|      5362 |  7936 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - |  7937 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       618 |  7938 | `			extra -= 1;` |
|       618 |  7939 | `			ri--;` |
|       311 |  7940 | `		}else{` |
|         - |  7941 | `			/* An ordinary single-slot argument. */` |
|      3278 |  7942 | `			pEnd--;` |
|         - |  7943 | `		}` |
|      7023 |  7944 | `		nPos--;` |
|         5 |  7945 | `	}` |
|         - |  7946 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - |  7947 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      3669 |  7948 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      3669 |  7949 | `	return extra;` |
|      1751 |  7950 | `}` |
|      1633 |  7951 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         5 |  7952 | `{` |
|      1638 |  7953 | `	ph7_value *pTos = *ppTos;` |
|      1638 |  7954 | `	sxu32 nEntry = pMap->nEntry;` |
|      1638 |  7955 | `	if( nEntry == 0 ){` |
|         - |  7956 | `		/* Nothing to unpack — remove the source from the stack */` |
|       262 |  7957 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       262 |  7958 | `		VmPopOperand(&pTos, 1);` |
|       133 |  7959 | `	}else{` |
|         - |  7960 | `		ph7_hashmap_node *pNode;` |
|         - |  7961 | `		ph7_value *pElem;` |
|         - |  7962 | `		sxu32 i;` |
|         - |  7963 | `		int bTemp;` |
|         - |  7964 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - |  7965 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - |  7966 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - |  7967 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - |  7968 | `		 * ordinary spread pays nothing for this. */` |
|      1375 |  7969 | `		if( bVarSource` |
|      1185 |  7970 | `		 && pMap != pVm->pGlobal` |
|       995 |  7971 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|      1000 |  7972 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - |  7973 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - |  7974 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - |  7975 | `			 * object. */` |
|      1000 |  7976 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|      1000 |  7977 | `			if( pSep ){` |
|      1000 |  7978 | `				pMap = pSep;` |
|      1000 |  7979 | `				nEntry = pMap->nEntry;` |
|       450 |  7980 | `			}` |
|       450 |  7981 | `		}` |
|      1380 |  7982 | `		pMap->iRef++;` |
|      1380 |  7983 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - |  7984 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - |  7985 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      1380 |  7986 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - |  7987 | `		/* Overwrite the source slot with the first element */` |
|      1380 |  7988 | `		pNode = pMap->pFirst;` |
|      1380 |  7989 | `		pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1380 |  7990 | `		PH7_MemObjRelease(pTos);` |
|      1380 |  7991 | `		if( pElem ){` |
|      1380 |  7992 | `			if( bTemp ){` |
|       370 |  7993 | `				PH7_MemObjStore(pElem, pTos);` |
|       187 |  7994 | `			}else{` |
|      1014 |  7995 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - |  7996 | `			}` |
|       640 |  7997 | `		}` |
|         - |  7998 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - |  7999 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - |  8000 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - |  8001 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - |  8002 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - |  8003 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - |  8004 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - |  8005 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|      1380 |  8006 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|      1380 |  8007 | `		if( !bVarSource \|\| bTemp ){` |
|       384 |  8008 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       190 |  8009 | `		}` |
|         - |  8010 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - |  8011 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      1380 |  8012 | `		pNode = pNode->pPrev;` |
|         - |  8013 | `		/* Push the remaining elements */` |
|      3610 |  8014 | `		for( i = 1; i < nEntry; i++ ){` |
|      2234 |  8015 | `			pTos++;` |
|      2234 |  8016 | `			PH7_MemObjInit(pVm, pTos);` |
|      2234 |  8017 | `			pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|      2234 |  8018 | `			if( pElem ){` |
|      2234 |  8019 | `				if( bTemp ){` |
|      1375 |  8020 | `					PH7_MemObjStore(pElem, pTos);` |
|       689 |  8021 | `				}else{` |
|       862 |  8022 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - |  8023 | `				}` |
|      1050 |  8024 | `			}` |
|      2234 |  8025 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      2234 |  8026 | `			if( !bVarSource \|\| bTemp ){` |
|      1380 |  8027 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       688 |  8028 | `			}` |
|      2234 |  8029 | `			pNode = pNode->pPrev;` |
|      1054 |  8030 | `		}` |
|      1380 |  8031 | `		PH7_HashmapUnref(pMap);` |
|         - |  8032 | `	}` |
|      1638 |  8033 | `	*ppTos = pTos;` |
|      1638 |  8034 | `}` |
|         - |  8035 | `/*` |
|         - |  8036 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - |  8037 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - |  8038 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - |  8039 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - |  8040 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - |  8041 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - |  8042 | ` *` |
|         - |  8043 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - |  8044 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - |  8045 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - |  8046 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - |  8047 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - |  8048 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - |  8049 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - |  8050 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - |  8051 | ` * method-name slot pop shifts pArg).` |
|         - |  8052 | ` *` |
|         - |  8053 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - |  8054 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - |  8055 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - |  8056 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - |  8057 | ` * which is after this call's synchronous named-arg resolution.` |
|         - |  8058 | ` */` |
|    277159 |  8059 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - |  8060 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         5 |  8061 | `{` |
|    277164 |  8062 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - |  8063 | `	VmSpreadRun *aRun;` |
|         - |  8064 | `	VmSpreadKey *aKey;` |
|         - |  8065 | `	const char *zKeyBase;` |
|    277164 |  8066 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|    277164 |  8067 | `	int bAnyNamed = 0;` |
|         - |  8068 | `	sxu32 ai, ci, ri, rStart;` |
|    277164 |  8069 | `	if( nRun == 0 ){` |
|         - |  8070 | `		/* No spread captured at all — the compile map is already aligned. */` |
|    275681 |  8071 | `		return 0;` |
|         - |  8072 | `	}` |
|      1488 |  8073 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1488 |  8074 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      1488 |  8075 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - |  8076 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - |  8077 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - |  8078 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - |  8079 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      1488 |  8080 | `	ri = pVm->nSpreadCallBase;` |
|      1488 |  8081 | `	rStart = ri;` |
|      1488 |  8082 | `	if( rStart >= nRun ){` |
|         - |  8083 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 |  8084 | `		return 0;` |
|         - |  8085 | `	}` |
|      1488 |  8086 | `	SySetReset(&pVm->aEffArgName);` |
|      1488 |  8087 | `	SySetReset(&pVm->aEffArgRun);` |
|      1488 |  8088 | `	ci = 0;` |
|      1488 |  8089 | `	ai = 0;` |
|      4100 |  8090 | `	while( ai < nActual ){` |
|         - |  8091 | `		SyString sName;` |
|         - |  8092 | `		sxu32 nRunId;` |
|      2617 |  8093 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - |  8094 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - |  8095 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      2629 |  8096 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 |  8097 | `			ci++; ri++;` |
|         1 |  8098 | `		}` |
|      2617 |  8099 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - |  8100 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - |  8101 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - |  8102 | `			 * run never desyncs the key stream. */` |
|      1376 |  8103 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      1376 |  8104 | `			nRunId = ci + 1;` |
|      4977 |  8105 | `			for( j = 0; j < K; j++ ){` |
|      3606 |  8106 | `				SyZero(&sName, sizeof(sName));` |
|      3606 |  8107 | `				if( aKey[ks + j].nLen > 0 ){` |
|       239 |  8108 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       239 |  8109 | `					bAnyNamed = 1;` |
|       118 |  8110 | `				}` |
|      3606 |  8111 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      3606 |  8112 | `				SySetPut(&pVm->aEffArgRun, (const void *)&nRunId);` |
|      1693 |  8113 | `			}` |
|      1376 |  8114 | `			ai += K;` |
|      1376 |  8115 | `			ci++; ri++;` |
|       643 |  8116 | `		}else{` |
|         - |  8117 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      1245 |  8118 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        38 |  8119 | `				sName = pCompile->aNames[ci];` |
|        38 |  8120 | `				bAnyNamed = 1;` |
|        18 |  8121 | `			}` |
|      1245 |  8122 | `			nRunId = ci + 1;` |
|      1245 |  8123 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1245 |  8124 | `			SySetPut(&pVm->aEffArgRun, (const void *)&nRunId);` |
|      1245 |  8125 | `			ai++;` |
|      1245 |  8126 | `			ci++;` |
|         - |  8127 | `		}` |
|         5 |  8128 | `	}` |
|         - |  8129 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - |  8130 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - |  8131 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - |  8132 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      1488 |  8133 | `	VmSpreadConsume(pVm);` |
|      1488 |  8134 | `	if( !bAnyNamed ){` |
|         - |  8135 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      1286 |  8136 | `		return 0;` |
|         - |  8137 | `	}` |
|       205 |  8138 | `	pEff->bHasNamed = 1;` |
|       205 |  8139 | ``	pEff->bFromUnpack = 1; /* picks php's ` during unpacking` refusal wording */`` |
|       205 |  8140 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       205 |  8141 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       205 |  8142 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - |  8143 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - |  8144 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - |  8145 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - |  8146 | `	 * is the caller's stack local. */` |
|       205 |  8147 | `	pEff->bArgShapes = 0;` |
|       205 |  8148 | `	pEff->nNonLvalMask = 0;` |
|       205 |  8149 | `	pEff->nTempCallMask = 0;` |
|       205 |  8150 | `	if( pCompile ){` |
|        51 |  8151 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        27 |  8152 | `	}else{` |
|       157 |  8153 | `		pEff->sAssertSrc.zString = 0;` |
|       157 |  8154 | `		pEff->sAssertSrc.nByte = 0;` |
|         - |  8155 | `	}` |
|       205 |  8156 | `	pEff->nTotal = nActual;` |
|       205 |  8157 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       205 |  8158 | `	pEff->aRun = (const sxu32 *)SySetBasePtr(&pVm->aEffArgRun);` |
|       205 |  8159 | `	return 1;` |
|    138500 |  8160 | `}` |
|         - |  8161 | `/*` |
|         - |  8162 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - |  8163 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - |  8164 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - |  8165 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - |  8166 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - |  8167 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - |  8168 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - |  8169 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - |  8170 | ` * pArg must be the site's FINAL argument base.` |
|         - |  8171 | ` */` |
|  21540551 |  8172 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - |  8173 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 |  8174 | `{` |
|  21540556 |  8175 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|  21540556 |  8176 | `	if( pInstr->iP2 == 0 ){` |
|  21263307 |  8177 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - |  8178 | `	}` |
|    277254 |  8179 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       205 |  8180 | `		return pStorage;` |
|         - |  8181 | `	}` |
|    277052 |  8182 | `	VmSpreadConsume(pVm);` |
|    277052 |  8183 | `	return pCompile;` |
|  10771661 |  8184 | `}` |
|         - |  8185 | `/*` |
|         - |  8186 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - |  8187 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - |  8188 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - |  8189 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - |  8190 | ` */` |
|        38 |  8191 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         4 |  8192 | `{` |
|        42 |  8193 | `	const char *zType = "unknown";` |
|         - |  8194 | `	char zMsg[64];` |
|        42 |  8195 | `	if( iFlags & MEMOBJ_STRING ){` |
|        11 |  8196 | `		zType = "string";` |
|        37 |  8197 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - |  8198 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - |  8199 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - |  8200 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 |  8201 | `		zType = "float";` |
|        33 |  8202 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        31 |  8203 | `		zType = "int";` |
|        17 |  8204 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 |  8205 | `		zType = "bool";` |
|         1 |  8206 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 |  8207 | `		zType = "object";` |
|       ! 0 |  8208 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 |  8209 | `		zType = "resource";` |
|       ! 0 |  8210 | `	}` |
|        42 |  8211 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        42 |  8212 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        42 |  8213 | `}` |
|         - |  8214 | `/*` |
|         - |  8215 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - |  8216 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - |  8217 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - |  8218 | ` * per context — see VmMemberCtxWantsValue.` |
|         - |  8219 | ` */` |
|    109500 |  8220 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 |  8221 | `{` |
|    109505 |  8222 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 |  8223 | `}` |
|         - |  8224 | `/*` |
|         - |  8225 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - |  8226 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - |  8227 | ` * isset() stops at the truth.` |
|         - |  8228 | ` */` |
|        56 |  8229 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 |  8230 | `{` |
|        59 |  8231 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 |  8232 | `}` |
|         - |  8233 | `/*` |
|         - |  8234 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - |  8235 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - |  8236 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - |  8237 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - |  8238 | ` * reads (other names / other instances) still dispatch.` |
|         - |  8239 | ` */` |
|      1578 |  8240 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 |  8241 | `{` |
|         - |  8242 | `	VmMagicGuard *aG;` |
|         - |  8243 | `	sxu32 nHash;` |
|         - |  8244 | `	sxu32 n;` |
|      1583 |  8245 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - |  8246 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - |  8247 | `		 * every hooked-property access consults the guard, often twice. */` |
|      1381 |  8248 | `		return FALSE;` |
|         - |  8249 | `	}` |
|       205 |  8250 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       205 |  8251 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       251 |  8252 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       205 |  8253 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       159 |  8254 | `			return TRUE;` |
|         - |  8255 | `		}` |
|        25 |  8256 | `	}` |
|        48 |  8257 | `	return FALSE;` |
|       794 |  8258 | `}` |
|       670 |  8259 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 |  8260 | `{` |
|         - |  8261 | `	VmMagicGuard sG;` |
|       675 |  8262 | `	sG.pThis = pThis;` |
|       675 |  8263 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       675 |  8264 | `	sG.cKind = cKind;` |
|       675 |  8265 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       675 |  8266 | `}` |
|       670 |  8267 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 |  8268 | `{` |
|       675 |  8269 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       675 |  8270 | `}` |
|         - |  8271 | `/*` |
|         - |  8272 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - |  8273 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - |  8274 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - |  8275 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - |  8276 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - |  8277 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - |  8278 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - |  8279 | ` * One-token lookahead only.` |
|         - |  8280 | ` */` |
|     43591 |  8281 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 |  8282 | `{` |
|     43596 |  8283 | `	switch( pNext->iOp ){` |
|      4996 |  8284 | `		case PH7_OP_STORE:` |
|      9997 |  8285 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|        30 |  8286 | `		case PH7_OP_STORE_REF:` |
|        62 |  8287 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|      1280 |  8288 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - |  8289 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - |  8290 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - |  8291 | `		case PH7_OP_CAT_STORE:` |
|         - |  8292 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - |  8293 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|      2565 |  8294 | `			return 1;` |
|     15489 |  8295 | `		default:` |
|     30984 |  8296 | `			return 0;` |
|         - |  8297 | `	}` |
|     21801 |  8298 | `}` |
|         - |  8299 | `/*` |
|         - |  8300 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - |  8301 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - |  8302 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - |  8303 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - |  8304 | ` * treat them as one.` |
|         - |  8305 | ` */` |
|     36779 |  8306 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 |  8307 | `{` |
|     55139 |  8308 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|     55152 |  8309 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 |  8310 | `}` |
|         - |  8311 | `/*` |
|         - |  8312 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - |  8313 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - |  8314 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - |  8315 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - |  8316 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - |  8317 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - |  8318 | ` */` |
|     37033 |  8319 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 |  8320 | `{` |
|     37038 |  8321 | `	switch( pNext->iOp ){` |
|        65 |  8322 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - |  8323 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - |  8324 | `		case PH7_OP_CAT_STORE:` |
|         - |  8325 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - |  8326 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       132 |  8327 | `			return 1;` |
|     18451 |  8328 | `		default:` |
|     36908 |  8329 | `			return 0;` |
|         - |  8330 | `	}` |
|     18522 |  8331 | `}` |
|         - |  8332 | `/*` |
|         - |  8333 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - |  8334 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - |  8335 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - |  8336 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - |  8337 | ` */` |
|       662 |  8338 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 |  8339 | `{` |
|       667 |  8340 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 |  8341 | `}` |
|         - |  8342 | `/*` |
|         - |  8343 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - |  8344 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - |  8345 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - |  8346 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - |  8347 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - |  8348 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - |  8349 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - |  8350 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - |  8351 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - |  8352 | ` * abort path; SXRET_OK otherwise.` |
|         - |  8353 | ` */` |
|        88 |  8354 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         3 |  8355 | `{` |
|         - |  8356 | `	char zHName[384];` |
|         - |  8357 | `	sxu32 nHName;` |
|         - |  8358 | `	ph7_class_method *pSetHook;` |
|        91 |  8359 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - |  8360 | `		/* get-only hooked property: php's read-only Error */` |
|         - |  8361 | `		SyBlob sErrMsg;` |
|         5 |  8362 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 |  8363 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 |  8364 | `			&pHThis->pClass->sDisp,&pHAttr->sName);` |
|         5 |  8365 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 |  8366 | `		return SXRET_OK;` |
|         - |  8367 | `	}` |
|        87 |  8368 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - |  8369 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 |  8370 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 |  8371 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 |  8372 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 |  8373 | `			return SXRET_OK;` |
|         - |  8374 | `		}` |
|       ! 0 |  8375 | `	}` |
|        87 |  8376 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        87 |  8377 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        87 |  8378 | `	if( pSetHook ){` |
|         - |  8379 | `		ph7_value sHookRet;` |
|         - |  8380 | `		ph7_value *apHArg[1];` |
|        87 |  8381 | `		apHArg[0] = pValue;` |
|        87 |  8382 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        87 |  8383 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        87 |  8384 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        87 |  8385 | `		VmMagicGuardPop(pVm);` |
|        84 |  8386 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        50 |  8387 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|        12 |  8388 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|        12 |  8389 | `			if( rcH == SXRET_OK ){` |
|        12 |  8390 | `				ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nBackIdx);` |
|        12 |  8391 | `				if( pBack ){` |
|        12 |  8392 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         7 |  8393 | `				}` |
|         5 |  8394 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 |  8395 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 |  8396 | `				return PH7_ABORT;` |
|         - |  8397 | `			}` |
|         - |  8398 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - |  8399 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - |  8400 | `			 * parked throw. */` |
|         5 |  8401 | `		}` |
|        87 |  8402 | `		PH7_MemObjRelease(&sHookRet);` |
|        42 |  8403 | `	}` |
|        87 |  8404 | `	return SXRET_OK;` |
|        47 |  8405 | `}` |
|         - |  8406 | `/*` |
|         - |  8407 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - |  8408 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - |  8409 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - |  8410 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - |  8411 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - |  8412 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - |  8413 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - |  8414 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - |  8415 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - |  8416 | ` */` |
|         - |  8417 | `/*` |
|         - |  8418 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - |  8419 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - |  8420 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - |  8421 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - |  8422 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - |  8423 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - |  8424 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - |  8425 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - |  8426 | ` * caller reads the raw slot then.` |
|         - |  8427 | ` */` |
|      1997 |  8428 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 |  8429 | `{` |
|      2002 |  8430 | `	ph7_vm *pVm = pThis->pVm;` |
|         - |  8431 | `	char zHName[384];` |
|         - |  8432 | `	sxu32 nHName;` |
|         - |  8433 | `	ph7_class_method *pGetHook;` |
|         - |  8434 | `	sxi32 rc;` |
|      1997 |  8435 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|      1113 |  8436 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       239 |  8437 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - |  8438 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - |  8439 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - |  8440 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - |  8441 | `		 * raw values whose output the routed throw then discards. */` |
|      1768 |  8442 | `		return SXERR_NOTFOUND;` |
|         - |  8443 | `	}` |
|       239 |  8444 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       239 |  8445 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       239 |  8446 | `	if( pGetHook == 0 ){` |
|       ! 0 |  8447 | `		return SXERR_NOTFOUND;` |
|         - |  8448 | `	}` |
|       239 |  8449 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       239 |  8450 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       239 |  8451 | `	VmMagicGuardPop(pVm);` |
|       239 |  8452 | `	return rc;` |
|      1001 |  8453 | `}` |
|         - |  8454 | `/*` |
|         - |  8455 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - |  8456 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - |  8457 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - |  8458 | ` */` |
|       158 |  8459 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         2 |  8460 | `{` |
|       160 |  8461 | `	ph7_value *pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       160 |  8462 | `	if( pScr ){` |
|       160 |  8463 | `		PH7_MemObjRelease(pScr);` |
|        79 |  8464 | `	}` |
|       160 |  8465 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       160 |  8466 | `}` |
|         - |  8467 | `/*` |
|         - |  8468 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - |  8469 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - |  8470 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - |  8471 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - |  8472 | ` * instance reference.` |
|         - |  8473 | ` */` |
|        24 |  8474 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 |  8475 | `{` |
|        26 |  8476 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        26 |  8477 | `	if( pEnt == 0 ){` |
|         5 |  8478 | `		return;` |
|         - |  8479 | `	}` |
|        21 |  8480 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        13 |  8481 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         6 |  8482 | `	}` |
|        21 |  8483 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - |  8484 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - |  8485 | `		 * property's backing store — this entry owns it. */` |
|         5 |  8486 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         2 |  8487 | `	}` |
|        21 |  8488 | `	SyBlobRelease(&pEnt->sName);` |
|        21 |  8489 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        21 |  8490 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        14 |  8491 | `}` |
|        92 |  8492 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         2 |  8493 | `{` |
|         - |  8494 | `	VmHookRmw sEnt;` |
|         - |  8495 | `	VmHookRmw *pEnt;` |
|         - |  8496 | `	ph7_value *pScr;` |
|         - |  8497 | `	ph7_value sVal;` |
|         - |  8498 | `	ph7_value sKey;` |
|        94 |  8499 | `	sxi32 rc = SXRET_OK;` |
|        94 |  8500 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        94 |  8501 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 |  8502 | `		return SXERR_NOTFOUND;` |
|         - |  8503 | `	}` |
|        94 |  8504 | `	sEnt = *pEnt;` |
|        94 |  8505 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - |  8506 | `	/* Copy the computed value out of the scratch slot, then free the slot:` |
|         - |  8507 | `	 * once it is back on the pool's free list the next reserve may hand it to` |
|         - |  8508 | `	 * someone else, so nothing may read the scratch index past this point. The` |
|         - |  8509 | `	 * DIM kind's KEY slot goes the same way, for the same reason. (The pool` |
|         - |  8510 | `	 * itself no longer MOVES -- P1 -- but a freed index is still a freed index.) */` |
|        94 |  8511 | `	PH7_MemObjInit(pVm,&sVal);` |
|        94 |  8512 | `	pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        94 |  8513 | `	if( pScr ){` |
|        94 |  8514 | `		PH7_MemObjStore(pScr,&sVal);` |
|        46 |  8515 | `	}` |
|        94 |  8516 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        94 |  8517 | `	sVal.nIdx = SXU32_HIGH;` |
|        94 |  8518 | `	PH7_MemObjInit(pVm,&sKey);` |
|        94 |  8519 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        52 |  8520 | `		ph7_value *pKeySlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        52 |  8521 | `		if( pKeySlot ){` |
|        52 |  8522 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        25 |  8523 | `		}` |
|        52 |  8524 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        52 |  8525 | `		sKey.nIdx = SXU32_HIGH;` |
|        25 |  8526 | `	}` |
|        94 |  8527 | `	if( pVm->nBoundaryRc == 0 ){` |
|        94 |  8528 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - |  8529 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - |  8530 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - |  8531 | `			SyString sPropName;` |
|        29 |  8532 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        29 |  8533 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        80 |  8534 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - |  8535 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - |  8536 | `			 * back through offsetSet($key, $value). */` |
|        52 |  8537 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - |  8538 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        52 |  8539 | `			if( pSet ){` |
|         - |  8540 | `				ph7_value *apArg[2];` |
|        43 |  8541 | `				apArg[0] = &sKey;` |
|        43 |  8542 | `				apArg[1] = &sVal;` |
|        43 |  8543 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        22 |  8544 | `			}else{` |
|         - |  8545 | `				/* A container that answers a READ and no ArrayAccess: its own` |
|         - |  8546 | `				 * dimension handler gets the computed value first -- php's` |
|         - |  8547 | `` 				 * SimpleXMLElement stores it, which is what makes `$x['a'] .= 'x'` `` |
|         - |  8548 | `				 * work there. A handler that stores nothing (DOMNodeList, PDORow)` |
|         - |  8549 | `				 * leaves the write, and php's read-then-write pair then ends in the` |
|         - |  8550 | ``				 * plain store's Error, so `$list[9] .= 'x'` says what`` |
|         - |  8551 | ``				 * `$list[9] = 'x'` says. Parked: this runs at an arithmetic op's`` |
|         - |  8552 | `				 * tail, not at a throw boundary. */` |
|         - |  8553 | `				PH7_NativeDimCtx sDim;` |
|        10 |  8554 | `				if( PH7_ClassNativeDimStore(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|         - |  8555 | `					&sKey,&sVal,&sDim) ){` |
|         6 |  8556 | `					if( sDim.zThrowClass ){` |
|         4 |  8557 | `						VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),sDim.zThrowClass,` |
|         2 |  8558 | `							sDim.zThrowMsg,(sxu32)SyStrlen(sDim.zThrowMsg)));` |
|         1 |  8559 | `					}` |
|         4 |  8560 | `				}else{` |
|         - |  8561 | `					char zMsg[256];` |
|         7 |  8562 | `					sxu32 nMsg = PH7_ClassNativeDimRefusal(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|         2 |  8563 | `						zMsg,sizeof(zMsg));` |
|         5 |  8564 | `					VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg));` |
|         - |  8565 | `				}` |
|         - |  8566 | `			}` |
|        27 |  8567 | `		}else{` |
|        15 |  8568 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - |  8569 | `		}` |
|        46 |  8570 | `	}` |
|        94 |  8571 | `	SyBlobRelease(&sEnt.sName);` |
|        94 |  8572 | `	PH7_MemObjRelease(&sKey);` |
|        94 |  8573 | `	PH7_MemObjRelease(&sVal);` |
|        94 |  8574 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        94 |  8575 | `	return rc;` |
|        48 |  8576 | `}` |
|         - |  8577 | `/*` |
|         - |  8578 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - |  8579 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - |  8580 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - |  8581 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - |  8582 | ` */` |
|      1154 |  8583 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         5 |  8584 | `{` |
|         - |  8585 | `	ph7_class_method *pSetMeth;` |
|      1159 |  8586 | `	if( PH7_ClassNativePropOwns(pSetThis,pName) ){` |
|         - |  8587 | `		/* php's write_property handler for a name the class's own table carries:` |
|         - |  8588 | ``		 * it answers BEFORE the standard path, so a subclass's `__set` never sees`` |
|         - |  8589 | `		 * a DOM property and the handler's refusal is the one a program catches.` |
|         - |  8590 | `		 * Every overloaded write funnels through here -- the plain store, the` |
|         - |  8591 | ``		 * compound assign's write-back, the `??=` and Reflection -- so this is the`` |
|         - |  8592 | `		 * one door the handler needs. The refusal is PARKED: this runs at an` |
|         - |  8593 | `		 * opcode's tail rather than at a throw boundary. */` |
|         - |  8594 | `		PH7_NativePropCtx sNat;` |
|      1102 |  8595 | `		if( PH7_ClassNativePropAsk(pSetThis,&sNat,PH7_NATIVE_PROP_STORE,pName,pValue)` |
|      1107 |  8596 | `		 && sNat.zThrowClass ){` |
|       301 |  8597 | `			VmBoundaryPark(&(*pVm),VmThrowFixedErrorCode(&(*pVm),sNat.zThrowClass,` |
|        99 |  8598 | `				sNat.iThrowCode,sNat.zThrowMsg));` |
|        99 |  8599 | `		}` |
|      1107 |  8600 | `		return;` |
|         - |  8601 | `	}` |
|        55 |  8602 | `	pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        55 |  8603 | `	if( pSetMeth ){` |
|         - |  8604 | `		ph7_value sNameVal;` |
|         - |  8605 | `		ph7_value *apSetArg[2];` |
|        55 |  8606 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        55 |  8607 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        55 |  8608 | `		apSetArg[0] = &sNameVal;` |
|        55 |  8609 | `		apSetArg[1] = pValue;` |
|        55 |  8610 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        55 |  8611 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        55 |  8612 | `		VmMagicGuardPop(pVm);` |
|        55 |  8613 | `		PH7_MemObjRelease(&sNameVal);` |
|        26 |  8614 | `	}` |
|       582 |  8615 | `}` |
|         - |  8616 | `/*` |
|         - |  8617 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - |  8618 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - |  8619 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - |  8620 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - |  8621 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - |  8622 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - |  8623 | ` * path (the SyHash-layout incident class).` |
|         - |  8624 | ` */` |
|         - |  8625 | `/*` |
|         - |  8626 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - |  8627 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - |  8628 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - |  8629 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - |  8630 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - |  8631 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - |  8632 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - |  8633 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - |  8634 | ` * never linked (INIT error path).` |
|         - |  8635 | ` */` |
|     48153 |  8636 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 |  8637 | `{` |
|     48158 |  8638 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     48158 |  8639 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - |  8640 | `	sxu32 i;` |
|         - |  8641 | `	/* Drop the owning activation's claim on it first: the frame list is what` |
|         - |  8642 | `	 * guarantees a step cannot outlive the frame that made it, so it has to be` |
|         - |  8643 | `	 * left in step with aStep by the same door. A step that never made it onto` |
|         - |  8644 | `	 * aStep never made it onto the frame list either (INIT links both together,` |
|         - |  8645 | `	 * after the SySetPut), so the walk below simply finds nothing. */` |
|     48158 |  8646 | `	if( pStep->pFrame ){` |
|     48158 |  8647 | `		ph7_foreach_step **ppLink = &pStep->pFrame->pForeachSteps;` |
|     48184 |  8648 | `		while( *ppLink ){` |
|     47816 |  8649 | `			if( *ppLink == pStep ){` |
|     47790 |  8650 | `				*ppLink = pStep->pNextFrameStep;` |
|     47790 |  8651 | `				break;` |
|         - |  8652 | `			}` |
|        29 |  8653 | `			ppLink = &(*ppLink)->pNextFrameStep;` |
|         3 |  8654 | `		}` |
|     48158 |  8655 | `		pStep->pNextFrameStep = 0;` |
|     24042 |  8656 | `	}` |
|     51342 |  8657 | `	for( i = 0 ; i < n ; ++i ){` |
|     51342 |  8658 | `		if( apStep[i] == pStep ){` |
|     48170 |  8659 | `			for( ; i + 1 < n ; ++i ){` |
|        13 |  8660 | `				apStep[i] = apStep[i + 1];` |
|         7 |  8661 | `			}` |
|     48158 |  8662 | `			(void)SySetPop(&pInfo->aStep);` |
|     48158 |  8663 | `			return;` |
|         - |  8664 | `		}` |
|      1597 |  8665 | `	}` |
|     24047 |  8666 | `}` |
|         - |  8667 | `/*` |
|         - |  8668 | ` * End every foreach walk this activation still owns, because the activation is` |
|         - |  8669 | ` * about to die.` |
|         - |  8670 | ` *` |
|         - |  8671 | `` * A loop left through `break`, `return`, `goto` or an exception never reaches the`` |
|         - |  8672 | ` * "no more entries" arm that frees its step. OP_FOREACH_INIT reclaims such a` |
|         - |  8673 | ` * leftover, but only one whose owning frame is the frame running INIT -- so a step` |
|         - |  8674 | ` * belonging to an activation that had already returned stayed on the per-STATEMENT` |
|         - |  8675 | ` * aStep for the life of the VM, holding ~140 bytes and a retain of the subject, and` |
|         - |  8676 | ` * INIT's reclaim scan walked past all of them on every single iteration of every` |
|         - |  8677 | ` * enclosing loop. That is quadratic in the number of broken loops a program runs:` |
|         - |  8678 | ` * phpcs over one 318-line file reached 3600 dead steps and spent 60% of its time in` |
|         - |  8679 | ` * that scan.` |
|         - |  8680 | ` *` |
|         - |  8681 | ` * The frame that made a step is the one that can always end it. Called from both` |
|         - |  8682 | ` * VmFrame free sites (VmLeaveFrame and VmFreeDetachedFrame), after the frame has` |
|         - |  8683 | ` * left the active chain and before its locals are torn down -- the same point, and` |
|         - |  8684 | ` * the same order, the loop's own last iteration would have released it at.` |
|         - |  8685 | ` */` |
|   3828687 |  8686 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  8687 | `{` |
|   3828692 |  8688 | `	if( pFrame == 0 ){` |
|       ! 0 |  8689 | `		return;` |
|         - |  8690 | `	}` |
|   3829060 |  8691 | `	while( pFrame->pForeachSteps ){` |
|       373 |  8692 | `		ph7_foreach_step *pStep = pFrame->pForeachSteps;` |
|         - |  8693 | `		/* Detach BEFORE releasing rather than letting the release do it. The release` |
|         - |  8694 | `		 * runs teardown that can re-enter the VM (an instance losing its last retain` |
|         - |  8695 | `		 * runs __destruct), and a head that is still linked when that happens is a` |
|         - |  8696 | `		 * step whose frame is dying being handed back out. It also makes the loop` |
|         - |  8697 | `		 * unconditionally terminate: nothing here depends on the release finding this` |
|         - |  8698 | `		 * step to unlink, which VmForeachStepUnlink then simply doesn't. */` |
|       373 |  8699 | `		pFrame->pForeachSteps = pStep->pNextFrameStep;` |
|       373 |  8700 | `		pStep->pNextFrameStep = 0;` |
|       373 |  8701 | `		if( pStep->pInfo == 0 ){` |
|         - |  8702 | `			/* Never linked to a statement, so nothing else can free it. */` |
|       ! 0 |  8703 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       ! 0 |  8704 | `			continue;` |
|         - |  8705 | `		}` |
|       373 |  8706 | `		VmForeachStepRelease(&(*pVm),pStep->pInfo,pStep);` |
|         5 |  8707 | `	}` |
|   1914404 |  8708 | `}` |
|      1302 |  8709 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 |  8710 | `{` |
|      1307 |  8711 | `	if( pStep->pOwner ){` |
|       779 |  8712 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       387 |  8713 | `	}` |
|      1307 |  8714 | `	VmForeachStepUnlink(pInfo,pStep);` |
|      1307 |  8715 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|      1307 |  8716 | `	PH7_ClassInstanceUnref(pThis);` |
|      1307 |  8717 | `}` |
|         - |  8718 | `/*` |
|         - |  8719 | ` * Release ONE foreach step of any kind — the single door OP_FOREACH_INIT uses to` |
|         - |  8720 | ` * reclaim a step its loop never exhausted.` |
|         - |  8721 | ` *` |
|         - |  8722 | `` * A `foreach` that leaves through `break`, `return`, `goto` or an exception never`` |
|         - |  8723 | ` * reaches the "no more entries" arm, so its step stayed on pInfo->aStep forever` |
|         - |  8724 | ` * (~140 bytes and one retain of the subject per execution: 200k broken loops leaked` |
|         - |  8725 | ` * 28 MB). Worse for an OBJECT loop, whose cursor is REGISTERED on the instance —` |
|         - |  8726 | ` * every abandoned walk left an entry that each later property add/remove had to` |
|         - |  8727 | ` * walk past. A step for THIS pInfo whose owning frame is the running one cannot be` |
|         - |  8728 | ` * mid-loop when INIT runs again (the frame executes one instruction at a time), so` |
|         - |  8729 | ` * INIT reclaims it before pushing its own.` |
|         - |  8730 | ` */` |
|       384 |  8731 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 |  8732 | `{` |
|         - |  8733 | `	ph7_class_instance *pThis;` |
|       389 |  8734 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|       375 |  8735 | `		VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|       375 |  8736 | `		return;` |
|         - |  8737 | `	}` |
|         - |  8738 | `	/* Object-shaped step (plain attribute walk or the Iterator protocol): both` |
|         - |  8739 | `	 * retain xIter.pThis, and only the plain one holds a registered cursor. */` |
|        22 |  8740 | `	pThis = (pStep->iFlags & (PH7_4EACH_STEP_OBJECT\|PH7_4EACH_STEP_ITERATOR))` |
|        14 |  8741 | `		? pStep->xIter.pThis : 0;` |
|        15 |  8742 | `	if( pStep->iFlags & PH7_4EACH_STEP_OBJECT ){` |
|       ! 0 |  8743 | `		PH7_ClassInstanceIterClose(pStep->xIter.pThis,&pStep->sAttrIter);` |
|       ! 0 |  8744 | `	}` |
|        15 |  8745 | `	if( pStep->pOwner ){` |
|       ! 0 |  8746 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       ! 0 |  8747 | `	}` |
|        15 |  8748 | `	VmForeachStepUnlink(pInfo,pStep);` |
|        15 |  8749 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        15 |  8750 | `	if( pThis ){` |
|        15 |  8751 | `		PH7_ClassInstanceUnref(pThis);` |
|         7 |  8752 | `	}` |
|       197 |  8753 | `}` |
|         - |  8754 | `/*` |
|         - |  8755 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - |  8756 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - |  8757 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - |  8758 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - |  8759 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - |  8760 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - |  8761 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - |  8762 | ` * map's last reference.` |
|         - |  8763 | ` */` |
|     46729 |  8764 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 |  8765 | `{` |
|     46734 |  8766 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     46734 |  8767 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     46734 |  8768 | `	if( bPop ){` |
|         - |  8769 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - |  8770 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     46734 |  8771 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     23330 |  8772 | `	}` |
|     46734 |  8773 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     46734 |  8774 | `	PH7_HashmapUnref(pMap);` |
|     46734 |  8775 | `}` |
|         - |  8776 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - |  8777 | `/*` |
|         - |  8778 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - |  8779 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - |  8780 | ` * See block-comment on that function for additional information.` |
|         - |  8781 | ` */` |
|   1552270 |  8782 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 |  8783 | `{` |
|         - |  8784 | `	ph7_value *pStack;` |
|         - |  8785 | `	sxu32 nCap;` |
|         - |  8786 | `	sxi32 rc;` |
|         - |  8787 | `	/* Allocate a new operand stack */` |
|   1552275 |  8788 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1552275 |  8789 | `	if( pStack == 0 ){` |
|       ! 0 |  8790 | `		return SXERR_MEM;` |
|         - |  8791 | `	}` |
|   1552275 |  8792 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - |  8793 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - |  8794 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1552275 |  8795 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - |  8796 | `	/* Free the operand stack */` |
|   1552275 |  8797 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - |  8798 | `	/* Execution result */` |
|   1552275 |  8799 | `	return rc;` |
|    776075 |  8800 | `}` |
|         - |  8801 | `/*` |
|         - |  8802 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - |  8803 | ` * statement must honour?` |
|         - |  8804 | ` *` |
|         - |  8805 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - |  8806 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - |  8807 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - |  8808 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - |  8809 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - |  8810 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - |  8811 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - |  8812 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - |  8813 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - |  8814 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - |  8815 | ` * drift apart.` |
|         - |  8816 | ` *` |
|         - |  8817 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - |  8818 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - |  8819 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - |  8820 | ` * this opcode did not create as its own throw.` |
|         - |  8821 | ` *` |
|         - |  8822 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - |  8823 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - |  8824 | ` */` |
|         - |  8825 | `/*` |
|         - |  8826 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - |  8827 | ` *` |
|         - |  8828 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - |  8829 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - |  8830 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - |  8831 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - |  8832 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - |  8833 | ` * captured value writes over the superglobal's own slot: calling` |
|         - |  8834 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - |  8835 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - |  8836 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - |  8837 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - |  8838 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - |  8839 | ` *` |
|         - |  8840 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - |  8841 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - |  8842 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - |  8843 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - |  8844 | ` */` |
|      9420 |  8845 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 |  8846 | `{` |
|         - |  8847 | `	static const char *const azAuto[] = {` |
|         - |  8848 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - |  8849 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - |  8850 | `	};` |
|         - |  8851 | `	sxu32 n;` |
|    102967 |  8852 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     93613 |  8853 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     93613 |  8854 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        69 |  8855 | `			return 1;` |
|         - |  8856 | `		}` |
|     46526 |  8857 | `	}` |
|      9359 |  8858 | `	return 0;` |
|      4690 |  8859 | `}` |
|     31919 |  8860 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 |  8861 | `{` |
|     30091 |  8862 | `	return rc == PH7_EXCEPTION` |
|     30097 |  8863 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     46062 |  8864 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 |  8865 | `}` |
|         - |  8866 | `/*` |
|         - |  8867 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - |  8868 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - |  8869 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - |  8870 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - |  8871 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - |  8872 | ` */` |
|       182 |  8873 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         4 |  8874 | `{` |
|       186 |  8875 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       186 |  8876 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - |  8877 | `	sxi32 rc;` |
|       186 |  8878 | `	if( pDeclCls ){` |
|       170 |  8879 | `		pVm->pConstEvalClass = pDeclCls;` |
|       170 |  8880 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        83 |  8881 | `	}` |
|       186 |  8882 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       186 |  8883 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       186 |  8884 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       186 |  8885 | `	return rc;` |
|         4 |  8886 | `}` |
|         - |  8887 | `/*` |
|         - |  8888 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|         - |  8889 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|         - |  8890 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|         - |  8891 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|         - |  8892 | ` * non-zero status — lost that output entirely.` |
|         - |  8893 | ` *` |
|         - |  8894 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|         - |  8895 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|         - |  8896 | ` * handed.` |
|         - |  8897 | ` */` |
|      6885 |  8898 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 |  8899 | `{` |
|      6890 |  8900 | `	PH7_VmObFlushAll(&(*pVm));` |
|      6890 |  8901 | `}` |
|         - |  8902 | `/*` |
|         - |  8903 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - |  8904 | ` * or more calls to [register_shutdown_function()].` |
|         - |  8905 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - |  8906 | ` * execution ends.` |
|         - |  8907 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - |  8908 | ` * additional information.` |
|         - |  8909 | ` */` |
|      6885 |  8910 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 |  8911 | `{` |
|         - |  8912 | `	VmShutdownCB *pEntry;` |
|         - |  8913 | `	ph7_value *apArg[10];` |
|         - |  8914 | `	sxu32 n,nEntry;` |
|      6890 |  8915 | `	sxi32 rc = SXRET_OK;` |
|         - |  8916 | `	int i;` |
|         - |  8917 | `	/* Point to the stack of registered callbacks */` |
|      6890 |  8918 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     75740 |  8919 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     68855 |  8920 | `		apArg[i] = 0;` |
|     34375 |  8921 | `	}` |
|         - |  8922 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - |  8923 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - |  8924 | `	 * callbacks, mirroring PHP.` |
|         - |  8925 | `	 */` |
|      6890 |  8926 | `	pVm->bHaltRequested = 0;` |
|      6932 |  8927 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        49 |  8928 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        49 |  8929 | `		if( pEntry ){` |
|         - |  8930 | `			/* Prepare callback arguments if any */` |
|        59 |  8931 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|        12 |  8932 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 |  8933 | `					break;` |
|         - |  8934 | `				}` |
|        12 |  8935 | `				apArg[i] = &pEntry->aArg[i];` |
|         7 |  8936 | `			}` |
|         - |  8937 | `			/* Invoke the callback */` |
|        71 |  8938 | `			rc = PH7_VmCallUserFunction(&(*pVm),` |
|        44 |  8939 | `				(pEntry->sInvoke.iFlags & MEMOBJ_OBJ) ? &pEntry->sInvoke : &pEntry->sCallback,` |
|        22 |  8940 | `				pEntry->nArg,apArg,0);` |
|         - |  8941 | `			/*` |
|         - |  8942 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - |  8943 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - |  8944 | `			 */` |
|        49 |  8945 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        49 |  8946 | `			if( pEntry ){` |
|        49 |  8947 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        49 |  8948 | `				PH7_MemObjRelease(&pEntry->sInvoke);` |
|        59 |  8949 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|        12 |  8950 | `					PH7_MemObjRelease(apArg[i]);` |
|         7 |  8951 | `				}` |
|        22 |  8952 | `			}` |
|        49 |  8953 | `			if( pVm->bHaltRequested \|\| rc == SXERR_ABORT ){` |
|         - |  8954 | `				/* exit() inside the callback, or a throwable it never caught: php` |
|         - |  8955 | `				 * abandons the remaining callbacks either way (the bailout leaves` |
|         - |  8956 | `				 * php_call_shutdown_functions), and goes on to the destructors. */` |
|         2 |  8957 | `				break;` |
|         - |  8958 | `			}` |
|        21 |  8959 | `		}` |
|        26 |  8960 | `	}` |
|      6890 |  8961 | `	SySetReset(&pVm->aShutdown);` |
|      6890 |  8962 | `}` |
|         - |  8963 | `/*` |
|         - |  8964 | ` * One name of the global symbol table, snapshotted for the shutdown pass below.` |
|         - |  8965 | ` * Held as an offset into a private blob rather than a pointer: a destructor is` |
|         - |  8966 | ` * arbitrary PHP and may unset any global, which frees the key the table owns.` |
|         - |  8967 | ` */` |
|         - |  8968 | `typedef struct VmShutdownName VmShutdownName;` |
|         - |  8969 | `struct VmShutdownName` |
|         - |  8970 | `{` |
|         - |  8971 | `	sxu32 nOfft;  /* Offset of the name in the caller's snapshot blob */` |
|         - |  8972 | `	sxu32 nByte;  /* Its length */` |
|         - |  8973 | `};` |
|         - |  8974 | `/*` |
|         - |  8975 | ` * TRUE when this slot is held by exactly ONE name and nothing else -- the state php` |
|         - |  8976 | `` * spells `Z_TYPE_P(zv) == IS_OBJECT` with a refcount of 1 on a symbol-table entry.`` |
|         - |  8977 | ` *` |
|         - |  8978 | `` * php's symbol-table pass tests the ZVAL, and a name written with `&` is not an object`` |
|         - |  8979 | `` * zval at all: `$g = new T; $r = &$g;` makes both entries IS_REFERENCE, which the test`` |
|         - |  8980 | ` * rejects outright and leaves to the object-store pass. This engine has no separate` |
|         - |  8981 | ` * reference cell -- the two names simply share one slot -- so the equivalent question` |
|         - |  8982 | ` * is how many names the slot's reference record still lists, plus whether anything the` |
|         - |  8983 | `` * record cannot name pins it (a `use (&$x)` capture, a static, a reference-bound`` |
|         - |  8984 | ` * property), which php would also be carrying as a reference.` |
|         - |  8985 | ` *` |
|         - |  8986 | ` * The $GLOBALS entry for the name is not a holder for this purpose: it is how this` |
|         - |  8987 | ` * engine spells the symbol table, not a second reference to the value.` |
|         - |  8988 | ` */` |
|      4323 |  8989 | `static int VmSlotHeldByOneName(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  8990 | `{` |
|      4328 |  8991 | `	if( PH7_VmSlotSelfPinned(&(*pVm),nIdx) ){` |
|        52 |  8992 | `		return 0;` |
|         - |  8993 | `	}` |
|      4280 |  8994 | `	return PH7_VmSlotEntryCount(&(*pVm),nIdx) == 1;` |
|      2163 |  8995 | `}` |
|         - |  8996 | `/*` |
|         - |  8997 | ` * php's shutdown destructor phase, first half: the GLOBAL SYMBOL TABLE.` |
|         - |  8998 | ` *` |
|         - |  8999 | `` * `shutdown_destructors()` walks the symbol table in REVERSE and drops every entry`` |
|         - |  9000 | ` * holding an object nothing else refers to, repeating the walk while the table keeps` |
|         - |  9001 | `` * shrinking. That reverse walk is observable -- `$a = new T; $b = new T;` destructs`` |
|         - |  9002 | ` * $b before $a -- and it is the half that actually FREES its objects, which is why a` |
|         - |  9003 | ` * destructor here sees the rest of the program's globals still standing.` |
|         - |  9004 | ` *` |
|         - |  9005 | `` * `iRef == 1` plus VmSlotHeldByOneName is this engine's spelling of php's`` |
|         - |  9006 | `` * `Z_TYPE_P(zv) == IS_OBJECT && Z_REFCOUNT_P(zv) == 1`: exactly one memory object holds`` |
|         - |  9007 | ` * the instance and exactly one name holds that, so dropping the name ends it. Everything` |
|         - |  9008 | ` * else -- an object two names share, one an array or a property also holds, one a name` |
|         - |  9009 | `` * written with `&` reaches -- is left to the second half.`` |
|         - |  9010 | ` */` |
|      6885 |  9011 | `static void VmShutdownGlobalPass(ph7_vm *pVm)` |
|         5 |  9012 | `{` |
|         - |  9013 | `	VmFrame *pFrame;` |
|      6926 |  9014 | `	for( pFrame = pVm->pFrame ; pFrame && pFrame->pParent ; pFrame = pFrame->pParent ){}` |
|      6890 |  9015 | `	if( pFrame == 0 ){` |
|       ! 0 |  9016 | `		return;` |
|         - |  9017 | `	}` |
|      4643 |  9018 | `	for(;;){` |
|         - |  9019 | `		ph7_hashmap_node *pNode;` |
|         - |  9020 | `		VmShutdownName *aName;` |
|         - |  9021 | `		SyBlob sNames;` |
|         - |  9022 | `		SySet aEntry;` |
|         - |  9023 | `		sxu32 n;` |
|      8096 |  9024 | `		int bDropped = 0;` |
|      8096 |  9025 | `		if( pVm->pGlobal == 0 \|\| pVm->pGlobal->nEntry < 1 ){` |
|       ! 0 |  9026 | `			return;` |
|         - |  9027 | `		}` |
|         - |  9028 | `		/* Snapshot the names, last-declared first. The map's insertion list runs` |
|         - |  9029 | `		 * pFirst -> pPrev -> ... -> pLast, so walking it BACKWARDS is pLast and the` |
|         - |  9030 | `		 * pNext chain (the two link names read the other way round here). */` |
|      8096 |  9031 | `		SyBlobInit(&sNames,&pVm->sAllocator);` |
|      8096 |  9032 | `		SySetInit(&aEntry,&pVm->sAllocator,sizeof(VmShutdownName));` |
|    119432 |  9033 | `		for( pNode = pVm->pGlobal->pLast ; pNode ; pNode = pNode->pNext ){` |
|         - |  9034 | `			VmShutdownName sName;` |
|    111341 |  9035 | `			if( pNode->iType != HASHMAP_BLOB_NODE \|\| SyBlobLength(&pNode->xKey.sKey) < 1 ){` |
|       ! 0 |  9036 | `				continue;` |
|         - |  9037 | `			}` |
|    111341 |  9038 | `			sName.nOfft = SyBlobLength(&sNames);` |
|    111341 |  9039 | `			sName.nByte = SyBlobLength(&pNode->xKey.sKey);` |
|    111336 |  9040 | `			if( SyBlobAppend(&sNames,SyBlobData(&pNode->xKey.sKey),sName.nByte) != SXRET_OK` |
|    111341 |  9041 | `			 \|\| SySetPut(&aEntry,(const void *)&sName) != SXRET_OK ){` |
|         - |  9042 | `				/* Out of memory: go on with the names already gathered. */` |
|       ! 0 |  9043 | `				break;` |
|         - |  9044 | `			}` |
|     55538 |  9045 | `		}` |
|      8096 |  9046 | `		aName = (VmShutdownName *)SySetBasePtr(&aEntry);` |
|    119380 |  9047 | `		for( n = 0 ; n < SySetUsed(&aEntry) ; ++n ){` |
|    111293 |  9048 | `			const char *zName = (const char *)SyBlobData(&sNames) + aName[n].nOfft;` |
|         - |  9049 | `			ph7_class_instance *pThis;` |
|         - |  9050 | `			SyHashEntry *pHash;` |
|         - |  9051 | `			ph7_value *pObj;` |
|    111293 |  9052 | `			pHash = SyHashGet(&pFrame->hVar,(const void *)zName,aName[n].nByte);` |
|    111293 |  9053 | `			if( pHash == 0 ){` |
|     80869 |  9054 | `				continue;  /* An earlier destructor already dropped this one */` |
|         - |  9055 | `			}` |
|     30429 |  9056 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pHash->pUserData));` |
|     30429 |  9057 | `			if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     24522 |  9058 | `				continue;` |
|         - |  9059 | `			}` |
|      5912 |  9060 | `			pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      5907 |  9061 | `			if( pThis->iRef != 1` |
|      5120 |  9062 | `			 \|\| !VmSlotHeldByOneName(&(*pVm),(sxu32)SX_PTR_TO_INT(pHash->pUserData)) ){` |
|      1657 |  9063 | `				continue;` |
|         - |  9064 | `			}` |
|      4260 |  9065 | `			VmUnsetVarByNameEx(&(*pVm),pFrame,zName,aName[n].nByte,FALSE);` |
|      4260 |  9066 | `			bDropped = 1;` |
|      4260 |  9067 | `			if( pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|         4 |  9068 | `				break;` |
|         - |  9069 | `			}` |
|      2127 |  9070 | `		}` |
|      8096 |  9071 | `		SySetRelease(&aEntry);` |
|      8096 |  9072 | `		SyBlobRelease(&sNames);` |
|      8096 |  9073 | `		if( !bDropped \|\| pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|      6890 |  9074 | `			return;` |
|         - |  9075 | `		}` |
|         5 |  9076 | `	}` |
|      3442 |  9077 | `}` |
|         - |  9078 | `/*` |
|         - |  9079 | ` * Order the collected instances by their object handle -- php's object store is` |
|         - |  9080 | ` * walked front to back, and a handle is handed out in creation order, so this is` |
|         - |  9081 | ` * "oldest object first". Shell sort: no allocation, no recursion, and the array is` |
|         - |  9082 | ` * the objects a finished program left alive.` |
|         - |  9083 | ` */` |
|       301 |  9084 | `static void VmSortByObjId(ph7_class_instance **apObj,sxu32 nUsed)` |
|         5 |  9085 | `{` |
|         - |  9086 | `	static const sxu32 aGap[] = { 701, 301, 132, 57, 23, 10, 4, 1 };` |
|         - |  9087 | `	sxu32 g;` |
|      2714 |  9088 | `	for( g = 0 ; g < SX_ARRAYSIZE(aGap) ; ++g ){` |
|      2413 |  9089 | `		sxu32 nGap = aGap[g], i;` |
|     15998 |  9090 | `		for( i = nGap ; i < nUsed ; ++i ){` |
|     13590 |  9091 | `			ph7_class_instance *pCur = apObj[i];` |
|     13590 |  9092 | `			sxu32 j = i;` |
|     21791 |  9093 | `			while( j >= nGap && apObj[j-nGap]->nObjId > pCur->nObjId ){` |
|      8206 |  9094 | `				apObj[j] = apObj[j-nGap];` |
|      8206 |  9095 | `				j -= nGap;` |
|         5 |  9096 | `			}` |
|     13590 |  9097 | `			apObj[j] = pCur;` |
|      6794 |  9098 | `		}` |
|      1205 |  9099 | `	}` |
|       306 |  9100 | `}` |
|         - |  9101 | `/*` |
|         - |  9102 | ` * php's shutdown destructor phase, second half: the OBJECT STORE.` |
|         - |  9103 | ` *` |
|         - |  9104 | `` * `zend_objects_store_call_destructors()` reaches every object still alive after the`` |
|         - |  9105 | ` * symbol-table pass -- one a class static, a function static, an array or another` |
|         - |  9106 | ` * object holds, and one that is only part of a cycle -- and calls its destructor in` |
|         - |  9107 | ` * CREATION order without freeing it. The free comes later, from the teardown proper,` |
|         - |  9108 | ` * which is why the destructor is flagged as already run (CLASS_INSTANCE_DTOR_CALLED).` |
|         - |  9109 | ` *` |
|         - |  9110 | ` * Every object is reachable from the memory-object pool, so that pool is the store.` |
|         - |  9111 | ` * A destructor may create objects of its own (php destructs those too), so the sweep` |
|         - |  9112 | ` * repeats until a round finds nothing new.` |
|         - |  9113 | ` */` |
|      6885 |  9114 | `static void VmShutdownObjectPass(ph7_vm *pVm)` |
|         5 |  9115 | `{` |
|      7191 |  9116 | `	while( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|         - |  9117 | `		ph7_class_instance **apObj;` |
|         - |  9118 | `		SySet aObj;` |
|         - |  9119 | `		sxu32 n,nUsed;` |
|      7187 |  9120 | `		SySetInit(&aObj,&pVm->sAllocator,sizeof(ph7_class_instance *));` |
|   6032272 |  9121 | `		for( n = 0 ; n < pVm->aMemObj.nUsed ; ++n ){` |
|   6025090 |  9122 | `			ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|   6025090 |  9123 | `			if( pObj && (pObj->iFlags & MEMOBJ_OBJ) && pObj->x.pOther ){` |
|      6769 |  9124 | `				ph7_class_instance *pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      6769 |  9125 | `				if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|      3393 |  9126 | `					SySetPut(&aObj,(const void *)&pThis);` |
|      1691 |  9127 | `				}` |
|      3376 |  9128 | `			}` |
|   3011317 |  9129 | `		}` |
|      7187 |  9130 | `		nUsed = SySetUsed(&aObj);` |
|      7187 |  9131 | `		if( nUsed < 1 ){` |
|      6886 |  9132 | `			SySetRelease(&aObj);` |
|      6886 |  9133 | `			return;` |
|         - |  9134 | `		}` |
|       306 |  9135 | `		apObj = (ph7_class_instance **)SySetBasePtr(&aObj);` |
|       306 |  9136 | `		VmSortByObjId(apObj,nUsed);` |
|         - |  9137 | `		/* Pin every one of them BEFORE the first body runs: a destructor is free to` |
|         - |  9138 | `		 * unset whatever holds another object on this list, and the release that` |
|         - |  9139 | `		 * follows would free a pointer still to be visited. php pins the same way,` |
|         - |  9140 | `		 * one at a time, because its store can tell a dead bucket from a live one. */` |
|      3694 |  9141 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      3393 |  9142 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       759 |  9143 | `				continue;  /* Several names for one object: sorted, so duplicates adjoin */` |
|         - |  9144 | `			}` |
|      2639 |  9145 | `			apObj[n]->iRef++;` |
|      1319 |  9146 | `		}` |
|      3694 |  9147 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      3393 |  9148 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       759 |  9149 | `				continue;` |
|         - |  9150 | `			}` |
|      2639 |  9151 | `			if( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|         - |  9152 | `				/* A body that leaves an uncaught throwable raises bShutdownAborted` |
|         - |  9153 | `				 * itself, which is what stops this loop and the sweep around it:` |
|         - |  9154 | `				 * php abandons the whole phase on the first one, leaving every` |
|         - |  9155 | `				 * remaining object undestructed. */` |
|      2639 |  9156 | `				PH7_ClassInstanceCallDestructor(apObj[n]);` |
|      1314 |  9157 | `			}` |
|      1319 |  9158 | `		}` |
|      3694 |  9159 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      3393 |  9160 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       759 |  9161 | `				continue;` |
|         - |  9162 | `			}` |
|      2639 |  9163 | `			PH7_ClassInstanceUnref(apObj[n]);` |
|      1319 |  9164 | `		}` |
|       306 |  9165 | `		SySetRelease(&aObj);` |
|         5 |  9166 | `	}` |
|      3442 |  9167 | `}` |
|         - |  9168 | `/*` |
|         - |  9169 | ` * Run every destructor a finished program still owes, between the shutdown callbacks` |
|         - |  9170 | `` * and the output-buffer flush -- php's `zend_call_destructors()`, in that same slot of`` |
|         - |  9171 | `` * `php_request_shutdown()`, which is why a destructor's own echo still lands inside an`` |
|         - |  9172 | ` * open output buffer.` |
|         - |  9173 | ` *` |
|         - |  9174 | ` * Before this existed, an object a program left in a global (or a static, or any` |
|         - |  9175 | ` * container) was torn down by PH7_VmReset with user destructors suppressed, so a` |
|         - |  9176 | ` * destructor that closes a file, flushes a buffer or commits a transaction simply` |
|         - |  9177 | ` * never fired. The two passes below are php's two, in php's order.` |
|         - |  9178 | ` */` |
|      6885 |  9179 | `static void VmCallShutdownDestructors(ph7_vm *pVm)` |
|         5 |  9180 | `{` |
|      6890 |  9181 | `	if( pVm->bInReset ){` |
|       ! 0 |  9182 | `		return;` |
|         - |  9183 | `	}` |
|         - |  9184 | `	/* A halt is consumed the same way the shutdown callbacks consume theirs: php runs` |
|         - |  9185 | `	 * the destructor phase after an exit(), and after a shutdown callback that threw. */` |
|      6890 |  9186 | `	pVm->bHaltRequested = 0;` |
|      6890 |  9187 | `	pVm->bShutdownAborted = 0;` |
|      6890 |  9188 | `	pVm->bInShutdownDtor = 1;` |
|      6890 |  9189 | `	VmShutdownGlobalPass(&(*pVm));` |
|      6890 |  9190 | `	VmShutdownObjectPass(&(*pVm));` |
|      6890 |  9191 | `	pVm->bInShutdownDtor = 0;` |
|      3442 |  9192 | `}` |
|         - |  9193 | `/*` |
|         - |  9194 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - |  9195 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - |  9196 | ` * See block-comment on that function for additional information.` |
|         - |  9197 | ` */` |
|      6869 |  9198 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 |  9199 | `{` |
|         - |  9200 | `	/* Make sure we are ready to execute this program */` |
|      6874 |  9201 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 |  9202 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - |  9203 | `	}` |
|         - |  9204 | `	/* Set the execution magic number  */` |
|      6874 |  9205 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - |  9206 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - |  9207 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - |  9208 | `	{` |
|      6874 |  9209 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|         - |  9210 | `		/* Top-level code is a body like any other, and the global frame is the one` |
|         - |  9211 | `		 * frame that was pushed long before there was anything to number. Number it` |
|         - |  9212 | `		 * here, where the program about to run is finally known. */` |
|      6874 |  9213 | `		if( pVm->pFrame && pVm->pFrame->pCodeBase == 0 ){` |
|      6874 |  9214 | `			sxu16 nMainName = 0;` |
|     10303 |  9215 | `			VmNumberLocals((VmInstr *)SySetBasePtr(pVm->pByteContainer),` |
|      6869 |  9216 | `				SySetUsed(pVm->pByteContainer),&nMainName);` |
|      6874 |  9217 | `			if( nMainName > 0 ){` |
|      2477 |  9218 | `				pVm->pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|      1231 |  9219 | `			}` |
|      3429 |  9220 | `		}` |
|      6874 |  9221 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - |  9222 | `	}` |
|         - |  9223 | `	/* Invoke any shutdown callbacks */` |
|      6882 |  9224 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - |  9225 | `	/* Then every destructor the program still owes: php's zend_call_destructors(),` |
|         - |  9226 | `	 * which sits exactly here -- after the shutdown callbacks, before the buffers. */` |
|      6882 |  9227 | `	VmCallShutdownDestructors(&(*pVm));` |
|         - |  9228 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - |  9229 | `	 * shutdown callbacks, which may still write into them. */` |
|      6882 |  9230 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - |  9231 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|         - |  9232 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|         - |  9233 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|         - |  9234 | `	 * them). */` |
|      6882 |  9235 | `	PH7_VmSessionShutdown(&(*pVm));` |
|         - |  9236 | `	/*` |
|         - |  9237 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - |  9238 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - |  9239 | `	 * [ph7_vm_reset()] first would fail.` |
|         - |  9240 | `	 */` |
|      6882 |  9241 | `	return SXRET_OK;` |
|      3434 |  9242 | `}` |
|         - |  9243 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - |  9244 | `/*` |
|         - |  9245 | ` * Invoke the installed VM output consumer callback to consume` |
|         - |  9246 | ` * the desired message.` |
|         - |  9247 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - |  9248 | ` * in 'api.c' for additional information.` |
|         - |  9249 | ` */` |
|    178777 |  9250 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - |  9251 | `	ph7_vm *pVm,      /* Target VM */` |
|         - |  9252 | `	SyString *pString /* Message to output */` |
|         - |  9253 | `	)` |
|         5 |  9254 | `{` |
|    178782 |  9255 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    178782 |  9256 | `	sxi32 rc = SXRET_OK;` |
|         - |  9257 | `	/* Call the output consumer */` |
|    178782 |  9258 | `	if( pString->nByte > 0 ){` |
|    178782 |  9259 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|    178782 |  9260 | `		VmTrackOutput(pVm, pString->nByte);` |
|     88495 |  9261 | `	}` |
|    178782 |  9262 | `	return rc;` |
|         5 |  9263 | `}` |
|         - |  9264 | `/*` |
|         - |  9265 | ` * Format a message and invoke the installed VM output consumer` |
|         - |  9266 | ` * callback to consume the formatted message.` |
|         - |  9267 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - |  9268 | ` * in 'api.c' for additional information.` |
|         - |  9269 | ` */` |
|        30 |  9270 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - |  9271 | `	ph7_vm *pVm,         /* Target VM */` |
|         - |  9272 | `	const char *zFormat, /* Formatted message to output */` |
|         - |  9273 | `	va_list ap           /* Variable list of arguments */` |
|         - |  9274 | `	)` |
|         2 |  9275 | `{` |
|        32 |  9276 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        32 |  9277 | `	sxi32 rc = SXRET_OK;` |
|         - |  9278 | `	SyBlob sWorker;` |
|         - |  9279 | `	/* Format the message and call the output consumer */` |
|        32 |  9280 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        32 |  9281 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        32 |  9282 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - |  9283 | `		/* Consume the formatted message */` |
|        32 |  9284 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        15 |  9285 | `	}` |
|        32 |  9286 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - |  9287 | `	/* Release the working buffer */` |
|        32 |  9288 | `	SyBlobRelease(&sWorker);` |
|        32 |  9289 | `	return rc;` |
|         2 |  9290 | `}` |
|         - |  9291 | `/*` |
|         - |  9292 | ` * Return a string representation of the given PH7 OP code.` |
|         - |  9293 | ` * This function never fail and always return a pointer` |
|         - |  9294 | ` * to a null terminated string.` |
|         - |  9295 | ` */` |
|        10 |  9296 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 |  9297 | `{` |
|        11 |  9298 | `	const char *zOp = "Unknown     ";` |
|        11 |  9299 | `	switch(nOp){` |
|         3 |  9300 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 |  9301 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 |  9302 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 |  9303 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 |  9304 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 |  9305 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 |  9306 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 |  9307 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 |  9308 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 |  9309 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 |  9310 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 |  9311 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 |  9312 | `	case PH7_OP_SNAPSHOT:   zOp = "SNAPSHOT   "; break;` |
|       ! 0 |  9313 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 |  9314 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 |  9315 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 |  9316 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 |  9317 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 |  9318 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 |  9319 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 |  9320 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 |  9321 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 |  9322 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 |  9323 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 |  9324 | `	case PH7_OP_NAMED_SEND: zOp = "NAMED_SEND "; break;` |
|       ! 0 |  9325 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 |  9326 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 |  9327 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 |  9328 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 |  9329 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 |  9330 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 |  9331 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 |  9332 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 |  9333 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 |  9334 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 |  9335 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 |  9336 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 |  9337 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 |  9338 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 |  9339 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 |  9340 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 |  9341 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 |  9342 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 |  9343 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 |  9344 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 |  9345 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 |  9346 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 |  9347 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 |  9348 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 |  9349 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 |  9350 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 |  9351 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 |  9352 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 |  9353 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 |  9354 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 |  9355 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 |  9356 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 |  9357 | `	case PH7_OP_PICK:       zOp = "PICK       "; break;` |
|       ! 0 |  9358 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 |  9359 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 |  9360 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 |  9361 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 |  9362 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 |  9363 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 |  9364 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 |  9365 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 |  9366 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 |  9367 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 |  9368 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 |  9369 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 |  9370 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 |  9371 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 |  9372 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 |  9373 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 |  9374 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 |  9375 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 |  9376 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 |  9377 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 |  9378 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 |  9379 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 |  9380 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 |  9381 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 |  9382 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 |  9383 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 |  9384 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 |  9385 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 |  9386 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 |  9387 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 |  9388 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 |  9389 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 |  9390 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 |  9391 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 |  9392 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 |  9393 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 |  9394 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 |  9395 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 |  9396 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 |  9397 | `	case PH7_OP_FUNC_DECL:  zOp = "FUNC_DECL  "; break;` |
|       ! 0 |  9398 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 |  9399 | `	case PH7_OP_CLASS_OBLIGE:zOp = "CLASS_OBLIGE"; break;` |
|       ! 0 |  9400 | `	case PH7_OP_CLASS_DECLARE:zOp = "CLASS_DECLARE"; break;` |
|       ! 0 |  9401 | `	case PH7_OP_CONST_DECL: zOp = "CONST_DECL "; break;` |
|       ! 0 |  9402 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 |  9403 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 |  9404 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 |  9405 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 |  9406 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 |  9407 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 |  9408 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 |  9409 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 |  9410 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 |  9411 | `	default:` |
|       ! 0 |  9412 | `		break;` |
|         - |  9413 | `	}` |
|        11 |  9414 | `	return zOp;` |
|         1 |  9415 | `}` |
|         - |  9416 | `/*` |
|         - |  9417 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - |  9418 | ` * The xConsumer() callback which is an used defined function` |
|         - |  9419 | ` * is responsible of consuming the generated dump.` |
|         - |  9420 | ` */` |
|         2 |  9421 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - |  9422 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  9423 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - |  9424 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - |  9425 | `	)` |
|         1 |  9426 | `{` |
|         - |  9427 | `	sxi32 rc;` |
|         3 |  9428 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 |  9429 | `	return rc;` |
|         1 |  9430 | `}` |
|         - |  9431 | `/*` |
|         - |  9432 | ` * Section:` |
|         - |  9433 | ` *  Function handling functions.` |
|         - |  9434 | ` * Status:` |
|         - |  9435 | ` *    Stable.` |
|         - |  9436 | ` */` |
|         - |  9437 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - |  9438 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - |  9439 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - |  9440 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - |  9441 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - |  9442 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - |  9443 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - |  9444 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - |  9445 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - |  9446 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - |  9447 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - |  9448 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - |  9449 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - |  9450 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - |  9451 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - |  9452 | `	    /* Constants management */` |
|         - |  9453 | `	{ "defined",  vm_builtin_defined              },` |
|         - |  9454 | `	{ "define",   vm_builtin_define               },` |
|         - |  9455 | `	{ "constant", vm_builtin_constant             },` |
|         - |  9456 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - |  9457 | `	   /* Class/Object functions */` |
|         - |  9458 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - |  9459 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - |  9460 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - |  9461 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - |  9462 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - |  9463 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - |  9464 | `	{ "class_parents",   vm_builtin_class_parents     },` |
|         - |  9465 | `	{ "class_implements",vm_builtin_class_implements  },` |
|         - |  9466 | `	{ "class_uses",      vm_builtin_class_uses        },` |
|         - |  9467 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - |  9468 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - |  9469 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - |  9470 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - |  9471 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - |  9472 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - |  9473 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|         - |  9474 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - |  9475 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - |  9476 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - |  9477 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|         - |  9478 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - |  9479 | `	{ "is_a", vm_builtin_is_a },` |
|         - |  9480 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - |  9481 | `	{ "clone",           vm_builtin_clone             },` |
|         - |  9482 | `	   /* SPL object identity */` |
|         - |  9483 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - |  9484 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - |  9485 | `	   /* SPL Autoloading */` |
|         - |  9486 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - |  9487 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - |  9488 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - |  9489 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - |  9490 | `	{ "spl_autoload_extensions", vm_builtin_spl_autoload_extensions },` |
|         - |  9491 | `	{ "spl_autoload_call",       vm_builtin_spl_autoload_call       },` |
|         - |  9492 | `	{ "spl_classes",             vm_builtin_spl_classes             },` |
|         - |  9493 | `	   /* Random numbers/strings generators */` |
|         - |  9494 | `	{ "rand",          vm_builtin_rand            },` |
|         - |  9495 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - |  9496 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - |  9497 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - |  9498 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - |  9499 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - |  9500 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - |  9501 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - |  9502 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - |  9503 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - |  9504 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - |  9505 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - |  9506 | `	   /* Language constructs functions.` |
|         - |  9507 | `	    *` |
|         - |  9508 | ``	    * `echo` is not among them: it is the one construct whose codegen emits an`` |
|         - |  9509 | `	    * OPCODE and no call at all (PH7_OP_CONSUME, PH7_CompileEcho), so the name was` |
|         - |  9510 | `	    * a registration nothing could ever dispatch -- and one more row php has no` |
|         - |  9511 | `	    * function for. The rest stay, hidden (PH7_VmGetHostFunction). */` |
|         - |  9512 | `	{ "print", vm_builtin_print                   },` |
|         - |  9513 | `	{ "exit",  vm_builtin_exit                    },` |
|         - |  9514 | `	{ "die",   vm_builtin_exit                    },` |
|         - |  9515 | `	{ "eval",  vm_builtin_eval                    },` |
|         - |  9516 | `	  /* Variable handling functions */` |
|         - |  9517 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - |  9518 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - |  9519 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - |  9520 | `	{ "settype",   vm_builtin_settype              },` |
|         - |  9521 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - |  9522 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - |  9523 | `	{ "isset",     vm_builtin_isset                },` |
|         - |  9524 | `	{ "unset",     vm_builtin_unset                },` |
|         - |  9525 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - |  9526 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - |  9527 | `	{ "var_export",vm_builtin_var_export           },` |
|         - |  9528 | `	  /* Ouput control functions */` |
|         - |  9529 | `	{ "flush",        vm_builtin_flush             },` |
|         - |  9530 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - |  9531 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - |  9532 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - |  9533 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - |  9534 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - |  9535 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - |  9536 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|         - |  9537 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - |  9538 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - |  9539 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - |  9540 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|         - |  9541 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - |  9542 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - |  9543 | `	  /* Assertion functions */` |
|         - |  9544 | `	{ "assert",          vm_builtin_assert         },` |
|         - |  9545 | `	  /* Error reporting functions */` |
|         - |  9546 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - |  9547 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - |  9548 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - |  9549 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - |  9550 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - |  9551 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - |  9552 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - |  9553 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - |  9554 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - |  9555 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - |  9556 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - |  9557 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - |  9558 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - |  9559 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - |  9560 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - |  9561 | `	  /* Release info */` |
|         - |  9562 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - |  9563 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - |  9564 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - |  9565 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - |  9566 | `	{"get_extension_funcs",   vm_builtin_get_extension_funcs   },` |
|         - |  9567 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - |  9568 | `	{"php_ini_loaded_file",   vm_builtin_php_ini_loaded_file },` |
|         - |  9569 | `	{"php_ini_scanned_files", vm_builtin_php_ini_scanned_files },` |
|         - |  9570 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - |  9571 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - |  9572 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - |  9573 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - |  9574 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - |  9575 | `	  /* hashmap */` |
|         - |  9576 | `	{"compact",          vm_builtin_compact       },` |
|         - |  9577 | `	{"extract",          vm_builtin_extract       },` |
|         - |  9578 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - |  9579 | `	  /* URL related function */` |
|         - |  9580 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - |  9581 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - |  9582 | `	   /* Command line processing */` |
|         - |  9583 | `	{"getopt",         vm_builtin_getopt     },` |
|         - |  9584 | `	   /* JSON encoding/decoding */` |
|         - |  9585 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - |  9586 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - |  9587 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - |  9588 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - |  9589 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - |  9590 | `	{"serialize",      vm_builtin_serialize },` |
|         - |  9591 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - |  9592 | `	   /* Files/URI inclusion facility */` |
|         - |  9593 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - |  9594 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - |  9595 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - |  9596 | ``	/* php's alias: the same list under the name a `require` reader`` |
|         - |  9597 | `	 * reaches for. */` |
|         - |  9598 | `	{ "get_required_files",vm_builtin_get_included_files},` |
|         - |  9599 | `	{ "include",      vm_builtin_include          },` |
|         - |  9600 | `	{ "include_once", vm_builtin_include_once     },` |
|         - |  9601 | `	{ "require",      vm_builtin_require          },` |
|         - |  9602 | `	{ "require_once", vm_builtin_require_once     },` |
|         - |  9603 | `};` |
|         - |  9604 | `/*` |
|         - |  9605 | ` * Register the built-in VM functions defined above.` |
|         - |  9606 | ` */` |
|      8445 |  9607 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 |  9608 | `{` |
|         - |  9609 | `	sxi32 rc;` |
|         - |  9610 | `	sxu32 n;` |
|   1148525 |  9611 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - |  9612 | `		/* Note that these special functions have access` |
|         - |  9613 | `		 * to the underlying virtual machine as their` |
|         - |  9614 | `		 * private data.` |
|         - |  9615 | `		 */` |
|   1140080 |  9616 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|   1140080 |  9617 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  9618 | `			return rc;` |
|         - |  9619 | `		}` |
|    569300 |  9620 | `	}` |
|      8450 |  9621 | `	return SXRET_OK;` |
|      4222 |  9622 | `}` |
|         - |  9623 | `/*` |
|         - |  9624 | ` * The nine host functions that are php LANGUAGE CONSTRUCTS. Their implementations stay` |
|         - |  9625 | ` * registered -- each construct's codegen dispatches an OP_CALL to the name -- and the` |
|         - |  9626 | ` * mark hides the name from every door a SCRIPT can ask through (PH7_VmGetHostFunction).` |
|         - |  9627 | ` *` |
|         - |  9628 | `` * `echo` is NOT here: it is the one construct with no call at all (PH7_CompileEcho emits`` |
|         - |  9629 | ``  * OP_CONSUME), so its host function was simply dropped rather than hidden. `exit`, `die` `` |
|         - |  9630 | `` * and `clone` are not here either -- php 8.5 has all three as real internal functions.`` |
|         - |  9631 | ` */` |
|         - |  9632 | `static const char * const azLangConstruct[] = {` |
|         - |  9633 | `	"print", "isset", "unset", "empty", "eval",` |
|         - |  9634 | `	"include", "include_once", "require", "require_once"` |
|         - |  9635 | `};` |
|         - |  9636 | `/*` |
|         - |  9637 | ` * Stamp the mark. Runs once at VM init, after every registration pass: the nine live in` |
|         - |  9638 | `` * two different tables (`empty` in builtin.c's, the rest in this file's), and one list`` |
|         - |  9639 | ` * walked at the end covers both without either table having to know.` |
|         - |  9640 | ` */` |
|      6985 |  9641 | `PH7_PRIVATE void PH7_VmMarkLanguageConstructs(ph7_vm *pVm)` |
|         5 |  9642 | `{` |
|         - |  9643 | `	sxu32 n;` |
|     69855 |  9644 | `	for( n = 0 ; n < SX_ARRAYSIZE(azLangConstruct) ; ++n ){` |
|    125735 |  9645 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hHostFunction,` |
|     62865 |  9646 | `			(const void *)azLangConstruct[n],SyStrlen(azLangConstruct[n]));` |
|     62870 |  9647 | `		if( pEntry && pEntry->pUserData ){` |
|     62870 |  9648 | `			((ph7_user_func *)pEntry->pUserData)->bConstruct = 1;` |
|     31383 |  9649 | `		}` |
|     31388 |  9650 | `	}` |
|      6990 |  9651 | `}` |
|         - |  9652 | `/*` |
|         - |  9653 | ` * The internal function names this engine carries that php has none of. They matter` |
|         - |  9654 | ` * to exactly one question -- may a user function take this name? -- and the answer` |
|         - |  9655 | ` * php gives is "no, the name is already taken", which is only true of a name php` |
|         - |  9656 | ` * itself has. Shadowing one of these RUNS under php, so it has to run here.` |
|         - |  9657 | ` *` |
|         - |  9658 | ` * Each is a PH7 legacy verb kept for the embedding API, an engine introspection` |
|         - |  9659 | ` * hook, or a name php REMOVED (each(), fgetss(), import_request_variables()) -- and` |
|         - |  9660 | ` * every one of them is a name real code declares: size_format() is WordPress's, and` |
|         - |  9661 | ` * each() is the classic php-5 polyfill.` |
|         - |  9662 | ` *` |
|         - |  9663 | ` * Re-derive after adding or removing a builtin, by diffing the internal half of` |
|         - |  9664 | ` * get_defined_functions() between this engine and the oracle:` |
|         - |  9665 | ` *` |
|         - |  9666 | ` *   echo '<?php $f=get_defined_functions()["internal"]; sort($f);` |
|         - |  9667 | ` *         echo implode("\n",$f),"\n";' > names.php` |
|         - |  9668 | ` *   phl names.php \| LC_ALL=C sort -u > phl.names` |
|         - |  9669 | ` *   XDEBUG_MODE=off php names.php \| LC_ALL=C sort -u > php.names` |
|         - |  9670 | ` *   LC_ALL=C comm -23 phl.names php.names` |
|         - |  9671 | ` *` |
|         - |  9672 | ` * The nine LANGUAGE CONSTRUCTS above are not here and do not need to be: every one` |
|         - |  9673 | `` * of them is a reserved word, so `function print(){}` never reaches this question --`` |
|         - |  9674 | ` * the parser refuses it first, under php and under this.` |
|         - |  9675 | ` *` |
|         - |  9676 | ` * __tempnam_in() is not here either, and is the one name this list still owes php:` |
|         - |  9677 | ` * it is a builtin this engine writes in PHP, so it lives in the USER table, and` |
|         - |  9678 | ` * letting the declaration through is not enough -- a call compiled against the` |
|         - |  9679 | ` * engine's copy stays bound to it, so the user function installs, answers` |
|         - |  9680 | ` * Reflection, and never runs. It needs the call site rebound, not a carve-out.` |
|         - |  9681 | ` */` |
|         - |  9682 | `static const char * const azEngineOnlyFunc[] = {` |
|         - |  9683 | `	"array_copy", "array_erase", "array_same", "debug_string_backtrace",` |
|         - |  9684 | `	"delete", "each", "fgetss", "func_get_args_byref", "get_defined_classes",` |
|         - |  9685 | `	"getgid", "getpid", "getuid", "implode_recursive",` |
|         - |  9686 | `	"import_request_variables", "join_recursive", "ph7_info", "ph7_uname",` |
|         - |  9687 | `	"ph7copyright", "ph7credits", "ph7info", "ph7version", "rand_str",` |
|         - |  9688 | `	"setenv", "size_format", "strglob"` |
|         - |  9689 | `};` |
|         - |  9690 | `/*` |
|         - |  9691 | ` */` |
|         - |  9692 | `/*` |
|         - |  9693 | ` * Is this name already an INTERNAL function, in the sense php's redeclaration fatal` |
|         - |  9694 | ` * means it? Both doors of a function declaration ask it -- the compiler for a` |
|         - |  9695 | ` * top-level one, OP_FUNC_DECL for a conditional one -- so they refuse the same set.` |
|         - |  9696 | ` *` |
|         - |  9697 | ` * The engine's own extras above are excluded; everything else in hHostFunction is a` |
|         - |  9698 | ` * name php has too, because the table this engine registers IS php's list minus what` |
|         - |  9699 | ` * is not built yet. A name php has that this engine has not implemented is simply` |
|         - |  9700 | ` * not here, and shadowing it stays allowed -- that is the capability gap, not this.` |
|         - |  9701 | ` */` |
|        14 |  9702 | `static int VmNameIsEngineOnlyFunc(const char *zName,sxu32 nByte)` |
|         4 |  9703 | `{` |
|         - |  9704 | `	sxu32 n;` |
|       322 |  9705 | `	for( n = 0 ; n < SX_ARRAYSIZE(azEngineOnlyFunc) ; ++n ){` |
|       310 |  9706 | `		if( SyStrlen(azEngineOnlyFunc[n]) == nByte` |
|       175 |  9707 | `		 && SyStrnicmp(azEngineOnlyFunc[n],zName,nByte) == 0 ){` |
|         7 |  9708 | `			return 1;` |
|         - |  9709 | `		}` |
|       156 |  9710 | `	}` |
|        11 |  9711 | `	return 0;` |
|        11 |  9712 | `}` |
|      5182 |  9713 | `PH7_PRIVATE int PH7_VmNameIsInternalFunc(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 |  9714 | `{` |
|      5187 |  9715 | `	if( nByte < 1 ){` |
|       ! 0 |  9716 | `		return 0;` |
|         - |  9717 | `	}` |
|      5187 |  9718 | `	if( SyHashGet(&pVm->hHostFunction,(const void *)zName,nByte) == 0 ){` |
|      5173 |  9719 | `		return 0;` |
|         - |  9720 | `	}` |
|        18 |  9721 | `	return !VmNameIsEngineOnlyFunc(zName,nByte);` |
|      2590 |  9722 | `}` |
|         - |  9723 | `/*` |
|         - |  9724 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - |  9725 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - |  9726 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - |  9727 | ` */` |
|  10559345 |  9728 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 |  9729 | `{` |
|  10559350 |  9730 | `	sxi32 iSkip = PH7_CLASS_HIDDEN;` |
|  10559350 |  9731 | `	if( iLoadable ){` |
|   2613075 |  9732 | `		iSkip \|= PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT;` |
|   1306488 |  9733 | `	}` |
|  10559366 |  9734 | `	while(pClass){` |
|  10559350 |  9735 | `		if( (pClass->iFlags & iSkip) == 0 ){` |
|  10559334 |  9736 | `			return pClass;` |
|         - |  9737 | `		}` |
|        17 |  9738 | `		pClass = pClass->pNextName;` |
|         1 |  9739 | `	}` |
|        17 |  9740 | `	return 0;` |
|   5277271 |  9741 | `}` |
|         - |  9742 | `/*` |
|         - |  9743 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - |  9744 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - |  9745 | ` * with the class name. After each callback, checks if the class is now` |
|         - |  9746 | ` * registered in the VM's class table.` |
|         - |  9747 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - |  9748 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - |  9749 | ` */` |
|       888 |  9750 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 |  9751 | `{` |
|         - |  9752 | `	VmAutoloadCB *pEntry;` |
|         - |  9753 | `	ph7_value sArg,sResult;` |
|         - |  9754 | `	SyString *pSavedTrace;` |
|         - |  9755 | `	SyHashEntry *pHashEntry;` |
|         - |  9756 | `	ph7_class *pClass;` |
|         - |  9757 | `	sxu32 n,nEntry;` |
|         - |  9758 | `	sxi32 rc;` |
|       893 |  9759 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       893 |  9760 | `	if( nEntry < 1 ){` |
|       403 |  9761 | `		return 0;` |
|         - |  9762 | `	}` |
|         - |  9763 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       495 |  9764 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 |  9765 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - |  9766 | `	}` |
|         - |  9767 | `	/* Mark this class as being autoloaded */` |
|       493 |  9768 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - |  9769 | `	/* Prepare the class name argument */` |
|       493 |  9770 | `	PH7_MemObjInit(pVm,&sArg);` |
|       493 |  9771 | `	PH7_MemObjInit(pVm,&sResult);` |
|       493 |  9772 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       493 |  9773 | `	pClass = 0;` |
|      1047 |  9774 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - |  9775 | `		ph7_value *apArg[1];` |
|       771 |  9776 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       771 |  9777 | `		if( pEntry == 0 ){` |
|       ! 0 |  9778 | `			continue;` |
|         - |  9779 | `		}` |
|       771 |  9780 | `		apArg[0] = &sArg;` |
|         - |  9781 | `		/* NOT PH7_VmCallUserFunction: that wrapper marks the dispatch as an internal` |
|         - |  9782 | `		 * function reaching for a callback, which binds the argument weakly. php's` |
|         - |  9783 | `		 * autoload call is the one such dispatch that is NOT weak -- it reads the` |
|         - |  9784 | `		 * strict_types of the code whose class reference triggered it, and says so in` |
|         - |  9785 | `		 * its own diagnostic ("called in <that file> on line <that line>"). An` |
|         - |  9786 | ``		 * autoloader declaring anything but `string` therefore RAISES under a strict`` |
|         - |  9787 | ``		 * caller, where PHL coerced the class name (`bool $c` got true) and ran the`` |
|         - |  9788 | `		 * loader on a value that no longer named anything. */` |
|         - |  9789 | ``		/* ...but a builtin that asked for the class (class_exists(), is_a(), `new`` |
|         - |  9790 | ``		 * ReflectionClass`) is still a frame of php's trace, between the loader's and`` |
|         - |  9791 | `		 * the caller's. Consume-once, so put back whatever a loader never reached. */` |
|       771 |  9792 | `		pSavedTrace = pVm->pNativeTraceName;` |
|       771 |  9793 | `		pVm->pNativeTraceName = PH7_VmReachingNativeName(pVm);` |
|      1154 |  9794 | `		rc = PH7_VmCallUserFunctionWithMap(pVm,` |
|       766 |  9795 | `				(pEntry->sInvoke.iFlags & MEMOBJ_OBJ) ? &pEntry->sInvoke : &pEntry->sCallback,` |
|       383 |  9796 | `				1,apArg,&sResult,0);` |
|       771 |  9797 | `		pVm->pNativeTraceName = pSavedTrace;` |
|       771 |  9798 | `		if( PH7_CALLBACK_UNWOUND(rc) ){` |
|         - |  9799 | `			/* The loader threw or exited: php stops the chain there, so no later` |
|         - |  9800 | `			 * loader is asked for a class the unwinding frame will never use. */` |
|        48 |  9801 | `			break;` |
|         - |  9802 | `		}` |
|       683 |  9803 | `		if( rc != SXRET_OK ){` |
|         - |  9804 | `			/* Callback could not be invoked — skip to next autoloader */` |
|       ! 0 |  9805 | `			continue;` |
|         - |  9806 | `		}` |
|         - |  9807 | `		/* Check if the class is now available */` |
|       683 |  9808 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       683 |  9809 | `		if( pHashEntry ){` |
|       129 |  9810 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|       129 |  9811 | `			if( pClass ){` |
|       129 |  9812 | `				break;` |
|         - |  9813 | `			}` |
|       ! 0 |  9814 | `		}` |
|       282 |  9815 | `	}` |
|       493 |  9816 | `	PH7_MemObjRelease(&sArg);` |
|       493 |  9817 | `	PH7_MemObjRelease(&sResult);` |
|         - |  9818 | `	/* Remove reentrancy guard */` |
|       493 |  9819 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       493 |  9820 | `	return pClass;` |
|       449 |  9821 | `}` |
|         - |  9822 | `/*` |
|         - |  9823 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - |  9824 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - |  9825 | ` */` |
|       120 |  9826 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 |  9827 | `{` |
|       125 |  9828 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         5 |  9829 | `}` |
|         - |  9830 | `/*` |
|         - |  9831 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - |  9832 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - |  9833 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - |  9834 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - |  9835 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - |  9836 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - |  9837 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - |  9838 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - |  9839 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - |  9840 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - |  9841 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - |  9842 | ` */` |
|  11095386 |  9843 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 |  9844 | `{` |
|  11095391 |  9845 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|       121 |  9846 | `		(*pzName)++;` |
|       121 |  9847 | `		(*pnByte)--;` |
|        58 |  9848 | `	}` |
|  11095391 |  9849 | `}` |
|         - |  9850 | `/*` |
|         - |  9851 | ` * Check if the given name refer to an installed class.` |
|         - |  9852 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - |  9853 | ` */` |
|  10559999 |  9854 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - |  9855 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  9856 | `	const char *zName,  /* Name of the target class */` |
|         - |  9857 | `	sxu32 nByte,        /* zName length */` |
|         - |  9858 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - |  9859 | `						 * [i.e: no abstract classes or interfaces]` |
|         - |  9860 | `						 */` |
|         - |  9861 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - |  9862 | `	)` |
|         5 |  9863 | `{` |
|         - |  9864 | `	SyHashEntry *pEntry;` |
|         - |  9865 | `	ph7_class *pClass;` |
|   5277593 |  9866 | `	SXUNUSED(iNest);` |
|         - |  9867 | `	/* Exact class lookup.` |
|         - |  9868 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - |  9869 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - |  9870 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - |  9871 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|  10560004 |  9872 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - |  9873 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|         - |  9874 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|  10560004 |  9875 | `	if( nByte < 1 ){` |
|        12 |  9876 | `		return 0;` |
|         - |  9877 | `	}` |
|  10559994 |  9878 | `	pEntry = PH7_VmClassEntry(pVm,zName,nByte);` |
|  10559994 |  9879 | `	if( pEntry == 0 ){` |
|         - |  9880 | `		/* Class not found in hash table — try autoload before giving up */` |
|       773 |  9881 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         - |  9882 | `	}` |
|  10559226 |  9883 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|  10559226 |  9884 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   5277598 |  9885 | `}` |
|         - |  9886 | `/*` |
|         - |  9887 | ` * Reference Table Implementation` |
|         - |  9888 | ` * Status: stable <chm@symisc.net>` |
|         - |  9889 | ` * Intro` |
|         - |  9890 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - |  9891 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - |  9892 | ` *  the reference implementation is consistent,solid and it's` |
|         - |  9893 | ` *  behavior resemble the C++ reference mechanism.` |
|         - |  9894 | ` *  Refer to the official for more information on this powerful` |
|         - |  9895 | ` *  extension.` |
|         - |  9896 | ` */` |
|         - |  9897 | `/*` |
|         - |  9898 | ` * ---------------------------------------------------------------------------` |
|         - |  9899 | ` * The reference table.` |
|         - |  9900 | ` *` |
|         - |  9901 | ` * One TAGGED WORD per memory-object slot (pVm->apRefObj[nIdx]; see VM_REF_TAG_*` |
|         - |  9902 | ` * in ph7int.h for the five shapes). The table answers one question -- who still` |
|         - |  9903 | ` * holds this slot -- and that answer decides when a value is freed, so every` |
|         - |  9904 | ` * accessor below is asked BY SLOT INDEX: a slot whose answer fits in its word has` |
|         - |  9905 | ` * no record for a caller to hold on to.` |
|         - |  9906 | ` *` |
|         - |  9907 | ` * A record (VmRefObj) is the fallback for the answers a word cannot carry: two or` |
|         - |  9908 | ` * more names, two or more nodes, or a pin standing beside a named holder.` |
|         - |  9909 | ` * ---------------------------------------------------------------------------` |
|         - |  9910 | ` */` |
|         - |  9911 | `/* The word for a slot the table has been grown to cover; 0 otherwise. */` |
|  52284928 |  9912 | `static void * VmRefWord(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  9913 | `{` |
|  52284933 |  9914 | `	if( nIdx >= pVm->nRefSize ){` |
|         1 |  9915 | `		return 0;` |
|         - |  9916 | `	}` |
|  52284933 |  9917 | `	return pVm->apRefObj[nIdx];` |
|  26140203 |  9918 | `}` |
|         - |  9919 | `/*` |
|         - |  9920 | ` * Grow the table to cover a slot. Doubling keeps the growth amortized and, unlike` |
|         - |  9921 | ` * the hash table this replaced, nothing has to be MOVED: the cells that exist keep` |
|         - |  9922 | ` * their index, so the copy is one memcpy and the tail is zeroed.` |
|         - |  9923 | ` */` |
|  29597972 |  9924 | `static sxi32 VmRefTableGrow(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  9925 | `{` |
|         - |  9926 | `	void **apNew;` |
|         - |  9927 | `	sxu32 nNew;` |
|  29597977 |  9928 | `	if( nIdx < pVm->nRefSize ){` |
|  29588338 |  9929 | `		return SXRET_OK;` |
|         - |  9930 | `	}` |
|      9644 |  9931 | `	nNew = pVm->nRefSize ? pVm->nRefSize : 0x10;` |
|     19285 |  9932 | `	while( nIdx >= nNew ){` |
|      9646 |  9933 | `		nNew <<= 1;` |
|         5 |  9934 | `	}` |
|      9644 |  9935 | `	apNew = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * nNew);` |
|      9644 |  9936 | `	if( apNew == 0 ){` |
|       ! 0 |  9937 | `		return SXERR_MEM;` |
|         - |  9938 | `	}` |
|      9644 |  9939 | `	if( pVm->nRefSize > 0 ){` |
|      9644 |  9940 | `		SyMemcpy((const void *)pVm->apRefObj,(void *)apNew,pVm->nRefSize * sizeof(void *));` |
|      4804 |  9941 | `	}` |
|      9644 |  9942 | `	SyZero((void *)&apNew[pVm->nRefSize],(nNew - pVm->nRefSize) * sizeof(void *));` |
|      9644 |  9943 | `	SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|      9644 |  9944 | `	pVm->apRefObj = apNew;` |
|      9644 |  9945 | `	pVm->nRefSize = nNew;` |
|      9644 |  9946 | `	return SXRET_OK;` |
|  14797204 |  9947 | `}` |
|         - |  9948 | `/*` |
|         - |  9949 | ` * Store a word. The ONE place nRefUsed moves: it counts FILLED CELLS, so a cell` |
|         - |  9950 | ` * that goes from one shape to another (a name dropped to a bare mark, a word` |
|         - |  9951 | ` * promoted to a record) does not move it. The caller must already have grown the` |
|         - |  9952 | ` * table -- this cannot fail, which is what lets the callers below commit a state` |
|         - |  9953 | ` * change and a holder in the same breath.` |
|         - |  9954 | ` */` |
|  86277141 |  9955 | `static void VmRefWordSet(ph7_vm *pVm,sxu32 nIdx,void *pWord)` |
|         5 |  9956 | `{` |
|  86277146 |  9957 | `	void *pOld = pVm->apRefObj[nIdx];` |
|  86277146 |  9958 | `	if( pOld == 0 && pWord != 0 ){` |
|  29383381 |  9959 | `		pVm->nRefUsed++;` |
|  71583872 |  9960 | `	}else if( pOld != 0 && pWord == 0 ){` |
|  28403070 |  9961 | `		pVm->nRefUsed--;` |
|  14200698 |  9962 | `	}` |
|  86277146 |  9963 | `	pVm->apRefObj[nIdx] = pWord;` |
|  86277146 |  9964 | `}` |
|         - |  9965 | `/* A holder pointer carried in a word, with its tag taken back off. */` |
|         - |  9966 | `#define VM_REF_UNTAG(W,T) ((void *)&((char *)(W))[-(T)])` |
|         - |  9967 | `/*` |
|         - |  9968 | ` * May this pointer be tagged? The pool allocator keeps every chunk 8-aligned (the` |
|         - |  9969 | ` * C library's own alignment, a SyMemBlock that is a multiple of 8, and a` |
|         - |  9970 | ` * pointer-sized SyMemHeader -- see the alignment note on sxmem.c's OS methods), so` |
|         - |  9971 | ` * this is true everywhere it is asked -- but a word whose low bits are not free` |
|         - |  9972 | ` * would read back as another shape entirely, so the question is asked rather than` |
|         - |  9973 | ` * assumed and a stray pointer simply takes the record path.` |
|         - |  9974 | ` */` |
|  18555686 |  9975 | `static int VmRefTaggable(void *pPtr)` |
|         5 |  9976 | `{` |
|  18555691 |  9977 | `	return pPtr != 0 && (SX_PTR_TO_INT(pPtr) & VM_REF_TAG_MASK) == 0;` |
|         5 |  9978 | `}` |
|         - |  9979 | `/* Build a MARK word out of a pin count and the flags. */` |
|  39214281 |  9980 | `static void * VmRefMarkWord(sxu32 nPin,sxi32 iFlags)` |
|         5 |  9981 | `{` |
|  39214286 |  9982 | `	int iWord = VM_REF_TAG_MARK;` |
|  39214286 |  9983 | `	if( iFlags & VM_REF_IDX_KEEP ){` |
|  10828021 |  9984 | `		iWord \|= VM_REF_MARK_KEEP;` |
|   5413275 |  9985 | `	}` |
|  39214286 |  9986 | `	iWord \|= (int)(nPin * VM_REF_MARK_PIN);` |
|  39214286 |  9987 | `	return SX_INT_TO_PTR(iWord);` |
|         5 |  9988 | `}` |
|  24204864 |  9989 | `static sxu32 VmRefMarkPin(void *pWord)` |
|         5 |  9990 | `{` |
|  24204869 |  9991 | `	return ((sxu32)SX_PTR_TO_INT(pWord)) / VM_REF_MARK_PIN;` |
|         5 |  9992 | `}` |
|  11686936 |  9993 | `static int VmRefMarkKeep(void *pWord)` |
|         5 |  9994 | `{` |
|  11686941 |  9995 | `	return (SX_PTR_TO_INT(pWord) & VM_REF_MARK_KEEP) != 0;` |
|         5 |  9996 | `}` |
|         - |  9997 | `/*` |
|         - |  9998 | ` * Allocate a new reference record.` |
|         - |  9999 | ` *` |
|         - | 10000 | ` * Reached only by a slot whose holders will not fit in its word -- two names, two` |
|         - | 10001 | ` * nodes, or a pin beside a named holder. It used to be allocated for EVERY variable` |
|         - | 10002 | ` * a frame binds and EVERY element an array inserts, which made its size the engine's` |
|         - | 10003 | ` * per-value memory overhead.` |
|         - | 10004 | ` */` |
|    104109 | 10005 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10006 | `{` |
|         - | 10007 | `	VmRefObj *pRef;` |
|    104114 | 10008 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|    104114 | 10009 | `	if( pRef == 0 ){` |
|       ! 0 | 10010 | `		return 0;` |
|         - | 10011 | `	}` |
|         - | 10012 | `	/* Zero the structure */` |
|    104114 | 10013 | `	SyZero(pRef,sizeof(VmRefObj));` |
|    104114 | 10014 | `	pRef->nIdx = nIdx;` |
|    104114 | 10015 | `	return pRef;` |
|     51963 | 10016 | `}` |
|         - | 10017 | `/*` |
|         - | 10018 | ` * The spill sets of a record that has just been given a second holder, created on` |
|         - | 10019 | ` * demand. NULL on OOM, in which case the caller drops the row -- the same` |
|         - | 10020 | ` * degradation SySetPut's own failure already produced.` |
|         - | 10021 | ` */` |
|      3452 | 10022 | `static VmRefSpill * VmRefSpillGet(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 10023 | `{` |
|         - | 10024 | `	VmRefSpill *pSpill;` |
|      3457 | 10025 | `	if( pRef->pSpill ){` |
|      2722 | 10026 | `		return pRef->pSpill;` |
|         - | 10027 | `	}` |
|       740 | 10028 | `	pSpill = (VmRefSpill *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefSpill));` |
|       740 | 10029 | `	if( pSpill == 0 ){` |
|       ! 0 | 10030 | `		return 0;` |
|         - | 10031 | `	}` |
|       740 | 10032 | `	SySetInit(&pSpill->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|       740 | 10033 | `	SySetInit(&pSpill->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|       740 | 10034 | `	pRef->pSpill = pSpill;` |
|       740 | 10035 | `	return pSpill;` |
|      1729 | 10036 | `}` |
|         - | 10037 | `/*` |
|         - | 10038 | ` * File one holder on a record, ignoring a name that is already on it -- a name can be` |
|         - | 10039 | ` * RE-BOUND to the same slot any number of times, and a table that only ever grew made` |
|         - | 10040 | ` * both the install and the holder count O(rows).` |
|         - | 10041 | ` */` |
|     28640 | 10042 | `static void VmRefAddEntry(ph7_vm *pVm,VmRefObj *pRef,SyHashEntry *pEntry)` |
|         5 | 10043 | `{` |
|         - | 10044 | `	VmRefSpill *pSpill;` |
|         - | 10045 | `	SyHashEntry **apEntry;` |
|     28645 | 10046 | `	sxu32 n, nFree = SXU32_HIGH;` |
|     28645 | 10047 | `	if( pRef->pEntry0 == pEntry ){` |
|       ! 0 | 10048 | `		return;` |
|         - | 10049 | `	}` |
|     28645 | 10050 | `	if( pRef->pEntry0 == 0 ){` |
|     25839 | 10051 | `		pRef->pEntry0 = pEntry;` |
|     25839 | 10052 | `		return;` |
|         - | 10053 | `	}` |
|      2811 | 10054 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|      2811 | 10055 | `	if( pSpill == 0 ){` |
|       ! 0 | 10056 | `		return;` |
|         - | 10057 | `	}` |
|      2811 | 10058 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|      5488 | 10059 | `	for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; ++n ){` |
|      2682 | 10060 | `		if( apEntry[n] == pEntry ){` |
|       ! 0 | 10061 | `			return; /* already recorded: never file one holder twice */` |
|         - | 10062 | `		}` |
|      2682 | 10063 | `		if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|      2470 | 10064 | `			nFree = n; /* a row a dead holder left behind */` |
|      1231 | 10065 | `		}` |
|      1342 | 10066 | `	}` |
|      2811 | 10067 | `	if( nFree != SXU32_HIGH ){` |
|      2470 | 10068 | `		apEntry[nFree] = pEntry;` |
|      1236 | 10069 | `	}else{` |
|       346 | 10070 | `		SySetPut(&pSpill->aReference,(const void *)&pEntry);` |
|         - | 10071 | `	}` |
|     14287 | 10072 | `}` |
|     77635 | 10073 | `static void VmRefAddNode(ph7_vm *pVm,VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|         5 | 10074 | `{` |
|         - | 10075 | `	VmRefSpill *pSpill;` |
|         - | 10076 | `	ph7_hashmap_node **apNode;` |
|     77640 | 10077 | `	sxu32 n, nFree = SXU32_HIGH;` |
|     77640 | 10078 | `	if( pRef->pNode0 == pNode ){` |
|       ! 0 | 10079 | `		return;` |
|         - | 10080 | `	}` |
|     77640 | 10081 | `	if( pRef->pNode0 == 0 ){` |
|     76994 | 10082 | `		pRef->pNode0 = pNode;` |
|     76994 | 10083 | `		return;` |
|         - | 10084 | `	}` |
|       650 | 10085 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|       650 | 10086 | `	if( pSpill == 0 ){` |
|       ! 0 | 10087 | `		return;` |
|         - | 10088 | `	}` |
|       650 | 10089 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|       954 | 10090 | `	for( n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|       306 | 10091 | `		if( apNode[n] == pNode ){` |
|       ! 0 | 10092 | `			return;` |
|         - | 10093 | `		}` |
|       306 | 10094 | `		if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|        23 | 10095 | `			nFree = n;` |
|        11 | 10096 | `		}` |
|       154 | 10097 | `	}` |
|       650 | 10098 | `	if( nFree != SXU32_HIGH ){` |
|        23 | 10099 | `		apNode[nFree] = pNode;` |
|        12 | 10100 | `	}else{` |
|       628 | 10101 | `		SySetPut(&pSpill->aArrEntries,(const void *)&pNode);` |
|         - | 10102 | `	}` |
|     38762 | 10103 | `}` |
|         - | 10104 | `/*` |
|         - | 10105 | ` * Drop one holder from a record. Every row that names it goes, inline or spilled:` |
|         - | 10106 | ` * the table has never promised a holder appears once, and the count below reads` |
|         - | 10107 | ` * whatever is left.` |
|         - | 10108 | ` */` |
|     20616 | 10109 | `static void VmRefDropEntry(VmRefObj *pRef,SyHashEntry *pEntry)` |
|         5 | 10110 | `{` |
|     20621 | 10111 | `	if( pRef->pEntry0 == pEntry ){` |
|     17787 | 10112 | `		pRef->pEntry0 = 0;` |
|      8886 | 10113 | `	}` |
|     20621 | 10114 | `	if( pRef->pSpill ){` |
|      3235 | 10115 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|         - | 10116 | `		sxu32 n;` |
|      6439 | 10117 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|      3209 | 10118 | `			if( apEntry[n] == pEntry ){` |
|      2793 | 10119 | `				apEntry[n] = 0;` |
|      1392 | 10120 | `			}` |
|      1605 | 10121 | `		}` |
|      1613 | 10122 | `	}` |
|     20621 | 10123 | `}` |
|     16750 | 10124 | `static void VmRefDropNode(VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|         5 | 10125 | `{` |
|     16755 | 10126 | `	if( pRef->pNode0 == pNode ){` |
|     16263 | 10127 | `		pRef->pNode0 = 0;` |
|      8124 | 10128 | `	}` |
|     16755 | 10129 | `	if( pRef->pSpill ){` |
|       897 | 10130 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|         - | 10131 | `		sxu32 n;` |
|      2279 | 10132 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|      1385 | 10133 | `			if( apNode[n] == pNode ){` |
|       495 | 10134 | `				apNode[n] = 0;` |
|       246 | 10135 | `			}` |
|       694 | 10136 | `		}` |
|       447 | 10137 | `	}` |
|     16755 | 10138 | `}` |
|         - | 10139 | `/*` |
|         - | 10140 | ` * How many LIVE holders of each kind a RECORD still carries. A node counts only while` |
|         - | 10141 | ` * it still points HERE -- a slot index travels through the free list, so a record can` |
|         - | 10142 | ` * outlive the node that filed the row.` |
|         - | 10143 | ` */` |
|     26619 | 10144 | `static sxu32 VmRefEntryCount(VmRefObj *pRef)` |
|         5 | 10145 | `{` |
|     26624 | 10146 | `	sxu32 n, nLive = pRef->pEntry0 ? 1 : 0;` |
|     26624 | 10147 | `	if( pRef->pSpill ){` |
|      1618 | 10148 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|      1976 | 10149 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|       362 | 10150 | `			if( apEntry[n] ){` |
|        72 | 10151 | `				nLive++;` |
|        35 | 10152 | `			}` |
|       183 | 10153 | `		}` |
|       807 | 10154 | `	}` |
|     26624 | 10155 | `	return nLive;` |
|         5 | 10156 | `}` |
|     22120 | 10157 | `static sxu32 VmRefNodeCount(VmRefObj *pRef,sxu32 nIdx)` |
|         5 | 10158 | `{` |
|     22125 | 10159 | `	sxu32 n, nLive = (pRef->pNode0 && pRef->pNode0->nValIdx == nIdx) ? 1 : 0;` |
|     22125 | 10160 | `	if( pRef->pSpill ){` |
|      1366 | 10161 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|      3154 | 10162 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|      1792 | 10163 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|       844 | 10164 | `				nLive++;` |
|       420 | 10165 | `			}` |
|       898 | 10166 | `		}` |
|       681 | 10167 | `	}` |
|     22125 | 10168 | `	return nLive;` |
|         5 | 10169 | `}` |
|         - | 10170 | `/*` |
|         - | 10171 | ` * The RECORD behind a slot, or 0 when the slot's answer is in its word (which,` |
|         - | 10172 | ` * unlike the old VmRefObjExtract, does NOT mean the slot is unheld). Nothing` |
|         - | 10173 | ` * outside this section may hold one: every question is asked by index below.` |
|         - | 10174 | ` */` |
|       208 | 10175 | `static VmRefObj * VmRefFull(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 10176 | `{` |
|       210 | 10177 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|       210 | 10178 | `	if( pWord == 0 \|\| VM_REF_TAGOF(pWord) != VM_REF_TAG_FULL ){` |
|       ! 0 | 10179 | `		return 0;` |
|         - | 10180 | `	}` |
|       210 | 10181 | `	return (VmRefObj *)pWord;` |
|       106 | 10182 | `}` |
|         - | 10183 | `/*` |
|         - | 10184 | ` * Promote whatever a slot has into a real record, creating one if the slot has` |
|         - | 10185 | ` * nothing yet. 0 on OOM (the table cannot be grown, or the record cannot be` |
|         - | 10186 | ` * allocated), which every caller degrades to "the slot records nothing", exactly` |
|         - | 10187 | ` * as a failed install always has.` |
|         - | 10188 | ` */` |
|    108321 | 10189 | `static VmRefObj * VmRefMaterialize(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10190 | `{` |
|         - | 10191 | `	void *pWord;` |
|         - | 10192 | `	VmRefObj *pRef;` |
|         - | 10193 | `	int iTag;` |
|    108326 | 10194 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|       ! 0 | 10195 | `		return 0;` |
|         - | 10196 | `	}` |
|    108326 | 10197 | `	pWord = pVm->apRefObj[nIdx];` |
|    108326 | 10198 | `	iTag = pWord ? VM_REF_TAGOF(pWord) : VM_REF_TAG_MARK;` |
|    108326 | 10199 | `	if( pWord != 0 && iTag == VM_REF_TAG_FULL ){` |
|      4217 | 10200 | `		return (VmRefObj *)pWord;` |
|         - | 10201 | `	}` |
|    104114 | 10202 | `	pRef = VmNewRefObj(&(*pVm),nIdx);` |
|    104114 | 10203 | `	if( pRef == 0 ){` |
|       ! 0 | 10204 | `		return 0;` |
|         - | 10205 | `	}` |
|    104114 | 10206 | `	if( pWord != 0 ){` |
|    104114 | 10207 | `		switch( iTag ){` |
|     39200 | 10208 | `		case VM_REF_TAG_NAME:` |
|     78284 | 10209 | `			pRef->pEntry0 = (SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME);` |
|     78284 | 10210 | `			break;` |
|     12860 | 10211 | `		case VM_REF_TAG_NODE:` |
|     25653 | 10212 | `			pRef->pNode0 = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|     25653 | 10213 | `			break;` |
|        91 | 10214 | `		default: /* VM_REF_TAG_MARK */` |
|       186 | 10215 | `			pRef->nPin = VmRefMarkPin(pWord);` |
|       186 | 10216 | `			pRef->iFlags = VmRefMarkKeep(pWord) ? VM_REF_IDX_KEEP : 0;` |
|       182 | 10217 | `			break;` |
|         - | 10218 | `		}` |
|     51958 | 10219 | `	}` |
|    104114 | 10220 | `	VmRefWordSet(&(*pVm),nIdx,(void *)pRef);` |
|    104114 | 10221 | `	return pRef;` |
|     54063 | 10222 | `}` |
|         - | 10223 | `/*` |
|         - | 10224 | ` * ---------------------------------------------------------------------------` |
|         - | 10225 | ` * The questions, all asked by slot index.` |
|         - | 10226 | ` * ---------------------------------------------------------------------------` |
|         - | 10227 | ` */` |
|         - | 10228 | `/* Has anything ever been registered against this slot? A slot that answers NO has` |
|         - | 10229 | ` * never been in the table at all, which is what keeps it out of the free pool. */` |
|   2658653 | 10230 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10231 | `{` |
|   2658658 | 10232 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10233 | `		return 0;` |
|         - | 10234 | `	}` |
|   2658658 | 10235 | `	return VmRefWord(&(*pVm),nIdx) != 0;` |
|   1328571 | 10236 | `}` |
|         - | 10237 | `/*` |
|         - | 10238 | ` * The BARE MARK -- the one word that decides a slot's whole teardown by itself.` |
|         - | 10239 | ` *` |
|         - | 10240 | ` * VM_REF_TAG_MARK with nothing above the tag means "registered, held by nothing,` |
|         - | 10241 | ` * pinned by nothing", which is what a dropped holder leaves behind and therefore` |
|         - | 10242 | ` * what an array element's slot looks like once its node has been unlinked. For` |
|         - | 10243 | ` * that word all three of the questions below are already answered -- registered` |
|         - | 10244 | ` * yes, keep no, holders none -- and PH7_VmSlotUnlink has nothing to unlink but the` |
|         - | 10245 | ` * cell itself, so this empties it and says it did.` |
|         - | 10246 | ` *` |
|         - | 10247 | ` * A throwaway counter build over the ecosystem gate's phpcs step (built with the` |
|         - | 10248 | ` * counter, read once, then reverted) says it is not an edge case:` |
|         - | 10249 | ` * 22,831,962 of the 23,016,949 slots PH7_VmReleaseUnheldSlot is handed carry` |
|         - | 10250 | ` * exactly this word -- 99.20% -- and none of them carry a name or a node. The` |
|         - | 10251 | ` * teardown of one of those used to ask the same cell seven separate loads across` |
|         - | 10252 | ` * eight out-of-line calls.` |
|         - | 10253 | ` *` |
|         - | 10254 | ` * It cannot be confused with a real holder: a name or a node word is a pointer the` |
|         - | 10255 | ` * allocator has kept 4-aligned, tagged with 1 or 2, so its low two bits are never` |
|         - | 10256 | ` * 3; a record pointer's are 0; and a mark carrying a pin or a keep has bits set` |
|         - | 10257 | ` * above the tag. Only the bare mark is the integer 3.` |
|         - | 10258 | ` */` |
|  28402607 | 10259 | `PH7_PRIVATE int PH7_VmSlotDropIfBare(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10260 | `{` |
|  28402607 | 10261 | `	if( nIdx >= pVm->nRefSize` |
|  28402612 | 10262 | `	 \|\| SX_PTR_TO_INT(pVm->apRefObj[nIdx]) != VM_REF_TAG_MARK ){` |
|     16355 | 10263 | `		return 0;` |
|         - | 10264 | `	}` |
|  28386262 | 10265 | `	VmRefWordSet(&(*pVm),nIdx,0);` |
|  28386262 | 10266 | `	return 1;` |
|  14200474 | 10267 | `}` |
|         - | 10268 | `/* The names bound to the slot. */` |
|   2613258 | 10269 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10270 | `{` |
|         - | 10271 | `	void *pWord;` |
|   2613263 | 10272 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10273 | `		return 0;` |
|         - | 10274 | `	}` |
|   2613263 | 10275 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   2613263 | 10276 | `	if( pWord == 0 ){` |
|       ! 0 | 10277 | `		return 0;` |
|         - | 10278 | `	}` |
|   2613263 | 10279 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 10280 | `	case VM_REF_TAG_NAME: return 1;` |
|   1305080 | 10281 | `	case VM_REF_TAG_NODE: /* fall through */` |
|   2608673 | 10282 | `	case VM_REF_TAG_MARK: return 0;` |
|      4594 | 10283 | `	default:              return VmRefEntryCount((VmRefObj *)pWord);` |
|         - | 10284 | `	}` |
|   1305885 | 10285 | `}` |
|         - | 10286 | `/* The array nodes still pointing HERE -- a node that has moved on does not count. */` |
|   2406892 | 10287 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx)` |
|         4 | 10288 | `{` |
|         - | 10289 | `	void *pWord;` |
|   2406896 | 10290 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10291 | `		return 0;` |
|         - | 10292 | `	}` |
|   2406896 | 10293 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   2406896 | 10294 | `	if( pWord == 0 ){` |
|       ! 0 | 10295 | `		return 0;` |
|         - | 10296 | `	}` |
|   2406896 | 10297 | `	switch( VM_REF_TAGOF(pWord) ){` |
|         3 | 10298 | `	case VM_REF_TAG_NODE: {` |
|         7 | 10299 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|         7 | 10300 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|         - | 10301 | `	}` |
|   1203435 | 10302 | `	case VM_REF_TAG_NAME: /* fall through */` |
|   2406878 | 10303 | `	case VM_REF_TAG_MARK: return 0;` |
|        13 | 10304 | `	default:              return VmRefNodeCount((VmRefObj *)pWord,nIdx);` |
|         - | 10305 | `	}` |
|   1203452 | 10306 | `}` |
|         - | 10307 | `/* The counted pins (a reference-bound property, one per binding). */` |
|   2615786 | 10308 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10309 | `{` |
|         - | 10310 | `	void *pWord;` |
|   2615791 | 10311 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10312 | `		return 0;` |
|         - | 10313 | `	}` |
|   2615791 | 10314 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   2615791 | 10315 | `	if( pWord == 0 ){` |
|       ! 0 | 10316 | `		return 0;` |
|         - | 10317 | `	}` |
|   2615791 | 10318 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|   2409197 | 10319 | `		return VmRefMarkPin(pWord);` |
|         - | 10320 | `	}` |
|    206599 | 10321 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|      4810 | 10322 | `		return ((VmRefObj *)pWord)->nPin;` |
|         - | 10323 | `	}` |
|    201793 | 10324 | `	return 0;` |
|   1307149 | 10325 | `}` |
|         - | 10326 | `/* The permanent pin (VM_REF_IDX_KEEP): a use(&$x) capture, a static, an enum case,` |
|         - | 10327 | ` * and the hold a declared property has on its own value slot. */` |
|    246544 | 10328 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10329 | `{` |
|         - | 10330 | `	void *pWord;` |
|    246549 | 10331 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10332 | `		return 0;` |
|         - | 10333 | `	}` |
|    246549 | 10334 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|    246549 | 10335 | `	if( pWord == 0 ){` |
|       ! 0 | 10336 | `		return 0;` |
|         - | 10337 | `	}` |
|    246549 | 10338 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|      2377 | 10339 | `		return VmRefMarkKeep(pWord);` |
|         - | 10340 | `	}` |
|    244177 | 10341 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|     42388 | 10342 | `		return (((VmRefObj *)pWord)->iFlags & VM_REF_IDX_KEEP) != 0;` |
|         - | 10343 | `	}` |
|    201793 | 10344 | `	return 0;` |
|    122516 | 10345 | `}` |
|         - | 10346 | `/*` |
|         - | 10347 | ` * Is pNode the FIRST node filed against this slot, and the only live one? The cycle` |
|         - | 10348 | ` * collector's "is this element held by its own array and nothing else" test; the` |
|         - | 10349 | ` * first-filed row is the one an ordinary insert leaves, so any other answer means` |
|         - | 10350 | ` * somebody else is pointing at the same slot.` |
|         - | 10351 | ` */` |
|    201947 | 10352 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|         4 | 10353 | `{` |
|         - | 10354 | `	void *pWord;` |
|    201951 | 10355 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10356 | `		return 0;` |
|         - | 10357 | `	}` |
|    201951 | 10358 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|    201951 | 10359 | `	if( pWord == 0 ){` |
|       ! 0 | 10360 | `		return 0;` |
|         - | 10361 | `	}` |
|    201951 | 10362 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_NODE ){` |
|    301940 | 10363 | `		return VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pNode` |
|    201789 | 10364 | `			&& pNode->nValIdx == nIdx;` |
|         - | 10365 | `	}` |
|       159 | 10366 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|       159 | 10367 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|       159 | 10368 | `		return pRef->pNode0 == pNode && VmRefNodeCount(pRef,nIdx) == 1;` |
|         - | 10369 | `	}` |
|       ! 0 | 10370 | `	return 0;` |
|    100230 | 10371 | `}` |
|         - | 10372 | `/*` |
|         - | 10373 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 10374 | ` * names bound to it plus the array nodes pointing at it, plus the holders the` |
|         - | 10375 | ` * table cannot name. php refcounts a reference set and keeps the VALUE alive` |
|         - | 10376 | ` * while any holder remains, so this is the count every "may I release this` |
|         - | 10377 | ` * slot?" decision asks for.` |
|         - | 10378 | ` */` |
|  13098539 | 10379 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10380 | `{` |
|         - | 10381 | `	void *pWord;` |
|  13098544 | 10382 | `	sxu32 nLive = 0;` |
|  13098544 | 10383 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10384 | `		return 0;` |
|         - | 10385 | `	}` |
|  13098544 | 10386 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  13098544 | 10387 | `	if( pWord == 0 ){` |
|         3 | 10388 | `		return 0;` |
|         - | 10389 | `	}` |
|  13098542 | 10390 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 10391 | `	case VM_REF_TAG_NAME:` |
|       ! 0 | 10392 | `		return 1;` |
|    696079 | 10393 | `	case VM_REF_TAG_NODE: {` |
|   1392126 | 10394 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|   1392126 | 10395 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|         - | 10396 | `	}` |
|   5839820 | 10397 | `	case VM_REF_TAG_MARK: {` |
|  11684391 | 10398 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|  11684391 | 10399 | `		if( nPin > 0 ){` |
|         5 | 10400 | `			return nPin;` |
|         - | 10401 | `		}` |
|         - | 10402 | ``		/* A permanent pin -- a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 10403 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 10404 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 10405 | `		 * php's refcount does. */` |
|  11684387 | 10406 | `		return VmRefMarkKeep(pWord) ? 1 : 0;` |
|         - | 10407 | `	}` |
|     11020 | 10408 | `	default: {` |
|     22035 | 10409 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|     22035 | 10410 | `		if( pRef->nPin > 0 ){` |
|       112 | 10411 | `			nLive += pRef->nPin;` |
|     21980 | 10412 | `		}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         8 | 10413 | `			nLive++;` |
|         3 | 10414 | `		}` |
|     22035 | 10415 | `		nLive += VmRefEntryCount(pRef);` |
|     22035 | 10416 | `		nLive += VmRefNodeCount(pRef,nIdx);` |
|     22035 | 10417 | `		return nLive;` |
|         - | 10418 | `	}` |
|         - | 10419 | `	}` |
|   6551624 | 10420 | `}` |
|         - | 10421 | `/*` |
|         - | 10422 | ` * Does this slot's holder count include the OWNER's own hold?` |
|         - | 10423 | ` *` |
|         - | 10424 | ` * A property's slot is installed in the reference table with a permanent pin` |
|         - | 10425 | ` * (VM_REF_IDX_KEEP) when the property was created dynamically or re-created after` |
|         - | 10426 | ` * unset(), with a COUNTED pin when the property was BOUND to somebody else's slot` |
|         - | 10427 | `` * (`$o->p =& $x`), and with nothing at all when it came straight from the class`` |
|         - | 10428 | ` * declaration. PH7_VmSlotHolderCount counts those pins as holders, so a renderer` |
|         - | 10429 | ` * asking "is this value a REFERENCE" has to subtract the one hold that is the` |
|         - | 10430 | ` * property itself -- an array ELEMENT, which is its own first holder in the table,` |
|         - | 10431 | ` * asks the same question with a threshold of two.` |
|         - | 10432 | ` */` |
|      6721 | 10433 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10434 | `{` |
|      6726 | 10435 | `	return PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 \|\| PH7_VmSlotKeepPinned(&(*pVm),nIdx);` |
|         5 | 10436 | `}` |
|         - | 10437 | `/*` |
|         - | 10438 | ` * Delete every holder a slot has and empty its cell.` |
|         - | 10439 | ` *` |
|         - | 10440 | ` * Each row is cleared BEFORE the call that acts on it: unlinking a node runs the` |
|         - | 10441 | ` * value's release, which can re-enter this table for the same slot, and a row still` |
|         - | 10442 | ` * filled when that happens is a holder being handed out twice. The base pointer is` |
|         - | 10443 | ` * re-read per row for the same reason -- a re-entrant install may have grown the set` |
|         - | 10444 | ` * out from under it.` |
|         - | 10445 | ` */` |
|     16808 | 10446 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10447 | `{` |
|         - | 10448 | `	VmRefSpill *pSpill;` |
|         - | 10449 | `	VmRefObj *pRef;` |
|         - | 10450 | `	void *pWord;` |
|         - | 10451 | `	sxu32 n;` |
|     16813 | 10452 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|     16813 | 10453 | `	if( pWord == 0 ){` |
|       ! 0 | 10454 | `		return;` |
|         - | 10455 | `	}` |
|         - | 10456 | `	/* Empty the cell first: everything below can re-enter the table for this slot. */` |
|     16813 | 10457 | `	VmRefWordSet(&(*pVm),nIdx,0);` |
|     16813 | 10458 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 10459 | `	case VM_REF_TAG_NAME:` |
|       ! 0 | 10460 | `		SyHashDeleteEntry2((SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME));` |
|       ! 0 | 10461 | `		return;` |
|       168 | 10462 | `	case VM_REF_TAG_NODE:` |
|       336 | 10463 | `		PH7_HashmapUnlinkNode((ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE),FALSE);` |
|       336 | 10464 | `		return;` |
|        33 | 10465 | `	case VM_REF_TAG_MARK:` |
|        67 | 10466 | `		return; /* pins and flags name nothing to delete */` |
|      8208 | 10467 | `	default:` |
|     16406 | 10468 | `		break;` |
|         - | 10469 | `	}` |
|     16411 | 10470 | `	pRef = (VmRefObj *)pWord;` |
|     16411 | 10471 | `	if( pRef->pEntry0 ){` |
|       167 | 10472 | `		SyHashEntry *pEntry = pRef->pEntry0;` |
|       167 | 10473 | `		pRef->pEntry0 = 0;` |
|       167 | 10474 | `		SyHashDeleteEntry2(pEntry);` |
|        83 | 10475 | `	}` |
|     16411 | 10476 | `	if( pRef->pNode0 ){` |
|       167 | 10477 | `		ph7_hashmap_node *pNode = pRef->pNode0;` |
|       167 | 10478 | `		pRef->pNode0 = 0;` |
|       167 | 10479 | `		PH7_HashmapUnlinkNode(pNode,FALSE);` |
|        83 | 10480 | `	}` |
|     16411 | 10481 | `	pSpill = pRef->pSpill;` |
|     16411 | 10482 | `	if( pSpill ){` |
|       576 | 10483 | `		for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; n++ ){` |
|       172 | 10484 | `			SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|       172 | 10485 | `			SyHashEntry *pEntry = apEntry[n];` |
|       172 | 10486 | `			if( pEntry ){` |
|       ! 0 | 10487 | `				apEntry[n] = 0;` |
|       ! 0 | 10488 | `				SyHashDeleteEntry2(pEntry);` |
|       ! 0 | 10489 | `			}` |
|        88 | 10490 | `		}` |
|       712 | 10491 | `		for(n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|       306 | 10492 | `			ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|       306 | 10493 | `			ph7_hashmap_node *pNode = apNode[n];` |
|       306 | 10494 | `			if( pNode ){` |
|       ! 0 | 10495 | `				apNode[n] = 0;` |
|       ! 0 | 10496 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|       ! 0 | 10497 | `			}` |
|       154 | 10498 | `		}` |
|       408 | 10499 | `		SySetRelease(&pRef->pSpill->aReference);` |
|       408 | 10500 | `		SySetRelease(&pRef->pSpill->aArrEntries);` |
|       408 | 10501 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRef->pSpill);` |
|       408 | 10502 | `		pRef->pSpill = 0;` |
|       202 | 10503 | `	}` |
|     16411 | 10504 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|      8404 | 10505 | `}` |
|         - | 10506 | `/*` |
|         - | 10507 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 10508 | ` *` |
|         - | 10509 | ` * iFlags is applied only when the slot has NOTHING registered against it yet; a slot` |
|         - | 10510 | ` * already in the table keeps the flags it has (VmPinMemObjSlot is the door that adds` |
|         - | 10511 | ` * one). That has always been the rule -- it is now spelled out because the word for a` |
|         - | 10512 | ` * fresh slot is chosen from it.` |
|         - | 10513 | ` *` |
|         - | 10514 | ` * The implementation of the reference mechanism in the PH7 engine differ greatly from` |
|         - | 10515 | ` * the one used by the zend engine. That is, the reference implementation is` |
|         - | 10516 | ` * consistent,solid and it's behavior resemble the C++ reference mechanism.` |
|         - | 10517 | ` */` |
|  29489651 | 10518 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 10519 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 10520 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 10521 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 10522 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 10523 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 10524 | `	)` |
|         5 | 10525 | `{` |
|         - | 10526 | `	VmFrame *pFrame;` |
|         - | 10527 | `	VmRefObj *pRef;` |
|         - | 10528 | `	void *pWord;` |
|         - | 10529 | `	/* Cover the slot up front: everything below commits without a way to fail, and a` |
|         - | 10530 | `	 * table that cannot be grown records nothing at all -- the same degradation a` |
|         - | 10531 | `	 * failed record allocation has always produced. */` |
|  29489656 | 10532 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|       ! 0 | 10533 | `		return SXERR_MEM;` |
|         - | 10534 | `	}` |
|  29489656 | 10535 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  29489656 | 10536 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 10537 | `		VmSlot sRef;` |
|         - | 10538 | `		/* Local frame,record referenced entry so that it can` |
|         - | 10539 | `		 * be deleted when we leave this frame.` |
|         - | 10540 | `		 */` |
|   1580561 | 10541 | `		sRef.nIdx = nIdx;` |
|   1580561 | 10542 | `		sRef.pUserData = pEntry;` |
|   1580561 | 10543 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 10544 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 10545 | `		}` |
|    793368 | 10546 | `	}` |
|  29489656 | 10547 | `	pWord = pVm->apRefObj[nIdx];` |
|  29489656 | 10548 | `	if( pWord == 0 ){` |
|         - | 10549 | `		/* A slot nothing has claimed yet. This is the common case by three orders of` |
|         - | 10550 | `		 * magnitude, and every shape of it fits in the word. */` |
|  29383381 | 10551 | `		if( iFlags == 0 && pEntry != 0 && pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|   1650055 | 10552 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|   1650055 | 10553 | `			return SXRET_OK;` |
|         - | 10554 | `		}` |
|  27733331 | 10555 | `		if( iFlags == 0 && pMapEntry != 0 && pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
|  16905641 | 10556 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
|  16905641 | 10557 | `			return SXRET_OK;` |
|         - | 10558 | `		}` |
|  10827695 | 10559 | `		if( pEntry == 0 && pMapEntry == 0 && (iFlags & ~VM_REF_IDX_KEEP) == 0 ){` |
|         - | 10560 | `			/* A pin with no named holder -- every declared property's own hold on its` |
|         - | 10561 | `			 * value slot -- and the empty registration a dropped holder leaves behind. */` |
|  10827695 | 10562 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,iFlags));` |
|  10827695 | 10563 | `			return SXRET_OK;` |
|         - | 10564 | `		}` |
|       ! 0 | 10565 | `	}else{` |
|    106280 | 10566 | `		switch( VM_REF_TAGOF(pWord) ){` |
|     38520 | 10567 | `		case VM_REF_TAG_NAME:` |
|         - | 10568 | `			/* A name RE-BOUND to the slot it already names changes nothing, which is` |
|         - | 10569 | `			 * the frame-local variable written to in a loop. */` |
|     76919 | 10570 | `			if( pMapEntry == 0` |
|     38433 | 10571 | `			 && (pEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry) ){` |
|       ! 0 | 10572 | `				return SXRET_OK;` |
|         - | 10573 | `			}` |
|     76924 | 10574 | `			break;` |
|     12845 | 10575 | `		case VM_REF_TAG_NODE:` |
|     25618 | 10576 | `			if( pEntry == 0` |
|     12890 | 10577 | `			 && (pMapEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry) ){` |
|       ! 0 | 10578 | `				return SXRET_OK;` |
|         - | 10579 | `			}` |
|     25623 | 10580 | `			break;` |
|        91 | 10581 | `		case VM_REF_TAG_MARK:` |
|       186 | 10582 | `			if( pEntry == 0 && pMapEntry == 0 ){` |
|       ! 0 | 10583 | `				return SXRET_OK; /* iFlags is ignored on a slot already registered */` |
|         - | 10584 | `			}` |
|       186 | 10585 | `			if( SX_PTR_TO_INT(pWord) == VM_REF_TAG_MARK ){` |
|         - | 10586 | `				/* A bare mark says "registered, held by nothing"; a holder on top of it` |
|         - | 10587 | `				 * says exactly what the pointer words say. */` |
|       ! 0 | 10588 | `				if( pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|       ! 0 | 10589 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|       ! 0 | 10590 | `					return SXRET_OK;` |
|         - | 10591 | `				}` |
|       ! 0 | 10592 | `				if( pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
|       ! 0 | 10593 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
|       ! 0 | 10594 | `					return SXRET_OK;` |
|         - | 10595 | `				}` |
|       ! 0 | 10596 | `			}` |
|       182 | 10597 | `			break;` |
|      1780 | 10598 | `		default:` |
|      3556 | 10599 | `			break;` |
|         - | 10600 | `		}` |
|         - | 10601 | `	}` |
|         - | 10602 | `	/* More than one word can say. */` |
|    106280 | 10603 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|    106280 | 10604 | `	if( pRef == 0 ){` |
|       ! 0 | 10605 | `		return SXERR_MEM;` |
|         - | 10606 | `	}` |
|    106280 | 10607 | `	if( pWord == 0 ){` |
|       ! 0 | 10608 | `		pRef->iFlags = iFlags;` |
|       ! 0 | 10609 | `	}` |
|    106280 | 10610 | `	if( pEntry ){` |
|         - | 10611 | `		/* The name bound to this slot */` |
|     28645 | 10612 | `		VmRefAddEntry(&(*pVm),pRef,pEntry);` |
|     14282 | 10613 | `	}` |
|    106280 | 10614 | `	if( pMapEntry ){` |
|         - | 10615 | `		/* The hashmap node [i.e: Array entry] pointing at it */` |
|     77640 | 10616 | `		VmRefAddNode(&(*pVm),pRef,pMapEntry);` |
|     38757 | 10617 | `	}` |
|    106280 | 10618 | `	return SXRET_OK;` |
|  14743146 | 10619 | `}` |
|         - | 10620 | `/*` |
|         - | 10621 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 10622 | ` *` |
|         - | 10623 | ` * The slot stays REGISTERED with no holders (a bare mark) rather than leaving the` |
|         - | 10624 | ` * table: that is what still returns the index to the free pool when the value goes.` |
|         - | 10625 | ` */` |
|  18312857 | 10626 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 10627 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 10628 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 10629 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 10630 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 10631 | `	)` |
|         5 | 10632 | `{` |
|         - | 10633 | `	VmRefObj *pRef;` |
|         - | 10634 | `	void *pWord;` |
|  18312862 | 10635 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  18312862 | 10636 | `	if( pWord == 0 ){` |
|         - | 10637 | `		/* Not such entry */` |
|         5 | 10638 | `		return SXERR_NOTFOUND;` |
|         - | 10639 | `	}` |
|  18312858 | 10640 | `	switch( VM_REF_TAGOF(pWord) ){` |
|    782631 | 10641 | `	case VM_REF_TAG_NAME:` |
|   1571451 | 10642 | `		if( pEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry ){` |
|   1571451 | 10643 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|    788815 | 10644 | `		}` |
|   1571451 | 10645 | `		return SXRET_OK;` |
|   8355223 | 10646 | `	case VM_REF_TAG_NODE:` |
|  16704046 | 10647 | `		if( pMapEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry ){` |
|  16704046 | 10648 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|   8348818 | 10649 | `		}` |
|  16704046 | 10650 | `		return SXRET_OK;` |
|       ! 0 | 10651 | `	case VM_REF_TAG_MARK:` |
|       ! 0 | 10652 | `		return SXRET_OK; /* nothing named to drop */` |
|     18695 | 10653 | `	default:` |
|     37366 | 10654 | `		break;` |
|         - | 10655 | `	}` |
|     37371 | 10656 | `	pRef = (VmRefObj *)pWord;` |
|         - | 10657 | `	/* Remove the desired entry */` |
|     37371 | 10658 | `	if( pEntry ){` |
|     20621 | 10659 | `		VmRefDropEntry(pRef,pEntry);` |
|     10301 | 10660 | `	}` |
|     37371 | 10661 | `	if( pMapEntry ){` |
|     16755 | 10662 | `		VmRefDropNode(pRef,pMapEntry);` |
|      8370 | 10663 | `	}` |
|     37371 | 10664 | `	return SXRET_OK;` |
|   9156311 | 10665 | `}` |
|         - | 10666 | `/*` |
|         - | 10667 | `` * Pin a slot past its frame: a `use (&$x)` capture, a static, an enum case, a`` |
|         - | 10668 | ` * reference-bound property. A pin is a holder the table cannot NAME.` |
|         - | 10669 | ` */` |
|      2262 | 10670 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10671 | `{` |
|         - | 10672 | `	void *pWord;` |
|         - | 10673 | `	VmRefObj *pRef;` |
|      2267 | 10674 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      2267 | 10675 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|      2267 | 10676 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|       159 | 10677 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(VmRefMarkPin(pWord),VM_REF_IDX_KEEP));` |
|       159 | 10678 | `		return;` |
|         - | 10679 | `	}` |
|      2111 | 10680 | `	if( pWord == 0 ){` |
|         - | 10681 | `		/* No record yet -- a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - | 10682 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        64 | 10683 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|        64 | 10684 | `		return;` |
|         - | 10685 | `	}` |
|      2051 | 10686 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|      2051 | 10687 | `	if( pRef ){` |
|      2051 | 10688 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|      1019 | 10689 | `	}` |
|      1132 | 10690 | `}` |
|         - | 10691 | `/*` |
|         - | 10692 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - | 10693 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - | 10694 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - | 10695 | ` */` |
|       210 | 10696 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         3 | 10697 | `{` |
|         - | 10698 | `	void *pWord;` |
|         - | 10699 | `	VmRefObj *pRef;` |
|       213 | 10700 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|       213 | 10701 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|       213 | 10702 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|       159 | 10703 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|       159 | 10704 | `		if( nPin < VM_REF_MARK_PINMAX ){` |
|       159 | 10705 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin + 1,VM_REF_IDX_KEEP));` |
|       159 | 10706 | `			return;` |
|         - | 10707 | `		}` |
|       ! 0 | 10708 | `	}` |
|        55 | 10709 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|        55 | 10710 | `	if( pRef == 0 && pWord != 0 ){` |
|       ! 0 | 10711 | `		pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|       ! 0 | 10712 | `	}` |
|        55 | 10713 | `	if( pRef ){` |
|        55 | 10714 | `		pRef->nPin++;` |
|        27 | 10715 | `	}` |
|       108 | 10716 | `}` |
|         - | 10717 | `/*` |
|         - | 10718 | ` * Give back a counted pin. The slot goes when it was the last holder -- without this the` |
|         - | 10719 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - | 10720 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - | 10721 | ` */` |
|       198 | 10722 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 10723 | `{` |
|       200 | 10724 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|         - | 10725 | `	VmRefObj *pRef;` |
|       200 | 10726 | `	if( pWord == 0 ){` |
|       ! 0 | 10727 | `		return;` |
|         - | 10728 | `	}` |
|       200 | 10729 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|        45 | 10730 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|        45 | 10731 | `		if( nPin < 1 ){` |
|       ! 0 | 10732 | `			return;` |
|         - | 10733 | `		}` |
|        45 | 10734 | `		nPin--;` |
|        45 | 10735 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin,nPin < 1 ? 0 : VM_REF_IDX_KEEP));` |
|        45 | 10736 | `		if( nPin < 1 ){` |
|        31 | 10737 | `			PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        15 | 10738 | `		}` |
|        45 | 10739 | `		return;` |
|         - | 10740 | `	}` |
|       156 | 10741 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|       156 | 10742 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 | 10743 | `		return;` |
|         - | 10744 | `	}` |
|       156 | 10745 | `	pRef->nPin--;` |
|       156 | 10746 | `	if( pRef->nPin < 1 ){` |
|       146 | 10747 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|       146 | 10748 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        72 | 10749 | `	}` |
|       101 | 10750 | `}` |
|         - | 10751 | `/*` |
|         - | 10752 | ` * Give up the OWNER's own hold on a slot, and say whether anybody else still has one.` |
|         - | 10753 | ` *` |
|         - | 10754 | `` * A property that is released -- with its object, or by `unset($o->p)` -- used to`` |
|         - | 10755 | ` * take its VALUE SLOT with it unconditionally, which is right only while the` |
|         - | 10756 | ` * property is the one thing naming it. php's refcount keeps the value alive for` |
|         - | 10757 | `` * whoever else holds a reference to it (`$r =& $o->p; unset($o->p);` leaves $r`` |
|         - | 10758 | `` * holding the value, and `$a[] =& $o->p` leaves the element), where unlinking the`` |
|         - | 10759 | ` * slot here dropped the array element and left the VARIABLE undefined.` |
|         - | 10760 | ` *` |
|         - | 10761 | ` * Returns TRUE when the caller must NOT free the slot. The owner's own hold is the` |
|         - | 10762 | ` * permanent pin a dynamically created / re-created property carries; a declared one` |
|         - | 10763 | ` * holds nothing at all, so there is nothing to give back for it.` |
|         - | 10764 | ` */` |
|  10110766 | 10765 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10766 | `{` |
|         - | 10767 | `	void *pWord;` |
|         - | 10768 | `	VmRefObj *pRef;` |
|  10110771 | 10769 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10770 | `		return 0;` |
|         - | 10771 | `	}` |
|  10110771 | 10772 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  10110771 | 10773 | `	if( pWord == 0 ){` |
|       ! 0 | 10774 | `		return 0;` |
|         - | 10775 | `	}` |
|  10110771 | 10776 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|  10110753 | 10777 | `		if( VmRefMarkPin(pWord) == 0 ){` |
|  10110753 | 10778 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|   5054660 | 10779 | `		}` |
|   5054674 | 10780 | `	}else if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|        19 | 10781 | `		pRef = (VmRefObj *)pWord;` |
|        19 | 10782 | `		if( pRef->nPin == 0 ){` |
|        19 | 10783 | `			pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|         9 | 10784 | `		}` |
|         9 | 10785 | `	}` |
|         - | 10786 | `	/* A word carrying a name or a node has no pin to give back; its holder is the` |
|         - | 10787 | `	 * one the count below reports. */` |
|  10110771 | 10788 | `	return PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0;` |
|   5054669 | 10789 | `}` |
|         - | 10790 | `/*` |
|         - | 10791 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 10792 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 10793 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 10794 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 10795 | ` */` |
|  16725899 | 10796 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 10797 | `{` |
|  16725904 | 10798 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 10799 | `		return;` |
|         - | 10800 | `	}` |
|  16725899 | 10801 | `	if( nIdx < pVm->nRefSize` |
|  16725904 | 10802 | `	 && SX_PTR_TO_INT(pVm->apRefObj[nIdx]) == VM_REF_TAG_MARK ){` |
|         - | 10803 | `		/* A bare mark (PH7_VmSlotDropIfBare): registered, unpinned, unheld. The three` |
|         - | 10804 | `		 * calls below would load this same word three more times to say so, and this` |
|         - | 10805 | `		 * is 99.2% of the door. Fall through to the release. */` |
|   8370609 | 10806 | `	}else if( PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|     21669 | 10807 | `		if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|       128 | 10808 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 10809 | `		}` |
|     21543 | 10810 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      5379 | 10811 | `			return; /* somebody still holds it */` |
|         - | 10812 | `		}` |
|      8077 | 10813 | `	}` |
|         - | 10814 | `	/* Nothing registered at all means nothing was ever recorded against the slot,` |
|         - | 10815 | `	 * which is the same answer as a count of zero — release it (this is what every` |
|         - | 10816 | `	 * caller did unconditionally before the holder rule). */` |
|  16720404 | 10817 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 10818 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 10819 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|  16720404 | 10820 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   8359747 | 10821 | `}` |
|         - | 10822 | `/*` |
|         - | 10823 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 10824 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 10825 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 10826 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 10827 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 10828 | ` */` |
|     16360 | 10829 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 10830 | `{` |
|         - | 10831 | `	VmFrame *pFrame;` |
|     39307 | 10832 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     22947 | 10833 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     22947 | 10834 | `		sxu32 n = 0;` |
|     44589 | 10835 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     21645 | 10836 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 10837 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 10838 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      4853 | 10839 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      4853 | 10840 | `				(void)SySetPop(&pFrame->sRef);` |
|      4853 | 10841 | `				continue;` |
|         - | 10842 | `			}` |
|     16794 | 10843 | `			n++;` |
|         2 | 10844 | `		}` |
|     11471 | 10845 | `	}` |
|     16365 | 10846 | `}` |
|         - | 10847 | `/*` |
|         - | 10848 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 10849 | ` *` |
|         - | 10850 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 10851 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 10852 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 10853 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 10854 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 10855 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 10856 | ` */` |
|     37343 | 10857 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 10858 | `{` |
|     37348 | 10859 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 10860 | `}` |
|         - | 10861 | `/*` |
|         - | 10862 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 10863 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 10864 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 10865 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 10866 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 10867 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 10868 | ` */` |
|      2972 | 10869 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 10870 | `	sxu32 nIdx)` |
|         5 | 10871 | `{` |
|      2977 | 10872 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|      2977 | 10873 | `	if( pEntry ){` |
|       116 | 10874 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|       116 | 10875 | `		return;` |
|         - | 10876 | `	}` |
|      2865 | 10877 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 10878 | `		return;` |
|         - | 10879 | `	}` |
|         - | 10880 | `	/* The name may already be memoized against the slot it had before this frame` |
|         - | 10881 | ``	 * installed it -- a by-reference `foreach` re-binds its value variable on every`` |
|         - | 10882 | `	 * step, and the first step is an INSERT. */` |
|      2865 | 10883 | `	VmVarMemoFlush(pFrame);` |
|      2865 | 10884 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 10885 | `		/* A global is also an entry of the $GLOBALS view */` |
|        42 | 10886 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 10887 | `	}` |
|      2865 | 10888 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      1489 | 10889 | `}` |
|         - | 10890 | `/*` |
|         - | 10891 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 10892 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 10893 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 10894 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 10895 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 10896 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 10897 | ` * the wrong variable.` |
|         - | 10898 | ` *` |
|         - | 10899 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 10900 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 10901 | ` * compares it by identity.` |
|         - | 10902 | ` */` |
|      4840 | 10903 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 10904 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 10905 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 10906 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 10907 | `	const char *zName,    /* Variable name */` |
|         - | 10908 | `	sxu32 nByte,          /* Name length */` |
|         - | 10909 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 10910 | `	)` |
|         5 | 10911 | `{` |
|      4845 | 10912 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      4845 | 10913 | `	if( nOld == nIdx ){` |
|         - | 10914 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|         5 | 10915 | `		return;` |
|         - | 10916 | `	}` |
|         - | 10917 | `	/* This name now means another slot; every memo this frame holds goes. */` |
|      4840 | 10918 | `	VmVarMemoFlush(pFrame);` |
|         - | 10919 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 10920 | `	 * "release this reference at exit" set */` |
|      4840 | 10921 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      4840 | 10922 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      4840 | 10923 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      4840 | 10924 | `	if( pFrame->pParent == 0 ){` |
|         - | 10925 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 10926 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 10927 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 10928 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       150 | 10929 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       146 | 10930 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       150 | 10931 | `		 && pGlobalNode ){` |
|       150 | 10932 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       150 | 10933 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       150 | 10934 | `				pGlobalNode->nValIdx = nIdx;` |
|       150 | 10935 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        73 | 10936 | `			}` |
|        77 | 10937 | `		}else{` |
|         - | 10938 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 10939 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 10940 | `		}` |
|        73 | 10941 | `	}` |
|      4840 | 10942 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 10943 | `	/* The old value dies with its last holder — and only then */` |
|      4840 | 10944 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      2425 | 10945 | `}` |
|         - | 10946 | `/*` |
|         - | 10947 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|         - | 10948 | ` *` |
|         - | 10949 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|         - | 10950 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|         - | 10951 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|         - | 10952 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|         - | 10953 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|         - | 10954 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|         - | 10955 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|         - | 10956 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|         - | 10957 | ` */` |
|    313346 | 10958 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|         5 | 10959 | `{` |
|    313351 | 10960 | `	int i = 0;` |
|   1701258 | 10961 | `	while( i < nByte ){` |
|   1701058 | 10962 | `		int c = zIn[i];` |
|   1701053 | 10963 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|    315088 | 10964 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|   1387912 | 10965 | `			i++;` |
|   1387912 | 10966 | `			continue;` |
|         - | 10967 | `		}` |
|    313151 | 10968 | `		break;` |
|       ! 0 | 10969 | `	}` |
|         - | 10970 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|         - | 10971 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|    313351 | 10972 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|      2893 | 10973 | `		*pnScheme = i;` |
|      2893 | 10974 | `		return 1;` |
|         - | 10975 | `	}` |
|    310463 | 10976 | `	return 0;` |
|    156715 | 10977 | `}` |
|         - | 10978 | `/*` |
|         - | 10979 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|         - | 10980 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|         - | 10981 | ` * is walkable at all.` |
|         - | 10982 | ` */` |
|        82 | 10983 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|         4 | 10984 | `{` |
|        86 | 10985 | `	int nScheme = 0;` |
|        86 | 10986 | `	if( zIn == 0 ){` |
|       ! 0 | 10987 | `		return 0;` |
|         - | 10988 | `	}` |
|        86 | 10989 | `	if( nByte < 0 ){` |
|        55 | 10990 | `		nByte = (int)SyStrlen(zIn);` |
|        22 | 10991 | `	}` |
|        86 | 10992 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|        41 | 10993 | `}` |
|         - | 10994 | `/*` |
|         - | 10995 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|         - | 10996 | ` *` |
|         - | 10997 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|         - | 10998 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|         - | 10999 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|         - | 11000 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|         - | 11001 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|         - | 11002 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|         - | 11003 | `` * `file://` on its own is the root directory.`` |
|         - | 11004 | ` *` |
|         - | 11005 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|         - | 11006 | ` * wrapper", as php does.` |
|         - | 11007 | ` */` |
|       156 | 11008 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|         4 | 11009 | `{` |
|         - | 11010 | `	static const char zLocal[] = "file://localhost/";` |
|       160 | 11011 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|       160 | 11012 | `	const char *zEnd = &zIn[nByte];` |
|       156 | 11013 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|         - | 11014 | `#ifdef __WINNT__` |
|         - | 11015 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|         4 | 11016 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|         - | 11017 | `#endif` |
|         - | 11018 | `	){` |
|        94 | 11019 | `		if( nByte < (int)sizeof(zLocal)-1` |
|        96 | 11020 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|        92 | 11021 | `			return 0; /* a host this build (and php) will not fetch from */` |
|         - | 11022 | `		}` |
|         4 | 11023 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|         2 | 11024 | `	}` |
|       191 | 11025 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|       125 | 11026 | `		zPath++;` |
|         3 | 11027 | `	}` |
|        69 | 11028 | `	*pzPath = zPath;` |
|        69 | 11029 | `	return 1;` |
|        82 | 11030 | `}` |
|         - | 11031 | `/*` |
|         - | 11032 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|         - | 11033 | ` * going through a stream device. It carried a second, shorter copy of the` |
|         - | 11034 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|         - | 11035 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|         - | 11036 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|         - | 11037 | ` * that is not a path, which is the FALSE php answers for it.` |
|         - | 11038 | ` */` |
|     94275 | 11039 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|         5 | 11040 | `{` |
|         - | 11041 | `	const char *zOut;` |
|     94280 | 11042 | `	int nByte,nScheme = 0;` |
|     94280 | 11043 | `	if( zPath == 0 ){` |
|       ! 0 | 11044 | `		return zPath;` |
|         - | 11045 | `	}` |
|     94280 | 11046 | `	nByte = (int)SyStrlen(zPath);` |
|     94275 | 11047 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|     47351 | 11048 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|        78 | 11049 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|     94260 | 11050 | `		return zPath;` |
|         - | 11051 | `	}` |
|        22 | 11052 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|         3 | 11053 | `		return zPath;` |
|         - | 11054 | `	}` |
|         - | 11055 | `#ifdef __WINNT__` |
|         - | 11056 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|         - | 11057 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|         - | 11058 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|         2 | 11059 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|         2 | 11060 | `		zOut++;` |
|         - | 11061 | `	}` |
|         - | 11062 | `#endif` |
|        20 | 11063 | `	return zOut;` |
|     47298 | 11064 | `}` |
|         - | 11065 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 11066 | `/*` |
|         - | 11067 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|         - | 11068 | ` */` |
|    219783 | 11069 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|         5 | 11070 | `{` |
|    219788 | 11071 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|         - | 11072 | `	sxu32 n;` |
|    222160 | 11073 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      2432 | 11074 | `		if( apOff[n] == pStream ){` |
|        58 | 11075 | `			return 1;` |
|         - | 11076 | `		}` |
|      1190 | 11077 | `	}` |
|    219732 | 11078 | `	return 0;` |
|    109777 | 11079 | `}` |
|         - | 11080 | `/*` |
|         - | 11081 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|         - | 11082 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|         - | 11083 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|         - | 11084 | ` * sit in the list, and the LIVE one is the later of the two.` |
|         - | 11085 | ` */` |
|    218759 | 11086 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|         5 | 11087 | `{` |
|    218764 | 11088 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|    218764 | 11089 | `	ph7_io_stream *pHit = 0;` |
|    218764 | 11090 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|    218764 | 11091 | `	if( nName < 0 ){` |
|       ! 0 | 11092 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 11093 | `	}` |
|   2409796 | 11094 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|   2191037 | 11095 | `		ph7_io_stream *pStream = apStream[n];` |
|   2191032 | 11096 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|   1640231 | 11097 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|   1972418 | 11098 | `			continue;` |
|         - | 11099 | `		}` |
|    218624 | 11100 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        19 | 11101 | `			continue;` |
|         - | 11102 | `		}` |
|    218606 | 11103 | `		pHit = pStream;` |
|    109191 | 11104 | `	}` |
|    218764 | 11105 | `	return pHit;` |
|         5 | 11106 | `}` |
|         - | 11107 | `/*` |
|         - | 11108 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|         - | 11109 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|         - | 11110 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|         - | 11111 | ` */` |
|        90 | 11112 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|         4 | 11113 | `{` |
|        94 | 11114 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|        94 | 11115 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|        94 | 11116 | `	int bSeen = 0;` |
|        94 | 11117 | `	if( nName < 0 ){` |
|       ! 0 | 11118 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 11119 | `	}` |
|       820 | 11120 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|       818 | 11121 | `		ph7_io_stream *pStream = apStream[n];` |
|       814 | 11122 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|       636 | 11123 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|       728 | 11124 | `			continue;` |
|         - | 11125 | `		}` |
|        94 | 11126 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        92 | 11127 | `			return 0; /* it is live */` |
|         - | 11128 | `		}` |
|         3 | 11129 | `		bSeen = 1;` |
|         2 | 11130 | `	}` |
|         3 | 11131 | `	return bSeen;` |
|        49 | 11132 | `}` |
|         - | 11133 | `/*` |
|         - | 11134 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 11135 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 11136 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 11137 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 11138 | ` * For more information on how to register IO stream devices,please` |
|         - | 11139 | ` * refer to the official documentation.` |
|         - | 11140 | ` */` |
|    218715 | 11141 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 11142 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 11143 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 11144 | `	int nByte              /* *pzDevice length*/` |
|         - | 11145 | `	)` |
|         5 | 11146 | `{` |
|         - | 11147 | `	const char *zIn,*zNext;` |
|         - | 11148 | `	ph7_io_stream *pStream;` |
|    218720 | 11149 | `	int nScheme = 0;` |
|         - | 11150 | `	/* A failed open names the URI the SCRIPT wrote, and every caller from here` |
|         - | 11151 | `	 * on holds only what is left after the scheme -- so both halves are` |
|         - | 11152 | `	 * remembered as the scheme comes off, and forgotten on every arm that does` |
|         - | 11153 | `	 * not take one off (see VfsThrowOpenWarning). */` |
|    218720 | 11154 | `	if( pVm->nOpenDepth < 1 ){` |
|    218572 | 11155 | `		pVm->zOpenUri = 0;` |
|    218572 | 11156 | `		pVm->zOpenUriTail = 0;` |
|    218572 | 11157 | `		pVm->nOpenUri = 0;` |
|         - | 11158 | `		/* The reason goes with them: a caller that resolves a device and then` |
|         - | 11159 | `		 * declines to open it (dom asks for xRead/xWrite first) must not report` |
|         - | 11160 | `		 * the PREVIOUS open's wrapper reason. */` |
|    218572 | 11161 | `		pVm->zOpenErr = 0;` |
|    109169 | 11162 | `	}` |
|         - | 11163 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    218720 | 11164 | `	zIn = *pzDevice;` |
|    218720 | 11165 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|         - | 11166 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|         - | 11167 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|         - | 11168 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|         - | 11169 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|         - | 11170 | `		 * than answering pDefStream is what makes both true. */` |
|    216228 | 11171 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|         - | 11172 | `	}` |
|      2497 | 11173 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|         - | 11174 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|         - | 11175 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|         - | 11176 | `	 * file:// is handed the stripped path too. */` |
|      2497 | 11177 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|        95 | 11178 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|        45 | 11179 | `			return 0;` |
|         - | 11180 | `		}` |
|        24 | 11181 | `	}` |
|      2453 | 11182 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|      2453 | 11183 | `	if( pStream == 0 ){` |
|         - | 11184 | `		/* No such stream -- or one a script has taken out of service. */` |
|       161 | 11185 | `		return 0;` |
|         - | 11186 | `	}` |
|      2297 | 11187 | `	*pzDevice = zNext;` |
|      2297 | 11188 | `	if( pVm->nOpenDepth < 1 ){` |
|      2295 | 11189 | `		pVm->zOpenUri = zIn;` |
|      2295 | 11190 | `		pVm->nOpenUri = nByte;` |
|      2295 | 11191 | `		pVm->zOpenUriTail = zNext;` |
|      1128 | 11192 | `	}` |
|      2297 | 11193 | `	return pStream;` |
|    109248 | 11194 | `}` |
|         - | 11195 | `/*` |
|         - | 11196 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|         - | 11197 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|         - | 11198 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|         - | 11199 | ` * threaded out of the lookup, so every call site stays one line.` |
|         - | 11200 | ` *` |
|         - | 11201 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|         - | 11202 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|         - | 11203 | ` */` |
|       274 | 11204 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|         5 | 11205 | `{` |
|         - | 11206 | `	const char *zPath;` |
|       279 | 11207 | `	int nScheme = 0;` |
|       279 | 11208 | `	if( nByte < 0 ){` |
|       151 | 11209 | `		nByte = (int)SyStrlen(zUri);` |
|        73 | 11210 | `	}` |
|       279 | 11211 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|         5 | 11212 | `		*pnScheme = 0;` |
|         5 | 11213 | `		return 0;` |
|         - | 11214 | `	}` |
|       275 | 11215 | `	*pnScheme = nScheme;` |
|       299 | 11216 | `	return nScheme == (int)sizeof("file")-1` |
|       159 | 11217 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|       294 | 11218 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|       142 | 11219 | `}` |
|         - | 11220 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 11221 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 11222 |  |
