# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4077/4665 lines (87.40%)

[Root index](../../index.md) | [Directory index](index.md)

|      Hits | Line | Source |
| --------: | ---: | :--- |
|         - |    1 | `/**` |
|         - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|         - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|         - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|         - |    5 | ` */` |
|         - |    6 | `#include "ph7int.h"` |
|         - |    7 | `#include <stddef.h>` |
|         - |    8 | `#include <stdlib.h>` |
|         - |    9 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|         - |   10 | `#include <math.h>` |
|         - |   11 | `#endif` |
|         - |   12 | `/* Signed 64-bit integer overflow detection lives in ph7int.h as the shared` |
|         - |   13 | ` * PH7_{ADD,SUB,MUL}_OVERFLOW64 macros (GCC/Clang intrinsics, MSVC fallbacks in` |
|         - |   14 | ` * memobj.c). The executor uses them to promote an overflowing integer` |
|         - |   15 | ` * operation to a float, matching PHP. */` |
|         - |   16 | `/*` |
|         - |   17 | ` * The code in this file implements execution method of the PH7 Virtual Machine.` |
|         - |   18 | ` * The PH7 compiler (implemented in 'compiler.c' and 'parse.c') generates a bytecode program` |
|         - |   19 | ` * which is then executed by the virtual machine implemented here to do the work of the PHP` |
|         - |   20 | ` * statements.` |
|         - |   21 | ` * PH7 bytecode programs are similar in form to assembly language. The program consists` |
|         - |   22 | ` * of a linear sequence of operations .Each operation has an opcode and 3 operands.` |
|         - |   23 | ` * Operands P1 and P2 are integers where the first is signed while the second is unsigned.` |
|         - |   24 | ` * Operand P3 is an arbitrary pointer specific to each instruction. The P2 operand is usually` |
|         - |   25 | ` * the jump destination used by the OP_JMP,OP_JZ,OP_JNZ,... instructions.` |
|         - |   26 | ` * Opcodes will typically ignore one or more operands. Many opcodes ignore all three operands.` |
|         - |   27 | ` * Computation results are stored on a stack. Each entry on the stack is of type ph7_value.` |
|         - |   28 | ` * PH7 uses the ph7_value object to represent all values that can be stored in a PHP variable.` |
|         - |   29 | ` * Since PHP uses dynamic typing for the values it stores. Values stored in ph7_value objects` |
|         - |   30 | ` * can be integers,floating point values,strings,arrays,class instances (object in the PHP jargon)` |
|         - |   31 | ` * and so on.` |
|         - |   32 | ` * Internally,the PH7 virtual machine manipulates nearly all PHP values as ph7_values structures.` |
|         - |   33 | ` * Each ph7_value may cache multiple representations(string,integer etc.) of the same value.` |
|         - |   34 | ` * An implicit conversion from one type to the other occurs as necessary.` |
|         - |   35 | ` * Most of the code in this file is taken up by the [VmByteCodeExec()] function which does` |
|         - |   36 | ` * the work of interpreting a PH7 bytecode program. But other routines are also provided` |
|         - |   37 | ` * to help in building up a program instruction by instruction. Also note that sepcial` |
|         - |   38 | ` * functions that need access to the underlying virtual machine details such as [die()],` |
|         - |   39 | ` * [func_get_args()],[call_user_func()],[ob_start()] and many more are implemented here.` |
|         - |   40 | ` */` |
|         - |   41 | `/* VmFrame struct and VM_FRAME_* defines moved to ph7int.h */` |
|         - |   42 | `/*` |
|         - |   43 | ` * When a user defined variable is released (via manual unset($x) or garbage collected)` |
|         - |   44 | ` * memory object index is stored in an instance of the following structure and put` |
|         - |   45 | ` * in the free object table so that it can be reused again without allocating` |
|         - |   46 | ` * a new memory object.` |
|         - |   47 | ` */` |
|         - |   48 | `/* VmSlot struct moved to ph7int.h */` |
|         - |   49 | `/*` |
|         - |   50 | ` * An entry in the reference table is represented by an instance of the` |
|         - |   51 | ` * follwoing table.` |
|         - |   52 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - |   53 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - |   54 | ` * the reference implementation is consistent,solid and it's` |
|         - |   55 | ` * behavior resemble the C++ reference mechanism.` |
|         - |   56 | ` * Refer to the official for more information on this powerful` |
|         - |   57 | ` * extension.` |
|         - |   58 | ` */` |
|         - |   59 | `/* struct VmRefObj + VM_REF_IDX_KEEP moved to ph7int.h */` |
|         - |   60 | `/*` |
|         - |   61 | ` * Each installed shutdown callback (registered using [register_shutdown_function()] )` |
|         - |   62 | ` * is stored in an instance of the following structure.` |
|         - |   63 | ` * Refer to the implementation of [register_shutdown_function(()] for more information.` |
|         - |   64 | ` */` |
|         - |   65 | `/* VmShutdownCB struct moved to ph7int.h */` |
|         - |   66 | `/*` |
|         - |   67 | ` * Each installed autoload callback (registered using [spl_autoload_register()] )` |
|         - |   68 | ` * is stored in an instance of the following structure.` |
|         - |   69 | ` * Refer to the implementation of [spl_autoload_register()] for more information.` |
|         - |   70 | ` */` |
|         - |   71 | `/* VmAutoloadCB struct moved to ph7int.h */` |
|         - |   72 |  |
|         - |   73 | `/*` |
|         - |   74 | ` * TRUE when php compares these two operands as UNORDERED -- a NaN against` |
|         - |   75 | ` * something php reads as a NUMBER or as a STRING. php answers 1 for that` |
|         - |   76 | `` * comparison in BOTH directions, which is what makes `==`, `<`, `>`, `<=` and`` |
|         - |   77 | `` * `>=` all false at once while `<=>` is 1 either way round.`` |
|         - |   78 | ` *` |
|         - |   79 | ` * Two rules ride on this predicate and both were wrong without them.` |
|         - |   80 | ` *` |
|         - |   81 | ` * It must be asked BEFORE PH7_MemObjCmp runs: the comparator converts its` |
|         - |   82 | ` * operands IN PLACE, so a NaN that took the string path is a MEMOBJ_STRING by` |
|         - |   83 | ` * the time the answer comes back and the float is gone. That is how` |
|         - |   84 | `` * `NAN == "NAN"` was TRUE here (php: false) and `NAN < "abc"` was TRUE`` |
|         - |   85 | ` * (php: false) -- the screen ran on two strings and saw no NaN at all.` |
|         - |   86 | ` *` |
|         - |   87 | ` * And php's own precedence comes FIRST: a comparison against null, a bool, an` |
|         - |   88 | ``  * array or an object never reaches the numeric/string rule, so `NAN == true` `` |
|         - |   89 | `` * is TRUE (both truthy) and `NAN < []` is TRUE (an array is greater). Those`` |
|         - |   90 | ` * flags are exactly the branches PH7_MemObjCmp answers ahead of its numeric` |
|         - |   91 | ` * one. A RESOURCE is not among them: php reads it as its ID there, so a NaN` |
|         - |   92 | ` * against one is as unordered as a NaN against any other number.` |
|         - |   93 | ` */` |
|   6672854 |   94 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   95 | `{` |
|   6672854 |   96 | `	if( (pLeft->iFlags \| pRight->iFlags)` |
|   6672859 |   97 | `	  & (MEMOBJ_NULL\|MEMOBJ_BOOL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ) ){` |
|       ! 0 |   98 | `		return FALSE;` |
|         - |   99 | `	}` |
|   6672859 |  100 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       334 |  101 | `		return TRUE;` |
|         - |  102 | `	}` |
|   6672527 |  103 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       145 |  104 | `		return TRUE;` |
|         - |  105 | `	}` |
|   6672383 |  106 | `	return FALSE;` |
|   3342129 |  107 | `}` |
|         - |  108 | `/*` |
|         - |  109 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |  110 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |  111 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |  112 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |  113 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |  114 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |  115 | ` * still go through the existing numeric coercion.` |
|         - |  116 | ` */` |
|    801194 |  117 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |  118 | `{` |
|         - |  119 | `	SyString sStr;` |
|    801199 |  120 | `	sxu8 bReal = FALSE;` |
|    801199 |  121 | `	const char *zTail = 0;` |
|         - |  122 | `	const char *zEnd;` |
|    801199 |  123 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    801181 |  124 | `		return FALSE;` |
|         - |  125 | `	}` |
|        21 |  126 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        21 |  127 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |  128 | `		return TRUE;` |
|         - |  129 | `	}` |
|        21 |  130 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |  131 | `		return TRUE;` |
|         - |  132 | `	}` |
|         - |  133 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |  134 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |  135 | `	 * still takes the Perl path. */` |
|        17 |  136 | `	zEnd = sStr.zString + sStr.nByte;` |
|        17 |  137 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |  138 | `		zTail++;` |
|       ! 0 |  139 | `	}` |
|        17 |  140 | `	return zTail < zEnd;` |
|    401355 |  141 | `}` |
|         - |  142 | `/* SyhttpUri, SyhttpHeader and HTTP method/protocol defines moved to ph7int.h */` |
|         - |  143 | `/* Constant expander used by define(); used below to recognise user-defined` |
|         - |  144 | ` * (vs. host/built-in) constants so their owned value object can be freed when` |
|         - |  145 | ` * a define() overwrites them. */` |
|         - |  146 | `/*` |
|         - |  147 | ` * Register a constant and it's associated expansion callback so that` |
|         - |  148 | ` * it can be expanded from the target PHP program.` |
|         - |  149 | ` * The constant expansion mechanism under PH7 is extremely powerful yet` |
|         - |  150 | ` * simple and work as follows:` |
|         - |  151 | ` * Each registered constant have a C procedure associated with it.` |
|         - |  152 | ` * This procedure known as the constant expansion callback is responsible` |
|         - |  153 | ` * of expanding the invoked constant to the desired value,for example:` |
|         - |  154 | ` * The C procedure associated with the "__PI__" constant expands to 3.14 (the value of PI).` |
|         - |  155 | ` * The "__OS__" constant procedure expands to the name of the host Operating Systems` |
|         - |  156 | ` * (Windows,Linux,...) and so on.` |
|         - |  157 | ` * Please refer to the official documentation for additional information.` |
|         - |  158 | ` */` |
|  10762749 |  159 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  160 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  161 | `	const SyString *pName,  /* Constant name */` |
|         - |  162 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  163 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  164 | `	)` |
|         5 |  165 | `{` |
|  10762754 |  166 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  167 | `}` |
|         - |  168 | `/*` |
|         - |  169 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  170 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  171 | ` */` |
|  10763087 |  172 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
|         - |  173 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  174 | `	const SyString *pName,  /* Constant name */` |
|         - |  175 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  176 | `	void *pUserData,        /* Last argument to xExpand() */` |
|         - |  177 | `	const SyString *pFile,  /* Defining file (VM-lifetime buffer) or NULL */` |
|         - |  178 | `	sxu32 nLine,            /* Declaration line, 0 = unknown */` |
|         - |  179 | `	int bUser               /* 1 when defined by user code */` |
|         - |  180 | `	)` |
|         5 |  181 | `{` |
|         - |  182 | `	ph7_constant *pCons;` |
|         - |  183 | `	SyHashEntry *pEntry;` |
|         - |  184 | `	char *zDupName;` |
|         - |  185 | `	sxi32 rc;` |
|  10763092 |  186 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|  10763092 |  187 | `	if( pEntry ){` |
|         - |  188 | `		/* Overwrite the old definition and return immediately */` |
|         3 |  189 | `		pCons = (ph7_constant *)pEntry->pUserData;` |
|         - |  190 | `		/* A user-defined (define()) constant owns a heap ph7_value as its` |
|         - |  191 | `		 * pUserData; free it before overwriting so repeated define()s — e.g.` |
|         - |  192 | `		 * the same script re-run on a reused VM — don't leak the old value. */` |
|         2 |  193 | `		if( pCons->xExpand == VmExpandUserConstant && pCons->pUserData` |
|         3 |  194 | `		 && pCons->pUserData != pUserData ){` |
|         3 |  195 | `			PH7_MemObjRelease((ph7_value *)pCons->pUserData);` |
|         3 |  196 | `			SyMemBackendPoolFree(&pVm->sAllocator,pCons->pUserData);` |
|         1 |  197 | `		}` |
|         3 |  198 | `		pCons->xExpand = xExpand;` |
|         3 |  199 | `		pCons->pUserData = pUserData;` |
|         3 |  200 | `		if( pFile ){` |
|         3 |  201 | `			SyStringDupPtr(&pCons->sFile,pFile);` |
|         2 |  202 | `		}else{` |
|       ! 0 |  203 | `			SyStringInitFromBuf(&pCons->sFile,0,0);` |
|         - |  204 | `		}` |
|         3 |  205 | `		pCons->nLine = nLine;` |
|         3 |  206 | `		pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         3 |  207 | `		pCons->zDeprecated = 0;     /* ...and its deprecation, which was the old symbol's */` |
|         3 |  208 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|         3 |  209 | `		return SXRET_OK;` |
|         - |  210 | `	}` |
|         - |  211 | `	/* Allocate a new constant instance */` |
|  10763090 |  212 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|  10763090 |  213 | `	if( pCons == 0 ){` |
|       ! 0 |  214 | `		return 0;` |
|         - |  215 | `	}` |
|         - |  216 | `	/* Duplicate constant name */` |
|  10763090 |  217 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  10763090 |  218 | `	if( zDupName == 0 ){` |
|       ! 0 |  219 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  220 | `		return 0;` |
|         - |  221 | `	}` |
|  10763090 |  222 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|  10763090 |  223 | `	if( pFile ){` |
|       341 |  224 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       168 |  225 | `	}` |
|  10763090 |  226 | `	pCons->nLine = nLine;` |
|  10763090 |  227 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|  10763090 |  228 | `	pCons->zDeprecated = 0;` |
|         - |  229 | `	/* Install the constant */` |
|  10763090 |  230 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|  10763090 |  231 | `	pCons->xExpand = xExpand;` |
|  10763090 |  232 | `	pCons->pUserData = pUserData;` |
|  10763090 |  233 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|  10763090 |  234 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|         - |  235 | `	/* A name that was not a constant is one now, so every PH7_OP_LOADC site that` |
|         - |  236 | `	 * remembers what its name resolved to has to ask again -- including one whose` |
|         - |  237 | `	 * namespaced candidate used to MISS and fall through to the global literal. */` |
|  10763090 |  238 | `	pVm->nConstGen++;` |
|  10763090 |  239 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  240 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  241 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  242 | `		return rc;` |
|         - |  243 | `	}` |
|         - |  244 | `	/* All done,constant can be invoked from PHP code */` |
|  10763090 |  245 | `	return SXRET_OK;` |
|   5214228 |  246 | `}` |
|         - |  247 | `/*` |
|         - |  248 | ` * Allocate a new foreign function instance.` |
|         - |  249 | ` * This function return SXRET_OK on success. Any other` |
|         - |  250 | ` * return value indicates failure.` |
|         - |  251 | ` * Please refer to the official documentation for an introduction to` |
|         - |  252 | ` * the foreign function mechanism.` |
|         - |  253 | ` */` |
|  15940363 |  254 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|         - |  255 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  256 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  257 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  258 | `	void *pUserData,          /* Foreign function private data */` |
|         - |  259 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|         - |  260 | `	)` |
|         5 |  261 | `{` |
|         - |  262 | `	ph7_user_func *pFunc;` |
|         - |  263 | `	char *zDup;` |
|         - |  264 | `	/* Allocate a new user function */` |
|  15940368 |  265 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|  15940368 |  266 | `	if( pFunc == 0 ){` |
|       ! 0 |  267 | `		return SXERR_MEM;` |
|         - |  268 | `	}` |
|         - |  269 | `	/* Duplicate function name */` |
|  15940368 |  270 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|  15940368 |  271 | `	if( zDup == 0 ){` |
|       ! 0 |  272 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  273 | `		return SXERR_MEM;` |
|         - |  274 | `	}` |
|         - |  275 | `	/* Zero the structure */` |
|  15940368 |  276 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  277 | `	/* Initialize structure fields */` |
|  15940368 |  278 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|  15940368 |  279 | `	pFunc->pVm   = pVm;` |
|  15940368 |  280 | `	pFunc->xFunc = xFunc;` |
|  15940368 |  281 | `	pFunc->pUserData = pUserData;` |
|  15940368 |  282 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  283 | `	/* Write a pointer to the new function */` |
|  15940368 |  284 | `	*ppOut = pFunc;` |
|  15940368 |  285 | `	return SXRET_OK;` |
|   7950490 |  286 | `}` |
|         - |  287 | `/*` |
|         - |  288 | ` * Install a foreign function and it's associated callback so that` |
|         - |  289 | ` * it can be invoked from the target PHP code.` |
|         - |  290 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  291 | ` * return value indicates failure.` |
|         - |  292 | ` * Please refer to the official documentation for an introduction to` |
|         - |  293 | ` * the foreign function mechanism.` |
|         - |  294 | ` */` |
|   6510534 |  295 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|         - |  296 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  297 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  298 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  299 | `	void *pUserData           /* Foreign function private data */` |
|         - |  300 | `	)` |
|         5 |  301 | `{` |
|         - |  302 | `	ph7_user_func *pFunc;` |
|         - |  303 | `	SyHashEntry *pEntry;` |
|         - |  304 | `	sxi32 rc;` |
|         - |  305 | `	/* Overwrite any previously registered function with the same name */` |
|   6510539 |  306 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   6510539 |  307 | `	if( pEntry ){` |
|       ! 0 |  308 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|       ! 0 |  309 | `		pFunc->pUserData = pUserData;` |
|       ! 0 |  310 | `		pFunc->xFunc = xFunc;` |
|       ! 0 |  311 | `		SySetReset(&pFunc->aAux);` |
|         - |  312 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  313 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  314 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  315 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|       ! 0 |  316 | `		pFunc->nMinArg  = 0;` |
|       ! 0 |  317 | `		pFunc->nMaxArg  = 0;` |
|       ! 0 |  318 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|       ! 0 |  319 | `		pFunc->bAtLeast = 0;` |
|       ! 0 |  320 | `		return SXRET_OK;` |
|         - |  321 | `	}` |
|         - |  322 | `	/* Create a new user function */` |
|   6510539 |  323 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   6510539 |  324 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  325 | `		return rc;` |
|         - |  326 | `	}` |
|         - |  327 | `	/* Install the function in the corresponding hashtable */` |
|   6510539 |  328 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   6510539 |  329 | `	pVm->nCallableGen++; /* a name that was not callable may be now (OP_CALL_INIT) */` |
|   6510539 |  330 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  331 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  332 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  333 | `		return rc;` |
|         - |  334 | `	}` |
|         - |  335 | `	/* User function successfully installed */` |
|   6510539 |  336 | `	return SXRET_OK;` |
|   3241889 |  337 | `}` |
|         - |  338 | `/*` |
|         - |  339 | ` * Initialize a VM function.` |
|         - |  340 | ` */` |
|   9601044 |  341 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  342 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  343 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  344 | `	const char *zName,  /* Function name */` |
|         - |  345 | `	sxu32 nByte,        /* zName length */` |
|         - |  346 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  347 | `	void *pUserData     /* Function private data */` |
|         - |  348 | `	)` |
|         5 |  349 | `{` |
|         - |  350 | `	/* Zero the structure */` |
|   9601049 |  351 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  352 | `	/* Initialize structure fields */` |
|         - |  353 | `	/* Arguments container */` |
|   9601049 |  354 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  355 | `	/* Static variable container */` |
|   9601049 |  356 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  357 | `	/* Bytecode container */` |
|   9601049 |  358 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  359 | `    /* Preallocate some instruction slots */` |
|   9601049 |  360 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  361 | `	/* Closure environment */` |
|   9601049 |  362 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  363 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   9601049 |  364 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  365 | `	/* Declared #[...] attributes */` |
|   9601049 |  366 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   9601049 |  367 | `	pFunc->iFlags = iFlags;` |
|   9601049 |  368 | `	pFunc->pUserData = pUserData;` |
|         - |  369 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  370 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   9601049 |  371 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   9601049 |  372 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  373 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   9577430 |  374 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   4782305 |  375 | `	}else{` |
|         - |  376 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  377 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  378 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     23624 |  379 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     23624 |  380 | `		if( pFile ){` |
|     23624 |  381 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|     11689 |  382 | `		}` |
|         - |  383 | `	}` |
|   9601049 |  384 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   9601049 |  385 | `	return SXRET_OK;` |
|         5 |  386 | `}` |
|         - |  387 | `/*` |
|         - |  388 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|         - |  389 | ` *` |
|         - |  390 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|         - |  391 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|         - |  392 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|         - |  393 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|         - |  394 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|         - |  395 | ` * one.` |
|         - |  396 | ` *` |
|         - |  397 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|         - |  398 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|         - |  399 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|         - |  400 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|         - |  401 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|         - |  402 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|         - |  403 | ` * READ in VmByteCodeExecBody).` |
|         - |  404 | ` *` |
|         - |  405 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|         - |  406 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|         - |  407 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|         - |  408 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|         - |  409 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|         - |  410 | ` * wrote ever passes 1.` |
|         - |  411 | ` */` |
|   3997733 |  412 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|         - |  413 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  414 | `	const void *pName,  /* Function name */` |
|         - |  415 | `	sxu32 nByte,        /* Name length */` |
|         - |  416 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |  417 | `	)` |
|         5 |  418 | `{` |
|   3997738 |  419 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|   3997738 |  420 | `	if( pEntry && !bEngineName ){` |
|     54886 |  421 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     54886 |  422 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|        25 |  423 | `			return 0;` |
|         - |  424 | `		}` |
|     27146 |  425 | `	}` |
|   3997714 |  426 | `	return pEntry;` |
|   1997622 |  427 | `}` |
|         - |  428 | `/*` |
|         - |  429 | ` * The one copy of a callee name that every call site spelling it shares, made on first` |
|         - |  430 | ` * demand. 0 when it cannot be made, which just costs the caller its cache.` |
|         - |  431 | ` */` |
|     17405 |  432 | `static const char * VmCallNameIntern(ph7_vm *pVm,const SyString *pName)` |
|         5 |  433 | `{` |
|     17410 |  434 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hCallName,pName->zString,pName->nByte);` |
|         - |  435 | `	char *zCopy;` |
|     17410 |  436 | `	if( pEntry ){` |
|     10935 |  437 | `		return (const char *)pEntry->pKey;` |
|         - |  438 | `	}` |
|      6480 |  439 | `	zCopy = (char *)SyMemBackendDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      6480 |  440 | `	if( zCopy == 0 ){` |
|       ! 0 |  441 | `		return 0;` |
|         - |  442 | `	}` |
|      6480 |  443 | `	if( SyHashInsert(&pVm->hCallName,zCopy,pName->nByte,zCopy) != SXRET_OK ){` |
|       ! 0 |  444 | `		SyMemBackendFree(&pVm->sAllocator,zCopy);` |
|       ! 0 |  445 | `		return 0;` |
|         - |  446 | `	}` |
|      6480 |  447 | `	return zCopy;` |
|      8640 |  448 | `}` |
|         - |  449 | `/*` |
|         - |  450 | ` * Are these two names the same bytes? Equality only -- the guard below never orders.` |
|         - |  451 | ` *` |
|         - |  452 | ` * SyMemcmp is a byte loop (SX_MACRO_FAST_CMP, four bytes unrolled with a branch each)` |
|         - |  453 | ` * behind a call, and THIS ONE SITE walked 1,737,378,747 bytes of callee name on the` |
|         - |  454 | ` * ecosystem gate's phpcs step: 42,994,248 guards averaging forty bytes, because a` |
|         - |  455 | ` * namespaced function name is long. It was the second-largest SyMemcmp caller in the` |
|         - |  456 | ` * engine, above the one inside SyHashGetHashed. Eight bytes at a time turns forty` |
|         - |  457 | ` * comparisons into five and drops the call. (PERF.md P16.)` |
|         - |  458 | ` *` |
|         - |  459 | ` * The load is through memcpy rather than a cast: an unaligned sxu64 read through a` |
|         - |  460 | ` * char pointer is what UBSan exists to catch, and every compiler in the matrix folds a` |
|         - |  461 | ` * constant-size memcpy into the one load anyway.` |
|         - |  462 | ` *` |
|         - |  463 | ` * This is NOT a case for widening SyMemcmp itself. PERF.md §5 records that measuring` |
|         - |  464 | ` * neutral, because most of its callers compare short property names where a word loop` |
|         - |  465 | ` * never gets going -- the same reason glibc's memcmp measured 1.3% SLOWER there.` |
|         - |  466 | ` */` |
|   4187542 |  467 | `static int VmCallNameEq(const char *zA,const char *zB,sxu32 nByte)` |
|         5 |  468 | `{` |
|         - |  469 | `	/* Declared and seeded out here for MSVC: /WX turns C4701 ("potentially` |
|         - |  470 | `	 * uninitialized local variable used") into an error, and cl cannot see that the` |
|         - |  471 | `	 * memmove below is what writes them. Both compilers drop the two stores. */` |
|   4187547 |  472 | `	sxu64 a = 0,b = 0;` |
|   6859658 |  473 | `	while( nByte >= sizeof(sxu64) ){` |
|   2672146 |  474 | `		SX_MACRO_FAST_MEMCPY(zA,&a,sizeof(a));` |
|   2672146 |  475 | `		SX_MACRO_FAST_MEMCPY(zB,&b,sizeof(b));` |
|   2672146 |  476 | `		if( a != b ){` |
|        31 |  477 | `			return 0;` |
|         - |  478 | `		}` |
|   2672116 |  479 | `		zA += sizeof(sxu64);` |
|   2672116 |  480 | `		zB += sizeof(sxu64);` |
|   2672116 |  481 | `		nByte -= (sxu32)sizeof(sxu64);` |
|         5 |  482 | `	}` |
|  21618091 |  483 | `	while( nByte-- > 0 ){` |
|  17431257 |  484 | `		if( *zA++ != *zB++ ){` |
|       683 |  485 | `			return 0;` |
|         - |  486 | `		}` |
|         5 |  487 | `	}` |
|   4186839 |  488 | `	return 1;` |
|   2094338 |  489 | `}` |
|         - |  490 | `/*` |
|         - |  491 | ` * The VmCallSite record a PH7_OP_CALL site owns. bClaim says which of the two doors is` |
|         - |  492 | ` * asking: the RECORDING one may create the record, the ASKING one only reads it.` |
|         - |  493 | ` *` |
|         - |  494 | ` * Answers 0 whenever the site has to resolve the long way -- it has no record yet, it` |
|         - |  495 | ` * has been marked dead, it is asking about a name it did not ask about before (which is` |
|         - |  496 | ` * what marks it dead), or the bookkeeping could not be allocated.` |
|         - |  497 | ` */` |
|  10723812 |  498 | `static VmCallSite * VmCallSiteFor(ph7_vm *pVm,VmInstr *pInstr,const SyString *pName,` |
|         - |  499 | `	int bEngineName,int bClaim)` |
|         5 |  500 | `{` |
|         - |  501 | `	VmCallSite *pSite;` |
|  10723817 |  502 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|       ! 0 |  503 | `		return 0;` |
|         - |  504 | `	}` |
|  10723817 |  505 | `	if( pInstr->nSite == 0 ){` |
|         - |  506 | `		/* No record yet. Claim one only on this site's SECOND execution, because a` |
|         - |  507 | `		 * record costs more memory than a site that runs once can ever save -- a` |
|         - |  508 | `		 * bootstrap, a one-shot branch, a sniff that matches nothing. pInstr->nAux is` |
|         - |  509 | `		 * free on a PH7_OP_CALL (OP_LOAD and OP_CALL_INIT are the only opcodes that` |
|         - |  510 | `		 * use it), so the site counts its own first two executions there.` |
|         - |  511 | `		 *` |
|         - |  512 | `		 * Only the ASKING door counts, and only the RECORDING door claims: both run on` |
|         - |  513 | `		 * one dispatch, so a shared counter would reach two before the first call has` |
|         - |  514 | `		 * finished and the site would pay on its first execution after all. */` |
|         - |  515 | `		VmCallSite sNew;` |
|         - |  516 | `		const char *zCopy;` |
|   6509161 |  517 | `		if( !bClaim ){` |
|   3374652 |  518 | `			if( pInstr->nAux < 2 ){` |
|   3134654 |  519 | `				pInstr->nAux++;` |
|   1566653 |  520 | `			}` |
|   3374652 |  521 | `			return 0;` |
|         - |  522 | `		}` |
|   3134514 |  523 | `		if( pInstr->nAux < 2 ){` |
|   3117109 |  524 | `			return 0;` |
|         - |  525 | `		}` |
|     17410 |  526 | `		zCopy = VmCallNameIntern(&(*pVm),pName);` |
|     17410 |  527 | `		if( zCopy == 0 ){` |
|       ! 0 |  528 | `			return 0;` |
|         - |  529 | `		}` |
|     17410 |  530 | `		sNew.zName = zCopy;` |
|     17410 |  531 | `		sNew.nName = pName->nByte;` |
|     17410 |  532 | `		sNew.pEntry = 0;` |
|     17410 |  533 | `		sNew.nGen = 0;` |
|     17410 |  534 | `		sNew.nNextFree = 0;` |
|     17410 |  535 | `		sNew.bHost = 0;` |
|     17410 |  536 | `		sNew.bEngine = (sxu8)(bEngineName ? 1 : 0);` |
|     17410 |  537 | `		sNew.bDead = 0;` |
|     17410 |  538 | `		if( pVm->nFreeCallSite ){` |
|      5027 |  539 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|      5027 |  540 | `			if( pSite ){` |
|      5027 |  541 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|      5027 |  542 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|      5027 |  543 | `				*pSite = sNew;` |
|      5027 |  544 | `				return pSite;` |
|         - |  545 | `			}` |
|       ! 0 |  546 | `			pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse rather than on the cache */` |
|       ! 0 |  547 | `		}` |
|     12384 |  548 | `		if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|       ! 0 |  549 | `			return 0; /* the interned name stays; another site may still want it */` |
|         - |  550 | `		}` |
|     12384 |  551 | `		pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|     12384 |  552 | `		return (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|         - |  553 | `	}` |
|   4214661 |  554 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|   4214661 |  555 | `	if( pSite == 0 \|\| pSite->bDead ){` |
|     26891 |  556 | `		return 0;` |
|         - |  557 | `	}` |
|   4187770 |  558 | `	if( pSite->nName != pName->nByte` |
|   4187656 |  559 | `	 \|\| pSite->bEngine != (sxu8)(bEngineName ? 1 : 0)` |
|   4187547 |  560 | `	 \|\| !VmCallNameEq(pSite->zName,pName->zString,pSite->nName) ){` |
|         - |  561 | `		/* A second name at one site: the callee is a variable (or a closure key), and` |
|         - |  562 | `		 * re-interning it on every call would cost more than the lookup it saves. */` |
|       941 |  563 | `		pSite->zName = 0;` |
|       941 |  564 | `		pSite->nName = 0;` |
|       941 |  565 | `		pSite->pEntry = 0;` |
|       941 |  566 | `		pSite->nGen = 0;` |
|       941 |  567 | `		pSite->bDead = 1;` |
|       941 |  568 | `		return 0;` |
|         - |  569 | `	}` |
|   4186839 |  570 | `	return pSite;` |
|   5360859 |  571 | `}` |
|         - |  572 | `/*` |
|         - |  573 | ` * The function-table entry this call site resolved its callee to last time, or 0 if it` |
|         - |  574 | ` * has to be resolved again. *pbHost says which table the answer is in.` |
|         - |  575 | ` */` |
|   7514260 |  576 | `PH7_PRIVATE SyHashEntry * PH7_VmCallSiteAnswer(` |
|         - |  577 | `	ph7_vm *pVm,          /* Target VM */` |
|         - |  578 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|         - |  579 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|         - |  580 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|         - |  581 | `	int *pbHost           /* OUT: 1 when the answer lives in hHostFunction */` |
|         - |  582 | `	)` |
|         5 |  583 | `{` |
|   7514265 |  584 | `	VmCallSite *pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,0);` |
|   7514265 |  585 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nCallableGen ){` |
|   3449695 |  586 | `		return 0;` |
|         - |  587 | `	}` |
|   4064575 |  588 | `	*pbHost = pSite->bHost;` |
|   4064575 |  589 | `	return pSite->pEntry;` |
|   3756892 |  590 | `}` |
|         - |  591 | `/*` |
|         - |  592 | ` * Remember what this call site's callee name resolved to, so the next execution can` |
|         - |  593 | ` * skip the lookups. Silently does nothing for a site that has no record to write to.` |
|         - |  594 | ` */` |
|   5004246 |  595 | `PH7_PRIVATE void PH7_VmCallSiteRecord(` |
|         - |  596 | `	ph7_vm *pVm,          /* Target VM */` |
|         - |  597 | `	VmInstr *pInstr,      /* The PH7_OP_CALL being dispatched */` |
|         - |  598 | `	const SyString *pName,/* Callee name, as this dispatch spelled it */` |
|         - |  599 | `	int bEngineName,      /* TRUE when the engine, not the script, spelled it */` |
|         - |  600 | `	int bHost,            /* TRUE when pEntry lives in hHostFunction */` |
|         - |  601 | `	SyHashEntry *pEntry   /* The entry the name resolved to */` |
|         - |  602 | `	)` |
|         5 |  603 | `{` |
|         - |  604 | `	VmCallSite *pSite;` |
|   5004251 |  605 | `	if( pEntry == 0 ){` |
|   1794699 |  606 | `		return;` |
|         - |  607 | `	}` |
|   3209557 |  608 | `	pSite = VmCallSiteFor(&(*pVm),pInstr,pName,bEngineName,1);` |
|   3209557 |  609 | `	if( pSite == 0 ){` |
|   3131020 |  610 | `		return;` |
|         - |  611 | `	}` |
|     78542 |  612 | `	pSite->pEntry = pEntry;` |
|     78542 |  613 | `	pSite->bHost = (sxu8)(bHost ? 1 : 0);` |
|     78542 |  614 | `	pSite->nGen = pVm->nCallableGen;` |
|   2500942 |  615 | `}` |
|         - |  616 | `/*` |
|         - |  617 | ` * The hConstant entry a PH7_OP_LOADC site resolved its constant to last time, or 0 when` |
|         - |  618 | ` * it has to be resolved the long way.` |
|         - |  619 | ` *` |
|         - |  620 | ` * A LOADC site looks up as many as TWO names on every execution -- the compile-time` |
|         - |  621 | `` * candidate in p3 (a `use const` import's FQN, or `current-namespace\NAME`) and then the`` |
|         - |  622 | ` * bare literal -- and on the ecosystem gate's phpcs step those two were 104M of the` |
|         - |  623 | ` * engine's 724M hash lookups and 3.0 GB of its 7.5 GB of hashed key bytes. The candidate` |
|         - |  624 | ` * alone hashed 2.3 GB to miss 91% of the time, because a namespace-qualified name is` |
|         - |  625 | ` * long and usually is not a constant.` |
|         - |  626 | ` *` |
|         - |  627 | ` * Which of the two wins, and what it resolves to, can only change when a name enters or` |
|         - |  628 | ` * leaves hConstant. So the site stamps the pVm->nConstGen it resolved at, and a site` |
|         - |  629 | ` * whose stamp is current answers without hashing anything.` |
|         - |  630 | ` *` |
|         - |  631 | ` * The record is claimed on the site's SECOND execution, for VmCallSiteFor's reason: a` |
|         - |  632 | ` * bootstrap or a one-shot branch would pay for bookkeeping it never reads. pInstr->nAux` |
|         - |  633 | ` * is free on a PH7_OP_LOADC, so the site counts its first two executions there.` |
|         - |  634 | ` */` |
|    149906 |  635 | `PH7_PRIVATE SyHashEntry * PH7_VmConstSiteAnswer(ph7_vm *pVm,VmInstr *pInstr)` |
|         5 |  636 | `{` |
|         - |  637 | `	VmCallSite *pSite;` |
|    149911 |  638 | `	if( pInstr->nSite == 0 ){` |
|      9646 |  639 | `		if( pInstr->nAux < 2 ){` |
|      9604 |  640 | `			pInstr->nAux++;` |
|      4717 |  641 | `		}` |
|      9646 |  642 | `		return 0;` |
|         - |  643 | `	}` |
|    140270 |  644 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|    140270 |  645 | `	if( pSite == 0 \|\| pSite->nGen != pVm->nConstGen ){` |
|       123 |  646 | `		return 0;` |
|         - |  647 | `	}` |
|    140148 |  648 | `	return pSite->pEntry;` |
|     74831 |  649 | `}` |
|         - |  650 | `/*` |
|         - |  651 | ` * Remember what this PH7_OP_LOADC site's constant name resolved to. Silently does` |
|         - |  652 | ` * nothing when the site has not earned a record yet or one cannot be allocated -- the` |
|         - |  653 | ` * lookup path above is always correct on its own.` |
|         - |  654 | ` */` |
|      9763 |  655 | `PH7_PRIVATE void PH7_VmConstSiteRecord(ph7_vm *pVm,VmInstr *pInstr,SyHashEntry *pEntry)` |
|         5 |  656 | `{` |
|         - |  657 | `	VmCallSite *pSite;` |
|      9768 |  658 | `	if( pEntry == 0 ){` |
|       183 |  659 | `		return;` |
|         - |  660 | `	}` |
|      9590 |  661 | `	if( pInstr->nSite == 0 ){` |
|         - |  662 | `		VmCallSite sNew;` |
|      9468 |  663 | `		if( pInstr->nAux < 2 ){` |
|      8818 |  664 | `			return;` |
|         - |  665 | `		}` |
|       655 |  666 | `		sNew.zName = 0;   /* a LOADC site's name is its instruction; nothing to guard */` |
|       655 |  667 | `		sNew.nName = 0;` |
|       655 |  668 | `		sNew.pEntry = 0;` |
|       655 |  669 | `		sNew.nGen = 0;` |
|       655 |  670 | `		sNew.nNextFree = 0;` |
|       655 |  671 | `		sNew.bHost = 0;` |
|       655 |  672 | `		sNew.bEngine = 0;` |
|       655 |  673 | `		sNew.bDead = 0;` |
|       655 |  674 | `		if( pVm->nFreeCallSite ){` |
|        99 |  675 | `			pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pVm->nFreeCallSite - 1);` |
|        99 |  676 | `			if( pSite ){` |
|        99 |  677 | `				pInstr->nSite = pVm->nFreeCallSite;` |
|        99 |  678 | `				pVm->nFreeCallSite = pSite->nNextFree;` |
|        99 |  679 | `				*pSite = sNew;` |
|        50 |  680 | `			}else{` |
|       ! 0 |  681 | `				pVm->nFreeCallSite = 0; /* corrupt link: give up on reuse, not on the cache */` |
|         - |  682 | `			}` |
|        49 |  683 | `		}` |
|       655 |  684 | `		if( pInstr->nSite == 0 ){` |
|       557 |  685 | `			if( SySetPut(&pVm->aCallSite,(const void *)&sNew) != SXRET_OK ){` |
|       ! 0 |  686 | `				return;` |
|         - |  687 | `			}` |
|       557 |  688 | `			pInstr->nSite = SySetUsed(&pVm->aCallSite); /* index + 1 */` |
|       271 |  689 | `		}` |
|       320 |  690 | `	}` |
|       777 |  691 | `	pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,pInstr->nSite - 1);` |
|       777 |  692 | `	if( pSite == 0 ){` |
|       ! 0 |  693 | `		return;` |
|         - |  694 | `	}` |
|       777 |  695 | `	pSite->pEntry = pEntry;` |
|       777 |  696 | `	pSite->nGen = pVm->nConstGen;` |
|      4804 |  697 | `}` |
|         - |  698 | `/*` |
|         - |  699 | ` * Give back every site record the instructions in a bytecode container claimed -- a` |
|         - |  700 | ` * PH7_OP_CALL's callee answer and a PH7_OP_LOADC's constant answer alike. Called just` |
|         - |  701 | ` * before the container itself is released -- which happens exactly once, for the chunk an` |
|         - |  702 | ` * eval() or an include compiles -- so that a program evaluating chunks in a loop reuses` |
|         - |  703 | ` * the records instead of accumulating one per chunk for ever.` |
|         - |  704 | ` */` |
|     28161 |  705 | `PH7_PRIVATE void PH7_VmCallSiteReleaseChunk(ph7_vm *pVm,SySet *pByteCode)` |
|         5 |  706 | `{` |
|     28166 |  707 | `	VmInstr *aInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|     28166 |  708 | `	sxu32 n = SySetUsed(pByteCode);` |
|         - |  709 | `	sxu32 i;` |
|     28166 |  710 | `	if( aInstr == 0 ){` |
|       ! 0 |  711 | `		return;` |
|         - |  712 | `	}` |
|    724783 |  713 | `	for( i = 0 ; i < n ; ++i ){` |
|         - |  714 | `		VmCallSite *pSite;` |
|    696622 |  715 | `		sxu32 nSite = aInstr[i].nSite;` |
|    696617 |  716 | `		if( nSite == 0` |
|    350874 |  717 | `		 \|\| (aInstr[i].iOp != PH7_OP_CALL && aInstr[i].iOp != PH7_OP_LOADC) ){` |
|    691492 |  718 | `			continue;` |
|         - |  719 | `		}` |
|      5131 |  720 | `		aInstr[i].nSite = 0;` |
|      5131 |  721 | `		pSite = (VmCallSite *)SySetAt(&pVm->aCallSite,nSite - 1);` |
|      5131 |  722 | `		if( pSite == 0 ){` |
|       ! 0 |  723 | `			continue;` |
|         - |  724 | `		}` |
|      5131 |  725 | `		pSite->zName = 0; /* the name itself is hCallName's, and other sites may share it */` |
|      5131 |  726 | `		pSite->nName = 0;` |
|      5131 |  727 | `		pSite->pEntry = 0;` |
|      5131 |  728 | `		pSite->nGen = 0;` |
|      5131 |  729 | `		pSite->bDead = 0;` |
|      5131 |  730 | `		pSite->nNextFree = pVm->nFreeCallSite;` |
|      5131 |  731 | `		pVm->nFreeCallSite = nSite;` |
|      2566 |  732 | `	}` |
|     14081 |  733 | `}` |
|         - |  734 | `/*` |
|         - |  735 | ` * Namespace-aware function lookup.` |
|         - |  736 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  737 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  738 | ` */` |
|         - |  739 | `/*` |
|         - |  740 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  741 | ` */` |
|  36623931 |  742 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  743 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  744 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  745 | `	SyString *pName     /* Function name */` |
|         - |  746 | `	)` |
|         5 |  747 | `{` |
|         - |  748 | `	SyHashEntry *pEntry;` |
|         - |  749 | `	sxi32 rc;` |
|  36623936 |  750 | `	if( pName == 0 ){` |
|         - |  751 | `		/* Use the built-in name */` |
|    170302 |  752 | `		pName = &pFunc->sName;` |
|     84923 |  753 | `	}` |
|         - |  754 | `	/* Check for duplicates (functions with the same name) first */` |
|  36623936 |  755 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  36623936 |  756 | `	if( pEntry ){` |
|  27429249 |  757 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  27429249 |  758 | `		if( pLink != pFunc ){` |
|         - |  759 | `			/* Link */` |
|        72 |  760 | `			pFunc->pNextName = pLink;` |
|        72 |  761 | `			pEntry->pUserData = pFunc;` |
|        34 |  762 | `		}` |
|  27429249 |  763 | `		return SXRET_OK;` |
|         - |  764 | `	}` |
|         - |  765 | `	/* First time seen */` |
|   9194692 |  766 | `	pFunc->pNextName = 0;` |
|   9194692 |  767 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   9194692 |  768 | `	if( (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) == 0 ){` |
|         - |  769 | `		/* A name a SCRIPT can now call. This table also holds every method and every` |
|         - |  770 | `		 * per-instantiation closure copy -- names PH7_VmGetUserFunction refuses to a` |
|         - |  771 | `		 * script -- and counting those would retire OP_CALL_INIT's screened-at stamps` |
|         - |  772 | `		 * on every closure EXPRESSION a program evaluates, which is most of them. */` |
|    151967 |  773 | `		pVm->nCallableGen++;` |
|     75877 |  774 | `	}` |
|   9194692 |  775 | `	return rc;` |
|  18285168 |  776 | `}` |
|         - |  777 | `/*` |
|         - |  778 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  779 | ` */` |
|   1477417 |  780 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  781 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  782 | `	ph7_class *pClass /* Target Class */` |
|         - |  783 | `	)` |
|         5 |  784 | `{` |
|   1477422 |  785 | `	SyString *pName = &pClass->sName;` |
|         - |  786 | `	SyHashEntry *pEntry;` |
|         - |  787 | `	sxi32 rc;` |
|         - |  788 | `	/* Check for duplicates */` |
|   1477422 |  789 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|   1477422 |  790 | `	if( pEntry ){` |
|         6 |  791 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  792 | `		/* Link entry with the same name */` |
|         6 |  793 | `		pClass->pNextName = pLink;` |
|         6 |  794 | `		pEntry->pUserData = pClass;` |
|         6 |  795 | `		return SXRET_OK;` |
|         - |  796 | `	}` |
|   1477416 |  797 | `	pClass->pNextName = 0;` |
|         - |  798 | `	/* Perform a simple hashtable insertion */` |
|   1477416 |  799 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|   1477416 |  800 | `	return rc;` |
|    737728 |  801 | `}` |
|         - |  802 | `/*` |
|         - |  803 | ` * Instruction builder interface.` |
|         - |  804 | ` */` |
|  14491278 |  805 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  806 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  807 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  808 | `	sxi32 iP1,    /* First operand */` |
|         - |  809 | `	sxu32 iP2,    /* Second operand */` |
|         - |  810 | `	void *p3,     /* Third operand */` |
|         - |  811 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  812 | `	)` |
|         5 |  813 | `{` |
|         - |  814 | `	VmInstr sInstr;` |
|  14491283 |  815 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  816 | `	sxi32 rc;` |
|         - |  817 | `	/* Fill the VM instruction */` |
|  14491283 |  818 | `	sInstr.iOp = (sxu8)iOp;` |
|  14491283 |  819 | `	sInstr.iP1 = iP1;` |
|  14491283 |  820 | `	sInstr.iP2 = iP2;` |
|  14491283 |  821 | `	sInstr.p3  = p3;` |
|         - |  822 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  823 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  824 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  825 | `	 * between statements, hence the range check. */` |
|  14491283 |  826 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|         - |  827 | `	/* Nothing is discarded until the statement that owns this call says so` |
|         - |  828 | `	 * (GenStateMarkDiscardedCall, after the fact) — but the field must not be` |
|         - |  829 | `	 * this stack frame's leftovers in the meantime. */` |
|  14491283 |  830 | `	sInstr.bDiscard = 0;` |
|         - |  831 | `	/* ...and neither must the reference-source marker: the codegen stamps it on the` |
|         - |  832 | `	 * one OP_MEMBER it belongs to, AFTER this returns. */` |
|  14491283 |  833 | `	sInstr.bRefSrc = 0;` |
|  14491283 |  834 | `	sInstr.nAux = 0;` |
|         - |  835 | `	/* ...nor the call site's cache index: a stale one would point this site at` |
|         - |  836 | `	 * another site's remembered callee. */` |
|  14491283 |  837 | `	sInstr.nSite = 0;` |
|  14491283 |  838 | `	sInstr.nLine = 0;` |
|  14491283 |  839 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   5351845 |  840 | `		sInstr.nLine = pGen->pIn->nLine;` |
|  11810693 |  841 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  842 | `		/* Past the end (statement tail): blame the last real token. */` |
|   9049795 |  843 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   4517218 |  844 | `	}` |
|  14491283 |  845 | `	if( pIndex ){` |
|         - |  846 | `		/* Instruction index in the bytecode array */` |
|   1214428 |  847 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    606380 |  848 | `	}` |
|         - |  849 | `	/* Finally,record the instruction */` |
|  14491283 |  850 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  14491283 |  851 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  852 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  853 | `		/* Fall throw */` |
|       ! 0 |  854 | `	}` |
|  14491283 |  855 | `	return rc;` |
|         5 |  856 | `}` |
|         - |  857 | `/*` |
|         - |  858 | ` * Swap the current bytecode container with the given one.` |
|         - |  859 | ` */` |
|    441662 |  860 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  861 | `{` |
|    441667 |  862 | `	if( pContainer == 0 ){` |
|         - |  863 | `		/* Point to the default container */` |
|       ! 0 |  864 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  865 | `	}else{` |
|         - |  866 | `		/* Change container */` |
|    441667 |  867 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  868 | `	}` |
|    441667 |  869 | `	return SXRET_OK;` |
|         5 |  870 | `}` |
|         - |  871 | `/*` |
|         - |  872 | ` * Return the current bytecode container.` |
|         - |  873 | ` */` |
|   1155183 |  874 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  875 | `{` |
|   1155188 |  876 | `	return pVm->pByteContainer;` |
|         5 |  877 | `}` |
|         - |  878 | `/*` |
|         - |  879 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  880 | ` */` |
|   1167446 |  881 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  882 | `{` |
|         - |  883 | `	VmInstr *pInstr;` |
|   1167451 |  884 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|   1167451 |  885 | `	return pInstr;` |
|         5 |  886 | `}` |
|         - |  887 | `/*` |
|         - |  888 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  889 | ` */` |
|  11845323 |  890 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  891 | `{` |
|  11845328 |  892 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  893 | `}` |
|         - |  894 | `/*` |
|         - |  895 | ` * Pop the last VM instruction.` |
|         - |  896 | ` */` |
|    884152 |  897 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  898 | `{` |
|    884157 |  899 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  900 | `}` |
|         - |  901 | `/*` |
|         - |  902 | ` * Peek the last VM instruction.` |
|         - |  903 | ` */` |
|   4786572 |  904 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  905 | `{` |
|   4786577 |  906 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  907 | `}` |
|     96594 |  908 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  909 | `{` |
|         - |  910 | `	VmInstr *aInstr;` |
|         - |  911 | `	sxu32 n;` |
|     96599 |  912 | `	n = SySetUsed(pVm->pByteContainer);` |
|     96599 |  913 | `	if( n < 2 ){` |
|       ! 0 |  914 | `		return 0;` |
|         - |  915 | `	}` |
|     96599 |  916 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     96599 |  917 | `	return &aInstr[n - 2];` |
|     48236 |  918 | `}` |
|         - |  919 | `/*` |
|         - |  920 | ` * Allocate a new virtual machine frame.` |
|         - |  921 | ` */` |
|   3745626 |  922 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  923 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  924 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  925 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  926 | `	)` |
|         5 |  927 | `{` |
|         - |  928 | `	VmFrame *pFrame;` |
|         - |  929 | `	/* Allocate a new vm frame */` |
|   3745631 |  930 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3745631 |  931 | `	if( pFrame == 0 ){` |
|       ! 0 |  932 | `		return 0;` |
|         - |  933 | `	}` |
|         - |  934 | `	/* Zero the structure */` |
|   3745631 |  935 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  936 | `	/* Initialize frame fields */` |
|   3745631 |  937 | `	pFrame->pUserData = pUserData;` |
|   3745631 |  938 | `	pFrame->pThis = pThis;` |
|   3745631 |  939 | `	pFrame->pVm = pVm;` |
|   3745631 |  940 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3745631 |  941 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3745631 |  942 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3745631 |  943 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3745631 |  944 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  945 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  946 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3745631 |  947 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3745631 |  948 | `	return pFrame;` |
|   1872541 |  949 | `}` |
|         - |  950 | `/* Forward declaration */` |
|         - |  951 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  952 | `/*` |
|         - |  953 | ` * The file the code RUNNING RIGHT NOW is written in -- what php would call the` |
|         - |  954 | ` * executing op array's filename, and what a trace frame records as its call site.` |
|         - |  955 | ` *` |
|         - |  956 | ` * Three answers, in order. An include/require/eval started from the current frame` |
|         - |  957 | ` * means that unit's own top-level code is what is running, so the include stack's` |
|         - |  958 | ` * top is the file (a frame is SHARED with the unit it includes: php gives the unit` |
|         - |  959 | ` * an op array of its own, this engine does not). Otherwise it is the defining file` |
|         - |  960 | ` * of the function whose frame this is -- the include stack is no help there, since` |
|         - |  961 | ` * a call chain spanning files leaves it pointing at the outermost unit. And for` |
|         - |  962 | ` * top-level code with no function at all, the include stack's top again.` |
|         - |  963 | ` *` |
|         - |  964 | `` * A `try` block pushes a frame of its own carrying no function, so the search for`` |
|         - |  965 | ` * the running function looks past those.` |
|         - |  966 | ` */` |
|   5280465 |  967 | `PH7_PRIVATE SyString * PH7_VmExecutingUnitFile(ph7_vm *pVm)` |
|         5 |  968 | `{` |
|   5280470 |  969 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - |  970 | `	sxu32 nInc;` |
|  12731150 |  971 | `	while( pFrame && pFrame->pParent` |
|  10091081 |  972 | `	    && (pFrame->iFlags & (VM_FRAME_EXCEPTION\|VM_FRAME_CATCH)) ){` |
|   2730658 |  973 | `		pFrame = pFrame->pParent;` |
|         5 |  974 | `	}` |
|   5280470 |  975 | `	nInc = SySetUsed(&pVm->aIncFrame);` |
|   5280470 |  976 | `	if( nInc > 0 ){` |
|    267571 |  977 | `		VmIncFrame *pInc = (VmIncFrame *)SySetAt(&pVm->aIncFrame,nInc - 1);` |
|    267571 |  978 | `		if( pInc && pInc->pFrame == (void *)pFrame ){` |
|    155091 |  979 | `			return (SyString *)SySetPeek(&pVm->aFiles);` |
|         - |  980 | `		}` |
|     56240 |  981 | `	}` |
|   5125384 |  982 | `	if( pFrame && pFrame->pUserData ){` |
|   1426155 |  983 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pFrame->pUserData;` |
|   1426155 |  984 | `		if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   1425035 |  985 | `			return &pFunc->sFile;` |
|         - |  986 | `		}` |
|       560 |  987 | `	}` |
|   3700354 |  988 | `	return (SyString *)SySetPeek(&pVm->aFiles);` |
|   2639914 |  989 | `}` |
|         - |  990 | `/* Defined with the variable-slot machinery below; VmEnterFrame is what arms it. */` |
|         - |  991 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName);` |
|         - |  992 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc);` |
|         - |  993 | `/*` |
|         - |  994 | ` * Enter a VM frame.` |
|         - |  995 | ` */` |
|   3744654 |  996 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  997 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  998 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  999 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - | 1000 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - | 1001 | `	)` |
|         5 | 1002 | `{` |
|         - | 1003 | `	VmFrame *pFrame;` |
|         - | 1004 | `	/* Allocate a new frame */` |
|   3744659 | 1005 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3744659 | 1006 | `	if( pFrame == 0 ){` |
|       ! 0 | 1007 | `		return SXERR_MEM;` |
|         - | 1008 | `	}` |
|   3744659 | 1009 | `	pFrame->pSelfClass = pThis ? pThis->pClass : 0; /* the caller overwrites it for a static call */` |
|   3744659 | 1010 | `	if( pUserData ){` |
|         - | 1011 | `		/* A function frame runs a body whose variables can be numbered; do it once,` |
|         - | 1012 | `		 * here, so every push site inherits it (the OP_CALL trampoline, a generator` |
|         - | 1013 | `		 * or fiber resume, a closure, an engine-dispatched magic method). A frame` |
|         - | 1014 | `		 * with no function -- the global one, a try's -- leaves pCodeBase 0 and its` |
|         - | 1015 | `		 * variables take the hash path. */` |
|    812121 | 1016 | `		VmFrameNumberBody(pFrame,(ph7_vm_func *)pUserData);` |
|    405922 | 1017 | `	}` |
|         - | 1018 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3744659 | 1019 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - | 1020 | `	{` |
|         - | 1021 | `		/* ...and the file that line is in, which has to be read NOW: the include` |
|         - | 1022 | `		 * stack has moved on by the time a backtrace is taken. */` |
|   3744659 | 1023 | `		SyString *pCallFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|   3744659 | 1024 | `		if( pCallFile ){` |
|   3737938 | 1025 | `			pFrame->sCallFile = *pCallFile;` |
|   1868694 | 1026 | `		}` |
|         - | 1027 | `	}` |
|         - | 1028 | `	/* Link to the list of active VM frame */` |
|   3744659 | 1029 | `	pFrame->pParent = pVm->pFrame;` |
|   3744659 | 1030 | `	pVm->pFrame = pFrame;` |
|   3744659 | 1031 | `	if( ppFrame ){` |
|         - | 1032 | `		/* Write a pointer to the new VM frame */` |
|   3737922 | 1033 | `		*ppFrame = pFrame;` |
|   1868686 | 1034 | `	}` |
|   3744659 | 1035 | `	return SXRET_OK;` |
|   1872055 | 1036 | `}` |
|         - | 1037 | `/*` |
|         - | 1038 | ` * Link a foreign variable with the TOP most active frame.` |
|         - | 1039 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - | 1040 | ` * information.` |
|         - | 1041 | ` */` |
|      1916 | 1042 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 | 1043 | `{` |
|         - | 1044 | `	VmFrame *pTarget,*pGlobal;` |
|         - | 1045 | `	SyHashEntry *pEntry;` |
|         - | 1046 | `	sxi32 rc;` |
|      1921 | 1047 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - | 1048 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - | 1049 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - | 1050 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - | 1051 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - | 1052 | `	 * writes never reached the real global. */` |
|      1921 | 1053 | `	pGlobal = pTarget;` |
|      4261 | 1054 | `	while( pGlobal->pParent ){` |
|      2345 | 1055 | `		pGlobal = pGlobal->pParent;` |
|         5 | 1056 | `	}` |
|      1921 | 1057 | `	if( pGlobal == pTarget ){` |
|         - | 1058 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 | 1059 | `		return SXRET_OK;` |
|         - | 1060 | `	}` |
|         - | 1061 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - | 1062 | `	 * plain global that would shadow it. */` |
|      1921 | 1063 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|      1921 | 1064 | `	if( pEntry == 0 ){` |
|      1919 | 1065 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|       955 | 1066 | `	}` |
|      1921 | 1067 | `	if( pEntry == 0 ){` |
|         - | 1068 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - | 1069 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - | 1070 | `		 * the assignment went to a local nobody could read. */` |
|        12 | 1071 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        12 | 1072 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 1073 | `			return rc;` |
|         - | 1074 | `		}` |
|        12 | 1075 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        12 | 1076 | `		if( pEntry == 0 ){` |
|       ! 0 | 1077 | `			return SXERR_NOTFOUND;` |
|         - | 1078 | `		}` |
|         5 | 1079 | `	}` |
|         - | 1080 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - | 1081 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|      2877 | 1082 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|      1916 | 1083 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|      1921 | 1084 | `	return SXRET_OK;` |
|       961 | 1085 | `}` |
|         - | 1086 | `/*` |
|         - | 1087 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - | 1088 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - | 1089 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - | 1090 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - | 1091 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - | 1092 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - | 1093 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - | 1094 | ` */` |
|   3738869 | 1095 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 | 1096 | `{` |
|   3738874 | 1097 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 | 1098 | `		VmClearResumeTarget(&(*pVm));` |
|       ! 0 | 1099 | `	}` |
|   3738874 | 1100 | `}` |
|         - | 1101 | `/*` |
|         - | 1102 | ` * The four resume fields are ONE record: a frame, the landing pad inside it, the` |
|         - | 1103 | ` * bytecode array that pad indexes, and the operand-stack base to drain to. They` |
|         - | 1104 | ` * were written together but cleared, saved and restored INDIVIDUALLY (only the` |
|         - | 1105 | ` * frame), so a live frame could end up paired with a dead try's pad and depth —` |
|         - | 1106 | ` * which drains the operand stack to a foreign base and lands mid-statement, one` |
|         - | 1107 | ` * slot below the stack. These four functions are the only writers.` |
|         - | 1108 | ` */` |
|   4304044 | 1109 | `PH7_PRIVATE void VmSetResumeTarget(ph7_vm *pVm,VmFrame *pFrame,sxu32 iPc,void *pInstr,sxi32 iStackDepth)` |
|         5 | 1110 | `{` |
|   4304049 | 1111 | `	pVm->pResumeFrame = pFrame;` |
|   4304049 | 1112 | `	pVm->iResumePc = iPc;` |
|   4304049 | 1113 | `	pVm->pResumeInstr = pInstr;` |
|   4304049 | 1114 | `	pVm->iResumeStackDepth = iStackDepth;` |
|   4304049 | 1115 | `}` |
|   2938224 | 1116 | `PH7_PRIVATE void VmClearResumeTarget(ph7_vm *pVm)` |
|         5 | 1117 | `{` |
|   2938229 | 1118 | `	VmSetResumeTarget(&(*pVm),0,0,0,0);` |
|   2938229 | 1119 | `}` |
|   1366364 | 1120 | `PH7_PRIVATE void VmSaveResumeTarget(ph7_vm *pVm,VmResumeTarget *pSave)` |
|         5 | 1121 | `{` |
|   1366369 | 1122 | `	pSave->pFrame = pVm->pResumeFrame;` |
|   1366369 | 1123 | `	pSave->iPc = pVm->iResumePc;` |
|   1366369 | 1124 | `	pSave->pInstr = pVm->pResumeInstr;` |
|   1366369 | 1125 | `	pSave->iStackDepth = pVm->iResumeStackDepth;` |
|   1366369 | 1126 | `}` |
|      1199 | 1127 | `PH7_PRIVATE void VmRestoreResumeTarget(ph7_vm *pVm,const VmResumeTarget *pSave)` |
|         5 | 1128 | `{` |
|      1204 | 1129 | `	VmSetResumeTarget(&(*pVm),pSave->pFrame,pSave->iPc,pSave->pInstr,pSave->iStackDepth);` |
|      1204 | 1130 | `}` |
|         - | 1131 | `/*` |
|         - | 1132 | ` * Leave the top-most active frame.` |
|         - | 1133 | ` */` |
|   3737699 | 1134 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 | 1135 | `{` |
|   3737704 | 1136 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3737704 | 1137 | `	if( pCurFrame ){` |
|         - | 1138 | `		/* Unlink from the list of active VM frame */` |
|   3737704 | 1139 | `		pVm->pFrame = pCurFrame->pParent;` |
|         - | 1140 | `		/* End the foreach walks this activation never finished, before its locals go:` |
|         - | 1141 | `		 * a step retains its subject, and an object walk holds a cursor registered on` |
|         - | 1142 | `		 * the instance. */` |
|   3737704 | 1143 | `		VmReleaseFrameForeachSteps(&(*pVm),pCurFrame);` |
|   3737704 | 1144 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - | 1145 | `			VmSlot  *aSlot;` |
|         - | 1146 | `			sxu32 n;` |
|         - | 1147 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - | 1148 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - | 1149 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - | 1150 | `			 * is about to free). */` |
|    811911 | 1151 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   2071249 | 1152 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1259343 | 1153 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    630623 | 1154 | `			}` |
|         - | 1155 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    811911 | 1156 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   2067783 | 1157 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1255877 | 1158 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - | 1159 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - | 1160 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - | 1161 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - | 1162 | `					 * down the slot and the reference table took the holders with it, so` |
|         - | 1163 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - | 1164 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        24 | 1165 | `					continue;` |
|         - | 1166 | `				}` |
|         - | 1167 | `				/* Unset the local variable */` |
|   1255855 | 1168 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    628881 | 1169 | `			}` |
|    405817 | 1170 | `		}` |
|         - | 1171 | `		/* Release internal containers */` |
|   3737704 | 1172 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3737704 | 1173 | `		SySetRelease(&pCurFrame->sArg);` |
|   3737704 | 1174 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3737704 | 1175 | `		SySetRelease(&pCurFrame->sRef);` |
|         - | 1176 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - | 1177 | `		 * containers above — released for every frame, including transparent` |
|         - | 1178 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3737704 | 1179 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - | 1180 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3737704 | 1181 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - | 1182 | `		/* This activation no longer needs the function it was running. For a` |
|         - | 1183 | `		 * run-time closure that is one of the two holds on its per-instantiation` |
|         - | 1184 | `		 * copy -- the other is the Closure object -- and the copy goes when both` |
|         - | 1185 | `		 * are gone. pUserData is a ph7_vm_func for a user-function frame and 0 for` |
|         - | 1186 | `		 * every other kind (the global frame, an exception wrapper, a local exec). */` |
|   3737704 | 1187 | `		if( pCurFrame->pUserData ){` |
|    811911 | 1188 | `			PH7_VmClosureFuncUnref(&(*pVm),(ph7_vm_func *)pCurFrame->pUserData);` |
|    405817 | 1189 | `		}` |
|         - | 1190 | `		/* Release the whole structure */` |
|   3737704 | 1191 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1868577 | 1192 | `	}` |
|   3737704 | 1193 | `}` |
|         - | 1194 | `/*` |
|         - | 1195 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - | 1196 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - | 1197 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - | 1198 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - | 1199 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - | 1200 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - | 1201 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - | 1202 | ` */` |
|         - | 1203 | `/*` |
|         - | 1204 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - | 1205 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - | 1206 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - | 1207 | ` *` |
|         - | 1208 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - | 1209 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - | 1210 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - | 1211 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - | 1212 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - | 1213 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - | 1214 | ` */` |
|  11504424 | 1215 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 1216 | `{` |
|         - | 1217 | `	VmFrame *pFrame;` |
|  32379195 | 1218 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  20876191 | 1219 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - | 1220 | `		sxu32 n;` |
|  55358162 | 1221 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  34483396 | 1222 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - | 1223 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|      1423 | 1224 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|      1423 | 1225 | `				(void)SySetPop(&pFrame->sLocal);` |
|      1423 | 1226 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - | 1227 | `			}` |
|  17239168 | 1228 | `		}` |
|  10433076 | 1229 | `	}` |
|  11503009 | 1230 | `	return FALSE;` |
|   5749467 | 1231 | `}` |
|         - | 1232 | `/*` |
|         - | 1233 | ` * The superglobal table, asked the cheap question first.` |
|         - | 1234 | ` *` |
|         - | 1235 | ` * Every variable access consults hSuper before the frame -- php resolves $_SERVER` |
|         - | 1236 | ` * the same in every scope, so the order is the semantics and cannot change -- and` |
|         - | 1237 | ` * for the ~9 names that are superglobals ($GLOBALS and the $_* set) the answer is` |
|         - | 1238 | ` * no. Hashing a whole variable name to learn that was, measured on the ecosystem` |
|         - | 1239 | ` * gate's phpcs step, 101M of the engine's 325M hash-table lookups.` |
|         - | 1240 | ` *` |
|         - | 1241 | ` * aSuperFirst is the set of first bytes any INSTALLED superglobal name starts` |
|         - | 1242 | ` * with, so a name whose first byte is not in it cannot be one and never reaches` |
|         - | 1243 | ` * the table. It is a set and not a fixed 'G'/'_' test because an embedder may` |
|         - | 1244 | ` * install a superglobal of its own (PH7_VM_CONFIG_CREATE_SUPER).` |
|         - | 1245 | ` */` |
|   5412461 | 1246 | `PH7_PRIVATE SyHashEntry * PH7_VmSuperGet(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 | 1247 | `{` |
|         - | 1248 | `	unsigned char c;` |
|   5412466 | 1249 | `	if( nByte < 1 \|\| zName == 0 ){` |
|         5 | 1250 | `		return 0;` |
|         - | 1251 | `	}` |
|   5412462 | 1252 | `	c = (unsigned char)zName[0];` |
|   5412462 | 1253 | `	if( (pVm->aSuperFirst[c >> 5] & (1u << (c & 31))) == 0 ){` |
|   5288764 | 1254 | `		return 0;` |
|         - | 1255 | `	}` |
|    123703 | 1256 | `	return SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|   2706034 | 1257 | `}` |
|         - | 1258 | `/* Record a name just installed in hSuper. Every insertion into that table must` |
|         - | 1259 | ` * come through here, or the lookup above stops finding it. */` |
|     61985 | 1260 | `PH7_PRIVATE void PH7_VmSuperNote(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 | 1261 | `{` |
|         - | 1262 | `	VmFrame *pFrame;` |
|         - | 1263 | `	unsigned char c;` |
|     61990 | 1264 | `	if( nByte < 1 \|\| zName == 0 ){` |
|       ! 0 | 1265 | `		return;` |
|         - | 1266 | `	}` |
|     61990 | 1267 | `	c = (unsigned char)zName[0];` |
|     61990 | 1268 | `	pVm->aSuperFirst[c >> 5] \|= (1u << (c & 31));` |
|         - | 1269 | `	/* A name the frames may already have memoized as an ordinary variable now` |
|         - | 1270 | `	 * resolves through hSuper instead, and hSuper is consulted FIRST. Installing a` |
|         - | 1271 | `	 * superglobal is a VM-configuration act with only the global frame live, so the` |
|         - | 1272 | `	 * active chain is every frame there is to correct. */` |
|    123975 | 1273 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     61990 | 1274 | `		VmVarMemoFlush(pFrame);` |
|     30948 | 1275 | `	}` |
|     30948 | 1276 | `}` |
|         - | 1277 | `/*` |
|         - | 1278 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - | 1279 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - | 1280 | ` * should be skipped when looking for the real execution context.` |
|         - | 1281 | ` */` |
|  55820417 | 1282 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 | 1283 | `{` |
|  68975659 | 1284 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  13155242 | 1285 | `		pFrame = pFrame->pParent;` |
|         5 | 1286 | `	}` |
|  55820422 | 1287 | `	return pFrame;` |
|         5 | 1288 | `}` |
|         - | 1289 | `/*` |
|         - | 1290 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - | 1291 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - | 1292 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - | 1293 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - | 1294 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - | 1295 | ` *` |
|         - | 1296 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - | 1297 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - | 1298 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - | 1299 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - | 1300 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - | 1301 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - | 1302 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - | 1303 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - | 1304 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - | 1305 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - | 1306 | ` *` |
|         - | 1307 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - | 1308 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - | 1309 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - | 1310 | ` * catching frame was not the nearest try (ROOT B).` |
|         - | 1311 | ` */` |
|   1976186 | 1312 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 | 1313 | `{` |
|   1976191 | 1314 | `	if( pVm->pResumeFrame == 0 ){` |
|        54 | 1315 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - | 1316 | `	}` |
|   1976141 | 1317 | `	if( pEntryFrame == 0 ){` |
|         - | 1318 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - | 1319 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - | 1320 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - | 1321 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - | 1322 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - | 1323 | `		 * NULL and the process dies. */` |
|        22 | 1324 | `		return FALSE;` |
|         - | 1325 | `	}` |
|         - | 1326 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - | 1327 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - | 1328 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - | 1329 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - | 1330 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - | 1331 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - | 1332 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - | 1333 | `	 * exec's resume site match and land. */` |
|   1976116 | 1334 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1672576 | 1335 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1366860 | 1336 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - | 1337 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - | 1338 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - | 1339 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - | 1340 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - | 1341 | `		 * never match a real frame. */` |
|    611502 | 1342 | `		return FALSE;` |
|         - | 1343 | `	}` |
|         - | 1344 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - | 1345 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - | 1346 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - | 1347 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - | 1348 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - | 1349 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - | 1350 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - | 1351 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - | 1352 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - | 1353 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - | 1354 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - | 1355 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - | 1356 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - | 1357 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - | 1358 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - | 1359 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - | 1360 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - | 1361 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - | 1362 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - | 1363 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - | 1364 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly.` |
|         - | 1365 | `	 *` |
|         - | 1366 | `	 * The record is CONSUMED FIRST — snapshotted whole and cleared — because the pop` |
|         - | 1367 | `	 * loop below runs USER CODE: leaving a frame releases its locals, and a local's` |
|         - | 1368 | `	 * last reference dying runs that object's __destruct(). A destructor allocates,` |
|         - | 1369 | `	 * calls, and may throw; a throw re-enters VmThrowException, whose first act is to` |
|         - | 1370 | `	 * invalidate the in-flight resume record. Reading pVm->iResumePc AFTER the loop` |
|         - | 1371 | ``	 * therefore read a ZERO the destructor had left behind, and `iResumePc - 1` handed`` |
|         - | 1372 | `	 * the dispatcher -1, which its pc++ turned into a re-run of the whole body from` |
|         - | 1373 | `	 * index 0: monolog's suite restarted its top-level script forever. The loop's own` |
|         - | 1374 | `	 * landing-pad test has to read the snapshot for the same reason. */` |
|         - | 1375 | `	{` |
|         - | 1376 | `		VmResumeTarget sTarget;` |
|   1364624 | 1377 | `		VmSaveResumeTarget(&(*pVm),&sTarget);` |
|   1364624 | 1378 | `		VmClearResumeTarget(&(*pVm)); /* one-shot consume: the whole record, before any teardown */` |
|   2251974 | 1379 | `		while( pVm->pFrame != pEntryFrame` |
|   2507142 | 1380 | `		    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1619729 | 1381 | `		         && pVm->pFrame->iExceptionJump == sTarget.iPc) ){` |
|    410202 | 1382 | `			VmLeaveFrame(&(*pVm));` |
|         5 | 1383 | `		}` |
|   1364624 | 1384 | `		*pResumePc = (sxi32)sTarget.iPc - 1;` |
|         - | 1385 | `	}` |
|         - | 1386 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - | 1387 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - | 1388 | `	 * point router must not re-fire it after this resume. */` |
|   1364624 | 1389 | `	pVm->nBoundaryRc = 0;` |
|   1364624 | 1390 | `	return TRUE;` |
|    988043 | 1391 | `}` |
|         - | 1392 | `/*` |
|         - | 1393 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - | 1394 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - | 1395 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - | 1396 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - | 1397 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - | 1398 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - | 1399 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - | 1400 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - | 1401 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - | 1402 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - | 1403 | ` */` |
|         - | 1404 | `/*` |
|         - | 1405 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - | 1406 | ` *` |
|         - | 1407 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - | 1408 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - | 1409 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - | 1410 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - | 1411 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - | 1412 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - | 1413 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - | 1414 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - | 1415 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - | 1416 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - | 1417 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - | 1418 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - | 1419 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - | 1420 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - | 1421 | ` * activation.` |
|         - | 1422 | ` */` |
|   1481004 | 1423 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 | 1424 | `{` |
|   1481009 | 1425 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1481009 | 1426 | `	if( pClone == 0 ){` |
|       ! 0 | 1427 | `		return 0;` |
|         - | 1428 | `	}` |
|   1481009 | 1429 | `	*pClone = *pCompiled;` |
|   1481009 | 1430 | `	pClone->pCompiled = pCompiled;` |
|   1481009 | 1431 | `	pClone->iFinallyDone = 0;` |
|   1481009 | 1432 | `	pClone->iInCatch = 0;` |
|   1481009 | 1433 | `	pClone->pInflight = 0;` |
|   1481009 | 1434 | `	pClone->pFrame = 0;` |
|   1481009 | 1435 | `	return pClone;` |
|    740398 | 1436 | `}` |
|   2946413 | 1437 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 | 1438 | `{` |
|   2946418 | 1439 | `	if( pExc && pExc->pCompiled ){` |
|         - | 1440 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - | 1441 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - | 1442 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - | 1443 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1480999 | 1444 | `		if( pExc->pInflight ){` |
|       ! 0 | 1445 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 | 1446 | `			pExc->pInflight = 0;` |
|       ! 0 | 1447 | `		}` |
|   1480999 | 1448 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    740388 | 1449 | `	}` |
|   2946418 | 1450 | `}` |
|         - | 1451 | `/*` |
|         - | 1452 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - | 1453 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - | 1454 | ` */` |
|     10013 | 1455 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 | 1456 | `{` |
|     10018 | 1457 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 | 1458 | `}` |
|         - | 1459 | `/*` |
|         - | 1460 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - | 1461 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - | 1462 | ` * handlers). The set itself is reset by the caller.` |
|         - | 1463 | ` */` |
|    101896 | 1464 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 | 1465 | `{` |
|    101901 | 1466 | `	sxu32 n = SySetUsed(pSet);` |
|    101901 | 1467 | `	if( n > 0 ){` |
|       ! 0 | 1468 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - | 1469 | `		sxu32 i;` |
|       ! 0 | 1470 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 | 1471 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 | 1472 | `		}` |
|       ! 0 | 1473 | `	}` |
|    101901 | 1474 | `}` |
|         - | 1475 | `/*` |
|         - | 1476 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - | 1477 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - | 1478 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - | 1479 | ` * down a try belonging to the caller.` |
|         - | 1480 | ` */` |
|        18 | 1481 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         3 | 1482 | `{` |
|        21 | 1483 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        21 | 1484 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        21 | 1485 | `	if( nBase < nFloor ){` |
|       ! 0 | 1486 | `		nBase = nFloor;` |
|       ! 0 | 1487 | `	}` |
|        21 | 1488 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         3 | 1489 | `}` |
|         - | 1490 | `/*` |
|         - | 1491 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - | 1492 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - | 1493 | ` * only carries the compiled pointer.` |
|         - | 1494 | ` */` |
|        82 | 1495 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 | 1496 | `{` |
|        87 | 1497 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        87 | 1498 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        87 | 1499 | `	while( n > 0 ){` |
|        87 | 1500 | `		n--;` |
|        87 | 1501 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        87 | 1502 | `			return ap[n];` |
|         - | 1503 | `		}` |
|       ! 0 | 1504 | `	}` |
|       ! 0 | 1505 | `	return 0;` |
|        46 | 1506 | `}` |
|   4422798 | 1507 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 | 1508 | `{` |
|         - | 1509 | `	sxu32 nUsed;` |
|   4422803 | 1510 | `	sxi32 rcOut = SXRET_OK;` |
|   4429149 | 1511 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|      6351 | 1512 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|      6351 | 1513 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|      6351 | 1514 | `		(void)SySetPop(&pVm->aException);` |
|      6351 | 1515 | `		pExc->pFrame = 0;` |
|         - | 1516 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - | 1517 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - | 1518 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - | 1519 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - | 1520 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|      6351 | 1521 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|      6351 | 1522 | `			VmLeaveFrame(&(*pVm));` |
|      3173 | 1523 | `		}` |
|      6385 | 1524 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - | 1525 | `			sxi32 rcF;` |
|        73 | 1526 | `			pExc->iFinallyDone = 1;` |
|        73 | 1527 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        73 | 1528 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        73 | 1529 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 | 1530 | `				return SXERR_ABORT;` |
|         - | 1531 | `			}` |
|        73 | 1532 | `			if( rcF == PH7_EXCEPTION ){` |
|         - | 1533 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - | 1534 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - | 1535 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 | 1536 | `				rcOut = PH7_EXCEPTION;` |
|         2 | 1537 | `			}` |
|        39 | 1538 | `		}else{` |
|      6282 | 1539 | `			VmExcRelease(&(*pVm),pExc);` |
|         - | 1540 | `		}` |
|         5 | 1541 | `	}` |
|   4422803 | 1542 | `	return rcOut;` |
|   2211105 | 1543 | `}` |
|         - | 1544 | `/*` |
|         - | 1545 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - | 1546 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - | 1547 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - | 1548 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - | 1549 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - | 1550 | ` */` |
|   3695172 | 1551 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 | 1552 | `{` |
|   3695177 | 1553 | `	pFrame->bHasRet = 0;` |
|   3695177 | 1554 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   3695177 | 1555 | `	pFrame->nCatchJmpPc = 0;` |
|   3695177 | 1556 | `}` |
|         - | 1557 | `/*` |
|         - | 1558 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - | 1559 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - | 1560 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - | 1561 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - | 1562 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - | 1563 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - | 1564 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1565 | ` */` |
|     23986 | 1566 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1567 | `{` |
|     23991 | 1568 | `	if( pResult ){` |
|     23991 | 1569 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     11993 | 1570 | `	}` |
|     23991 | 1571 | `	VmClearFramePending(pEntryFrame);` |
|     23991 | 1572 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|       ! 0 | 1573 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 1574 | `	}` |
|     23991 | 1575 | `}` |
|         - | 1576 | `/*` |
|         - | 1577 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1578 | ` */` |
|         4 | 1579 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         1 | 1580 | `{` |
|         5 | 1581 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|         5 | 1582 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|         5 | 1583 | `	const char *zSin = pSecond->zString;` |
|         5 | 1584 | `	const char *zFin = pFirst->zString;` |
|         5 | 1585 | `	const char *zPtr = zFin;` |
|         2 | 1586 | `	for(;;){` |
|         5 | 1587 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|         3 | 1588 | `			break;` |
|         - | 1589 | `		}` |
|       ! 0 | 1590 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1591 | `			/* mismatch */` |
|       ! 0 | 1592 | `			break;` |
|         - | 1593 | `		}` |
|       ! 0 | 1594 | `		zFin++;` |
|       ! 0 | 1595 | `		zSin++;` |
|       ! 0 | 1596 | `	}` |
|         5 | 1597 | `	return (int)(zFin-zPtr);` |
|         1 | 1598 | `}` |
|         - | 1599 | `/*` |
|         - | 1600 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1601 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1602 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1603 | ` * Refer to the official documentation for more information.` |
|         - | 1604 | ` */` |
|        72 | 1605 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1606 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1607 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1608 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1609 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1610 | `	)` |
|         4 | 1611 | `{` |
|         - | 1612 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1613 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1614 | `	ph7_vm_func *pLink;` |
|         - | 1615 | `	SyString sArgSig;` |
|         - | 1616 | `	SyBlob sSig;` |
|         - | 1617 |  |
|        76 | 1618 | `	pLink = pList;` |
|        76 | 1619 | `	i = 0;` |
|         - | 1620 | `	/* Put functions expecting the same number of passed arguments */` |
|       636 | 1621 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|       584 | 1622 | `		if( pLink == 0 ){` |
|        24 | 1623 | `			break;` |
|         - | 1624 | `		}` |
|       564 | 1625 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1626 | `			/* Candidate for overloading */` |
|       564 | 1627 | `			apSet[i++] = pLink;` |
|       280 | 1628 | `		}` |
|         - | 1629 | `		/* Point to the next entry */` |
|       564 | 1630 | `		pLink = pLink->pNextName;` |
|         4 | 1631 | `	}` |
|        76 | 1632 | `	if( i < 1 ){` |
|         - | 1633 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1634 | `		return pList;` |
|         - | 1635 | `	}` |
|        76 | 1636 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1637 | `		/* Return the only candidate */` |
|        74 | 1638 | `		return apSet[0];` |
|         - | 1639 | `	}` |
|         - | 1640 | `	/* Calculate function signature */` |
|         3 | 1641 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|         5 | 1642 | `	for( j = 0 ; j < nArg ; j++ ){` |
|         3 | 1643 | `		int c = 'n'; /* null */` |
|         3 | 1644 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1645 | `			/* Hashmap */` |
|       ! 0 | 1646 | `			c = 'h';` |
|         3 | 1647 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1648 | `			/* bool */` |
|       ! 0 | 1649 | `			c = 'b';` |
|         3 | 1650 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1651 | `			/* int */` |
|         3 | 1652 | `			c = 'i';` |
|         1 | 1653 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1654 | `			/* String */` |
|       ! 0 | 1655 | `			c = 's';` |
|       ! 0 | 1656 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1657 | `			/* Float */` |
|       ! 0 | 1658 | `			c = 'f';` |
|       ! 0 | 1659 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1660 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1661 | `			int marker = 'o';` |
|       ! 0 | 1662 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1663 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1664 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1665 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1666 | `			c = -1;` |
|       ! 0 | 1667 | `		}` |
|         3 | 1668 | `		if( c > 0 ){` |
|         3 | 1669 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|         1 | 1670 | `		}` |
|         2 | 1671 | `	}` |
|         3 | 1672 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|         3 | 1673 | `	iTarget = 0;` |
|         3 | 1674 | `	iMax = -1;` |
|         - | 1675 | `	/* Select the appropriate function */` |
|         7 | 1676 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1677 | `		/* Compare the two signatures */` |
|         5 | 1678 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|         5 | 1679 | `		if( iCur > iMax ){` |
|         3 | 1680 | `			iMax = iCur;` |
|         3 | 1681 | `			iTarget = j;` |
|         1 | 1682 | `		}` |
|         3 | 1683 | `	}` |
|         3 | 1684 | `	SyBlobRelease(&sSig);` |
|         - | 1685 | `	/* Appropriate function for the current call context */` |
|         3 | 1686 | `	return apSet[iTarget];` |
|        40 | 1687 | `}` |
|         - | 1688 | `/* Forward declaration */` |
|         - | 1689 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1690 | `/*` |
|         - | 1691 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot.` |
|         - | 1692 | ` *` |
|         - | 1693 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1694 | ` * end-of-exec. When the pool was one doubling buffer, an initializer that` |
|         - | 1695 | ` * allocated pool memobjs — a large array literal reserves one per element —` |
|         - | 1696 | ` * reallocated and FREED that buffer, so a pResult pointing into it dangled and` |
|         - | 1697 | ` * the final store was a heap use-after-free (confirmed via ASan on a >=~227` |
|         - | 1698 | ` * element class-const array; it is what blocked Composer's autoload class-map).` |
|         - | 1699 | ` * The answer was to evaluate into a stable local and store into the slot` |
|         - | 1700 | ` * re-fetched by its index, which is what this does. Redundant since P1 -- the` |
|         - | 1701 | ` * pool's segments are fixed, so a slot's address never moves. Left for the` |
|         - | 1702 | ` * harvest sweep (PERF.md P1); the history above is why it was ever needed.` |
|         - | 1703 | ` *` |
|         - | 1704 | ` * On return *ppMemObj points at the valid post-eval slot. PH7_MemObjStore` |
|         - | 1705 | ` * preserves the destination slot's nIdx (excluded from its memcpy), so the slot` |
|         - | 1706 | ` * identity is kept. Mirrors the enum-case backing path.` |
|         - | 1707 | ` */` |
|      4254 | 1708 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1709 | `{` |
|         - | 1710 | `	ph7_value sVal;` |
|      4259 | 1711 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1712 | `	sxi32 rc;` |
|      4259 | 1713 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      4259 | 1714 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1715 | `	/* Re-fetch by the reserved index (redundant since P1: see above). */` |
|      4259 | 1716 | `	*ppMemObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|      4259 | 1717 | `	if( *ppMemObj ){` |
|      4259 | 1718 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      2127 | 1719 | `	}` |
|      4259 | 1720 | `	PH7_MemObjRelease(&sVal);` |
|      4259 | 1721 | `	return rc;` |
|         5 | 1722 | `}` |
|         - | 1723 | `/*` |
|         - | 1724 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - | 1725 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - | 1726 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - | 1727 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - | 1728 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - | 1729 | ` * C caller ever regains control.` |
|         - | 1730 | ` */` |
|         - | 1731 | `typedef struct VmMuteState {` |
|         - | 1732 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - | 1733 | `	sxu32 nSaved;` |
|         - | 1734 | `	sxi32 iSaveStatus;` |
|         - | 1735 | `	sxi32 iSaveBoundary;` |
|         - | 1736 | `	VmResumeTarget sSaveResume;` |
|         - | 1737 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - | 1738 | `	ph7_class *pSaveCycleClass;` |
|         - | 1739 | `} VmMuteState;` |
|       594 | 1740 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 | 1741 | `{` |
|       599 | 1742 | `	pSave->apSaved = 0;` |
|       599 | 1743 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|       599 | 1744 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|       599 | 1745 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|       599 | 1746 | `	VmSaveResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|       599 | 1747 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|       599 | 1748 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|       599 | 1749 | `	if( pSave->nSaved > 0 ){` |
|       410 | 1750 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       204 | 1751 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|       206 | 1752 | `		if( pSave->apSaved ){` |
|       308 | 1753 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|       204 | 1754 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|       206 | 1755 | `			SySetReset(&pVm->aException);` |
|       102 | 1756 | `		}` |
|       102 | 1757 | `	}` |
|       599 | 1758 | `	pVm->nMuteThrow++;` |
|       599 | 1759 | `}` |
|         - | 1760 | `/*` |
|         - | 1761 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - | 1762 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - | 1763 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - | 1764 | ` * TRUE when a throw was actually swallowed.` |
|         - | 1765 | ` */` |
|       594 | 1766 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 | 1767 | `{` |
|         - | 1768 | `	VmFrame *pFrame;` |
|       599 | 1769 | `	pVm->nMuteThrow--;` |
|         - | 1770 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - | 1771 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - | 1772 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - | 1773 | `	 * hidden. */` |
|       599 | 1774 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       599 | 1775 | `	SySetReset(&pVm->aException);` |
|       599 | 1776 | `	if( pSave->apSaved ){` |
|         - | 1777 | `		sxu32 k;` |
|       410 | 1778 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|       206 | 1779 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|       104 | 1780 | `		}` |
|       206 | 1781 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|       206 | 1782 | `		pSave->apSaved = 0;` |
|       102 | 1783 | `	}` |
|       599 | 1784 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|       549 | 1785 | `		return FALSE;` |
|         - | 1786 | `	}` |
|        53 | 1787 | `	pFrame = pVm->pFrame;` |
|        53 | 1788 | `	if( pFrame ){` |
|        53 | 1789 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        53 | 1790 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        25 | 1791 | `	}` |
|        53 | 1792 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        53 | 1793 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        53 | 1794 | `	VmRestoreResumeTarget(&(*pVm),&pSave->sSaveResume);` |
|        53 | 1795 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        53 | 1796 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        53 | 1797 | `	return TRUE;` |
|       302 | 1798 | `}` |
|         - | 1799 | `/*` |
|         - | 1800 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - | 1801 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - | 1802 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - | 1803 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - | 1804 | ` *` |
|         - | 1805 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - | 1806 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - | 1807 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - | 1808 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - | 1809 | ` */` |
|       198 | 1810 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - | 1811 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 1812 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - | 1813 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - | 1814 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - | 1815 | `	int nArg,                    /* Argument count */` |
|         - | 1816 | `	ph7_value **apArg,           /* Arguments */` |
|         - | 1817 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - | 1818 | `	)` |
|         1 | 1819 | `{` |
|         - | 1820 | `	VmMuteState sSave;` |
|         - | 1821 | `	sxi32 rc;` |
|       199 | 1822 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       199 | 1823 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|       199 | 1824 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         7 | 1825 | `		if( pbThrew ){` |
|         7 | 1826 | `			*pbThrew = TRUE;` |
|         3 | 1827 | `		}` |
|         7 | 1828 | `		return SXRET_OK;` |
|         - | 1829 | `	}` |
|       193 | 1830 | `	if( pbThrew ){` |
|       193 | 1831 | `		*pbThrew = FALSE;` |
|        96 | 1832 | `	}` |
|       193 | 1833 | `	return rc;` |
|       100 | 1834 | `}` |
|         - | 1835 | `/*` |
|         - | 1836 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1837 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1838 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1839 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1840 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1841 | ` *` |
|         - | 1842 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1843 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1844 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1845 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1846 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1847 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1848 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1849 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1850 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1851 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1852 | ` */` |
|       380 | 1853 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1854 | `{` |
|         - | 1855 | `	VmMuteState sSave;` |
|         - | 1856 | `	sxi32 rc;` |
|       385 | 1857 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       385 | 1858 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       385 | 1859 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - | 1860 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1861 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1862 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1863 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1864 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1865 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1866 | `	}` |
|         - | 1867 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - | 1868 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - | 1869 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - | 1870 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - | 1871 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - | 1872 | `	 * detects the cycle again. */` |
|       385 | 1873 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       385 | 1874 | `	return rc;` |
|         5 | 1875 | `}` |
|         - | 1876 | `/*` |
|         - | 1877 | ` * Run a compiled constant expression only to LOOK at the value it produces, and` |
|         - | 1878 | ` * report whether php's own compiler would have FOLDED it.` |
|         - | 1879 | ` *` |
|         - | 1880 | ` * php folds a parameter default at compile time and keeps the folded zval; what` |
|         - | 1881 | `` * it cannot reduce stays an AST and prints as `<expression>` in the declaration`` |
|         - | 1882 | ` * php renders for an incompatible-override fatal (see PH7_ClassRenderDecl).` |
|         - | 1883 | ``  * "Cannot reduce" is not a syntactic property -- `2 * 1024` folds and `1 / 0` `` |
|         - | 1884 | ` * does not -- so the question is asked by RUNNING the program and watching for` |
|         - | 1885 | ` * anything php's folder would have refused on.` |
|         - | 1886 | ` *` |
|         - | 1887 | ` * The window is doubly sealed, because this runs at CLASS-LINK time: a program` |
|         - | 1888 | ` * php has not reached, in the middle of compiling one it has. VmMuteEnter hides` |
|         - | 1889 | `` * the live try activations and swallows a throw (`1/0`, `"a"+1`), and`` |
|         - | 1890 | ` * nSpeculative drops every diagnostic without running a user error handler or` |
|         - | 1891 | `` * touching error_get_last() (`[1,2][5]`). Either one having happened is exactly`` |
|         - | 1892 | ` * php's "did not fold".` |
|         - | 1893 | ` *` |
|         - | 1894 | ` * Returns TRUE with *pOut holding the value, or FALSE (caller prints` |
|         - | 1895 | `` * `<expression>`). The caller must have SCREENED the program first: only a run`` |
|         - | 1896 | ` * built from literal loads and pure value operators belongs here -- a constant` |
|         - | 1897 | `` * NAME, a class constant and a `new` are all things php keeps unfolded and this`` |
|         - | 1898 | ` * would happily evaluate (or construct).` |
|         - | 1899 | ` */` |
|        16 | 1900 | `PH7_PRIVATE int PH7_VmEvalConstExpr(ph7_vm *pVm,SySet *pByteCode,ph7_value *pOut)` |
|         2 | 1901 | `{` |
|         - | 1902 | `	VmMuteState sSave;` |
|         - | 1903 | `	sxu32 nDiag;` |
|         - | 1904 | `	sxi32 rc;` |
|         - | 1905 | `	int bFolded;` |
|        18 | 1906 | `	int bFramePushed = 0;` |
|        18 | 1907 | `	if( pVm->pFrame == 0 ){` |
|         - | 1908 | `		/* CLASS-LINK time: the script has not started, so there is no frame at all --` |
|         - | 1909 | `		 * and the reference table, the array builder and the throw path all read one.` |
|         - | 1910 | `		 * Stand a global-shaped frame up for the duration (pParent == 0, so its` |
|         - | 1911 | `		 * teardown is the global frame's: nothing of the caller's is torn down with` |
|         - | 1912 | `		 * it). Without this an array default segfaulted the compiler. */` |
|       ! 0 | 1913 | `		if( VmEnterFrame(&(*pVm),0,0,0) != SXRET_OK ){` |
|       ! 0 | 1914 | `			return 0;` |
|         - | 1915 | `		}` |
|       ! 0 | 1916 | `		bFramePushed = 1;` |
|       ! 0 | 1917 | `	}` |
|        18 | 1918 | `	VmMuteEnter(&(*pVm),&sSave);` |
|        18 | 1919 | `	pVm->nSpeculative++;` |
|        18 | 1920 | `	nDiag = pVm->nSpecDiag;` |
|        18 | 1921 | `	rc = VmLocalExec(&(*pVm),pByteCode,pOut,FALSE);` |
|        18 | 1922 | `	pVm->nSpeculative--;` |
|        18 | 1923 | `	bFolded = ( rc == SXRET_OK && pVm->nSpecDiag == nDiag );` |
|        18 | 1924 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|       ! 0 | 1925 | `		bFolded = 0; /* a throw was swallowed */` |
|       ! 0 | 1926 | `	}` |
|        18 | 1927 | `	if( bFramePushed ){` |
|       ! 0 | 1928 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 1929 | `	}` |
|        18 | 1930 | `	return bFolded;` |
|        10 | 1931 | `}` |
|         - | 1932 | `/*` |
|         - | 1933 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1934 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1935 | ` */` |
|         - | 1936 | `/*` |
|         - | 1937 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1938 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1939 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1940 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1941 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1942 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1943 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1944 | ` */` |
|   3670994 | 1945 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1946 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1947 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1948 | `	)` |
|         5 | 1949 | `{` |
|         - | 1950 | `	ph7_class_attr *pAttr;` |
|         - | 1951 | `	SyHashEntry *pEntry;` |
|         - | 1952 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1953 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1954 | `	 * over both tables. */` |
|         - | 1955 | `	SyHash *apMount[2];` |
|         - | 1956 | `	int iMount;` |
|   3670999 | 1957 | `	apMount[0] = &pClass->hAttr;` |
|   3670999 | 1958 | `	apMount[1] = &pClass->hConst;` |
|  11012979 | 1959 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1960 | `	/* Reset the loop cursor */` |
|   7341993 | 1961 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1962 | `	/* Process only static and constant attribute */` |
|  30736953 | 1963 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1964 | `		/* Extract the current attribute */` |
|  23394973 | 1965 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  23394968 | 1966 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  16069412 | 1967 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   4377694 | 1968 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1969 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1970 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1971 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1972 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1973 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1974 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1975 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1976 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1977 | `			 * %s as value for class constant" fatal without any access). */` |
|   8762603 | 1978 | `			continue;` |
|         - | 1979 | `		}` |
|  14632375 | 1980 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1981 | `			ph7_value *pMemObj;` |
|      7923 | 1982 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1983 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1984 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1985 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1986 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1987 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1988 | `				 * access / instantiation throws like php's. */` |
|      7491 | 1989 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|      5023 | 1990 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1991 | `						/* Its default threw at the other class's mount and is` |
|         - | 1992 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1993 | `						 * both static tables). */` |
|         3 | 1994 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|      5022 | 1995 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        20 | 1996 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|        12 | 1997 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|        14 | 1998 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1999 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 2000 | `						}` |
|         6 | 2001 | `					}` |
|      2510 | 2002 | `				}` |
|      7494 | 2003 | `				continue;` |
|         - | 2004 | `			}` |
|         - | 2005 | `			/* Reserve a memory object for this constant/static attribute */` |
|       435 | 2006 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       435 | 2007 | `			if( pMemObj == 0 ){` |
|       ! 0 | 2008 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 2009 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 2010 | `					&pClass->sName,&pAttr->sName` |
|         - | 2011 | `					);` |
|       ! 0 | 2012 | `				return SXERR_MEM;` |
|         - | 2013 | `			}` |
|       435 | 2014 | `			if( pAttr->pNativeValue ){` |
|         - | 2015 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 2016 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       435 | 2017 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 2018 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 2019 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 2020 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       385 | 2021 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       385 | 2022 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|       385 | 2023 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 2024 | `				sxi32 rcExec;` |
|       385 | 2025 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|         - | 2026 | `				/* ...and the frame marker is what makes that fallback reachable when a` |
|         - | 2027 | `				 * frame IS current: a class declared inside a METHOD mounts here, and` |
|         - | 2028 | `				 * without the marker PH7_VmPeekDeclaringClass answers that method's` |
|         - | 2029 | ``				 * class instead (`class G { function go(){ eval('class Q { const K=5;`` |
|         - | 2030 | ``				 * public static $s = self::K; }'); } }` read G::K). */`` |
|       385 | 2031 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|       385 | 2032 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       385 | 2033 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       385 | 2034 | `				pVm->nConstEvalDepth++;` |
|         - | 2035 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 2036 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 2037 | `				 * not something it can see. What reaches this line is a static` |
|         - | 2038 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 2039 | `				 * which php does validate here, but only when the initializer` |
|         - | 2040 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 2041 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 2042 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       385 | 2043 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       385 | 2044 | `				pVm->nConstEvalDepth--;` |
|       385 | 2045 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       385 | 2046 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       385 | 2047 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|       385 | 2048 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 2049 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 2050 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 2051 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 2052 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 2053 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 2054 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 2055 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 2056 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 2057 | `					 * unset nIdx. */` |
|        46 | 2058 | `					if( bStaticProp ){` |
|        40 | 2059 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        40 | 2060 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        21 | 2061 | `					}else{` |
|         - | 2062 | `						/* Release before recycling: the value a muted eval that` |
|         - | 2063 | `						 * only recorded a CYCLE left here must go before the` |
|         - | 2064 | `						 * slot's dead nIdx word becomes the free-list link. */` |
|         7 | 2065 | `						PH7_MemObjRelease(pMemObj);` |
|         7 | 2066 | `						VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|         7 | 2067 | `						continue;` |
|         - | 2068 | `					}` |
|       357 | 2069 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|       173 | 2070 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 2071 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 2072 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 2073 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        55 | 2074 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        55 | 2075 | `					if( rcType != SXRET_OK ){` |
|        11 | 2076 | `						return rcType;` |
|         - | 2077 | `					}` |
|        21 | 2078 | `				}` |
|       183 | 2079 | `			}` |
|         - | 2080 | `			/* Record attribute index */` |
|       421 | 2081 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 2082 | `			/* Install static attribute in the reference table */` |
|       421 | 2083 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 2084 | `			/* If this is a typed static property, register the slot so the` |
|         - | 2085 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 2086 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 2087 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 2088 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 2089 | `			 * already enforced above, so they need no store-time slot. */` |
|       416 | 2090 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       280 | 2091 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        97 | 2092 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        97 | 2093 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 2094 | `					return SXERR_MEM;` |
|         - | 2095 | `				}` |
|        97 | 2096 | `				pVmAttrS->pAttr = pAttr;` |
|        97 | 2097 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        97 | 2098 | `				pVmAttrS->iState = 0;` |
|        97 | 2099 | `				pVmAttrS->pOwner = pClass;` |
|        97 | 2100 | `				pVmAttrS->pInst = 0;   /* the class's own slot: no instance behind it */` |
|         - | 2101 | `				/* Static typed property with no default starts uninitialized` |
|         - | 2102 | `				 * (constants are already excluded by the enclosing condition). */` |
|        97 | 2103 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        28 | 2104 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        85 | 2105 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 2106 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 2107 | `					 * typed static default LAZILY at the first static-property` |
|         - | 2108 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 2109 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 2110 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 2111 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 2112 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 2113 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 2114 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 2115 | `					 * the materializer checks it after the re-run. */` |
|        73 | 2116 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        24 | 2117 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        24 | 2118 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 2119 | `					}` |
|        34 | 2120 | `				}` |
|        97 | 2121 | `				if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 2122 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 2123 | `					return SXERR_MEM;` |
|         - | 2124 | `				}` |
|        46 | 2125 | `			}` |
|       208 | 2126 | `		}` |
|         5 | 2127 | `	}` |
|   3667053 | 2128 | `	} /* for iMount */` |
|   3670991 | 2129 | `	return SXRET_OK;` |
|   1833531 | 2130 | `}` |
|         - | 2131 | `/*` |
|         - | 2132 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 2133 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 2134 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 2135 | ` */` |
|   3668800 | 2136 | `static sxi32 VmMountUserClassMethods(` |
|         - | 2137 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 2138 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 2139 | `	)` |
|         5 | 2140 | `{` |
|         - | 2141 | `	ph7_class_method *pMeth;` |
|         - | 2142 | `	SyHashEntry *pEntry;` |
|         - | 2143 | `	sxi32 rc;` |
|         - | 2144 | `	/* Install class methods */` |
|   3668805 | 2145 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 2146 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 2147 | `		 */` |
|   1270413 | 2148 | `		return SXRET_OK;` |
|         - | 2149 | `	}` |
|         - | 2150 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 2151 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 2152 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 2153 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 2154 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 2155 | `	/* Install the methods now */` |
|   2398397 | 2156 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  40102407 | 2157 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  36503073 | 2158 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  36503073 | 2159 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  36453639 | 2160 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  36453639 | 2161 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2162 | `				return rc;` |
|         - | 2163 | `			}` |
|  18200240 | 2164 | `		}` |
|         5 | 2165 | `	}` |
|         - | 2166 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   2398397 | 2167 | `	pClass->bMounted = TRUE;` |
|   2398397 | 2168 | `	return SXRET_OK;` |
|   1832434 | 2169 | `}` |
|   2436483 | 2170 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 2171 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 2172 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 2173 | `	)` |
|         5 | 2174 | `{` |
|         - | 2175 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 2176 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 2177 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 2178 | `	 * initializer that throws finds the exception classes ready. */` |
|   2436488 | 2179 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   2436488 | 2180 | `	if( rc != SXRET_OK ){` |
|         3 | 2181 | `		return rc;` |
|         - | 2182 | `	}` |
|   2436486 | 2183 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|   1217261 | 2184 | `}` |
|         - | 2185 | `/*` |
|         - | 2186 | ` * Allocate a private frame for attributes of the given` |
|         - | 2187 | ` * class instance (Object in the PHP jargon).` |
|         - | 2188 | ` */` |
|   1621673 | 2189 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 2190 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2191 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 2192 | `	)` |
|         5 | 2193 | `{` |
|   1621678 | 2194 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 2195 | `	ph7_class_attr *pAttr;` |
|         - | 2196 | `	SyHashEntry *pEntry;` |
|         - | 2197 | `	sxi32 rc;` |
|   1621678 | 2198 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 2199 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 2200 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 2201 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1621678 | 2202 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  12209661 | 2203 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 2204 | `		VmClassAttr *pVmAttr;` |
|         - | 2205 | `		/* The KEY the class filed it under, not the attribute's own name: an` |
|         - | 2206 | `		 * inherited PRIVATE instance property lives under php's mangled storage` |
|         - | 2207 | ``		 * name (PH7_ClassAttrStorageName), which is what keeps a base's `$q` and a`` |
|         - | 2208 | ``		 * child's `$q` two slots on one object instead of one. */`` |
|  10587988 | 2209 | `		const void *pKey = pEntry->pKey;` |
|  10587988 | 2210 | `		sxu32 nKeyLen = pEntry->nKeyLen;` |
|         - | 2211 | `		/* Extract the current attribute */` |
|  10587988 | 2212 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10587988 | 2213 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT ){` |
|         - | 2214 | `			/* php's VIRTUAL property: the class declares the name and answers it` |
|         - | 2215 | `			 * from its own state, and the OBJECT has no slot for it at all -- so` |
|         - | 2216 | `			 * every table walk (the (array) cast, get_object_vars, foreach,` |
|         - | 2217 | `			 * json_encode, var_export, serialize) finds nothing, and a read, a` |
|         - | 2218 | `			 * write or an isset() takes the miss path to the class's magic trio.` |
|         - | 2219 | `			 * Nothing ever installs one; unlike the LAZY kind there is no C body` |
|         - | 2220 | `			 * behind it that would. */` |
|    147561 | 2221 | `			continue;` |
|         - | 2222 | `		}` |
|  10440432 | 2223 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY ){` |
|         - | 2224 | `			/* A property php's own object does not HOLD until its constructor` |
|         - | 2225 | `			 * fills it: no slot, no hAttr entry, nothing for a read, an isset()` |
|         - | 2226 | `			 * or a property walk to find. PH7_NativeMaterializeLazy installs the` |
|         - | 2227 | `			 * whole set the first time a C body writes one. */` |
|      7683 | 2228 | `			continue;` |
|         - | 2229 | `		}` |
|  10432752 | 2230 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10432752 | 2231 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 2232 | `			return SXERR_MEM;` |
|         - | 2233 | `		}` |
|  10432752 | 2234 | `		pVmAttr->pAttr = pAttr;` |
|  10432752 | 2235 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 2236 | `			ph7_value *pMemObj;` |
|         - | 2237 | `			/* Reserve a memory object for this attribute */` |
|  10431416 | 2238 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10431416 | 2239 | `			if( pMemObj == 0 ){` |
|       ! 0 | 2240 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2241 | `				return SXERR_MEM;` |
|         - | 2242 | `			}` |
|  10431416 | 2243 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10431416 | 2244 | `			pVmAttr->iState = 0;` |
|  10431416 | 2245 | `			pVmAttr->pOwner = pClass;` |
|  10431416 | 2246 | `			pVmAttr->pInst = pObj;` |
|  10431416 | 2247 | `			if( pAttr->pNativeValue ){` |
|         - | 2248 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 2249 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 2250 | `				 * literal cannot throw and the builder states the type itself. */` |
|   9949413 | 2251 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5456145 | 2252 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 2253 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 2254 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 2255 | ``				 * against the declaring class. This runs at `new`, i.e. at an`` |
|         - | 2256 | `				 * arbitrary point in execution -- typically inside some OTHER` |
|         - | 2257 | `				 * class's method, whose frame is still current because` |
|         - | 2258 | `				 * VmLocalExec pushes none of its own. Mark that frame, or` |
|         - | 2259 | `				 * PH7_VmPeekDeclaringClass answers the caller's class and` |
|         - | 2260 | ``				 * `public $v = self::K` read F::K when `new` ran in F::make(). */`` |
|      3335 | 2261 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|      3335 | 2262 | `				void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 2263 | `				sxi32 rcExec;` |
|      3335 | 2264 | `				pVm->pConstEvalClass = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      3335 | 2265 | `				pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|      3335 | 2266 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      3335 | 2267 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      3335 | 2268 | `				pVm->pConstEvalFrame = pSaveFrame;` |
|      3335 | 2269 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 2270 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 2271 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 2272 | `					 * also type-check the leftover value (a spurious second` |
|         - | 2273 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 2274 | `					 * nothing further for the remaining attributes.` |
|         - | 2275 | `					 *` |
|         - | 2276 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 2277 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 2278 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 2279 | `					 * and the throw came back as a status nobody read: this` |
|         - | 2280 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 2281 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 2282 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 2283 | `					 * existing construction-aborted route. */` |
|        26 | 2284 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        26 | 2285 | `					bDefThrew = 1;` |
|      3323 | 2286 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 2287 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 2288 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 2289 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 2290 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 2291 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 2292 | `					 * aborts construction) / the fetch-point router. */` |
|       565 | 2293 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       565 | 2294 | `					if( rcDef != SXRET_OK ){` |
|        13 | 2295 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 2296 | `						bDefThrew = 1;` |
|         6 | 2297 | `					}` |
|       285 | 2298 | `				}` |
|    480343 | 2299 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 2300 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 2301 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    476796 | 2302 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    238355 | 2303 | `			}` |
|  10431416 | 2304 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
|  10431416 | 2305 | `			if( rc != SXRET_OK ){` |
|         - | 2306 | `				/* Restore the reserved (NULL-valued) slot to the free list */` |
|       ! 0 | 2307 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 | 2308 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2309 | `				return SXERR_MEM;` |
|         - | 2310 | `			}` |
|         - | 2311 | `			/* Install attribute in the reference table */` |
|  10431416 | 2312 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 2313 | `			/* Register the slot with the store filter -- a declared TYPE to` |
|         - | 2314 | `			 * enforce, a native class's write handler, or both. On failure roll` |
|         - | 2315 | `			 * back the just-installed hAttr entry and the reserved memobj so the` |
|         - | 2316 | `			 * caller sees a consistent instance. */` |
|  10431416 | 2317 | `			rc = PH7_VmStoreFilterRegister(&(*pVm),pVmAttr);` |
|  10431416 | 2318 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2319 | `				SyHashDeleteEntry(&pObj->hAttr,pKey,nKeyLen,0);` |
|       ! 0 | 2320 | `				VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 | 2321 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2322 | `				return SXERR_MEM;` |
|         - | 2323 | `			}` |
|   5215103 | 2324 | `		}else{` |
|         - | 2325 | `			/* Install static/constant attribute */` |
|      1341 | 2326 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|      1341 | 2327 | `			pVmAttr->iState = 0;` |
|      1341 | 2328 | `			pVmAttr->pOwner = pClass;` |
|      1341 | 2329 | `			pVmAttr->pInst = 0;   /* a static slot belongs to the class, not to this object */` |
|      1341 | 2330 | `			rc = SyHashInsertTail(&pObj->hAttr,pKey,nKeyLen,pVmAttr);` |
|      1341 | 2331 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 2332 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2333 | `				return SXERR_MEM;` |
|         - | 2334 | `			}` |
|         - | 2335 | `		}` |
|         5 | 2336 | `	}` |
|   1621678 | 2337 | `	return SXRET_OK;` |
|    810674 | 2338 | `}` |
|         - | 2339 | `/*` |
|         - | 2340 | ` * Whether [pClass] permits runtime-created (dynamic) properties: stdClass, and` |
|         - | 2341 | `` * any class php's own `#[AllowDynamicProperties]` opts in (the attribute is`` |
|         - | 2342 | ` * inherited, so the ancestry is walked -- VmClassHasAttributeNamed does that).` |
|         - | 2343 | ` *` |
|         - | 2344 | ` * The attribute half used to be spelled out at each caller, and one of the three` |
|         - | 2345 | ` * did not have it: the by-REFERENCE binder (VmBindPropByRef) asked this alone, so` |
|         - | 2346 | `` * an opted-in class refused `f($o->undeclared)` with §10's `Cannot create dynamic`` |
|         - | 2347 | `` * property` on a write php performs -- while `$o->undeclared = 1` next to it`` |
|         - | 2348 | ` * worked. One decision, one site.` |
|         - | 2349 | ` */` |
|       328 | 2350 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 2351 | `{` |
|       333 | 2352 | `	if( pVm->pStdClass != 0 ){` |
|         - | 2353 | `		/* ...and stdClass's own permission is INHERITED, exactly as the attribute` |
|         - | 2354 | ``		 * is: php deprecates nothing for `class C extends stdClass`, so §10 must`` |
|         - | 2355 | `		 * refuse nothing there either. Only the class ITSELF was recognized, so a` |
|         - | 2356 | `		 * subclass of the one class php lets a script build freely could not take a` |
|         - | 2357 | `		 * property at all. */` |
|       333 | 2358 | `		ph7_class *pAncestor = pClass;` |
|       425 | 2359 | `		while( pAncestor ){` |
|       345 | 2360 | `			if( pAncestor == pVm->pStdClass ){` |
|       252 | 2361 | `				return TRUE;` |
|         - | 2362 | `			}` |
|        97 | 2363 | `			pAncestor = pAncestor->pBase;` |
|         5 | 2364 | `		}` |
|        40 | 2365 | `	}` |
|        85 | 2366 | `	return VmClassHasAttributeNamed(pClass,"AllowDynamicProperties",` |
|         - | 2367 | `		sizeof("AllowDynamicProperties")-1);` |
|       169 | 2368 | `}` |
|         - | 2369 | `/*` |
|         - | 2370 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 2371 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 2372 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 2373 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 2374 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 2375 | ` */` |
|        80 | 2376 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         5 | 2377 | `{` |
|       141 | 2378 | `	while( pClass ){` |
|        91 | 2379 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 2380 | `		sxu32 n;` |
|        91 | 2381 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        30 | 2382 | `			if( aAttr[n].sName.nByte == nName` |
|        33 | 2383 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        33 | 2384 | `				return TRUE;` |
|         - | 2385 | `			}` |
|       ! 0 | 2386 | `		}` |
|        61 | 2387 | `		pClass = pClass->pBase;` |
|         5 | 2388 | `	}` |
|        55 | 2389 | `	return FALSE;` |
|        45 | 2390 | `}` |
|         - | 2391 | `/*` |
|         - | 2392 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - | 2393 | ` * access or method call on such an instance is php's incomplete-object` |
|         - | 2394 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - | 2395 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - | 2396 | ` */` |
|    278775 | 2397 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 2398 | `{` |
|    278780 | 2399 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 | 2400 | `}` |
|         - | 2401 | `/*` |
|         - | 2402 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - | 2403 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - | 2404 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - | 2405 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - | 2406 | ` * had one says "unknown", like php.` |
|         - | 2407 | ` */` |
|        44 | 2408 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 | 2409 | `{` |
|        45 | 2410 | `	const char *zName = "unknown";` |
|        45 | 2411 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 | 2412 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - | 2413 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 | 2414 | `	if( pEntry ){` |
|        45 | 2415 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 | 2416 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 | 2417 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 | 2418 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 | 2419 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 | 2420 | `		}` |
|        22 | 2421 | `	}` |
|        67 | 2422 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - | 2423 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - | 2424 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 | 2425 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 | 2426 | `}` |
|         - | 2427 | `/*` |
|         - | 2428 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - | 2429 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - | 2430 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - | 2431 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - | 2432 | ` * (property_exists() passes its name in pFuncName).` |
|         - | 2433 | ` */` |
|        16 | 2434 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 | 2435 | `{` |
|         - | 2436 | `	SyBlob sMsg;` |
|        17 | 2437 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 | 2438 | `	if( pFuncName ){` |
|       ! 0 | 2439 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 | 2440 | `	}else{` |
|        17 | 2441 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 | 2442 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 | 2443 | `		if( pFunc == 0 ){` |
|       ! 0 | 2444 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 | 2445 | `		}else{` |
|        17 | 2446 | `			const char *zDisp = 0;` |
|         - | 2447 | `			int nDisp;` |
|        17 | 2448 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 | 2449 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 | 2450 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 | 2451 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 | 2452 | `			}` |
|        17 | 2453 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 | 2454 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - | 2455 | `		}` |
|         - | 2456 | `	}` |
|        17 | 2457 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 | 2458 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 | 2459 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 | 2460 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 | 2461 | `	SyBlobRelease(&sMsg);` |
|        17 | 2462 | `}` |
|         - | 2463 | `/*` |
|         - | 2464 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 2465 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 2466 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 2467 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 2468 | ` *` |
|         - | 2469 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 2470 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 2471 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 2472 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 2473 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 2474 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 2475 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 2476 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 2477 | ` */` |
|       624 | 2478 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         5 | 2479 | `{` |
|         - | 2480 | `	ph7_class_attr *pAttr;` |
|       629 | 2481 | `	VmClassAttr *pVmAttr = 0;` |
|       629 | 2482 | `	ph7_value *pMemObj = 0;` |
|         - | 2483 | `	char *zCopy;` |
|         - | 2484 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       629 | 2485 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       629 | 2486 | `	if( pAttr == 0 ){` |
|       ! 0 | 2487 | `		return 0;` |
|         - | 2488 | `	}` |
|       629 | 2489 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       629 | 2490 | `	zCopy = (char *)&pAttr[1];` |
|       629 | 2491 | `	if( nName > 0 ){` |
|       617 | 2492 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       306 | 2493 | `	}` |
|       629 | 2494 | `	zCopy[nName] = 0;` |
|       629 | 2495 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       629 | 2496 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       629 | 2497 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       629 | 2498 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 2499 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 2500 | `	 * value, never a union. */` |
|       629 | 2501 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       629 | 2502 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 2503 | `		goto fail_attr;` |
|         - | 2504 | `	}` |
|       629 | 2505 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       629 | 2506 | `	if( pMemObj == 0 ){` |
|       ! 0 | 2507 | `		goto fail_vmattr;` |
|         - | 2508 | `	}` |
|       629 | 2509 | `	pVmAttr->pAttr = pAttr;` |
|       629 | 2510 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       629 | 2511 | `	pVmAttr->iState = 0;` |
|       629 | 2512 | `	pVmAttr->pOwner = pThis->pClass;` |
|       629 | 2513 | `	pVmAttr->pInst = pThis;` |
|         - | 2514 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 2515 | `	 * property-creation order, matching PHP. */` |
|       629 | 2516 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 2517 | `		goto fail_slot;` |
|         - | 2518 | `	}` |
|         - | 2519 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       629 | 2520 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 2521 | ``	/* php walks the LIVE property table: a `foreach`/`array_walk` that has run off`` |
|         - | 2522 | `	 * the end re-arms onto a property the body just created. */` |
|       941 | 2523 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|       624 | 2524 | `		SyHashGet(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName)));` |
|       629 | 2525 | `	if( ppAttr ){` |
|       272 | 2526 | `		*ppAttr = pVmAttr;` |
|       134 | 2527 | `	}` |
|       629 | 2528 | `	return pMemObj;` |
|       ! 0 | 2529 | `fail_slot:` |
|       ! 0 | 2530 | `	VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 | 2531 | `fail_vmattr:` |
|       ! 0 | 2532 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2533 | `fail_attr:` |
|       ! 0 | 2534 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 2535 | `	return 0;` |
|       317 | 2536 | `}` |
|         - | 2537 | `/*` |
|         - | 2538 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 2539 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 2540 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 2541 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 2542 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 2543 | ` */` |
|      5848 | 2544 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         3 | 2545 | `{` |
|         - | 2546 | `	VmClassAttr *pVmAttr;` |
|         - | 2547 | `	ph7_value *pMemObj;` |
|         - | 2548 | `	/* php's storage name: a base's private goes back into the slot it came out` |
|         - | 2549 | `	 * of, beside (not over) a same-named property of the object's own class. */` |
|      5851 | 2550 | `	const SyString *pKey = PH7_ClassAttrStorageName(&(*pVm),pThis->pClass,pAttr);` |
|      5851 | 2551 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|      5851 | 2552 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 2553 | `		return;` |
|         - | 2554 | `	}` |
|      5851 | 2555 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|      5851 | 2556 | `	if( pMemObj == 0 ){` |
|       ! 0 | 2557 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2558 | `		return;` |
|         - | 2559 | `	}` |
|      5851 | 2560 | `	pVmAttr->pAttr = pAttr;` |
|      5851 | 2561 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|      5851 | 2562 | `	pVmAttr->iState = 0;` |
|      5851 | 2563 | `	pVmAttr->pOwner = pThis->pClass;` |
|      5851 | 2564 | `	pVmAttr->pInst = pThis;` |
|         - | 2565 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 2566 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 2567 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 2568 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 2569 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|      5851 | 2570 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      1107 | 2571 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|       553 | 2572 | `	}` |
|         - | 2573 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 2574 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 2575 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 2576 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|      5851 | 2577 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 2578 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 | 2579 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2580 | `		return;` |
|         - | 2581 | `	}` |
|      5851 | 2582 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|      5851 | 2583 | `	if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 2584 | `		SyHashDeleteEntry(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey),0);` |
|       ! 0 | 2585 | `		VmMemPoolFreeSlot(&pVm->aMemObj,pMemObj->nIdx);` |
|       ! 0 | 2586 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2587 | `		return;` |
|         - | 2588 | `	}` |
|         - | 2589 | `	/* Re-armed only once the entry is here to stay: the rollback above deletes it. */` |
|      8775 | 2590 | `	PH7_ClassInstanceAttrAppended(pThis,` |
|      5848 | 2591 | `		SyHashGet(&pThis->hAttr,SyStringData(pKey),SyStringLength(pKey)));` |
|      5851 | 2592 | `	if( ppAttr ){` |
|      5851 | 2593 | `		*ppAttr = pVmAttr;` |
|      2924 | 2594 | `	}` |
|      2927 | 2595 | `}` |
|         - | 2596 | `/* Forward declaration */` |
|         - | 2597 | `/*` |
|         - | 2598 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 2599 | ` */` |
|         - | 2600 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 2601 | `/*` |
|         - | 2602 | ` * Reserve a constant memory object.` |
|         - | 2603 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2604 | ` */` |
|   1655769 | 2605 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2606 | `{` |
|         - | 2607 | `	ph7_value *pObj;` |
|         - | 2608 | `	sxi32 rc;` |
|   1655774 | 2609 | `	if( pIndex ){` |
|         - | 2610 | `		/* Object index in the object table */` |
|   1635611 | 2611 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    816311 | 2612 | `	}` |
|         - | 2613 | `	/* Reserve a slot for the new object */` |
|   1655774 | 2614 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   1655774 | 2615 | `	if( rc != SXRET_OK ){` |
|         - | 2616 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2617 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2618 | `		 */` |
|       ! 0 | 2619 | `		return 0;` |
|         - | 2620 | `	}` |
|   1655774 | 2621 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   1655774 | 2622 | `	return pObj;` |
|    826384 | 2623 | `}` |
|         - | 2624 | `/*` |
|         - | 2625 | ` * The segmented memory-object pool (PERF.md P1). See VmMemPool in ph7int.h.` |
|         - | 2626 | ` * A slot's ADDRESS never moves once it exists, which is the whole point: the` |
|         - | 2627 | ` * engine's standing "a pointer into aMemObj dangles across a reserve" hazard` |
|         - | 2628 | ` * (written down at pointers-die-across-a-user-callback) exists only because the` |
|         - | 2629 | ` * old table reallocated. Growth here appends a VM_MEMPOOL_SEG_SLOTS segment --` |
|         - | 2630 | ` * one allocation, no copy -- and a fully-free trailing segment is returned on` |
|         - | 2631 | ` * truncate, so a reused VM (-S server, in-process .phpt runner) hands the pool` |
|         - | 2632 | ` * back most of what a large run grew.` |
|         - | 2633 | ` */` |
|      6721 | 2634 | `PH7_PRIVATE sxi32 VmMemPoolInit(VmMemPool *pPool,SyMemBackend *pAllocator)` |
|         5 | 2635 | `{` |
|         - | 2636 | `	ph7_value *pSeg;` |
|      6726 | 2637 | `	SyZero(pPool,sizeof(VmMemPool));` |
|      6726 | 2638 | `	pPool->pAllocator = pAllocator;` |
|         - | 2639 | `	/* The first segment up front, mirroring the SySetAlloc(&pVm->aMemObj,0xFF)` |
|         - | 2640 | `	 * the pool replaced: an aMemObj exists the moment the VM does, and at 256` |
|         - | 2641 | `	 * slots it costs the same 16 KB that opening bid did. This allocation IS the` |
|         - | 2642 | `	 * per-VM floor -- see the segment-size note on VmMemPool in ph7int.h before` |
|         - | 2643 | `	 * raising VM_MEMPOOL_SEG_SHIFT. */` |
|      6726 | 2644 | `	pSeg = (ph7_value *)SyMemBackendAlloc(pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|      6726 | 2645 | `	if( pSeg == 0 ){` |
|       ! 0 | 2646 | `		return SXERR_MEM;` |
|         - | 2647 | `	}` |
|      6726 | 2648 | `	pPool->apSeg = (ph7_value **)SyMemBackendAlloc(pAllocator,sizeof(ph7_value *) * 16);` |
|      6726 | 2649 | `	if( pPool->apSeg == 0 ){` |
|       ! 0 | 2650 | `		SyMemBackendFree(pAllocator,pSeg);` |
|       ! 0 | 2651 | `		return SXERR_MEM;` |
|         - | 2652 | `	}` |
|      6726 | 2653 | `	pPool->apSeg[0] = pSeg;` |
|      6726 | 2654 | `	pPool->nSeg = 1;` |
|      6726 | 2655 | `	pPool->nCap = 16;` |
|      6726 | 2656 | `	pPool->nFreeHead = SXU32_HIGH; /* 0 is a valid slot; the empty-list mark cannot be it */` |
|      6726 | 2657 | `	return SXRET_OK;` |
|      3361 | 2658 | `}` |
|         - | 2659 | `/*` |
|         - | 2660 | ` * Return a freed slot to the pool's intrusive free list. The slot's value must` |
|         - | 2661 | ` * already have been RELEASED by the caller (the sites that push NULL-valued` |
|         - | 2662 | ` * freshly-reserved slots on error have nothing to release): the link is written` |
|         - | 2663 | ` * into the slot's own dead nIdx word, so a slot in the list must be a dead slot.` |
|         - | 2664 | ` * O(1), and zero memory beyond the pool's single head word.` |
|         - | 2665 | ` *` |
|         - | 2666 | ` * Freeing an index that is already on the list is a NO-OP, not a corruption:` |
|         - | 2667 | ` * see MEMOBJ_POOLFREE. That is the one behaviour the old aFreeObj stack had for` |
|         - | 2668 | ` * free and this list does not, so it is bought back explicitly.` |
|         - | 2669 | ` */` |
|  22489819 | 2670 | `PH7_PRIVATE void VmMemPoolFreeSlot(VmMemPool *pPool,sxu32 nIdx)` |
|         5 | 2671 | `{` |
|  22489824 | 2672 | `	ph7_value *pObj = PH7_MemObjAt(pPool,nIdx);` |
|  22489824 | 2673 | `	if( pObj == 0 ){` |
|       ! 0 | 2674 | `		return;   /* stale index -- the caller's own contract, and truncate's */` |
|         - | 2675 | `	}` |
|  22489824 | 2676 | `	if( pObj->iFlags & MEMOBJ_POOLFREE ){` |
|         - | 2677 | `		/* Already on the list. The old aFreeObj stack tolerated a double free by` |
|         - | 2678 | `		 * handing the index out twice and draining; this list would write the head` |
|         - | 2679 | `		 * into the slot the head already names, and every reserve after it would` |
|         - | 2680 | `		 * return that one slot forever. Refusing leaks nothing -- the slot stays` |
|         - | 2681 | `		 * exactly where it already is, on the list. */` |
|       ! 0 | 2682 | `		return;` |
|         - | 2683 | `	}` |
|  22489824 | 2684 | `	pObj->iFlags \|= MEMOBJ_POOLFREE;` |
|  22489824 | 2685 | `	pObj->nIdx = pPool->nFreeHead;` |
|  22489824 | 2686 | `	pPool->nFreeHead = nIdx;` |
|  11242522 | 2687 | `}` |
|         - | 2688 | `/*` |
|         - | 2689 | ` * Reserve a slot at the end of the pool. Returns the raw slot (uninitialized --` |
|         - | 2690 | ` * callers PH7_MemObjInit it, as they did the SySetPeek of the set this replaced)` |
|         - | 2691 | ` * and stores its index. Appending a slot never relocates an existing one.` |
|         - | 2692 | ` */` |
|   3063448 | 2693 | `PH7_PRIVATE ph7_value * VmMemPoolReserve(VmMemPool *pPool,sxu32 *pIndex)` |
|         5 | 2694 | `{` |
|         - | 2695 | `	sxu32 nIdx;` |
|         - | 2696 | `	sxu32 nSeg;` |
|   3063453 | 2697 | `	if( pPool->nUsed >= (pPool->nSeg << VM_MEMPOOL_SEG_SHIFT) ){` |
|         - | 2698 | `		ph7_value *pSeg;` |
|     11321 | 2699 | `		if( pPool->nSeg >= pPool->nCap ){` |
|         - | 2700 | `			/* The segment table itself doubles. It is a few hundred pointers at` |
|         - | 2701 | `			 * the engine's real peaks, so this is cheap and does not touch the` |
|         - | 2702 | `			 * values. */` |
|         - | 2703 | `			ph7_value **apNew;` |
|        41 | 2704 | `			sxu32 nNew = pPool->nCap ? pPool->nCap * 2 : 16;` |
|        41 | 2705 | `			apNew = (ph7_value **)SyMemBackendRealloc(pPool->pAllocator,pPool->apSeg,sizeof(ph7_value *) * nNew);` |
|        41 | 2706 | `			if( apNew == 0 ){` |
|       ! 0 | 2707 | `				return 0;` |
|         - | 2708 | `			}` |
|        41 | 2709 | `			pPool->apSeg = apNew;` |
|        41 | 2710 | `			pPool->nCap = nNew;` |
|        19 | 2711 | `		}` |
|     11321 | 2712 | `		pSeg = (ph7_value *)SyMemBackendAlloc(pPool->pAllocator,sizeof(ph7_value) * VM_MEMPOOL_SEG_SLOTS);` |
|     11321 | 2713 | `		if( pSeg == 0 ){` |
|       ! 0 | 2714 | `			return 0;` |
|         - | 2715 | `		}` |
|     11321 | 2716 | `		pPool->apSeg[pPool->nSeg++] = pSeg;` |
|      5656 | 2717 | `	}` |
|   3063453 | 2718 | `	nIdx = pPool->nUsed;` |
|   3063453 | 2719 | `	pPool->nUsed++;` |
|   3063453 | 2720 | `	nSeg = nIdx >> VM_MEMPOOL_SEG_SHIFT;` |
|   3063453 | 2721 | `	if( pIndex ){` |
|   3063453 | 2722 | `		*pIndex = nIdx;` |
|   1530934 | 2723 | `	}` |
|   3063453 | 2724 | `	return &pPool->apSeg[nSeg][nIdx & VM_MEMPOOL_SEG_MASK];` |
|   1530939 | 2725 | `}` |
|         - | 2726 | `/*` |
|         - | 2727 | ` * Shrink the pool's logical size. Fully-free trailing segments are RETURNED to` |
|         - | 2728 | ` * the allocator; the segment table itself keeps its capacity (a few hundred` |
|         - | 2729 | ` * pointers), so a reset does not realloc the table that describes the pool.` |
|         - | 2730 | ` */` |
|        16 | 2731 | `PH7_PRIVATE sxi32 VmMemPoolTruncate(VmMemPool *pPool,sxu32 nNewSize)` |
|       ! 0 | 2732 | `{` |
|         - | 2733 | `	sxu32 nSegNeed;` |
|         - | 2734 | `	sxu32 nGuard;` |
|         - | 2735 | `	sxu32 nCur;` |
|         - | 2736 | `	sxu32 n;` |
|         - | 2737 | `	/* Abandon the free list: its chain threads through slots that are about to be` |
|         - | 2738 | `	 * truncated away (and through segments about to be freed). The retained` |
|         - | 2739 | `	 * segments' free slots are simply forgotten -- they become fresh reserves,` |
|         - | 2740 | `	 * exactly as the SySetReset(&pVm->aFreeObj) this replaces emptied the old` |
|         - | 2741 | `	 * stack. Walk it FIRST, while nUsed still resolves every link, to take` |
|         - | 2742 | `	 * MEMOBJ_POOLFREE back off the slots that survive: the bit means "on the` |
|         - | 2743 | `	 * list", and a slot still wearing it after the list is gone would refuse the` |
|         - | 2744 | `	 * next legitimate free of that index. nGuard bounds the walk by the slot` |
|         - | 2745 | `	 * count so a chain corrupted from outside cannot spin here. */` |
|        16 | 2746 | `	nCur = pPool->nFreeHead;` |
|        36 | 2747 | `	for( nGuard = pPool->nUsed ; nGuard > 0 && nCur != SXU32_HIGH ; --nGuard ){` |
|        20 | 2748 | `		ph7_value *pFree = PH7_MemObjAt(pPool,nCur);` |
|        20 | 2749 | `		if( pFree == 0 ){` |
|       ! 0 | 2750 | `			break;` |
|         - | 2751 | `		}` |
|        20 | 2752 | `		pFree->iFlags &= ~MEMOBJ_POOLFREE;` |
|        20 | 2753 | `		nCur = pFree->nIdx;` |
|        10 | 2754 | `	}` |
|        16 | 2755 | `	pPool->nFreeHead = SXU32_HIGH;` |
|        16 | 2756 | `	if( nNewSize < pPool->nUsed ){` |
|        16 | 2757 | `		pPool->nUsed = nNewSize;` |
|         8 | 2758 | `	}` |
|         - | 2759 | `	/* Return every segment past the one that still holds a slot. nSeg is kept` |
|         - | 2760 | `	 * rounded UP to cover nUsed, so a slot index already handed out never stops` |
|         - | 2761 | `	 * resolving. */` |
|        16 | 2762 | `	nSegNeed = (pPool->nUsed + VM_MEMPOOL_SEG_SLOTS - 1) >> VM_MEMPOOL_SEG_SHIFT;` |
|        16 | 2763 | `	if( nSegNeed < pPool->nSeg ){` |
|        32 | 2764 | `		for( n = nSegNeed ; n < pPool->nSeg ; ++n ){` |
|        16 | 2765 | `			SyMemBackendFree(pPool->pAllocator,pPool->apSeg[n]);` |
|        16 | 2766 | `			pPool->apSeg[n] = 0;` |
|         8 | 2767 | `		}` |
|        16 | 2768 | `		pPool->nSeg = nSegNeed;` |
|         8 | 2769 | `	}` |
|        16 | 2770 | `	return SXRET_OK;` |
|       ! 0 | 2771 | `}` |
|         - | 2772 | `/*` |
|         - | 2773 | ` * Reserve a memory object.` |
|         - | 2774 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2775 | ` */` |
|   3063448 | 2776 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2777 | `{` |
|         - | 2778 | `	ph7_value *pObj;` |
|   3063453 | 2779 | `	pObj = VmMemPoolReserve(&pVm->aMemObj,pIndex);` |
|   3063453 | 2780 | `	if( pObj == 0 ){` |
|       ! 0 | 2781 | `		return 0;` |
|         - | 2782 | `	}` |
|         - | 2783 | `	/* The slot this replaced came from a SySetPut of a zeroed filler, so a slot` |
|         - | 2784 | `	 * the caller leaves untouched (a static without an initializer, say) reads as` |
|         - | 2785 | `	 * a null value. Keep that: zero the fresh slot. */` |
|   3063453 | 2786 | `	SyZero(pObj,sizeof(ph7_value));` |
|   3063453 | 2787 | `	return pObj;` |
|   1530939 | 2788 | `}` |
|         - | 2789 | `/* Forward declaration */` |
|         - | 2790 | `/* Forward declarations for Fiber C functions */` |
|         - | 2791 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 2792 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 2793 | `/*` |
|         - | 2794 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 2795 | ` * directly as foreign functions.` |
|         - | 2796 | ` */` |
|         - | 2797 |  |
|         - | 2798 | `/*` |
|         - | 2799 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 2800 | ` * start compiling the target PHP program.` |
|         - | 2801 | ` */` |
|      6721 | 2802 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 2803 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 2804 | `	 ph7 *pEngine /* Master engine */` |
|         - | 2805 | `	 )` |
|         5 | 2806 | `{` |
|         - | 2807 | `	ph7_value *pObj;` |
|         - | 2808 | `	sxi32 rc;` |
|         - | 2809 | `	/* Zero the structure */` |
|      6726 | 2810 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 2811 | `	/* Initialize VM fields */` |
|      6726 | 2812 | `	pVm->pEngine = &(*pEngine);` |
|      6726 | 2813 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|      6726 | 2814 | `	PH7_GcInit(&(*pVm));` |
|      6726 | 2815 | `	SySetInit(&pVm->aDeadClosure,&pVm->sAllocator,sizeof(ph7_vm_func *));` |
|         - | 2816 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 2817 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 2818 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      6726 | 2819 | `	pVm->bDisplayErrors = 0;` |
|      6726 | 2820 | `	pVm->bLogErrors = 1;` |
|         - | 2821 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|         - | 2822 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|      6726 | 2823 | `	pVm->iMbSubstitute = '?';` |
|      6726 | 2824 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 2825 | `	/* Instructions containers */` |
|      6726 | 2826 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      6726 | 2827 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      6726 | 2828 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 2829 | `	/* Object containers */` |
|      6726 | 2830 | `	rc = VmMemPoolInit(&pVm->aMemObj,&pVm->sAllocator);` |
|      6726 | 2831 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2832 | `		return rc;` |
|         - | 2833 | `	}` |
|         - | 2834 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      6726 | 2835 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      6726 | 2836 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      6726 | 2837 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      6726 | 2838 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 2839 | `	/* Virtual machine internal containers */` |
|      6726 | 2840 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      6726 | 2841 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      6726 | 2842 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      6726 | 2843 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|         - | 2844 | `	/* The http:// wrapper's last response headers (see PH7_HttpPublishHeaders). */` |
|      6726 | 2845 | `	SyBlobInit(&pVm->sHttpRespHdrs,&pVm->sAllocator);` |
|      6726 | 2846 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      6726 | 2847 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 2848 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 2849 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 2850 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 2851 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 2852 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 2853 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 2854 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 2855 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      6726 | 2856 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      6726 | 2857 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|         - | 2858 | `	/* 0 means "this call site has never been screened", so the first generation is 1. */` |
|      6726 | 2859 | `	pVm->nCallableGen = 1;` |
|      6726 | 2860 | `	pVm->nConstGen = 1; /* likewise for a PH7_OP_LOADC site (PH7_VmConstSiteAnswer) */` |
|      6726 | 2861 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      6726 | 2862 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      6726 | 2863 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      6726 | 2864 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|      6726 | 2865 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      6726 | 2866 | `	SySetInit(&pVm->aCallSite,&pVm->sAllocator,sizeof(VmCallSite));` |
|      6726 | 2867 | `	SyHashInit(&pVm->hCallName,&pVm->sAllocator,0,0);` |
|      6726 | 2868 | `	pVm->nFreeCallSite = 0;` |
|      6726 | 2869 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      6726 | 2870 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      6726 | 2871 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      6726 | 2872 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      6726 | 2873 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|      6726 | 2874 | `	pVm->bIniSeeded = 0;` |
|      6726 | 2875 | `	pVm->pGettext = 0;   /* ext/gettext binds its first domain lazily */` |
|      6726 | 2876 | `	pVm->pPcntl = 0;     /* ext/pcntl allocates its handler table on the first call */` |
|      6726 | 2877 | `	pVm->pSyslog = 0;    /* openlog() allocates the prefix it has to keep alive */` |
|      6726 | 2878 | `	pVm->iPosixErr = 0;  /* ext/posix has seen no failure yet */` |
|      6726 | 2879 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      6726 | 2880 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|      6726 | 2881 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|      6726 | 2882 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      6726 | 2883 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      6726 | 2884 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      6726 | 2885 | `	SyBlobInit(&pVm->sOutStartFile,&pVm->sAllocator);` |
|      6726 | 2886 | `	SyBlobInit(&pVm->sSessStartFile,&pVm->sAllocator);` |
|      6726 | 2887 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      6726 | 2888 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      6726 | 2889 | `	SyBlobInit(&pVm->sAutoloadExt,&pVm->sAllocator);` |
|      6726 | 2890 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|      6726 | 2891 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      6726 | 2892 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      6726 | 2893 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      6726 | 2894 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      6726 | 2895 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      6726 | 2896 | `	pVm->nResourceIdNext = 1;` |
|      6726 | 2897 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      6726 | 2898 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      6726 | 2899 | `	pVm->pMagicSetThis = 0;` |
|      6726 | 2900 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      6726 | 2901 | `	pVm->pHookSetThis = 0;` |
|      6726 | 2902 | `	pVm->pHookSetAttr = 0;` |
|      6726 | 2903 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      6726 | 2904 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      6726 | 2905 | `	pVm->pMagicCallThis = 0;` |
|      6726 | 2906 | `	pVm->pMagicCallClass = 0;` |
|      6726 | 2907 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      6726 | 2908 | `	pVm->pIdleCallFrames = 0;` |
|      6726 | 2909 | `	SyZero(pVm->apIdleOperandStack,sizeof(pVm->apIdleOperandStack));` |
|      6726 | 2910 | `	pVm->nIdleOperandStacks = 0;` |
|      6726 | 2911 | `	pVm->nIdleOperandSlots = 0;` |
|      6726 | 2912 | `	pVm->pIdleStackNodes = 0;` |
|      6726 | 2913 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      6726 | 2914 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      6726 | 2915 | `	pVm->pPendingException = 0;` |
|      6726 | 2916 | `	pVm->pInflightException = 0;` |
|      6726 | 2917 | `	pVm->nInflightExcBase = 0;` |
|      6726 | 2918 | `	VmClearResumeTarget(&(*pVm));` |
|      6726 | 2919 | `	pVm->nBoundaryRc = 0;` |
|      6726 | 2920 | `	PH7_CmpRefusalClear(&(*pVm));` |
|      6726 | 2921 | `	pVm->pConstEvalClass = 0;` |
|      6726 | 2922 | `	pVm->nConstEvalDepth = 0;` |
|      6726 | 2923 | `	pVm->pConstCycleAttr = 0;` |
|      6726 | 2924 | `	pVm->pConstCycleClass = 0;` |
|      6726 | 2925 | `	SySetReset(&pVm->aMagicGuard);` |
|      6726 | 2926 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2927 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2928 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2929 | `	}` |
|      6726 | 2930 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      6726 | 2931 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2932 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2933 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2934 | `	}` |
|      6726 | 2935 | `	pVm->pHookSetAttr = 0;` |
|      6726 | 2936 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      6726 | 2937 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2938 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2939 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2940 | `	}` |
|      6726 | 2941 | `	pVm->pMagicCallClass = 0;` |
|      6726 | 2942 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 2943 | `	/* Configuration containers */` |
|      6726 | 2944 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      6726 | 2945 | `	SySetInit(&pVm->aIncFrame,&pVm->sAllocator,sizeof(VmIncFrame));` |
|      6726 | 2946 | `	SyBlobInit(&pVm->sReflectConstName,&pVm->sAllocator);` |
|      6726 | 2947 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      6726 | 2948 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      6726 | 2949 | `	SySetInit(&pVm->aEvalFile,&pVm->sAllocator,sizeof(SyString));` |
|      6726 | 2950 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      6726 | 2951 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|         - | 2952 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|         - | 2953 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|      6726 | 2954 | `	pVm->iResponseStatus = 0;` |
|      6726 | 2955 | `	pVm->bHeadersSent = 0;` |
|      6726 | 2956 | `	SyBlobReset(&pVm->sOutStartFile);` |
|      6726 | 2957 | `	pVm->nOutStartLine = 0;` |
|      6726 | 2958 | `	SyBlobReset(&pVm->sSessStartFile);` |
|      6726 | 2959 | `	pVm->nSessStartLine = 0;` |
|      6726 | 2960 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|      6726 | 2961 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 2962 | `	/* Error callbacks containers */` |
|      6726 | 2963 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|      6726 | 2964 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|      6726 | 2965 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|      6726 | 2966 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      6726 | 2967 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      6726 | 2968 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 2969 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 2970 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 2971 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 2972 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 2973 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 2974 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 2975 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      6726 | 2976 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      6726 | 2977 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 2978 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 2979 | `#else` |
|         - | 2980 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 2981 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 2982 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 2983 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 2984 | `	pVm->nMaxDepth = 512;` |
|         - | 2985 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 2986 | `#endif` |
|         - | 2987 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 2988 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 2989 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      6726 | 2990 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 2991 | `	/* JSON return status */` |
|      6726 | 2992 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2993 | `	/* PRNG context */` |
|      6726 | 2994 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 2995 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 2996 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      6726 | 2997 | `	pVm->mtSeeded = FALSE;` |
|         - | 2998 | `	/* Install the null constant */` |
|      6726 | 2999 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      6726 | 3000 | `	if( pObj == 0 ){` |
|       ! 0 | 3001 | `		rc = SXERR_MEM;` |
|       ! 0 | 3002 | `		goto Err;` |
|         - | 3003 | `	}` |
|      6726 | 3004 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 3005 | `	/* Install the boolean TRUE constant */` |
|      6726 | 3006 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      6726 | 3007 | `	if( pObj == 0 ){` |
|       ! 0 | 3008 | `		rc = SXERR_MEM;` |
|       ! 0 | 3009 | `		goto Err;` |
|         - | 3010 | `	}` |
|      6726 | 3011 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 3012 | `	/* Install the boolean FALSE constant */` |
|      6726 | 3013 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      6726 | 3014 | `	if( pObj == 0 ){` |
|       ! 0 | 3015 | `		rc = SXERR_MEM;` |
|       ! 0 | 3016 | `		goto Err;` |
|         - | 3017 | `	}` |
|      6726 | 3018 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 3019 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 3020 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 3021 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      6726 | 3022 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      6726 | 3023 | `	if( pObj == 0 ){` |
|       ! 0 | 3024 | `		rc = SXERR_MEM;` |
|       ! 0 | 3025 | `		goto Err;` |
|         - | 3026 | `	}` |
|      6726 | 3027 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 3028 | `	/* Allocate the reference table. It belongs to VM INIT rather than to` |
|         - | 3029 | `	 * PH7_VmMakeReady because the COMPILER now runs bytecode of its own: the` |
|         - | 3030 | `	 * constant-expression evaluation behind a declaration message` |
|         - | 3031 | `	 * (PH7_VmEvalConstExpr) builds the array a parameter defaults to, and every` |
|         - | 3032 | `	 * hashmap insert installs a reference-table entry. (While the table was a` |
|         - | 3033 | `` 	 * HASH, an unallocated one made that lookup index `apRefObj[hash & (0 - 1)]` `` |
|         - | 3034 | `	 * and segfaulted the compiler; the slot-indexed table answers "no record"` |
|         - | 3035 | `	 * for an out-of-range index instead, so this is now a head start rather than` |
|         - | 3036 | `	 * the thing standing between the compiler and a crash.) */` |
|      6726 | 3037 | `	pVm->nRefSize = 0x10;` |
|      6726 | 3038 | `	pVm->apRefObj = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * pVm->nRefSize);` |
|      6726 | 3039 | `	if( pVm->apRefObj == 0 ){` |
|       ! 0 | 3040 | `		rc = SXERR_MEM;` |
|       ! 0 | 3041 | `		goto Err;` |
|         - | 3042 | `	}` |
|         - | 3043 | `	/* Zero the reference table */` |
|      6726 | 3044 | `	SyZero(pVm->apRefObj,sizeof(void *) * pVm->nRefSize);` |
|         - | 3045 | `	/* Create the global frame */` |
|      6726 | 3046 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      6726 | 3047 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3048 | `		goto Err;` |
|         - | 3049 | `	}` |
|         - | 3050 | `	/* Initialize the code generator */` |
|      6726 | 3051 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      6726 | 3052 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3053 | `		goto Err;` |
|         - | 3054 | `	}` |
|         - | 3055 | `	/* VM correctly initialized,set the magic number */` |
|      6726 | 3056 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 3057 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 3058 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      6726 | 3059 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 3060 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      6726 | 3061 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 3062 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 3063 | `	 * compiled — its classes are internal too. */` |
|         - | 3064 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      6726 | 3065 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      6726 | 3066 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      6726 | 3067 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      6726 | 3068 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      6726 | 3069 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 3070 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      6726 | 3071 | `	pVm->pCoalesceObj = 0;` |
|      6726 | 3072 | `	pVm->bCoalesceArmed = 0;` |
|      6726 | 3073 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 3074 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 3075 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 3076 | ``	 * segfaults the first `new Fiber`. */`` |
|      6726 | 3077 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      6726 | 3078 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 3079 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - | 3080 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - | 3081 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - | 3082 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - | 3083 | `	 * rather than being stamped on afterwards. */` |
|      6726 | 3084 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      6726 | 3085 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      6726 | 3086 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      6726 | 3087 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 3088 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      6726 | 3089 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 3090 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      6726 | 3091 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - | 3092 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - | 3093 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 3094 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 3095 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 3096 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      6726 | 3097 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      6726 | 3098 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 3099 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - | 3100 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - | 3101 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      6726 | 3102 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - | 3103 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 3104 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 3105 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      6726 | 3106 | `	PH7_VmInstallReflection(&(*pVm));` |
|      6726 | 3107 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      6726 | 3108 | `	PH7_VmInstallSpl(&(*pVm));` |
|         - | 3109 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      6726 | 3110 | `	PH7_VmInstallHashContext(&(*pVm));` |
|         - | 3111 | `#endif` |
|         - | 3112 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 3113 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|         - | 3114 | `	 * builds its filters out of. */` |
|      6726 | 3115 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|         - | 3116 | `#endif` |
|      6726 | 3117 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|         - | 3118 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 3119 | `	/* php 8.4's RoundingMode, round()'s declared third argument. It rides the` |
|         - | 3120 | `	 * builtin guard because round() -- and bcround() -- do: a build with no` |
|         - | 3121 | `	 * consumer for the symbol does not ship the symbol. */` |
|      6726 | 3122 | `	PH7_VmInstallRoundingMode(&(*pVm));` |
|         - | 3123 | `	/* Pcntl\QosClass: php registers this pure enum on every platform, even the` |
|         - | 3124 | `	 * ones whose build has no function that reads it. */` |
|      6726 | 3125 | `	PH7_VmInstallPcntl(&(*pVm));` |
|         - | 3126 | `	/* BcMath\Number: after RoundingMode, whose cases its round() reads. */` |
|      6726 | 3127 | `	PH7_VmInstallBcMath(&(*pVm));` |
|         - | 3128 | `	/* php's ext/random object surface. It rides the builtin guard for the same` |
|         - | 3129 | `	 * reason bcmath does: the tiny build ships no consumer for it. */` |
|      6726 | 3130 | `	PH7_VmInstallRandom(&(*pVm));` |
|         - | 3131 | `	/* php's ext/fileinfo: the finfo class. It rides the builtin guard with the` |
|         - | 3132 | `	 * two above -- the tiny build ships none of its six functions. */` |
|      6726 | 3133 | `	PH7_VmInstallFileinfo(&(*pVm));` |
|         - | 3134 | `	/* ext/phar stands on SPL's directory iterators (a Phar IS one) and on` |
|         - | 3135 | `	 * ext/zlib for a compressed entry, so it mounts after both. */` |
|      6726 | 3136 | `	PH7_VmInstallPhar(&(*pVm));` |
|         - | 3137 | `#endif` |
|      6726 | 3138 | `	PH7_VmInstallSession(&(*pVm));` |
|      6726 | 3139 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 3140 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3141 | `	/* libxml2-backed surfaces: shared plumbing first, then the ext/xml push` |
|         - | 3142 | `	 * parser and the DOM and XMLWriter class libraries that build on it. */` |
|      6726 | 3143 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      6726 | 3144 | `	PH7_VmInstallXml(&(*pVm));` |
|      6726 | 3145 | `	PH7_VmInstallDom(&(*pVm));` |
|      6726 | 3146 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 3147 | `	/* ext/simplexml stands on ext/dom's document shells and hands nodes back to` |
|         - | 3148 | `	 * it (dom_import_simplexml), so it mounts after DOMDocument exists. */` |
|      6726 | 3149 | `	PH7_VmInstallSimpleXml(&(*pVm));` |
|         - | 3150 | `#endif` |
|         - | 3151 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 3152 | ``	/* ext/pdo's class library first: `Pdo\Sqlite` extends PDO, so the driver's`` |
|         - | 3153 | `	 * installer needs the parent already mounted. */` |
|      6726 | 3154 | `	PH7_VmInstallPdo(&(*pVm));` |
|      6726 | 3155 | `	PH7_VmInstallPdoSqlite(&(*pVm));` |
|         - | 3156 | `	/* ext/sqlite3: php's other sqlite surface, independent of both. */` |
|      6726 | 3157 | `	PH7_VmInstallSqlite3(&(*pVm));` |
|         - | 3158 | `#endif` |
|         - | 3159 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3160 | `	/* ext/curl: the libcurl binding. */` |
|      6726 | 3161 | `	PH7_VmInstallCurl(&(*pVm));` |
|         - | 3162 | `#endif` |
|         - | 3163 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - | 3164 | `	/* ext/sockets: the Socket/AddressInfo handle classes and php's BSD socket` |
|         - | 3165 | `	 * API over the descriptors net.c already drives for the stream wrappers. */` |
|         - | 3166 | `	{` |
|         - | 3167 | `		const ph7_builtin_func *aSock;` |
|      6726 | 3168 | `		sxu32 nSock = 0,n;` |
|      6726 | 3169 | `		PH7_VmInstallSockets(&(*pVm));` |
|      6726 | 3170 | `		aSock = PH7_SocketsFuncTable(&nSock);` |
|    255403 | 3171 | `		for( n = 0 ; n < nSock ; ++n ){` |
|    248682 | 3172 | `			ph7_create_function(&(*pVm),aSock[n].zName,aSock[n].xFunc,pVm);` |
|    124177 | 3173 | `		}` |
|         - | 3174 | `	}` |
|         - | 3175 | `#endif` |
|         - | 3176 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 3177 | `	/* ext/openssl: the three opaque handle classes and the extension's own` |
|         - | 3178 | `	 * functions, in two units -- the library-wide/cipher half and the` |
|         - | 3179 | `	 * certificate half. */` |
|         - | 3180 | `	{` |
|         - | 3181 | `		const ph7_builtin_func *aSsl;` |
|      6726 | 3182 | `		sxu32 nSsl = 0,n;` |
|      6726 | 3183 | `		PH7_VmInstallOpenSsl(&(*pVm));` |
|      6726 | 3184 | `		aSsl = PH7_OpenSslFuncTable(&nSsl);` |
|    235240 | 3185 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|    228519 | 3186 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|    114109 | 3187 | `		}` |
|      6726 | 3188 | `		aSsl = PH7_OpenSslX509FuncTable(&nSsl);` |
|    188193 | 3189 | `		for( n = 0 ; n < nSsl ; ++n ){` |
|    181472 | 3190 | `			ph7_create_function(&(*pVm),aSsl[n].zName,aSsl[n].xFunc,pVm);` |
|     90617 | 3191 | `		}` |
|         - | 3192 | `	}` |
|         - | 3193 | `#endif` |
|         - | 3194 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 3195 | `	/* ext/zlib: the two context classes and the extension's own functions.` |
|         - | 3196 | `	 * Its gz* handle verbs are aliases registered beside the stream functions` |
|         - | 3197 | `	 * they are (vfs.c), and its device beside the other wrappers. */` |
|         - | 3198 | `	{` |
|         - | 3199 | `		const ph7_builtin_func *aZlib;` |
|      6726 | 3200 | `		sxu32 nZlib = 0,n;` |
|      6726 | 3201 | `		PH7_VmInstallZlib(&(*pVm));` |
|      6726 | 3202 | `		aZlib = PH7_ZlibFuncTable(&nZlib);` |
|    134425 | 3203 | `		for( n = 0 ; n < nZlib ; ++n ){` |
|    127704 | 3204 | `			ph7_create_function(&(*pVm),aZlib[n].zName,aZlib[n].xFunc,pVm);` |
|     63769 | 3205 | `		}` |
|         - | 3206 | `	}` |
|         - | 3207 | `	/* ext/zip: the ZipArchive class and the ten deprecated procedural verbs.` |
|         - | 3208 | `	 * It stands ON ext/zlib -- a deflated member is the format's normal case,` |
|         - | 3209 | `	 * and php's own build requires the library for the same reason -- so it` |
|         - | 3210 | `	 * mounts inside that guard and after it. */` |
|         - | 3211 | `	{` |
|         - | 3212 | `		const ph7_builtin_func *aZip;` |
|      6726 | 3213 | `		sxu32 nZip = 0,n;` |
|      6726 | 3214 | `		PH7_VmInstallZip(&(*pVm));` |
|      6726 | 3215 | `		aZip = PH7_ZipFuncTable(&nZip);` |
|     73936 | 3216 | `		for( n = 0 ; n < nZip ; ++n ){` |
|     67215 | 3217 | `			ph7_create_function(&(*pVm),aZip[n].zName,aZip[n].xFunc,pVm);` |
|     33565 | 3218 | `		}` |
|         - | 3219 | `	}` |
|         - | 3220 | `#endif` |
|      6726 | 3221 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 3222 | `	/* Reset the code generator */` |
|      6726 | 3223 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      6726 | 3224 | `	return SXRET_OK;` |
|       ! 0 | 3225 | `Err:` |
|       ! 0 | 3226 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 3227 | `	return rc;` |
|      3361 | 3228 | `}` |
|         - | 3229 | `/*` |
|         - | 3230 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 3231 | ` * routine which store the output in an internal blob.` |
|         - | 3232 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 3233 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 3234 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 3235 | ` * Refer to the official docurmentation for additional information.` |
|         - | 3236 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 3237 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 3238 | ` * to finish executing and extracting the output.` |
|         - | 3239 | ` */` |
|       352 | 3240 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 3241 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 3242 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 3243 | `	void *pUserData     /* User private data */` |
|         - | 3244 | `	)` |
|         4 | 3245 | `{` |
|         - | 3246 | `	 sxi32 rc;` |
|         - | 3247 | `	 /* Store the output in an internal BLOB */` |
|       356 | 3248 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       356 | 3249 | `	 return rc;` |
|         4 | 3250 | `}` |
|         - | 3251 | `/*` |
|         - | 3252 | ` * WHERE the response body began -- the file and the line php names in every` |
|         - | 3253 | ` * headers-already-sent diagnostic and hands back through headers_sent()'s two` |
|         - | 3254 | ` * by-ref out-params. Answers 0 while nothing has been emitted, which is php's` |
|         - | 3255 | ` * "" and 0 rather than a missing answer.` |
|         - | 3256 | ` */` |
|        30 | 3257 | `PH7_PRIVATE int PH7_VmOutputOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|         4 | 3258 | `{` |
|        34 | 3259 | `	if( pFile ){` |
|        34 | 3260 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sOutStartFile),` |
|         - | 3261 | `			SyBlobLength(&pVm->sOutStartFile));` |
|        15 | 3262 | `	}` |
|        34 | 3263 | `	if( pnLine ){` |
|        34 | 3264 | `		*pnLine = pVm->nOutStartLine;` |
|        15 | 3265 | `	}` |
|        34 | 3266 | `	return SyBlobLength(&pVm->sOutStartFile) > 0;` |
|         4 | 3267 | `}` |
|         - | 3268 | `/*` |
|         - | 3269 | ` * WHERE the active session was started, which is the other half php names --` |
|         - | 3270 | ` * a session directive refused because a session is ACTIVE points at the` |
|         - | 3271 | ` * session_start() that opened it.` |
|         - | 3272 | ` */` |
|        12 | 3273 | `PH7_PRIVATE int PH7_VmSessionOrigin(ph7_vm *pVm,SyString *pFile,sxu32 *pnLine)` |
|         2 | 3274 | `{` |
|        14 | 3275 | `	if( pFile ){` |
|        14 | 3276 | `		SyStringInitFromBuf(pFile,(const char *)SyBlobData(&pVm->sSessStartFile),` |
|         - | 3277 | `			SyBlobLength(&pVm->sSessStartFile));` |
|         6 | 3278 | `	}` |
|        14 | 3279 | `	if( pnLine ){` |
|        14 | 3280 | `		*pnLine = pVm->nSessStartLine;` |
|         6 | 3281 | `	}` |
|        14 | 3282 | `	return SyBlobLength(&pVm->sSessStartFile) > 0;` |
|         2 | 3283 | `}` |
|         - | 3284 | `/*` |
|         - | 3285 | ` * php's provenance clause, appended to a session refusal: a session that is` |
|         - | 3286 | ` * ACTIVE points at the session_start() that opened it, and a response that has` |
|         - | 3287 | ` * already begun points at the output. Appends nothing when the place is not` |
|         - | 3288 | ` * known, which is the message php prints for a session no script started.` |
|         - | 3289 | ` */` |
|        18 | 3290 | `PH7_PRIVATE void PH7_VmAppendWhere(ph7_vm *pVm,SyBlob *pMsg,int bSessionActive)` |
|         3 | 3291 | `{` |
|         - | 3292 | `	SyString sFile;` |
|        21 | 3293 | `	sxu32 nLine = 0;` |
|         - | 3294 | `	char zTail[64];` |
|        18 | 3295 | `	int bHave = bSessionActive ? PH7_VmSessionOrigin(pVm,&sFile,&nLine)` |
|        12 | 3296 | `	                           : PH7_VmOutputOrigin(pVm,&sFile,&nLine);` |
|        21 | 3297 | `	if( !bHave ){` |
|       ! 0 | 3298 | `		return;` |
|         - | 3299 | `	}` |
|         - | 3300 | `	{` |
|        21 | 3301 | `		const char *zOpen = bSessionActive ? " (started from " : " (sent from ";` |
|        21 | 3302 | `		SyBlobAppend(pMsg,zOpen,(sxu32)SyStrlen(zOpen));` |
|         - | 3303 | `	}` |
|        21 | 3304 | `	SyBlobAppend(pMsg,sFile.zString,sFile.nByte);` |
|        21 | 3305 | `	SyBufferFormat(zTail,sizeof(zTail)," on line %u)",nLine);` |
|        21 | 3306 | `	SyBlobAppend(pMsg,zTail,(sxu32)SyStrlen(zTail));` |
|        12 | 3307 | `}` |
|         - | 3308 | `/*` |
|         - | 3309 | ` * Record where the session now going ACTIVE was started.` |
|         - | 3310 | ` */` |
|        96 | 3311 | `PH7_PRIVATE void PH7_VmSetSessionOrigin(ph7_vm *pVm)` |
|         4 | 3312 | `{` |
|       100 | 3313 | `	SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|       100 | 3314 | `	SyBlobReset(&pVm->sSessStartFile);` |
|       100 | 3315 | `	if( pFile && pFile->nByte > 0 ){` |
|       100 | 3316 | `		SyBlobAppend(&pVm->sSessStartFile,pFile->zString,pFile->nByte);` |
|        48 | 3317 | `	}` |
|       100 | 3318 | `	pVm->nSessStartLine = pVm->nCurLine;` |
|       100 | 3319 | `}` |
|         - | 3320 | `/*` |
|         - | 3321 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 3322 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 3323 | ` */` |
|    281877 | 3324 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 3325 | `{` |
|    281882 | 3326 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    281882 | 3327 | `	if( xCons != VmObConsumer ){` |
|     78690 | 3328 | `		pVm->nOutputLen += nLen;` |
|     78690 | 3329 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|         - | 3330 | `			/* The ORIGIN of the output is recorded with the flag, once: php's` |
|         - | 3331 | `			 * four headers-sent diagnostics all name the place the response` |
|         - | 3332 | `			 * body began, and headers_sent() hands the same pair back through` |
|         - | 3333 | `			 * its two by-ref out-params. */` |
|      2017 | 3334 | `			SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|      2017 | 3335 | `			pVm->bHeadersSent = 1;` |
|      2017 | 3336 | `			SyBlobReset(&pVm->sOutStartFile);` |
|      2017 | 3337 | `			if( pFile && pFile->nByte > 0 ){` |
|      2017 | 3338 | `				SyBlobAppend(&pVm->sOutStartFile,pFile->zString,pFile->nByte);` |
|      1006 | 3339 | `			}` |
|      2017 | 3340 | `			pVm->nOutStartLine = pVm->nCurLine;` |
|      1006 | 3341 | `		}` |
|     38382 | 3342 | `	}` |
|    281882 | 3343 | `}` |
|         - | 3344 | `/*` |
|         - | 3345 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 3346 | ` *` |
|         - | 3347 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 3348 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 3349 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 3350 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 3351 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 3352 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 3353 | ` *` |
|         - | 3354 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 3355 | ` * conservative BY CONSTRUCTION:` |
|         - | 3356 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 3357 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 3358 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 3359 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 3360 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 3361 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 3362 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 3363 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 3364 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 3365 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 3366 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 3367 | ` *     instruction-count bound -> fallback.` |
|         - | 3368 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 3369 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 3370 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 3371 | ` *` |
|         - | 3372 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 3373 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 3374 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 3375 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 3376 | ` *` |
|         - | 3377 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 3378 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 3379 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 3380 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 3381 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 3382 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 3383 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 3384 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 3385 | ` * entry here.` |
|         - | 3386 | ` */` |
|         - | 3387 | `/*` |
|         - | 3388 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 3389 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 3390 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 3391 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 3392 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 3393 | ` */` |
|     80424 | 3394 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 3395 | `{` |
|     80429 | 3396 | `	int push = 0, n = 0;` |
|         - | 3397 | `	sxi32 d;` |
|     80429 | 3398 | `	switch( pI->iOp ){` |
|         - | 3399 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 3400 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|     15272 | 3401 | `	case PH7_OP_LOADC:` |
|         - | 3402 | `	case PH7_OP_DUP:` |
|     30259 | 3403 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      5953 | 3404 | `	case PH7_OP_LOAD:` |
|     11884 | 3405 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|     11884 | 3406 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         9 | 3407 | `	case PH7_OP_LOAD_REF:` |
|        19 | 3408 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 3409 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       694 | 3410 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 3411 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 3412 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1392 | 3413 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 3414 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 3415 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       312 | 3416 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 3417 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       606 | 3418 | `		if( pI->iP2 == 0 ){` |
|       606 | 3419 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       294 | 3420 | `		}else{` |
|       ! 0 | 3421 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 3422 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 3423 | `		}` |
|       606 | 3424 | `		break;` |
|         - | 3425 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 3426 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       205 | 3427 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 3428 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 3429 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 3430 | `	case PH7_OP_NOOP:` |
|       414 | 3431 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 3432 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 3433 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       934 | 3434 | `	case PH7_OP_STORE:` |
|      1871 | 3435 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1871 | 3436 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 3437 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1801 | 3438 | `	case PH7_OP_POP:` |
|         - | 3439 | `	case PH7_OP_CONSUME:` |
|      3585 | 3440 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 3441 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - | 3442 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 | 3443 | `	case PH7_OP_ROT_CALLEE:` |
|       ! 0 | 3444 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 3445 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 3446 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 3447 | `	 * true pop count is a runtime value — never reaches here. */` |
|       601 | 3448 | `	case PH7_OP_CALL:` |
|      1152 | 3449 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 3450 | `	/* Jumps. */` |
|        86 | 3451 | `	case PH7_OP_JMP:` |
|       177 | 3452 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       219 | 3453 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       441 | 3454 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       441 | 3455 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 3456 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      4321 | 3457 | `	case PH7_OP_DONE:` |
|      8604 | 3458 | `		n = 0; break;` |
|     10157 | 3459 | `	default:` |
|     20081 | 3460 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 3461 | `	}` |
|     60353 | 3462 | `	*pPush = push; *pN = n;` |
|     60353 | 3463 | `	return 1;` |
|     39865 | 3464 | `}` |
|         - | 3465 | `/*` |
|         - | 3466 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 3467 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 3468 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 3469 | ` */` |
|     23050 | 3470 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 3471 | `{` |
|         - | 3472 | `	void *pScratch;` |
|         - | 3473 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 3474 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 3475 | `	sxi32 iMax;` |
|         - | 3476 | `	int push, n, k;` |
|         - | 3477 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     23055 | 3478 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 3479 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 3480 | `		return VM_STACK_UNMODELED;` |
|         - | 3481 | `	}` |
|         - | 3482 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     73982 | 3483 | `	for( i = 0; i < nInstr; i++ ){` |
|     71008 | 3484 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     20081 | 3485 | `			return VM_STACK_UNMODELED;` |
|         - | 3486 | `		}` |
|     25278 | 3487 | `	}` |
|         - | 3488 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 3489 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 3490 | `	 * first (the byte array last needs no alignment). */` |
|      2979 | 3491 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2979 | 3492 | `	if( pScratch == 0 ){` |
|       ! 0 | 3493 | `		return VM_STACK_UNMODELED;` |
|         - | 3494 | `	}` |
|      2979 | 3495 | `	aH  = (sxi32 *)pScratch;` |
|      2979 | 3496 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2979 | 3497 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     15012 | 3498 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2979 | 3499 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2979 | 3500 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|     12400 | 3501 | `	while( nQ > 0 ){` |
|      9426 | 3502 | `		sxu32 pc = aQ[--nQ];` |
|         - | 3503 | `		sxi32 h;` |
|      9426 | 3504 | `		aIn[pc] = 0;` |
|      9426 | 3505 | `		h = aH[pc];` |
|      9426 | 3506 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|      9426 | 3507 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|      9426 | 3508 | `		if( h + push > iMax ){ iMax = h + push; }` |
|      9426 | 3509 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     15905 | 3510 | `		for( k = 0; k < n; k++ ){` |
|      6484 | 3511 | `			sxi32 hn = h + delta[k];` |
|      6484 | 3512 | `			sxu32 t = succ[k];` |
|      6484 | 3513 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      6484 | 3514 | `			if( hn > aH[t] ){` |
|      6452 | 3515 | `				aH[t] = hn;` |
|      6452 | 3516 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      3188 | 3517 | `			}` |
|      3209 | 3518 | `		}` |
|      9426 | 3519 | `		if( iMax < 0 ){ break; }` |
|         5 | 3520 | `	}` |
|      2979 | 3521 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2979 | 3522 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|     11404 | 3523 | `}` |
|         - | 3524 | `/*` |
|         - | 3525 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 3526 | ` * our compiled PHP program.` |
|         - | 3527 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 3528 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 3529 | ` *` |
|         - | 3530 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 3531 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 3532 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 3533 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 3534 | ` */` |
|   4551986 | 3535 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 3536 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3537 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 3538 | `	)` |
|         5 | 3539 | `{` |
|         - | 3540 | `	ph7_value *pStack;` |
|         - | 3541 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 3542 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 3543 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 3544 | `  ** on the maximum stack depth required.` |
|         - | 3545 | `  **` |
|         - | 3546 | `  ** Allocation all the stack space we will ever need.` |
|         - | 3547 | `  */` |
|   4551991 | 3548 | `	nInstr += VM_STACK_GUARD;` |
|   4551991 | 3549 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   4551991 | 3550 | `	if( pStack == 0 ){` |
|       ! 0 | 3551 | `		return 0;` |
|         - | 3552 | `	}` |
|         - | 3553 | `	/* Initialize the operand stack. PH7_MemObjInit per slot is one call, one` |
|         - | 3554 | `	 * sizeof(ph7_value) SyZero and one SyBlobInit each; the whole buffer is` |
|         - | 3555 | `	 * contiguous, so zero it ONCE and fill in only the three fields a null value` |
|         - | 3556 | `	 * needs that are not zero. This is a hot path in its own right -- an OP_CALL` |
|         - | 3557 | `	 * that misses the recycling pool inits the callee's whole stack, and the fill` |
|         - | 3558 | `	 * loop below was 11% of a phpcs run. */` |
|   4551991 | 3559 | `	SyZero(pStack,nInstr * sizeof(ph7_value));` |
|  89164750 | 3560 | `	while( nInstr > 0 ){` |
|  84612764 | 3561 | `		ph7_value *pSlot = &pStack[--nInstr];` |
|  84612764 | 3562 | `		pSlot->pVm = pVm;` |
|  84612764 | 3563 | `		pSlot->sBlob.pAllocator = &pVm->sAllocator;` |
|  84612764 | 3564 | `		pSlot->iFlags = MEMOBJ_NULL;` |
|         5 | 3565 | `	}` |
|         - | 3566 | `	/* Ready for bytecode execution */` |
|   4551991 | 3567 | `	return pStack;` |
|   2275727 | 3568 | `}` |
|         - | 3569 | `/*` |
|         - | 3570 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 3571 | ` *` |
|         - | 3572 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 3573 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 3574 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 3575 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 3576 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 3577 | ` * the per-slot PH7_MemObjInit.` |
|         - | 3578 | ` *` |
|         - | 3579 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 3580 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 3581 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 3582 | ` * cold/suspend/abort paths that never route through here. A buffer is reused` |
|         - | 3583 | ` * only for a request of exactly its size (never over-allocated), and the pool is` |
|         - | 3584 | ` * bounded three ways so it can't grow without end: entry count, per-buffer size,` |
|         - | 3585 | ` * and -- the one that actually bounds the MEMORY -- a total parked-slot budget.` |
|         - | 3586 | ` *` |
|         - | 3587 | ` * Only an EXACT size is reusable, so the size picks the chain: the pool is` |
|         - | 3588 | `` * PH7_STACK_POOL_BUCKETS separate LIFO lists indexed by `nCap & (BUCKETS-1)`, with`` |
|         - | 3589 | ` * the size still checked per node. It used to be ONE list walked end to end. That` |
|         - | 3590 | ` * replaced a head-only match, which was tuned for the design target -- recursion, or` |
|         - | 3591 | ` * a hot loop calling one function: one size, near-total reuse -- and which real code` |
|         - | 3592 | ` * (a dozen differently-sized functions in turn) missed on nearly every call, falling` |
|         - | 3593 | ` * back to a fresh buffer whose every slot had to be initialized: 11% of a phpcs run` |
|         - | 3594 | ` * sat in that init. Walking the whole list fixed the misses and bought its own` |
|         - | 3595 | ` * problem, because the cap that makes the reuse work is 256 buffers and a call` |
|         - | 3596 | ` * compared itself against all of them: 2.2% of the run, nearly all of it against` |
|         - | 3597 | ` * sizes it could never take. Keying by size keeps the hit rate and drops the walk.` |
|         - | 3598 | ` */` |
|         - | 3599 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 3600 | `struct VmIdleStack {` |
|         - | 3601 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 3602 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 3603 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 3604 | `};` |
|         - | 3605 | `#define VM_STACK_POOL_MAX 256      /* max buffers parked at once. A program calls far more` |
|         - | 3606 | `                                    * than 64 distinct-sized functions in its hot loop, and a` |
|         - | 3607 | `                                    * pool that is FULL turns every recycle into a free and` |
|         - | 3608 | `                                    * every call after it into a fresh, freshly-initialized` |
|         - | 3609 | `                                    * buffer: at 64 entries a phpcs run refused 86k of its` |
|         - | 3610 | `                                    * 400k recycles for being full and missed 24% of its` |
|         - | 3611 | `                                    * allocations. The memory this could cost is bounded by` |
|         - | 3612 | `                                    * the slot budget below, not by this count. */` |
|         - | 3613 | `#define VM_STACK_POOL_MAXSLOTS 4096 /* never pool a buffer bigger than this: one outlier` |
|         - | 3614 | `                                    * (an unmodelable body falls back to its whole` |
|         - | 3615 | `                                    * instruction count — 8000+ slots is real) must not sit` |
|         - | 3616 | `                                    * in the pool holding half a megabyte for a size nothing` |
|         - | 3617 | `                                    * asks for again */` |
|         - | 3618 | `#define VM_STACK_POOL_SLOTS 65536  /* total slots parked across the pool: ~4 MB of ph7_values,` |
|         - | 3619 | `                                    * the real bound on what recycling costs. Entry count and` |
|         - | 3620 | `                                    * per-buffer size are shape limits; this is the budget. */` |
|         - | 3621 | `/*` |
|         - | 3622 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 3623 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 3624 | ` */` |
|    807542 | 3625 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 3626 | `{` |
|    807547 | 3627 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|         - | 3628 | `	/* Only this size's own chain can hold a buffer this call can take. */` |
|    807547 | 3629 | `	VmIdleStack **ppIdle =` |
|    807542 | 3630 | `		(VmIdleStack **)&pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|    838466 | 3631 | `	while( *ppIdle ){` |
|    816055 | 3632 | `		VmIdleStack *pIdle = *ppIdle;` |
|    816055 | 3633 | `		if( pIdle->nCap == nCap ){` |
|    785136 | 3634 | `			ph7_value *pStack = pIdle->pStack;` |
|    785136 | 3635 | `			*ppIdle = pIdle->pNext;` |
|    785136 | 3636 | `			pVm->nIdleOperandStacks--;` |
|    785136 | 3637 | `			pVm->nIdleOperandSlots -= nCap;` |
|         - | 3638 | `			/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 3639 | `			 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    785136 | 3640 | `			pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    785136 | 3641 | `			pVm->pIdleStackNodes = pIdle;` |
|    785136 | 3642 | `			return pStack; /* slots already released -> reusable without re-init */` |
|         - | 3643 | `		}` |
|     30924 | 3644 | `		ppIdle = &pIdle->pNext;` |
|         5 | 3645 | `	}` |
|     22416 | 3646 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    403640 | 3647 | `}` |
|         - | 3648 | `/*` |
|         - | 3649 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 3650 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size); a parked` |
|         - | 3651 | ` * buffer must leave here with every slot released, so it can be handed to the next` |
|         - | 3652 | ` * call without re-initialization and can never retain a live value.` |
|         - | 3653 | ` *` |
|         - | 3654 | ` * nLive is how many slots the finishing activation could have touched -- its` |
|         - | 3655 | ` * operand-stack WATERMARK, the deepest its top ever reached (VmByteCodeExecBody` |
|         - | 3656 | ` * keeps it; the callee's record carries it here). Above that the buffer is still` |
|         - | 3657 | ` * exactly as it was handed out: clean. Releasing the whole capacity instead was` |
|         - | 3658 | ` * 1,044,870,548 releases on the phpcs step of record -- 38% of every release the` |
|         - | 3659 | ` * engine makes -- of which 34,354 found a value and 1,044,836,194 found a slot` |
|         - | 3660 | ` * that owned nothing. 11.85 million recycles walking 88 slots each, to free 34` |
|         - | 3661 | ` * thousand values, all of which live in the first handful of slots. The watermark` |
|         - | 3662 | ` * bounds the same sweep at 44.5 million slots (-95.7%) and finds every one of` |
|         - | 3663 | ` * those values; both corpora and the phpcs step agree that NOTHING above it is` |
|         - | 3664 | ` * ever dirty (PERF.md P10 item 1, and §7's fourth instrument is what found it).` |
|         - | 3665 | ` *` |
|         - | 3666 | ` * The bound has to be the watermark and not the final top: a call abandons its` |
|         - | 3667 | `` * argument slots by lowering the top past them (`pTos = &pTos[-nCallArgs]`), so a`` |
|         - | 3668 | ` * body's own stack routinely carries dirt ABOVE where its top ends up -- 28,343` |
|         - | 3669 | ` * of those 11.85 million recycles. The watermark is above both by construction.` |
|         - | 3670 | ` */` |
|    807342 | 3671 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap, sxu32 nLive)` |
|         5 | 3672 | `{` |
|         - | 3673 | `	VmIdleStack *pIdle;` |
|         - | 3674 | `	sxu32 i;` |
|    807347 | 3675 | `	if( pStack == 0 ){` |
|       ! 0 | 3676 | `		return;` |
|         - | 3677 | `	}` |
|    807342 | 3678 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX` |
|    798902 | 3679 | `	 \|\| nCap > VM_STACK_POOL_MAXSLOTS` |
|    790467 | 3680 | `	 \|\| pVm->nIdleOperandSlots + nCap > VM_STACK_POOL_SLOTS ){` |
|     16881 | 3681 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     16881 | 3682 | `		return;` |
|         - | 3683 | `	}` |
|         - | 3684 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 3685 | `	 * pool-allocate only when the spare list is empty. */` |
|    790467 | 3686 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    790467 | 3687 | `	if( pIdle ){` |
|    785136 | 3688 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    392517 | 3689 | `	}else{` |
|      5336 | 3690 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      5336 | 3691 | `		if( pIdle == 0 ){` |
|       ! 0 | 3692 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 3693 | `			return;` |
|         - | 3694 | `		}` |
|         - | 3695 | `	}` |
|    790467 | 3696 | `	if( nLive > nCap ){` |
|       ! 0 | 3697 | `		nLive = nCap;   /* defensive: never walk past the buffer */` |
|       ! 0 | 3698 | `	}` |
|   2687653 | 3699 | `	for( i = 0; i < nLive; i++ ){` |
|   1897191 | 3700 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 3701 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 3702 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 3703 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 3704 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 3705 | `		 * across invocations — cheap defense in depth. The slots ABOVE nLive keep` |
|         - | 3706 | `		 * whatever index they had, which is exactly what a FRESH buffer looks like:` |
|         - | 3707 | `		 * VmNewOperandStack zeroes the array and never writes nIdx, so slot 0's` |
|         - | 3708 | `		 * index is what an untouched slot has always carried. */` |
|   1897191 | 3709 | `		pStack[i].nIdx = SXU32_HIGH;` |
|    945094 | 3710 | `	}` |
|    790467 | 3711 | `	pIdle->pStack = pStack;` |
|    790467 | 3712 | `	pIdle->nCap = nCap;` |
|    790467 | 3713 | `	pIdle->pNext = (VmIdleStack *)pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)];` |
|    790467 | 3714 | `	pVm->apIdleOperandStack[nCap & (PH7_STACK_POOL_BUCKETS - 1)] = pIdle;` |
|    790467 | 3715 | `	pVm->nIdleOperandStacks++;` |
|    790467 | 3716 | `	pVm->nIdleOperandSlots += nCap;` |
|    403540 | 3717 | `}` |
|         - | 3718 | `/* Forward declaration */` |
|         - | 3719 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 3720 | `/*` |
|         - | 3721 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 3722 | ` * This routine gets called by the PH7 engine after` |
|         - | 3723 | ` * successful compilation of the target PHP program.` |
|         - | 3724 | ` */` |
|      5619 | 3725 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 3726 | `	ph7_vm *pVm /* Target VM */` |
|         - | 3727 | `	)` |
|         5 | 3728 | `{` |
|         - | 3729 | `	SyHashEntry *pEntry;` |
|         - | 3730 | `	sxi32 rc;` |
|      5624 | 3731 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 3732 | `		/* Initialize your VM first */` |
|       ! 0 | 3733 | `		return SXERR_CORRUPT;` |
|         - | 3734 | `	}` |
|         - | 3735 | `	/* Mark the VM ready for byte-code execution */` |
|      5624 | 3736 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 3737 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 3738 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 3739 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 3740 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      5624 | 3741 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 3742 | `	/* Emit the DONE instruction */` |
|      5624 | 3743 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      5624 | 3744 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3745 | `		return SXERR_MEM;` |
|         - | 3746 | `	}` |
|         - | 3747 | `	/* Script return value */` |
|      5624 | 3748 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 3749 | `	/* Allocate a new operand stack */` |
|      5624 | 3750 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      5624 | 3751 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 3752 | `		return SXERR_MEM;` |
|         - | 3753 | `	}` |
|         - | 3754 | `	/* Set the default VM output consumer callback and it's` |
|         - | 3755 | `	 * private data. */` |
|      5624 | 3756 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      5624 | 3757 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 3758 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      5624 | 3759 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      5624 | 3760 | `	if( rc != SXRET_OK ){` |
|         - | 3761 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 3762 | `		return rc;` |
|         - | 3763 | `	}` |
|         - | 3764 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 3765 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 3766 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 3767 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 3768 | `	 * below it is compile-time/init state that survives a reset. */` |
|      5624 | 3769 | `	pVm->nSuperBaseline = pVm->aMemObj.nUsed;` |
|         - | 3770 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      5624 | 3771 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      5624 | 3772 | `	if( rc != SXRET_OK ){` |
|         - | 3773 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 3774 | `		return rc;` |
|         - | 3775 | `	}` |
|         - | 3776 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      5624 | 3777 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 3778 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      5624 | 3779 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 3780 | `	/* Register ext/pcntl's signal, priority and namespace constants (none of` |
|         - | 3781 | `	 * which exist on Windows, where php builds no ext/pcntl either) */` |
|      5624 | 3782 | `	PH7_RegisterPcntlConstants(&(*pVm));` |
|         - | 3783 | `	/* Register the LOG_* constants ext/standard's syslog trio reads */` |
|      5624 | 3784 | `	PH7_RegisterSyslogConstants(&(*pVm));` |
|         - | 3785 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - | 3786 | `	/* Register ext/sockets' AF_, SOCK_, SO_ and SOCKET_E family. php builds this` |
|         - | 3787 | `	 * extension on every platform, so unlike pcntl's these are not #ifdef'd` |
|         - | 3788 | `	 * away on Windows -- only the names that platform has no macro for are. */` |
|      5624 | 3789 | `	PH7_RegisterSocketsConstants(&(*pVm));` |
|         - | 3790 | `#endif` |
|         - | 3791 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      5624 | 3792 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 3793 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      5624 | 3794 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 3795 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 3796 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      5624 | 3797 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      5624 | 3798 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 3799 | `#endif` |
|         - | 3800 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3801 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      5624 | 3802 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 3803 | `#endif` |
|         - | 3804 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3805 | `	/* Register the CURLOPT_* / CURLINFO_* / CURLE_* family */` |
|      5624 | 3806 | `	PH7_RegisterCurlConstants(&(*pVm));` |
|         - | 3807 | `#endif` |
|         - | 3808 | `	/* Every extension has registered its own constants by now, so the` |
|         - | 3809 | `	 * deprecation marks can be stamped on the names they cover wherever those` |
|         - | 3810 | `	 * were installed -- a name a build does not carry is simply skipped. */` |
|      5624 | 3811 | `	PH7_MarkDeprecatedConstants(&(*pVm));` |
|         - | 3812 | `	/* Same stamp for the FUNCTIONS and native methods php deprecated; the` |
|         - | 3813 | `	 * classes were installed by PH7_VmInit, well before this runs. */` |
|      5624 | 3814 | `	PH7_MarkDeprecatedFunctions(&(*pVm));` |
|         - | 3815 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 3816 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      5624 | 3817 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 3818 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      5624 | 3819 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 3820 | `	/* Initialize and install static and constants class attributes.` |
|         - | 3821 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 3822 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 3823 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 3824 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 3825 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 3826 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 3827 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 3828 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 3829 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 3830 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 3831 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 3832 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 3833 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 3834 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      5624 | 3835 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   1237943 | 3836 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   1232324 | 3837 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   1232324 | 3838 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 3839 | `			return rc;` |
|         - | 3840 | `		}` |
|         5 | 3841 | `	}` |
|      5624 | 3842 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|   1236621 | 3843 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|   1231008 | 3844 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|   1231008 | 3845 | `		if( rc != SXRET_OK ){` |
|         9 | 3846 | `			return rc;` |
|         - | 3847 | `		}` |
|         5 | 3848 | `	}` |
|         - | 3849 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|         - | 3850 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      5618 | 3851 | `	pVm->nNextObjId = 1;` |
|         - | 3852 | `	/* VM is ready for bytecode execution */` |
|      5618 | 3853 | `	return SXRET_OK;` |
|      2810 | 3854 | `}` |
|         - | 3855 | `/*` |
|         - | 3856 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 3857 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 3858 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 3859 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 3860 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 3861 | ` */` |
|        16 | 3862 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 3863 | `{` |
|         - | 3864 | `	/* VmRefSlotUnlink empties the cell it is given and decrements nRefUsed, so a` |
|         - | 3865 | `	 * sweep of the table leaves it empty and nRefUsed at 0 — no extra clearing` |
|         - | 3866 | `	 * needed. The cell array and nRefSize survive.` |
|         - | 3867 | `	 *` |
|         - | 3868 | `	 * Unlinking one cell deletes the names and array nodes it holds, which` |
|         - | 3869 | `	 * releases values, which can unlink OTHER cells — including ones this sweep` |
|         - | 3870 | `	 * has already passed, and (through a destructor) ones it has not created yet.` |
|         - | 3871 | `	 * So the sweep repeats while it is still making progress rather than trusting` |
|         - | 3872 | `	 * one pass, and stops the moment a pass frees nothing so it cannot spin. */` |
|        24 | 3873 | `	for(;;){` |
|        32 | 3874 | `		sxu32 n, nBefore = pVm->nRefUsed;` |
|        32 | 3875 | `		if( nBefore == 0 ){` |
|        16 | 3876 | `			break;` |
|         - | 3877 | `		}` |
|       912 | 3878 | `		for( n = 0 ; n < pVm->nRefSize ; ++n ){` |
|      1404 | 3879 | `			while( pVm->apRefObj[n] ){` |
|       508 | 3880 | `				void *pWord = pVm->apRefObj[n];` |
|       508 | 3881 | `				PH7_VmSlotUnlink(&(*pVm),n);` |
|       508 | 3882 | `				if( pVm->apRefObj[n] == pWord ){` |
|       ! 0 | 3883 | `					break; /* unlink did not clear it: do not spin on this cell */` |
|         - | 3884 | `				}` |
|       ! 0 | 3885 | `			}` |
|       448 | 3886 | `		}` |
|        16 | 3887 | `		if( pVm->nRefUsed >= nBefore ){` |
|       ! 0 | 3888 | `			break;` |
|         - | 3889 | `		}` |
|       ! 0 | 3890 | `	}` |
|        16 | 3891 | `}` |
|         - | 3892 | `/*` |
|         - | 3893 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 3894 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 3895 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 3896 | ` */` |
|        96 | 3897 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 3898 | `{` |
|        96 | 3899 | `	PH7_MemObjRelease(pObj);` |
|        96 | 3900 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        96 | 3901 | `}` |
|         - | 3902 | `/*` |
|         - | 3903 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|         - | 3904 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|         - | 3905 | ` * whole thing dies with the VM allocator either way.` |
|         - | 3906 | ` */` |
|        32 | 3907 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|       ! 0 | 3908 | `{` |
|        32 | 3909 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|         - | 3910 | `	sxu32 n;` |
|        32 | 3911 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|       ! 0 | 3912 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|       ! 0 | 3913 | `	}` |
|        32 | 3914 | `	SySetReset(pStack);` |
|        32 | 3915 | `}` |
|         - | 3916 | `/*` |
|         - | 3917 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 3918 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 3919 | ` * of statics).` |
|         - | 3920 | ` */` |
|     39157 | 3921 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|         5 | 3922 | `{` |
|     39162 | 3923 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 3924 | `	sxu32 k;` |
|     39170 | 3925 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         9 | 3926 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         5 | 3927 | `	}` |
|     39162 | 3928 | `}` |
|         - | 3929 | `/*` |
|         - | 3930 | ` * Tear down one run-time closure's per-instantiation ph7_vm_func: reset its` |
|         - | 3931 | ` * (template-shared) statics, release its captured-by-value environment, then free` |
|         - | 3932 | ` * the name buffer and the structure. The caller owns unlinking the hFunction row.` |
|         - | 3933 | ` */` |
|     17329 | 3934 | `static void VmFreeRuntimeClosure(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|         5 | 3935 | `{` |
|     17334 | 3936 | `	ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     17334 | 3937 | `	const char *zName = SyStringData(&pFunc->sName);` |
|         - | 3938 | `	sxu32 k;` |
|     17334 | 3939 | `	VmResetFuncStatics(pFunc);` |
|     43669 | 3940 | `	for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|     26340 | 3941 | `		PH7_MemObjRelease(&aEnv[k].sValue);` |
|     13031 | 3942 | `	}` |
|     17334 | 3943 | `	SySetRelease(&pFunc->aClosureEnv);` |
|     17334 | 3944 | `	if( zName ){` |
|     17334 | 3945 | `		SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|      8545 | 3946 | `	}` |
|     17334 | 3947 | `	SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|     17334 | 3948 | `}` |
|         - | 3949 | `/*` |
|         - | 3950 | ` * The run-time closure this name belongs to, or NULL when the name is anything` |
|         - | 3951 | ` * else -- a named user function, a host function, a method. Only a` |
|         - | 3952 | ` * per-instantiation copy (VM_FUNC_CLOSURE, minted by OP_LOAD_CLOSURE) is owned by` |
|         - | 3953 | ` * the Closure objects that name it; everything else in hFunction outlives them.` |
|         - | 3954 | ` */` |
|     37441 | 3955 | `PH7_PRIVATE ph7_vm_func * PH7_VmRuntimeClosure(ph7_vm *pVm,const char *zName,sxu32 nByte)` |
|         5 | 3956 | `{` |
|         - | 3957 | `	SyHashEntry *pEntry;` |
|         - | 3958 | `	ph7_vm_func *pFunc;` |
|     37446 | 3959 | `	if( zName == 0 \|\| nByte < 1 ){` |
|       ! 0 | 3960 | `		return 0;` |
|         - | 3961 | `	}` |
|     37446 | 3962 | `	pEntry = SyHashGet(&pVm->hFunction,(const void *)zName,nByte);` |
|     37446 | 3963 | `	if( pEntry == 0 ){` |
|       649 | 3964 | `		return 0;` |
|         - | 3965 | `	}` |
|     36802 | 3966 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     36802 | 3967 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|       599 | 3968 | `		return 0;` |
|         - | 3969 | `	}` |
|     36208 | 3970 | `	return pFunc;` |
|     18484 | 3971 | `}` |
|    831519 | 3972 | `PH7_PRIVATE void PH7_VmClosureFuncRef(ph7_vm_func *pFunc)` |
|         5 | 3973 | `{` |
|    831524 | 3974 | `	if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|     53501 | 3975 | `		pFunc->nRef++;` |
|     26389 | 3976 | `	}` |
|    831524 | 3977 | `}` |
|         - | 3978 | `/*` |
|         - | 3979 | ` * Give back one hold on a run-time closure. At zero nothing can reach it any more` |
|         - | 3980 | ` * -- no Closure object names it and no activation is running it -- so it leaves` |
|         - | 3981 | ` * the function table and the memory goes back.` |
|         - | 3982 | ` */` |
|    831818 | 3983 | `PH7_PRIVATE void PH7_VmClosureFuncUnref(ph7_vm *pVm,ph7_vm_func *pFunc)` |
|         5 | 3984 | `{` |
|    831823 | 3985 | `	if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|    778740 | 3986 | `		return;` |
|         - | 3987 | `	}` |
|     53088 | 3988 | `	pFunc->nRef--;` |
|     53088 | 3989 | `	if( pFunc->nRef > 0 \|\| pVm->bInReset \|\| pFunc->bQueued ){` |
|     35005 | 3990 | `		return;` |
|         - | 3991 | `	}` |
|         - | 3992 | `	/* Queued, not freed on the spot. The drop that brings a closure to zero is` |
|         - | 3993 | `	 * usually OP_CALL releasing the Closure OBJECT it just unwrapped -- and the` |
|         - | 3994 | `	 * dispatch that did it is about to look the function up BY NAME. Freeing here` |
|         - | 3995 | ``	 * turned `(function(){ yield 1; })()` into "Call to undefined function`` |
|         - | 3996 | `	 * [closure_6]()". The list is drained at the VM's fetch point, where no dispatch` |
|         - | 3997 | `	 * is half-done; anything that took the function up again in between (the call's` |
|         - | 3998 | `	 * own frame does, one instruction later) is simply dropped from the list. */` |
|     18088 | 3999 | `	pFunc->bQueued = 1;` |
|     18088 | 4000 | `	if( SySetPut(&pVm->aDeadClosure,(const void *)&pFunc) != SXRET_OK ){` |
|       ! 0 | 4001 | `		pFunc->bQueued = 0;` |
|       ! 0 | 4002 | `		return; /* no room to remember it: it goes with the wholesale teardown */` |
|         - | 4003 | `	}` |
|     18088 | 4004 | `	pVm->bClosurePurge = 1;` |
|    415658 | 4005 | `}` |
|         - | 4006 | `/*` |
|         - | 4007 | ` * Free the run-time closures nothing needs any more. Called only from the VM's` |
|         - | 4008 | ` * fetch point, between two instructions, where no dispatch is half-resolved.` |
|         - | 4009 | ` *` |
|         - | 4010 | ` * Drained by POPPING: freeing one releases its captured environment, which can` |
|         - | 4011 | ` * release the last Closure object naming ANOTHER one and queue it mid-drain.` |
|         - | 4012 | ` */` |
|     16784 | 4013 | `PH7_PRIVATE void PH7_VmPurgeDeadClosures(ph7_vm *pVm)` |
|         5 | 4014 | `{` |
|     16789 | 4015 | `	pVm->bClosurePurge = 0;` |
|     25757 | 4016 | `	for(;;){` |
|     34433 | 4017 | `		ph7_vm_func **ppFunc = (ph7_vm_func **)SySetPop(&pVm->aDeadClosure);` |
|         - | 4018 | `		ph7_vm_func *pFunc;` |
|         - | 4019 | `		SyHashEntry *pEntry;` |
|     34433 | 4020 | `		if( ppFunc == 0 ){` |
|     16789 | 4021 | `			break;` |
|         - | 4022 | `		}` |
|     17649 | 4023 | `		pFunc = *ppFunc;` |
|     17649 | 4024 | `		pFunc->bQueued = 0;` |
|     17649 | 4025 | `		if( pFunc->nRef > 0 \|\| pVm->bInReset ){` |
|       324 | 4026 | `			continue; /* taken up again between the drop and here */` |
|         - | 4027 | `		}` |
|     25873 | 4028 | `		pEntry = SyHashGet(&pVm->hFunction,(const void *)SyStringData(&pFunc->sName),` |
|      8543 | 4029 | `			SyStringLength(&pFunc->sName));` |
|     17330 | 4030 | `		if( pEntry == 0 \|\| pEntry->pUserData != (void *)pFunc ){` |
|         - | 4031 | `			/* The name is not this copy's any more (an overload chain, a reset in` |
|         - | 4032 | `			 * flight): leave it to the wholesale teardown rather than guess. */` |
|       ! 0 | 4033 | `			continue;` |
|         - | 4034 | `		}` |
|     17330 | 4035 | `		SyHashDeleteEntry2(pEntry);` |
|     17330 | 4036 | `		VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|         5 | 4037 | `	}` |
|     16789 | 4038 | `}` |
|         - | 4039 | `/*` |
|         - | 4040 | `` * A Closure OBJECT taking or giving back its hold on the function `$__fn` names.`` |
|         - | 4041 | `` * One door for all of them: `function(){}` (OP_LOAD_CLOSURE via VmCreateClosure),`` |
|         - | 4042 | `` * `clone`, and `bindTo`/`bind`, which clones.`` |
|         - | 4043 | ` */` |
|     38393 | 4044 | `PH7_PRIVATE void PH7_VmClosureInstanceRef(ph7_vm *pVm,ph7_class_instance *pObj,int iDelta)` |
|         5 | 4045 | `{` |
|         - | 4046 | `	ph7_value *pFn;` |
|         - | 4047 | `	ph7_vm_func *pFunc;` |
|         - | 4048 | `	SyString sAttr;` |
|     38398 | 4049 | `	if( pObj == 0 \|\| pVm->pClosureClass == 0 \|\| pObj->pClass != pVm->pClosureClass ){` |
|      1576 | 4050 | `		return;` |
|         - | 4051 | `	}` |
|     37446 | 4052 | `	SyStringInitFromBuf(&sAttr,"__fn",4);` |
|     37446 | 4053 | `	pFn = PH7_ClassInstanceFetchAttr(pObj,&sAttr);` |
|     37446 | 4054 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 ){` |
|       ! 0 | 4055 | `		return;` |
|         - | 4056 | `	}` |
|     55925 | 4057 | `	pFunc = PH7_VmRuntimeClosure(&(*pVm),(const char *)SyBlobData(&pFn->sBlob),` |
|     18479 | 4058 | `		SyBlobLength(&pFn->sBlob));` |
|     37446 | 4059 | `	if( pFunc == 0 ){` |
|      1243 | 4060 | `		return;` |
|         - | 4061 | `	}` |
|     36208 | 4062 | `	if( iDelta > 0 ){` |
|     18436 | 4063 | `		PH7_VmClosureFuncRef(pFunc);` |
|      9099 | 4064 | `	}else{` |
|     17777 | 4065 | `		PH7_VmClosureFuncUnref(&(*pVm),pFunc);` |
|         - | 4066 | `	}` |
|     18960 | 4067 | `}` |
|         - | 4068 | `/*` |
|         - | 4069 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 4070 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 4071 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 4072 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 4073 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 4074 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 4075 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 4076 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 4077 | ` *    has its static sentinels reset.` |
|         - | 4078 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 4079 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 4080 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 4081 | ` */` |
|        16 | 4082 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 4083 | `{` |
|         - | 4084 | `	SyHashEntry *pEntry;` |
|        16 | 4085 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     21848 | 4086 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     21832 | 4087 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     21832 | 4088 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 4089 | `			/* Whatever run-time closures outlived their objects (one being executed` |
|         - | 4090 | `			 * when its last holder went, one the engine still names) go here. */` |
|         - | 4091 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 4092 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 4093 | `			VmFreeRuntimeClosure(&(*pVm),pFunc);` |
|         4 | 4094 | `			continue;` |
|         - | 4095 | `		}` |
|         - | 4096 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     43656 | 4097 | `		while( pFunc ){` |
|     21828 | 4098 | `			VmResetFuncStatics(pFunc);` |
|     21828 | 4099 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 4100 | `		}` |
|       ! 0 | 4101 | `	}` |
|        16 | 4102 | `	pVm->closure_cnt = 0;` |
|        16 | 4103 | `}` |
|         - | 4104 | `/*` |
|         - | 4105 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 4106 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 4107 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 4108 | ` * the class re-mount registers fresh ones.` |
|         - | 4109 | ` */` |
|        16 | 4110 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 4111 | `{` |
|         - | 4112 | `	SyHashEntry *pEntry;` |
|         - | 4113 | `	/* The bitmap in front of the table goes with it, whether or not the table has` |
|         - | 4114 | `	 * anything left in it: a bit that outlived its entry would send a store into a` |
|         - | 4115 | `	 * lookup that answers nothing, and the class re-mount registers fresh ones. */` |
|        16 | 4116 | `	if( pVm->pFilterBits ){` |
|         4 | 4117 | `		SyZero(pVm->pFilterBits,pVm->nFilterBits >> 3);` |
|         2 | 4118 | `	}` |
|         - | 4119 | `	/* The table is emptied with it, so a bitmap that had been switched off can be` |
|         - | 4120 | `	 * trusted again from here. */` |
|        16 | 4121 | `	pVm->bFilterBitsOff = 0;` |
|         - | 4122 | `	/* Common case: no class static typed properties — table already empty. */` |
|        16 | 4123 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        12 | 4124 | `		return;` |
|         - | 4125 | `	}` |
|         - | 4126 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 4127 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 4128 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 4129 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 4130 | `		if( pEntry->pUserData ){` |
|         4 | 4131 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 4132 | `		}` |
|       ! 0 | 4133 | `	}` |
|         4 | 4134 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 4135 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 4136 | `	pVm->nNativeSetSlot = 0;` |
|         8 | 4137 | `}` |
|         - | 4138 | `/*` |
|         - | 4139 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 4140 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 4141 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 4142 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 4143 | ` * comparing equal — both used to cast to 1.` |
|         - | 4144 | ` *` |
|         - | 4145 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 4146 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 4147 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 4148 | ` * aborting a cast.` |
|         - | 4149 | ` */` |
|       468 | 4150 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         5 | 4151 | `{` |
|         - | 4152 | `	SyHashEntry *pEntry;` |
|         - | 4153 | `	phl_res_id *pRec;` |
|       473 | 4154 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 4155 | `		return 0;` |
|         - | 4156 | `	}` |
|       473 | 4157 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       473 | 4158 | `	if( pEntry ){` |
|       419 | 4159 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 4160 | `	}` |
|        59 | 4161 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        59 | 4162 | `	if( pRec == 0 ){` |
|       ! 0 | 4163 | `		return 0;` |
|         - | 4164 | `	}` |
|        59 | 4165 | `	pRec->pRes = pRes;` |
|        59 | 4166 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        59 | 4167 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 4168 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 4169 | `		return 0;` |
|         - | 4170 | `	}` |
|        59 | 4171 | `	return pRec->nId;` |
|       239 | 4172 | `}` |
|         - | 4173 | `/*` |
|         - | 4174 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 4175 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 4176 | ` */` |
|        16 | 4177 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 4178 | `{` |
|         - | 4179 | `	SyHashEntry *pEntry;` |
|        16 | 4180 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        16 | 4181 | `		pVm->nResourceIdNext = 1;` |
|        16 | 4182 | `		return;` |
|         - | 4183 | `	}` |
|       ! 0 | 4184 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 4185 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 4186 | `		if( pEntry->pUserData ){` |
|       ! 0 | 4187 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 4188 | `		}` |
|       ! 0 | 4189 | `	}` |
|       ! 0 | 4190 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 4191 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 4192 | `	pVm->nResourceIdNext = 1;` |
|         8 | 4193 | `}` |
|         - | 4194 | `/*` |
|         - | 4195 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 4196 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 4197 | ` *` |
|         - | 4198 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 4199 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 4200 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 4201 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 4202 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 4203 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 4204 | ` *` |
|         - | 4205 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 4206 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 4207 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 4208 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 4209 | ` * reserved during the run.` |
|         - | 4210 | ` *` |
|         - | 4211 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 4212 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 4213 | ` * global-scope destructors never fired.` |
|         - | 4214 | ` */` |
|        16 | 4215 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 4216 | `{` |
|         - | 4217 | `	sxu32 nWater,n;` |
|        16 | 4218 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 4219 | `		return SXERR_CORRUPT;` |
|         - | 4220 | `	}` |
|        16 | 4221 | `	nWater = pVm->nSuperBaseline;` |
|         - | 4222 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 4223 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        16 | 4224 | `	pVm->pGlobal = 0;` |
|         - | 4225 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 4226 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 4227 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 4228 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 4229 | `	 * object); unref'ing here would race the teardown below. */` |
|        16 | 4230 | `	pVm->pClosureThis = 0;` |
|        16 | 4231 | `	pVm->pClosureScope = 0;` |
|         - | 4232 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 4233 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 4234 | `	 * here is unsafe. Engine memory is still reclaimed. Mirrors prior behaviour` |
|         - | 4235 | `	 * (global destructors never ran). */` |
|        16 | 4236 | `	pVm->bInReset = 1;` |
|         - | 4237 | `	/* (0) Forget every buffered cycle root. The object pool is about to go, and a` |
|         - | 4238 | `	 * row that outlived it would name freed memory on the next run. */` |
|        16 | 4239 | `	PH7_GcResetBuffer(&(*pVm));` |
|         - | 4240 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        16 | 4241 | `	VmResetRefTable(&(*pVm));` |
|         - | 4242 | `	/* (1b) The pending-free list names functions the wholesale teardown below is` |
|         - | 4243 | `	 * about to free anyway; forget it rather than leave rows pointing at them. */` |
|        16 | 4244 | `	SySetReset(&pVm->aDeadClosure);` |
|        16 | 4245 | `	pVm->bClosurePurge = 0;` |
|         - | 4246 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 4247 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 4248 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 4249 | `	 * re-run define() overwrites the value in place). */` |
|        16 | 4250 | `	VmResetFunctionState(&(*pVm));` |
|         - | 4251 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 4252 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       560 | 4253 | `	for( n = nWater ; n < pVm->aMemObj.nUsed ; ++n ){` |
|       544 | 4254 | `		ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|       544 | 4255 | `		if( pObj ){` |
|       544 | 4256 | `			PH7_MemObjRelease(pObj);` |
|       272 | 4257 | `		}` |
|       272 | 4258 | `	}` |
|         - | 4259 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 4260 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        16 | 4261 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 4262 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 4263 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        16 | 4264 | `	VmResetResourceIds(&(*pVm));` |
|         - | 4265 | `	/* (5) Unwind any active frames back to none. */` |
|        32 | 4266 | `	while( pVm->pFrame ){` |
|        16 | 4267 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 4268 | `	}` |
|         - | 4269 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        16 | 4270 | `	pVm->bInReset = 0;` |
|         - | 4271 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 4272 | `	 * slots (their indices no longer exist). Fully-free trailing segments are` |
|         - | 4273 | `	 * returned by VmMemPoolTruncate. */` |
|        16 | 4274 | `	VmMemPoolTruncate(&pVm->aMemObj,nWater);` |
|         - | 4275 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        16 | 4276 | `	SyHashRelease(&pVm->hSuper);` |
|        16 | 4277 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|        16 | 4278 | `	SyZero(pVm->aSuperFirst,sizeof(pVm->aSuperFirst));` |
|         - | 4279 | `	/* A reused VM (the -S server, the in-process .phpt runner) starts the next run` |
|         - | 4280 | `	 * with the bytecode of the last one still holding its screened-at stamps. */` |
|        16 | 4281 | `	pVm->nCallableGen++;` |
|        16 | 4282 | `	pVm->nConstGen++;` |
|         - | 4283 | `	/* (8) Drain remaining per-exec containers. */` |
|        16 | 4284 | `	SySetReset(&pVm->aSelf);` |
|         - | 4285 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 4286 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 4287 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        16 | 4288 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 4289 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 4290 | `		if( pCB ){` |
|         - | 4291 | `			int iArg;` |
|       ! 0 | 4292 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 4293 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 4294 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 4295 | `			}` |
|       ! 0 | 4296 | `		}` |
|       ! 0 | 4297 | `	}` |
|        16 | 4298 | `	SySetReset(&pVm->aShutdown);` |
|         - | 4299 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 4300 | `	 * aborted program can leave entries behind). */` |
|        16 | 4301 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        16 | 4302 | `	SySetReset(&pVm->aException);` |
|        16 | 4303 | `	SySetReset(&pVm->aFinallyAction);` |
|        16 | 4304 | `	pVm->pPendingException = 0;` |
|        16 | 4305 | `	pVm->pInflightException = 0;` |
|        16 | 4306 | `	pVm->nInflightExcBase = 0;` |
|        16 | 4307 | `	VmClearResumeTarget(&(*pVm));` |
|        16 | 4308 | `	pVm->nBoundaryRc = 0;` |
|        16 | 4309 | `	PH7_CmpRefusalClear(&(*pVm));` |
|        16 | 4310 | `	pVm->pConstEvalClass = 0;` |
|        16 | 4311 | `	pVm->nConstEvalDepth = 0;` |
|        16 | 4312 | `	pVm->pConstCycleAttr = 0;` |
|        16 | 4313 | `	pVm->pConstCycleClass = 0;` |
|        16 | 4314 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 4315 | `	{` |
|         - | 4316 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 4317 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        16 | 4318 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        16 | 4319 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 4320 | `		sxu32 iRmw;` |
|        16 | 4321 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 4322 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 4323 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 4324 | `		}` |
|        16 | 4325 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 4326 | `	}` |
|        16 | 4327 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 4328 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 4329 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 4330 | `	}` |
|        16 | 4331 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        16 | 4332 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 4333 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 4334 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 4335 | `	}` |
|        16 | 4336 | `	pVm->pHookSetAttr = 0;` |
|        16 | 4337 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        16 | 4338 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 4339 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 4340 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 4341 | `	}` |
|        16 | 4342 | `	pVm->pMagicCallClass = 0;` |
|        16 | 4343 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        16 | 4344 | `	pVm->nExceptDepth = 0;` |
|         - | 4345 | `	/* spl_autoload_register() callbacks are per request */` |
|        16 | 4346 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 4347 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 4348 | `		if( pCB ){` |
|       ! 0 | 4349 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 4350 | `		}` |
|       ! 0 | 4351 | `	}` |
|        16 | 4352 | `	SySetReset(&pVm->aAutoload);` |
|         - | 4353 | `	/* ...and so is the extension list they are searched with. */` |
|        16 | 4354 | `	SyBlobReset(&pVm->sAutoloadExt);` |
|        16 | 4355 | `	SyBlobAppend(&pVm->sAutoloadExt,PH7_SPL_AUTOLOAD_EXT,sizeof(PH7_SPL_AUTOLOAD_EXT)-1);` |
|         - | 4356 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 4357 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        16 | 4358 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 4359 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 4360 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 4361 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 4362 | `	}` |
|         - | 4363 | `	/* Output buffers */` |
|        16 | 4364 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 4365 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 4366 | `		if( pOb ){` |
|       ! 0 | 4367 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 4368 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 4369 | `		}` |
|       ! 0 | 4370 | `	}` |
|        16 | 4371 | `	SySetReset(&pVm->aOB);` |
|        16 | 4372 | `	pVm->nObDepth = 0;` |
|         - | 4373 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 4374 | `	{` |
|        16 | 4375 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        16 | 4376 | `		if( rc == SXRET_OK ){` |
|        16 | 4377 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         8 | 4378 | `		}` |
|        16 | 4379 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 4380 | `			return rc;` |
|         - | 4381 | `		}` |
|         - | 4382 | `	}` |
|         - | 4383 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 4384 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 4385 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 4386 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 4387 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 4388 | `	{` |
|         - | 4389 | `		SyHashEntry *pEntry;` |
|        16 | 4390 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3524 | 4391 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3508 | 4392 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 4393 | `			ph7_class_attr *pAttr;` |
|         - | 4394 | `			SyHashEntry *pAttrEntry;` |
|      3508 | 4395 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     24178 | 4396 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|     18916 | 4397 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     18916 | 4398 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 | 4399 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 | 4400 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 | 4401 | `				}` |
|       ! 0 | 4402 | `			}` |
|         - | 4403 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 4404 | `			 * slots too so VM reuse re-evaluates them. */` |
|      3508 | 4405 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|     14404 | 4406 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|     10896 | 4407 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|     10896 | 4408 | `				pAttr->nIdx = SXU32_HIGH;` |
|     10896 | 4409 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 4410 | `			}` |
|       ! 0 | 4411 | `		}` |
|        16 | 4412 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      3524 | 4413 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      3508 | 4414 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      3508 | 4415 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 4416 | `				return rc;` |
|         - | 4417 | `			}` |
|       ! 0 | 4418 | `		}` |
|         - | 4419 | `	}` |
|         - | 4420 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        16 | 4421 | `	SyBlobReset(&pVm->sConsumer);` |
|        16 | 4422 | `	pVm->nOutputLen = 0;` |
|        16 | 4423 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        16 | 4424 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|         - | 4425 | `	/* 0 is php's "no code set": a CLI run reads FALSE until something sets` |
|         - | 4426 | `	 * one, and a request-driven run is put at 200 when the request arrives. */` |
|        16 | 4427 | `	pVm->iResponseStatus = 0;` |
|        16 | 4428 | `	pVm->bHeadersSent = 0;` |
|        16 | 4429 | `	SyBlobReset(&pVm->sOutStartFile);` |
|        16 | 4430 | `	pVm->nOutStartLine = 0;` |
|        16 | 4431 | `	SyBlobReset(&pVm->sSessStartFile);` |
|        16 | 4432 | `	pVm->nSessStartLine = 0;` |
|        16 | 4433 | `	pVm->bHttpContext = 0;` |
|        16 | 4434 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|        16 | 4435 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|        16 | 4436 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|        16 | 4437 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|        16 | 4438 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|        16 | 4439 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         - | 4440 | `	/* The session's userland save handler belongs to the request that installed` |
|         - | 4441 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|         - | 4442 | `	 * through the previous script's object. */` |
|        16 | 4443 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|        16 | 4444 | `	pVm->bSessOpened = 0;` |
|        16 | 4445 | `	SyBlobReset(&pVm->sSessData);` |
|        16 | 4446 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 4447 | `#ifdef PH7_ENABLE_PCRE` |
|        16 | 4448 | `	pVm->iPcreLastError = 0;` |
|         - | 4449 | `#endif` |
|         - | 4450 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 4451 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        16 | 4452 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 4453 | `#endif` |
|         - | 4454 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 4455 | `	/* Close the previous request's databases: a reused VM (the -S server's)` |
|         - | 4456 | `	 * must not answer the next request through a handle that request opened. */` |
|        16 | 4457 | `	PH7_PdoVmReset(&(*pVm));` |
|        16 | 4458 | `	PH7_Sqlite3VmReset(&(*pVm));` |
|         - | 4459 | `#endif` |
|         - | 4460 | `#ifdef PH7_ENABLE_CURL` |
|         - | 4461 | `	/* Same rule for the previous request's curl handles, which hold sockets` |
|         - | 4462 | `	 * and a connection cache of their own. */` |
|        16 | 4463 | `	PH7_CurlVmReset(&(*pVm));` |
|         - | 4464 | `#endif` |
|         - | 4465 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - | 4466 | `	/* And every ext/sockets descriptor: a reused VM must not leave the previous` |
|         - | 4467 | `	 * request's listener bound to its port. */` |
|        16 | 4468 | `	PH7_SocketsVmReset(&(*pVm));` |
|         - | 4469 | `#endif` |
|         - | 4470 | `	/* php's "last opened directory stream" is per REQUEST: a reused VM must not` |
|         - | 4471 | `	 * let readdir() with no argument reach the previous one's handle. */` |
|        16 | 4472 | `	pVm->pLastDir = 0;` |
|         - | 4473 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 4474 | `	/* And its deflate/inflate contexts: a z_stream's window is libz's own` |
|         - | 4475 | `	 * allocation, which the wholesale release below would not reach. */` |
|        16 | 4476 | `	PH7_ZlibVmReset(&(*pVm));` |
|        16 | 4477 | `	PH7_ZipVmReset(&(*pVm));` |
|         - | 4478 | `#endif` |
|         - | 4479 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 4480 | `	/* And every certificate, key and signing request still held: each is` |
|         - | 4481 | `	 * OpenSSL's own allocation, outside the backend the release below wipes. */` |
|        16 | 4482 | `	PH7_SslVmReset(&(*pVm));` |
|         - | 4483 | `#endif` |
|         - | 4484 | `	/* And every archive it opened: php's phar cache is per-request too. */` |
|        16 | 4485 | `	PH7_PharVmReset(&(*pVm));` |
|         - | 4486 | `	/* Drop the stream contexts this run created, the default one included: a` |
|         - | 4487 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|         - | 4488 | `	 * previous one's stream_context_set_default(). */` |
|        16 | 4489 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|         - | 4490 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|         - | 4491 | `	 * still owns memory the next request must not inherit. */` |
|        16 | 4492 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|         - | 4493 | `	/* And the last http:// exchange's response headers, for the same reason:` |
|         - | 4494 | `	 * http_get_last_response_headers() must not answer the previous request's. */` |
|        16 | 4495 | `	PH7_HttpClearResponseHeaders(&(*pVm));` |
|        16 | 4496 | `	pVm->iCmpCallbackExc = 0;` |
|        16 | 4497 | `	pVm->bHaltRequested = 0;` |
|        16 | 4498 | `	pVm->iExitStatus = 0;` |
|        16 | 4499 | `	pVm->nSpreadCallBase = 0;` |
|        16 | 4500 | `	VmSpreadCaptureReset(pVm);` |
|        16 | 4501 | `	pVm->nRecursionDepth = 0;` |
|        16 | 4502 | `	pVm->pActiveCtx = 0;` |
|        16 | 4503 | `	pVm->pCurFiber = 0;` |
|        16 | 4504 | `	pVm->pCoalesceObj = 0;` |
|        16 | 4505 | `	pVm->bCoalesceArmed = 0;` |
|        16 | 4506 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 4507 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 4508 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        16 | 4509 | `	pVm->nNextObjId = 1;` |
|         - | 4510 | `	/* Set the ready flag */` |
|        16 | 4511 | `	pVm->nMagic = PH7_VM_RUN;` |
|        16 | 4512 | `	return SXRET_OK;` |
|         8 | 4513 | `}` |
|         - | 4514 | `/*` |
|         - | 4515 | ` * Release a Virtual Machine.` |
|         - | 4516 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 4517 | ` */` |
|      5629 | 4518 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 4519 | `{` |
|         - | 4520 | `	/* Set the stale magic number */` |
|      5634 | 4521 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 4522 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 4523 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 4524 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      5634 | 4525 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 4526 | `#endif` |
|         - | 4527 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 4528 | `	/* Same rule for the sqlite3 handles behind still-open PDO objects. */` |
|      5634 | 4529 | `	PH7_PdoVmRelease(pVm);` |
|      5634 | 4530 | `	PH7_Sqlite3VmRelease(pVm);` |
|         - | 4531 | `#endif` |
|         - | 4532 | `#ifdef PH7_ENABLE_CURL` |
|         - | 4533 | `	/* Same rule for the libcurl handles behind still-open CurlHandle objects. */` |
|      5634 | 4534 | `	PH7_CurlVmRelease(pVm);` |
|         - | 4535 | `#endif` |
|         - | 4536 | `#if defined(PH7_ENABLE_NET) && !defined(PH7_DISABLE_BUILTIN_FUNC)` |
|         - | 4537 | `	/* Same rule for the descriptors behind still-open Socket objects. */` |
|      5634 | 4538 | `	PH7_SocketsVmRelease(pVm);` |
|         - | 4539 | `#endif` |
|         - | 4540 | `#ifdef PH7_ENABLE_ZLIB` |
|         - | 4541 | `	/* Same rule for the z_streams behind still-open Deflate/InflateContexts. */` |
|      5634 | 4542 | `	PH7_ZlibVmRelease(pVm);` |
|         - | 4543 | `	/* ...and for the archives behind still-open ZipArchives, whose entry` |
|         - | 4544 | `	 * tables are this allocator's but whose lifetime is not the object's. */` |
|      5634 | 4545 | `	PH7_ZipVmRelease(pVm);` |
|         - | 4546 | `#endif` |
|         - | 4547 | `#ifdef PH7_ENABLE_OPENSSL` |
|         - | 4548 | `	/* Same rule for the X509/EVP_PKEY handles behind still-open objects. */` |
|      5634 | 4549 | `	PH7_SslVmRelease(pVm);` |
|         - | 4550 | `#endif` |
|         - | 4551 | `	/* ext/pcntl put a C signal handler in front of this VM's handler table;` |
|         - | 4552 | `	 * every disposition it took over goes back to SIG_DFL before the allocator` |
|         - | 4553 | `	 * that table lives in disappears. */` |
|      5634 | 4554 | `	PH7_PcntlVmRelease(pVm);` |
|         - | 4555 | `	/* ...and for the syslog prefix a still-open openlog() points at. */` |
|      5634 | 4556 | `	PH7_SyslogVmRelease(pVm);` |
|      5634 | 4557 | `	PH7_PharVmRelease(pVm);` |
|         - | 4558 | `	/* Same rule for the OS directory streams behind still-open directory` |
|         - | 4559 | `	 * iterators: the DIR lives outside the backend. */` |
|      5634 | 4560 | `	PH7_SplDirVmRelease(pVm);` |
|      5634 | 4561 | `	SySetRelease(&pVm->aDeadClosure);` |
|      5634 | 4562 | `	PH7_GcRelease(pVm);` |
|         - | 4563 | `	/* Release the private memory subsystem */` |
|      5634 | 4564 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      5634 | 4565 | `	return SXRET_OK;` |
|         5 | 4566 | `}` |
|         - | 4567 | `/*` |
|         - | 4568 | ` * Initialize a foreign function call context.` |
|         - | 4569 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 4570 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 4571 | ` * functions.` |
|         - | 4572 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 4573 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 4574 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 4575 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 4576 | ` */` |
|   6468639 | 4577 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 4578 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 4579 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 4580 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 4581 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 4582 | `	sxi32 iFlags          /* Control flags */` |
|         - | 4583 | `	)` |
|         5 | 4584 | `{` |
|   6468644 | 4585 | `	pOut->pFunc = pFunc;` |
|   6468644 | 4586 | `	pOut->pVm   = pVm;` |
|   6468644 | 4587 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   6468644 | 4588 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 4589 | `	/* Assume a null return value */` |
|   6468644 | 4590 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   6468644 | 4591 | `	pOut->pRet = pRet;` |
|   6468644 | 4592 | `	pOut->iFlags = iFlags;` |
|   6468644 | 4593 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   6468644 | 4594 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 4595 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 4596 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 4597 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 4598 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   6468644 | 4599 | `	pOut->pThis = 0;` |
|   6468644 | 4600 | `	pOut->pCalledClass = 0;` |
|   6468644 | 4601 | `	pOut->bThisInit = 0;` |
|         - | 4602 | `	/* Only the scratch context a native PROPERTY handler runs on carries one; every` |
|         - | 4603 | `	 * ordinary call leaves it empty, so a refusal there throws as it always did. */` |
|   6468644 | 4604 | `	pOut->pPropCtx = 0;` |
|   6468644 | 4605 | `	return SXRET_OK;` |
|         5 | 4606 | `}` |
|         - | 4607 | `/*` |
|         - | 4608 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 4609 | ` * left behind.` |
|         - | 4610 | ` */` |
|   6468655 | 4611 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 4612 | `{` |
|         - | 4613 | `	sxu32 n;` |
|   6468660 | 4614 | `	if( pCtx->bThisInit ){` |
|         - | 4615 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 4616 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 4617 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 4618 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      8527 | 4619 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      8527 | 4620 | `		pCtx->sThis.x.pOther = 0;` |
|      8527 | 4621 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      8527 | 4622 | `		pCtx->bThisInit = 0;` |
|      4261 | 4623 | `	}` |
|   6468660 | 4624 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|    849955 | 4625 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|   2141999 | 4626 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|   1292049 | 4627 | `			if( apObj[n] == 0 ){` |
|         - | 4628 | `				/* Already released */` |
|     55293 | 4629 | `				continue;` |
|         - | 4630 | `			}` |
|   1236761 | 4631 | `			PH7_MemObjRelease(apObj[n]);` |
|   1236761 | 4632 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|    618284 | 4633 | `		}` |
|    849955 | 4634 | `		SySetRelease(&pCtx->sVar);` |
|    424936 | 4635 | `	}` |
|   6468660 | 4636 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 4637 | `		ph7_aux_data *aAux;` |
|         - | 4638 | `		void *pChunk;` |
|         - | 4639 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 4640 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 4641 | `		 */` |
|      7676 | 4642 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|     30259 | 4643 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|     22588 | 4644 | `			pChunk = aAux[n].pAuxData;` |
|         - | 4645 | `			/* Release the chunk */` |
|     22588 | 4646 | `			if( pChunk ){` |
|     21896 | 4647 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|     10801 | 4648 | `			}` |
|     11152 | 4649 | `		}` |
|      7676 | 4650 | `		SySetRelease(&pCtx->sChunk);` |
|      3787 | 4651 | `	}` |
|   6468660 | 4652 | `}` |
|         - | 4653 | `/*` |
|         - | 4654 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 4655 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 4656 | ` */` |
|     55288 | 4657 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 4658 | `	ph7_context *pCtx, /* Call context */` |
|         - | 4659 | `	ph7_value *pValue  /* Release this value */` |
|         - | 4660 | `	)` |
|         5 | 4661 | `{` |
|     55293 | 4662 | `	if( pValue == 0 ){` |
|         - | 4663 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 4664 | `		return;` |
|         - | 4665 | `	}` |
|     55293 | 4666 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     55293 | 4667 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 4668 | `		sxu32 n;` |
|    814973 | 4669 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    814973 | 4670 | `			if( apObj[n] == pValue ){` |
|     55293 | 4671 | `				PH7_MemObjRelease(pValue);` |
|     55293 | 4672 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 4673 | `				/* Mark as released */` |
|     55293 | 4674 | `				apObj[n] = 0;` |
|     55293 | 4675 | `				break;` |
|         - | 4676 | `			}` |
|    379169 | 4677 | `		}` |
|     27603 | 4678 | `	}` |
|     27608 | 4679 | `}` |
|         - | 4680 | `/*` |
|         - | 4681 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 4682 | ` */` |
|  27040901 | 4683 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 4684 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 4685 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 4686 | `	)` |
|         5 | 4687 | `{` |
|  27040906 | 4688 | `	ph7_value *pTos = *ppTos;` |
|  58422743 | 4689 | `	while( nPop > 0 ){` |
|  31381842 | 4690 | `		PH7_MemObjRelease(pTos);` |
|  31381842 | 4691 | `		pTos--;` |
|  31381842 | 4692 | `		nPop--;` |
|         5 | 4693 | `	}` |
|         - | 4694 | `	/* Top of the stack */` |
|  27040906 | 4695 | `	*ppTos = pTos;` |
|  27040906 | 4696 | `}` |
|         - | 4697 | `/*` |
|         - | 4698 | ` * Reserve a memory object.` |
|         - | 4699 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 4700 | ` */` |
|  23406647 | 4701 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 4702 | `{` |
|  23406652 | 4703 | `	ph7_value *pObj = 0;` |
|         - | 4704 | `	sxu32 nIdx;` |
|         - | 4705 | `	/* Check for a free slot. The head is a slot index, and the freed slot's own` |
|         - | 4706 | `	 * (dead) nIdx word holds the next one -- one load past the bounds test, the` |
|         - | 4707 | `	 * same shape as the SySetPop of the stack this replaced. The PH7_MemObjInit` |
|         - | 4708 | `	 * below is what takes MEMOBJ_POOLFREE back off: every acquire runs it, so the` |
|         - | 4709 | `	 * bit means "on the list" and nothing else. */` |
|  23406652 | 4710 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  23406652 | 4711 | `	if( pVm->aMemObj.nFreeHead != SXU32_HIGH ){` |
|  20343300 | 4712 | `		nIdx = pVm->aMemObj.nFreeHead;` |
|  20343300 | 4713 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|  20343300 | 4714 | `		if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_POOLFREE) == 0 ){` |
|         - | 4715 | `			/* Stale or corrupted chain (defensive -- truncate clears the head and` |
|         - | 4716 | `			 * clears the bit): abandon it rather than hand out a LIVE slot. */` |
|       ! 0 | 4717 | `			pVm->aMemObj.nFreeHead = SXU32_HIGH;` |
|       ! 0 | 4718 | `			pObj = 0;` |
|       ! 0 | 4719 | `		}else{` |
|  20343300 | 4720 | `			pVm->aMemObj.nFreeHead = pObj->nIdx;` |
|         - | 4721 | `		}` |
|  10169432 | 4722 | `	}` |
|  23406652 | 4723 | `	if( pObj == 0 ){` |
|         - | 4724 | `		/* Reserve a new memory object */` |
|   3063357 | 4725 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   3063357 | 4726 | `		if( pObj == 0 ){` |
|       ! 0 | 4727 | `			return 0;` |
|         - | 4728 | `		}` |
|   1530886 | 4729 | `	}` |
|         - | 4730 | `	/* Set a null default value */` |
|  23406652 | 4731 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  23406652 | 4732 | `	pObj->nIdx = nIdx;` |
|  23406652 | 4733 | `	return pObj;` |
|  11700323 | 4734 | `}` |
|         - | 4735 | `/*` |
|         - | 4736 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 4737 | ` */` |
|     79305 | 4738 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 4739 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 4740 | `	const char *zKey,  /* Entry key */` |
|         - | 4741 | `	sxu32 nByte,       /* Key length */` |
|         - | 4742 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 4743 | `	)` |
|         5 | 4744 | `{` |
|         - | 4745 | `	ph7_value sKey;` |
|         - | 4746 | `	sxi32 rc;` |
|     79310 | 4747 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     79310 | 4748 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 4749 | `	/* Perform the insertion */` |
|     79310 | 4750 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     79310 | 4751 | `	PH7_MemObjRelease(&sKey);` |
|     79310 | 4752 | `	return rc;` |
|         5 | 4753 | `}` |
|         - | 4754 | `/*` |
|         - | 4755 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 4756 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 4757 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 4758 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 4759 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 4760 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 4761 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 4762 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 4763 | ` *     superglobal in place.` |
|         - | 4764 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 4765 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - | 4766 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - | 4767 | ` *     a plain variable.` |
|         - | 4768 | ` */` |
|       240 | 4769 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         4 | 4770 | `{` |
|       244 | 4771 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 4772 | `	SyHashEntry *pEntry;` |
|         - | 4773 | `	ph7_value *pObj;` |
|         - | 4774 | `	char *zDup;` |
|         - | 4775 | `	sxu32 nIdx;` |
|         - | 4776 | `	sxi32 rc;` |
|         - | 4777 | `	/* Walk down to the global frame */` |
|       310 | 4778 | `	while( pFrame->pParent ){` |
|        70 | 4779 | `		pFrame = pFrame->pParent;` |
|         4 | 4780 | `	}` |
|         - | 4781 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       244 | 4782 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|       244 | 4783 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 4784 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 4785 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 4786 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 4787 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 4788 | `		 * of $GLOBALS itself). */` |
|         5 | 4789 | `		pEntry = 0;` |
|         2 | 4790 | `	}` |
|       244 | 4791 | `	if( pEntry == 0 ){` |
|       244 | 4792 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       120 | 4793 | `	}` |
|       244 | 4794 | `	if( pEntry ){` |
|        37 | 4795 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 4796 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - | 4797 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|        35 | 4798 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|        35 | 4799 | `			return SXRET_OK;` |
|         - | 4800 | `		}` |
|         3 | 4801 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 4802 | `		if( pObj == 0 ){` |
|       ! 0 | 4803 | `			return SXERR_NOTFOUND;` |
|         - | 4804 | `		}` |
|         3 | 4805 | `		if( pValue ){` |
|         3 | 4806 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 4807 | `		}else{` |
|       ! 0 | 4808 | `			PH7_MemObjToNull(pObj);` |
|         - | 4809 | `		}` |
|         3 | 4810 | `		return SXRET_OK;` |
|         - | 4811 | `	}` |
|       208 | 4812 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 4813 | `		/* Reserve a fresh slot for the new global */` |
|       202 | 4814 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       202 | 4815 | `		if( pObj == 0 ){` |
|       ! 0 | 4816 | `			return SXERR_MEM;` |
|         - | 4817 | `		}` |
|       202 | 4818 | `		nIdx = pObj->nIdx;` |
|       103 | 4819 | `	}else{` |
|         - | 4820 | `		/* Reference assignment: bind the name to the existing slot */` |
|         7 | 4821 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nRefIdx);` |
|         7 | 4822 | `		if( pObj == 0 ){` |
|       ! 0 | 4823 | `			return SXERR_NOTFOUND;` |
|         - | 4824 | `		}` |
|         7 | 4825 | `		nIdx = nRefIdx;` |
|         - | 4826 | `	}` |
|       208 | 4827 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       208 | 4828 | `	if( zDup == 0 ){` |
|       ! 0 | 4829 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 4830 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 4831 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|       ! 0 | 4832 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 | 4833 | `		}` |
|       ! 0 | 4834 | `		return SXERR_MEM;` |
|         - | 4835 | `	}` |
|       208 | 4836 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       208 | 4837 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 4838 | `		if( nRefIdx == SXU32_HIGH ){` |
|       ! 0 | 4839 | `			VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 | 4840 | `		}` |
|       ! 0 | 4841 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 4842 | `		return rc;` |
|         - | 4843 | `	}` |
|         - | 4844 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       208 | 4845 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       208 | 4846 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       208 | 4847 | `	if( nRefIdx == SXU32_HIGH ){` |
|       202 | 4848 | `		pObj->nIdx = nIdx;` |
|       202 | 4849 | `		if( pValue ){` |
|       188 | 4850 | `			PH7_MemObjStore(pValue,pObj);` |
|        92 | 4851 | `		}` |
|        99 | 4852 | `	}` |
|       208 | 4853 | `	return SXRET_OK;` |
|       124 | 4854 | `}` |
|         - | 4855 | `/*` |
|         - | 4856 | ` * Extract a variable value from the top active VM frame.` |
|         - | 4857 | ` * Return a pointer to the variable value on success.` |
|         - | 4858 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 4859 | ` *` |
|         - | 4860 | ` * pnIdx, when given, receives the SLOT the name resolved to -- which the value's own` |
|         - | 4861 | ` * nIdx does not always carry (a superglobal's does not), and which the caller cannot` |
|         - | 4862 | ` * ask for afterwards without repeating the lookup this function just did.` |
|         - | 4863 | ` */` |
|   5302986 | 4864 | `static ph7_value * VmExtractMemObjEx(` |
|         - | 4865 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 4866 | `	const SyString *pName, /* Variable name */` |
|         - | 4867 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 4868 | `	int bCreate,           /* True to create the variable if non-existent */` |
|         - | 4869 | `	sxu32 *pnIdx           /* OUT: the slot the name is bound to (may be NULL) */` |
|         - | 4870 | `	)` |
|         5 | 4871 | `{` |
|   5302991 | 4872 | `	int bNullify = FALSE;` |
|         - | 4873 | `	SyHashEntry *pEntry;` |
|         - | 4874 | `	VmFrame *pFrame;` |
|         - | 4875 | `	ph7_value *pObj;` |
|         - | 4876 | `	sxu32 nIdx;` |
|         - | 4877 | `	sxi32 rc;` |
|         - | 4878 | `	/* Point to the top active frame */` |
|   5302991 | 4879 | `	pFrame = pVm->pFrame;` |
|   5302991 | 4880 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 4881 | `	/* Perform the lookup */` |
|   5302991 | 4882 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 4883 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 | 4884 | `		pName = &sAnnon;` |
|         - | 4885 | `		/* Always nullify the object */` |
|        18 | 4886 | `		bNullify = TRUE;` |
|        18 | 4887 | `		bDup = FALSE;` |
|         8 | 4888 | `	}` |
|         - | 4889 | `	/* Check the superglobals table first */` |
|   5302991 | 4890 | `	pEntry = PH7_VmSuperGet(&(*pVm),pName->zString,pName->nByte);` |
|   5302991 | 4891 | `	if( pEntry == 0 ){` |
|         - | 4892 | `		/* Query the top active frame */` |
|   5301519 | 4893 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|   5301519 | 4894 | `		if( pEntry == 0 ){` |
|   1293987 | 4895 | `			char *zName = (char *)pName->zString;` |
|         - | 4896 | `			VmSlot sLocal;` |
|   1293987 | 4897 | `			if( !bCreate ){` |
|         - | 4898 | `				/* Do not create the variable,return NULL instead */` |
|     18274 | 4899 | `				return 0;` |
|         - | 4900 | `			}` |
|         - | 4901 | `			/* No such variable,automatically create a new one and install` |
|         - | 4902 | `			 * it in the current frame.` |
|         - | 4903 | `			 */` |
|   1275718 | 4904 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1275718 | 4905 | `			if( pObj == 0 ){` |
|       ! 0 | 4906 | `				return 0;` |
|         - | 4907 | `			}` |
|   1275718 | 4908 | `			nIdx = pObj->nIdx;` |
|   1275718 | 4909 | `			if( bDup ){` |
|         - | 4910 | `				/* Duplicate name */` |
|     12791 | 4911 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|     12791 | 4912 | `				if( zName == 0 ){` |
|       ! 0 | 4913 | `					return 0;` |
|         - | 4914 | `				}` |
|      6353 | 4915 | `			}` |
|         - | 4916 | `			/* Link to the top active VM frame */` |
|   1275718 | 4917 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1275718 | 4918 | `			if( rc != SXRET_OK ){` |
|         - | 4919 | `				/* Return the slot to the free pool */` |
|       ! 0 | 4920 | `				VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       ! 0 | 4921 | `				return 0;` |
|         - | 4922 | `			}` |
|   1275718 | 4923 | `			if( pFrame->pParent != 0 ){` |
|         - | 4924 | `				/* Local variable */` |
|   1258417 | 4925 | `				sLocal.nIdx = nIdx;` |
|   1258417 | 4926 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    647463 | 4927 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - | 4928 | `				/* Register in the $GLOBALS array */` |
|     17047 | 4929 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|      8490 | 4930 | `			}` |
|         - | 4931 | `			/* Install in the reference table */` |
|   1275718 | 4932 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 4933 | `			/* Save object index */` |
|   1275718 | 4934 | `			pObj->nIdx = nIdx;` |
|    638781 | 4935 | `		}else{` |
|         - | 4936 | `			/* Extract variable contents */` |
|   4007537 | 4937 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|   4007537 | 4938 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|   4007537 | 4939 | `			if( bNullify && pObj ){` |
|         3 | 4940 | `				PH7_MemObjRelease(pObj);` |
|         1 | 4941 | `			}` |
|         - | 4942 | `		}` |
|   2641638 | 4943 | `	}else{` |
|         - | 4944 | `		/* Superglobal */` |
|      1477 | 4945 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      1477 | 4946 | `		pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|         - | 4947 | `	}` |
|   5284722 | 4948 | `	if( pnIdx ){` |
|   2479817 | 4949 | `		*pnIdx = nIdx;` |
|   1240682 | 4950 | `	}` |
|   5284722 | 4951 | `	return pObj;` |
|   2651384 | 4952 | `}` |
|   2822882 | 4953 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 4954 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 4955 | `	const SyString *pName, /* Variable name */` |
|         - | 4956 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 4957 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 4958 | `	)` |
|         5 | 4959 | `{` |
|   2822887 | 4960 | `	return VmExtractMemObjEx(&(*pVm),pName,bDup,bCreate,0);` |
|         5 | 4961 | `}` |
|         - | 4962 | `/*` |
|         - | 4963 | ` * Number a body's variables, once, from the body itself.` |
|         - | 4964 | ` *` |
|         - | 4965 | ` * Every instruction that names a variable the compiler wrote down -- OP_LOAD and` |
|         - | 4966 | ` * OP_STORE with a p3 -- gets a small NUMBER in its nSite, and the running frame then` |
|         - | 4967 | ` * answers that number out of an array instead of hashing the name (see VmFrame's` |
|         - | 4968 | ` * aLocalSlot). Two instructions naming the same variable get the same number, so the` |
|         - | 4969 | ` * frame holds one entry per NAME and not one per site.` |
|         - | 4970 | ` *` |
|         - | 4971 | ` * Names are compared by ADDRESS, which is exact and not an approximation: the compiler` |
|         - | 4972 | ` * interns every variable name it emits into one VM-lifetime buffer (pGen->hVar), so` |
|         - | 4973 | ` * within a body the same spelling is the same pointer. Two pointers for one spelling` |
|         - | 4974 | ` * would only cost a body two numbers for one name, which stays correct -- both entries` |
|         - | 4975 | ` * hold the same slot and both are emptied together.` |
|         - | 4976 | ` *` |
|         - | 4977 | ` * A body with more distinct names than PH7_VAR_SLOT_MAX numbers its most REFERENCED` |
|         - | 4978 | ` * ones: the pass counts static references first and hands the numbers out in that` |
|         - | 4979 | ` * order, so what a hot loop reads is what fits. The rest keep nSite = 0 and take the` |
|         - | 4980 | ` * hash path, exactly as every site did before this existed.` |
|         - | 4981 | ` *` |
|         - | 4982 | ` * Lazy and self-computing, like nMaxStack: a body that has not been walked yet just` |
|         - | 4983 | ` * walks. There is no path that can produce a WRONG number -- an unwalked body has 0` |
|         - | 4984 | ` * everywhere, which means "ask the table".` |
|         - | 4985 | ` */` |
|         - | 4986 | `#define VM_LOCAL_SCAN_MAX 32   /* distinct names the pass will rank; past this it stops` |
|         - | 4987 | `                                * counting and numbers what it has. A body naming more` |
|         - | 4988 | `                                * than this has long since stopped fitting the frame, and` |
|         - | 4989 | `                                * the three arrays below are C STACK -- 352 bytes at this` |
|         - | 4990 | `                                * width, which matters on a 16-frame embedded target. */` |
|   1172076 | 4991 | `static int VmInstrNamesVar(const VmInstr *pInstr)` |
|         5 | 4992 | `{` |
|   1172081 | 4993 | `	return ( pInstr->iOp == PH7_OP_LOAD \|\| pInstr->iOp == PH7_OP_STORE ) && pInstr->p3 != 0;` |
|         5 | 4994 | `}` |
|     28831 | 4995 | `static void VmNumberLocals(VmInstr *aInstr,sxu32 nInstr,sxu16 *pnName)` |
|         5 | 4996 | `{` |
|         - | 4997 | `	const char *azName[VM_LOCAL_SCAN_MAX];` |
|         - | 4998 | `	sxu16 aRef[VM_LOCAL_SCAN_MAX];   /* references, then re-used as name -> number+1 */` |
|         - | 4999 | `	sxu8 aRank[VM_LOCAL_SCAN_MAX];` |
|     28836 | 5000 | `	sxu32 nName = 0;` |
|         - | 5001 | `	sxu32 i,j,n;` |
|     28836 | 5002 | `	*pnName = 0;` |
|     28836 | 5003 | `	if( aInstr == 0 ){` |
|       ! 0 | 5004 | `		return;` |
|         - | 5005 | `	}` |
|         - | 5006 | `	/* Pass one: the distinct names, and how many instructions reach for each. */` |
|    665754 | 5007 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|         - | 5008 | `		const char *zName;` |
|    636923 | 5009 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|    554220 | 5010 | `			continue;` |
|         - | 5011 | `		}` |
|     82708 | 5012 | `		zName = (const char *)aInstr[i].p3;` |
|         - | 5013 | `		/* The length belongs to the name and not to the execution: measure it here,` |
|         - | 5014 | `		 * once, for the handlers that used to call SyStrlen on every pass. */` |
|     82708 | 5015 | `		if( aInstr[i].nAux == 0 ){` |
|     77214 | 5016 | `			aInstr[i].nAux = (sxu32)SyStrlen(zName);` |
|     38316 | 5017 | `		}` |
|    384359 | 5018 | `		for( j = 0 ; j < nName ; ++j ){` |
|    348167 | 5019 | `			if( azName[j] == zName ){` |
|     46516 | 5020 | `				if( aRef[j] < SXU16_HIGH ){` |
|     46516 | 5021 | `					aRef[j]++;   /* a count that saturates still ranks first */` |
|     23066 | 5022 | `				}` |
|     46516 | 5023 | `				break;` |
|         - | 5024 | `			}` |
|    149235 | 5025 | `		}` |
|     82708 | 5026 | `		if( j == nName ){` |
|     36197 | 5027 | `			if( nName >= VM_LOCAL_SCAN_MAX ){` |
|      1059 | 5028 | `				continue;` |
|         - | 5029 | `			}` |
|     35143 | 5030 | `			azName[nName] = zName;` |
|     35143 | 5031 | `			aRef[nName] = 1;` |
|     35143 | 5032 | `			nName++;` |
|     17458 | 5033 | `		}` |
|     40529 | 5034 | `	}` |
|     28836 | 5035 | `	if( nName < 1 ){` |
|     11177 | 5036 | `		return;` |
|         - | 5037 | `	}` |
|         - | 5038 | `	/* Rank by static reference count, first appearance breaking ties -- an insertion` |
|         - | 5039 | `	 * sort over at most VM_LOCAL_SCAN_MAX entries, run once per body. aRank[k] is the` |
|         - | 5040 | `	 * name that gets number k. */` |
|     52802 | 5041 | `	for( i = 0 ; i < nName ; ++i ){` |
|     57821 | 5042 | `		for( j = i ; j > 0 && aRef[aRank[j-1]] < aRef[i] ; --j ){` |
|     22683 | 5043 | `			aRank[j] = aRank[j-1];` |
|     11215 | 5044 | `		}` |
|         - | 5045 | `		/* aRank holds name INDICES, so VM_LOCAL_SCAN_MAX must fit an sxu8. */` |
|     35143 | 5046 | `		aRank[j] = (sxu8)i;` |
|     17463 | 5047 | `	}` |
|     17664 | 5048 | `	n = nName > PH7_VAR_SLOT_MAX ? PH7_VAR_SLOT_MAX : nName;` |
|         - | 5049 | `	/* aRef is re-used as name -> number+1, so pass two is a single lookup. */` |
|     52802 | 5050 | `	for( i = 0 ; i < nName ; ++i ){` |
|     35143 | 5051 | `		aRef[i] = 0;` |
|     17463 | 5052 | `	}` |
|     52744 | 5053 | `	for( i = 0 ; i < n ; ++i ){` |
|     35085 | 5054 | `		aRef[aRank[i]] = (sxu16)(i + 1);` |
|     17434 | 5055 | `	}` |
|         - | 5056 | `	/* Pass two: stamp the number on every instruction that names one. */` |
|    552822 | 5057 | `	for( i = 0 ; i < nInstr ; ++i ){` |
|         - | 5058 | `		const char *zName;` |
|    535163 | 5059 | `		if( !VmInstrNamesVar(&aInstr[i]) ){` |
|    452460 | 5060 | `			continue;` |
|         - | 5061 | `		}` |
|     82708 | 5062 | `		zName = (const char *)aInstr[i].p3;` |
|    384359 | 5063 | `		for( j = 0 ; j < nName ; ++j ){` |
|    383305 | 5064 | `			if( azName[j] == zName ){` |
|     81654 | 5065 | `				aInstr[i].nSite = aRef[j];` |
|     81654 | 5066 | `				break;` |
|         - | 5067 | `			}` |
|    149235 | 5068 | `		}` |
|     41056 | 5069 | `	}` |
|     17664 | 5070 | `	*pnName = (sxu16)n;` |
|     14290 | 5071 | `}` |
|         - | 5072 | `/*` |
|         - | 5073 | ` * Number a function body if it has not been numbered, and tell the frame about to run` |
|         - | 5074 | ` * it which body its numbers belong to. One branch per activation; the walk itself` |
|         - | 5075 | ` * happens once per function for the life of the VM.` |
|         - | 5076 | ` */` |
|    812116 | 5077 | `static void VmFrameNumberBody(VmFrame *pFrame,ph7_vm_func *pFunc)` |
|         5 | 5078 | `{` |
|    812121 | 5079 | `	if( !pFunc->bNumbered ){` |
|     34796 | 5080 | `		VmNumberLocals((VmInstr *)SySetBasePtr(&pFunc->aByteCode),` |
|     11513 | 5081 | `			SySetUsed(&pFunc->aByteCode),&pFunc->nLocalName);` |
|     23283 | 5082 | `		pFunc->bNumbered = 1;` |
|     11513 | 5083 | `	}` |
|    812121 | 5084 | `	if( pFunc->nLocalName > 0 ){` |
|    181701 | 5085 | `		pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(&pFunc->aByteCode);` |
|     90806 | 5086 | `	}` |
|    812121 | 5087 | `}` |
|         - | 5088 | `/*` |
|         - | 5089 | ` * Forget where this frame's variables live. Called from the doors that can move a NAME` |
|         - | 5090 | ` * to a different slot; a door that only ever INSTALLS a name the frame did not have` |
|         - | 5091 | ` * does not need it, because a lookup that found nothing is never filed.` |
|         - | 5092 | ` */` |
|     78084 | 5093 | `PH7_PRIVATE void VmVarMemoFlush(VmFrame *pFrame)` |
|         5 | 5094 | `{` |
|         - | 5095 | `	sxu32 i;` |
|   2264441 | 5096 | `	for( i = 0 ; i < PH7_VAR_SLOT_MAX ; ++i ){` |
|   2186357 | 5097 | `		pFrame->aLocalSlot[i] = 0;` |
|   1091613 | 5098 | `	}` |
|     78089 | 5099 | `}` |
|         - | 5100 | `/*` |
|         - | 5101 | ` * VmExtractMemObj for a name the CALLER guarantees outlives the lookup -- a variable` |
|         - | 5102 | ` * name the compiler interned into the bytecode, and nothing else.` |
|         - | 5103 | ` *` |
|         - | 5104 | ` * Every variable access consults the superglobal table and then hashes the name into` |
|         - | 5105 | ` * the frame's symbol table; measured on the ecosystem gate's phpcs step that was the` |
|         - | 5106 | ` * largest single row in the engine's whole name-lookup census, and the hash is over a` |
|         - | 5107 | ` * name whose answer cannot change between two accesses in the same frame unless` |
|         - | 5108 | ` * something re-binds it. So the answer is remembered on the frame, BY NUMBER (see` |
|         - | 5109 | ` * VmFrame's aLocalSlot), and the second and later reads of a variable inside one` |
|         - | 5110 | ` * activation cost an array index.` |
|         - | 5111 | ` *` |
|         - | 5112 | ` * nSlot is the number the body gave this name plus one, and aCode the instruction` |
|         - | 5113 | ` * array it was numbered in -- 0 for a caller that has neither, which then pays the` |
|         - | 5114 | ` * lookup it always did. The aCode compare is what keeps an included unit, an eval and` |
|         - | 5115 | ` * a default-argument mini-program from reading numbers that are not theirs: they share` |
|         - | 5116 | ` * the frame, so their instructions must not index its array.` |
|         - | 5117 | ` *` |
|         - | 5118 | ` * bDup is deliberately absent: a name that has to be COPIED to become a symbol-table` |
|         - | 5119 | ` * key is by definition not one that outlives the lookup.` |
|         - | 5120 | ` */` |
|  16264723 | 5121 | `PH7_PRIVATE ph7_value * PH7_VmExtractVarSlot(` |
|         - | 5122 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 5123 | `	const SyString *pName, /* Variable name -- interned, NUL-terminated, VM-lifetime */` |
|         - | 5124 | `	int bCreate,           /* True to create the variable if non-existent */` |
|         - | 5125 | `	sxu32 nSlot,           /* The body's number for this name, plus one (0 = none) */` |
|         - | 5126 | `	const VmInstr *aCode   /* The instruction array nSlot was numbered in */` |
|         - | 5127 | `	)` |
|         5 | 5128 | `{` |
|         - | 5129 | `	VmFrame *pFrame;` |
|         - | 5130 | `	ph7_value *pObj;` |
|         - | 5131 | `	sxu32 nIdx;` |
|  16264728 | 5132 | `	if( pName->nByte < 1 \|\| pName->zString == 0 ){` |
|       ! 0 | 5133 | `		return VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,0);` |
|         - | 5134 | `	}` |
|  16264728 | 5135 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  16693294 | 5136 | `	if( nSlot > 0 && pFrame->pCodeBase == aCode ){` |
|  14643709 | 5137 | `		sxu32 nCached = pFrame->aLocalSlot[nSlot - 1];` |
|  14643709 | 5138 | `		if( nCached > 0 ){` |
|  13784624 | 5139 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nCached - 1);` |
|  13784624 | 5140 | `			if( pObj ){` |
|  13784624 | 5141 | `				return pObj;` |
|         - | 5142 | `			}` |
|       ! 0 | 5143 | `		}` |
|    430524 | 5144 | `	}else{` |
|   1621024 | 5145 | `		nSlot = 0;` |
|         - | 5146 | `	}` |
|   2480109 | 5147 | `	nIdx = SXU32_HIGH;` |
|   2480109 | 5148 | `	pObj = VmExtractMemObjEx(&(*pVm),pName,FALSE,bCreate,&nIdx);` |
|   2480109 | 5149 | `	if( pObj && nSlot > 0 && nIdx != SXU32_HIGH ){` |
|         - | 5150 | `		/* VmExtractMemObjEx may have grown the frame chain's tables, but never the` |
|         - | 5151 | `		 * chain itself, so the frame the answer belongs to is still this one. */` |
|    858886 | 5152 | `		pFrame->aLocalSlot[nSlot - 1] = nIdx + 1;` |
|    430417 | 5153 | `	}` |
|   2480109 | 5154 | `	return pObj;` |
|   8141179 | 5155 | `}` |
|         - | 5156 | `/*` |
|         - | 5157 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 5158 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 5159 | ` */` |
|     50849 | 5160 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 5161 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 5162 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 5163 | `	sxu32 nByte        /* zName length */` |
|         - | 5164 | `	)` |
|         5 | 5165 | `{` |
|         - | 5166 | `	SyHashEntry *pEntry;` |
|         - | 5167 | `	ph7_value *pValue;` |
|         - | 5168 | `	sxu32 nIdx;` |
|         - | 5169 | `	/* Query the superglobal table */` |
|     50854 | 5170 | `	pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|     50854 | 5171 | `	if( pEntry == 0 ){` |
|         - | 5172 | `		/* No such entry */` |
|       ! 0 | 5173 | `		return 0;` |
|         - | 5174 | `	}` |
|         - | 5175 | `	/* Extract the superglobal index in the global object pool */` |
|     50854 | 5176 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 5177 | `	/* Extract the variable value  */` |
|     50854 | 5178 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|     50854 | 5179 | `	return pValue;` |
|     25389 | 5180 | `}` |
|         - | 5181 | `/*` |
|         - | 5182 | ` * Perform a raw hashmap insertion.` |
|         - | 5183 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 5184 | ` */` |
|     39663 | 5185 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 5186 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 5187 | `	const char *zKey,   /* Entry key */` |
|         - | 5188 | `	int nKeylen,        /* zKey length*/` |
|         - | 5189 | `	const char *zData,  /* Entry data */` |
|         - | 5190 | `	int nLen            /* zData length */` |
|         - | 5191 | `	)` |
|         5 | 5192 | `{` |
|         - | 5193 | `	ph7_value sKey,sValue;` |
|         - | 5194 | `	sxi32 rc;` |
|     39668 | 5195 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     39668 | 5196 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     39668 | 5197 | `	if( zKey ){` |
|     34085 | 5198 | `		if( nKeylen < 0 ){` |
|     33911 | 5199 | `			nKeylen = (int)SyStrlen(zKey);` |
|     16926 | 5200 | `		}` |
|     34085 | 5201 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     17013 | 5202 | `	}` |
|     39668 | 5203 | `	if( zData ){` |
|     39668 | 5204 | `		if( nLen < 0 ){` |
|         - | 5205 | `			/* Compute length automatically */` |
|     22445 | 5206 | `			nLen = (int)SyStrlen(zData);` |
|     11202 | 5207 | `		}` |
|     39668 | 5208 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     19800 | 5209 | `	}` |
|         - | 5210 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 5211 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 5212 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 5213 | `	 * every argument under "". */` |
|     39668 | 5214 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     39668 | 5215 | `	PH7_MemObjRelease(&sKey);` |
|     39668 | 5216 | `	PH7_MemObjRelease(&sValue);` |
|     39668 | 5217 | `	return rc;` |
|         5 | 5218 | `}` |
|         - | 5219 | `/*` |
|         - | 5220 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 5221 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 5222 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 5223 | ` */` |
|        34 | 5224 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 5225 | `{` |
|        38 | 5226 | `	sxi64 iVal = 0;` |
|        38 | 5227 | `	if( nValue == 0 ){` |
|       ! 0 | 5228 | `		return 0;` |
|         - | 5229 | `	}` |
|        34 | 5230 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 5231 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 5232 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 5233 | `		return 1;` |
|         - | 5234 | `	}` |
|        38 | 5235 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 5236 | `	return iVal != 0;` |
|        21 | 5237 | `}` |
|         - | 5238 | `/*` |
|         - | 5239 | ` * Configure a working virtual machine instance.` |
|         - | 5240 | ` *` |
|         - | 5241 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 5242 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 5243 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 5244 | ` * The second argument to this function is an integer configuration option` |
|         - | 5245 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 5246 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 5247 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 5248 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 5249 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 5250 | ` */` |
|    179967 | 5251 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 5252 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 5253 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 5254 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 5255 | `	)` |
|         5 | 5256 | `{` |
|    179972 | 5257 | `	sxi32 rc = SXRET_OK;` |
|    179972 | 5258 | `	switch(nOp){` |
|      2758 | 5259 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      5512 | 5260 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      5512 | 5261 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 5262 | `		/* VM output consumer callback */` |
|         - | 5263 | `#ifdef UNTRUST` |
|         - | 5264 | `		if( xConsumer == 0 ){` |
|         - | 5265 | `			rc = SXERR_CORRUPT;` |
|         - | 5266 | `			break;` |
|         - | 5267 | `		}` |
|         - | 5268 | `#endif` |
|         - | 5269 | `		/* Install the output consumer */` |
|      5512 | 5270 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      5512 | 5271 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      5512 | 5272 | `		break;` |
|         - | 5273 | `							   }` |
|      2758 | 5274 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      5512 | 5275 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      5512 | 5276 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 5277 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 5278 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 5279 | `		 * instead of the program-output stream. */` |
|         - | 5280 | `#ifdef UNTRUST` |
|         - | 5281 | `		if( xConsumer == 0 ){` |
|         - | 5282 | `			rc = SXERR_CORRUPT;` |
|         - | 5283 | `			break;` |
|         - | 5284 | `		}` |
|         - | 5285 | `#endif` |
|      5512 | 5286 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      5512 | 5287 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      5512 | 5288 | `		break;` |
|         - | 5289 | `								   }` |
|      2811 | 5290 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 5291 | `		/* Import path */` |
|         - | 5292 | `		  const char *zPath;` |
|         - | 5293 | `		  SyString sPath;` |
|      5618 | 5294 | `		  zPath = va_arg(ap,const char *);` |
|         - | 5295 | `#if defined(UNTRUST)` |
|         - | 5296 | `		  if( zPath == 0 ){` |
|         - | 5297 | `			  rc = SXERR_EMPTY;` |
|         - | 5298 | `			  break;` |
|         - | 5299 | `		  }` |
|         - | 5300 | `#endif` |
|      5618 | 5301 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 5302 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 5303 | `#ifdef __WINNT__` |
|         5 | 5304 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 5305 | `#endif` |
|     11231 | 5306 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 5307 | `		  /* Remove leading and trailing white spaces */` |
|      5618 | 5308 | `		  SyStringFullTrim(&sPath);` |
|      5618 | 5309 | `		  if( sPath.nByte > 0 ){` |
|         - | 5310 | `			  /* Store the path in the corresponding conatiner */` |
|      5618 | 5311 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      2802 | 5312 | `		  }` |
|      5618 | 5313 | `		  break;` |
|         - | 5314 | `									 }` |
|      2781 | 5315 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 5316 | `		/* Run-Time Error report */` |
|      5558 | 5317 | `		pVm->bErrReport = 1;` |
|      5558 | 5318 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      5558 | 5319 | `		break;` |
|         2 | 5320 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 5321 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 5322 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 5323 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 5324 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 5325 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 5326 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 5327 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 5328 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 5329 | `		int nDepth = va_arg(ap,int);` |
|         5 | 5330 | `		if( nDepth >= 0 ){` |
|         5 | 5331 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 5332 | `		}` |
|         5 | 5333 | `		break;` |
|         - | 5334 | `									   }` |
|         5 | 5335 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 5336 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 5337 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 5338 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 5339 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 5340 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 5341 | `		 * so it is rejected as a footgun). */` |
|        12 | 5342 | `		int nDepth = va_arg(ap,int);` |
|        12 | 5343 | `		if( nDepth > 1 ){` |
|        12 | 5344 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 5345 | `		}` |
|        12 | 5346 | `		break;` |
|         - | 5347 | `									   }` |
|       ! 0 | 5348 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 5349 | `		/* VM output length in bytes */` |
|       ! 0 | 5350 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 5351 | `#ifdef UNTRUST` |
|         - | 5352 | `		if( pOut == 0 ){` |
|         - | 5353 | `			rc = SXERR_CORRUPT;` |
|         - | 5354 | `			break;` |
|         - | 5355 | `		}` |
|         - | 5356 | `#endif` |
|       ! 0 | 5357 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 5358 | `		break;` |
|         - | 5359 | `							   }` |
|         - | 5360 |  |
|     30978 | 5361 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 5362 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 5363 | `		/* Create a new superglobal/global variable */` |
|     61862 | 5364 | `		const char *zName = va_arg(ap,const char *);` |
|     61862 | 5365 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 5366 | `		SyHashEntry *pEntry;` |
|         - | 5367 | `		ph7_value *pObj;` |
|         - | 5368 | `		sxu32 nByte;` |
|         - | 5369 | `		sxu32 nIdx;` |
|         - | 5370 | `#ifdef UNTRUST` |
|         - | 5371 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 5372 | `			rc = SXERR_CORRUPT;` |
|         - | 5373 | `			break;` |
|         - | 5374 | `		}` |
|         - | 5375 | `#endif` |
|     61862 | 5376 | `		nByte = SyStrlen(zName);` |
|     61862 | 5377 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 5378 | `			/* Check if the superglobal is already installed */` |
|     56355 | 5379 | `			pEntry = PH7_VmSuperGet(&(*pVm),zName,nByte);` |
|     28135 | 5380 | `		}else{` |
|         - | 5381 | `			/* Query the top active VM frame */` |
|      5512 | 5382 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 5383 | `		}` |
|     61862 | 5384 | `		if( pEntry ){` |
|         - | 5385 | `			/* Variable already installed */` |
|       ! 0 | 5386 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 5387 | `			/* Extract contents */` |
|       ! 0 | 5388 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 5389 | `			if( pObj ){` |
|         - | 5390 | `				/* Overwrite old contents */` |
|       ! 0 | 5391 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 5392 | `			}` |
|       ! 0 | 5393 | `		}else{` |
|         - | 5394 | `			/* Install a new variable */` |
|     61862 | 5395 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     61862 | 5396 | `			if( pObj == 0 ){` |
|       ! 0 | 5397 | `				rc = SXERR_MEM;` |
|       ! 0 | 5398 | `				break;` |
|         - | 5399 | `			}` |
|     61862 | 5400 | `			nIdx = pObj->nIdx;` |
|         - | 5401 | `			/* Copy value */` |
|     61862 | 5402 | `			PH7_MemObjStore(pValue,pObj);` |
|     61862 | 5403 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 5404 | `				/* Install the superglobal */` |
|     56355 | 5405 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     56355 | 5406 | `				if( rc == SXRET_OK ){` |
|     56355 | 5407 | `					PH7_VmSuperNote(&(*pVm),zName,nByte);` |
|     28130 | 5408 | `				}` |
|     28135 | 5409 | `			}else{` |
|         - | 5410 | `				/* Install in the current frame */` |
|      5512 | 5411 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 5412 | `			}` |
|     61862 | 5413 | `			if( rc == SXRET_OK ){` |
|         - | 5414 | `				SyHashEntry *pRef;` |
|     61862 | 5415 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     56355 | 5416 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     28135 | 5417 | `				}else{` |
|      5512 | 5418 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 5419 | `				}` |
|         - | 5420 | `				/* Install in the reference table */` |
|     61862 | 5421 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     61862 | 5422 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 5423 | `					/* Register in the $GLOBALS array */` |
|     61862 | 5424 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     30879 | 5425 | `				}` |
|     30879 | 5426 | `			}` |
|         - | 5427 | `		}` |
|     61862 | 5428 | `		break;` |
|         - | 5429 | `									}` |
|     16980 | 5430 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 5431 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 5432 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 5433 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 5434 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 5435 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 5436 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     33911 | 5437 | `		const char *zKey   = va_arg(ap,const char *);` |
|     33911 | 5438 | `		const char *zValue = va_arg(ap,const char *);` |
|     33911 | 5439 | `		int nLen = va_arg(ap,int);` |
|         - | 5440 | `		ph7_hashmap *pMap;` |
|         - | 5441 | `		ph7_value *pValue;` |
|     33911 | 5442 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 5443 | `			/* Extract the $_ENV superglobal */` |
|       ! 0 | 5444 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     33911 | 5445 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 5446 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 5447 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     33911 | 5448 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 5449 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 5450 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     33911 | 5451 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 5452 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 5453 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     33911 | 5454 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 5455 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 5456 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     33911 | 5457 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 5458 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 5459 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 5460 | `		}else{` |
|         - | 5461 | `			/* Extract the $_SERVER superglobal */` |
|     33911 | 5462 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 5463 | `		}` |
|     33911 | 5464 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 5465 | `			/* No such entry */` |
|       ! 0 | 5466 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 5467 | `			break;` |
|         - | 5468 | `		}` |
|         - | 5469 | `		/* Point to the hashmap */` |
|     33911 | 5470 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 5471 | `		/* Perform the insertion */` |
|     33911 | 5472 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     33911 | 5473 | `		break;` |
|         - | 5474 | `								   }` |
|      2796 | 5475 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 5476 | `		/* Script arguments */` |
|      5588 | 5477 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 5478 | `		ph7_hashmap *pMap;` |
|         - | 5479 | `		ph7_value *pValue;` |
|         - | 5480 | `		sxu32 n;` |
|         - | 5481 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 5482 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 5483 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 5484 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      5588 | 5485 | `		if( zValue == 0 ){` |
|       ! 0 | 5486 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 5487 | `			break;` |
|         - | 5488 | `		}` |
|         - | 5489 | `		/* Extract the $argv array */` |
|      5588 | 5490 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      5588 | 5491 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 5492 | `			/* No such entry */` |
|       ! 0 | 5493 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 5494 | `			break;` |
|         - | 5495 | `		}` |
|         - | 5496 | `		/* Point to the hashmap */` |
|      5588 | 5497 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 5498 | `		/* Perform the insertion */` |
|      5588 | 5499 | `		n = (sxu32)SyStrlen(zValue);` |
|      5588 | 5500 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      5588 | 5501 | `		break;` |
|         - | 5502 | `								  }` |
|      2758 | 5503 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 5504 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 5505 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 5506 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 5507 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 5508 | `		ph7_value *pArgv,*pServer;` |
|         - | 5509 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 5510 | `		ph7_value sArgvVal,sKey,sCount;` |
|      5512 | 5511 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      5512 | 5512 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      5507 | 5513 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      5512 | 5514 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 5515 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 5516 | `			break;` |
|         - | 5517 | `		}` |
|      5512 | 5518 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      5512 | 5519 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 5520 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      5512 | 5521 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      5512 | 5522 | `		if( pDup == 0 ){` |
|       ! 0 | 5523 | `			rc = SXERR_MEM;` |
|       ! 0 | 5524 | `			break;` |
|         - | 5525 | `		}` |
|      5512 | 5526 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      5512 | 5527 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      5512 | 5528 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      5512 | 5529 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      5512 | 5530 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      5512 | 5531 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      5512 | 5532 | `		PH7_MemObjRelease(&sKey);` |
|         - | 5533 | `		/* $_SERVER['argc'] = count($argv). */` |
|      5512 | 5534 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      5512 | 5535 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      5512 | 5536 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      5512 | 5537 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      5512 | 5538 | `		PH7_MemObjRelease(&sCount);` |
|      5512 | 5539 | `		PH7_MemObjRelease(&sKey);` |
|      5512 | 5540 | `		rc = SXRET_OK;` |
|      5512 | 5541 | `		break;` |
|         - | 5542 | `								  }` |
|        66 | 5543 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 5544 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 5545 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 5546 | `		 * apply immediately so they take effect even if the script never` |
|         - | 5547 | `		 * touches the INI API. */` |
|       135 | 5548 | `		const char *zName = va_arg(ap,const char *);` |
|       135 | 5549 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 5550 | `		VmIniEntry sEntry;` |
|         - | 5551 | `		char *zDupN,*zDupV;` |
|         - | 5552 | `		sxu32 nName,nValue;` |
|       135 | 5553 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 5554 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 5555 | `			break;` |
|         - | 5556 | `		}` |
|       135 | 5557 | `		if( zValue == 0 ){` |
|       ! 0 | 5558 | `			zValue = "";` |
|       ! 0 | 5559 | `		}` |
|       135 | 5560 | `		nName = (sxu32)SyStrlen(zName);` |
|       135 | 5561 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       135 | 5562 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       135 | 5563 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       135 | 5564 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 5565 | `			rc = SXERR_MEM;` |
|       ! 0 | 5566 | `			break;` |
|         - | 5567 | `		}` |
|       135 | 5568 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       135 | 5569 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       135 | 5570 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       135 | 5571 | `		if( rc == SXRET_OK ){` |
|       131 | 5572 | `			if( nName == sizeof("error_reporting")-1` |
|        94 | 5573 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         8 | 5574 | `				sxi64 iLevel = 0;` |
|         8 | 5575 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         8 | 5576 | `				pVm->bErrReport = iLevel != 0;` |
|         - | 5577 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       127 | 5578 | `			}else if( nName == sizeof("memory_limit")-1` |
|        75 | 5579 | `			 && SyMemcmp(zName,"memory_limit",nName) == 0 ){` |
|         - | 5580 | ``				/* Arm the allocator ceiling now: `-d memory_limit=32M` has to hold`` |
|         - | 5581 | `				 * for the whole run, and the INI chunk that would otherwise carry it` |
|         - | 5582 | `				 * is seeded lazily -- by which time a runaway script has already` |
|         - | 5583 | `				 * taken the box.` |
|         - | 5584 | `				 *` |
|         - | 5585 | `				 * Guarded because the applier lives in vm_builtin_ini.c, which the` |
|         - | 5586 | `				 * tiny build compiles away wholesale: an unguarded call here links` |
|         - | 5587 | ``				 * fine in `full` and fails ONLY in tiny, which is the one build the`` |
|         - | 5588 | `				 * ASan and Windows gates do not cover. */` |
|         4 | 5589 | `				PH7_VmApplyMemoryLimit(&(*pVm),zValue,nValue);` |
|         - | 5590 | `#endif` |
|       121 | 5591 | `			}else if( nName == sizeof("date.timezone")-1` |
|        68 | 5592 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|         8 | 5593 | `			 && nValue == 3` |
|         4 | 5594 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 5595 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 5596 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 5597 | `				pVm->nDefTz = 3;` |
|       119 | 5598 | `			}else if( nName == sizeof("zend.assertions")-1` |
|       102 | 5599 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 5600 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 5601 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 5602 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 5603 | `				sxi64 iZend = 0;` |
|        40 | 5604 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 5605 | `				if( iZend >= 1 ){` |
|        40 | 5606 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 5607 | `				}else{` |
|       ! 0 | 5608 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 5609 | `				}` |
|       105 | 5610 | `			}else if( nName == sizeof("display_errors")-1` |
|        54 | 5611 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 5612 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 5613 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 5614 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 5615 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        78 | 5616 | `			}else if( nName == sizeof("log_errors")-1` |
|        44 | 5617 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 5618 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|        61 | 5619 | `			}else if( nName == sizeof("include_path")-1` |
|        32 | 5620 | `			 && SyMemcmp(zName,"include_path",nName) == 0` |
|        16 | 5621 | `			 && nValue > 0 ){` |
|         - | 5622 | `				/* The path SET is the store this directive names, and the INI` |
|         - | 5623 | ``				 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|         - | 5624 | `				 * here or a script that never touches the INI API keeps looking` |
|         - | 5625 | `				 * in the default directory. Empty is refused, as php's` |
|         - | 5626 | `				 * OnUpdateStringUnempty refuses it. */` |
|         9 | 5627 | `				PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|         4 | 5628 | `			}` |
|        65 | 5629 | `		}` |
|       135 | 5630 | `		break;` |
|         - | 5631 | `								  }` |
|       ! 0 | 5632 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 5633 | `		/* error_log() consumer */` |
|       ! 0 | 5634 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 5635 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 5636 | `		break;` |
|         - | 5637 | `										}` |
|       ! 0 | 5638 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 5639 | `		/* Script return value */` |
|       ! 0 | 5640 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 5641 | `#ifdef UNTRUST` |
|         - | 5642 | `		if( ppValue == 0 ){` |
|         - | 5643 | `			rc = SXERR_CORRUPT;` |
|         - | 5644 | `			break;` |
|         - | 5645 | `		}` |
|         - | 5646 | `#endif` |
|       ! 0 | 5647 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 5648 | `		break;` |
|         - | 5649 | `								   }` |
|     25343 | 5650 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 5651 | `		/* Register an IO stream device */` |
|     50610 | 5652 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 5653 | `		/* Make sure we are dealing with a valid IO stream. A wrapper has to be` |
|         - | 5654 | `		 * able to do ONE of the two things a wrapper does -- open a byte stream` |
|         - | 5655 | `		 * or open a directory. php's glob:// is a dir_opener and nothing else,` |
|         - | 5656 | ``		 * and demanding xOpen here would leave `opendir('glob://…')` with no`` |
|         - | 5657 | `		 * device to reach. */` |
|     53415 | 5658 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     50605 | 5659 | `			((pStream->xOpen == 0 \|\| pStream->xRead == 0) && pStream->xOpenDir == 0) ){` |
|         - | 5660 | `				/* Invalid stream */` |
|       ! 0 | 5661 | `				rc = SXERR_INVALID;` |
|       ! 0 | 5662 | `				break;` |
|         - | 5663 | `		}` |
|     50610 | 5664 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 5665 | `			/* Make the 'file://' stream the defaut stream device */` |
|      5624 | 5666 | `			pVm->pDefStream = pStream;` |
|      2805 | 5667 | `		}` |
|         - | 5668 | `		/* Insert in the appropriate container */` |
|     50610 | 5669 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     50610 | 5670 | `		break;` |
|         - | 5671 | `								  }` |
|        23 | 5672 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 5673 | `		/* Point to the VM internal output consumer buffer */` |
|        46 | 5674 | `		const void **ppOut = va_arg(ap,const void **);` |
|        46 | 5675 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 5676 | `#ifdef UNTRUST` |
|         - | 5677 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 5678 | `			rc = SXERR_CORRUPT;` |
|         - | 5679 | `			break;` |
|         - | 5680 | `		}` |
|         - | 5681 | `#endif` |
|        46 | 5682 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        46 | 5683 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        46 | 5684 | `		break;` |
|         - | 5685 | `									   }` |
|        23 | 5686 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 5687 | `		/* Raw HTTP request*/` |
|        46 | 5688 | `		const char *zRequest = va_arg(ap,const char *);` |
|        46 | 5689 | `		int nByte = va_arg(ap,int);` |
|        46 | 5690 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 5691 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 5692 | `			break;` |
|         - | 5693 | `		}` |
|        46 | 5694 | `		if( nByte < 0 ){` |
|         - | 5695 | `			/* Compute length automatically */` |
|       ! 0 | 5696 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 5697 | `		}` |
|         - | 5698 | `		/* Process the request */` |
|        46 | 5699 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 5700 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        46 | 5701 | `		if( rc == SXRET_OK ){` |
|        44 | 5702 | `			pVm->bHttpContext = 1;` |
|        44 | 5703 | `			if( pVm->iResponseStatus == 0 ){` |
|         - | 5704 | `				/* A request-driven run starts at 200, which is what` |
|         - | 5705 | `				 * http_response_code() reads back before anything sets one. */` |
|        44 | 5706 | `				pVm->iResponseStatus = 200;` |
|        22 | 5707 | `			}` |
|        22 | 5708 | `		}` |
|        46 | 5709 | `		break;` |
|         - | 5710 | `									}` |
|        23 | 5711 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 5712 | `		/* Extract HTTP response status code */` |
|        46 | 5713 | `		int *pStatus = va_arg(ap, int *);` |
|        46 | 5714 | `		if( pStatus ){` |
|         - | 5715 | `			/* A response nothing set a code for goes out as 200. */` |
|        46 | 5716 | `			*pStatus = pVm->iResponseStatus ? pVm->iResponseStatus : 200;` |
|        23 | 5717 | `		}` |
|        46 | 5718 | `		break;` |
|         - | 5719 | `										}` |
|        23 | 5720 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 5721 | `		/* Iterate response headers via callback */` |
|         - | 5722 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        46 | 5723 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        46 | 5724 | `		void *pUserData = va_arg(ap, void *);` |
|        46 | 5725 | `		if( xCallback ){` |
|        46 | 5726 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        46 | 5727 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       112 | 5728 | `			for( k = 0; k < nHdr; k++ ){` |
|        99 | 5729 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        66 | 5730 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        33 | 5731 | `							   pUserData);` |
|        66 | 5732 | `				if( rc != PH7_OK ){` |
|       ! 0 | 5733 | `					break;` |
|         - | 5734 | `				}` |
|        33 | 5735 | `			}` |
|        23 | 5736 | `		}` |
|        46 | 5737 | `		break;` |
|         - | 5738 | `										 }` |
|       ! 0 | 5739 | `	default:` |
|         - | 5740 | `		/* Unknown configuration option */` |
|       ! 0 | 5741 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 5742 | `		break;` |
|         - | 5743 | `	}` |
|    179972 | 5744 | `	return rc;` |
|         5 | 5745 | `}` |
|         - | 5746 | `/* Forward declaration */` |
|         - | 5747 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 5748 | `/*` |
|         - | 5749 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 5750 | ` * format.` |
|         - | 5751 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 5752 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 5753 | ` * (STDOUT).` |
|         - | 5754 | ` */` |
|         2 | 5755 | `static sxi32 VmByteCodeDump(` |
|         - | 5756 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 5757 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 5758 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5759 | `	)` |
|         1 | 5760 | `{` |
|         - | 5761 | `	static const char zDump[] = {` |
|         - | 5762 | `		"====================================================\n"` |
|         - | 5763 | `		"PH7 VM Dump\n"` |
|         - | 5764 | `		"====================================================\n"` |
|         - | 5765 | `	};` |
|         - | 5766 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 5767 | `	sxi32 rc = SXRET_OK;` |
|         - | 5768 | `	sxu32 n;` |
|         - | 5769 | `	/* Point to the PH7 instructions */` |
|         3 | 5770 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 5771 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 5772 | `	n = 0;` |
|         3 | 5773 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 5774 | `	/* Dump instructions */` |
|         6 | 5775 | `	for(;;){` |
|        13 | 5776 | `		if( pInstr >= pEnd ){` |
|         - | 5777 | `			/* No more instructions */` |
|         3 | 5778 | `			break;` |
|         - | 5779 | `		}` |
|         - | 5780 | `		/* Format and call the consumer callback */` |
|        16 | 5781 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 5782 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 5783 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 5784 | `		if( rc != SXRET_OK ){` |
|         - | 5785 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 5786 | `			return rc;` |
|         - | 5787 | `		}` |
|        11 | 5788 | `		++n;` |
|        11 | 5789 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 5790 | `	}` |
|         3 | 5791 | `	return rc;` |
|         2 | 5792 | `}` |
|         - | 5793 | `/*` |
|         - | 5794 | ` * Save the execution state of a fiber/generator context.` |
|         - | 5795 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 5796 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 5797 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 5798 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 5799 | ` * when VmByteCodeExec returns.` |
|         - | 5800 | ` */` |
|      2002 | 5801 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 5802 | `	ph7_vm *pVm,` |
|         - | 5803 | `	ph7_exec_ctx *pCtx,` |
|         - | 5804 | `	sxi32 pc,` |
|         - | 5805 | `	sxi32 nTos` |
|         - | 5806 | `	)` |
|         5 | 5807 | `{` |
|      1001 | 5808 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      2007 | 5809 | `	pCtx->pc = pc;` |
|      2007 | 5810 | `	pCtx->nTos = nTos;` |
|      2007 | 5811 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      2007 | 5812 | `	return PH7_SUSPEND;` |
|         5 | 5813 | `}` |
|         - | 5814 | `/*` |
|         - | 5815 | ` * Resolve named-argument mapping.` |
|         - | 5816 | ` *` |
|         - | 5817 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 5818 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 5819 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 5820 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 5821 | ` * every formal parameter that received a value.` |
|         - | 5822 | ` *` |
|         - | 5823 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 5824 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 5825 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 5826 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 5827 | ` */` |
|       486 | 5828 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 5829 | `	ph7_vm *pVm,` |
|         - | 5830 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 5831 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 5832 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 5833 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 5834 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 5835 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 5836 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 5837 | `)` |
|         5 | 5838 | `{` |
|       491 | 5839 | `	sxi32 posIdx = 0;` |
|         - | 5840 | `	sxu32 i;` |
|       491 | 5841 | `	int bSeenNamed = 0;` |
|         - | 5842 | `	char zErrMsg[256];` |
|       491 | 5843 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1593 | 5844 | `	for( i = 0; i < nActual; i++ ){` |
|      1107 | 5845 | `		aSlot[i] = -2;` |
|       556 | 5846 | `	}` |
|      1569 | 5847 | `	for( i = 0; i < nActual; i++ ){` |
|      1461 | 5848 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 5849 | `			/* Named argument — find formal by name */` |
|       733 | 5850 | `			int found = 0;` |
|       733 | 5851 | `			bSeenNamed = 1;` |
|         - | 5852 | `			sxu32 k;` |
|      1139 | 5853 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       932 | 5854 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       911 | 5855 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       880 | 5856 | `						pMap->aNames[i].zString,` |
|      1320 | 5857 | `						pMap->aNames[i].nByte) == 0 ){` |
|       531 | 5858 | `					if( aUsed[k] ){` |
|        19 | 5859 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 5860 | `							"Named parameter $%.*s overwrites previous argument",` |
|        10 | 5861 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        14 | 5862 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 5863 | `					}` |
|       521 | 5864 | `					aSlot[i] = (sxi32)k;` |
|       521 | 5865 | `					aUsed[k] = 1;` |
|       521 | 5866 | `					found = 1;` |
|       521 | 5867 | `					break;` |
|         - | 5868 | `				}` |
|       208 | 5869 | `			}` |
|       723 | 5870 | `			if( !found ){` |
|       207 | 5871 | `				if( iVariadicIdx >= 0 ){` |
|       196 | 5872 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|       100 | 5873 | `				}else{` |
|        18 | 5874 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 5875 | `						"Unknown named parameter $%.*s",` |
|        10 | 5876 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|        13 | 5877 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 5878 | `				}` |
|        96 | 5879 | `			}` |
|       359 | 5880 | `		}else{` |
|         - | 5881 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 5882 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 5883 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 5884 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       379 | 5885 | `			if( bSeenNamed ){` |
|         - | 5886 | `				/* php has two sentences for the one rule: the argument list an` |
|         - | 5887 | ``				 * UNPACK produced ends with ` during unpacking`, and the one`` |
|         - | 5888 | `				 * call_user_func_array() rebuilt from an array does not. */` |
|         5 | 5889 | `				if( pMap->bFromUnpack ){` |
|         3 | 5890 | `					return VmThrowNamedArgError(&(*pVm),` |
|         - | 5891 | `						"Cannot use positional argument after named argument during unpacking",` |
|         - | 5892 | `						sizeof("Cannot use positional argument after named argument during unpacking") - 1);` |
|         - | 5893 | `				}` |
|         3 | 5894 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 5895 | `					"Cannot use positional argument after named argument",` |
|         - | 5896 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 5897 | `			}` |
|       375 | 5898 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        69 | 5899 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 5900 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 5901 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 5902 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 5903 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 5904 | `				}` |
|        69 | 5905 | `				aSlot[i] = posIdx;` |
|        69 | 5906 | `				aUsed[posIdx] = 1;` |
|       341 | 5907 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 5908 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 5909 | `			}` |
|       375 | 5910 | `			posIdx++;` |
|         - | 5911 | `		}` |
|       544 | 5912 | `	}` |
|       467 | 5913 | `	return SXRET_OK;` |
|       248 | 5914 | `}` |
|         - | 5915 | `/*` |
|         - | 5916 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 5917 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 5918 | ` */` |
|      1453 | 5919 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         5 | 5920 | `{` |
|      1458 | 5921 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|      1358 | 5922 | `		return 0;` |
|         - | 5923 | `	}` |
|       104 | 5924 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       684 | 5925 | `}` |
|         - | 5926 | `/*` |
|         - | 5927 | ` * Shared body of the two Traversable-spread steps. An ARRAY source can only hand` |
|         - | 5928 | ` * over the two key types an array holds, but an ITERATOR may answer key() with` |
|         - | 5929 | `` * anything at all, so php screens it: `Keys must be of type int\|string during`` |
|         - | 5930 | `` * {array,argument} unpacking` is an Error raised at the offending element, and it`` |
|         - | 5931 | ` * refuses a float (a WHOLE one included), a bool, a null and a resource as well as` |
|         - | 5932 | ` * the two containers — this is not the offset rule set, which folds all four.` |
|         - | 5933 | ` *` |
|         - | 5934 | ` * What survives the screen follows php's ordinary 8.1 unpack rules, which are the` |
|         - | 5935 | ` * ones an array source already gets: a key that stays a STRING is kept, a key that` |
|         - | 5936 | ` * FOLDS to an integer — a canonical numeric string like "7" among them — is` |
|         - | 5937 | ` * renumbered. PH7_HashmapKeyIsInt answers that fold, and asking it is what keeps` |
|         - | 5938 | `` * `yield "7" => v` off the integer key 7 the raw insert would have written.`` |
|         - | 5939 | ` *` |
|         - | 5940 | ` * On the ARGUMENT path the kept string key is what makes the element a NAMED` |
|         - | 5941 | ` * argument: VmSpreadCaptureRun reads the temp map's node keys, so binding, the` |
|         - | 5942 | ` * unknown-name Error and the duplicate-name Error all come for free. Before this,` |
|         - | 5943 | `` * both steps threw the key away — `[...$gen]` silently renumbered a key php`` |
|         - | 5944 | `` * refuses, and `f(...$gen)` passed a named argument positionally, which is a`` |
|         - | 5945 | ` * DIFFERENT parameter's value with no diagnostic at all.` |
|         - | 5946 | ` */` |
|       104 | 5947 | `static sxi32 VmSpreadKeyedStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue,` |
|         - | 5948 | `                               ph7_hashmap *pMap, int bArgs)` |
|         3 | 5949 | `{` |
|         - | 5950 | `	SyBlob sMsg;` |
|         - | 5951 | `	sxi32 rc;` |
|         - | 5952 | `	int bKeep;` |
|       104 | 5953 | `	if( (pKey->iFlags & (MEMOBJ_STRING\|MEMOBJ_INT)) == 0` |
|        93 | 5954 | `	 \|\| (pKey->iFlags & (MEMOBJ_REAL\|MEMOBJ_BOOL\|MEMOBJ_NULL\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) ){` |
|        34 | 5955 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        34 | 5956 | `		SyBlobFormat(&sMsg,"Keys must be of type int\|string during %s unpacking",` |
|        16 | 5957 | `			bArgs ? "argument" : "array");` |
|        34 | 5958 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|        34 | 5959 | `		SyBlobRelease(&sMsg);` |
|        34 | 5960 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - | 5961 | `	}` |
|         - | 5962 | `	/* PH7_HashmapKeyIsInt may cast pKey to a string to answer; pKey is the walk's` |
|         - | 5963 | `	 * own temporary, released the moment this step returns. */` |
|        75 | 5964 | `	bKeep = (pKey->iFlags & MEMOBJ_STRING) && !PH7_HashmapKeyIsInt(pKey);` |
|        75 | 5965 | `	if( bKeep && bArgs && PH7_HashmapLookup(pMap,pKey,0) == SXRET_OK ){` |
|         - | 5966 | `		/* Two elements under the same string key are two NAMED arguments with the` |
|         - | 5967 | `		 * same name, which php refuses. The array path lets the later one win (that` |
|         - | 5968 | `		 * IS php's array-unpack rule), but here the collision would silently drop an` |
|         - | 5969 | `		 * argument: the temp map keeps one element, so the callee would be handed a` |
|         - | 5970 | `		 * shorter list with no diagnostic. The message is the binder's own — a` |
|         - | 5971 | `		 * duplicate spread ACROSS two sources still reaches it there. */` |
|         6 | 5972 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|         6 | 5973 | `		SyBlobFormat(&sMsg,"Named parameter $%.*s overwrites previous argument",` |
|         4 | 5974 | `			(int)SyBlobLength(&pKey->sBlob),(const char *)SyBlobData(&pKey->sBlob));` |
|         6 | 5975 | `		rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));` |
|         6 | 5976 | `		SyBlobRelease(&sMsg);` |
|         6 | 5977 | `		return (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;` |
|         - | 5978 | `	}` |
|        71 | 5979 | `	PH7_HashmapInsert(pMap, bKeep ? pKey : 0 /* auto-index */, pValue);` |
|        71 | 5980 | `	return SXRET_OK;` |
|        55 | 5981 | `}` |
|         - | 5982 | `/*` |
|         - | 5983 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 5984 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 5985 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 5986 | ` */` |
|        46 | 5987 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         2 | 5988 | `{` |
|        48 | 5989 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,0);` |
|         2 | 5990 | `}` |
|         - | 5991 | `/*` |
|         - | 5992 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 5993 | ` * collect the elements into a temp array, keeping a string key so the CALL` |
|         - | 5994 | ` * replays it as a named argument.` |
|         - | 5995 | ` */` |
|        58 | 5996 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         3 | 5997 | `{` |
|        61 | 5998 | `	return VmSpreadKeyedStep(pVm,pKey,pValue,(ph7_hashmap *)pUserData,1);` |
|         3 | 5999 | `}` |
|         - | 6000 | `/*` |
|         - | 6001 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 6002 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 6003 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 6004 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 6005 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 6006 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 6007 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 6008 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 6009 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 6010 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 6011 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 6012 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 6013 | ` */` |
|         - | 6014 | `/*` |
|         - | 6015 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 6016 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 6017 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 6018 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 6019 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 6020 | ` */` |
|      1305 | 6021 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         5 | 6022 | `{` |
|         - | 6023 | `	VmSpreadRun sRun;` |
|         - | 6024 | `	ph7_hashmap_node *pNode;` |
|         - | 6025 | `	sxu32 i;` |
|      1310 | 6026 | `	sRun.pStart = pFirst;` |
|      1310 | 6027 | `	sRun.nCount = nCount;` |
|      1310 | 6028 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|      1310 | 6029 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|      1310 | 6030 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 6031 | `		return;` |
|         - | 6032 | `	}` |
|      1310 | 6033 | `	pNode = pMap->pFirst;` |
|      4443 | 6034 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 6035 | `		VmSpreadKey sKey;` |
|      3138 | 6036 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 6037 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 6038 | `			 * the source map's release before CALL replays them. */` |
|       129 | 6039 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       129 | 6040 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       129 | 6041 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        66 | 6042 | `		}else{` |
|         - | 6043 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      3012 | 6044 | `			sKey.nOff = 0;` |
|      3012 | 6045 | `			sKey.nLen = 0;` |
|         - | 6046 | `		}` |
|      3138 | 6047 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      3138 | 6048 | `		pNode = pNode->pPrev; /* forward link */` |
|      1459 | 6049 | `	}` |
|       610 | 6050 | `}` |
|         - | 6051 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 6052 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        16 | 6053 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 6054 | `{` |
|        16 | 6055 | `	SySetReset(&pVm->aSpreadRun);` |
|        16 | 6056 | `	SySetReset(&pVm->aSpreadKey);` |
|        16 | 6057 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        16 | 6058 | `}` |
|         - | 6059 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 6060 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 6061 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 6062 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 6063 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 6064 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 6065 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 6066 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 6067 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 6068 | ` * slot — from being consumed by that nested call. */` |
|      2456 | 6069 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         5 | 6070 | `{` |
|      2461 | 6071 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      2461 | 6072 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 6073 | `	VmSpreadRun *aRun;` |
|      2461 | 6074 | `	if( rStart >= nRun ){` |
|      1174 | 6075 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 6076 | `	}` |
|      1292 | 6077 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1292 | 6078 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|      1292 | 6079 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|      1292 | 6080 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       596 | 6081 | `	}` |
|      1292 | 6082 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|      1138 | 6083 | `}` |
|         - | 6084 | `/*` |
|         - | 6085 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 6086 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 6087 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 6088 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 6089 | ` *` |
|         - | 6090 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 6091 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 6092 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 6093 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 6094 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 6095 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 6096 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 6097 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 6098 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 6099 | ` *` |
|         - | 6100 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 6101 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 6102 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 6103 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 6104 | ` */` |
|      3056 | 6105 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         5 | 6106 | `{` |
|      3061 | 6107 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 6108 | `	VmSpreadRun *aRun;` |
|      3061 | 6109 | `	ph7_value *pEnd = pTos;` |
|      3061 | 6110 | `	sxi32 nPos = iP1;` |
|      3061 | 6111 | `	sxi32 ri, extra = 0;` |
|      3061 | 6112 | `	if( nRun == 0 ){` |
|        17 | 6113 | `		pVm->nSpreadCallBase = 0;` |
|        17 | 6114 | `		return 0;` |
|         - | 6115 | `	}` |
|      3045 | 6116 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      3045 | 6117 | `	ri = (sxi32)nRun - 1;` |
|      9205 | 6118 | `	while( nPos > 0 ){` |
|      6165 | 6119 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 6120 | `			/* A non-empty unpack occupying nCount slots. */` |
|      2541 | 6121 | `			pEnd = aRun[ri].pStart;` |
|      2541 | 6122 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      2541 | 6123 | `			ri--;` |
|      4801 | 6124 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 6125 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       515 | 6126 | `			extra -= 1;` |
|       515 | 6127 | `			ri--;` |
|       259 | 6128 | `		}else{` |
|         - | 6129 | `			/* An ordinary single-slot argument. */` |
|      3116 | 6130 | `			pEnd--;` |
|         - | 6131 | `		}` |
|      6165 | 6132 | `		nPos--;` |
|         5 | 6133 | `	}` |
|         - | 6134 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 6135 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      3045 | 6136 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      3045 | 6137 | `	return extra;` |
|      1438 | 6138 | `}` |
|      1305 | 6139 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         5 | 6140 | `{` |
|      1310 | 6141 | `	ph7_value *pTos = *ppTos;` |
|      1310 | 6142 | `	sxu32 nEntry = pMap->nEntry;` |
|      1310 | 6143 | `	if( nEntry == 0 ){` |
|         - | 6144 | `		/* Nothing to unpack — remove the source from the stack */` |
|       215 | 6145 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|       215 | 6146 | `		VmPopOperand(&pTos, 1);` |
|       109 | 6147 | `	}else{` |
|         - | 6148 | `		ph7_hashmap_node *pNode;` |
|         - | 6149 | `		ph7_value *pElem;` |
|         - | 6150 | `		sxu32 i;` |
|         - | 6151 | `		int bTemp;` |
|         - | 6152 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - | 6153 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - | 6154 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - | 6155 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - | 6156 | `		 * ordinary spread pays nothing for this. */` |
|      1093 | 6157 | `		if( bVarSource` |
|       997 | 6158 | `		 && pMap != pVm->pGlobal` |
|       901 | 6159 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|       906 | 6160 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - | 6161 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - | 6162 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - | 6163 | `			 * object. */` |
|       905 | 6164 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|       905 | 6165 | `			if( pSep ){` |
|       905 | 6166 | `				pMap = pSep;` |
|       905 | 6167 | `				nEntry = pMap->nEntry;` |
|       403 | 6168 | `			}` |
|       403 | 6169 | `		}` |
|      1098 | 6170 | `		pMap->iRef++;` |
|      1098 | 6171 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 6172 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 6173 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|      1098 | 6174 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 6175 | `		/* Overwrite the source slot with the first element */` |
|      1098 | 6176 | `		pNode = pMap->pFirst;` |
|      1098 | 6177 | `		pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1098 | 6178 | `		PH7_MemObjRelease(pTos);` |
|      1098 | 6179 | `		if( pElem ){` |
|      1098 | 6180 | `			if( bTemp ){` |
|       187 | 6181 | `				PH7_MemObjStore(pElem, pTos);` |
|        95 | 6182 | `			}else{` |
|       913 | 6183 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 6184 | `			}` |
|       499 | 6185 | `		}` |
|         - | 6186 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - | 6187 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - | 6188 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - | 6189 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - | 6190 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - | 6191 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - | 6192 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - | 6193 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|      1098 | 6194 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|      1098 | 6195 | `		if( !bVarSource \|\| bTemp ){` |
|       195 | 6196 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|        96 | 6197 | `		}` |
|         - | 6198 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 6199 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|      1098 | 6200 | `		pNode = pNode->pPrev;` |
|         - | 6201 | `		/* Push the remaining elements */` |
|      3138 | 6202 | `		for( i = 1; i < nEntry; i++ ){` |
|      2045 | 6203 | `			pTos++;` |
|      2045 | 6204 | `			PH7_MemObjInit(pVm, pTos);` |
|      2045 | 6205 | `			pElem = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pNode->nValIdx);` |
|      2045 | 6206 | `			if( pElem ){` |
|      2045 | 6207 | `				if( bTemp ){` |
|      1299 | 6208 | `					PH7_MemObjStore(pElem, pTos);` |
|       651 | 6209 | `				}else{` |
|       748 | 6210 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 6211 | `				}` |
|       955 | 6212 | `			}` |
|      2045 | 6213 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      2045 | 6214 | `			if( !bVarSource \|\| bTemp ){` |
|      1299 | 6215 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       648 | 6216 | `			}` |
|      2045 | 6217 | `			pNode = pNode->pPrev;` |
|       960 | 6218 | `		}` |
|      1098 | 6219 | `		PH7_HashmapUnref(pMap);` |
|         - | 6220 | `	}` |
|      1310 | 6221 | `	*ppTos = pTos;` |
|      1310 | 6222 | `}` |
|         - | 6223 | `/*` |
|         - | 6224 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 6225 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 6226 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 6227 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 6228 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 6229 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 6230 | ` *` |
|         - | 6231 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 6232 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 6233 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 6234 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 6235 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 6236 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 6237 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 6238 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 6239 | ` * method-name slot pop shifts pArg).` |
|         - | 6240 | ` *` |
|         - | 6241 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 6242 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 6243 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 6244 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 6245 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 6246 | ` */` |
|      1253 | 6247 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 6248 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         5 | 6249 | `{` |
|      1258 | 6250 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 6251 | `	VmSpreadRun *aRun;` |
|         - | 6252 | `	VmSpreadKey *aKey;` |
|         - | 6253 | `	const char *zKeyBase;` |
|      1258 | 6254 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|      1258 | 6255 | `	int bAnyNamed = 0;` |
|         - | 6256 | `	sxu32 ai, ci, ri, rStart;` |
|      1258 | 6257 | `	if( nRun == 0 ){` |
|         - | 6258 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 6259 | `		return 0;` |
|         - | 6260 | `	}` |
|      1258 | 6261 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1258 | 6262 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|      1258 | 6263 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 6264 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 6265 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 6266 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 6267 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|      1258 | 6268 | `	ri = pVm->nSpreadCallBase;` |
|      1258 | 6269 | `	rStart = ri;` |
|      1258 | 6270 | `	if( rStart >= nRun ){` |
|         - | 6271 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 6272 | `		return 0;` |
|         - | 6273 | `	}` |
|      1258 | 6274 | `	SySetReset(&pVm->aEffArgName);` |
|      1258 | 6275 | `	ci = 0;` |
|      1258 | 6276 | `	ai = 0;` |
|      3558 | 6277 | `	while( ai < nActual ){` |
|         - | 6278 | `		SyString sName;` |
|      2305 | 6279 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 6280 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 6281 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|      2317 | 6282 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 6283 | `			ci++; ri++;` |
|         1 | 6284 | `		}` |
|      2305 | 6285 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 6286 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 6287 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 6288 | `			 * run never desyncs the key stream. */` |
|      1098 | 6289 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      4231 | 6290 | `			for( j = 0; j < K; j++ ){` |
|      3138 | 6291 | `				SyZero(&sName, sizeof(sName));` |
|      3138 | 6292 | `				if( aKey[ks + j].nLen > 0 ){` |
|       129 | 6293 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       129 | 6294 | `					bAnyNamed = 1;` |
|        63 | 6295 | `				}` |
|      3138 | 6296 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1459 | 6297 | `			}` |
|      1098 | 6298 | `			ai += K;` |
|      1098 | 6299 | `			ci++; ri++;` |
|       504 | 6300 | `		}else{` |
|         - | 6301 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|      1211 | 6302 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        38 | 6303 | `				sName = pCompile->aNames[ci];` |
|        38 | 6304 | `				bAnyNamed = 1;` |
|        18 | 6305 | `			}` |
|      1211 | 6306 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1211 | 6307 | `			ai++;` |
|      1211 | 6308 | `			ci++;` |
|         - | 6309 | `		}` |
|         5 | 6310 | `	}` |
|         - | 6311 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 6312 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 6313 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 6314 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|      1258 | 6315 | `	VmSpreadConsume(pVm);` |
|      1258 | 6316 | `	if( !bAnyNamed ){` |
|         - | 6317 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|      1158 | 6318 | `		return 0;` |
|         - | 6319 | `	}` |
|       103 | 6320 | `	pEff->bHasNamed = 1;` |
|       103 | 6321 | ``	pEff->bFromUnpack = 1; /* picks php's ` during unpacking` refusal wording */`` |
|       103 | 6322 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|       103 | 6323 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|       103 | 6324 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - | 6325 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - | 6326 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - | 6327 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - | 6328 | `	 * is the caller's stack local. */` |
|       103 | 6329 | `	pEff->bArgShapes = 0;` |
|       103 | 6330 | `	pEff->nNonLvalMask = 0;` |
|       103 | 6331 | `	pEff->nTempCallMask = 0;` |
|       103 | 6332 | `	if( pCompile ){` |
|        42 | 6333 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        22 | 6334 | `	}else{` |
|        63 | 6335 | `		pEff->sAssertSrc.zString = 0;` |
|        63 | 6336 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 6337 | `	}` |
|       103 | 6338 | `	pEff->nTotal = nActual;` |
|       103 | 6339 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|       103 | 6340 | `	return 1;` |
|       584 | 6341 | `}` |
|         - | 6342 | `/*` |
|         - | 6343 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 6344 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 6345 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 6346 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 6347 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 6348 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 6349 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 6350 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 6351 | ` * pArg must be the site's FINAL argument base.` |
|         - | 6352 | ` */` |
|   8695723 | 6353 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 6354 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 6355 | `{` |
|   8695728 | 6356 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   8695728 | 6357 | `	if( pInstr->iP2 == 0 ){` |
|   8694425 | 6358 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 6359 | `	}` |
|      1308 | 6360 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|       103 | 6361 | `		return pStorage;` |
|         - | 6362 | `	}` |
|      1208 | 6363 | `	VmSpreadConsume(pVm);` |
|      1208 | 6364 | `	return pCompile;` |
|   4347612 | 6365 | `}` |
|         - | 6366 | `/*` |
|         - | 6367 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 6368 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 6369 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 6370 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 6371 | ` */` |
|        38 | 6372 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         3 | 6373 | `{` |
|        41 | 6374 | `	const char *zType = "unknown";` |
|         - | 6375 | `	char zMsg[64];` |
|        41 | 6376 | `	if( iFlags & MEMOBJ_STRING ){` |
|        11 | 6377 | `		zType = "string";` |
|        37 | 6378 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 6379 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 6380 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 6381 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 6382 | `		zType = "float";` |
|        33 | 6383 | `	}else if( iFlags & MEMOBJ_INT ){` |
|        31 | 6384 | `		zType = "int";` |
|        17 | 6385 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 6386 | `		zType = "bool";` |
|         1 | 6387 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 6388 | `		zType = "object";` |
|       ! 0 | 6389 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 6390 | `		zType = "resource";` |
|       ! 0 | 6391 | `	}` |
|        41 | 6392 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        41 | 6393 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        41 | 6394 | `}` |
|         - | 6395 | `/*` |
|         - | 6396 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 6397 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 6398 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 6399 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 6400 | ` */` |
|    104828 | 6401 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 6402 | `{` |
|    104833 | 6403 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 6404 | `}` |
|         - | 6405 | `/*` |
|         - | 6406 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 6407 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 6408 | ` * isset() stops at the truth.` |
|         - | 6409 | ` */` |
|        56 | 6410 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 6411 | `{` |
|        59 | 6412 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 6413 | `}` |
|         - | 6414 | `/*` |
|         - | 6415 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 6416 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 6417 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 6418 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 6419 | ` * reads (other names / other instances) still dispatch.` |
|         - | 6420 | ` */` |
|      1364 | 6421 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         4 | 6422 | `{` |
|         - | 6423 | `	VmMagicGuard *aG;` |
|         - | 6424 | `	sxu32 nHash;` |
|         - | 6425 | `	sxu32 n;` |
|      1368 | 6426 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 6427 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 6428 | `		 * every hooked-property access consults the guard, often twice. */` |
|      1180 | 6429 | `		return FALSE;` |
|         - | 6430 | `	}` |
|       191 | 6431 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       191 | 6432 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       235 | 6433 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       191 | 6434 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       147 | 6435 | `			return TRUE;` |
|         - | 6436 | `		}` |
|        23 | 6437 | `	}` |
|        45 | 6438 | `	return FALSE;` |
|       686 | 6439 | `}` |
|       580 | 6440 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         4 | 6441 | `{` |
|         - | 6442 | `	VmMagicGuard sG;` |
|       584 | 6443 | `	sG.pThis = pThis;` |
|       584 | 6444 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       584 | 6445 | `	sG.cKind = cKind;` |
|       584 | 6446 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|       584 | 6447 | `}` |
|       580 | 6448 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         4 | 6449 | `{` |
|       584 | 6450 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|       584 | 6451 | `}` |
|         - | 6452 | `/*` |
|         - | 6453 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 6454 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 6455 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 6456 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 6457 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 6458 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 6459 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 6460 | ` * One-token lookahead only.` |
|         - | 6461 | ` */` |
|     13327 | 6462 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 6463 | `{` |
|     13332 | 6464 | `	switch( pNext->iOp ){` |
|       596 | 6465 | `		case PH7_OP_STORE:` |
|      1197 | 6466 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|        30 | 6467 | `		case PH7_OP_STORE_REF:` |
|        62 | 6468 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|      1280 | 6469 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 6470 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 6471 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 6472 | `		case PH7_OP_CAT_STORE:` |
|         - | 6473 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 6474 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|      2565 | 6475 | `			return 1;` |
|      4757 | 6476 | `		default:` |
|      9520 | 6477 | `			return 0;` |
|         - | 6478 | `	}` |
|      6669 | 6479 | `}` |
|         - | 6480 | `/*` |
|         - | 6481 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - | 6482 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - | 6483 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - | 6484 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - | 6485 | ` * treat them as one.` |
|         - | 6486 | ` */` |
|      6981 | 6487 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 | 6488 | `{` |
|     10442 | 6489 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|     10455 | 6490 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 | 6491 | `}` |
|         - | 6492 | `/*` |
|         - | 6493 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - | 6494 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - | 6495 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - | 6496 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - | 6497 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - | 6498 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - | 6499 | ` */` |
|      7231 | 6500 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 | 6501 | `{` |
|      7236 | 6502 | `	switch( pNext->iOp ){` |
|        65 | 6503 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 6504 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 6505 | `		case PH7_OP_CAT_STORE:` |
|         - | 6506 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 6507 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       132 | 6508 | `			return 1;` |
|      3550 | 6509 | `		default:` |
|      7106 | 6510 | `			return 0;` |
|         - | 6511 | `	}` |
|      3621 | 6512 | `}` |
|         - | 6513 | `/*` |
|         - | 6514 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 6515 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 6516 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 6517 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 6518 | ` */` |
|       574 | 6519 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         4 | 6520 | `{` |
|       578 | 6521 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         4 | 6522 | `}` |
|         - | 6523 | `/*` |
|         - | 6524 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 6525 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 6526 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 6527 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 6528 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 6529 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 6530 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 6531 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 6532 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 6533 | ` * abort path; SXRET_OK otherwise.` |
|         - | 6534 | ` */` |
|        76 | 6535 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         3 | 6536 | `{` |
|         - | 6537 | `	char zHName[384];` |
|         - | 6538 | `	sxu32 nHName;` |
|         - | 6539 | `	ph7_class_method *pSetHook;` |
|        79 | 6540 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 6541 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 6542 | `		SyBlob sErrMsg;` |
|         5 | 6543 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 6544 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 6545 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 6546 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 6547 | `		return SXRET_OK;` |
|         - | 6548 | `	}` |
|        75 | 6549 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 6550 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 6551 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 6552 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 6553 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 6554 | `			return SXRET_OK;` |
|         - | 6555 | `		}` |
|       ! 0 | 6556 | `	}` |
|        75 | 6557 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        75 | 6558 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        75 | 6559 | `	if( pSetHook ){` |
|         - | 6560 | `		ph7_value sHookRet;` |
|         - | 6561 | `		ph7_value *apHArg[1];` |
|        75 | 6562 | `		apHArg[0] = pValue;` |
|        75 | 6563 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        75 | 6564 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        75 | 6565 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        75 | 6566 | `		VmMagicGuardPop(pVm);` |
|        72 | 6567 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        41 | 6568 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 6569 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 6570 | `			if( rcH == SXRET_OK ){` |
|         6 | 6571 | `				ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 6572 | `				if( pBack ){` |
|         6 | 6573 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 6574 | `				}` |
|         2 | 6575 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 6576 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 6577 | `				return PH7_ABORT;` |
|         - | 6578 | `			}` |
|         - | 6579 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 6580 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 6581 | `			 * parked throw. */` |
|         2 | 6582 | `		}` |
|        75 | 6583 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 6584 | `	}` |
|        75 | 6585 | `	return SXRET_OK;` |
|        41 | 6586 | `}` |
|         - | 6587 | `/*` |
|         - | 6588 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 6589 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 6590 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 6591 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 6592 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 6593 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 6594 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 6595 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 6596 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 6597 | ` */` |
|         - | 6598 | `/*` |
|         - | 6599 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 6600 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 6601 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 6602 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 6603 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 6604 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 6605 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 6606 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 6607 | ` * caller reads the raw slot then.` |
|         - | 6608 | ` */` |
|      1168 | 6609 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 6610 | `{` |
|      1173 | 6611 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 6612 | `	char zHName[384];` |
|         - | 6613 | `	sxu32 nHName;` |
|         - | 6614 | `	ph7_class_method *pGetHook;` |
|         - | 6615 | `	sxi32 rc;` |
|      1168 | 6616 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       685 | 6617 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       207 | 6618 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 6619 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 6620 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 6621 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 6622 | `		 * raw values whose output the routed throw then discards. */` |
|       971 | 6623 | `		return SXERR_NOTFOUND;` |
|         - | 6624 | `	}` |
|       206 | 6625 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       206 | 6626 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       206 | 6627 | `	if( pGetHook == 0 ){` |
|       ! 0 | 6628 | `		return SXERR_NOTFOUND;` |
|         - | 6629 | `	}` |
|       206 | 6630 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       206 | 6631 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       206 | 6632 | `	VmMagicGuardPop(pVm);` |
|       206 | 6633 | `	return rc;` |
|       589 | 6634 | `}` |
|         - | 6635 | `/*` |
|         - | 6636 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 6637 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 6638 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 6639 | ` */` |
|       158 | 6640 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 6641 | `{` |
|       160 | 6642 | `	ph7_value *pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx);` |
|       160 | 6643 | `	if( pScr ){` |
|       160 | 6644 | `		PH7_MemObjRelease(pScr);` |
|        79 | 6645 | `	}` |
|       160 | 6646 | `	VmMemPoolFreeSlot(&pVm->aMemObj,nIdx);` |
|       160 | 6647 | `}` |
|         - | 6648 | `/*` |
|         - | 6649 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 6650 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 6651 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 6652 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 6653 | ` * instance reference.` |
|         - | 6654 | ` */` |
|        24 | 6655 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 6656 | `{` |
|        26 | 6657 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        26 | 6658 | `	if( pEnt == 0 ){` |
|         5 | 6659 | `		return;` |
|         - | 6660 | `	}` |
|        21 | 6661 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        13 | 6662 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         6 | 6663 | `	}` |
|        21 | 6664 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - | 6665 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - | 6666 | `		 * property's backing store — this entry owns it. */` |
|         5 | 6667 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         2 | 6668 | `	}` |
|        21 | 6669 | `	SyBlobRelease(&pEnt->sName);` |
|        21 | 6670 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        21 | 6671 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        14 | 6672 | `}` |
|        92 | 6673 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 6674 | `{` |
|         - | 6675 | `	VmHookRmw sEnt;` |
|         - | 6676 | `	VmHookRmw *pEnt;` |
|         - | 6677 | `	ph7_value *pScr;` |
|         - | 6678 | `	ph7_value sVal;` |
|         - | 6679 | `	ph7_value sKey;` |
|        94 | 6680 | `	sxi32 rc = SXRET_OK;` |
|        94 | 6681 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        94 | 6682 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 6683 | `		return SXERR_NOTFOUND;` |
|         - | 6684 | `	}` |
|        94 | 6685 | `	sEnt = *pEnt;` |
|        94 | 6686 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 6687 | `	/* Copy the computed value out of the scratch slot, then free the slot:` |
|         - | 6688 | `	 * once it is back on the pool's free list the next reserve may hand it to` |
|         - | 6689 | `	 * someone else, so nothing may read the scratch index past this point. The` |
|         - | 6690 | `	 * DIM kind's KEY slot goes the same way, for the same reason. (The pool` |
|         - | 6691 | `	 * itself no longer MOVES -- P1 -- but a freed index is still a freed index.) */` |
|        94 | 6692 | `	PH7_MemObjInit(pVm,&sVal);` |
|        94 | 6693 | `	pScr = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        94 | 6694 | `	if( pScr ){` |
|        94 | 6695 | `		PH7_MemObjStore(pScr,&sVal);` |
|        46 | 6696 | `	}` |
|        94 | 6697 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        94 | 6698 | `	sVal.nIdx = SXU32_HIGH;` |
|        94 | 6699 | `	PH7_MemObjInit(pVm,&sKey);` |
|        94 | 6700 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        52 | 6701 | `		ph7_value *pKeySlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        52 | 6702 | `		if( pKeySlot ){` |
|        52 | 6703 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        25 | 6704 | `		}` |
|        52 | 6705 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        52 | 6706 | `		sKey.nIdx = SXU32_HIGH;` |
|        25 | 6707 | `	}` |
|        94 | 6708 | `	if( pVm->nBoundaryRc == 0 ){` |
|        94 | 6709 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - | 6710 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - | 6711 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - | 6712 | `			SyString sPropName;` |
|        29 | 6713 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        29 | 6714 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        80 | 6715 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - | 6716 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - | 6717 | `			 * back through offsetSet($key, $value). */` |
|        52 | 6718 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - | 6719 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        52 | 6720 | `			if( pSet ){` |
|         - | 6721 | `				ph7_value *apArg[2];` |
|        43 | 6722 | `				apArg[0] = &sKey;` |
|        43 | 6723 | `				apArg[1] = &sVal;` |
|        43 | 6724 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        22 | 6725 | `			}else{` |
|         - | 6726 | `				/* A container that answers a READ and no ArrayAccess: its own` |
|         - | 6727 | `				 * dimension handler gets the computed value first -- php's` |
|         - | 6728 | `` 				 * SimpleXMLElement stores it, which is what makes `$x['a'] .= 'x'` `` |
|         - | 6729 | `				 * work there. A handler that stores nothing (DOMNodeList, PDORow)` |
|         - | 6730 | `				 * leaves the write, and php's read-then-write pair then ends in the` |
|         - | 6731 | ``				 * plain store's Error, so `$list[9] .= 'x'` says what`` |
|         - | 6732 | ``				 * `$list[9] = 'x'` says. Parked: this runs at an arithmetic op's`` |
|         - | 6733 | `				 * tail, not at a throw boundary. */` |
|         - | 6734 | `				PH7_NativeDimCtx sDim;` |
|        10 | 6735 | `				if( PH7_ClassNativeDimStore(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|         - | 6736 | `					&sKey,&sVal,&sDim) ){` |
|         6 | 6737 | `					if( sDim.zThrowClass ){` |
|         4 | 6738 | `						VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),sDim.zThrowClass,` |
|         2 | 6739 | `							sDim.zThrowMsg,(sxu32)SyStrlen(sDim.zThrowMsg)));` |
|         1 | 6740 | `					}` |
|         4 | 6741 | `				}else{` |
|         - | 6742 | `					char zMsg[256];` |
|         7 | 6743 | `					sxu32 nMsg = PH7_ClassNativeDimRefusal(sEnt.pThis,PH7_NATIVE_DIM_WRITE,` |
|         2 | 6744 | `						zMsg,sizeof(zMsg));` |
|         5 | 6745 | `					VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg));` |
|         - | 6746 | `				}` |
|         - | 6747 | `			}` |
|        27 | 6748 | `		}else{` |
|        15 | 6749 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - | 6750 | `		}` |
|        46 | 6751 | `	}` |
|        94 | 6752 | `	SyBlobRelease(&sEnt.sName);` |
|        94 | 6753 | `	PH7_MemObjRelease(&sKey);` |
|        94 | 6754 | `	PH7_MemObjRelease(&sVal);` |
|        94 | 6755 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        94 | 6756 | `	return rc;` |
|        48 | 6757 | `}` |
|         - | 6758 | `/*` |
|         - | 6759 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 6760 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 6761 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 6762 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 6763 | ` */` |
|       804 | 6764 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 6765 | `{` |
|         - | 6766 | `	ph7_class_method *pSetMeth;` |
|       807 | 6767 | `	if( PH7_ClassNativePropOwns(pSetThis,pName) ){` |
|         - | 6768 | `		/* php's write_property handler for a name the class's own table carries:` |
|         - | 6769 | ``		 * it answers BEFORE the standard path, so a subclass's `__set` never sees`` |
|         - | 6770 | `		 * a DOM property and the handler's refusal is the one a program catches.` |
|         - | 6771 | `		 * Every overloaded write funnels through here -- the plain store, the` |
|         - | 6772 | ``		 * compound assign's write-back, the `??=` and Reflection -- so this is the`` |
|         - | 6773 | `		 * one door the handler needs. The refusal is PARKED: this runs at an` |
|         - | 6774 | `		 * opcode's tail rather than at a throw boundary. */` |
|         - | 6775 | `		PH7_NativePropCtx sNat;` |
|       752 | 6776 | `		if( PH7_ClassNativePropAsk(pSetThis,&sNat,PH7_NATIVE_PROP_STORE,pName,pValue)` |
|       754 | 6777 | `		 && sNat.zThrowClass ){` |
|       246 | 6778 | `			VmBoundaryPark(&(*pVm),VmThrowFixedErrorCode(&(*pVm),sNat.zThrowClass,` |
|        81 | 6779 | `				sNat.iThrowCode,sNat.zThrowMsg));` |
|        81 | 6780 | `		}` |
|       754 | 6781 | `		return;` |
|         - | 6782 | `	}` |
|        55 | 6783 | `	pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|        55 | 6784 | `	if( pSetMeth ){` |
|         - | 6785 | `		ph7_value sNameVal;` |
|         - | 6786 | `		ph7_value *apSetArg[2];` |
|        55 | 6787 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|        55 | 6788 | `		sNameVal.nIdx = SXU32_HIGH;` |
|        55 | 6789 | `		apSetArg[0] = &sNameVal;` |
|        55 | 6790 | `		apSetArg[1] = pValue;` |
|        55 | 6791 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|        55 | 6792 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|        55 | 6793 | `		VmMagicGuardPop(pVm);` |
|        55 | 6794 | `		PH7_MemObjRelease(&sNameVal);` |
|        26 | 6795 | `	}` |
|       405 | 6796 | `}` |
|         - | 6797 | `/*` |
|         - | 6798 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 6799 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 6800 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 6801 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 6802 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 6803 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 6804 | ` * path (the SyHash-layout incident class).` |
|         - | 6805 | ` */` |
|         - | 6806 | `/*` |
|         - | 6807 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 6808 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 6809 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 6810 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 6811 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 6812 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 6813 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 6814 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 6815 | ` * never linked (INIT error path).` |
|         - | 6816 | ` */` |
|     41120 | 6817 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 6818 | `{` |
|     41125 | 6819 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     41125 | 6820 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 6821 | `	sxu32 i;` |
|         - | 6822 | `	/* Drop the owning activation's claim on it first: the frame list is what` |
|         - | 6823 | `	 * guarantees a step cannot outlive the frame that made it, so it has to be` |
|         - | 6824 | `	 * left in step with aStep by the same door. A step that never made it onto` |
|         - | 6825 | `	 * aStep never made it onto the frame list either (INIT links both together,` |
|         - | 6826 | `	 * after the SySetPut), so the walk below simply finds nothing. */` |
|     41125 | 6827 | `	if( pStep->pFrame ){` |
|     41125 | 6828 | `		ph7_foreach_step **ppLink = &pStep->pFrame->pForeachSteps;` |
|     41147 | 6829 | `		while( *ppLink ){` |
|     40917 | 6830 | `			if( *ppLink == pStep ){` |
|     40895 | 6831 | `				*ppLink = pStep->pNextFrameStep;` |
|     40895 | 6832 | `				break;` |
|         - | 6833 | `			}` |
|        26 | 6834 | `			ppLink = &(*ppLink)->pNextFrameStep;` |
|         4 | 6835 | `		}` |
|     41125 | 6836 | `		pStep->pNextFrameStep = 0;` |
|     20528 | 6837 | `	}` |
|     43967 | 6838 | `	for( i = 0 ; i < n ; ++i ){` |
|     43967 | 6839 | `		if( apStep[i] == pStep ){` |
|     41137 | 6840 | `			for( ; i + 1 < n ; ++i ){` |
|        13 | 6841 | `				apStep[i] = apStep[i + 1];` |
|         7 | 6842 | `			}` |
|     41125 | 6843 | `			(void)SySetPop(&pInfo->aStep);` |
|     41125 | 6844 | `			return;` |
|         - | 6845 | `		}` |
|      1426 | 6846 | `	}` |
|     20533 | 6847 | `}` |
|         - | 6848 | `/*` |
|         - | 6849 | ` * End every foreach walk this activation still owns, because the activation is` |
|         - | 6850 | ` * about to die.` |
|         - | 6851 | ` *` |
|         - | 6852 | `` * A loop left through `break`, `return`, `goto` or an exception never reaches the`` |
|         - | 6853 | ` * "no more entries" arm that frees its step. OP_FOREACH_INIT reclaims such a` |
|         - | 6854 | ` * leftover, but only one whose owning frame is the frame running INIT -- so a step` |
|         - | 6855 | ` * belonging to an activation that had already returned stayed on the per-STATEMENT` |
|         - | 6856 | ` * aStep for the life of the VM, holding ~140 bytes and a retain of the subject, and` |
|         - | 6857 | ` * INIT's reclaim scan walked past all of them on every single iteration of every` |
|         - | 6858 | ` * enclosing loop. That is quadratic in the number of broken loops a program runs:` |
|         - | 6859 | ` * phpcs over one 318-line file reached 3600 dead steps and spent 60% of its time in` |
|         - | 6860 | ` * that scan.` |
|         - | 6861 | ` *` |
|         - | 6862 | ` * The frame that made a step is the one that can always end it. Called from both` |
|         - | 6863 | ` * VmFrame free sites (VmLeaveFrame and VmFreeDetachedFrame), after the frame has` |
|         - | 6864 | ` * left the active chain and before its locals are torn down -- the same point, and` |
|         - | 6865 | ` * the same order, the loop's own last iteration would have released it at.` |
|         - | 6866 | ` */` |
|   3738869 | 6867 | `PH7_PRIVATE void VmReleaseFrameForeachSteps(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 | 6868 | `{` |
|   3738874 | 6869 | `	if( pFrame == 0 ){` |
|       ! 0 | 6870 | `		return;` |
|         - | 6871 | `	}` |
|   3739104 | 6872 | `	while( pFrame->pForeachSteps ){` |
|       234 | 6873 | `		ph7_foreach_step *pStep = pFrame->pForeachSteps;` |
|         - | 6874 | `		/* Detach BEFORE releasing rather than letting the release do it. The release` |
|         - | 6875 | `		 * runs teardown that can re-enter the VM (an instance losing its last retain` |
|         - | 6876 | `		 * runs __destruct), and a head that is still linked when that happens is a` |
|         - | 6877 | `		 * step whose frame is dying being handed back out. It also makes the loop` |
|         - | 6878 | `		 * unconditionally terminate: nothing here depends on the release finding this` |
|         - | 6879 | `		 * step to unlink, which VmForeachStepUnlink then simply doesn't. */` |
|       234 | 6880 | `		pFrame->pForeachSteps = pStep->pNextFrameStep;` |
|       234 | 6881 | `		pStep->pNextFrameStep = 0;` |
|       234 | 6882 | `		if( pStep->pInfo == 0 ){` |
|         - | 6883 | `			/* Never linked to a statement, so nothing else can free it. */` |
|       ! 0 | 6884 | `			SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       ! 0 | 6885 | `			continue;` |
|         - | 6886 | `		}` |
|       234 | 6887 | `		VmForeachStepRelease(&(*pVm),pStep->pInfo,pStep);` |
|         4 | 6888 | `	}` |
|   1869167 | 6889 | `}` |
|       708 | 6890 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 6891 | `{` |
|       713 | 6892 | `	if( pStep->pOwner ){` |
|       253 | 6893 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       125 | 6894 | `	}` |
|       713 | 6895 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       713 | 6896 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       713 | 6897 | `	PH7_ClassInstanceUnref(pThis);` |
|       713 | 6898 | `}` |
|         - | 6899 | `/*` |
|         - | 6900 | ` * Release ONE foreach step of any kind — the single door OP_FOREACH_INIT uses to` |
|         - | 6901 | ` * reclaim a step its loop never exhausted.` |
|         - | 6902 | ` *` |
|         - | 6903 | `` * A `foreach` that leaves through `break`, `return`, `goto` or an exception never`` |
|         - | 6904 | ` * reaches the "no more entries" arm, so its step stayed on pInfo->aStep forever` |
|         - | 6905 | ` * (~140 bytes and one retain of the subject per execution: 200k broken loops leaked` |
|         - | 6906 | ` * 28 MB). Worse for an OBJECT loop, whose cursor is REGISTERED on the instance —` |
|         - | 6907 | ` * every abandoned walk left an entry that each later property add/remove had to` |
|         - | 6908 | ` * walk past. A step for THIS pInfo whose owning frame is the running one cannot be` |
|         - | 6909 | ` * mid-loop when INIT runs again (the frame executes one instruction at a time), so` |
|         - | 6910 | ` * INIT reclaims it before pushing its own.` |
|         - | 6911 | ` */` |
|       244 | 6912 | `PH7_PRIVATE void VmForeachStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 6913 | `{` |
|         - | 6914 | `	ph7_class_instance *pThis;` |
|       249 | 6915 | `	if( pStep->iFlags & PH7_4EACH_STEP_HASHMAP ){` |
|       235 | 6916 | `		VmForeachHashmapStepRelease(&(*pVm),pInfo,pStep,TRUE);` |
|       235 | 6917 | `		return;` |
|         - | 6918 | `	}` |
|         - | 6919 | `	/* Object-shaped step (plain attribute walk or the Iterator protocol): both` |
|         - | 6920 | `	 * retain xIter.pThis, and only the plain one holds a registered cursor. */` |
|        22 | 6921 | `	pThis = (pStep->iFlags & (PH7_4EACH_STEP_OBJECT\|PH7_4EACH_STEP_ITERATOR))` |
|        14 | 6922 | `		? pStep->xIter.pThis : 0;` |
|        15 | 6923 | `	if( pStep->iFlags & PH7_4EACH_STEP_OBJECT ){` |
|       ! 0 | 6924 | `		PH7_ClassInstanceIterClose(pStep->xIter.pThis,&pStep->sAttrIter);` |
|       ! 0 | 6925 | `	}` |
|        15 | 6926 | `	if( pStep->pOwner ){` |
|       ! 0 | 6927 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|       ! 0 | 6928 | `	}` |
|        15 | 6929 | `	VmForeachStepUnlink(pInfo,pStep);` |
|        15 | 6930 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|        15 | 6931 | `	if( pThis ){` |
|        15 | 6932 | `		PH7_ClassInstanceUnref(pThis);` |
|         7 | 6933 | `	}` |
|       127 | 6934 | `}` |
|         - | 6935 | `/*` |
|         - | 6936 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 6937 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 6938 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 6939 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 6940 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 6941 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 6942 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 6943 | ` * map's last reference.` |
|         - | 6944 | ` */` |
|     40292 | 6945 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 6946 | `{` |
|     40297 | 6947 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     40297 | 6948 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     40297 | 6949 | `	if( bPop ){` |
|         - | 6950 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 6951 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     40297 | 6952 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     20114 | 6953 | `	}` |
|     40297 | 6954 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     40297 | 6955 | `	PH7_HashmapUnref(pMap);` |
|     40297 | 6956 | `}` |
|         - | 6957 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 6958 | `/*` |
|         - | 6959 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 6960 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 6961 | ` * See block-comment on that function for additional information.` |
|         - | 6962 | ` */` |
|   1516721 | 6963 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 6964 | `{` |
|         - | 6965 | `	ph7_value *pStack;` |
|         - | 6966 | `	sxu32 nCap;` |
|         - | 6967 | `	sxi32 rc;` |
|         - | 6968 | `	/* Allocate a new operand stack */` |
|   1516726 | 6969 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1516726 | 6970 | `	if( pStack == 0 ){` |
|       ! 0 | 6971 | `		return SXERR_MEM;` |
|         - | 6972 | `	}` |
|   1516726 | 6973 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 6974 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 6975 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1516726 | 6976 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 6977 | `	/* Free the operand stack */` |
|   1516726 | 6978 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 6979 | `	/* Execution result */` |
|   1516726 | 6980 | `	return rc;` |
|    758325 | 6981 | `}` |
|         - | 6982 | `/*` |
|         - | 6983 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 6984 | ` * statement must honour?` |
|         - | 6985 | ` *` |
|         - | 6986 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 6987 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 6988 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 6989 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 6990 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 6991 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 6992 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 6993 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 6994 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 6995 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 6996 | ` * drift apart.` |
|         - | 6997 | ` *` |
|         - | 6998 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 6999 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 7000 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 7001 | ` * this opcode did not create as its own throw.` |
|         - | 7002 | ` *` |
|         - | 7003 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 7004 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 7005 | ` */` |
|         - | 7006 | `/*` |
|         - | 7007 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 7008 | ` *` |
|         - | 7009 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 7010 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 7011 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 7012 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 7013 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 7014 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 7015 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 7016 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 7017 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 7018 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 7019 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 7020 | ` *` |
|         - | 7021 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 7022 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 7023 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 7024 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 7025 | ` */` |
|      7188 | 7026 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 7027 | `{` |
|         - | 7028 | `	static const char *const azAuto[] = {` |
|         - | 7029 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 7030 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 7031 | `	};` |
|         - | 7032 | `	sxu32 n;` |
|     78455 | 7033 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     71329 | 7034 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     71329 | 7035 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        65 | 7036 | `			return 1;` |
|         - | 7037 | `		}` |
|     35386 | 7038 | `	}` |
|      7131 | 7039 | `	return 0;` |
|      3574 | 7040 | `}` |
|     29503 | 7041 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 7042 | `{` |
|     27678 | 7043 | `	return rc == PH7_EXCEPTION` |
|     27682 | 7044 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     42438 | 7045 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 7046 | `}` |
|         - | 7047 | `/*` |
|         - | 7048 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 7049 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 7050 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 7051 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 7052 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 7053 | ` */` |
|       182 | 7054 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         3 | 7055 | `{` |
|       185 | 7056 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       185 | 7057 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 7058 | `	sxi32 rc;` |
|       185 | 7059 | `	if( pDeclCls ){` |
|       169 | 7060 | `		pVm->pConstEvalClass = pDeclCls;` |
|       169 | 7061 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        83 | 7062 | `	}` |
|       185 | 7063 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       185 | 7064 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       185 | 7065 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       185 | 7066 | `	return rc;` |
|         3 | 7067 | `}` |
|         - | 7068 | `/*` |
|         - | 7069 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|         - | 7070 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|         - | 7071 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|         - | 7072 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|         - | 7073 | ` * non-zero status — lost that output entirely.` |
|         - | 7074 | ` *` |
|         - | 7075 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|         - | 7076 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|         - | 7077 | ` * handed.` |
|         - | 7078 | ` */` |
|      5569 | 7079 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 7080 | `{` |
|      5574 | 7081 | `	PH7_VmObFlushAll(&(*pVm));` |
|      5574 | 7082 | `}` |
|         - | 7083 | `/*` |
|         - | 7084 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 7085 | ` * or more calls to [register_shutdown_function()].` |
|         - | 7086 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 7087 | ` * execution ends.` |
|         - | 7088 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 7089 | ` * additional information.` |
|         - | 7090 | ` */` |
|      5569 | 7091 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 7092 | `{` |
|         - | 7093 | `	VmShutdownCB *pEntry;` |
|         - | 7094 | `	ph7_value *apArg[10];` |
|         - | 7095 | `	sxu32 n,nEntry;` |
|      5574 | 7096 | `	sxi32 rc = SXRET_OK;` |
|         - | 7097 | `	int i;` |
|         - | 7098 | `	/* Point to the stack of registered callbacks */` |
|      5574 | 7099 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     61264 | 7100 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     55695 | 7101 | `		apArg[i] = 0;` |
|     27805 | 7102 | `	}` |
|         - | 7103 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 7104 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 7105 | `	 * callbacks, mirroring PHP.` |
|         - | 7106 | `	 */` |
|      5574 | 7107 | `	pVm->bHaltRequested = 0;` |
|      5598 | 7108 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        31 | 7109 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        31 | 7110 | `		if( pEntry ){` |
|         - | 7111 | `			/* Prepare callback arguments if any */` |
|        31 | 7112 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 7113 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 7114 | `					break;` |
|         - | 7115 | `				}` |
|       ! 0 | 7116 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 7117 | `			}` |
|         - | 7118 | `			/* Invoke the callback */` |
|        31 | 7119 | `			rc = PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 7120 | `			/*` |
|         - | 7121 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 7122 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 7123 | `			 */` |
|        31 | 7124 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        31 | 7125 | `			if( pEntry ){` |
|        31 | 7126 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        31 | 7127 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 7128 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 7129 | `				}` |
|        13 | 7130 | `			}` |
|        31 | 7131 | `			if( pVm->bHaltRequested \|\| rc == SXERR_ABORT ){` |
|         - | 7132 | `				/* exit() inside the callback, or a throwable it never caught: php` |
|         - | 7133 | `				 * abandons the remaining callbacks either way (the bailout leaves` |
|         - | 7134 | `				 * php_call_shutdown_functions), and goes on to the destructors. */` |
|         2 | 7135 | `				break;` |
|         - | 7136 | `			}` |
|        12 | 7137 | `		}` |
|        17 | 7138 | `	}` |
|      5574 | 7139 | `	SySetReset(&pVm->aShutdown);` |
|      5574 | 7140 | `}` |
|         - | 7141 | `/*` |
|         - | 7142 | ` * One name of the global symbol table, snapshotted for the shutdown pass below.` |
|         - | 7143 | ` * Held as an offset into a private blob rather than a pointer: a destructor is` |
|         - | 7144 | ` * arbitrary PHP and may unset any global, which frees the key the table owns.` |
|         - | 7145 | ` */` |
|         - | 7146 | `typedef struct VmShutdownName VmShutdownName;` |
|         - | 7147 | `struct VmShutdownName` |
|         - | 7148 | `{` |
|         - | 7149 | `	sxu32 nOfft;  /* Offset of the name in the caller's snapshot blob */` |
|         - | 7150 | `	sxu32 nByte;  /* Its length */` |
|         - | 7151 | `};` |
|         - | 7152 | `/*` |
|         - | 7153 | ` * TRUE when this slot is held by exactly ONE name and nothing else -- the state php` |
|         - | 7154 | `` * spells `Z_TYPE_P(zv) == IS_OBJECT` with a refcount of 1 on a symbol-table entry.`` |
|         - | 7155 | ` *` |
|         - | 7156 | `` * php's symbol-table pass tests the ZVAL, and a name written with `&` is not an object`` |
|         - | 7157 | `` * zval at all: `$g = new T; $r = &$g;` makes both entries IS_REFERENCE, which the test`` |
|         - | 7158 | ` * rejects outright and leaves to the object-store pass. This engine has no separate` |
|         - | 7159 | ` * reference cell -- the two names simply share one slot -- so the equivalent question` |
|         - | 7160 | ` * is how many names the slot's reference record still lists, plus whether anything the` |
|         - | 7161 | `` * record cannot name pins it (a `use (&$x)` capture, a static, a reference-bound`` |
|         - | 7162 | ` * property), which php would also be carrying as a reference.` |
|         - | 7163 | ` *` |
|         - | 7164 | ` * The $GLOBALS entry for the name is not a holder for this purpose: it is how this` |
|         - | 7165 | ` * engine spells the symbol table, not a second reference to the value.` |
|         - | 7166 | ` */` |
|      3443 | 7167 | `static int VmSlotHeldByOneName(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 7168 | `{` |
|      3448 | 7169 | `	if( PH7_VmSlotSelfPinned(&(*pVm),nIdx) ){` |
|        56 | 7170 | `		return 0;` |
|         - | 7171 | `	}` |
|      3396 | 7172 | `	return PH7_VmSlotEntryCount(&(*pVm),nIdx) == 1;` |
|      1723 | 7173 | `}` |
|         - | 7174 | `/*` |
|         - | 7175 | ` * php's shutdown destructor phase, first half: the GLOBAL SYMBOL TABLE.` |
|         - | 7176 | ` *` |
|         - | 7177 | `` * `shutdown_destructors()` walks the symbol table in REVERSE and drops every entry`` |
|         - | 7178 | ` * holding an object nothing else refers to, repeating the walk while the table keeps` |
|         - | 7179 | `` * shrinking. That reverse walk is observable -- `$a = new T; $b = new T;` destructs`` |
|         - | 7180 | ` * $b before $a -- and it is the half that actually FREES its objects, which is why a` |
|         - | 7181 | ` * destructor here sees the rest of the program's globals still standing.` |
|         - | 7182 | ` *` |
|         - | 7183 | `` * `iRef == 1` plus VmSlotHeldByOneName is this engine's spelling of php's`` |
|         - | 7184 | `` * `Z_TYPE_P(zv) == IS_OBJECT && Z_REFCOUNT_P(zv) == 1`: exactly one memory object holds`` |
|         - | 7185 | ` * the instance and exactly one name holds that, so dropping the name ends it. Everything` |
|         - | 7186 | ` * else -- an object two names share, one an array or a property also holds, one a name` |
|         - | 7187 | `` * written with `&` reaches -- is left to the second half.`` |
|         - | 7188 | ` */` |
|      5569 | 7189 | `static void VmShutdownGlobalPass(ph7_vm *pVm)` |
|         5 | 7190 | `{` |
|         - | 7191 | `	VmFrame *pFrame;` |
|      5608 | 7192 | `	for( pFrame = pVm->pFrame ; pFrame && pFrame->pParent ; pFrame = pFrame->pParent ){}` |
|      5574 | 7193 | `	if( pFrame == 0 ){` |
|       ! 0 | 7194 | `		return;` |
|         - | 7195 | `	}` |
|      3746 | 7196 | `	for(;;){` |
|         - | 7197 | `		ph7_hashmap_node *pNode;` |
|         - | 7198 | `		VmShutdownName *aName;` |
|         - | 7199 | `		SyBlob sNames;` |
|         - | 7200 | `		SySet aEntry;` |
|         - | 7201 | `		sxu32 n;` |
|      6540 | 7202 | `		int bDropped = 0;` |
|      6540 | 7203 | `		if( pVm->pGlobal == 0 \|\| pVm->pGlobal->nEntry < 1 ){` |
|       ! 0 | 7204 | `			return;` |
|         - | 7205 | `		}` |
|         - | 7206 | `		/* Snapshot the names, last-declared first. The map's insertion list runs` |
|         - | 7207 | `		 * pFirst -> pPrev -> ... -> pLast, so walking it BACKWARDS is pLast and the` |
|         - | 7208 | `		 * pNext chain (the two link names read the other way round here). */` |
|      6540 | 7209 | `		SyBlobInit(&sNames,&pVm->sAllocator);` |
|      6540 | 7210 | `		SySetInit(&aEntry,&pVm->sAllocator,sizeof(VmShutdownName));` |
|     95747 | 7211 | `		for( pNode = pVm->pGlobal->pLast ; pNode ; pNode = pNode->pNext ){` |
|         - | 7212 | `			VmShutdownName sName;` |
|     89212 | 7213 | `			if( pNode->iType != HASHMAP_BLOB_NODE \|\| SyBlobLength(&pNode->xKey.sKey) < 1 ){` |
|       ! 0 | 7214 | `				continue;` |
|         - | 7215 | `			}` |
|     89212 | 7216 | `			sName.nOfft = SyBlobLength(&sNames);` |
|     89212 | 7217 | `			sName.nByte = SyBlobLength(&pNode->xKey.sKey);` |
|     89207 | 7218 | `			if( SyBlobAppend(&sNames,SyBlobData(&pNode->xKey.sKey),sName.nByte) != SXRET_OK` |
|     89212 | 7219 | `			 \|\| SySetPut(&aEntry,(const void *)&sName) != SXRET_OK ){` |
|         - | 7220 | `				/* Out of memory: go on with the names already gathered. */` |
|       ! 0 | 7221 | `				break;` |
|         - | 7222 | `			}` |
|     44489 | 7223 | `		}` |
|      6540 | 7224 | `		aName = (VmShutdownName *)SySetBasePtr(&aEntry);` |
|     95695 | 7225 | `		for( n = 0 ; n < SySetUsed(&aEntry) ; ++n ){` |
|     89164 | 7226 | `			const char *zName = (const char *)SyBlobData(&sNames) + aName[n].nOfft;` |
|         - | 7227 | `			ph7_class_instance *pThis;` |
|         - | 7228 | `			SyHashEntry *pHash;` |
|         - | 7229 | `			ph7_value *pObj;` |
|     89164 | 7230 | `			pHash = SyHashGet(&pFrame->hVar,(const void *)zName,aName[n].nByte);` |
|     89164 | 7231 | `			if( pHash == 0 ){` |
|     65315 | 7232 | `				continue;  /* An earlier destructor already dropped this one */` |
|         - | 7233 | `			}` |
|     23854 | 7234 | `			pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pHash->pUserData));` |
|     23854 | 7235 | `			if( pObj == 0 \|\| (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     19473 | 7236 | `				continue;` |
|         - | 7237 | `			}` |
|      4386 | 7238 | `			pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      4381 | 7239 | `			if( pThis->iRef != 1` |
|      3917 | 7240 | `			 \|\| !VmSlotHeldByOneName(&(*pVm),(sxu32)SX_PTR_TO_INT(pHash->pUserData)) ){` |
|      1005 | 7241 | `				continue;` |
|         - | 7242 | `			}` |
|      3386 | 7243 | `			VmUnsetVarByNameEx(&(*pVm),pFrame,zName,aName[n].nByte,FALSE);` |
|      3386 | 7244 | `			bDropped = 1;` |
|      3386 | 7245 | `			if( pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|         4 | 7246 | `				break;` |
|         - | 7247 | `			}` |
|      1690 | 7248 | `		}` |
|      6540 | 7249 | `		SySetRelease(&aEntry);` |
|      6540 | 7250 | `		SyBlobRelease(&sNames);` |
|      6540 | 7251 | `		if( !bDropped \|\| pVm->bHaltRequested \|\| pVm->bShutdownAborted ){` |
|      5574 | 7252 | `			return;` |
|         - | 7253 | `		}` |
|         5 | 7254 | `	}` |
|      2785 | 7255 | `}` |
|         - | 7256 | `/*` |
|         - | 7257 | ` * Order the collected instances by their object handle -- php's object store is` |
|         - | 7258 | ` * walked front to back, and a handle is handed out in creation order, so this is` |
|         - | 7259 | ` * "oldest object first". Shell sort: no allocation, no recursion, and the array is` |
|         - | 7260 | ` * the objects a finished program left alive.` |
|         - | 7261 | ` */` |
|       195 | 7262 | `static void VmSortByObjId(ph7_class_instance **apObj,sxu32 nUsed)` |
|         5 | 7263 | `{` |
|         - | 7264 | `	static const sxu32 aGap[] = { 701, 301, 132, 57, 23, 10, 4, 1 };` |
|         - | 7265 | `	sxu32 g;` |
|      1760 | 7266 | `	for( g = 0 ; g < SX_ARRAYSIZE(aGap) ; ++g ){` |
|      1565 | 7267 | `		sxu32 nGap = aGap[g], i;` |
|     18473 | 7268 | `		for( i = nGap ; i < nUsed ; ++i ){` |
|     16913 | 7269 | `			ph7_class_instance *pCur = apObj[i];` |
|     16913 | 7270 | `			sxu32 j = i;` |
|     28514 | 7271 | `			while( j >= nGap && apObj[j-nGap]->nObjId > pCur->nObjId ){` |
|     11606 | 7272 | `				apObj[j] = apObj[j-nGap];` |
|     11606 | 7273 | `				j -= nGap;` |
|         5 | 7274 | `			}` |
|     16913 | 7275 | `			apObj[j] = pCur;` |
|      8455 | 7276 | `		}` |
|       781 | 7277 | `	}` |
|       200 | 7278 | `}` |
|         - | 7279 | `/*` |
|         - | 7280 | ` * php's shutdown destructor phase, second half: the OBJECT STORE.` |
|         - | 7281 | ` *` |
|         - | 7282 | `` * `zend_objects_store_call_destructors()` reaches every object still alive after the`` |
|         - | 7283 | ` * symbol-table pass -- one a class static, a function static, an array or another` |
|         - | 7284 | ` * object holds, and one that is only part of a cycle -- and calls its destructor in` |
|         - | 7285 | ` * CREATION order without freeing it. The free comes later, from the teardown proper,` |
|         - | 7286 | ` * which is why the destructor is flagged as already run (CLASS_INSTANCE_DTOR_CALLED).` |
|         - | 7287 | ` *` |
|         - | 7288 | ` * Every object is reachable from the memory-object pool, so that pool is the store.` |
|         - | 7289 | ` * A destructor may create objects of its own (php destructs those too), so the sweep` |
|         - | 7290 | ` * repeats until a round finds nothing new.` |
|         - | 7291 | ` */` |
|      5569 | 7292 | `static void VmShutdownObjectPass(ph7_vm *pVm)` |
|         5 | 7293 | `{` |
|      5769 | 7294 | `	while( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|         - | 7295 | `		ph7_class_instance **apObj;` |
|         - | 7296 | `		SySet aObj;` |
|         - | 7297 | `		sxu32 n,nUsed;` |
|      5765 | 7298 | `		SySetInit(&aObj,&pVm->sAllocator,sizeof(ph7_class_instance *));` |
|   5932005 | 7299 | `		for( n = 0 ; n < pVm->aMemObj.nUsed ; ++n ){` |
|   5926245 | 7300 | `			ph7_value *pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,n);` |
|   5926245 | 7301 | `			if( pObj && (pObj->iFlags & MEMOBJ_OBJ) && pObj->x.pOther ){` |
|      5895 | 7302 | `				ph7_class_instance *pThis = (ph7_class_instance *)pObj->x.pOther;` |
|      5895 | 7303 | `				if( (pThis->iFlags & CLASS_INSTANCE_DTOR_CALLED) == 0 ){` |
|      2953 | 7304 | `					SySetPut(&aObj,(const void *)&pThis);` |
|      1471 | 7305 | `				}` |
|      2939 | 7306 | `			}` |
|   2962130 | 7307 | `		}` |
|      5765 | 7308 | `		nUsed = SySetUsed(&aObj);` |
|      5765 | 7309 | `		if( nUsed < 1 ){` |
|      5570 | 7310 | `			SySetRelease(&aObj);` |
|      5570 | 7311 | `			return;` |
|         - | 7312 | `		}` |
|       200 | 7313 | `		apObj = (ph7_class_instance **)SySetBasePtr(&aObj);` |
|       200 | 7314 | `		VmSortByObjId(apObj,nUsed);` |
|         - | 7315 | `		/* Pin every one of them BEFORE the first body runs: a destructor is free to` |
|         - | 7316 | `		 * unset whatever holds another object on this list, and the release that` |
|         - | 7317 | `		 * follows would free a pointer still to be visited. php pins the same way,` |
|         - | 7318 | `		 * one at a time, because its store can tell a dead bucket from a live one. */` |
|      3148 | 7319 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      2953 | 7320 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       999 | 7321 | `				continue;  /* Several names for one object: sorted, so duplicates adjoin */` |
|         - | 7322 | `			}` |
|      1959 | 7323 | `			apObj[n]->iRef++;` |
|       979 | 7324 | `		}` |
|      3148 | 7325 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      2953 | 7326 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       999 | 7327 | `				continue;` |
|         - | 7328 | `			}` |
|      1959 | 7329 | `			if( !pVm->bHaltRequested && !pVm->bShutdownAborted ){` |
|         - | 7330 | `				/* A body that leaves an uncaught throwable raises bShutdownAborted` |
|         - | 7331 | `				 * itself, which is what stops this loop and the sweep around it:` |
|         - | 7332 | `				 * php abandons the whole phase on the first one, leaving every` |
|         - | 7333 | `				 * remaining object undestructed. */` |
|      1959 | 7334 | `				PH7_ClassInstanceCallDestructor(apObj[n]);` |
|       974 | 7335 | `			}` |
|       979 | 7336 | `		}` |
|      3148 | 7337 | `		for( n = 0 ; n < nUsed ; ++n ){` |
|      2953 | 7338 | `			if( n > 0 && apObj[n] == apObj[n-1] ){` |
|       999 | 7339 | `				continue;` |
|         - | 7340 | `			}` |
|      1959 | 7341 | `			PH7_ClassInstanceUnref(apObj[n]);` |
|       979 | 7342 | `		}` |
|       200 | 7343 | `		SySetRelease(&aObj);` |
|         5 | 7344 | `	}` |
|      2785 | 7345 | `}` |
|         - | 7346 | `/*` |
|         - | 7347 | ` * Run every destructor a finished program still owes, between the shutdown callbacks` |
|         - | 7348 | `` * and the output-buffer flush -- php's `zend_call_destructors()`, in that same slot of`` |
|         - | 7349 | `` * `php_request_shutdown()`, which is why a destructor's own echo still lands inside an`` |
|         - | 7350 | ` * open output buffer.` |
|         - | 7351 | ` *` |
|         - | 7352 | ` * Before this existed, an object a program left in a global (or a static, or any` |
|         - | 7353 | ` * container) was torn down by PH7_VmReset with user destructors suppressed, so a` |
|         - | 7354 | ` * destructor that closes a file, flushes a buffer or commits a transaction simply` |
|         - | 7355 | ` * never fired. The two passes below are php's two, in php's order.` |
|         - | 7356 | ` */` |
|      5569 | 7357 | `static void VmCallShutdownDestructors(ph7_vm *pVm)` |
|         5 | 7358 | `{` |
|      5574 | 7359 | `	if( pVm->bInReset ){` |
|       ! 0 | 7360 | `		return;` |
|         - | 7361 | `	}` |
|         - | 7362 | `	/* A halt is consumed the same way the shutdown callbacks consume theirs: php runs` |
|         - | 7363 | `	 * the destructor phase after an exit(), and after a shutdown callback that threw. */` |
|      5574 | 7364 | `	pVm->bHaltRequested = 0;` |
|      5574 | 7365 | `	pVm->bShutdownAborted = 0;` |
|      5574 | 7366 | `	pVm->bInShutdownDtor = 1;` |
|      5574 | 7367 | `	VmShutdownGlobalPass(&(*pVm));` |
|      5574 | 7368 | `	VmShutdownObjectPass(&(*pVm));` |
|      5574 | 7369 | `	pVm->bInShutdownDtor = 0;` |
|      2785 | 7370 | `}` |
|         - | 7371 | `/*` |
|         - | 7372 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 7373 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 7374 | ` * See block-comment on that function for additional information.` |
|         - | 7375 | ` */` |
|      5553 | 7376 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 7377 | `{` |
|         - | 7378 | `	/* Make sure we are ready to execute this program */` |
|      5558 | 7379 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 7380 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 7381 | `	}` |
|         - | 7382 | `	/* Set the execution magic number  */` |
|      5558 | 7383 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 7384 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 7385 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 7386 | `	{` |
|      5558 | 7387 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|         - | 7388 | `		/* Top-level code is a body like any other, and the global frame is the one` |
|         - | 7389 | `		 * frame that was pushed long before there was anything to number. Number it` |
|         - | 7390 | `		 * here, where the program about to run is finally known. */` |
|      5558 | 7391 | `		if( pVm->pFrame && pVm->pFrame->pCodeBase == 0 ){` |
|      5558 | 7392 | `			sxu16 nMainName = 0;` |
|      8330 | 7393 | `			VmNumberLocals((VmInstr *)SySetBasePtr(pVm->pByteContainer),` |
|      5553 | 7394 | `				SySetUsed(pVm->pByteContainer),&nMainName);` |
|      5558 | 7395 | `			if( nMainName > 0 ){` |
|      1658 | 7396 | `				pVm->pFrame->pCodeBase = (const VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|       822 | 7397 | `			}` |
|      2772 | 7398 | `		}` |
|      5558 | 7399 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 7400 | `	}` |
|         - | 7401 | `	/* Invoke any shutdown callbacks */` |
|      5566 | 7402 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 7403 | `	/* Then every destructor the program still owes: php's zend_call_destructors(),` |
|         - | 7404 | `	 * which sits exactly here -- after the shutdown callbacks, before the buffers. */` |
|      5566 | 7405 | `	VmCallShutdownDestructors(&(*pVm));` |
|         - | 7406 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 7407 | `	 * shutdown callbacks, which may still write into them. */` |
|      5566 | 7408 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 7409 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|         - | 7410 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|         - | 7411 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|         - | 7412 | `	 * them). */` |
|      5566 | 7413 | `	PH7_VmSessionShutdown(&(*pVm));` |
|         - | 7414 | `	/*` |
|         - | 7415 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 7416 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 7417 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 7418 | `	 */` |
|      5566 | 7419 | `	return SXRET_OK;` |
|      2777 | 7420 | `}` |
|         - | 7421 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 7422 | `/*` |
|         - | 7423 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 7424 | ` * the desired message.` |
|         - | 7425 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 7426 | ` * in 'api.c' for additional information.` |
|         - | 7427 | ` */` |
|    125938 | 7428 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 7429 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 7430 | `	SyString *pString /* Message to output */` |
|         - | 7431 | `	)` |
|         5 | 7432 | `{` |
|    125943 | 7433 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|    125943 | 7434 | `	sxi32 rc = SXRET_OK;` |
|         - | 7435 | `	/* Call the output consumer */` |
|    125943 | 7436 | `	if( pString->nByte > 0 ){` |
|    125943 | 7437 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|    125943 | 7438 | `		VmTrackOutput(pVm, pString->nByte);` |
|     62080 | 7439 | `	}` |
|    125943 | 7440 | `	return rc;` |
|         5 | 7441 | `}` |
|         - | 7442 | `/*` |
|         - | 7443 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 7444 | ` * callback to consume the formatted message.` |
|         - | 7445 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 7446 | ` * in 'api.c' for additional information.` |
|         - | 7447 | ` */` |
|        30 | 7448 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 7449 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 7450 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 7451 | `	va_list ap           /* Variable list of arguments */` |
|         - | 7452 | `	)` |
|         2 | 7453 | `{` |
|        32 | 7454 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        32 | 7455 | `	sxi32 rc = SXRET_OK;` |
|         - | 7456 | `	SyBlob sWorker;` |
|         - | 7457 | `	/* Format the message and call the output consumer */` |
|        32 | 7458 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        32 | 7459 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        32 | 7460 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 7461 | `		/* Consume the formatted message */` |
|        32 | 7462 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        15 | 7463 | `	}` |
|        32 | 7464 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 7465 | `	/* Release the working buffer */` |
|        32 | 7466 | `	SyBlobRelease(&sWorker);` |
|        32 | 7467 | `	return rc;` |
|         2 | 7468 | `}` |
|         - | 7469 | `/*` |
|         - | 7470 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 7471 | ` * This function never fail and always return a pointer` |
|         - | 7472 | ` * to a null terminated string.` |
|         - | 7473 | ` */` |
|        10 | 7474 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 7475 | `{` |
|        11 | 7476 | `	const char *zOp = "Unknown     ";` |
|        11 | 7477 | `	switch(nOp){` |
|         3 | 7478 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 7479 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 7480 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 7481 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 7482 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 7483 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 7484 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 7485 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 7486 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 7487 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 7488 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 7489 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 7490 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 7491 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 7492 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 7493 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 7494 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 7495 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 7496 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 7497 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 7498 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 7499 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 | 7500 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 | 7501 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 7502 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 7503 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 7504 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 7505 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 7506 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 7507 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 7508 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 7509 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 7510 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 7511 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 7512 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 7513 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 7514 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 7515 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 7516 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 7517 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 7518 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 7519 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 7520 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 7521 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 7522 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 7523 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 7524 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 7525 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 7526 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 7527 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 7528 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 7529 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 7530 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 7531 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 7532 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 7533 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 7534 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 7535 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 7536 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 7537 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 7538 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 7539 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 7540 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 7541 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 7542 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 7543 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 7544 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 7545 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 7546 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 7547 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 7548 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 7549 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 7550 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 7551 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 7552 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 7553 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 7554 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 7555 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 7556 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 7557 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 7558 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 7559 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 7560 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 7561 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 7562 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 7563 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 7564 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 7565 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 7566 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 7567 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 7568 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 7569 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 7570 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 7571 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 7572 | `	case PH7_OP_FUNC_DECL:  zOp = "FUNC_DECL  "; break;` |
|       ! 0 | 7573 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 7574 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 7575 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 7576 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 7577 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 7578 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 7579 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 7580 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 7581 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 7582 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 7583 | `	default:` |
|       ! 0 | 7584 | `		break;` |
|         - | 7585 | `	}` |
|        11 | 7586 | `	return zOp;` |
|         1 | 7587 | `}` |
|         - | 7588 | `/*` |
|         - | 7589 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 7590 | ` * The xConsumer() callback which is an used defined function` |
|         - | 7591 | ` * is responsible of consuming the generated dump.` |
|         - | 7592 | ` */` |
|         2 | 7593 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 7594 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 7595 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 7596 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 7597 | `	)` |
|         1 | 7598 | `{` |
|         - | 7599 | `	sxi32 rc;` |
|         3 | 7600 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 7601 | `	return rc;` |
|         1 | 7602 | `}` |
|         - | 7603 | `/*` |
|         - | 7604 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 7605 | ` * outside a class body [i.e: global or function scope].` |
|         - | 7606 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 7607 | ` * in 'compile.c' for additional information.` |
|         - | 7608 | ` */` |
|         4 | 7609 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         1 | 7610 | `{` |
|         5 | 7611 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 7612 | `	/* Evaluate and expand constant value */` |
|         5 | 7613 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|         5 | 7614 | `}` |
|         - | 7615 | `/*` |
|         - | 7616 | ` * Section:` |
|         - | 7617 | ` *  Function handling functions.` |
|         - | 7618 | ` * Status:` |
|         - | 7619 | ` *    Stable.` |
|         - | 7620 | ` */` |
|         - | 7621 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 7622 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 7623 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 7624 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 7625 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 7626 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 7627 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 7628 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 7629 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 7630 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 7631 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 7632 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 7633 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 7634 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 7635 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 7636 | `	    /* Constants management */` |
|         - | 7637 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 7638 | `	{ "define",   vm_builtin_define               },` |
|         - | 7639 | `	{ "constant", vm_builtin_constant             },` |
|         - | 7640 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 7641 | `	   /* Class/Object functions */` |
|         - | 7642 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 7643 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 7644 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 7645 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 7646 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 7647 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 7648 | `	{ "class_parents",   vm_builtin_class_parents     },` |
|         - | 7649 | `	{ "class_implements",vm_builtin_class_implements  },` |
|         - | 7650 | `	{ "class_uses",      vm_builtin_class_uses        },` |
|         - | 7651 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 7652 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 7653 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 7654 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 7655 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 7656 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 7657 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|         - | 7658 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 7659 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 7660 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 7661 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|         - | 7662 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 7663 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 7664 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - | 7665 | `	{ "clone",           vm_builtin_clone             },` |
|         - | 7666 | `	   /* SPL object identity */` |
|         - | 7667 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 7668 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 7669 | `	   /* SPL Autoloading */` |
|         - | 7670 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 7671 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 7672 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 7673 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 7674 | `	{ "spl_autoload_extensions", vm_builtin_spl_autoload_extensions },` |
|         - | 7675 | `	{ "spl_autoload_call",       vm_builtin_spl_autoload_call       },` |
|         - | 7676 | `	{ "spl_classes",             vm_builtin_spl_classes             },` |
|         - | 7677 | `	   /* Random numbers/strings generators */` |
|         - | 7678 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 7679 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 7680 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 7681 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 7682 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 7683 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 7684 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 7685 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 7686 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 7687 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 7688 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 7689 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 7690 | `	   /* Language constructs functions */` |
|         - | 7691 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 7692 | `	{ "print", vm_builtin_print                   },` |
|         - | 7693 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 7694 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 7695 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 7696 | `	  /* Variable handling functions */` |
|         - | 7697 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 7698 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 7699 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - | 7700 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 7701 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 7702 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 7703 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 7704 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 7705 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 7706 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 7707 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 7708 | `	  /* Ouput control functions */` |
|         - | 7709 | `	{ "flush",        vm_builtin_flush             },` |
|         - | 7710 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 7711 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 7712 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 7713 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 7714 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 7715 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 7716 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|         - | 7717 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 7718 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 7719 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 7720 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|         - | 7721 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 7722 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 7723 | `	  /* Assertion functions */` |
|         - | 7724 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 7725 | `	  /* Error reporting functions */` |
|         - | 7726 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 7727 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 7728 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 7729 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 7730 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 7731 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 7732 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 7733 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 7734 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 7735 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 7736 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 7737 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 7738 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 7739 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 7740 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 7741 | `	  /* Release info */` |
|         - | 7742 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 7743 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 7744 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - | 7745 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - | 7746 | `	{"get_extension_funcs",   vm_builtin_get_extension_funcs   },` |
|         - | 7747 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 7748 | `	{"php_ini_loaded_file",   vm_builtin_php_ini_loaded_file },` |
|         - | 7749 | `	{"php_ini_scanned_files", vm_builtin_php_ini_loaded_file },` |
|         - | 7750 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 7751 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 7752 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 7753 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 7754 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 7755 | `	  /* hashmap */` |
|         - | 7756 | `	{"compact",          vm_builtin_compact       },` |
|         - | 7757 | `	{"extract",          vm_builtin_extract       },` |
|         - | 7758 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 7759 | `	  /* URL related function */` |
|         - | 7760 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 7761 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 7762 | `	   /* Command line processing */` |
|         - | 7763 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 7764 | `	   /* JSON encoding/decoding */` |
|         - | 7765 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 7766 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 7767 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 7768 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 7769 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 7770 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 7771 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 7772 | `	   /* Files/URI inclusion facility */` |
|         - | 7773 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 7774 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 7775 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 7776 | ``	/* php's alias: the same list under the name a `require` reader`` |
|         - | 7777 | `	 * reaches for. */` |
|         - | 7778 | `	{ "get_required_files",vm_builtin_get_included_files},` |
|         - | 7779 | `	{ "include",      vm_builtin_include          },` |
|         - | 7780 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 7781 | `	{ "require",      vm_builtin_require          },` |
|         - | 7782 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 7783 | `};` |
|         - | 7784 | `/*` |
|         - | 7785 | ` * Register the built-in VM functions defined above.` |
|         - | 7786 | ` */` |
|      5619 | 7787 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 7788 | `{` |
|         - | 7789 | `	sxi32 rc;` |
|         - | 7790 | `	sxu32 n;` |
|    769808 | 7791 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 7792 | `		/* Note that these special functions have access` |
|         - | 7793 | `		 * to the underlying virtual machine as their` |
|         - | 7794 | `		 * private data.` |
|         - | 7795 | `		 */` |
|    764189 | 7796 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    764189 | 7797 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 7798 | `			return rc;` |
|         - | 7799 | `		}` |
|    381485 | 7800 | `	}` |
|      5624 | 7801 | `	return SXRET_OK;` |
|      2810 | 7802 | `}` |
|         - | 7803 | `/*` |
|         - | 7804 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 7805 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 7806 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 7807 | ` */` |
|   9127031 | 7808 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 7809 | `{` |
|   9127036 | 7810 | `	if( !iLoadable ){` |
|   6527564 | 7811 | `		return pClass;` |
|         - | 7812 | `	}` |
|   2599493 | 7813 | `	while(pClass){` |
|   2599477 | 7814 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   2599461 | 7815 | `			return pClass;` |
|         - | 7816 | `		}` |
|        17 | 7817 | `		pClass = pClass->pNextName;` |
|         1 | 7818 | `	}` |
|        17 | 7819 | `	return 0;` |
|   4561905 | 7820 | `}` |
|         - | 7821 | `/*` |
|         - | 7822 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 7823 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 7824 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 7825 | ` * registered in the VM's class table.` |
|         - | 7826 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 7827 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 7828 | ` */` |
|       646 | 7829 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 7830 | `{` |
|         - | 7831 | `	VmAutoloadCB *pEntry;` |
|         - | 7832 | `	ph7_value sArg,sResult;` |
|         - | 7833 | `	SyHashEntry *pHashEntry;` |
|         - | 7834 | `	ph7_class *pClass;` |
|         - | 7835 | `	sxu32 n,nEntry;` |
|       651 | 7836 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       651 | 7837 | `	if( nEntry < 1 ){` |
|       469 | 7838 | `		return 0;` |
|         - | 7839 | `	}` |
|         - | 7840 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       187 | 7841 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 7842 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 7843 | `	}` |
|         - | 7844 | `	/* Mark this class as being autoloaded */` |
|       185 | 7845 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 7846 | `	/* Prepare the class name argument */` |
|       185 | 7847 | `	PH7_MemObjInit(pVm,&sArg);` |
|       185 | 7848 | `	PH7_MemObjInit(pVm,&sResult);` |
|       185 | 7849 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       185 | 7850 | `	pClass = 0;` |
|       341 | 7851 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 7852 | `		ph7_value *apArg[1];` |
|       199 | 7853 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       199 | 7854 | `		if( pEntry == 0 ){` |
|       ! 0 | 7855 | `			continue;` |
|         - | 7856 | `		}` |
|       199 | 7857 | `		apArg[0] = &sArg;` |
|         - | 7858 | `		/* NOT PH7_VmCallUserFunction: that wrapper marks the dispatch as an internal` |
|         - | 7859 | `		 * function reaching for a callback, which binds the argument weakly. php's` |
|         - | 7860 | `		 * autoload call is the one such dispatch that is NOT weak -- it reads the` |
|         - | 7861 | `		 * strict_types of the code whose class reference triggered it, and says so in` |
|         - | 7862 | `		 * its own diagnostic ("called in <that file> on line <that line>"). An` |
|         - | 7863 | ``		 * autoloader declaring anything but `string` therefore RAISES under a strict`` |
|         - | 7864 | ``		 * caller, where PHL coerced the class name (`bool $c` got true) and ran the`` |
|         - | 7865 | `		 * loader on a value that no longer named anything. */` |
|       199 | 7866 | `		if( PH7_VmCallUserFunctionWithMap(pVm,&pEntry->sCallback,1,apArg,&sResult,0) != SXRET_OK ){` |
|         - | 7867 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        27 | 7868 | `			continue;` |
|         - | 7869 | `		}` |
|         - | 7870 | `		/* Check if the class is now available */` |
|       175 | 7871 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       175 | 7872 | `		if( pHashEntry ){` |
|        43 | 7873 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        43 | 7874 | `			if( pClass ){` |
|        43 | 7875 | `				break;` |
|         - | 7876 | `			}` |
|       ! 0 | 7877 | `		}` |
|        71 | 7878 | `	}` |
|       185 | 7879 | `	PH7_MemObjRelease(&sArg);` |
|       185 | 7880 | `	PH7_MemObjRelease(&sResult);` |
|         - | 7881 | `	/* Remove reentrancy guard */` |
|       185 | 7882 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       185 | 7883 | `	return pClass;` |
|       328 | 7884 | `}` |
|         - | 7885 | `/*` |
|         - | 7886 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 7887 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 7888 | ` */` |
|        66 | 7889 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         4 | 7890 | `{` |
|        70 | 7891 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         4 | 7892 | `}` |
|         - | 7893 | `/*` |
|         - | 7894 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 7895 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 7896 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 7897 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 7898 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 7899 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 7900 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 7901 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 7902 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 7903 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 7904 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 7905 | ` */` |
|   9668799 | 7906 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 7907 | `{` |
|   9668804 | 7908 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        84 | 7909 | `		(*pzName)++;` |
|        84 | 7910 | `		(*pnByte)--;` |
|        40 | 7911 | `	}` |
|   9668804 | 7912 | `}` |
|         - | 7913 | `/*` |
|         - | 7914 | ` * Check if the given name refer to an installed class.` |
|         - | 7915 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 7916 | ` */` |
|   9127583 | 7917 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 7918 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 7919 | `	const char *zName,  /* Name of the target class */` |
|         - | 7920 | `	sxu32 nByte,        /* zName length */` |
|         - | 7921 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 7922 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 7923 | `						 */` |
|         - | 7924 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 7925 | `	)` |
|         5 | 7926 | `{` |
|         - | 7927 | `	SyHashEntry *pEntry;` |
|         - | 7928 | `	ph7_class *pClass;` |
|   4562176 | 7929 | `	SXUNUSED(iNest);` |
|         - | 7930 | `	/* Exact class lookup.` |
|         - | 7931 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 7932 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 7933 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 7934 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   9127588 | 7935 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 7936 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|         - | 7937 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|   9127588 | 7938 | `	if( nByte < 1 ){` |
|        12 | 7939 | `		return 0;` |
|         - | 7940 | `	}` |
|   9127578 | 7941 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   9127578 | 7942 | `	if( pEntry == 0 ){` |
|         - | 7943 | `		/* Class not found in hash table — try autoload before giving up */` |
|       585 | 7944 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         - | 7945 | `	}` |
|   9126998 | 7946 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   9126998 | 7947 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   4562181 | 7948 | `}` |
|         - | 7949 | `/*` |
|         - | 7950 | ` * Reference Table Implementation` |
|         - | 7951 | ` * Status: stable <chm@symisc.net>` |
|         - | 7952 | ` * Intro` |
|         - | 7953 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 7954 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 7955 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 7956 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 7957 | ` *  Refer to the official for more information on this powerful` |
|         - | 7958 | ` *  extension.` |
|         - | 7959 | ` */` |
|         - | 7960 | `/*` |
|         - | 7961 | ` * ---------------------------------------------------------------------------` |
|         - | 7962 | ` * The reference table.` |
|         - | 7963 | ` *` |
|         - | 7964 | ` * One TAGGED WORD per memory-object slot (pVm->apRefObj[nIdx]; see VM_REF_TAG_*` |
|         - | 7965 | ` * in ph7int.h for the five shapes). The table answers one question -- who still` |
|         - | 7966 | ` * holds this slot -- and that answer decides when a value is freed, so every` |
|         - | 7967 | ` * accessor below is asked BY SLOT INDEX: a slot whose answer fits in its word has` |
|         - | 7968 | ` * no record for a caller to hold on to.` |
|         - | 7969 | ` *` |
|         - | 7970 | ` * A record (VmRefObj) is the fallback for the answers a word cannot carry: two or` |
|         - | 7971 | ` * more names, two or more nodes, or a pin standing beside a named holder.` |
|         - | 7972 | ` * ---------------------------------------------------------------------------` |
|         - | 7973 | ` */` |
|         - | 7974 | `/* The word for a slot the table has been grown to cover; 0 otherwise. */` |
| 143557796 | 7975 | `static void * VmRefWord(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 7976 | `{` |
| 143557801 | 7977 | `	if( nIdx >= pVm->nRefSize ){` |
|         2 | 7978 | `		return 0;` |
|         - | 7979 | `	}` |
| 143557799 | 7980 | `	return pVm->apRefObj[nIdx];` |
|  71760194 | 7981 | `}` |
|         - | 7982 | `/*` |
|         - | 7983 | ` * Grow the table to cover a slot. Doubling keeps the growth amortized and, unlike` |
|         - | 7984 | ` * the hash table this replaced, nothing has to be MOVED: the cells that exist keep` |
|         - | 7985 | ` * their index, so the copy is one memcpy and the tail is zeroed.` |
|         - | 7986 | ` */` |
|  23572676 | 7987 | `static sxi32 VmRefTableGrow(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 7988 | `{` |
|         - | 7989 | `	void **apNew;` |
|         - | 7990 | `	sxu32 nNew;` |
|  23572681 | 7991 | `	if( nIdx < pVm->nRefSize ){` |
|  23565602 | 7992 | `		return SXRET_OK;` |
|         - | 7993 | `	}` |
|      7084 | 7994 | `	nNew = pVm->nRefSize ? pVm->nRefSize : 0x10;` |
|     14165 | 7995 | `	while( nIdx >= nNew ){` |
|      7086 | 7996 | `		nNew <<= 1;` |
|         5 | 7997 | `	}` |
|      7084 | 7998 | `	apNew = (void **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(void *) * nNew);` |
|      7084 | 7999 | `	if( apNew == 0 ){` |
|       ! 0 | 8000 | `		return SXERR_MEM;` |
|         - | 8001 | `	}` |
|      7084 | 8002 | `	if( pVm->nRefSize > 0 ){` |
|      7084 | 8003 | `		SyMemcpy((const void *)pVm->apRefObj,(void *)apNew,pVm->nRefSize * sizeof(void *));` |
|      3527 | 8004 | `	}` |
|      7084 | 8005 | `	SyZero((void *)&apNew[pVm->nRefSize],(nNew - pVm->nRefSize) * sizeof(void *));` |
|      7084 | 8006 | `	SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|      7084 | 8007 | `	pVm->apRefObj = apNew;` |
|      7084 | 8008 | `	pVm->nRefSize = nNew;` |
|      7084 | 8009 | `	return SXRET_OK;` |
|  11783187 | 8010 | `}` |
|         - | 8011 | `/*` |
|         - | 8012 | ` * Store a word. The ONE place nRefUsed moves: it counts FILLED CELLS, so a cell` |
|         - | 8013 | ` * that goes from one shape to another (a name dropped to a bare mark, a word` |
|         - | 8014 | ` * promoted to a record) does not move it. The caller must already have grown the` |
|         - | 8015 | ` * table -- this cannot fail, which is what lets the callers below commit a state` |
|         - | 8016 | ` * change and a holder in the same breath.` |
|         - | 8017 | ` */` |
|  68450406 | 8018 | `static void VmRefWordSet(ph7_vm *pVm,sxu32 nIdx,void *pWord)` |
|         5 | 8019 | `{` |
|  68450411 | 8020 | `	void *pOld = pVm->apRefObj[nIdx];` |
|  68450411 | 8021 | `	if( pOld == 0 && pWord != 0 ){` |
|  23400031 | 8022 | `		pVm->nRefUsed++;` |
|  56747411 | 8023 | `	}else if( pOld != 0 && pWord == 0 ){` |
|  22490132 | 8024 | `		pVm->nRefUsed--;` |
|  11242671 | 8025 | `	}` |
|  68450411 | 8026 | `	pVm->apRefObj[nIdx] = pWord;` |
|  68450411 | 8027 | `}` |
|         - | 8028 | `/* A holder pointer carried in a word, with its tag taken back off. */` |
|         - | 8029 | `#define VM_REF_UNTAG(W,T) ((void *)&((char *)(W))[-(T)])` |
|         - | 8030 | `/*` |
|         - | 8031 | ` * May this pointer be tagged? The pool allocator keeps every chunk 8-aligned (the` |
|         - | 8032 | ` * C library's own alignment, a SyMemBlock that is a multiple of 8, and a` |
|         - | 8033 | ` * pointer-sized SyMemHeader -- see the alignment note on sxmem.c's OS methods), so` |
|         - | 8034 | ` * this is true everywhere it is asked -- but a word whose low bits are not free` |
|         - | 8035 | ` * would read back as another shape entirely, so the question is asked rather than` |
|         - | 8036 | ` * assumed and a stray pointer simply takes the record path.` |
|         - | 8037 | ` */` |
|  12961054 | 8038 | `static int VmRefTaggable(void *pPtr)` |
|         5 | 8039 | `{` |
|  12961059 | 8040 | `	return pPtr != 0 && (SX_PTR_TO_INT(pPtr) & VM_REF_TAG_MASK) == 0;` |
|         5 | 8041 | `}` |
|         - | 8042 | `/* Build a MARK word out of a pin count and the flags. */` |
|  32915232 | 8043 | `static void * VmRefMarkWord(sxu32 nPin,sxi32 iFlags)` |
|         5 | 8044 | `{` |
|  32915237 | 8045 | `	int iWord = VM_REF_TAG_MARK;` |
|  32915237 | 8046 | `	if( iFlags & VM_REF_IDX_KEEP ){` |
|  10439287 | 8047 | `		iWord \|= VM_REF_MARK_KEEP;` |
|   5219031 | 8048 | `	}` |
|  32915237 | 8049 | `	iWord \|= (int)(nPin * VM_REF_MARK_PIN);` |
|  32915237 | 8050 | `	return SX_INT_TO_PTR(iWord);` |
|         5 | 8051 | `}` |
|  33738729 | 8052 | `static sxu32 VmRefMarkPin(void *pWord)` |
|         5 | 8053 | `{` |
|  33738734 | 8054 | `	return ((sxu32)SX_PTR_TO_INT(pWord)) / VM_REF_MARK_PIN;` |
|         5 | 8055 | `}` |
|  56444701 | 8056 | `static int VmRefMarkKeep(void *pWord)` |
|         5 | 8057 | `{` |
|  56444706 | 8058 | `	return (SX_PTR_TO_INT(pWord) & VM_REF_MARK_KEEP) != 0;` |
|         5 | 8059 | `}` |
|         - | 8060 | `/*` |
|         - | 8061 | ` * Allocate a new reference record.` |
|         - | 8062 | ` *` |
|         - | 8063 | ` * Reached only by a slot whose holders will not fit in its word -- two names, two` |
|         - | 8064 | ` * nodes, or a pin beside a named holder. It used to be allocated for EVERY variable` |
|         - | 8065 | ` * a frame binds and EVERY element an array inserts, which made its size the engine's` |
|         - | 8066 | ` * per-value memory overhead.` |
|         - | 8067 | ` */` |
|     83993 | 8068 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8069 | `{` |
|         - | 8070 | `	VmRefObj *pRef;` |
|     83998 | 8071 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|     83998 | 8072 | `	if( pRef == 0 ){` |
|       ! 0 | 8073 | `		return 0;` |
|         - | 8074 | `	}` |
|         - | 8075 | `	/* Zero the structure */` |
|     83998 | 8076 | `	SyZero(pRef,sizeof(VmRefObj));` |
|     83998 | 8077 | `	pRef->nIdx = nIdx;` |
|     83998 | 8078 | `	return pRef;` |
|     41921 | 8079 | `}` |
|         - | 8080 | `/*` |
|         - | 8081 | ` * The spill sets of a record that has just been given a second holder, created on` |
|         - | 8082 | ` * demand. NULL on OOM, in which case the caller drops the row -- the same` |
|         - | 8083 | ` * degradation SySetPut's own failure already produced.` |
|         - | 8084 | ` */` |
|      2614 | 8085 | `static VmRefSpill * VmRefSpillGet(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 8086 | `{` |
|         - | 8087 | `	VmRefSpill *pSpill;` |
|      2619 | 8088 | `	if( pRef->pSpill ){` |
|      1984 | 8089 | `		return pRef->pSpill;` |
|         - | 8090 | `	}` |
|       640 | 8091 | `	pSpill = (VmRefSpill *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefSpill));` |
|       640 | 8092 | `	if( pSpill == 0 ){` |
|       ! 0 | 8093 | `		return 0;` |
|         - | 8094 | `	}` |
|       640 | 8095 | `	SySetInit(&pSpill->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|       640 | 8096 | `	SySetInit(&pSpill->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|       640 | 8097 | `	pRef->pSpill = pSpill;` |
|       640 | 8098 | `	return pSpill;` |
|      1310 | 8099 | `}` |
|         - | 8100 | `/*` |
|         - | 8101 | ` * File one holder on a record, ignoring a name that is already on it -- a name can be` |
|         - | 8102 | ` * RE-BOUND to the same slot any number of times, and a table that only ever grew made` |
|         - | 8103 | ` * both the install and the holder count O(rows).` |
|         - | 8104 | ` */` |
|     22842 | 8105 | `static void VmRefAddEntry(ph7_vm *pVm,VmRefObj *pRef,SyHashEntry *pEntry)` |
|         5 | 8106 | `{` |
|         - | 8107 | `	VmRefSpill *pSpill;` |
|         - | 8108 | `	SyHashEntry **apEntry;` |
|     22847 | 8109 | `	sxu32 n, nFree = SXU32_HIGH;` |
|     22847 | 8110 | `	if( pRef->pEntry0 == pEntry ){` |
|       ! 0 | 8111 | `		return;` |
|         - | 8112 | `	}` |
|     22847 | 8113 | `	if( pRef->pEntry0 == 0 ){` |
|     20803 | 8114 | `		pRef->pEntry0 = pEntry;` |
|     20803 | 8115 | `		return;` |
|         - | 8116 | `	}` |
|      2049 | 8117 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|      2049 | 8118 | `	if( pSpill == 0 ){` |
|       ! 0 | 8119 | `		return;` |
|         - | 8120 | `	}` |
|      2049 | 8121 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|      3828 | 8122 | `	for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; ++n ){` |
|      1784 | 8123 | `		if( apEntry[n] == pEntry ){` |
|       ! 0 | 8124 | `			return; /* already recorded: never file one holder twice */` |
|         - | 8125 | `		}` |
|      1784 | 8126 | `		if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|      1778 | 8127 | `			nFree = n; /* a row a dead holder left behind */` |
|       885 | 8128 | `		}` |
|       893 | 8129 | `	}` |
|      2049 | 8130 | `	if( nFree != SXU32_HIGH ){` |
|      1778 | 8131 | `		apEntry[nFree] = pEntry;` |
|       890 | 8132 | `	}else{` |
|       276 | 8133 | `		SySetPut(&pSpill->aReference,(const void *)&pEntry);` |
|         - | 8134 | `	}` |
|     11393 | 8135 | `}` |
|     62575 | 8136 | `static void VmRefAddNode(ph7_vm *pVm,VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|         5 | 8137 | `{` |
|         - | 8138 | `	VmRefSpill *pSpill;` |
|         - | 8139 | `	ph7_hashmap_node **apNode;` |
|     62580 | 8140 | `	sxu32 n, nFree = SXU32_HIGH;` |
|     62580 | 8141 | `	if( pRef->pNode0 == pNode ){` |
|       ! 0 | 8142 | `		return;` |
|         - | 8143 | `	}` |
|     62580 | 8144 | `	if( pRef->pNode0 == 0 ){` |
|     62010 | 8145 | `		pRef->pNode0 = pNode;` |
|     62010 | 8146 | `		return;` |
|         - | 8147 | `	}` |
|       575 | 8148 | `	pSpill = VmRefSpillGet(&(*pVm),pRef);` |
|       575 | 8149 | `	if( pSpill == 0 ){` |
|       ! 0 | 8150 | `		return;` |
|         - | 8151 | `	}` |
|       575 | 8152 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|       813 | 8153 | `	for( n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|       240 | 8154 | `		if( apNode[n] == pNode ){` |
|       ! 0 | 8155 | `			return;` |
|         - | 8156 | `		}` |
|       240 | 8157 | `		if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|         3 | 8158 | `			nFree = n;` |
|         1 | 8159 | `		}` |
|       121 | 8160 | `	}` |
|       575 | 8161 | `	if( nFree != SXU32_HIGH ){` |
|         3 | 8162 | `		apNode[nFree] = pNode;` |
|         2 | 8163 | `	}else{` |
|       573 | 8164 | `		SySetPut(&pSpill->aArrEntries,(const void *)&pNode);` |
|         - | 8165 | `	}` |
|     31243 | 8166 | `}` |
|         - | 8167 | `/*` |
|         - | 8168 | ` * Drop one holder from a record. Every row that names it goes, inline or spilled:` |
|         - | 8169 | ` * the table has never promised a holder appears once, and the count below reads` |
|         - | 8170 | ` * whatever is left.` |
|         - | 8171 | ` */` |
|     17161 | 8172 | `static void VmRefDropEntry(VmRefObj *pRef,SyHashEntry *pEntry)` |
|         5 | 8173 | `{` |
|     17166 | 8174 | `	if( pRef->pEntry0 == pEntry ){` |
|     15090 | 8175 | `		pRef->pEntry0 = 0;` |
|      7538 | 8176 | `	}` |
|     17166 | 8177 | `	if( pRef->pSpill ){` |
|      2437 | 8178 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|         - | 8179 | `		sxu32 n;` |
|      4641 | 8180 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|      2209 | 8181 | `			if( apEntry[n] == pEntry ){` |
|      2035 | 8182 | `				apEntry[n] = 0;` |
|      1013 | 8183 | `			}` |
|      1105 | 8184 | `		}` |
|      1214 | 8185 | `	}` |
|     17166 | 8186 | `}` |
|     14099 | 8187 | `static void VmRefDropNode(VmRefObj *pRef,ph7_hashmap_node *pNode)` |
|         5 | 8188 | `{` |
|     14104 | 8189 | `	if( pRef->pNode0 == pNode ){` |
|     13662 | 8190 | `		pRef->pNode0 = 0;` |
|      6824 | 8191 | `	}` |
|     14104 | 8192 | `	if( pRef->pSpill ){` |
|       833 | 8193 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|         - | 8194 | `		sxu32 n;` |
|      2077 | 8195 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|      1247 | 8196 | `			if( apNode[n] == pNode ){` |
|       445 | 8197 | `				apNode[n] = 0;` |
|       221 | 8198 | `			}` |
|       625 | 8199 | `		}` |
|       415 | 8200 | `	}` |
|     14104 | 8201 | `}` |
|         - | 8202 | `/*` |
|         - | 8203 | ` * How many LIVE holders of each kind a RECORD still carries. A node counts only while` |
|         - | 8204 | ` * it still points HERE -- a slot index travels through the free list, so a record can` |
|         - | 8205 | ` * outlive the node that filed the row.` |
|         - | 8206 | ` */` |
|     21122 | 8207 | `static sxu32 VmRefEntryCount(VmRefObj *pRef)` |
|         5 | 8208 | `{` |
|     21127 | 8209 | `	sxu32 n, nLive = pRef->pEntry0 ? 1 : 0;` |
|     21127 | 8210 | `	if( pRef->pSpill ){` |
|      1197 | 8211 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->pSpill->aReference);` |
|      1467 | 8212 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aReference) ; ++n ){` |
|       274 | 8213 | `			if( apEntry[n] ){` |
|        59 | 8214 | `				nLive++;` |
|        28 | 8215 | `			}` |
|       139 | 8216 | `		}` |
|       596 | 8217 | `	}` |
|     21127 | 8218 | `	return nLive;` |
|         5 | 8219 | `}` |
|     17647 | 8220 | `static sxu32 VmRefNodeCount(VmRefObj *pRef,sxu32 nIdx)` |
|         5 | 8221 | `{` |
|     17652 | 8222 | `	sxu32 n, nLive = (pRef->pNode0 && pRef->pNode0->nValIdx == nIdx) ? 1 : 0;` |
|     17652 | 8223 | `	if( pRef->pSpill ){` |
|      1090 | 8224 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->pSpill->aArrEntries);` |
|      2556 | 8225 | `		for( n = 0 ; n < SySetUsed(&pRef->pSpill->aArrEntries) ; ++n ){` |
|      1470 | 8226 | `			if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|       680 | 8227 | `				nLive++;` |
|       338 | 8228 | `			}` |
|       737 | 8229 | `		}` |
|       543 | 8230 | `	}` |
|     17652 | 8231 | `	return nLive;` |
|         5 | 8232 | `}` |
|         - | 8233 | `/*` |
|         - | 8234 | ` * The RECORD behind a slot, or 0 when the slot's answer is in its word (which,` |
|         - | 8235 | ` * unlike the old VmRefObjExtract, does NOT mean the slot is unheld). Nothing` |
|         - | 8236 | ` * outside this section may hold one: every question is asked by index below.` |
|         - | 8237 | ` */` |
|       200 | 8238 | `static VmRefObj * VmRefFull(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 8239 | `{` |
|       202 | 8240 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|       202 | 8241 | `	if( pWord == 0 \|\| VM_REF_TAGOF(pWord) != VM_REF_TAG_FULL ){` |
|       ! 0 | 8242 | `		return 0;` |
|         - | 8243 | `	}` |
|       202 | 8244 | `	return (VmRefObj *)pWord;` |
|       102 | 8245 | `}` |
|         - | 8246 | `/*` |
|         - | 8247 | ` * Promote whatever a slot has into a real record, creating one if the slot has` |
|         - | 8248 | ` * nothing yet. 0 on OOM (the table cannot be grown, or the record cannot be` |
|         - | 8249 | ` * allocated), which every caller degrades to "the slot records nothing", exactly` |
|         - | 8250 | ` * as a failed install always has.` |
|         - | 8251 | ` */` |
|     87233 | 8252 | `static VmRefObj * VmRefMaterialize(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8253 | `{` |
|         - | 8254 | `	void *pWord;` |
|         - | 8255 | `	VmRefObj *pRef;` |
|         - | 8256 | `	int iTag;` |
|     87238 | 8257 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|       ! 0 | 8258 | `		return 0;` |
|         - | 8259 | `	}` |
|     87238 | 8260 | `	pWord = pVm->apRefObj[nIdx];` |
|     87238 | 8261 | `	iTag = pWord ? VM_REF_TAGOF(pWord) : VM_REF_TAG_MARK;` |
|     87238 | 8262 | `	if( pWord != 0 && iTag == VM_REF_TAG_FULL ){` |
|      3245 | 8263 | `		return (VmRefObj *)pWord;` |
|         - | 8264 | `	}` |
|     83998 | 8265 | `	pRef = VmNewRefObj(&(*pVm),nIdx);` |
|     83998 | 8266 | `	if( pRef == 0 ){` |
|       ! 0 | 8267 | `		return 0;` |
|         - | 8268 | `	}` |
|     83998 | 8269 | `	if( pWord != 0 ){` |
|     83998 | 8270 | `		switch( iTag ){` |
|     31652 | 8271 | `		case VM_REF_TAG_NAME:` |
|     63210 | 8272 | `			pRef->pEntry0 = (SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME);` |
|     63210 | 8273 | `			break;` |
|     10336 | 8274 | `		case VM_REF_TAG_NODE:` |
|     20615 | 8275 | `			pRef->pNode0 = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|     20615 | 8276 | `			break;` |
|        89 | 8277 | `		default: /* VM_REF_TAG_MARK */` |
|       182 | 8278 | `			pRef->nPin = VmRefMarkPin(pWord);` |
|       182 | 8279 | `			pRef->iFlags = VmRefMarkKeep(pWord) ? VM_REF_IDX_KEEP : 0;` |
|       178 | 8280 | `			break;` |
|         - | 8281 | `		}` |
|     41916 | 8282 | `	}` |
|     83998 | 8283 | `	VmRefWordSet(&(*pVm),nIdx,(void *)pRef);` |
|     83998 | 8284 | `	return pRef;` |
|     43535 | 8285 | `}` |
|         - | 8286 | `/*` |
|         - | 8287 | ` * ---------------------------------------------------------------------------` |
|         - | 8288 | ` * The questions, all asked by slot index.` |
|         - | 8289 | ` * ---------------------------------------------------------------------------` |
|         - | 8290 | ` */` |
|         - | 8291 | `/* Has anything ever been registered against this slot? A slot that answers NO has` |
|         - | 8292 | ` * never been in the table at all, which is what keeps it out of the free pool. */` |
|  35650295 | 8293 | `PH7_PRIVATE int PH7_VmSlotRegistered(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8294 | `{` |
|  35650300 | 8295 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8296 | `		return 0;` |
|         - | 8297 | `	}` |
|  35650300 | 8298 | `	return VmRefWord(&(*pVm),nIdx) != 0;` |
|  17819767 | 8299 | `}` |
|         - | 8300 | `/* The names bound to the slot. */` |
|   1647342 | 8301 | `PH7_PRIVATE sxu32 PH7_VmSlotEntryCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8302 | `{` |
|         - | 8303 | `	void *pWord;` |
|   1647347 | 8304 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8305 | `		return 0;` |
|         - | 8306 | `	}` |
|   1647347 | 8307 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   1647347 | 8308 | `	if( pWord == 0 ){` |
|       ! 0 | 8309 | `		return 0;` |
|         - | 8310 | `	}` |
|   1647347 | 8311 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 8312 | `	case VM_REF_TAG_NAME: return 1;` |
|    822170 | 8313 | `	case VM_REF_TAG_NODE: /* fall through */` |
|   1643857 | 8314 | `	case VM_REF_TAG_MARK: return 0;` |
|      3492 | 8315 | `	default:              return VmRefEntryCount((VmRefObj *)pWord);` |
|         - | 8316 | `	}` |
|    823430 | 8317 | `}` |
|         - | 8318 | `/* The array nodes still pointing HERE -- a node that has moved on does not count. */` |
|   1528244 | 8319 | `PH7_PRIVATE sxu32 PH7_VmSlotNodeCount(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 8320 | `{` |
|         - | 8321 | `	void *pWord;` |
|   1528246 | 8322 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8323 | `		return 0;` |
|         - | 8324 | `	}` |
|   1528246 | 8325 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   1528246 | 8326 | `	if( pWord == 0 ){` |
|       ! 0 | 8327 | `		return 0;` |
|         - | 8328 | `	}` |
|   1528246 | 8329 | `	switch( VM_REF_TAGOF(pWord) ){` |
|         3 | 8330 | `	case VM_REF_TAG_NODE: {` |
|         7 | 8331 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|         7 | 8332 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|         - | 8333 | `	}` |
|    764113 | 8334 | `	case VM_REF_TAG_NAME: /* fall through */` |
|   1528228 | 8335 | `	case VM_REF_TAG_MARK: return 0;` |
|        13 | 8336 | `	default:              return VmRefNodeCount((VmRefObj *)pWord,nIdx);` |
|         - | 8337 | `	}` |
|    764124 | 8338 | `}` |
|         - | 8339 | `/* The counted pins (a reference-bound property, one per binding). */` |
|   1649358 | 8340 | `PH7_PRIVATE sxu32 PH7_VmSlotPinCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8341 | `{` |
|         - | 8342 | `	void *pWord;` |
|   1649363 | 8343 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8344 | `		return 0;` |
|         - | 8345 | `	}` |
|   1649363 | 8346 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   1649363 | 8347 | `	if( pWord == 0 ){` |
|       ! 0 | 8348 | `		return 0;` |
|         - | 8349 | `	}` |
|   1649363 | 8350 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|   1530137 | 8351 | `		return VmRefMarkPin(pWord);` |
|         - | 8352 | `	}` |
|    119231 | 8353 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|      3608 | 8354 | `		return ((VmRefObj *)pWord)->nPin;` |
|         - | 8355 | `	}` |
|    115625 | 8356 | `	return 0;` |
|    824438 | 8357 | `}` |
|         - | 8358 | `/* The permanent pin (VM_REF_IDX_KEEP): a use(&$x) capture, a static, an enum case,` |
|         - | 8359 | ` * and the hold a declared property has on its own value slot. */` |
|  34116781 | 8360 | `PH7_PRIVATE int PH7_VmSlotKeepPinned(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8361 | `{` |
|         - | 8362 | `	void *pWord;` |
|  34116786 | 8363 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8364 | `		return 0;` |
|         - | 8365 | `	}` |
|  34116786 | 8366 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  34116786 | 8367 | `	if( pWord == 0 ){` |
|       ! 0 | 8368 | `		return 0;` |
|         - | 8369 | `	}` |
|  34116786 | 8370 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|  33966682 | 8371 | `		return VmRefMarkKeep(pWord);` |
|         - | 8372 | `	}` |
|    150109 | 8373 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|     34486 | 8374 | `		return (((VmRefObj *)pWord)->iFlags & VM_REF_IDX_KEEP) != 0;` |
|         - | 8375 | `	}` |
|    115625 | 8376 | `	return 0;` |
|  17053011 | 8377 | `}` |
|         - | 8378 | `/*` |
|         - | 8379 | ` * Is pNode the FIRST node filed against this slot, and the only live one? The cycle` |
|         - | 8380 | ` * collector's "is this element held by its own array and nothing else" test; the` |
|         - | 8381 | ` * first-filed row is the one an ordinary insert leaves, so any other answer means` |
|         - | 8382 | ` * somebody else is pointing at the same slot.` |
|         - | 8383 | ` */` |
|    115667 | 8384 | `PH7_PRIVATE int PH7_VmSlotSoleNodeIs(ph7_vm *pVm,sxu32 nIdx,ph7_hashmap_node *pNode)` |
|         2 | 8385 | `{` |
|         - | 8386 | `	void *pWord;` |
|    115669 | 8387 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8388 | `		return 0;` |
|         - | 8389 | `	}` |
|    115669 | 8390 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|    115669 | 8391 | `	if( pWord == 0 ){` |
|       ! 0 | 8392 | `		return 0;` |
|         - | 8393 | `	}` |
|    115669 | 8394 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_NODE ){` |
|    173194 | 8395 | `		return VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pNode` |
|    115623 | 8396 | `			&& pNode->nValIdx == nIdx;` |
|         - | 8397 | `	}` |
|        45 | 8398 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|        45 | 8399 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|        45 | 8400 | `		return pRef->pNode0 == pNode && VmRefNodeCount(pRef,nIdx) == 1;` |
|         - | 8401 | `	}` |
|       ! 0 | 8402 | `	return 0;` |
|     57593 | 8403 | `}` |
|         - | 8404 | `/*` |
|         - | 8405 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 8406 | ` * names bound to it plus the array nodes pointing at it, plus the holders the` |
|         - | 8407 | ` * table cannot name. php refcounts a reference set and keeps the VALUE alive` |
|         - | 8408 | ` * while any holder remains, so this is the count every "may I release this` |
|         - | 8409 | ` * slot?" decision asks for.` |
|         - | 8410 | ` */` |
|  23850164 | 8411 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8412 | `{` |
|         - | 8413 | `	void *pWord;` |
|  23850169 | 8414 | `	sxu32 nLive = 0;` |
|  23850169 | 8415 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8416 | `		return 0;` |
|         - | 8417 | `	}` |
|  23850169 | 8418 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  23850169 | 8419 | `	if( pWord == 0 ){` |
|         3 | 8420 | `		return 0;` |
|         - | 8421 | `	}` |
|  23850167 | 8422 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 8423 | `	case VM_REF_TAG_NAME:` |
|       ! 0 | 8424 | `		return 1;` |
|    677355 | 8425 | `	case VM_REF_TAG_NODE: {` |
|   1354682 | 8426 | `		ph7_hashmap_node *pNode = (ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE);` |
|   1354682 | 8427 | `		return pNode->nValIdx == nIdx ? 1 : 0;` |
|         - | 8428 | `	}` |
|  11241313 | 8429 | `	case VM_REF_TAG_MARK: {` |
|  22477855 | 8430 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|  22477855 | 8431 | `		if( nPin > 0 ){` |
|         5 | 8432 | `			return nPin;` |
|         - | 8433 | `		}` |
|         - | 8434 | ``		/* A permanent pin -- a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 8435 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 8436 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 8437 | `		 * php's refcount does. */` |
|  22477851 | 8438 | `		return VmRefMarkKeep(pWord) ? 1 : 0;` |
|         - | 8439 | `	}` |
|      8822 | 8440 | `	default: {` |
|     17640 | 8441 | `		VmRefObj *pRef = (VmRefObj *)pWord;` |
|     17640 | 8442 | `		if( pRef->nPin > 0 ){` |
|        96 | 8443 | `			nLive += pRef->nPin;` |
|     17593 | 8444 | `		}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         5 | 8445 | `			nLive++;` |
|         2 | 8446 | `		}` |
|     17640 | 8447 | `		nLive += VmRefEntryCount(pRef);` |
|     17640 | 8448 | `		nLive += VmRefNodeCount(pRef,nIdx);` |
|     17640 | 8449 | `		return nLive;` |
|         - | 8450 | `	}` |
|         - | 8451 | `	}` |
|  11922678 | 8452 | `}` |
|         - | 8453 | `/*` |
|         - | 8454 | ` * Does this slot's holder count include the OWNER's own hold?` |
|         - | 8455 | ` *` |
|         - | 8456 | ` * A property's slot is installed in the reference table with a permanent pin` |
|         - | 8457 | ` * (VM_REF_IDX_KEEP) when the property was created dynamically or re-created after` |
|         - | 8458 | ` * unset(), with a COUNTED pin when the property was BOUND to somebody else's slot` |
|         - | 8459 | `` * (`$o->p =& $x`), and with nothing at all when it came straight from the class`` |
|         - | 8460 | ` * declaration. PH7_VmSlotHolderCount counts those pins as holders, so a renderer` |
|         - | 8461 | ` * asking "is this value a REFERENCE" has to subtract the one hold that is the` |
|         - | 8462 | ` * property itself -- an array ELEMENT, which is its own first holder in the table,` |
|         - | 8463 | ` * asks the same question with a threshold of two.` |
|         - | 8464 | ` */` |
|      5425 | 8465 | `PH7_PRIVATE int PH7_VmSlotSelfPinned(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8466 | `{` |
|      5430 | 8467 | `	return PH7_VmSlotPinCount(&(*pVm),nIdx) > 0 \|\| PH7_VmSlotKeepPinned(&(*pVm),nIdx);` |
|         5 | 8468 | `}` |
|         - | 8469 | `/*` |
|         - | 8470 | ` * Delete every holder a slot has and empty its cell.` |
|         - | 8471 | ` *` |
|         - | 8472 | ` * Each row is cleared BEFORE the call that acts on it: unlinking a node runs the` |
|         - | 8473 | ` * value's release, which can re-enter this table for the same slot, and a row still` |
|         - | 8474 | ` * filled when that happens is a holder being handed out twice. The base pointer is` |
|         - | 8475 | ` * re-read per row for the same reason -- a re-entrant install may have grown the set` |
|         - | 8476 | ` * out from under it.` |
|         - | 8477 | ` */` |
|  22490127 | 8478 | `PH7_PRIVATE void PH7_VmSlotUnlink(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8479 | `{` |
|         - | 8480 | `	VmRefSpill *pSpill;` |
|         - | 8481 | `	VmRefObj *pRef;` |
|         - | 8482 | `	void *pWord;` |
|         - | 8483 | `	sxu32 n;` |
|  22490132 | 8484 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  22490132 | 8485 | `	if( pWord == 0 ){` |
|       ! 0 | 8486 | `		return;` |
|         - | 8487 | `	}` |
|         - | 8488 | `	/* Empty the cell first: everything below can re-enter the table for this slot. */` |
|  22490132 | 8489 | `	VmRefWordSet(&(*pVm),nIdx,0);` |
|  22490132 | 8490 | `	switch( VM_REF_TAGOF(pWord) ){` |
|       ! 0 | 8491 | `	case VM_REF_TAG_NAME:` |
|       ! 0 | 8492 | `		SyHashDeleteEntry2((SyHashEntry *)VM_REF_UNTAG(pWord,VM_REF_TAG_NAME));` |
|       ! 0 | 8493 | `		return;` |
|       168 | 8494 | `	case VM_REF_TAG_NODE:` |
|       336 | 8495 | `		PH7_HashmapUnlinkNode((ph7_hashmap_node *)VM_REF_UNTAG(pWord,VM_REF_TAG_NODE),FALSE);` |
|       336 | 8496 | `		return;` |
|  11240390 | 8497 | `	case VM_REF_TAG_MARK:` |
|  22476009 | 8498 | `		return; /* pins and flags name nothing to delete */` |
|      6898 | 8499 | `	default:` |
|     13787 | 8500 | `		break;` |
|         - | 8501 | `	}` |
|     13792 | 8502 | `	pRef = (VmRefObj *)pWord;` |
|     13792 | 8503 | `	if( pRef->pEntry0 ){` |
|       167 | 8504 | `		SyHashEntry *pEntry = pRef->pEntry0;` |
|       167 | 8505 | `		pRef->pEntry0 = 0;` |
|       167 | 8506 | `		SyHashDeleteEntry2(pEntry);` |
|        83 | 8507 | `	}` |
|     13792 | 8508 | `	if( pRef->pNode0 ){` |
|       167 | 8509 | `		ph7_hashmap_node *pNode = pRef->pNode0;` |
|       167 | 8510 | `		pRef->pNode0 = 0;` |
|       167 | 8511 | `		PH7_HashmapUnlinkNode(pNode,FALSE);` |
|        83 | 8512 | `	}` |
|     13792 | 8513 | `	pSpill = pRef->pSpill;` |
|     13792 | 8514 | `	if( pSpill ){` |
|       505 | 8515 | `		for( n = 0 ; n < SySetUsed(&pSpill->aReference) ; n++ ){` |
|       139 | 8516 | `			SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pSpill->aReference);` |
|       139 | 8517 | `			SyHashEntry *pEntry = apEntry[n];` |
|       139 | 8518 | `			if( pEntry ){` |
|       ! 0 | 8519 | `				apEntry[n] = 0;` |
|       ! 0 | 8520 | `				SyHashDeleteEntry2(pEntry);` |
|       ! 0 | 8521 | `			}` |
|        71 | 8522 | `		}` |
|       667 | 8523 | `		for(n = 0 ; n < SySetUsed(&pSpill->aArrEntries) ; ++n ){` |
|       300 | 8524 | `			ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pSpill->aArrEntries);` |
|       300 | 8525 | `			ph7_hashmap_node *pNode = apNode[n];` |
|       300 | 8526 | `			if( pNode ){` |
|       ! 0 | 8527 | `				apNode[n] = 0;` |
|       ! 0 | 8528 | `				PH7_HashmapUnlinkNode(pNode,FALSE);` |
|       ! 0 | 8529 | `			}` |
|       151 | 8530 | `		}` |
|       369 | 8531 | `		SySetRelease(&pRef->pSpill->aReference);` |
|       369 | 8532 | `		SySetRelease(&pRef->pSpill->aArrEntries);` |
|       369 | 8533 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRef->pSpill);` |
|       369 | 8534 | `		pRef->pSpill = 0;` |
|       183 | 8535 | `	}` |
|     13792 | 8536 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  11242676 | 8537 | `}` |
|         - | 8538 | `/*` |
|         - | 8539 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 8540 | ` *` |
|         - | 8541 | ` * iFlags is applied only when the slot has NOTHING registered against it yet; a slot` |
|         - | 8542 | ` * already in the table keeps the flags it has (VmPinMemObjSlot is the door that adds` |
|         - | 8543 | ` * one). That has always been the rule -- it is now spelled out because the word for a` |
|         - | 8544 | ` * fresh slot is chosen from it.` |
|         - | 8545 | ` *` |
|         - | 8546 | ` * The implementation of the reference mechanism in the PH7 engine differ greatly from` |
|         - | 8547 | ` * the one used by the zend engine. That is, the reference implementation is` |
|         - | 8548 | ` * consistent,solid and it's behavior resemble the C++ reference mechanism.` |
|         - | 8549 | ` */` |
|  23485443 | 8550 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 8551 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 8552 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 8553 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 8554 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 8555 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 8556 | `	)` |
|         5 | 8557 | `{` |
|         - | 8558 | `	VmFrame *pFrame;` |
|         - | 8559 | `	VmRefObj *pRef;` |
|         - | 8560 | `	void *pWord;` |
|         - | 8561 | `	/* Cover the slot up front: everything below commits without a way to fail, and a` |
|         - | 8562 | `	 * table that cannot be grown records nothing at all -- the same degradation a` |
|         - | 8563 | `	 * failed record allocation has always produced. */` |
|  23485448 | 8564 | `	if( VmRefTableGrow(&(*pVm),nIdx) != SXRET_OK ){` |
|       ! 0 | 8565 | `		return SXERR_MEM;` |
|         - | 8566 | `	}` |
|  23485448 | 8567 | `	pFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|  23485448 | 8568 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 8569 | `		VmSlot sRef;` |
|         - | 8570 | `		/* Local frame,record referenced entry so that it can` |
|         - | 8571 | `		 * be deleted when we leave this frame.` |
|         - | 8572 | `		 */` |
|   1263711 | 8573 | `		sRef.nIdx = nIdx;` |
|   1263711 | 8574 | `		sRef.pUserData = pEntry;` |
|   1263711 | 8575 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 8576 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 8577 | `		}` |
|    632802 | 8578 | `	}` |
|  23485448 | 8579 | `	pWord = pVm->apRefObj[nIdx];` |
|  23485448 | 8580 | `	if( pWord == 0 ){` |
|         - | 8581 | `		/* A slot nothing has claimed yet. This is the common case by three orders of` |
|         - | 8582 | `		 * magnitude, and every shape of it fits in the word. */` |
|  23400031 | 8583 | `		if( iFlags == 0 && pEntry != 0 && pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|   1320533 | 8584 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|   1320533 | 8585 | `			return SXRET_OK;` |
|         - | 8586 | `		}` |
|  22079503 | 8587 | `		if( iFlags == 0 && pMapEntry != 0 && pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
|  11640531 | 8588 | `			VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
|  11640531 | 8589 | `			return SXRET_OK;` |
|         - | 8590 | `		}` |
|  10438977 | 8591 | `		if( pEntry == 0 && pMapEntry == 0 && (iFlags & ~VM_REF_IDX_KEEP) == 0 ){` |
|         - | 8592 | `			/* A pin with no named holder -- every declared property's own hold on its` |
|         - | 8593 | `			 * value slot -- and the empty registration a dropped holder leaves behind. */` |
|  10438977 | 8594 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,iFlags));` |
|  10438977 | 8595 | `			return SXRET_OK;` |
|         - | 8596 | `		}` |
|       ! 0 | 8597 | `	}else{` |
|     85422 | 8598 | `		switch( VM_REF_TAGOF(pWord) ){` |
|     31004 | 8599 | `		case VM_REF_TAG_NAME:` |
|         - | 8600 | `			/* A name RE-BOUND to the slot it already names changes nothing, which is` |
|         - | 8601 | `			 * the frame-local variable written to in a loop. */` |
|     61909 | 8602 | `			if( pMapEntry == 0` |
|     30923 | 8603 | `			 && (pEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry) ){` |
|       ! 0 | 8604 | `				return SXRET_OK;` |
|         - | 8605 | `			}` |
|     61914 | 8606 | `			break;` |
|     10321 | 8607 | `		case VM_REF_TAG_NODE:` |
|     20580 | 8608 | `			if( pEntry == 0` |
|     10373 | 8609 | `			 && (pMapEntry == 0 \|\| VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry) ){` |
|       ! 0 | 8610 | `				return SXRET_OK;` |
|         - | 8611 | `			}` |
|     20585 | 8612 | `			break;` |
|        89 | 8613 | `		case VM_REF_TAG_MARK:` |
|       182 | 8614 | `			if( pEntry == 0 && pMapEntry == 0 ){` |
|       ! 0 | 8615 | `				return SXRET_OK; /* iFlags is ignored on a slot already registered */` |
|         - | 8616 | `			}` |
|       182 | 8617 | `			if( SX_PTR_TO_INT(pWord) == VM_REF_TAG_MARK ){` |
|         - | 8618 | `				/* A bare mark says "registered, held by nothing"; a holder on top of it` |
|         - | 8619 | `				 * says exactly what the pointer words say. */` |
|       ! 0 | 8620 | `				if( pMapEntry == 0 && VmRefTaggable(pEntry) ){` |
|       ! 0 | 8621 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pEntry)[VM_REF_TAG_NAME]);` |
|       ! 0 | 8622 | `					return SXRET_OK;` |
|         - | 8623 | `				}` |
|       ! 0 | 8624 | `				if( pEntry == 0 && VmRefTaggable(pMapEntry) ){` |
|       ! 0 | 8625 | `					VmRefWordSet(&(*pVm),nIdx,(void *)&((char *)pMapEntry)[VM_REF_TAG_NODE]);` |
|       ! 0 | 8626 | `					return SXRET_OK;` |
|         - | 8627 | `				}` |
|       ! 0 | 8628 | `			}` |
|       178 | 8629 | `			break;` |
|      1377 | 8630 | `		default:` |
|      2750 | 8631 | `			break;` |
|         - | 8632 | `		}` |
|         - | 8633 | `	}` |
|         - | 8634 | `	/* More than one word can say. */` |
|     85422 | 8635 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|     85422 | 8636 | `	if( pRef == 0 ){` |
|       ! 0 | 8637 | `		return SXERR_MEM;` |
|         - | 8638 | `	}` |
|     85422 | 8639 | `	if( pWord == 0 ){` |
|       ! 0 | 8640 | `		pRef->iFlags = iFlags;` |
|       ! 0 | 8641 | `	}` |
|     85422 | 8642 | `	if( pEntry ){` |
|         - | 8643 | `		/* The name bound to this slot */` |
|     22847 | 8644 | `		VmRefAddEntry(&(*pVm),pRef,pEntry);` |
|     11388 | 8645 | `	}` |
|     85422 | 8646 | `	if( pMapEntry ){` |
|         - | 8647 | `		/* The hashmap node [i.e: Array entry] pointing at it */` |
|     62580 | 8648 | `		VmRefAddNode(&(*pVm),pRef,pMapEntry);` |
|     31238 | 8649 | `	}` |
|     85422 | 8650 | `	return SXRET_OK;` |
|  11739657 | 8651 | `}` |
|         - | 8652 | `/*` |
|         - | 8653 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 8654 | ` *` |
|         - | 8655 | ` * The slot stays REGISTERED with no holders (a bare mark) rather than leaving the` |
|         - | 8656 | ` * table: that is what still returns the index to the free pool when the value goes.` |
|         - | 8657 | ` */` |
|  12776955 | 8658 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 8659 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 8660 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 8661 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 8662 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 8663 | `	)` |
|         5 | 8664 | `{` |
|         - | 8665 | `	VmRefObj *pRef;` |
|         - | 8666 | `	void *pWord;` |
|  12776960 | 8667 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|  12776960 | 8668 | `	if( pWord == 0 ){` |
|         - | 8669 | `		/* Not such entry */` |
|         5 | 8670 | `		return SXERR_NOTFOUND;` |
|         - | 8671 | `	}` |
|  12776956 | 8672 | `	switch( VM_REF_TAGOF(pWord) ){` |
|    627579 | 8673 | `	case VM_REF_TAG_NAME:` |
|   1257065 | 8674 | `		if( pEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NAME) == (void *)pEntry ){` |
|   1257065 | 8675 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|    629481 | 8676 | `		}` |
|   1257065 | 8677 | `		return SXRET_OK;` |
|   5747057 | 8678 | `	case VM_REF_TAG_NODE:` |
|  11488636 | 8679 | `		if( pMapEntry != 0 && VM_REF_UNTAG(pWord,VM_REF_TAG_NODE) == (void *)pMapEntry ){` |
|  11488636 | 8680 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|   5741574 | 8681 | `		}` |
|  11488636 | 8682 | `		return SXRET_OK;` |
|       ! 0 | 8683 | `	case VM_REF_TAG_MARK:` |
|       ! 0 | 8684 | `		return SXRET_OK; /* nothing named to drop */` |
|     15641 | 8685 | `	default:` |
|     31260 | 8686 | `		break;` |
|         - | 8687 | `	}` |
|     31265 | 8688 | `	pRef = (VmRefObj *)pWord;` |
|         - | 8689 | `	/* Remove the desired entry */` |
|     31265 | 8690 | `	if( pEntry ){` |
|     17166 | 8691 | `		VmRefDropEntry(pRef,pEntry);` |
|      8574 | 8692 | `	}` |
|     31265 | 8693 | `	if( pMapEntry ){` |
|     14104 | 8694 | `		VmRefDropNode(pRef,pMapEntry);` |
|      7045 | 8695 | `	}` |
|     31265 | 8696 | `	return SXRET_OK;` |
|   6386681 | 8697 | `}` |
|         - | 8698 | `/*` |
|         - | 8699 | `` * Pin a slot past its frame: a `use (&$x)` capture, a static, an enum case, a`` |
|         - | 8700 | ` * reference-bound property. A pin is a holder the table cannot NAME.` |
|         - | 8701 | ` */` |
|      2024 | 8702 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8703 | `{` |
|         - | 8704 | `	void *pWord;` |
|         - | 8705 | `	VmRefObj *pRef;` |
|      2029 | 8706 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|      2029 | 8707 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|      2029 | 8708 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|       150 | 8709 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(VmRefMarkPin(pWord),VM_REF_IDX_KEEP));` |
|       150 | 8710 | `		return;` |
|         - | 8711 | `	}` |
|      1881 | 8712 | `	if( pWord == 0 ){` |
|         - | 8713 | `		/* No record yet -- a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - | 8714 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        64 | 8715 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|        64 | 8716 | `		return;` |
|         - | 8717 | `	}` |
|      1821 | 8718 | `	pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|      1821 | 8719 | `	if( pRef ){` |
|      1821 | 8720 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       904 | 8721 | `	}` |
|      1013 | 8722 | `}` |
|         - | 8723 | `/*` |
|         - | 8724 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - | 8725 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - | 8726 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - | 8727 | ` */` |
|       200 | 8728 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 8729 | `{` |
|         - | 8730 | `	void *pWord;` |
|         - | 8731 | `	VmRefObj *pRef;` |
|       202 | 8732 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|       202 | 8733 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|       202 | 8734 | `	if( pWord != 0 && VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|       152 | 8735 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|       152 | 8736 | `		if( nPin < VM_REF_MARK_PINMAX ){` |
|       152 | 8737 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin + 1,VM_REF_IDX_KEEP));` |
|       152 | 8738 | `			return;` |
|         - | 8739 | `		}` |
|       ! 0 | 8740 | `	}` |
|        51 | 8741 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|        51 | 8742 | `	if( pRef == 0 && pWord != 0 ){` |
|       ! 0 | 8743 | `		pRef = VmRefMaterialize(&(*pVm),nIdx);` |
|       ! 0 | 8744 | `	}` |
|        51 | 8745 | `	if( pRef ){` |
|        51 | 8746 | `		pRef->nPin++;` |
|        25 | 8747 | `	}` |
|       102 | 8748 | `}` |
|         - | 8749 | `/*` |
|         - | 8750 | ` * Give back a counted pin. The slot goes when it was the last holder -- without this the` |
|         - | 8751 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - | 8752 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - | 8753 | ` */` |
|       190 | 8754 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         2 | 8755 | `{` |
|       192 | 8756 | `	void *pWord = VmRefWord(&(*pVm),nIdx);` |
|         - | 8757 | `	VmRefObj *pRef;` |
|       192 | 8758 | `	if( pWord == 0 ){` |
|       ! 0 | 8759 | `		return;` |
|         - | 8760 | `	}` |
|       192 | 8761 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|        41 | 8762 | `		sxu32 nPin = VmRefMarkPin(pWord);` |
|        41 | 8763 | `		if( nPin < 1 ){` |
|       ! 0 | 8764 | `			return;` |
|         - | 8765 | `		}` |
|        41 | 8766 | `		nPin--;` |
|        41 | 8767 | `		VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(nPin,nPin < 1 ? 0 : VM_REF_IDX_KEEP));` |
|        41 | 8768 | `		if( nPin < 1 ){` |
|        29 | 8769 | `			PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        14 | 8770 | `		}` |
|        41 | 8771 | `		return;` |
|         - | 8772 | `	}` |
|       152 | 8773 | `	pRef = VmRefFull(&(*pVm),nIdx);` |
|       152 | 8774 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 | 8775 | `		return;` |
|         - | 8776 | `	}` |
|       152 | 8777 | `	pRef->nPin--;` |
|       152 | 8778 | `	if( pRef->nPin < 1 ){` |
|       144 | 8779 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|       144 | 8780 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        71 | 8781 | `	}` |
|        97 | 8782 | `}` |
|         - | 8783 | `/*` |
|         - | 8784 | ` * Give up the OWNER's own hold on a slot, and say whether anybody else still has one.` |
|         - | 8785 | ` *` |
|         - | 8786 | `` * A property that is released -- with its object, or by `unset($o->p)` -- used to`` |
|         - | 8787 | ` * take its VALUE SLOT with it unconditionally, which is right only while the` |
|         - | 8788 | ` * property is the one thing naming it. php's refcount keeps the value alive for` |
|         - | 8789 | `` * whoever else holds a reference to it (`$r =& $o->p; unset($o->p);` leaves $r`` |
|         - | 8790 | `` * holding the value, and `$a[] =& $o->p` leaves the element), where unlinking the`` |
|         - | 8791 | ` * slot here dropped the array element and left the VARIABLE undefined.` |
|         - | 8792 | ` *` |
|         - | 8793 | ` * Returns TRUE when the caller must NOT free the slot. The owner's own hold is the` |
|         - | 8794 | ` * permanent pin a dynamically created / re-created property carries; a declared one` |
|         - | 8795 | ` * holds nothing at all, so there is nothing to give back for it.` |
|         - | 8796 | ` */` |
|   9730249 | 8797 | `PH7_PRIVATE int PH7_VmSlotDropOwnerHold(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8798 | `{` |
|         - | 8799 | `	void *pWord;` |
|         - | 8800 | `	VmRefObj *pRef;` |
|   9730254 | 8801 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8802 | `		return 0;` |
|         - | 8803 | `	}` |
|   9730254 | 8804 | `	pWord = VmRefWord(&(*pVm),nIdx);` |
|   9730254 | 8805 | `	if( pWord == 0 ){` |
|       ! 0 | 8806 | `		return 0;` |
|         - | 8807 | `	}` |
|   9730254 | 8808 | `	if( VM_REF_TAGOF(pWord) == VM_REF_TAG_MARK ){` |
|   9730236 | 8809 | `		if( VmRefMarkPin(pWord) == 0 ){` |
|   9730236 | 8810 | `			VmRefWordSet(&(*pVm),nIdx,VmRefMarkWord(0,0));` |
|   4864523 | 8811 | `		}` |
|   4864537 | 8812 | `	}else if( VM_REF_TAGOF(pWord) == VM_REF_TAG_FULL ){` |
|        19 | 8813 | `		pRef = (VmRefObj *)pWord;` |
|        19 | 8814 | `		if( pRef->nPin == 0 ){` |
|        19 | 8815 | `			pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|         9 | 8816 | `		}` |
|         9 | 8817 | `	}` |
|         - | 8818 | `	/* A word carrying a name or a node has no pin to give back; its holder is the` |
|         - | 8819 | `	 * one the count below reports. */` |
|   9730254 | 8820 | `	return PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0;` |
|   4864532 | 8821 | `}` |
|         - | 8822 | `/*` |
|         - | 8823 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 8824 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 8825 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 8826 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 8827 | ` */` |
|  11506196 | 8828 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 8829 | `{` |
|  11506201 | 8830 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 8831 | `		return;` |
|         - | 8832 | `	}` |
|  11506201 | 8833 | `	if( PH7_VmSlotRegistered(&(*pVm),nIdx) ){` |
|  11506155 | 8834 | `		if( PH7_VmSlotKeepPinned(&(*pVm),nIdx) ){` |
|       114 | 8835 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 8836 | `		}` |
|  11506043 | 8837 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      3690 | 8838 | `			return; /* somebody still holds it */` |
|         - | 8839 | `		}` |
|   5748430 | 8840 | `	}` |
|         - | 8841 | `	/* Nothing registered at all means nothing was ever recorded against the slot,` |
|         - | 8842 | `	 * which is the same answer as a count of zero — release it (this is what every` |
|         - | 8843 | `	 * caller did unconditionally before the holder rule). */` |
|  11502403 | 8844 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 8845 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 8846 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|  11502403 | 8847 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   5750357 | 8848 | `}` |
|         - | 8849 | `/*` |
|         - | 8850 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 8851 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 8852 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 8853 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 8854 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 8855 | ` */` |
|     13737 | 8856 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 8857 | `{` |
|         - | 8858 | `	VmFrame *pFrame;` |
|     30797 | 8859 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     17060 | 8860 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     17060 | 8861 | `		sxu32 n = 0;` |
|     30542 | 8862 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     13485 | 8863 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 8864 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 8865 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      3221 | 8866 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      3221 | 8867 | `				(void)SySetPop(&pFrame->sRef);` |
|      3221 | 8868 | `				continue;` |
|         - | 8869 | `			}` |
|     10266 | 8870 | `			n++;` |
|         2 | 8871 | `		}` |
|      8528 | 8872 | `	}` |
|     13742 | 8873 | `}` |
|         - | 8874 | `/*` |
|         - | 8875 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 8876 | ` *` |
|         - | 8877 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 8878 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 8879 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 8880 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 8881 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 8882 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 8883 | ` */` |
|     29901 | 8884 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 8885 | `{` |
|     29906 | 8886 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 8887 | `}` |
|         - | 8888 | `/*` |
|         - | 8889 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 8890 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 8891 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 8892 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 8893 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 8894 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 8895 | ` */` |
|      2246 | 8896 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 8897 | `	sxu32 nIdx)` |
|         5 | 8898 | `{` |
|      2251 | 8899 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|      2251 | 8900 | `	if( pEntry ){` |
|       116 | 8901 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|       116 | 8902 | `		return;` |
|         - | 8903 | `	}` |
|      2139 | 8904 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 8905 | `		return;` |
|         - | 8906 | `	}` |
|         - | 8907 | `	/* The name may already be memoized against the slot it had before this frame` |
|         - | 8908 | ``	 * installed it -- a by-reference `foreach` re-binds its value variable on every`` |
|         - | 8909 | `	 * step, and the first step is an INSERT. */` |
|      2139 | 8910 | `	VmVarMemoFlush(pFrame);` |
|      2139 | 8911 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 8912 | `		/* A global is also an entry of the $GLOBALS view */` |
|        42 | 8913 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 8914 | `	}` |
|      2139 | 8915 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|      1126 | 8916 | `}` |
|         - | 8917 | `/*` |
|         - | 8918 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 8919 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 8920 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 8921 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 8922 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 8923 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 8924 | ` * the wrong variable.` |
|         - | 8925 | ` *` |
|         - | 8926 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 8927 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 8928 | ` * compares it by identity.` |
|         - | 8929 | ` */` |
|      3236 | 8930 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 8931 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 8932 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 8933 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 8934 | `	const char *zName,    /* Variable name */` |
|         - | 8935 | `	sxu32 nByte,          /* Name length */` |
|         - | 8936 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 8937 | `	)` |
|         4 | 8938 | `{` |
|      3240 | 8939 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      3240 | 8940 | `	if( nOld == nIdx ){` |
|         - | 8941 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|       ! 0 | 8942 | `		return;` |
|         - | 8943 | `	}` |
|         - | 8944 | `	/* This name now means another slot; every memo this frame holds goes. */` |
|      3240 | 8945 | `	VmVarMemoFlush(pFrame);` |
|         - | 8946 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 8947 | `	 * "release this reference at exit" set */` |
|      3240 | 8948 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      3240 | 8949 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      3240 | 8950 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      3240 | 8951 | `	if( pFrame->pParent == 0 ){` |
|         - | 8952 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 8953 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 8954 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 8955 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       150 | 8956 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       146 | 8957 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       150 | 8958 | `		 && pGlobalNode ){` |
|       150 | 8959 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       150 | 8960 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       150 | 8961 | `				pGlobalNode->nValIdx = nIdx;` |
|       150 | 8962 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        73 | 8963 | `			}` |
|        77 | 8964 | `		}else{` |
|         - | 8965 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 8966 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 8967 | `		}` |
|        73 | 8968 | `	}` |
|      3240 | 8969 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 8970 | `	/* The old value dies with its last holder — and only then */` |
|      3240 | 8971 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      1622 | 8972 | `}` |
|         - | 8973 | `/*` |
|         - | 8974 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|         - | 8975 | ` *` |
|         - | 8976 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|         - | 8977 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|         - | 8978 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|         - | 8979 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|         - | 8980 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|         - | 8981 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|         - | 8982 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|         - | 8983 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|         - | 8984 | ` */` |
|    257356 | 8985 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|         5 | 8986 | `{` |
|    257361 | 8987 | `	int i = 0;` |
|   1422242 | 8988 | `	while( i < nByte ){` |
|   1422152 | 8989 | `		int c = zIn[i];` |
|   1422147 | 8990 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|    258589 | 8991 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|   1164886 | 8992 | `			i++;` |
|   1164886 | 8993 | `			continue;` |
|         - | 8994 | `		}` |
|    257271 | 8995 | `		break;` |
|       ! 0 | 8996 | `	}` |
|         - | 8997 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|         - | 8998 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|    257361 | 8999 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|      2213 | 9000 | `		*pnScheme = i;` |
|      2213 | 9001 | `		return 1;` |
|         - | 9002 | `	}` |
|    255153 | 9003 | `	return 0;` |
|    128750 | 9004 | `}` |
|         - | 9005 | `/*` |
|         - | 9006 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|         - | 9007 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|         - | 9008 | ` * is walkable at all.` |
|         - | 9009 | ` */` |
|        64 | 9010 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|         3 | 9011 | `{` |
|        67 | 9012 | `	int nScheme = 0;` |
|        67 | 9013 | `	if( zIn == 0 ){` |
|       ! 0 | 9014 | `		return 0;` |
|         - | 9015 | `	}` |
|        67 | 9016 | `	if( nByte < 0 ){` |
|        39 | 9017 | `		nByte = (int)SyStrlen(zIn);` |
|        14 | 9018 | `	}` |
|        67 | 9019 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|        31 | 9020 | `}` |
|         - | 9021 | `/*` |
|         - | 9022 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|         - | 9023 | ` *` |
|         - | 9024 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|         - | 9025 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|         - | 9026 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|         - | 9027 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|         - | 9028 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|         - | 9029 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|         - | 9030 | `` * `file://` on its own is the root directory.`` |
|         - | 9031 | ` *` |
|         - | 9032 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|         - | 9033 | ` * wrapper", as php does.` |
|         - | 9034 | ` */` |
|        84 | 9035 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|         3 | 9036 | `{` |
|         - | 9037 | `	static const char zLocal[] = "file://localhost/";` |
|        87 | 9038 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|        87 | 9039 | `	const char *zEnd = &zIn[nByte];` |
|        84 | 9040 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|         - | 9041 | `#ifdef __WINNT__` |
|         - | 9042 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|         3 | 9043 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|         - | 9044 | `#endif` |
|         - | 9045 | `	){` |
|        36 | 9046 | `		if( nByte < (int)sizeof(zLocal)-1` |
|        38 | 9047 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|        34 | 9048 | `			return 0; /* a host this build (and php) will not fetch from */` |
|         - | 9049 | `		}` |
|         4 | 9050 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|         2 | 9051 | `	}` |
|       150 | 9052 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|        98 | 9053 | `		zPath++;` |
|         2 | 9054 | `	}` |
|        54 | 9055 | `	*pzPath = zPath;` |
|        54 | 9056 | `	return 1;` |
|        45 | 9057 | `}` |
|         - | 9058 | `/*` |
|         - | 9059 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|         - | 9060 | ` * going through a stream device. It carried a second, shorter copy of the` |
|         - | 9061 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|         - | 9062 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|         - | 9063 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|         - | 9064 | ` * that is not a path, which is the FALSE php answers for it.` |
|         - | 9065 | ` */` |
|     84640 | 9066 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|         5 | 9067 | `{` |
|         - | 9068 | `	const char *zOut;` |
|     84645 | 9069 | `	int nByte,nScheme = 0;` |
|     84645 | 9070 | `	if( zPath == 0 ){` |
|       ! 0 | 9071 | `		return zPath;` |
|         - | 9072 | `	}` |
|     84645 | 9073 | `	nByte = (int)SyStrlen(zPath);` |
|     84640 | 9074 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|     42499 | 9075 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|        60 | 9076 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|     84623 | 9077 | `		return zPath;` |
|         - | 9078 | `	}` |
|        25 | 9079 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|         6 | 9080 | `		return zPath;` |
|         - | 9081 | `	}` |
|         - | 9082 | `#ifdef __WINNT__` |
|         - | 9083 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|         - | 9084 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|         - | 9085 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|         2 | 9086 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|         2 | 9087 | `		zOut++;` |
|         - | 9088 | `	}` |
|         - | 9089 | `#endif` |
|        20 | 9090 | `	return zOut;` |
|     42465 | 9091 | `}` |
|         - | 9092 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 9093 | `/*` |
|         - | 9094 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|         - | 9095 | ` */` |
|    173491 | 9096 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|         5 | 9097 | `{` |
|    173496 | 9098 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|         - | 9099 | `	sxu32 n;` |
|    175714 | 9100 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|      2277 | 9101 | `		if( apOff[n] == pStream ){` |
|        58 | 9102 | `			return 1;` |
|         - | 9103 | `		}` |
|      1112 | 9104 | `	}` |
|    173440 | 9105 | `	return 0;` |
|     86677 | 9106 | `}` |
|         - | 9107 | `/*` |
|         - | 9108 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|         - | 9109 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|         - | 9110 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|         - | 9111 | ` * sit in the list, and the LIVE one is the later of the two.` |
|         - | 9112 | ` */` |
|    172600 | 9113 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|         5 | 9114 | `{` |
|    172605 | 9115 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|    172605 | 9116 | `	ph7_io_stream *pHit = 0;` |
|    172605 | 9117 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|    172605 | 9118 | `	if( nName < 0 ){` |
|       ! 0 | 9119 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 9120 | `	}` |
|   1729143 | 9121 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|   1556543 | 9122 | `		ph7_io_stream *pStream = apStream[n];` |
|   1556538 | 9123 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|   1208278 | 9124 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|   1383957 | 9125 | `			continue;` |
|         - | 9126 | `		}` |
|    172591 | 9127 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        19 | 9128 | `			continue;` |
|         - | 9129 | `		}` |
|    172573 | 9130 | `		pHit = pStream;` |
|     86220 | 9131 | `	}` |
|    172605 | 9132 | `	return pHit;` |
|         5 | 9133 | `}` |
|         - | 9134 | `/*` |
|         - | 9135 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|         - | 9136 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|         - | 9137 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|         - | 9138 | ` */` |
|         2 | 9139 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|         1 | 9140 | `{` |
|         3 | 9141 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|         3 | 9142 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|         3 | 9143 | `	int bSeen = 0;` |
|         3 | 9144 | `	if( nName < 0 ){` |
|       ! 0 | 9145 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 9146 | `	}` |
|        23 | 9147 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        21 | 9148 | `		ph7_io_stream *pStream = apStream[n];` |
|        20 | 9149 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|        16 | 9150 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|        19 | 9151 | `			continue;` |
|         - | 9152 | `		}` |
|         3 | 9153 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       ! 0 | 9154 | `			return 0; /* it is live */` |
|         - | 9155 | `		}` |
|         3 | 9156 | `		bSeen = 1;` |
|         2 | 9157 | `	}` |
|         3 | 9158 | `	return bSeen;` |
|         2 | 9159 | `}` |
|         - | 9160 | `/*` |
|         - | 9161 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 9162 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 9163 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 9164 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 9165 | ` * For more information on how to register IO stream devices,please` |
|         - | 9166 | ` * refer to the official documentation.` |
|         - | 9167 | ` */` |
|    172616 | 9168 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 9169 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 9170 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 9171 | `	int nByte              /* *pzDevice length*/` |
|         - | 9172 | `	)` |
|         5 | 9173 | `{` |
|         - | 9174 | `	const char *zIn,*zNext;` |
|         - | 9175 | `	ph7_io_stream *pStream;` |
|    172621 | 9176 | `	int nScheme = 0;` |
|         - | 9177 | `	/* A failed open names the URI the SCRIPT wrote, and every caller from here` |
|         - | 9178 | `	 * on holds only what is left after the scheme -- so both halves are` |
|         - | 9179 | `	 * remembered as the scheme comes off, and forgotten on every arm that does` |
|         - | 9180 | `	 * not take one off (see VfsThrowOpenWarning). */` |
|    172621 | 9181 | `	if( pVm->nOpenDepth < 1 ){` |
|    172473 | 9182 | `		pVm->zOpenUri = 0;` |
|    172473 | 9183 | `		pVm->zOpenUriTail = 0;` |
|    172473 | 9184 | `		pVm->nOpenUri = 0;` |
|         - | 9185 | `		/* The reason goes with them: a caller that resolves a device and then` |
|         - | 9186 | `		 * declines to open it (dom asks for xRead/xWrite first) must not report` |
|         - | 9187 | `		 * the PREVIOUS open's wrapper reason. */` |
|    172473 | 9188 | `		pVm->zOpenErr = 0;` |
|     86165 | 9189 | `	}` |
|         - | 9190 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|    172621 | 9191 | `	zIn = *pzDevice;` |
|    172621 | 9192 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|         - | 9193 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|         - | 9194 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|         - | 9195 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|         - | 9196 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|         - | 9197 | `		 * than answering pDefStream is what makes both true. */` |
|    170537 | 9198 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|         - | 9199 | `	}` |
|      2089 | 9200 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|         - | 9201 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|         - | 9202 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|         - | 9203 | `	 * file:// is handed the stripped path too. */` |
|      2089 | 9204 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|        53 | 9205 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|        18 | 9206 | `			return 0;` |
|         - | 9207 | `		}` |
|        17 | 9208 | `	}` |
|      2073 | 9209 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|      2073 | 9210 | `	if( pStream == 0 ){` |
|         - | 9211 | `		/* No such stream -- or one a script has taken out of service. */` |
|        34 | 9212 | `		return 0;` |
|         - | 9213 | `	}` |
|      2043 | 9214 | `	*pzDevice = zNext;` |
|      2043 | 9215 | `	if( pVm->nOpenDepth < 1 ){` |
|      2041 | 9216 | `		pVm->zOpenUri = zIn;` |
|      2041 | 9217 | `		pVm->nOpenUri = nByte;` |
|      2041 | 9218 | `		pVm->zOpenUriTail = zNext;` |
|      1001 | 9219 | `	}` |
|      2043 | 9220 | `	return pStream;` |
|     86244 | 9221 | `}` |
|         - | 9222 | `/*` |
|         - | 9223 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|         - | 9224 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|         - | 9225 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|         - | 9226 | ` * threaded out of the lookup, so every call site stays one line.` |
|         - | 9227 | ` *` |
|         - | 9228 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|         - | 9229 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|         - | 9230 | ` */` |
|        36 | 9231 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|         4 | 9232 | `{` |
|         - | 9233 | `	const char *zPath;` |
|        40 | 9234 | `	int nScheme = 0;` |
|        40 | 9235 | `	if( nByte < 0 ){` |
|        40 | 9236 | `		nByte = (int)SyStrlen(zUri);` |
|        18 | 9237 | `	}` |
|        40 | 9238 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|         3 | 9239 | `		*pnScheme = 0;` |
|         3 | 9240 | `		return 0;` |
|         - | 9241 | `	}` |
|        38 | 9242 | `	*pnScheme = nScheme;` |
|        45 | 9243 | `	return nScheme == (int)sizeof("file")-1` |
|        24 | 9244 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|        41 | 9245 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|        22 | 9246 | `}` |
|         - | 9247 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 9248 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 9249 |  |
