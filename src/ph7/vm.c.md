# src/ph7/vm.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2935/3416 lines (85.92%)

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
|   4310602 |   94 | `PH7_PRIVATE sxi32 VmIsUnorderedCmp(ph7_value *pLeft,ph7_value *pRight)` |
|         5 |   95 | `{` |
|   4310602 |   96 | `	if( (pLeft->iFlags \| pRight->iFlags)` |
|   4310607 |   97 | `	  & (MEMOBJ_NULL\|MEMOBJ_BOOL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ) ){` |
|       ! 0 |   98 | `		return FALSE;` |
|         - |   99 | `	}` |
|   4310607 |  100 | `	if( (pLeft->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pLeft->rVal) ){` |
|       334 |  101 | `		return TRUE;` |
|         - |  102 | `	}` |
|   4310275 |  103 | `	if( (pRight->iFlags & MEMOBJ_REAL) && PH7_IS_NAN(pRight->rVal) ){` |
|       145 |  104 | `		return TRUE;` |
|         - |  105 | `	}` |
|   4310131 |  106 | `	return FALSE;` |
|   2158523 |  107 | `}` |
|         - |  108 | `/*` |
|         - |  109 | ` * Return TRUE if the value should take the Perl-style string-increment path:` |
|         - |  110 | ` * any MEMOBJ_STRING that is empty, or whose contents are not a complete` |
|         - |  111 | ` * number (matching PHP's is_numeric semantics — the whole string must parse` |
|         - |  112 | ` * as a number, with optional surrounding whitespace).  Strings with a` |
|         - |  113 | ` * numeric prefix followed by non-whitespace bytes (e.g. "5foo") take the` |
|         - |  114 | ` * Perl path, like PHP.  Strict numeric strings ("5", "1.5", "5e2", "  5  ")` |
|         - |  115 | ` * still go through the existing numeric coercion.` |
|         - |  116 | ` */` |
|    697489 |  117 | `PH7_PRIVATE int VmStringWantsPerlIncr(ph7_value *pVal)` |
|         5 |  118 | `{` |
|         - |  119 | `	SyString sStr;` |
|    697494 |  120 | `	sxu8 bReal = FALSE;` |
|    697494 |  121 | `	const char *zTail = 0;` |
|         - |  122 | `	const char *zEnd;` |
|    697494 |  123 | `	if( (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    697476 |  124 | `		return FALSE;` |
|         - |  125 | `	}` |
|        20 |  126 | `	SyStringInitFromBuf(&sStr,SyBlobData(&pVal->sBlob),SyBlobLength(&pVal->sBlob));` |
|        20 |  127 | `	if( sStr.nByte == 0 ){` |
|       ! 0 |  128 | `		return TRUE;` |
|         - |  129 | `	}` |
|        20 |  130 | `	if( SyStrIsNumeric(sStr.zString,sStr.nByte,&bReal,&zTail) != SXRET_OK ){` |
|         5 |  131 | `		return TRUE;` |
|         - |  132 | `	}` |
|         - |  133 | `	/* SyStrIsNumeric accepts a leading numeric prefix; require the` |
|         - |  134 | `	 * remainder to be whitespace only so leading-numeric junk like "5foo"` |
|         - |  135 | `	 * still takes the Perl path. */` |
|        16 |  136 | `	zEnd = sStr.zString + sStr.nByte;` |
|        16 |  137 | `	while( zTail < zEnd && (unsigned char)*zTail < 0xc0 && SyisSpace(*zTail) ){` |
|       ! 0 |  138 | `		zTail++;` |
|       ! 0 |  139 | `	}` |
|        16 |  140 | `	return zTail < zEnd;` |
|    349179 |  141 | `}` |
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
|   6032686 |  159 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstant(` |
|         - |  160 | `	ph7_vm *pVm,            /* Target VM */` |
|         - |  161 | `	const SyString *pName,  /* Constant name */` |
|         - |  162 | `	ProcConstant xExpand,   /* Constant expansion callback */` |
|         - |  163 | `	void *pUserData         /* Last argument to xExpand() */` |
|         - |  164 | `	)` |
|         5 |  165 | `{` |
|   6032691 |  166 | `	return PH7_VmRegisterConstantEx(&(*pVm),pName,xExpand,pUserData,0,0,0);` |
|         5 |  167 | `}` |
|         - |  168 | `/*` |
|         - |  169 | ` * Like PH7_VmRegisterConstant, additionally recording the constant's` |
|         - |  170 | ` * origin (file/line/user-defined) for ReflectionConstant.` |
|         - |  171 | ` */` |
|   6032982 |  172 | `PH7_PRIVATE sxi32 PH7_VmRegisterConstantEx(` |
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
|   6032987 |  186 | `	pEntry = SyHashGet(&pVm->hConstant,(const void *)pName->zString,pName->nByte);` |
|   6032987 |  187 | `	if( pEntry ){` |
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
|         3 |  207 | `		SySetReset(&pCons->aAttrs); /* redefinition drops the old attributes */` |
|         3 |  208 | `		return SXRET_OK;` |
|         - |  209 | `	}` |
|         - |  210 | `	/* Allocate a new constant instance */` |
|   6032985 |  211 | `	pCons = (ph7_constant *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_constant));` |
|   6032985 |  212 | `	if( pCons == 0 ){` |
|       ! 0 |  213 | `		return 0;` |
|         - |  214 | `	}` |
|         - |  215 | `	/* Duplicate constant name */` |
|   6032985 |  216 | `	zDupName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   6032985 |  217 | `	if( zDupName == 0 ){` |
|       ! 0 |  218 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  219 | `		return 0;` |
|         - |  220 | `	}` |
|   6032985 |  221 | `	SyStringInitFromBuf(&pCons->sFile,0,0);` |
|   6032985 |  222 | `	if( pFile ){` |
|       299 |  223 | `		SyStringDupPtr(&pCons->sFile,pFile);` |
|       147 |  224 | `	}` |
|   6032985 |  225 | `	pCons->nLine = nLine;` |
|   6032985 |  226 | `	pCons->bUserDefined = (sxu8)(bUser ? 1 : 0);` |
|         - |  227 | `	/* Install the constant */` |
|   6032985 |  228 | `	SyStringInitFromBuf(&pCons->sName,zDupName,pName->nByte);` |
|   6032985 |  229 | `	pCons->xExpand = xExpand;` |
|   6032985 |  230 | `	pCons->pUserData = pUserData;` |
|   6032985 |  231 | `	SySetInit(&pCons->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   6032985 |  232 | `	rc = SyHashInsert(&pVm->hConstant,(const void *)zDupName,SyStringLength(&pCons->sName),pCons);` |
|   6032985 |  233 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  234 | `		SyMemBackendFree(&pVm->sAllocator,zDupName);` |
|       ! 0 |  235 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCons);` |
|       ! 0 |  236 | `		return rc;` |
|         - |  237 | `	}` |
|         - |  238 | `	/* All done,constant can be invoked from PHP code */` |
|   6032985 |  239 | `	return SXRET_OK;` |
|   3016496 |  240 | `}` |
|         - |  241 | `/*` |
|         - |  242 | ` * Allocate a new foreign function instance.` |
|         - |  243 | ` * This function return SXRET_OK on success. Any other` |
|         - |  244 | ` * return value indicates failure.` |
|         - |  245 | ` * Please refer to the official documentation for an introduction to` |
|         - |  246 | ` * the foreign function mechanism.` |
|         - |  247 | ` */` |
|   9066148 |  248 | `PH7_PRIVATE sxi32 PH7_NewForeignFunction(` |
|         - |  249 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  250 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  251 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  252 | `	void *pUserData,          /* Foreign function private data */` |
|         - |  253 | `	ph7_user_func **ppOut     /* OUT: VM image of the foreign function */` |
|         - |  254 | `	)` |
|         5 |  255 | `{` |
|         - |  256 | `	ph7_user_func *pFunc;` |
|         - |  257 | `	char *zDup;` |
|         - |  258 | `	/* Allocate a new user function */` |
|   9066153 |  259 | `	pFunc = (ph7_user_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_user_func));` |
|   9066153 |  260 | `	if( pFunc == 0 ){` |
|       ! 0 |  261 | `		return SXERR_MEM;` |
|         - |  262 | `	}` |
|         - |  263 | `	/* Duplicate function name */` |
|   9066153 |  264 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|   9066153 |  265 | `	if( zDup == 0 ){` |
|       ! 0 |  266 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  267 | `		return SXERR_MEM;` |
|         - |  268 | `	}` |
|         - |  269 | `	/* Zero the structure */` |
|   9066153 |  270 | `	SyZero(pFunc,sizeof(ph7_user_func));` |
|         - |  271 | `	/* Initialize structure fields */` |
|   9066153 |  272 | `	SyStringInitFromBuf(&pFunc->sName,zDup,pName->nByte);` |
|   9066153 |  273 | `	pFunc->pVm   = pVm;` |
|   9066153 |  274 | `	pFunc->xFunc = xFunc;` |
|   9066153 |  275 | `	pFunc->pUserData = pUserData;` |
|   9066153 |  276 | `	SySetInit(&pFunc->aAux,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - |  277 | `	/* Write a pointer to the new function */` |
|   9066153 |  278 | `	*ppOut = pFunc;` |
|   9066153 |  279 | `	return SXRET_OK;` |
|   4533079 |  280 | `}` |
|         - |  281 | `/*` |
|         - |  282 | ` * Install a foreign function and it's associated callback so that` |
|         - |  283 | ` * it can be invoked from the target PHP code.` |
|         - |  284 | ` * This function return SXRET_OK on successful registration. Any other` |
|         - |  285 | ` * return value indicates failure.` |
|         - |  286 | ` * Please refer to the official documentation for an introduction to` |
|         - |  287 | ` * the foreign function mechanism.` |
|         - |  288 | ` */` |
|   3727874 |  289 | `PH7_PRIVATE sxi32 PH7_VmInstallForeignFunction(` |
|         - |  290 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  291 | `	const SyString *pName,    /* Foreign function name */` |
|         - |  292 | `	ProchHostFunction xFunc,  /* Foreign function implementation */` |
|         - |  293 | `	void *pUserData           /* Foreign function private data */` |
|         - |  294 | `	)` |
|         5 |  295 | `{` |
|         - |  296 | `	ph7_user_func *pFunc;` |
|         - |  297 | `	SyHashEntry *pEntry;` |
|         - |  298 | `	sxi32 rc;` |
|         - |  299 | `	/* Overwrite any previously registered function with the same name */` |
|   3727879 |  300 | `	pEntry = SyHashGet(&pVm->hHostFunction,pName->zString,pName->nByte);` |
|   3727879 |  301 | `	if( pEntry ){` |
|       ! 0 |  302 | `		pFunc = (ph7_user_func *)pEntry->pUserData;` |
|       ! 0 |  303 | `		pFunc->pUserData = pUserData;` |
|       ! 0 |  304 | `		pFunc->xFunc = xFunc;` |
|       ! 0 |  305 | `		SySetReset(&pFunc->aAux);` |
|         - |  306 | `		/* A replacement implementation carries its own (unknown) arity, so drop` |
|         - |  307 | `		 * any minimum-arity metadata stamped on the previous holder of this name` |
|         - |  308 | `		 * — otherwise an embedder overriding a listed builtin (e.g. a 1-arg` |
|         - |  309 | `		 * custom "substr") would inherit the old ArgumentCountError threshold. */` |
|       ! 0 |  310 | `		pFunc->nMinArg  = 0;` |
|       ! 0 |  311 | `		pFunc->nMaxArg  = 0;` |
|       ! 0 |  312 | `		pFunc->bHasMaxArg = 0; /* no too-many-arguments check until a signature stamps one */` |
|       ! 0 |  313 | `		pFunc->bAtLeast = 0;` |
|       ! 0 |  314 | `		return SXRET_OK;` |
|         - |  315 | `	}` |
|         - |  316 | `	/* Create a new user function */` |
|   3727879 |  317 | `	rc = PH7_NewForeignFunction(&(*pVm),&(*pName),xFunc,pUserData,&pFunc);` |
|   3727879 |  318 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  319 | `		return rc;` |
|         - |  320 | `	}` |
|         - |  321 | `	/* Install the function in the corresponding hashtable */` |
|   3727879 |  322 | `	rc = SyHashInsert(&pVm->hHostFunction,SyStringData(&pFunc->sName),pName->nByte,pFunc);` |
|   3727879 |  323 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  324 | `		SyMemBackendFree(&pVm->sAllocator,(void *)SyStringData(&pFunc->sName));` |
|       ! 0 |  325 | `		SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|       ! 0 |  326 | `		return rc;` |
|         - |  327 | `	}` |
|         - |  328 | `	/* User function successfully installed */` |
|   3727879 |  329 | `	return SXRET_OK;` |
|   1863942 |  330 | `}` |
|         - |  331 | `/*` |
|         - |  332 | ` * Initialize a VM function.` |
|         - |  333 | ` */` |
|   5500548 |  334 | `PH7_PRIVATE sxi32 PH7_VmInitFuncState(` |
|         - |  335 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  336 | `	ph7_vm_func *pFunc, /* Target Fucntion */` |
|         - |  337 | `	const char *zName,  /* Function name */` |
|         - |  338 | `	sxu32 nByte,        /* zName length */` |
|         - |  339 | `	sxi32 iFlags,       /* Configuration flags */` |
|         - |  340 | `	void *pUserData     /* Function private data */` |
|         - |  341 | `	)` |
|         5 |  342 | `{` |
|         - |  343 | `	/* Zero the structure */` |
|   5500553 |  344 | `	SyZero(pFunc,sizeof(ph7_vm_func));` |
|         - |  345 | `	/* Initialize structure fields */` |
|         - |  346 | `	/* Arguments container */` |
|   5500553 |  347 | `	SySetInit(&pFunc->aArgs,&pVm->sAllocator,sizeof(ph7_vm_func_arg));` |
|         - |  348 | `	/* Static variable container */` |
|   5500553 |  349 | `	SySetInit(&pFunc->aStatic,&pVm->sAllocator,sizeof(ph7_vm_func_static_var));` |
|         - |  350 | `	/* Bytecode container */` |
|   5500553 |  351 | `	SySetInit(&pFunc->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|         - |  352 | `    /* Preallocate some instruction slots */` |
|   5500553 |  353 | `	SySetAlloc(&pFunc->aByteCode,0x10);` |
|         - |  354 | `	/* Closure environment */` |
|   5500553 |  355 | `	SySetInit(&pFunc->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|         - |  356 | `	/* Return-type union alternatives (empty unless declared as a union) */` |
|   5500553 |  357 | `	SySetInit(&pFunc->aReturnUnion,&pVm->sAllocator,sizeof(ph7_type_alt));` |
|         - |  358 | `	/* Declared #[...] attributes */` |
|   5500553 |  359 | `	SySetInit(&pFunc->aAttrs,&pVm->sAllocator,sizeof(ph7_attribute));` |
|   5500553 |  360 | `	pFunc->iFlags = iFlags;` |
|   5500553 |  361 | `	pFunc->pUserData = pUserData;` |
|         - |  362 | `	/* Capture the defining file's strict_types mode. PHP scopes return-type` |
|         - |  363 | `	 * coercion by the callee's file, so we freeze it at definition time. */` |
|   5500553 |  364 | `	pFunc->bStrictTypes = (sxu8)(pVm->sCodeGen.bStrictTypes ? 1 : 0);` |
|   5500553 |  365 | `	if( pVm->bCompilingBuiltin ){` |
|         - |  366 | `		/* Defined by an embedded builtin chunk: internal, no defining file */` |
|   5485181 |  367 | `		pFunc->iFlags \|= VM_FUNC_INTERNAL;` |
|   2742593 |  368 | `	}else{` |
|         - |  369 | `		/* Alias the VM-lifetime path dup on top of the include stack. eval()` |
|         - |  370 | `		 * chunks push nothing, so eval-defined functions report the includer's` |
|         - |  371 | `		 * file (documented divergence from PHP's "eval()'d code" pseudo-path). */` |
|     15377 |  372 | `		SyString *pFile = (SyString *)SySetPeek(&pVm->aFiles);` |
|     15377 |  373 | `		if( pFile ){` |
|     15377 |  374 | `			SyStringDupPtr(&pFunc->sFile,pFile);` |
|      7686 |  375 | `		}` |
|         - |  376 | `	}` |
|   5500553 |  377 | `	SyStringInitFromBuf(&pFunc->sName,zName,nByte);` |
|   5500553 |  378 | `	return SXRET_OK;` |
|         5 |  379 | `}` |
|         - |  380 | `/*` |
|         - |  381 | ` * Look a name up in the compiled-function table AS A SCRIPT SPELLS IT.` |
|         - |  382 | ` *` |
|         - |  383 | ` * hFunction is the ENGINE's table, not the script's. Besides the functions a program` |
|         - |  384 | ` * declared it holds every mounted class METHOD -- VmMountUserClassMethods installs each` |
|         - |  385 | `` * one under the engine name `[__Class@meth_xxxxxxxxxx]` that compile_class.c mints -- and`` |
|         - |  386 | `` * every compiled CLOSURE, under `[closure_N]`. Neither is a php function name (no php`` |
|         - |  387 | `` * label may hold a `[`, an `@` or a `]`), and php has no table in which a script can find`` |
|         - |  388 | ` * one.` |
|         - |  389 | ` *` |
|         - |  390 | ` * A plain SyHashGet therefore answered a name that does not exist to every surface that` |
|         - |  391 | ` * asks whether a function does: function_exists(), is_callable() and the whole callback` |
|         - |  392 | `` * screen behind it, ReflectionFunction, `new Fiber(name)` -- and the dispatch itself.`` |
|         - |  393 | ` * Reaching a METHOD that way really did run it: the body assumes the receiver frame the` |
|         - |  394 | `` * plain-function path never builds, so `$n = '[__Foo@bar_...]'; $n();` popped past the`` |
|         - |  395 | ` * bottom of the operand stack (SIGSEGV in the release build, an ASan heap-buffer-overflow` |
|         - |  396 | ` * READ in VmByteCodeExecBody).` |
|         - |  397 | ` *` |
|         - |  398 | ` * bEngineName is for the engine's OWN dispatch of those same entries, which is by name` |
|         - |  399 | `` * too: OP_MEMBER pushes a resolved method's `sVmName` onto the callee slot, the closure`` |
|         - |  400 | `` * machinery unwraps a Closure to its `[closure_N]`, and the two synthetic call builders`` |
|         - |  401 | ` * do both without an OP_MEMBER ahead of them. Each of those marks its own call site` |
|         - |  402 | ` * (MEMOBJ_AUX_MEMBERCALL / MEMOBJ_AUX_ENGINEFN / the OP_CALL local); nothing a program` |
|         - |  403 | ` * wrote ever passes 1.` |
|         - |  404 | ` */` |
|   5538400 |  405 | `PH7_PRIVATE SyHashEntry * PH7_VmGetUserFunction(` |
|         - |  406 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  407 | `	const void *pName,  /* Function name */` |
|         - |  408 | `	sxu32 nByte,        /* Name length */` |
|         - |  409 | `	int bEngineName     /* TRUE when the engine, not the script, spelled it */` |
|         - |  410 | `	)` |
|         5 |  411 | `{` |
|   5538405 |  412 | `	SyHashEntry *pEntry = SyHashGet(&pVm->hFunction,pName,nByte);` |
|   5538405 |  413 | `	if( pEntry && !bEngineName ){` |
|    383339 |  414 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|    383339 |  415 | `		if( pFunc == 0 \|\| (pFunc->iFlags & (VM_FUNC_CLASS_METHOD\|VM_FUNC_CLOSURE)) ){` |
|        25 |  416 | `			return 0;` |
|         - |  417 | `		}` |
|    192099 |  418 | `	}` |
|   5538381 |  419 | `	return pEntry;` |
|   2771284 |  420 | `}` |
|         - |  421 | `/*` |
|         - |  422 | ` * Namespace-aware function lookup.` |
|         - |  423 | ` * Resolution order: exact name -> use imports -> current NS\name -> global fallback.` |
|         - |  424 | ` * For functions (unlike classes), PHP falls back to global if not found in current NS.` |
|         - |  425 | ` */` |
|         - |  426 | `/*` |
|         - |  427 | ` * Install a user defined function in the corresponding VM container.` |
|         - |  428 | ` */` |
|  22158458 |  429 | `PH7_PRIVATE sxi32 PH7_VmInstallUserFunction(` |
|         - |  430 | `	ph7_vm *pVm,        /* Target VM */` |
|         - |  431 | `	ph7_vm_func *pFunc, /* Target function */` |
|         - |  432 | `	SyString *pName     /* Function name */` |
|         - |  433 | `	)` |
|         5 |  434 | `{` |
|         - |  435 | `	SyHashEntry *pEntry;` |
|         - |  436 | `	sxi32 rc;` |
|  22158463 |  437 | `	if( pName == 0 ){` |
|         - |  438 | `		/* Use the built-in name */` |
|    159753 |  439 | `		pName = &pFunc->sName;` |
|     79874 |  440 | `	}` |
|         - |  441 | `	/* Check for duplicates (functions with the same name) first */` |
|  22158463 |  442 | `	pEntry = SyHashGet(&pVm->hFunction,pName->zString,pName->nByte);` |
|  22158463 |  443 | `	if( pEntry ){` |
|  16924827 |  444 | `		ph7_vm_func *pLink = (ph7_vm_func *)pEntry->pUserData;` |
|  16924827 |  445 | `		if( pLink != pFunc ){` |
|         - |  446 | `			/* Link */` |
|        91 |  447 | `			pFunc->pNextName = pLink;` |
|        91 |  448 | `			pEntry->pUserData = pFunc;` |
|        43 |  449 | `		}` |
|  16924827 |  450 | `		return SXRET_OK;` |
|         - |  451 | `	}` |
|         - |  452 | `	/* First time seen */` |
|   5233641 |  453 | `	pFunc->pNextName = 0;` |
|   5233641 |  454 | `	rc = SyHashInsert(&pVm->hFunction,pName->zString,pName->nByte,pFunc);` |
|   5233641 |  455 | `	return rc;` |
|  11079234 |  456 | `}` |
|         - |  457 | `/*` |
|         - |  458 | ` * Install a user defined class in the corresponding VM container.` |
|         - |  459 | ` */` |
|    860300 |  460 | `PH7_PRIVATE sxi32 PH7_VmInstallClass(` |
|         - |  461 | `	ph7_vm *pVm,      /* Target VM  */` |
|         - |  462 | `	ph7_class *pClass /* Target Class */` |
|         - |  463 | `	)` |
|         5 |  464 | `{` |
|    860305 |  465 | `	SyString *pName = &pClass->sName;` |
|         - |  466 | `	SyHashEntry *pEntry;` |
|         - |  467 | `	sxi32 rc;` |
|         - |  468 | `	/* Check for duplicates */` |
|    860305 |  469 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)pName->zString,pName->nByte);` |
|    860305 |  470 | `	if( pEntry ){` |
|         3 |  471 | `		ph7_class *pLink = (ph7_class *)pEntry->pUserData;` |
|         - |  472 | `		/* Link entry with the same name */` |
|         3 |  473 | `		pClass->pNextName = pLink;` |
|         3 |  474 | `		pEntry->pUserData = pClass;` |
|         3 |  475 | `		return SXRET_OK;` |
|         - |  476 | `	}` |
|    860303 |  477 | `	pClass->pNextName = 0;` |
|         - |  478 | `	/* Perform a simple hashtable insertion */` |
|    860303 |  479 | `	rc = SyHashInsert(&pVm->hClass,(const void *)pName->zString,pName->nByte,pClass);` |
|    860303 |  480 | `	return rc;` |
|    430155 |  481 | `}` |
|         - |  482 | `/*` |
|         - |  483 | ` * Instruction builder interface.` |
|         - |  484 | ` */` |
|  12050176 |  485 | `PH7_PRIVATE sxi32 PH7_VmEmitInstr(` |
|         - |  486 | `	ph7_vm *pVm,  /* Target VM */` |
|         - |  487 | `	sxi32 iOp,    /* Operation to perform */` |
|         - |  488 | `	sxi32 iP1,    /* First operand */` |
|         - |  489 | `	sxu32 iP2,    /* Second operand */` |
|         - |  490 | `	void *p3,     /* Third operand */` |
|         - |  491 | `	sxu32 *pIndex /* Instruction index. NULL otherwise */` |
|         - |  492 | `	)` |
|         5 |  493 | `{` |
|         - |  494 | `	VmInstr sInstr;` |
|  12050181 |  495 | `	ph7_gen_state *pGen = &pVm->sCodeGen;` |
|         - |  496 | `	sxi32 rc;` |
|         - |  497 | `	/* Fill the VM instruction */` |
|  12050181 |  498 | `	sInstr.iOp = (sxu8)iOp;` |
|  12050181 |  499 | `	sInstr.iP1 = iP1;` |
|  12050181 |  500 | `	sInstr.iP2 = iP2;` |
|  12050181 |  501 | `	sInstr.p3  = p3;` |
|         - |  502 | `	/* Stamp the source line. The node handlers point pGen->pIn at the token being` |
|         - |  503 | `	 * compiled (that is how they read its text), so the current token IS this` |
|         - |  504 | `	 * instruction's source position; pIn can sit one past the end of the stream` |
|         - |  505 | `	 * between statements, hence the range check. */` |
|  12050181 |  506 | `	sInstr.bStrict = (sxu8)(pGen->bStrictTypes ? 1 : 0);` |
|  12050181 |  507 | `	sInstr.nLine = 0;` |
|  12050181 |  508 | `	if( pGen->pIn && pGen->pEnd && pGen->pIn < pGen->pEnd ){` |
|   4349979 |  509 | `		sInstr.nLine = pGen->pIn->nLine;` |
|   9875194 |  510 | `	}else if( pGen->pIn && pGen->pEnd && pGen->pIn >= pGen->pEnd && pGen->pEnd > (SyToken *)0 ){` |
|         - |  511 | `		/* Past the end (statement tail): blame the last real token. */` |
|   7648257 |  512 | `		sInstr.nLine = pGen->pEnd[-1].nLine;` |
|   3824126 |  513 | `	}` |
|  12050181 |  514 | `	if( pIndex ){` |
|         - |  515 | `		/* Instruction index in the bytecode array */` |
|    973471 |  516 | `		*pIndex = SySetUsed(pVm->pByteContainer);` |
|    486733 |  517 | `	}` |
|         - |  518 | `	/* Finally,record the instruction */` |
|  12050181 |  519 | `	rc = SySetPut(pVm->pByteContainer,(const void *)&sInstr);` |
|  12050181 |  520 | `	if( rc != SXRET_OK ){` |
|       ! 0 |  521 | `		PH7_GenCompileError(&pVm->sCodeGen,E_ERROR,1,"Fatal,Cannot emit instruction due to a memory failure");` |
|         - |  522 | `		/* Fall throw */` |
|       ! 0 |  523 | `	}` |
|  12050181 |  524 | `	return rc;` |
|         5 |  525 | `}` |
|         - |  526 | `/*` |
|         - |  527 | ` * Swap the current bytecode container with the given one.` |
|         - |  528 | ` */` |
|    496436 |  529 | `PH7_PRIVATE sxi32 PH7_VmSetByteCodeContainer(ph7_vm *pVm,SySet *pContainer)` |
|         5 |  530 | `{` |
|    496441 |  531 | `	if( pContainer == 0 ){` |
|         - |  532 | `		/* Point to the default container */` |
|       ! 0 |  533 | `		pVm->pByteContainer = &pVm->aByteCode;` |
|       ! 0 |  534 | `	}else{` |
|         - |  535 | `		/* Change container */` |
|    496441 |  536 | `		pVm->pByteContainer = &(*pContainer);` |
|         - |  537 | `	}` |
|    496441 |  538 | `	return SXRET_OK;` |
|         5 |  539 | `}` |
|         - |  540 | `/*` |
|         - |  541 | ` * Return the current bytecode container.` |
|         - |  542 | ` */` |
|    976098 |  543 | `PH7_PRIVATE SySet * PH7_VmGetByteCodeContainer(ph7_vm *pVm)` |
|         5 |  544 | `{` |
|    976103 |  545 | `	return pVm->pByteContainer;` |
|         5 |  546 | `}` |
|         - |  547 | `/*` |
|         - |  548 | ` * Extract the VM instruction rooted at nIndex.` |
|         - |  549 | ` */` |
|    316386 |  550 | `PH7_PRIVATE VmInstr * PH7_VmGetInstr(ph7_vm *pVm,sxu32 nIndex)` |
|         5 |  551 | `{` |
|         - |  552 | `	VmInstr *pInstr;` |
|    316391 |  553 | `	pInstr = (VmInstr *)SySetAt(pVm->pByteContainer,nIndex);` |
|    316391 |  554 | `	return pInstr;` |
|         5 |  555 | `}` |
|         - |  556 | `/*` |
|         - |  557 | ` * Return the total number of VM instructions recorded so far.` |
|         - |  558 | ` */` |
|   6027010 |  559 | `PH7_PRIVATE sxu32 PH7_VmInstrLength(ph7_vm *pVm)` |
|         5 |  560 | `{` |
|   6027015 |  561 | `	return SySetUsed(pVm->pByteContainer);` |
|         5 |  562 | `}` |
|         - |  563 | `/*` |
|         - |  564 | ` * Pop the last VM instruction.` |
|         - |  565 | ` */` |
|    867134 |  566 | `PH7_PRIVATE VmInstr * PH7_VmPopInstr(ph7_vm *pVm)` |
|         5 |  567 | `{` |
|    867139 |  568 | `	return (VmInstr *)SySetPop(pVm->pByteContainer);` |
|         5 |  569 | `}` |
|         - |  570 | `/*` |
|         - |  571 | ` * Peek the last VM instruction.` |
|         - |  572 | ` */` |
|   2328736 |  573 | `PH7_PRIVATE VmInstr * PH7_VmPeekInstr(ph7_vm *pVm)` |
|         5 |  574 | `{` |
|   2328741 |  575 | `	return (VmInstr *)SySetPeek(pVm->pByteContainer);` |
|         5 |  576 | `}` |
|     84870 |  577 | `PH7_PRIVATE VmInstr * PH7_VmPeekNextInstr(ph7_vm *pVm)` |
|         5 |  578 | `{` |
|         - |  579 | `	VmInstr *aInstr;` |
|         - |  580 | `	sxu32 n;` |
|     84875 |  581 | `	n = SySetUsed(pVm->pByteContainer);` |
|     84875 |  582 | `	if( n < 2 ){` |
|       ! 0 |  583 | `		return 0;` |
|         - |  584 | `	}` |
|     84875 |  585 | `	aInstr = (VmInstr *)SySetBasePtr(pVm->pByteContainer);` |
|     84875 |  586 | `	return &aInstr[n - 2];` |
|     42440 |  587 | `}` |
|         - |  588 | `/*` |
|         - |  589 | ` * Allocate a new virtual machine frame.` |
|         - |  590 | ` */` |
|   3674829 |  591 | `PH7_PRIVATE VmFrame * VmNewFrame(` |
|         - |  592 | `	ph7_vm *pVm,              /* Target VM */` |
|         - |  593 | `	void *pUserData,          /* Upper-layer private data */` |
|         - |  594 | `	ph7_class_instance *pThis /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  595 | `	)` |
|         5 |  596 | `{` |
|         - |  597 | `	VmFrame *pFrame;` |
|         - |  598 | `	/* Allocate a new vm frame */` |
|   3674834 |  599 | `	pFrame = (VmFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmFrame));` |
|   3674834 |  600 | `	if( pFrame == 0 ){` |
|       ! 0 |  601 | `		return 0;` |
|         - |  602 | `	}` |
|         - |  603 | `	/* Zero the structure */` |
|   3674834 |  604 | `	SyZero(pFrame,sizeof(VmFrame));` |
|         - |  605 | `	/* Initialize frame fields */` |
|   3674834 |  606 | `	pFrame->pUserData = pUserData;` |
|   3674834 |  607 | `	pFrame->pThis = pThis;` |
|   3674834 |  608 | `	pFrame->pVm = pVm;` |
|   3674834 |  609 | `	SyHashInit(&pFrame->hVar,&pVm->sAllocator,0,0);` |
|   3674834 |  610 | `	SySetInit(&pFrame->sArg,&pVm->sAllocator,sizeof(VmSlot));` |
|   3674834 |  611 | `	SySetInit(&pFrame->sLocal,&pVm->sAllocator,sizeof(VmSlot));` |
|   3674834 |  612 | `	SySetInit(&pFrame->sRef,&pVm->sAllocator,sizeof(VmSlot));` |
|   3674834 |  613 | `	pFrame->nActualArgs = -1; /* unknown until an arg-install site stamps it */` |
|         - |  614 | `	/* Per-frame pending catch/finally return slot (always-init so release is` |
|         - |  615 | `	 * unconditional; bHasRet is already 0 from SyZero). */` |
|   3674834 |  616 | `	PH7_MemObjInit(&(*pVm),&pFrame->sRet);` |
|   3674834 |  617 | `	return pFrame;` |
|   1837636 |  618 | `}` |
|         - |  619 | `/* Forward declaration */` |
|         - |  620 | `static void VmSpreadCaptureReset(ph7_vm *pVm);` |
|         - |  621 | `/*` |
|         - |  622 | ` * Enter a VM frame.` |
|         - |  623 | ` */` |
|   3674135 |  624 | `PH7_PRIVATE sxi32 VmEnterFrame(` |
|         - |  625 | `	ph7_vm *pVm,               /* Target VM */` |
|         - |  626 | `	void *pUserData,           /* Upper-layer private data */` |
|         - |  627 | `	ph7_class_instance *pThis, /* Top most class instance [i.e: Object in the PHP jargon]. NULL otherwise */` |
|         - |  628 | `	VmFrame **ppFrame          /* OUT: Top most active frame */` |
|         - |  629 | `	)` |
|         5 |  630 | `{` |
|         - |  631 | `	VmFrame *pFrame;` |
|         - |  632 | `	/* Allocate a new frame */` |
|   3674140 |  633 | `	pFrame = VmNewFrame(&(*pVm),pUserData,pThis);` |
|   3674140 |  634 | `	if( pFrame == 0 ){` |
|       ! 0 |  635 | `		return SXERR_MEM;` |
|         - |  636 | `	}` |
|         - |  637 | `	/* The line currently executing IS the call site for the frame being pushed. */` |
|   3674140 |  638 | `	pFrame->nCallLine = pVm->nCurLine;` |
|         - |  639 | `	/* Link to the list of active VM frame */` |
|   3674140 |  640 | `	pFrame->pParent = pVm->pFrame;` |
|   3674140 |  641 | `	pVm->pFrame = pFrame;` |
|   3674140 |  642 | `	if( ppFrame ){` |
|         - |  643 | `		/* Write a pointer to the new VM frame */` |
|   3668870 |  644 | `		*ppFrame = pFrame;` |
|   1834649 |  645 | `	}` |
|   3674140 |  646 | `	return SXRET_OK;` |
|   1837289 |  647 | `}` |
|         - |  648 | `/*` |
|         - |  649 | ` * Link a foreign variable with the TOP most active frame.` |
|         - |  650 | ` * Refer to the PH7_OP_UPLINK instruction implementation for more` |
|         - |  651 | ` * information.` |
|         - |  652 | ` */` |
|       316 |  653 | `PH7_PRIVATE sxi32 VmFrameLink(ph7_vm *pVm,SyString *pName)` |
|         5 |  654 | `{` |
|         - |  655 | `	VmFrame *pTarget,*pGlobal;` |
|         - |  656 | `	SyHashEntry *pEntry;` |
|         - |  657 | `	sxi32 rc;` |
|       321 |  658 | `	pTarget = VmSkipExceptionFrames(pVm->pFrame);` |
|         - |  659 | ``	/* php's `global` names the GLOBAL scope and nothing else. PH7 walked the frame`` |
|         - |  660 | `	 * chain and linked the FIRST frame that happened to hold the name — and that` |
|         - |  661 | ``	 * chain is the CALL STACK, so `global $v` inside a callee bound the CALLER's`` |
|         - |  662 | ``	 * local `$v`: the function read a value that depended on who called it, and its`` |
|         - |  663 | `	 * writes never reached the real global. */` |
|       321 |  664 | `	pGlobal = pTarget;` |
|       673 |  665 | `	while( pGlobal->pParent ){` |
|       357 |  666 | `		pGlobal = pGlobal->pParent;` |
|         5 |  667 | `	}` |
|       321 |  668 | `	if( pGlobal == pTarget ){` |
|         - |  669 | ``		/* Already the global scope: php's `global $x` is a no-op there. */`` |
|       ! 0 |  670 | `		return SXRET_OK;` |
|         - |  671 | `	}` |
|         - |  672 | `	/* A superglobal is already global storage; link ITS slot rather than creating a` |
|         - |  673 | `	 * plain global that would shadow it. */` |
|       321 |  674 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|       321 |  675 | `	if( pEntry == 0 ){` |
|       319 |  676 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|       157 |  677 | `	}` |
|       321 |  678 | `	if( pEntry == 0 ){` |
|         - |  679 | `		/* php CREATES the global (NULL) at the declaration, which is what makes the` |
|         - |  680 | ``		 * `global $out; $out = …;` initializer idiom work; PH7 left it unlinked and`` |
|         - |  681 | `		 * the assignment went to a local nobody could read. */` |
|        12 |  682 | `		rc = PH7_VmInstallGlobalVar(&(*pVm),pName->zString,pName->nByte,0,SXU32_HIGH);` |
|        12 |  683 | `		if( rc != SXRET_OK ){` |
|       ! 0 |  684 | `			return rc;` |
|         - |  685 | `		}` |
|        12 |  686 | `		pEntry = SyHashGet(&pGlobal->hVar,(const void *)pName->zString,pName->nByte);` |
|        12 |  687 | `		if( pEntry == 0 ){` |
|       ! 0 |  688 | `			return SXERR_NOTFOUND;` |
|         - |  689 | `		}` |
|         5 |  690 | `	}` |
|         - |  691 | `	/* Bind the name in the calling frame to that slot — a REBIND when the frame` |
|         - |  692 | ``	 * already has a local of the same name, which php's `global` also replaces. */`` |
|       479 |  693 | `	PH7_VmBindVarSlot(&(*pVm),pTarget,pName->zString,pName->nByte,` |
|       316 |  694 | `		(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|       321 |  695 | `	return SXRET_OK;` |
|       163 |  696 | `}` |
|         - |  697 | `/*` |
|         - |  698 | ` * Invalidate a recorded in-place-catch resume target (ROOT B) that still points at` |
|         - |  699 | ` * a frame about to be pool-freed, so a later VmRecordedResume can't match (and walk)` |
|         - |  700 | ` * a dangling/reused frame. Called from every VmFrame free site (VmLeaveFrame and the` |
|         - |  701 | ` * detached generator/fiber frame in VmReleaseExecCtx). In normal flow the target is` |
|         - |  702 | ` * consumed before its catching body unwinds (the body is a pinned ancestor), so this` |
|         - |  703 | ` * is defensive; it never fires for an intermediate exception wrapper popped during a` |
|         - |  704 | ` * resume — pVm->pResumeFrame is always a real body, never a wrapper.` |
|         - |  705 | ` */` |
|   3669369 |  706 | `PH7_PRIVATE void VmDropResumeTarget(ph7_vm *pVm, VmFrame *pFrame)` |
|         5 |  707 | `{` |
|   3669374 |  708 | `	if( pVm->pResumeFrame == pFrame ){` |
|       ! 0 |  709 | `		pVm->pResumeFrame = 0;` |
|       ! 0 |  710 | `	}` |
|   3669374 |  711 | `}` |
|         - |  712 | `/*` |
|         - |  713 | ` * Leave the top-most active frame.` |
|         - |  714 | ` */` |
|   3668655 |  715 | `PH7_PRIVATE void VmLeaveFrame(ph7_vm *pVm)` |
|         5 |  716 | `{` |
|   3668660 |  717 | `		VmFrame *pCurFrame = pVm->pFrame;` |
|   3668660 |  718 | `	if( pCurFrame ){` |
|         - |  719 | `		/* Unlink from the list of active VM frame */` |
|   3668660 |  720 | `		pVm->pFrame = pCurFrame->pParent;` |
|   3668660 |  721 | `		if( pCurFrame->pParent && (pCurFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|         - |  722 | `			VmSlot  *aSlot;` |
|         - |  723 | `			sxu32 n;` |
|         - |  724 | `			/* Remove this frame's NAME bindings from the reference table FIRST: a local` |
|         - |  725 | `			 * is a holder of its own slot, and the release decision below counts holders` |
|         - |  726 | `			 * (it also stops the record keeping pointers to hash entries this teardown` |
|         - |  727 | `			 * is about to free). */` |
|    770509 |  728 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sRef);` |
|   1929385 |  729 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sRef) ; ++n ){` |
|   1158881 |  730 | `				PH7_VmRefObjRemove(&(*pVm),aSlot[n].nIdx,(SyHashEntry *)aSlot[n].pUserData,0);` |
|    580754 |  731 | `			}` |
|         - |  732 | `			/* Restore local variable to the free pool so that they can be reused again */` |
|    770509 |  733 | `			aSlot = (VmSlot *)SySetBasePtr(&pCurFrame->sLocal);` |
|   1928741 |  734 | `			for(n = 0 ; n < SySetUsed(&pCurFrame->sLocal) ; ++n ){` |
|   1158237 |  735 | `				if( PH7_VmSlotHolderCount(&(*pVm),aSlot[n].nIdx) > 0 ){` |
|         - |  736 | `					/* Something OUTSIDE this frame refers to the local: an array element` |
|         - |  737 | ``					 * bound to it (`function f(){ $v = 9; return [1, &$v]; }`) or another`` |
|         - |  738 | `					 * name. php keeps the VALUE for whoever is left holding it — PH7 tore` |
|         - |  739 | `					 * down the slot and the reference table took the holders with it, so` |
|         - |  740 | `					 * the returned array came back one element SHORT. The last holder to` |
|         - |  741 | `					 * die releases the slot (PH7_VmReleaseUnheldSlot). */` |
|        24 |  742 | `					continue;` |
|         - |  743 | `				}` |
|         - |  744 | `				/* Unset the local variable */` |
|   1158215 |  745 | `				PH7_VmUnsetMemObj(&(*pVm),aSlot[n].nIdx,FALSE);` |
|    580421 |  746 | `			}` |
|    385469 |  747 | `		}` |
|         - |  748 | `		/* Release internal containers */` |
|   3668660 |  749 | `		SyHashRelease(&pCurFrame->hVar);` |
|   3668660 |  750 | `		SySetRelease(&pCurFrame->sArg);` |
|   3668660 |  751 | `		SySetRelease(&pCurFrame->sLocal);` |
|   3668660 |  752 | `		SySetRelease(&pCurFrame->sRef);` |
|         - |  753 | `		/* Release the per-frame pending-return slot (a frame-level resource like the` |
|         - |  754 | `		 * containers above — released for every frame, including transparent` |
|         - |  755 | `		 * exception/catch wrappers, which never own a return so it is empty there). */` |
|   3668660 |  756 | `		PH7_MemObjRelease(&pCurFrame->sRet);` |
|         - |  757 | `		/* Drop a recorded in-place-catch resume target pointing at this frame (ROOT B). */` |
|   3668660 |  758 | `		VmDropResumeTarget(pVm,pCurFrame);` |
|         - |  759 | `		/* Release the whole structure */` |
|   3668660 |  760 | `		SyMemBackendPoolFree(&pVm->sAllocator,pCurFrame);` |
|   1834544 |  761 | `	}` |
|   3668660 |  762 | `}` |
|         - |  763 | `/*` |
|         - |  764 | ` * Pin a memory-object slot past its owning frame: remove it from whichever` |
|         - |  765 | ` * active frame's local-teardown set records it (walking the parent chain` |
|         - |  766 | ` * covers by-ref argument aliases whose slot belongs to a caller), and flag` |
|         - |  767 | ` * its reference record VM_REF_IDX_KEEP so unset() cannot recycle the index.` |
|         - |  768 | `` * Used for by-reference closure captures (`use (&$x)`), whose slot must stay`` |
|         - |  769 | ` * alive as long as the closure itself: the slot then lives until VM reset —` |
|         - |  770 | ` * php frees it by refcount, PHL trades that for a script-lifetime pin.` |
|         - |  771 | ` */` |
|         - |  772 | `/*` |
|         - |  773 | ` * Remove a memobj slot from whichever active frame's local-teardown set (sLocal)` |
|         - |  774 | ` * records it — walking the parent chain covers by-reference aliases whose slot is` |
|         - |  775 | ` * owned by a caller. Returns TRUE if an entry was dropped.` |
|         - |  776 | ` *` |
|         - |  777 | ` * A slot lands in sLocal so VmLeaveFrame frees it when the frame exits. But a slot` |
|         - |  778 | ` * can leave its frame's ownership EARLY — pinned past the frame (VmPinMemObjSlot),` |
|         - |  779 | ` * or returned to the free pool by unset() — and once the index is recycled for a` |
|         - |  780 | ` * different owner (an object property reserved with VM_REF_IDX_KEEP, say) a stale` |
|         - |  781 | ` * sLocal entry makes VmLeaveFrame release that unrelated owner's value. Dropping the` |
|         - |  782 | ` * entry at the point the slot leaves the frame closes that use-after-free.` |
|         - |  783 | ` */` |
|   7839003 |  784 | `PH7_PRIVATE int VmDropFrameLocalSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  785 | `{` |
|         - |  786 | `	VmFrame *pFrame;` |
|  21191307 |  787 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|  13352570 |  788 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|         - |  789 | `		sxu32 n;` |
|  28552274 |  790 | `		for( n = 0 ; n < SySetUsed(&pFrame->sLocal) ; ++n ){` |
|  15199975 |  791 | `			if( aSlot[n].nIdx == nIdx ){` |
|         - |  792 | `				/* Swap-remove: teardown order over sLocal is immaterial */` |
|       269 |  793 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sLocal)-1];` |
|       269 |  794 | `				(void)SySetPop(&pFrame->sLocal);` |
|       269 |  795 | `				return TRUE; /* Slot owned by exactly one frame */` |
|         - |  796 | `			}` |
|   7599854 |  797 | `		}` |
|   6676170 |  798 | `	}` |
|   7838742 |  799 | `	return FALSE;` |
|   3919525 |  800 | `}` |
|       566 |  801 | `PH7_PRIVATE void VmPinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 |  802 | `{` |
|         - |  803 | `	VmRefObj *pRef;` |
|       571 |  804 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|       571 |  805 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|       571 |  806 | `	if( pRef ){` |
|       547 |  807 | `		pRef->iFlags \|= VM_REF_IDX_KEEP;` |
|       276 |  808 | `	}else{` |
|         - |  809 | `		/* No record yet — a pin on a slot nothing refers to was silently a NO-OP, so the` |
|         - |  810 | `		 * slot stayed releasable and (since a pin is a holder) nothing counted it. */` |
|        27 |  811 | `		PH7_VmRefObjInstall(&(*pVm),nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - |  812 | `	}` |
|       571 |  813 | `}` |
|         - |  814 | `/*` |
|         - |  815 | `` * A pin that can be GIVEN BACK: a reference-bound property (`$o->p =& $x`) holds the slot`` |
|         - |  816 | `` * only while the property does. The permanent pins (a `use (&$x)` capture, a static, an`` |
|         - |  817 | ` * enum case) stay on VmPinMemObjSlot, which never counts down.` |
|         - |  818 | ` */` |
|        42 |  819 | `PH7_PRIVATE void VmPinMemObjSlotCounted(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  820 | `{` |
|         - |  821 | `	VmRefObj *pRef;` |
|        43 |  822 | `	VmPinMemObjSlot(&(*pVm),nIdx);` |
|        43 |  823 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        43 |  824 | `	if( pRef ){` |
|        43 |  825 | `		pRef->nPin++;` |
|        21 |  826 | `	}` |
|        43 |  827 | `}` |
|         - |  828 | `/*` |
|         - |  829 | ` * Give back a counted pin. The slot goes when it was the last holder — without this the` |
|         - |  830 | ` * value a released property was pinning stayed alive for the rest of the script (the pin` |
|         - |  831 | ` * used to be a flag, so nothing could tell one holder from two).` |
|         - |  832 | ` */` |
|        36 |  833 | `PH7_PRIVATE void VmUnpinMemObjSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         1 |  834 | `{` |
|        37 |  835 | `	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|        37 |  836 | `	if( pRef == 0 \|\| pRef->nPin < 1 ){` |
|       ! 0 |  837 | `		return;` |
|         - |  838 | `	}` |
|        37 |  839 | `	pRef->nPin--;` |
|        37 |  840 | `	if( pRef->nPin < 1 ){` |
|        31 |  841 | `		pRef->iFlags &= ~VM_REF_IDX_KEEP;` |
|        31 |  842 | `		PH7_VmReleaseUnheldSlot(&(*pVm),nIdx);` |
|        15 |  843 | `	}` |
|        19 |  844 | `}` |
|         - |  845 | `/*` |
|         - |  846 | ` * Skip exception frames to reach the nearest non-exception frame.` |
|         - |  847 | ` * Exception frames are transparent wrappers pushed by try/catch and` |
|         - |  848 | ` * should be skipped when looking for the real execution context.` |
|         - |  849 | ` */` |
|  43285888 |  850 | `PH7_PRIVATE VmFrame * VmSkipExceptionFrames(VmFrame *pFrame)` |
|         5 |  851 | `{` |
|  56758173 |  852 | `	while( pFrame->pParent && (pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|  13472285 |  853 | `		pFrame = pFrame->pParent;` |
|         5 |  854 | `	}` |
|  43285893 |  855 | `	return pFrame;` |
|         5 |  856 | `}` |
|         - |  857 | `/*` |
|         - |  858 | `` * After a `catch` ran IN PLACE inside VmThrowException, the throwing site — which`` |
|         - |  859 | ` * may be several frames below the frame that caught the exception — must resume at` |
|         - |  860 | ` * the CATCHING body's landing pad, not the lexically/stack-nearest try. To make that` |
|         - |  861 | ` * possible VmThrowException records the catching body frame (pVm->pResumeFrame) and` |
|         - |  862 | ` * its post-try landing pad (pVm->iResumePc) when it handles a throw in place.` |
|         - |  863 | ` *` |
|         - |  864 | ` * This predicate, consulted at every resume site, returns TRUE and sets *pResumePc` |
|         - |  865 | ` * when the CURRENT exec is the one that owns the catching frame — so the landing pc` |
|         - |  866 | ` * is valid against this exec's bytecode array. It returns FALSE to mean "propagate"` |
|         - |  867 | ` * (goto Exception / return PH7_EXCEPTION), so the exception keeps unwinding until it` |
|         - |  868 | ` * reaches the exec that actually caught it, which then matches and lands. A` |
|         - |  869 | ` * catch/finally mini-program (bReturnPropagates) NEVER resumes: an in-place catch's` |
|         - |  870 | ` * landing pad indexes the enclosing function's bytecode, never the mini-program's` |
|         - |  871 | ` * small array — so it always propagates to its enclosing body. The recorded target` |
|         - |  872 | ` * is consumed one-shot on a match. pEntryFrame is the running exec's entry frame;` |
|         - |  873 | ` * VmSkipExceptionFrames yields its real body frame.` |
|         - |  874 | ` *` |
|         - |  875 | ` * This replaces the older "is there a resumable try frame here" test` |
|         - |  876 | ` * (VmTryFrameInCurrentExec on pVm->pFrame), which answered presence-of-a-try rather` |
|         - |  877 | ` * than identity-of-the-catcher and so resumed at the wrong landing pad whenever the` |
|         - |  878 | ` * catching frame was not the nearest try (ROOT B).` |
|         - |  879 | ` */` |
|   1961188 |  880 | `PH7_PRIVATE int VmRecordedResume(ph7_vm *pVm,sxi32 *pResumePc,VmFrame *pEntryFrame,VmInstr *aInstr)` |
|         5 |  881 | `{` |
|   1961193 |  882 | `	if( pVm->pResumeFrame == 0 ){` |
|        24 |  883 | `		return FALSE; /* no in-place catch recorded for this in-flight throw */` |
|         - |  884 | `	}` |
|   1961173 |  885 | `	if( pEntryFrame == 0 ){` |
|         - |  886 | `		/* A SYNTHETIC exec state — VmReDriveStep runs a single LOAD_IDX/MEMBER on a` |
|         - |  887 | `		 * two-slot stack with a zeroed VmExecState, so it has no entry frame and no` |
|         - |  888 | `		 * landing pad of its own. It can never be the exec that owns the catching` |
|         - |  889 | `		 * try, so propagate (the re-drive's caller turns that into PH7_EXCEPTION at` |
|         - |  890 | `		 * the OP_CALL site). Without the guard VmSkipExceptionFrames dereferences` |
|         - |  891 | `		 * NULL and the process dies. */` |
|        15 |  892 | `		return FALSE;` |
|         - |  893 | `	}` |
|         - |  894 | `	/* Resume here only when THIS exec is the one that owns the catching try: same` |
|         - |  895 | `	 * body frame AND same bytecode array. The bytecode-array check is essential` |
|         - |  896 | `	 * because a catch/finally mini-program runs in its enclosing body's frame (no` |
|         - |  897 | `	 * new frame), so the body-frame test alone cannot tell a mini-program apart from` |
|         - |  898 | `	 * the body that shares it — and the recorded landing pad indexes only the array` |
|         - |  899 | `	 * the try was compiled into. A mismatch on either means the exception was caught` |
|         - |  900 | `	 * in a different exec, so we propagate (return/goto Exception) and let the owning` |
|         - |  901 | `	 * exec's resume site match and land. */` |
|   1961156 |  902 | `	if( VmSkipExceptionFrames(pEntryFrame) != pVm->pResumeFrame` |
|   1659008 |  903 | `	 \|\| (void *)aInstr != pVm->pResumeInstr` |
|   1356621 |  904 | `	 \|\| pVm->iResumePc == 0 ){` |
|         - |  905 | `		/* iResumePc is a try's post-construct landing pad (OP_LOAD_EXCEPTION's iP2),` |
|         - |  906 | `		 * always >= 1 in practice; the ==0 guard keeps a malformed record from` |
|         - |  907 | ``		 * underflowing `*pResumePc = iResumePc - 1` to -1 (which the dispatcher's pc++`` |
|         - |  908 | `		 * would turn into a re-run from index 0) and from making the pop loop below` |
|         - |  909 | `		 * never match a real frame. */` |
|    604788 |  910 | `		return FALSE;` |
|         - |  911 | `	}` |
|         - |  912 | `	/* The catch may have run at an OUTER try, several call-levels above the throw.` |
|         - |  913 | `	 * VmThrowException runs the catch in place and then leaves pVm->pFrame at the` |
|         - |  914 | `	 * THROW SITE (its pThrowSite restore), so between here and the catching try's` |
|         - |  915 | `	 * landing pad the chain can hold: the intermediate try's own VM_FRAME_EXCEPTION` |
|         - |  916 | `	 * frames (each normally torn down by its OWN OP_POP_EXCEPTION, which we are about` |
|         - |  917 | `	 * to skip), AND — when the throw was raised in a DEEPER call than the one that` |
|         - |  918 | `	 * declared the catching try — the dead body frames of those abandoned callees` |
|         - |  919 | `	 * (the exception unwound past them, but the in-place-catch resume short-circuits` |
|         - |  920 | `	 * the per-record VmCallFinish that would otherwise have popped them). Pop them ALL` |
|         - |  921 | `	 * so exactly one frame (the catching try's exception frame) is left for the` |
|         - |  922 | `	 * OP_POP_EXCEPTION we land on; without this the skipped inner tries and the` |
|         - |  923 | `	 * dangling callee frames leak, and — worse — the landing code runs with pVm->pFrame` |
|         - |  924 | `	 * pointing at a dead callee, so its locals resolve against the wrong scope (a live` |
|         - |  925 | `	 * caller variable reads as an uninitialised fresh one). Their handlers/finally` |
|         - |  926 | `	 * already ran in place during VmThrowException. The loop stops on either term, both` |
|         - |  927 | `	 * load-bearing: the catching try's exception frame is reached (its iExceptionJump ==` |
|         - |  928 | `	 * the recorded landing); or this exec's entry is reached (structural floor). Landing` |
|         - |  929 | `	 * pads are unique per try within one bytecode array, and the function guard above` |
|         - |  930 | `	 * pins (frame,array) to this exec, so the iExceptionJump match cannot stop at the` |
|         - |  931 | `	 * wrong try. OP_POP_EXCEPTION's frame-leave is guarded on VM_FRAME_EXCEPTION, so` |
|         - |  932 | `	 * leaving the catching exception frame here (rather than the body) lands cleanly. */` |
|   2188589 |  933 | `	while( pVm->pFrame != pEntryFrame` |
|   2392697 |  934 | `	    && !((pVm->pFrame->iFlags & VM_FRAME_EXCEPTION)` |
|   1560475 |  935 | `	         && pVm->pFrame->iExceptionJump == pVm->iResumePc) ){` |
|    308065 |  936 | `		VmLeaveFrame(&(*pVm));` |
|         5 |  937 | `	}` |
|   1356378 |  938 | `	*pResumePc = (sxi32)pVm->iResumePc - 1;` |
|   1356378 |  939 | `	pVm->pResumeFrame = 0; /* one-shot consume */` |
|         - |  940 | `	/* Landing at the catch pad consumes any C-boundary parked copy of the same` |
|         - |  941 | `	 * in-flight throw (VmBoundaryPark): the status is routed now, so the fetch-` |
|         - |  942 | `	 * point router must not re-fire it after this resume. */` |
|   1356378 |  943 | `	pVm->nBoundaryRc = 0;` |
|   1356378 |  944 | `	return TRUE;` |
|    980598 |  945 | `}` |
|         - |  946 | `/*` |
|         - |  947 | ` * Drain pending finally blocks for the try/catch contexts pushed during the` |
|         - |  948 | ` * current VmByteCodeExec invocation (those above nExceptionBase). Invoked when` |
|         - |  949 | ` * control leaves a function/try via 'return' (OP_DONE) or via a 'return' issued` |
|         - |  950 | ` * inside a catch/finally (the OP_THROW / OP_POP_EXCEPTION consumers, and a` |
|         - |  951 | ` * nested try/finally inside a catch body). Each finally runs with` |
|         - |  952 | ` * bReturnPropagates=TRUE so a 'return' inside it overrides the pending value on` |
|         - |  953 | ` * its body frame's sRet slot. Returns SXERR_ABORT if a finally aborted, PH7_EXCEPTION if a` |
|         - |  954 | ` * finally threw an exception that escaped it (the caller must then unwind as an` |
|         - |  955 | ` * exception rather than return its pending value — PHP: a throwing finally` |
|         - |  956 | ` * discards the in-flight return), SXRET_OK otherwise.` |
|         - |  957 | ` */` |
|         - |  958 | `/*` |
|         - |  959 | ` * BYTECODE stage 2b — per-activation try state.` |
|         - |  960 | ` *` |
|         - |  961 | ` * A lexical try compiles to ONE ph7_exception (pInstr->p3). Pushing that` |
|         - |  962 | ` * object itself onto pVm->aException meant every recursive activation of the` |
|         - |  963 | ` * same try shared one pFrame/iFinallyDone/iInCatch/pInflight — unwinding a` |
|         - |  964 | ` * deep throw then ran every level's catch/finally against the deepest frame` |
|         - |  965 | ` * (silent wrong answers; see the try_unwind_recursive_frames` |
|         - |  966 | ` * twins). OP_LOAD_EXCEPTION now pushes a pool-allocated ACTIVATION: a shallow` |
|         - |  967 | ` * copy of the compiled object (sEntry/sFinally share the read-only compiled` |
|         - |  968 | ` * containers) with fresh mutable state and pCompiled pointing at the origin.` |
|         - |  969 | ` * Opcodes that only know the compiled p3 find their live activation with` |
|         - |  970 | ` * VmExcLive. Activations are freed at the sites that discard an entry for` |
|         - |  971 | ` * good: OP_POP_EXCEPTION, VmDrainFinally, VmThrowException's discard paths,` |
|         - |  972 | ` * VmThrowInline's non-repush paths, VmReleaseExecCtx (parked handlers of an` |
|         - |  973 | ` * abandoned coroutine) and VM reset. This also retires TICKET 1433-60's` |
|         - |  974 | `` * "never free" constraint: a `goto` re-entering the try simply mints a fresh`` |
|         - |  975 | ` * activation.` |
|         - |  976 | ` */` |
|   1461608 |  977 | `PH7_PRIVATE ph7_exception * VmExcActivate(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 |  978 | `{` |
|   1461613 |  979 | `	ph7_exception *pClone = (ph7_exception *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_exception));` |
|   1461613 |  980 | `	if( pClone == 0 ){` |
|       ! 0 |  981 | `		return 0;` |
|         - |  982 | `	}` |
|   1461613 |  983 | `	*pClone = *pCompiled;` |
|   1461613 |  984 | `	pClone->pCompiled = pCompiled;` |
|   1461613 |  985 | `	pClone->iFinallyDone = 0;` |
|   1461613 |  986 | `	pClone->iInCatch = 0;` |
|   1461613 |  987 | `	pClone->pInflight = 0;` |
|   1461613 |  988 | `	pClone->pFrame = 0;` |
|   1461613 |  989 | `	return pClone;` |
|    730809 |  990 | `}` |
|   2918723 |  991 | `PH7_PRIVATE void VmExcRelease(ph7_vm *pVm,ph7_exception *pExc)` |
|         5 |  992 | `{` |
|   2918728 |  993 | `	if( pExc && pExc->pCompiled ){` |
|         - |  994 | `		/* Only activations are freed; the compiled object is compiler-owned.` |
|         - |  995 | `		 * An unconsumed pInflight (VmThrowInline's iRef++ hold that OP_CATCH` |
|         - |  996 | `		 * never ran to release — abort/reset between the pc-redirect and the` |
|         - |  997 | `		 * catch) is dropped here so the exception instance cannot leak. */` |
|   1461595 |  998 | `		if( pExc->pInflight ){` |
|       ! 0 |  999 | `			PH7_ClassInstanceUnref(pExc->pInflight);` |
|       ! 0 | 1000 | `			pExc->pInflight = 0;` |
|       ! 0 | 1001 | `		}` |
|   1461595 | 1002 | `		SyMemBackendPoolFree(&pVm->sAllocator,pExc);` |
|    730795 | 1003 | `	}` |
|   2918728 | 1004 | `}` |
|         - | 1005 | `/*` |
|         - | 1006 | ` * TRUE when the aException entry pExc is (an activation of) the compiled try` |
|         - | 1007 | ` * pCompiled. One home for the identity rule (VmExcLive, OP_POP_EXCEPTION).` |
|         - | 1008 | ` */` |
|      4801 | 1009 | `PH7_PRIVATE int VmExcMatches(ph7_exception *pExc,ph7_exception *pCompiled)` |
|         5 | 1010 | `{` |
|      4806 | 1011 | `	return pExc == pCompiled \|\| pExc->pCompiled == pCompiled;` |
|         5 | 1012 | `}` |
|         - | 1013 | `/*` |
|         - | 1014 | ` * Free every activation held in an exception-entry container (leftovers at VM` |
|         - | 1015 | ` * reset, a discarded hide/restore set, an abandoned coroutine's parked` |
|         - | 1016 | ` * handlers). The set itself is reset by the caller.` |
|         - | 1017 | ` */` |
|    101010 | 1018 | `PH7_PRIVATE void VmExcReleaseAll(ph7_vm *pVm,SySet *pSet)` |
|         5 | 1019 | `{` |
|    101015 | 1020 | `	sxu32 n = SySetUsed(pSet);` |
|    101015 | 1021 | `	if( n > 0 ){` |
|       ! 0 | 1022 | `		ph7_exception **ap = (ph7_exception **)SySetBasePtr(pSet);` |
|         - | 1023 | `		sxu32 i;` |
|       ! 0 | 1024 | `		for( i = 0; i < n; i++ ){` |
|       ! 0 | 1025 | `			VmExcRelease(pVm,ap[i]);` |
|       ! 0 | 1026 | `		}` |
|       ! 0 | 1027 | `	}` |
|    101015 | 1028 | `}` |
|         - | 1029 | `/*` |
|         - | 1030 | ` * Drain the topmost nCross try activations — the ones a jump is leaving without` |
|         - | 1031 | ` * reaching their OP_POP_EXCEPTION, so nothing else would run their finally. nFloor is` |
|         - | 1032 | ` * the running execution's exception base: never drain below it, or a jump would tear` |
|         - | 1033 | ` * down a try belonging to the caller.` |
|         - | 1034 | ` */` |
|        18 | 1035 | `PH7_PRIVATE sxi32 VmDrainCrossedTrys(ph7_vm *pVm,sxu32 nCross,sxu32 nFloor)` |
|         3 | 1036 | `{` |
|        21 | 1037 | `	sxu32 nUsed = SySetUsed(&pVm->aException);` |
|        21 | 1038 | `	sxu32 nBase = nUsed > nCross ? nUsed - nCross : 0;` |
|        21 | 1039 | `	if( nBase < nFloor ){` |
|       ! 0 | 1040 | `		nBase = nFloor;` |
|       ! 0 | 1041 | `	}` |
|        21 | 1042 | `	return VmDrainFinally(&(*pVm),nBase);` |
|         3 | 1043 | `}` |
|         - | 1044 | `/*` |
|         - | 1045 | ` * The live activation of a lexical try: the topmost aException entry cloned` |
|         - | 1046 | ` * from pCompiled. Used by the inline opcodes (OP_CATCH) whose instruction` |
|         - | 1047 | ` * only carries the compiled pointer.` |
|         - | 1048 | ` */` |
|        76 | 1049 | `PH7_PRIVATE ph7_exception * VmExcLive(ph7_vm *pVm,ph7_exception *pCompiled)` |
|         5 | 1050 | `{` |
|        81 | 1051 | `	ph7_exception **ap = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|        81 | 1052 | `	sxu32 n = SySetUsed(&pVm->aException);` |
|        81 | 1053 | `	while( n > 0 ){` |
|        81 | 1054 | `		n--;` |
|        81 | 1055 | `		if( VmExcMatches(ap[n],pCompiled) ){` |
|        81 | 1056 | `			return ap[n];` |
|         - | 1057 | `		}` |
|       ! 0 | 1058 | `	}` |
|       ! 0 | 1059 | `	return 0;` |
|        43 | 1060 | `}` |
|   3158910 | 1061 | `PH7_PRIVATE sxi32 VmDrainFinally(ph7_vm *pVm, sxu32 nExceptionBase)` |
|         5 | 1062 | `{` |
|         - | 1063 | `	sxu32 nUsed;` |
|   3158915 | 1064 | `	sxi32 rcOut = SXRET_OK;` |
|   3159231 | 1065 | `	while( (nUsed = SySetUsed(&pVm->aException)) > nExceptionBase ){` |
|       321 | 1066 | `		ph7_exception **apExc = (ph7_exception **)SySetBasePtr(&pVm->aException);` |
|       321 | 1067 | `		ph7_exception *pExc = apExc[nUsed - 1];` |
|       321 | 1068 | `		(void)SySetPop(&pVm->aException);` |
|       321 | 1069 | `		pExc->pFrame = 0;` |
|         - | 1070 | `		/* Leave the try's exception frame — but only a genuine one. For a RESUMED` |
|         - | 1071 | `		 * generator/fiber body the handler was restored from the parked ctx and its` |
|         - | 1072 | `		 * exception frame was discarded at suspend, so pVm->pFrame is the coroutine` |
|         - | 1073 | `		 * body itself; popping it would free the entry frame OP_DONE still reads` |
|         - | 1074 | `		 * (same guard as OP_POP_EXCEPTION's). */` |
|       321 | 1075 | `		if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){` |
|       321 | 1076 | `			VmLeaveFrame(&(*pVm));` |
|       158 | 1077 | `		}` |
|       353 | 1078 | `		if( pExc->iHasFinally && !pExc->iFinallyDone ){` |
|         - | 1079 | `			sxi32 rcF;` |
|        69 | 1080 | `			pExc->iFinallyDone = 1;` |
|        69 | 1081 | `			rcF = VmLocalExec(&(*pVm),&pExc->sFinally,0,TRUE);` |
|        69 | 1082 | `			VmExcRelease(&(*pVm),pExc); /* nothing re-references a popped activation */` |
|        69 | 1083 | `			if( rcF == SXERR_ABORT ){` |
|       ! 0 | 1084 | `				return SXERR_ABORT;` |
|         - | 1085 | `			}` |
|        69 | 1086 | `			if( rcF == PH7_EXCEPTION ){` |
|         - | 1087 | `				/* The finally threw past itself (its catch, if any, ran in place).` |
|         - | 1088 | `				 * Remember it so the caller unwinds as an exception; keep draining` |
|         - | 1089 | `				 * the remaining outer finallys so the frame stack stays balanced. */` |
|         5 | 1090 | `				rcOut = PH7_EXCEPTION;` |
|         2 | 1091 | `			}` |
|        37 | 1092 | `		}else{` |
|       256 | 1093 | `			VmExcRelease(&(*pVm),pExc);` |
|         - | 1094 | `		}` |
|         5 | 1095 | `	}` |
|   3158915 | 1096 | `	return rcOut;` |
|   1579668 | 1097 | `}` |
|         - | 1098 | `/*` |
|         - | 1099 | `` * Drop a body frame's pending catch/finally ACTION — the `return` parked on sRet and`` |
|         - | 1100 | `` * the `break`/`continue` parked by OP_CATCH_JMP. Both mean "when this try's landing`` |
|         - | 1101 | ` * pad is reached, do X instead of falling through", and every path that abandons the` |
|         - | 1102 | ` * frame or lets an exception supersede it has to drop both. Safe on a frame with` |
|         - | 1103 | ` * nothing pending (the slot is then an empty MEMOBJ_NULL value, release is a no-op).` |
|         - | 1104 | ` */` |
|   2567698 | 1105 | `PH7_PRIVATE void VmClearFramePending(VmFrame *pFrame)` |
|         5 | 1106 | `{` |
|   2567703 | 1107 | `	pFrame->bHasRet = 0;` |
|   2567703 | 1108 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|   2567703 | 1109 | `	pFrame->nCatchJmpPc = 0;` |
|   2567703 | 1110 | `}` |
|         - | 1111 | `/*` |
|         - | 1112 | `` * Materialize a `return` issued inside a catch/finally mini-program: copy the`` |
|         - | 1113 | ` * value deferred on the enclosing body frame (pEntryFrame->sRet) into the` |
|         - | 1114 | ` * function's result, clear the per-frame slot, and tear down any try frames left` |
|         - | 1115 | ` * open above pEntryFrame whose OP_POP_EXCEPTION the return bypassed (bounded by` |
|         - | 1116 | ` * pEntryFrame so a tangled exception-in-finally chain can't over-leave). Only the` |
|         - | 1117 | ` * real function body (bReturnPropagates=FALSE) calls this; a nested mini-program` |
|         - | 1118 | ` * leaves the slot set so it materializes at its own enclosing body.` |
|         - | 1119 | ` */` |
|     20414 | 1120 | `PH7_PRIVATE void VmMaterializeCatchReturn(ph7_vm *pVm, ph7_value *pResult, VmFrame *pEntryFrame)` |
|         5 | 1121 | `{` |
|     20419 | 1122 | `	if( pResult ){` |
|     20419 | 1123 | `		PH7_MemObjStore(&pEntryFrame->sRet,pResult);` |
|     10207 | 1124 | `	}` |
|     20419 | 1125 | `	VmClearFramePending(pEntryFrame);` |
|     20423 | 1126 | `	while( pVm->pFrame && pVm->pFrame != pEntryFrame ){` |
|         5 | 1127 | `		VmLeaveFrame(&(*pVm));` |
|         1 | 1128 | `	}` |
|     20419 | 1129 | `}` |
|         - | 1130 | `/*` |
|         - | 1131 | ` * Compare two functions signature and return the comparison result.` |
|         - | 1132 | ` */` |
|      1186 | 1133 | `static int VmOverloadCompare(SyString *pFirst,SyString *pSecond)` |
|         2 | 1134 | `{` |
|      1188 | 1135 | `	const char *zSend = &pSecond->zString[pSecond->nByte];` |
|      1188 | 1136 | `	const char *zFend = &pFirst->zString[pFirst->nByte];` |
|      1188 | 1137 | `	const char *zSin = pSecond->zString;` |
|      1188 | 1138 | `	const char *zFin = pFirst->zString;` |
|      1188 | 1139 | `	const char *zPtr = zFin;` |
|       593 | 1140 | `	for(;;){` |
|      1188 | 1141 | `		if( zFin >= zFend \|\| zSin >= zSend ){` |
|       595 | 1142 | `			break;` |
|         - | 1143 | `		}` |
|       ! 0 | 1144 | `		if( zFin[0] != zSin[0] ){` |
|         - | 1145 | `			/* mismatch */` |
|       ! 0 | 1146 | `			break;` |
|         - | 1147 | `		}` |
|       ! 0 | 1148 | `		zFin++;` |
|       ! 0 | 1149 | `		zSin++;` |
|       ! 0 | 1150 | `	}` |
|      1188 | 1151 | `	return (int)(zFin-zPtr);` |
|         2 | 1152 | `}` |
|         - | 1153 | `/*` |
|         - | 1154 | ` * Select the appropriate VM function for the current call context.` |
|         - | 1155 | ` * This is the implementation of the powerful 'function overloading' feature` |
|         - | 1156 | ` * introduced by the version 2 of the PH7 engine.` |
|         - | 1157 | ` * Refer to the official documentation for more information.` |
|         - | 1158 | ` */` |
|       284 | 1159 | `PH7_PRIVATE ph7_vm_func * VmOverload(` |
|         - | 1160 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 1161 | `	ph7_vm_func *pList,  /* Linked list of candidates for overloading */` |
|         - | 1162 | `	ph7_value *aArg,     /* Array of passed arguments */` |
|         - | 1163 | `	int nArg             /* Total number of passed arguments  */` |
|         - | 1164 | `	)` |
|         5 | 1165 | `{` |
|         - | 1166 | `	int iTarget,i,j,iCur,iMax;` |
|         - | 1167 | `	ph7_vm_func *apSet[10];   /* Maximum number of candidates */` |
|         - | 1168 | `	ph7_vm_func *pLink;` |
|         - | 1169 | `	SyString sArgSig;` |
|         - | 1170 | `	SyBlob sSig;` |
|         - | 1171 |  |
|       289 | 1172 | `	pLink = pList;` |
|       289 | 1173 | `	i = 0;` |
|         - | 1174 | `	/* Put functions expecting the same number of passed arguments */` |
|      1891 | 1175 | `	while( i < (int)SX_ARRAYSIZE(apSet) ){` |
|      1823 | 1176 | `		if( pLink == 0 ){` |
|       220 | 1177 | `			break;` |
|         - | 1178 | `		}` |
|      1607 | 1179 | `		if( (int)SySetUsed(&pLink->aArgs) == nArg ){` |
|         - | 1180 | `			/* Candidate for overloading */` |
|      1607 | 1181 | `			apSet[i++] = pLink;` |
|       801 | 1182 | `		}` |
|         - | 1183 | `		/* Point to the next entry */` |
|      1607 | 1184 | `		pLink = pLink->pNextName;` |
|         5 | 1185 | `	}` |
|       289 | 1186 | `	if( i < 1 ){` |
|         - | 1187 | `		/* No candidates,return the head of the list */` |
|       ! 0 | 1188 | `		return pList;` |
|         - | 1189 | `	}` |
|       289 | 1190 | `	if( nArg < 1 \|\| i < 2 ){` |
|         - | 1191 | `		/* Return the only candidate */` |
|        61 | 1192 | `		return apSet[0];` |
|         - | 1193 | `	}` |
|         - | 1194 | `	/* Calculate function signature */` |
|       230 | 1195 | `	SyBlobInit(&sSig,&pVm->sAllocator);` |
|       458 | 1196 | `	for( j = 0 ; j < nArg ; j++ ){` |
|       230 | 1197 | `		int c = 'n'; /* null */` |
|       230 | 1198 | `		if( aArg[j].iFlags & MEMOBJ_HASHMAP ){` |
|         - | 1199 | `			/* Hashmap */` |
|       ! 0 | 1200 | `			c = 'h';` |
|       230 | 1201 | `		}else if( aArg[j].iFlags & MEMOBJ_BOOL ){` |
|         - | 1202 | `			/* bool */` |
|        85 | 1203 | `			c = 'b';` |
|       188 | 1204 | `		}else if( aArg[j].iFlags & MEMOBJ_INT ){` |
|         - | 1205 | `			/* int */` |
|        48 | 1206 | `			c = 'i';` |
|       122 | 1207 | `		}else if( aArg[j].iFlags & MEMOBJ_STRING ){` |
|         - | 1208 | `			/* String */` |
|        87 | 1209 | `			c = 's';` |
|        56 | 1210 | `		}else if( aArg[j].iFlags & MEMOBJ_REAL ){` |
|         - | 1211 | `			/* Float */` |
|        11 | 1212 | `			c = 'f';` |
|         8 | 1213 | `		}else if( aArg[j].iFlags & MEMOBJ_OBJ ){` |
|         - | 1214 | `			/* Class instance — prefix with 'o' to match formal object/class signatures */` |
|       ! 0 | 1215 | `			int marker = 'o';` |
|       ! 0 | 1216 | `			ph7_class *pClass = ((ph7_class_instance *)aArg[j].x.pOther)->pClass;` |
|       ! 0 | 1217 | `			SyString *pName = &pClass->sName;` |
|       ! 0 | 1218 | `			SyBlobAppend(&sSig,(const void *)&marker,sizeof(char));` |
|       ! 0 | 1219 | `			SyBlobAppend(&sSig,(const void *)pName->zString,pName->nByte);` |
|       ! 0 | 1220 | `			c = -1;` |
|       ! 0 | 1221 | `		}` |
|       230 | 1222 | `		if( c > 0 ){` |
|       230 | 1223 | `			SyBlobAppend(&sSig,(const void *)&c,sizeof(char));` |
|       114 | 1224 | `		}` |
|       116 | 1225 | `	}` |
|       230 | 1226 | `	SyStringInitFromBuf(&sArgSig,SyBlobData(&sSig),SyBlobLength(&sSig));` |
|       230 | 1227 | `	iTarget = 0;` |
|       230 | 1228 | `	iMax = -1;` |
|         - | 1229 | `	/* Select the appropriate function */` |
|      1416 | 1230 | `	for( j = 0 ; j < i ; j++ ){` |
|         - | 1231 | `		/* Compare the two signatures */` |
|      1188 | 1232 | `		iCur = VmOverloadCompare(&sArgSig,&apSet[j]->sSignature);` |
|      1188 | 1233 | `		if( iCur > iMax ){` |
|       230 | 1234 | `			iMax = iCur;` |
|       230 | 1235 | `			iTarget = j;` |
|       114 | 1236 | `		}` |
|       595 | 1237 | `	}` |
|       230 | 1238 | `	SyBlobRelease(&sSig);` |
|         - | 1239 | `	/* Appropriate function for the current call context */` |
|       230 | 1240 | `	return apSet[iTarget];` |
|       147 | 1241 | `}` |
|         - | 1242 | `/* Forward declaration */` |
|         - | 1243 | `/* VmLocalExec and VmErrorFormat forward declarations removed - now PH7_PRIVATE in ph7int.h */` |
|         - | 1244 | `/*` |
|         - | 1245 | ` * Evaluate a constant/default initializer bytecode into a pool memory-object slot,` |
|         - | 1246 | ` * safely across a pool reallocation.` |
|         - | 1247 | ` *` |
|         - | 1248 | ` * VmLocalExec writes its result through the caller's pResult pointer at` |
|         - | 1249 | ` * end-of-exec. When pResult is a slot in the growable aMemObj pool AND the` |
|         - | 1250 | ` * initializer allocates pool memobjs — a large array literal grows aMemObj via` |
|         - | 1251 | ` * PH7_ReserveMemObj (per element), reallocating and FREEING the pool buffer —` |
|         - | 1252 | ` * the reserved pResult pointer dangles and the final store is a heap` |
|         - | 1253 | ` * use-after-free (confirmed via ASan on a >=~227-element class-const array; it` |
|         - | 1254 | ` * is what blocked Composer's autoload class-map). Evaluate into a stable local` |
|         - | 1255 | ` * instead, then store into the slot re-fetched by its (stable) index. On return` |
|         - | 1256 | ` * *ppMemObj points at the valid post-eval slot. PH7_MemObjStore preserves the` |
|         - | 1257 | ` * destination slot's nIdx (excluded from its memcpy), so the slot identity is` |
|         - | 1258 | ` * kept. Mirrors the enum-case backing path, which already evaluates into a local.` |
|         - | 1259 | ` */` |
|      2982 | 1260 | `PH7_PRIVATE sxi32 VmLocalExecIntoObj(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj,int bReturnPropagates)` |
|         5 | 1261 | `{` |
|         - | 1262 | `	ph7_value sVal;` |
|      2987 | 1263 | `	sxu32 nIdx = (*ppMemObj)->nIdx;` |
|         - | 1264 | `	sxi32 rc;` |
|      2987 | 1265 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|      2987 | 1266 | `	rc = VmLocalExec(&(*pVm),pByteCode,&sVal,bReturnPropagates);` |
|         - | 1267 | `	/* aMemObj may have moved during the eval — re-fetch by the reserved index. */` |
|      2987 | 1268 | `	*ppMemObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|      2987 | 1269 | `	if( *ppMemObj ){` |
|      2987 | 1270 | `		PH7_MemObjStore(&sVal,*ppMemObj);` |
|      1491 | 1271 | `	}` |
|      2987 | 1272 | `	PH7_MemObjRelease(&sVal);` |
|      2987 | 1273 | `	return rc;` |
|         5 | 1274 | `}` |
|         - | 1275 | `/*` |
|         - | 1276 | ` * The two halves of the THROW MUTING both callers below share. Hiding the live` |
|         - | 1277 | ` * try activations is what makes a swallowed throw local: a callee's OWN tries` |
|         - | 1278 | ` * push onto the emptied set and still catch normally, while nothing OUTSIDE the` |
|         - | 1279 | ` * muted region can see the throw — which matters because PHL dispatches a throw` |
|         - | 1280 | ` * raised under a C call site INLINE, running an enclosing user catch before the` |
|         - | 1281 | ` * C caller ever regains control.` |
|         - | 1282 | ` */` |
|         - | 1283 | `typedef struct VmMuteState {` |
|         - | 1284 | `	ph7_exception **apSaved;   /* try activations hidden for the duration */` |
|         - | 1285 | `	sxu32 nSaved;` |
|         - | 1286 | `	sxi32 iSaveStatus;` |
|         - | 1287 | `	sxi32 iSaveBoundary;` |
|         - | 1288 | `	VmFrame *pSaveResume;` |
|         - | 1289 | `	ph7_class_attr *pSaveCycleAttr;` |
|         - | 1290 | `	ph7_class *pSaveCycleClass;` |
|         - | 1291 | `} VmMuteState;` |
|       280 | 1292 | `static void VmMuteEnter(ph7_vm *pVm,VmMuteState *pSave)` |
|         5 | 1293 | `{` |
|       285 | 1294 | `	pSave->apSaved = 0;` |
|       285 | 1295 | `	pSave->nSaved = SySetUsed(&pVm->aException);` |
|       285 | 1296 | `	pSave->iSaveStatus = pVm->iExitStatus;` |
|       285 | 1297 | `	pSave->iSaveBoundary = pVm->nBoundaryRc;` |
|       285 | 1298 | `	pSave->pSaveResume = pVm->pResumeFrame;` |
|       285 | 1299 | `	pSave->pSaveCycleAttr = pVm->pConstCycleAttr;` |
|       285 | 1300 | `	pSave->pSaveCycleClass = pVm->pConstCycleClass;` |
|       285 | 1301 | `	if( pSave->nSaved > 0 ){` |
|        34 | 1302 | `		pSave->apSaved = (ph7_exception **)SyMemBackendAlloc(&pVm->sAllocator,` |
|        16 | 1303 | `			pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1304 | `		if( pSave->apSaved ){` |
|        26 | 1305 | `			SyMemcpy(SySetBasePtr(&pVm->aException),pSave->apSaved,` |
|        16 | 1306 | `				pSave->nSaved * sizeof(ph7_exception *));` |
|        18 | 1307 | `			SySetReset(&pVm->aException);` |
|         8 | 1308 | `		}` |
|         8 | 1309 | `	}` |
|       285 | 1310 | `	pVm->nMuteThrow++;` |
|       285 | 1311 | `}` |
|         - | 1312 | `/*` |
|         - | 1313 | ` * Undo everything a swallowed throw stamped: the frame flag an enclosing` |
|         - | 1314 | ` * execution would read as "an unwind is in progress", the uncaught exit status,` |
|         - | 1315 | ` * the C-boundary park and any recorded in-place-catch resume target. Returns` |
|         - | 1316 | ` * TRUE when a throw was actually swallowed.` |
|         - | 1317 | ` */` |
|       280 | 1318 | `static int VmMuteLeave(ph7_vm *pVm,VmMuteState *pSave,sxi32 rc)` |
|         5 | 1319 | `{` |
|         - | 1320 | `	VmFrame *pFrame;` |
|       285 | 1321 | `	pVm->nMuteThrow--;` |
|         - | 1322 | `	/* Nothing may be left on the hidden stack (the muted region has no try of its` |
|         - | 1323 | `	 * own that survives it), but a muted throw unwinding out of one would leave an` |
|         - | 1324 | `	 * activation behind: release whatever is there whether or not anything was` |
|         - | 1325 | `	 * hidden. */` |
|       285 | 1326 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|       285 | 1327 | `	SySetReset(&pVm->aException);` |
|       285 | 1328 | `	if( pSave->apSaved ){` |
|         - | 1329 | `		sxu32 k;` |
|        34 | 1330 | `		for( k = 0 ; k < pSave->nSaved ; ++k ){` |
|        18 | 1331 | `			SySetPut(&pVm->aException,(const void *)&pSave->apSaved[k]);` |
|        10 | 1332 | `		}` |
|        18 | 1333 | `		SyMemBackendFree(&pVm->sAllocator,pSave->apSaved);` |
|        18 | 1334 | `		pSave->apSaved = 0;` |
|         8 | 1335 | `	}` |
|       285 | 1336 | `	if( rc != PH7_EXCEPTION && rc != PH7_ABORT ){` |
|       239 | 1337 | `		return FALSE;` |
|         - | 1338 | `	}` |
|        50 | 1339 | `	pFrame = pVm->pFrame;` |
|        50 | 1340 | `	if( pFrame ){` |
|        50 | 1341 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        50 | 1342 | `		pFrame->iFlags &= ~VM_FRAME_THROW;` |
|        23 | 1343 | `	}` |
|        50 | 1344 | `	pVm->iExitStatus = pSave->iSaveStatus;` |
|        50 | 1345 | `	pVm->nBoundaryRc = pSave->iSaveBoundary;` |
|        50 | 1346 | `	pVm->pResumeFrame = pSave->pSaveResume;` |
|        50 | 1347 | `	pVm->pConstCycleAttr = pSave->pSaveCycleAttr;` |
|        50 | 1348 | `	pVm->pConstCycleClass = pSave->pSaveCycleClass;` |
|        50 | 1349 | `	return TRUE;` |
|       145 | 1350 | `}` |
|         - | 1351 | `/*` |
|         - | 1352 | ` * Call a class method from C and SWALLOW any throw it raises — php's` |
|         - | 1353 | ` * zend_clear_exception() at a C call site, which nothing else here can spell.` |
|         - | 1354 | ` * *pbThrew (optional) reports whether one was swallowed; the return status is` |
|         - | 1355 | ` * SXRET_OK either way, because to the caller a swallowed throw is not a failure.` |
|         - | 1356 | ` *` |
|         - | 1357 | ` * RecursiveIteratorIterator's RIT_CATCH_GET_CHILD is the first user: php clears` |
|         - | 1358 | ` * the exception a hasChildren()/getChildren() raised and carries the traversal` |
|         - | 1359 | ` * on to the next element. Reach for this ONLY where php itself clears — a` |
|         - | 1360 | ` * swallowed throw is invisible, and every other C call site wants the status.` |
|         - | 1361 | ` */` |
|        12 | 1362 | `PH7_PRIVATE sxi32 PH7_VmCallMethodSwallow(` |
|         - | 1363 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 1364 | `	ph7_class_instance *pThis,   /* Receiver */` |
|         - | 1365 | `	ph7_class_method *pMethod,   /* Method to run */` |
|         - | 1366 | `	ph7_value *pResult,          /* OUT: return value, or 0 */` |
|         - | 1367 | `	int nArg,                    /* Argument count */` |
|         - | 1368 | `	ph7_value **apArg,           /* Arguments */` |
|         - | 1369 | `	int *pbThrew                 /* OUT: TRUE if a throw was swallowed, or 0 */` |
|         - | 1370 | `	)` |
|         1 | 1371 | `{` |
|         - | 1372 | `	VmMuteState sSave;` |
|         - | 1373 | `	sxi32 rc;` |
|        13 | 1374 | `	VmMuteEnter(&(*pVm),&sSave);` |
|        13 | 1375 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pMethod,pResult,nArg,apArg);` |
|        13 | 1376 | `	if( VmMuteLeave(&(*pVm),&sSave,rc) ){` |
|         3 | 1377 | `		if( pbThrew ){` |
|         3 | 1378 | `			*pbThrew = TRUE;` |
|         1 | 1379 | `		}` |
|         3 | 1380 | `		return SXRET_OK;` |
|         - | 1381 | `	}` |
|        11 | 1382 | `	if( pbThrew ){` |
|        11 | 1383 | `		*pbThrew = FALSE;` |
|         5 | 1384 | `	}` |
|        11 | 1385 | `	return rc;` |
|         7 | 1386 | `}` |
|         - | 1387 | `/*` |
|         - | 1388 | ` * Evaluate an initializer with the engine's THROW machinery muted: the live` |
|         - | 1389 | ` * try activations are hidden for the duration (so no user catch runs IN PLACE),` |
|         - | 1390 | ` * no exception handler is invoked, no uncaught report is printed, and the exit` |
|         - | 1391 | ` * status, the C-boundary park and any recorded in-place-catch resume are` |
|         - | 1392 | ` * restored on the way out. A throw comes back only as the returned status.` |
|         - | 1393 | ` *` |
|         - | 1394 | ` * This is what lets a class STATIC property's default be evaluated EAGERLY at` |
|         - | 1395 | ` * mount while php evaluates it LAZILY: php has not reached the initializer at` |
|         - | 1396 | ` * declaration time, so a throw there is not the declaration's business. Muted,` |
|         - | 1397 | ` * the failed attempt leaves NO trace — the attribute is flagged` |
|         - | 1398 | ` * PH7_CLASS_ATTR_STATIC_DEFER and the initializer re-runs, unmuted, at the` |
|         - | 1399 | ` * first static-table materialization (PH7_VmMaterializeClassStatics), which is` |
|         - | 1400 | ` * where php raises it. Without the muting the throw would be dispatched here:` |
|         - | 1401 | `` * a `try { include "decl.php"; } catch` ran its catch at the DECLARATION and`` |
|         - | 1402 | ` * then carried on into the include, and a top-level declaration merely stamped` |
|         - | 1403 | ` * exit status 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 1404 | ` */` |
|       268 | 1405 | `static sxi32 VmEvalDefaultMuted(ph7_vm *pVm,SySet *pByteCode,ph7_value **ppMemObj)` |
|         5 | 1406 | `{` |
|         - | 1407 | `	VmMuteState sSave;` |
|         - | 1408 | `	sxi32 rc;` |
|       273 | 1409 | `	VmMuteEnter(&(*pVm),&sSave);` |
|       273 | 1410 | `	rc = VmLocalExecIntoObj(&(*pVm),pByteCode,ppMemObj,FALSE);` |
|       273 | 1411 | `	if( rc == SXRET_OK && pVm->pConstCycleAttr != sSave.pSaveCycleAttr ){` |
|         - | 1412 | `		/* The initializer named a SELF-REFERENCING constant. That does not throw` |
|         - | 1413 | `		 * where it is found — the innermost evaluation only records it for an` |
|         - | 1414 | `		 * outer level to raise — but the value is unusable and php raises at the` |
|         - | 1415 | `		 * access, so report it as a throw: the caller defers, and the re-run` |
|         - | 1416 | `		 * records the cycle again and raises it there. */` |
|       ! 0 | 1417 | `		rc = PH7_EXCEPTION;` |
|       ! 0 | 1418 | `	}` |
|         - | 1419 | `	/* VmMuteLeave rolls the attempt back whole — including pConstCycleAttr, which` |
|         - | 1420 | `	 * a self-referencing constant reached by the abandoned initializer only` |
|         - | 1421 | `	 * RECORDS for an outer level to raise. Left standing it would be raised,` |
|         - | 1422 | `	 * unmuted, by the next attribute whose default happens to succeed — at the` |
|         - | 1423 | `	 * declaration site, and blamed on the wrong member. The deferred re-run` |
|         - | 1424 | `	 * detects the cycle again. */` |
|       273 | 1425 | `	VmMuteLeave(&(*pVm),&sSave,rc);` |
|       273 | 1426 | `	return rc;` |
|         5 | 1427 | `}` |
|         - | 1428 | `/*` |
|         - | 1429 | ` * Mount a compiled class into the freshly created vitual machine so that` |
|         - | 1430 | ` * it can be instanciated from the executed PHP script.` |
|         - | 1431 | ` */` |
|         - | 1432 | `/*` |
|         - | 1433 | ` * Reserve and initialize the static/constant attribute slots of a class.` |
|         - | 1434 | ` * This is the per-execution part of mounting a class: every static/const` |
|         - | 1435 | ` * attribute gets a fresh memory object, its default initializer is run, the` |
|         - | 1436 | ` * slot is pinned in the reference table (VM_REF_IDX_KEEP) and typed static` |
|         - | 1437 | ` * properties register their enforcement slot. It is factored out of` |
|         - | 1438 | ` * VmMountUserClass() so that ph7_vm_reset() can rebuild these slots on a VM` |
|         - | 1439 | ` * reuse without re-installing the (compile-time) methods.` |
|         - | 1440 | ` */` |
|   2004466 | 1441 | `static sxi32 VmMountUserClassAttrs(` |
|         - | 1442 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1443 | `	ph7_class *pClass /* Class whose static/const attributes are mounted */` |
|         - | 1444 | `	)` |
|         5 | 1445 | `{` |
|         - | 1446 | `	ph7_class_attr *pAttr;` |
|         - | 1447 | `	SyHashEntry *pEntry;` |
|         - | 1448 | `	/* Static properties live in hAttr, constants (incl. enum cases) in hConst —` |
|         - | 1449 | `	 * separate php member namespaces. Both need their slots reserved, so mount` |
|         - | 1450 | `	 * over both tables. */` |
|         - | 1451 | `	SyHash *apMount[2];` |
|         - | 1452 | `	int iMount;` |
|   2004471 | 1453 | `	apMount[0] = &pClass->hAttr;` |
|   2004471 | 1454 | `	apMount[1] = &pClass->hConst;` |
|   6013397 | 1455 | `	for( iMount = 0 ; iMount < 2 ; iMount++ ){` |
|         - | 1456 | `	/* Reset the loop cursor */` |
|   4008937 | 1457 | `	SyHashResetLoopCursor(apMount[iMount]);` |
|         - | 1458 | `	/* Process only static and constant attribute */` |
|  14388971 | 1459 | `	while( (pEntry = SyHashGetNextEntry(apMount[iMount])) != 0 ){` |
|         - | 1460 | `		/* Extract the current attribute */` |
|  10380045 | 1461 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10380040 | 1462 | `		if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   7314526 | 1463 | `		 && ((pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|   2125057 | 1464 | `			\|\| (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0) ){` |
|         - | 1465 | `			/* Untyped class constants and enum cases are evaluated LAZILY, on` |
|         - | 1466 | `			 * first access (VmClassConstEvalOnDemand / VmEnumMaterializeCase),` |
|         - | 1467 | `			 * matching php. Eager evaluation here ran BEFORE execution for` |
|         - | 1468 | `			 * top-level classes (PH7_VmMakeReady), so an initializer error` |
|         - | 1469 | `			 * (self-reference, enum backing mismatch) could never reach a` |
|         - | 1470 | `			 * user catch, and initializers referencing constants of a class` |
|         - | 1471 | `			 * mounted later in hash order silently read NULL. TYPED constants` |
|         - | 1472 | `			 * stay eager: php validates them at DECLARATION time ("Cannot use` |
|         - | 1473 | `			 * %s as value for class constant" fatal without any access). */` |
|   4247895 | 1474 | `			continue;` |
|         - | 1475 | `		}` |
|   6132155 | 1476 | `		if( pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT) ){` |
|         - | 1477 | `			ph7_value *pMemObj;` |
|      1389 | 1478 | `			if( pAttr->nIdx != SXU32_HIGH ){` |
|         - | 1479 | `				/* Already materialized (an attr shared with an earlier-mounted` |
|         - | 1480 | `				 * class). PH7_VmReset invalidates every nIdx before its` |
|         - | 1481 | `				 * re-mount pass, so VM reuse still re-evaluates. Propagate a` |
|         - | 1482 | `				 * pending static-default failure — a deferred EVALUATION or a` |
|         - | 1483 | `				 * failed TYPE check — to THIS class too, so a subclass's static` |
|         - | 1484 | `				 * access / instantiation throws like php's. */` |
|      1094 | 1485 | `				if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        26 | 1486 | `					if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER ){` |
|         - | 1487 | `						/* Its default threw at the other class's mount and is` |
|         - | 1488 | `						 * pending re-evaluation (php: the shared slot belongs to` |
|         - | 1489 | `						 * both static tables). */` |
|         3 | 1490 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1491 | `					}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|        11 | 1492 | `						SyHashEntry *pSlotD = SyHashGet(&pVm->hTypedSlot,` |
|         6 | 1493 | `							(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|         8 | 1494 | `						if( pSlotD && (((VmClassAttr *)pSlotD->pUserData)->iState & VM_CLASS_ATTR_TYPE_DEFER) ){` |
|         3 | 1495 | `							pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|         1 | 1496 | `						}` |
|         3 | 1497 | `					}` |
|        11 | 1498 | `				}` |
|      1097 | 1499 | `				continue;` |
|         - | 1500 | `			}` |
|         - | 1501 | `			/* Reserve a memory object for this constant/static attribute */` |
|       299 | 1502 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       299 | 1503 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1504 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|         - | 1505 | `					"Cannot reserve a memory object for class attribute '%z->%z' due to a memory failure",` |
|       ! 0 | 1506 | `					&pClass->sName,&pAttr->sName` |
|         - | 1507 | `					);` |
|       ! 0 | 1508 | `				return SXERR_MEM;` |
|         - | 1509 | `			}` |
|       299 | 1510 | `			if( pAttr->pNativeValue ){` |
|         - | 1511 | `				/* A native class's literal initializer: no expression to run. */` |
|       ! 0 | 1512 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|       299 | 1513 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1514 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1515 | `				 * pConstEvalClass lets self::/parent:: in the initializer` |
|         - | 1516 | `				 * resolve (VmLocalExec runs without a method frame). */` |
|       273 | 1517 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       273 | 1518 | `				int bStaticProp = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0;` |
|         - | 1519 | `				sxi32 rcExec;` |
|       273 | 1520 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|       273 | 1521 | `				pAttr->iFlags \|= PH7_CLASS_ATTR_EVALING; /* cycle guard, shared with the on-demand path */` |
|       273 | 1522 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_STATIC_DEFER; /* re-armed below; matters on a VM reset */` |
|       273 | 1523 | `				pVm->nConstEvalDepth++;` |
|         - | 1524 | `				/* MUTED, for both kinds. php evaluates a class-level initializer` |
|         - | 1525 | `				 * when the member is first USED, so a throw at declaration time is` |
|         - | 1526 | `				 * not something it can see. What reaches this line is a static` |
|         - | 1527 | `				 * property's default (php-lazy throughout) or a TYPED constant's —` |
|         - | 1528 | `				 * which php does validate here, but only when the initializer` |
|         - | 1529 | ``				 * actually produced a value: `const int A = "x"` and`` |
|         - | 1530 | ``				 * `const int A = PHP_EOL` are both declaration-time fatals, while`` |
|         - | 1531 | ``				 * `const int A = UNDEF` says nothing until the constant is read. */`` |
|       273 | 1532 | `				rcExec = VmEvalDefaultMuted(&(*pVm),&pAttr->aByteCode,&pMemObj);` |
|       273 | 1533 | `				pVm->nConstEvalDepth--;` |
|       273 | 1534 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       273 | 1535 | `				pVm->pConstEvalClass = pSaveCtx;` |
|       273 | 1536 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1537 | `					/* php has not reached this initializer: defer it whole to the` |
|         - | 1538 | `					 * first USE, where the throw is raised at the access site and is` |
|         - | 1539 | `					 * catchable there. The leftover value is null and must NOT be` |
|         - | 1540 | `					 * type-checked — a spurious TypeError/fatal would replace the` |
|         - | 1541 | `					 * real Error (the instance path's bDefThrew rule). A static` |
|         - | 1542 | `					 * property keeps its (already reserved) slot and re-runs through` |
|         - | 1543 | `					 * PH7_VmMaterializeClassStatics; a constant gives its slot back` |
|         - | 1544 | `					 * and re-runs through the on-demand path, which is keyed on an` |
|         - | 1545 | `					 * unset nIdx. */` |
|        47 | 1546 | `					if( bStaticProp ){` |
|        41 | 1547 | `						pAttr->iFlags \|= PH7_CLASS_ATTR_STATIC_DEFER;` |
|        41 | 1548 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        25 | 1549 | `					}else{` |
|         - | 1550 | `						VmSlot sSlot;` |
|         - | 1551 | `						/* Release before recycling: PH7_ReserveMemObj re-inits a` |
|         - | 1552 | `						 * reused slot without releasing it, and a muted eval that` |
|         - | 1553 | `						 * only recorded a CYCLE still left its value here. */` |
|         8 | 1554 | `						sSlot.nIdx = pMemObj->nIdx;` |
|         8 | 1555 | `						sSlot.pUserData = 0;` |
|         8 | 1556 | `						PH7_MemObjRelease(pMemObj);` |
|         8 | 1557 | `						SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|         8 | 1558 | `						continue;` |
|         - | 1559 | `					}` |
|       246 | 1560 | `				}else if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED))` |
|       117 | 1561 | `					== (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1562 | `					/* Typed class constant (PHP 8.3): enforce the computed value` |
|         - | 1563 | `					 * against the declared type. A mismatch is a non-catchable` |
|         - | 1564 | `					 * fatal, raised here at definition time (matching PHP). */` |
|        42 | 1565 | `					sxi32 rcType = VmEnforceConstantType(&(*pVm),pClass,pAttr,pMemObj,0 /* at the declaration */);` |
|        42 | 1566 | `					if( rcType != SXRET_OK ){` |
|         9 | 1567 | `						return rcType;` |
|         - | 1568 | `					}` |
|        16 | 1569 | `				}` |
|       128 | 1570 | `			}` |
|         - | 1571 | `			/* Record attribute index */` |
|       287 | 1572 | `			pAttr->nIdx = pMemObj->nIdx;` |
|         - | 1573 | `			/* Install static attribute in the reference table */` |
|       287 | 1574 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1575 | `			/* If this is a typed static property, register the slot so the` |
|         - | 1576 | `			 * STORE path can enforce the declared type. We allocate a tiny` |
|         - | 1577 | `			 * VmClassAttr to uniformize with instance properties; the key` |
|         - | 1578 | `			 * points at its own nIdx field (stable for the VM lifetime).` |
|         - | 1579 | `			 * Typed *constants* are excluded — they are immutable and were` |
|         - | 1580 | `			 * already enforced above, so they need no store-time slot. */` |
|       282 | 1581 | `			if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|       201 | 1582 | `				&& (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|        83 | 1583 | `				VmClassAttr *pVmAttrS = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        83 | 1584 | `				if( pVmAttrS == 0 ){` |
|       ! 0 | 1585 | `					return SXERR_MEM;` |
|         - | 1586 | `				}` |
|        83 | 1587 | `				pVmAttrS->pAttr = pAttr;` |
|        83 | 1588 | `				pVmAttrS->nIdx = pMemObj->nIdx;` |
|        83 | 1589 | `				pVmAttrS->iState = 0;` |
|        83 | 1590 | `				pVmAttrS->pOwner = pClass;` |
|        83 | 1591 | `				pVmAttrS->pInst = 0;   /* the class's own slot: no instance behind it */` |
|         - | 1592 | `				/* Static typed property with no default starts uninitialized` |
|         - | 1593 | `				 * (constants are already excluded by the enclosing condition). */` |
|        83 | 1594 | `				if( SySetUsed(&pAttr->aByteCode) == 0 ){` |
|        19 | 1595 | `					pVmAttrS->iState \|= VM_CLASS_ATTR_UNINIT;` |
|        75 | 1596 | `				}else if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC_DEFER) == 0 ){` |
|         - | 1597 | `					/* The default was evaluated EAGERLY above, but php validates a` |
|         - | 1598 | `					 * typed static default LAZILY at the first static-property` |
|         - | 1599 | `					 * access / instantiation (a never-touched bad default is` |
|         - | 1600 | `					 * silent). Check now WITHOUT throwing — a pass coerces in` |
|         - | 1601 | `					 * place (int -> float widening, whole-real materialization,` |
|         - | 1602 | `					 * matching php's access-time value) and a failure is DEFERRED:` |
|         - | 1603 | `					 * the slot and the class are flagged, and the access sites` |
|         - | 1604 | `					 * throw via PH7_VmMaterializeClassStatics. A default whose own` |
|         - | 1605 | `					 * EVALUATION was deferred (it threw) has no value to check yet:` |
|         - | 1606 | `					 * the materializer checks it after the re-run. */` |
|        68 | 1607 | `					if( VmCheckTypedDefault(&(*pVm),pClass,pAttr,pMemObj) != SXRET_OK ){` |
|        24 | 1608 | `						pVmAttrS->iState \|= VM_CLASS_ATTR_TYPE_DEFER;` |
|        24 | 1609 | `						pClass->iFlags \|= PH7_CLASS_STATIC_DEFER;` |
|        11 | 1610 | `					}` |
|        32 | 1611 | `				}` |
|        83 | 1612 | `				if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttrS) != SXRET_OK ){` |
|       ! 0 | 1613 | `					SyMemBackendPoolFree(&pVm->sAllocator,pVmAttrS);` |
|       ! 0 | 1614 | `					return SXERR_MEM;` |
|         - | 1615 | `				}` |
|        39 | 1616 | `			}` |
|       141 | 1617 | `		}` |
|         5 | 1618 | `	}` |
|   2004468 | 1619 | `	} /* for iMount */` |
|   2004465 | 1620 | `	return SXRET_OK;` |
|   1002238 | 1621 | `}` |
|         - | 1622 | `/*` |
|         - | 1623 | ` * The other half of mounting: install the class's invocable methods. Split from` |
|         - | 1624 | ` * the attribute half above because PH7_VmMakeReady must run it for EVERY class` |
|         - | 1625 | ` * BEFORE any attribute initializer executes — see the two-pass loop there.` |
|         - | 1626 | ` */` |
|   2002506 | 1627 | `static sxi32 VmMountUserClassMethods(` |
|         - | 1628 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1629 | `	ph7_class *pClass /* Class whose methods are installed */` |
|         - | 1630 | `	)` |
|         5 | 1631 | `{` |
|         - | 1632 | `	ph7_class_method *pMeth;` |
|         - | 1633 | `	SyHashEntry *pEntry;` |
|         - | 1634 | `	sxi32 rc;` |
|         - | 1635 | `	/* Install class methods */` |
|   2002511 | 1636 | `	if( pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT) ){` |
|         - | 1637 | `		/* Do not mount interface/trait methods since they are not directly invocable.` |
|         - | 1638 | `		 */` |
|    611201 | 1639 | `		return SXRET_OK;` |
|         - | 1640 | `	}` |
|         - | 1641 | `	/* PHP-4-style constructors (a method named like the class) were REMOVED in` |
|         - | 1642 | `	 * PHP 8.0: such a method is now a plain method, never the constructor. We used` |
|         - | 1643 | `` 	 * to alias it to __construct here, which made `new C` on `class C{function c(){}}` `` |
|         - | 1644 | `	 * invoke c() as the ctor (and a required param there fataled at construction).` |
|         - | 1645 | `	 * No alias now — only an explicit __construct is the constructor. */` |
|         - | 1646 | `	/* Install the methods now */` |
|   1391315 | 1647 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  24125382 | 1648 | `	while((pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  22038417 | 1649 | `		pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  22038417 | 1650 | `		if( (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0 ){` |
|  21998715 | 1651 | `			rc = PH7_VmInstallUserFunction(&(*pVm),&pMeth->sFunc,&pMeth->sVmName);` |
|  21998715 | 1652 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1653 | `				return rc;` |
|         - | 1654 | `			}` |
|  10999355 | 1655 | `		}` |
|         5 | 1656 | `	}` |
|         - | 1657 | `	/* Mark class as mounted to avoid redundant mounting */` |
|   1391315 | 1658 | `	pClass->bMounted = TRUE;` |
|   1391315 | 1659 | `	return SXRET_OK;` |
|   1001258 | 1660 | `}` |
|   1241566 | 1661 | `PH7_PRIVATE sxi32 VmMountUserClass(` |
|         - | 1662 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 1663 | `	ph7_class *pClass /* Class to be mounted */` |
|         - | 1664 | `	)` |
|         5 | 1665 | `{` |
|         - | 1666 | `	/* Reserve/initialize the static and constant attribute slots, then install` |
|         - | 1667 | `	 * the methods. Mid-execution mounts (include/require, a deferred declaration)` |
|         - | 1668 | `	 * take this whole-class form: every builtin class is mounted by then, so an` |
|         - | 1669 | `	 * initializer that throws finds the exception classes ready. */` |
|   1241571 | 1670 | `	sxi32 rc = VmMountUserClassAttrs(&(*pVm),pClass);` |
|   1241571 | 1671 | `	if( rc != SXRET_OK ){` |
|         3 | 1672 | `		return rc;` |
|         - | 1673 | `	}` |
|   1241569 | 1674 | `	return VmMountUserClassMethods(&(*pVm),pClass);` |
|    620788 | 1675 | `}` |
|         - | 1676 | `/*` |
|         - | 1677 | ` * Allocate a private frame for attributes of the given` |
|         - | 1678 | ` * class instance (Object in the PHP jargon).` |
|         - | 1679 | ` */` |
|   1587831 | 1680 | `PH7_PRIVATE sxi32 PH7_VmCreateClassInstanceFrame(` |
|         - | 1681 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 1682 | `	ph7_class_instance *pObj /* Class instance */` |
|         - | 1683 | `	)` |
|         5 | 1684 | `{` |
|   1587836 | 1685 | `	ph7_class *pClass = pObj->pClass;` |
|         - | 1686 | `	ph7_class_attr *pAttr;` |
|         - | 1687 | `	SyHashEntry *pEntry;` |
|         - | 1688 | `	sxi32 rc;` |
|   1587836 | 1689 | `	int bDefThrew = 0; /* one throw per instantiation: php aborts construction` |
|         - | 1690 | `	                    * at the FIRST bad default; a second registered throw` |
|         - | 1691 | `	                    * would escape the catch as an uncaught fatal. */` |
|         - | 1692 | `	/* Install class attribute in the private frame associated with this instance */` |
|   1587836 | 1693 | `	SyHashResetLoopCursor(&pClass->hAttr);` |
|  11885049 | 1694 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|         - | 1695 | `		VmClassAttr *pVmAttr;` |
|         - | 1696 | `		/* Extract the current attribute */` |
|  10297218 | 1697 | `		pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  10297218 | 1698 | `		pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|  10297218 | 1699 | `		if( pVmAttr == 0 ){` |
|       ! 0 | 1700 | `			return SXERR_MEM;` |
|         - | 1701 | `		}` |
|  10297218 | 1702 | `		pVmAttr->pAttr = pAttr;` |
|  10297218 | 1703 | `		if( (pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC)) == 0 ){` |
|         - | 1704 | `			ph7_value *pMemObj;` |
|         - | 1705 | `			/* Reserve a memory object for this attribute */` |
|  10297058 | 1706 | `			pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|  10297058 | 1707 | `			if( pMemObj == 0 ){` |
|       ! 0 | 1708 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1709 | `				return SXERR_MEM;` |
|         - | 1710 | `			}` |
|  10297058 | 1711 | `			pVmAttr->nIdx = pMemObj->nIdx;` |
|  10297058 | 1712 | `			pVmAttr->iState = 0;` |
|  10297058 | 1713 | `			pVmAttr->pOwner = pClass;` |
|  10297058 | 1714 | `			pVmAttr->pInst = pObj;` |
|  10297058 | 1715 | `			if( pAttr->pNativeValue ){` |
|         - | 1716 | `				/* Native class, literal default: no initializer to execute, so none` |
|         - | 1717 | `				 * of the throw/typed-default machinery below can apply either — a` |
|         - | 1718 | `				 * literal cannot throw and the builder states the type itself. */` |
|   9832787 | 1719 | `				PH7_NativeLiteralValue(&(*pVm),pAttr->pNativeValue,pMemObj);` |
|   5380664 | 1720 | `			}else if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|         - | 1721 | `				/* Initialize attribute default value (any complex expression).` |
|         - | 1722 | `				 * pConstEvalClass: self::CONST in a property default resolves` |
|         - | 1723 | `				 * against the declaring class (no method frame here). */` |
|      2291 | 1724 | `				ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|         - | 1725 | `				sxi32 rcExec;` |
|      2291 | 1726 | `				pVm->pConstEvalClass = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      2291 | 1727 | `				rcExec = VmLocalExecIntoObj(&(*pVm),&pAttr->aByteCode,&pMemObj,FALSE);` |
|      2291 | 1728 | `				pVm->pConstEvalClass = pSaveCtx;` |
|      2291 | 1729 | `				if( rcExec == PH7_EXCEPTION \|\| rcExec == PH7_ABORT ){` |
|         - | 1730 | `					/* The initializer itself threw (undefined constant, throwing` |
|         - | 1731 | `					 * enum case): its exception is already registered — do NOT` |
|         - | 1732 | `					 * also type-check the leftover value (a spurious second` |
|         - | 1733 | `					 * TypeError would escape the user's catch), and throw` |
|         - | 1734 | `					 * nothing further for the remaining attributes.` |
|         - | 1735 | `					 *` |
|         - | 1736 | `					 * PARK the status too, exactly as the typed-default branch` |
|         - | 1737 | `					 * below does. The initializer is a mini-program (VmLocalExec)` |
|         - | 1738 | `					 * sharing this frame, so an enclosing try caught it IN PLACE` |
|         - | 1739 | `					 * and the throw came back as a status nobody read: this` |
|         - | 1740 | `					 * function answered SXRET_OK, so OP_NEW finished the object` |
|         - | 1741 | `					 * and the whole statement RESUMED after the catch — php` |
|         - | 1742 | `					 * abandons it. Parking hands the same status to OP_NEW's` |
|         - | 1743 | `					 * existing construction-aborted route. */` |
|        23 | 1744 | `					VmBoundaryPark(&(*pVm),rcExec);` |
|        23 | 1745 | `					bDefThrew = 1;` |
|      2280 | 1746 | `				}else if( !bDefThrew && (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|         - | 1747 | `					/* Typed property DEFAULT: php validates the computed value` |
|         - | 1748 | `					 * with the typed-CONSTANT rule (exact match or int->float` |
|         - | 1749 | ``					 * widening — no weak coercion, `public int $p = "5"` throws)`` |
|         - | 1750 | `					 * as a CATCHABLE TypeError when the default materializes,` |
|         - | 1751 | ``					 * i.e. here at `new`. Park the throw for OP_NEW (which`` |
|         - | 1752 | `					 * aborts construction) / the fetch-point router. */` |
|       329 | 1753 | `					sxi32 rcDef = VmEnforceTypedDefault(&(*pVm),pClass,pAttr,pMemObj);` |
|       329 | 1754 | `					if( rcDef != SXRET_OK ){` |
|        13 | 1755 | `						VmBoundaryPark(&(*pVm),rcDef);` |
|        13 | 1756 | `						bDefThrew = 1;` |
|         6 | 1757 | `					}` |
|       167 | 1758 | `				}` |
|    463133 | 1759 | `			}else if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         - | 1760 | `				/* Typed property without a default: mark uninitialized. Reading` |
|         - | 1761 | `				 * it before the first write is an Error in PHP 7.4+. */` |
|    460964 | 1762 | `				pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|    230479 | 1763 | `			}` |
|  10297058 | 1764 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|  10297058 | 1765 | `			if( rc != SXRET_OK ){` |
|         - | 1766 | `				VmSlot sSlot;` |
|         - | 1767 | `				/* Restore memory object */` |
|       ! 0 | 1768 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1769 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1770 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1771 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1772 | `				return SXERR_MEM;` |
|         - | 1773 | `			}` |
|         - | 1774 | `			/* Install attribute in the reference table */` |
|  10297058 | 1775 | `			PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|         - | 1776 | `			/* Register the slot with the store filter -- a declared TYPE to` |
|         - | 1777 | `			 * enforce, a native class's write handler, or both. On failure roll` |
|         - | 1778 | `			 * back the just-installed hAttr entry and the reserved memobj so the` |
|         - | 1779 | `			 * caller sees a consistent instance. */` |
|  10297058 | 1780 | `			rc = PH7_VmStoreFilterRegister(&(*pVm),pVmAttr);` |
|  10297058 | 1781 | `			if( rc != SXRET_OK ){` |
|         - | 1782 | `				VmSlot sSlot;` |
|       ! 0 | 1783 | `				SyHashDeleteEntry(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 1784 | `				sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1785 | `				sSlot.pUserData = 0;` |
|       ! 0 | 1786 | `				SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1787 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1788 | `				return SXERR_MEM;` |
|         - | 1789 | `			}` |
|   5148528 | 1790 | `		}else{` |
|         - | 1791 | `			/* Install static/constant attribute */` |
|       165 | 1792 | `			pVmAttr->nIdx = pAttr->nIdx;` |
|       165 | 1793 | `			pVmAttr->iState = 0;` |
|       165 | 1794 | `			pVmAttr->pOwner = pClass;` |
|       165 | 1795 | `			pVmAttr->pInst = 0;   /* a static slot belongs to the class, not to this object */` |
|       165 | 1796 | `			rc = SyHashInsertTail(&pObj->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr);` |
|       165 | 1797 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 1798 | `				SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1799 | `				return SXERR_MEM;` |
|         - | 1800 | `			}` |
|         - | 1801 | `		}` |
|         5 | 1802 | `	}` |
|   1587836 | 1803 | `	return SXRET_OK;` |
|    793920 | 1804 | `}` |
|         - | 1805 | `/*` |
|         - | 1806 | ` * Whether [pClass] permits runtime-created (dynamic) properties. Scoped to` |
|         - | 1807 | ` * stdClass for now; the future general-dynamic-props work turns` |
|         - | 1808 | ` * this into a class-flag / #[AllowDynamicProperties] check at this one site.` |
|         - | 1809 | ` */` |
|       166 | 1810 | `PH7_PRIVATE int VmClassAllowsDynamicProps(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1811 | `{` |
|       171 | 1812 | `	return pVm->pStdClass != 0 && pClass == pVm->pStdClass;` |
|         5 | 1813 | `}` |
|         - | 1814 | `/*` |
|         - | 1815 | ` * Whether pClass carries a #[...] attribute of the given (resolved) name —` |
|         - | 1816 | ` * band A #3b uses it for #[AllowDynamicProperties], which suppresses the` |
|         - | 1817 | ` * php 8.2 dynamic-property deprecation. Inherited attributes do not apply` |
|         - | 1818 | ` * (php: the attribute must be on the class itself... except php DOES honor` |
|         - | 1819 | ` * it on parents for AllowDynamicProperties — walk the ancestry).` |
|         - | 1820 | ` */` |
|        26 | 1821 | `PH7_PRIVATE int VmClassHasAttributeNamed(ph7_class *pClass,const char *zName,sxu32 nName)` |
|         4 | 1822 | `{` |
|        44 | 1823 | `	while( pClass ){` |
|        30 | 1824 | `		ph7_attribute *aAttr = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|         - | 1825 | `		sxu32 n;` |
|        30 | 1826 | `		for( n = 0; n < SySetUsed(&pClass->aAttrs); ++n ){` |
|        12 | 1827 | `			if( aAttr[n].sName.nByte == nName` |
|        13 | 1828 | `			 && SyMemcmp((const void *)aAttr[n].sName.zString,(const void *)zName,nName) == 0 ){` |
|        13 | 1829 | `				return TRUE;` |
|         - | 1830 | `			}` |
|       ! 0 | 1831 | `		}` |
|        18 | 1832 | `		pClass = pClass->pBase;` |
|         4 | 1833 | `	}` |
|        18 | 1834 | `	return FALSE;` |
|        17 | 1835 | `}` |
|         - | 1836 | `/*` |
|         - | 1837 | ` * Is [pClass] the __PHP_Incomplete_Class carrier? Every script-level property` |
|         - | 1838 | ` * access or method call on such an instance is php's incomplete-object` |
|         - | 1839 | ` * diagnostic; only the engine's own surfaces (serialize, var_dump, foreach,` |
|         - | 1840 | ` * (array), get_object_vars) read its attribute table freely.` |
|         - | 1841 | ` */` |
|    243025 | 1842 | `PH7_PRIVATE int PH7_VmIsIncompleteClass(ph7_vm *pVm,ph7_class *pClass)` |
|         5 | 1843 | `{` |
|    243030 | 1844 | `	return pVm->pIncClass != 0 && pClass == pVm->pIncClass;` |
|         5 | 1845 | `}` |
|         - | 1846 | `/*` |
|         - | 1847 | ` * Build php's incomplete-object diagnostic body into pOut. zWhat is the verb` |
|         - | 1848 | ` * phrase php varies — "access a property" (E_WARNING), "modify a property" /` |
|         - | 1849 | ` * "call a method" (both Error) — and the class named is the ORIGINAL one the` |
|         - | 1850 | ` * payload spelled, read from the magic member; a hand-built carrier that never` |
|         - | 1851 | ` * had one says "unknown", like php.` |
|         - | 1852 | ` */` |
|        44 | 1853 | `PH7_PRIVATE void PH7_VmIncompleteMsg(ph7_vm *pVm,ph7_class_instance *pThis,const char *zWhat,SyBlob *pOut)` |
|         1 | 1854 | `{` |
|        45 | 1855 | `	const char *zName = "unknown";` |
|        45 | 1856 | `	sxu32 nName = sizeof("unknown")-1;` |
|        45 | 1857 | `	SyHashEntry *pEntry = SyHashGet(&pThis->hAttr,` |
|         - | 1858 | `		(const void *)PH7_INCOMPLETE_MAGIC_MEMBER,sizeof(PH7_INCOMPLETE_MAGIC_MEMBER)-1);` |
|        45 | 1859 | `	if( pEntry ){` |
|        45 | 1860 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|        45 | 1861 | `		ph7_value *pVal = pVmAttr ? (ph7_value *)SySetAt(&pVm->aMemObj,pVmAttr->nIdx) : 0;` |
|        45 | 1862 | `		if( pVal && (pVal->iFlags & MEMOBJ_STRING) && SyBlobLength(&pVal->sBlob) > 0 ){` |
|        45 | 1863 | `			zName = (const char *)SyBlobData(&pVal->sBlob);` |
|        45 | 1864 | `			nName = SyBlobLength(&pVal->sBlob);` |
|        22 | 1865 | `		}` |
|        22 | 1866 | `	}` |
|        67 | 1867 | `	SyBlobFormat(pOut,"The script tried to %s on an incomplete object. "` |
|         - | 1868 | `		"Please ensure that the class definition \"%.*s\" of the object you are "` |
|         - | 1869 | `		"trying to operate on was loaded _before_ unserialize() gets called or "` |
|        22 | 1870 | `		"provide an autoloader to load the class definition",zWhat,(int)nName,zName);` |
|        45 | 1871 | `}` |
|         - | 1872 | `/*` |
|         - | 1873 | ` * php's E_WARNING for READING (or isset()-probing) a property of an incomplete` |
|         - | 1874 | `` * object. The message body carries php's `func(): ` docref qualifier: the`` |
|         - | 1875 | `` * CURRENT function for an engine-raised access (`main` at global scope,`` |
|         - | 1876 | `` * `C::m` inside a method), or the builtin's own name when one raises it`` |
|         - | 1877 | ` * (property_exists() passes its name in pFuncName).` |
|         - | 1878 | ` */` |
|        16 | 1879 | `PH7_PRIVATE void PH7_VmIncompleteAccessWarn(ph7_vm *pVm,ph7_class_instance *pThis,SyString *pFuncName)` |
|         1 | 1880 | `{` |
|         - | 1881 | `	SyBlob sMsg;` |
|        17 | 1882 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|        17 | 1883 | `	if( pFuncName ){` |
|       ! 0 | 1884 | `		SyBlobAppend(&sMsg,pFuncName->zString,pFuncName->nByte);` |
|       ! 0 | 1885 | `	}else{` |
|        17 | 1886 | `		VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|        17 | 1887 | `		ph7_vm_func *pFunc = (pFrame && pFrame->pParent) ? (ph7_vm_func *)pFrame->pUserData : 0;` |
|        17 | 1888 | `		if( pFunc == 0 ){` |
|       ! 0 | 1889 | `			SyBlobAppend(&sMsg,"main",sizeof("main")-1);` |
|       ! 0 | 1890 | `		}else{` |
|        17 | 1891 | `			const char *zDisp = 0;` |
|         - | 1892 | `			int nDisp;` |
|        17 | 1893 | `			if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|         3 | 1894 | `				SyString *pCls = &((ph7_class *)pFunc->pUserData)->sName;` |
|         3 | 1895 | `				SyBlobAppend(&sMsg,pCls->zString,pCls->nByte);` |
|         3 | 1896 | `				SyBlobAppend(&sMsg,"::",2);` |
|         1 | 1897 | `			}` |
|        17 | 1898 | `			nDisp = PH7_VmFuncDisplayName(pVm,pFunc,&zDisp);` |
|        17 | 1899 | `			SyBlobAppend(&sMsg,zDisp,(sxu32)nDisp);` |
|         - | 1900 | `		}` |
|         - | 1901 | `	}` |
|        17 | 1902 | `	SyBlobAppend(&sMsg,"(): ",sizeof("(): ")-1);` |
|        17 | 1903 | `	PH7_VmIncompleteMsg(pVm,pThis,"access a property",&sMsg);` |
|        25 | 1904 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"%.*s",` |
|        16 | 1905 | `		(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|        17 | 1906 | `	SyBlobRelease(&sMsg);` |
|        17 | 1907 | `}` |
|         - | 1908 | `/*` |
|         - | 1909 | ` * Create a dynamic (runtime-added) property named [zName:nName] on a class` |
|         - | 1910 | ` * instance and return its freshly reserved value slot (the caller stores the` |
|         - | 1911 | ` * value via PH7_MemObjStore). If [ppAttr] is non-NULL it receives the new` |
|         - | 1912 | ` * VmClassAttr (saving the caller a re-lookup). Returns NULL on OOM.` |
|         - | 1913 | ` *` |
|         - | 1914 | ` * Mirrors the non-static declared-attribute path in PH7_VmCreateClassInstanceFrame,` |
|         - | 1915 | ` * but the ph7_class_attr is SYNTHESIZED and instance-owned: a single allocation` |
|         - | 1916 | ` * holds the attr struct followed by the name bytes (so the SyHash key, which` |
|         - | 1917 | ` * SyHashInsert stores by pointer, stays valid for the entry's lifetime). The` |
|         - | 1918 | ` * attr carries PH7_CLASS_ATTR_DYNAMIC; PH7_ClassInstanceRelease frees it (and its` |
|         - | 1919 | ` * inline name) on that flag — the only place a per-instance pAttr is freed.` |
|         - | 1920 | ` * The property is public + untyped, so the iFlags/sName dereferences in the` |
|         - | 1921 | ` * member-read, type-enforcement and destruction paths all behave normally.` |
|         - | 1922 | ` */` |
|       430 | 1923 | `PH7_PRIVATE ph7_value * PH7_VmCreateDynamicAttr(ph7_vm *pVm,ph7_class_instance *pThis,const char *zName,sxu32 nName,VmClassAttr **ppAttr)` |
|         5 | 1924 | `{` |
|         - | 1925 | `	ph7_class_attr *pAttr;` |
|       435 | 1926 | `	VmClassAttr *pVmAttr = 0;` |
|       435 | 1927 | `	ph7_value *pMemObj = 0;` |
|         - | 1928 | `	char *zCopy;` |
|         - | 1929 | `	/* One block: ph7_class_attr struct + inline NUL-terminated name. */` |
|       435 | 1930 | `	pAttr = (ph7_class_attr *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(ph7_class_attr) + nName + 1);` |
|       435 | 1931 | `	if( pAttr == 0 ){` |
|       ! 0 | 1932 | `		return 0;` |
|         - | 1933 | `	}` |
|       435 | 1934 | `	SyZero(pAttr,sizeof(ph7_class_attr));` |
|       435 | 1935 | `	zCopy = (char *)&pAttr[1];` |
|       435 | 1936 | `	if( nName > 0 ){` |
|       427 | 1937 | `		SyMemcpy((const void *)zName,(void *)zCopy,nName);` |
|       211 | 1938 | `	}` |
|       435 | 1939 | `	zCopy[nName] = 0;` |
|       435 | 1940 | `	SyStringInitFromBuf(&pAttr->sName,zCopy,nName);` |
|       435 | 1941 | `	pAttr->iFlags = PH7_CLASS_ATTR_DYNAMIC;` |
|       435 | 1942 | `	pAttr->iProtection = PH7_CLASS_PROT_PUBLIC;` |
|       435 | 1943 | `	pAttr->pDeclClass = pThis->pClass;` |
|         - | 1944 | `	/* nType / aByteCode / aUnionAlts left zeroed by SyZero: untyped, no default` |
|         - | 1945 | `	 * value, never a union. */` |
|       435 | 1946 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|       435 | 1947 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1948 | `		goto fail_attr;` |
|         - | 1949 | `	}` |
|       435 | 1950 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|       435 | 1951 | `	if( pMemObj == 0 ){` |
|       ! 0 | 1952 | `		goto fail_vmattr;` |
|         - | 1953 | `	}` |
|       435 | 1954 | `	pVmAttr->pAttr = pAttr;` |
|       435 | 1955 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|       435 | 1956 | `	pVmAttr->iState = 0;` |
|       435 | 1957 | `	pVmAttr->pOwner = pThis->pClass;` |
|       435 | 1958 | `	pVmAttr->pInst = pThis;` |
|         - | 1959 | `	/* Tail-insert so iteration (json_encode/foreach/(array)/var_dump) follows` |
|         - | 1960 | `	 * property-creation order, matching PHP. */` |
|       435 | 1961 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|       ! 0 | 1962 | `		goto fail_slot;` |
|         - | 1963 | `	}` |
|         - | 1964 | `	/* Install in the reference table so COW/refcount tracks the slot. */` |
|       435 | 1965 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|       435 | 1966 | `	if( ppAttr ){` |
|       145 | 1967 | `		*ppAttr = pVmAttr;` |
|        70 | 1968 | `	}` |
|       435 | 1969 | `	return pMemObj;` |
|       ! 0 | 1970 | `fail_slot:` |
|         - | 1971 | `	{` |
|         - | 1972 | `		VmSlot sSlot;` |
|       ! 0 | 1973 | `		sSlot.nIdx = pMemObj->nIdx;` |
|       ! 0 | 1974 | `		sSlot.pUserData = 0;` |
|       ! 0 | 1975 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 1976 | `	}` |
|       ! 0 | 1977 | `fail_vmattr:` |
|       ! 0 | 1978 | `	SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 1979 | `fail_attr:` |
|       ! 0 | 1980 | `	SyMemBackendFree(&pVm->sAllocator,pAttr);` |
|       ! 0 | 1981 | `	return 0;` |
|       220 | 1982 | `}` |
|         - | 1983 | `/*` |
|         - | 1984 | ` * Recreate a DECLARED (non-static/non-constant) instance property that was removed by unset() and is` |
|         - | 1985 | ` * now being re-assigned: PHP re-creates it, appended at the end (creation order) like a dynamic` |
|         - | 1986 | ` * property. Unlike PH7_VmCreateDynamicAttr the ph7_class_attr is the CLASS-owned declared attr (no` |
|         - | 1987 | ` * DYNAMIC flag), so PH7_ClassInstanceRelease must NOT free it. Returns the new VmClassAttr via *ppAttr` |
|         - | 1988 | ` * (left untouched on OOM, so the caller still sees pObjAttr==0 and degrades gracefully).` |
|         - | 1989 | ` */` |
|        10 | 1990 | `PH7_PRIVATE void VmRecreateDeclaredAttr(ph7_vm *pVm,ph7_class_instance *pThis,ph7_class_attr *pAttr,VmClassAttr **ppAttr)` |
|         2 | 1991 | `{` |
|         - | 1992 | `	VmClassAttr *pVmAttr;` |
|         - | 1993 | `	ph7_value *pMemObj;` |
|        12 | 1994 | `	pVmAttr = (VmClassAttr *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmClassAttr));` |
|        12 | 1995 | `	if( pVmAttr == 0 ){` |
|       ! 0 | 1996 | `		return;` |
|         - | 1997 | `	}` |
|        12 | 1998 | `	pMemObj = PH7_ReserveMemObj(&(*pVm));` |
|        12 | 1999 | `	if( pMemObj == 0 ){` |
|       ! 0 | 2000 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2001 | `		return;` |
|         - | 2002 | `	}` |
|        12 | 2003 | `	pVmAttr->pAttr = pAttr;` |
|        12 | 2004 | `	pVmAttr->nIdx = pMemObj->nIdx;` |
|        12 | 2005 | `	pVmAttr->iState = 0;` |
|        12 | 2006 | `	pVmAttr->pOwner = pThis->pClass;` |
|        12 | 2007 | `	pVmAttr->pInst = pThis;` |
|         - | 2008 | `	/* Do NOT re-run the declared default initializer. A property recreated after unset() is a fresh` |
|         - | 2009 | `	 * UNDEFINED property — PHP applies the class default only at construction, not on re-creation. The` |
|         - | 2010 | ``	 * reserved slot stays NULL, so a read-modify-write that triggered this (`$o->p += 1`, `.=`, `??=`)`` |
|         - | 2011 | ``	 * sees null/0/"" as PHP does; a plain `$o->p = v` overwrites it either way. For a typed property,`` |
|         - | 2012 | `	 * mark it uninitialized so a read before the (re)assignment is an Error, matching PHP. */` |
|        12 | 2013 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|         3 | 2014 | `		pVmAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|         1 | 2015 | `	}` |
|         - | 2016 | `	/* Tail-insert: a re-created property appends (creation order), consistent with a dynamic prop.` |
|         - | 2017 | `	 * PHP keeps a re-added DECLARED property in its original declared position; replicating that` |
|         - | 2018 | `	 * exactly needs a keep-entry/mark-unset model across every iteration site — deferred. The value` |
|         - | 2019 | `	 * is always correct; only the relative order of a declared prop re-added after unset differs. */` |
|        12 | 2020 | `	if( SyHashInsertTail(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),pVmAttr) != SXRET_OK ){` |
|         - | 2021 | `		VmSlot sSlot;` |
|       ! 0 | 2022 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 2023 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 2024 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2025 | `		return;` |
|         - | 2026 | `	}` |
|        12 | 2027 | `	PH7_VmRefObjInstall(&(*pVm),pMemObj->nIdx,0,0,VM_REF_IDX_KEEP);` |
|        12 | 2028 | `	if( PH7_VmStoreFilterRegister(&(*pVm),pVmAttr) != SXRET_OK ){` |
|         - | 2029 | `		VmSlot sSlot;` |
|       ! 0 | 2030 | `		SyHashDeleteEntry(&pThis->hAttr,SyStringData(&pAttr->sName),SyStringLength(&pAttr->sName),0);` |
|       ! 0 | 2031 | `		sSlot.nIdx = pMemObj->nIdx; sSlot.pUserData = 0;` |
|       ! 0 | 2032 | `		SySetPut(&pVm->aFreeObj,(const void *)&sSlot);` |
|       ! 0 | 2033 | `		SyMemBackendPoolFree(&pVm->sAllocator,pVmAttr);` |
|       ! 0 | 2034 | `		return;` |
|         - | 2035 | `	}` |
|        12 | 2036 | `	if( ppAttr ){` |
|        12 | 2037 | `		*ppAttr = pVmAttr;` |
|         5 | 2038 | `	}` |
|         7 | 2039 | `}` |
|         - | 2040 | `/* Forward declaration */` |
|         - | 2041 | `/*` |
|         - | 2042 | ` * Dummy read-only buffer used for slot reservation.` |
|         - | 2043 | ` */` |
|         - | 2044 | `static const char zDummy[sizeof(ph7_value)] = { 0 }; /* Must be >= sizeof(ph7_value) */` |
|         - | 2045 | `/*` |
|         - | 2046 | ` * Reserve a constant memory object.` |
|         - | 2047 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2048 | ` */` |
|   1398328 | 2049 | `PH7_PRIVATE ph7_value * PH7_ReserveConstObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2050 | `{` |
|         - | 2051 | `	ph7_value *pObj;` |
|         - | 2052 | `	sxi32 rc;` |
|   1398333 | 2053 | `	if( pIndex ){` |
|         - | 2054 | `		/* Object index in the object table */` |
|   1382571 | 2055 | `		*pIndex = SySetUsed(&pVm->aLitObj);` |
|    691283 | 2056 | `	}` |
|         - | 2057 | `	/* Reserve a slot for the new object */` |
|   1398333 | 2058 | `	rc = SySetPut(&pVm->aLitObj,(const void *)zDummy);` |
|   1398333 | 2059 | `	if( rc != SXRET_OK ){` |
|         - | 2060 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2061 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2062 | `		 */` |
|       ! 0 | 2063 | `		return 0;` |
|         - | 2064 | `	}` |
|   1398333 | 2065 | `	pObj = (ph7_value *)SySetPeek(&pVm->aLitObj);` |
|   1398333 | 2066 | `	return pObj;` |
|    699169 | 2067 | `}` |
|         - | 2068 | `/*` |
|         - | 2069 | ` * Reserve a memory object.` |
|         - | 2070 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 2071 | ` */` |
|   3019003 | 2072 | `PH7_PRIVATE ph7_value * VmReserveMemObj(ph7_vm *pVm,sxu32 *pIndex)` |
|         5 | 2073 | `{` |
|         - | 2074 | `	ph7_value *pObj;` |
|         - | 2075 | `	sxi32 rc;` |
|   3019008 | 2076 | `	if( pIndex ){` |
|         - | 2077 | `		/* Object index in the object table */` |
|   3019008 | 2078 | `		*pIndex = SySetUsed(&pVm->aMemObj);` |
|   1509493 | 2079 | `	}` |
|         - | 2080 | `	/* Reserve a slot for the new object */` |
|   3019008 | 2081 | `	rc = SySetPut(&pVm->aMemObj,(const void *)zDummy);` |
|   3019008 | 2082 | `	if( rc != SXRET_OK ){` |
|         - | 2083 | `		/* If the supplied memory subsystem is so sick that we are unable to allocate` |
|         - | 2084 | `		 * a tiny chunk of memory, there is no much we can do here.` |
|         - | 2085 | `		 */` |
|       ! 0 | 2086 | `		return 0;` |
|         - | 2087 | `	}` |
|   3019008 | 2088 | `	pObj = (ph7_value *)SySetPeek(&pVm->aMemObj);` |
|   3019008 | 2089 | `	return pObj;` |
|   1509498 | 2090 | `}` |
|         - | 2091 | `/* Forward declaration */` |
|         - | 2092 | `/* Forward declarations for Fiber C functions */` |
|         - | 2093 | `/* Forward declarations for Fiber/Generator infrastructure */` |
|         - | 2094 | `/* Forward declarations for Generator helpers and C functions */` |
|         - | 2095 | `/*` |
|         - | 2096 | ` * Built-in classes/interfaces and some functions that cannot be implemented` |
|         - | 2097 | ` * directly as foreign functions.` |
|         - | 2098 | ` */` |
|         - | 2099 |  |
|         - | 2100 | `/*` |
|         - | 2101 | ` * Initialize a freshly allocated PH7 Virtual Machine so that we can` |
|         - | 2102 | ` * start compiling the target PHP program.` |
|         - | 2103 | ` */` |
|      5254 | 2104 | `PH7_PRIVATE sxi32 PH7_VmInit(` |
|         - | 2105 | `	 ph7_vm *pVm, /* Initialize this */` |
|         - | 2106 | `	 ph7 *pEngine /* Master engine */` |
|         - | 2107 | `	 )` |
|         5 | 2108 | `{` |
|         - | 2109 | `	ph7_value *pObj;` |
|         - | 2110 | `	sxi32 rc;` |
|         - | 2111 | `	/* Zero the structure */` |
|      5259 | 2112 | `	SyZero(pVm,sizeof(ph7_vm));` |
|         - | 2113 | `	/* Initialize VM fields */` |
|      5259 | 2114 | `	pVm->pEngine = &(*pEngine);` |
|      5259 | 2115 | `	pVm->bGcEnabled = 1; /* php default: the cycle collector is enabled */` |
|         - | 2116 | `	/* php CLI diagnostic-stream defaults: display_errors off (program stdout stays` |
|         - | 2117 | `	 * clean), log_errors on (the log copy goes to stderr). -d/-c and ini_set()` |
|         - | 2118 | `	 * override these; bErrReport is the separate master gate installed by the CLI. */` |
|      5259 | 2119 | `	pVm->bDisplayErrors = 0;` |
|      5259 | 2120 | `	pVm->bLogErrors = 1;` |
|         - | 2121 | `	/* mbstring's substitute character, php's default (the internal encoding` |
|         - | 2122 | `	 * beside it is UTF-8, which is the zero the struct already holds) */` |
|      5259 | 2123 | `	pVm->iMbSubstitute = '?';` |
|      5259 | 2124 | `	SyMemBackendInitFromParent(&pVm->sAllocator,&pEngine->sAllocator);` |
|         - | 2125 | `	/* Instructions containers */` |
|      5259 | 2126 | `	SySetInit(&pVm->aByteCode,&pVm->sAllocator,sizeof(VmInstr));` |
|      5259 | 2127 | `	SySetAlloc(&pVm->aByteCode,0xFF);` |
|      5259 | 2128 | `	pVm->pByteContainer = &pVm->aByteCode;` |
|         - | 2129 | `	/* Object containers */` |
|      5259 | 2130 | `	SySetInit(&pVm->aMemObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5259 | 2131 | `	SySetAlloc(&pVm->aMemObj,0xFF);` |
|         - | 2132 | `	/* Argument-unpacking key capture (see VmSpreadRun/VmSpreadKey in vm.c) */` |
|      5259 | 2133 | `	SySetInit(&pVm->aSpreadRun,&pVm->sAllocator,sizeof(VmSpreadRun));` |
|      5259 | 2134 | `	SySetInit(&pVm->aSpreadKey,&pVm->sAllocator,sizeof(VmSpreadKey));` |
|      5259 | 2135 | `	SyBlobInit(&pVm->sSpreadKeyBlob,&pVm->sAllocator);` |
|      5259 | 2136 | `	SySetInit(&pVm->aEffArgName,&pVm->sAllocator,sizeof(SyString));` |
|         - | 2137 | `	/* Virtual machine internal containers */` |
|      5259 | 2138 | `	SyBlobInit(&pVm->sConsumer,&pVm->sAllocator);` |
|      5259 | 2139 | `	SyBlobInit(&pVm->sWorker,&pVm->sAllocator);` |
|      5259 | 2140 | `	SyBlobInit(&pVm->sLastErrMsg,&pVm->sAllocator);` |
|      5259 | 2141 | `	SyBlobInit(&pVm->sLastErrFile,&pVm->sAllocator);` |
|      5259 | 2142 | `	SySetInit(&pVm->aLitObj,&pVm->sAllocator,sizeof(ph7_value));` |
|      5259 | 2143 | `	SySetAlloc(&pVm->aLitObj,0xFF);` |
|         - | 2144 | ``	/* php FUNCTION names are case-insensitive — `STRLEN("x")`, `MyFn()` and`` |
|         - | 2145 | ``	 * `is_callable('STRLEN')` all resolve, and `function myFn(){} function MYFN(){}` is a`` |
|         - | 2146 | `	 * redeclaration — so both function tables hash and compare case-INSENSITIVELY, exactly` |
|         - | 2147 | `	 * like hClass/hMethod below. Every lookup site (OP_CALL, function_exists, is_callable,` |
|         - | 2148 | `	 * string/array callables, Reflection, the redeclare guards) goes through SyHashGet, so` |
|         - | 2149 | `	 * this one pair of comparators covers them all. The stored KEY keeps the declared` |
|         - | 2150 | ``	 * spelling, which is what `__FUNCTION__` and ReflectionFunction::getName() report.`` |
|         - | 2151 | `	 * (Only the constant tables stay byte-exact: php constants ARE case-sensitive.) */` |
|      5259 | 2152 | `	SyHashInit(&pVm->hHostFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5259 | 2153 | `	SyHashInit(&pVm->hFunction,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5259 | 2154 | `	SyHashInit(&pVm->hClass,&pVm->sAllocator,SyStrHash,SyStrnmicmp);` |
|      5259 | 2155 | `	SyHashInit(&pVm->hConstant,&pVm->sAllocator,0,0);` |
|      5259 | 2156 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|      5259 | 2157 | `	SyHashInit(&pVm->hPDO,&pVm->sAllocator,0,0);` |
|      5259 | 2158 | `	SySetInit(&pVm->aFreeObj,&pVm->sAllocator,sizeof(VmSlot));` |
|      5259 | 2159 | `	SySetInit(&pVm->aSelf,&pVm->sAllocator,sizeof(ph7_class *));` |
|      5259 | 2160 | `	SySetInit(&pVm->aShutdown,&pVm->sAllocator,sizeof(VmShutdownCB));` |
|      5259 | 2161 | `	SySetInit(&pVm->aIniCli,&pVm->sAllocator,sizeof(VmIniEntry));` |
|      5259 | 2162 | `	SySetInit(&pVm->aIniTab,&pVm->sAllocator,sizeof(VmIniSlot));` |
|      5259 | 2163 | `	SySetInit(&pVm->aPersistSock,&pVm->sAllocator,sizeof(VmPersistSock));` |
|      5259 | 2164 | `	pVm->bIniSeeded = 0;` |
|      5259 | 2165 | `	pVm->iSessStatus = 1; /* PHP_SESSION_NONE */` |
|      5259 | 2166 | `	PH7_MemObjInit(&(*pVm),&pVm->sSessHandler);` |
|      5259 | 2167 | `	SyBlobInit(&pVm->sSessData,&pVm->sAllocator);` |
|      5259 | 2168 | `	SyBlobInit(&pVm->sSessId,&pVm->sAllocator);` |
|      5259 | 2169 | `	SyBlobInit(&pVm->sSessName,&pVm->sAllocator);` |
|      5259 | 2170 | `	SyBlobInit(&pVm->sSessPath,&pVm->sAllocator);` |
|      5259 | 2171 | `	SyBlobAppend(&pVm->sSessName,"PHPSESSID",sizeof("PHPSESSID")-1);` |
|      5259 | 2172 | `	SySetInit(&pVm->aAutoload,&pVm->sAllocator,sizeof(VmAutoloadCB));` |
|      5259 | 2173 | `	SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|      5259 | 2174 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|      5259 | 2175 | `	SyHashInit(&pVm->hDirHandle,&pVm->sAllocator,0,0);` |
|      5259 | 2176 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|      5259 | 2177 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|      5259 | 2178 | `	pVm->nResourceIdNext = 1;` |
|      5259 | 2179 | `	SySetInit(&pVm->aException,&pVm->sAllocator,sizeof(ph7_exception *));` |
|      5259 | 2180 | `	SySetInit(&pVm->aMagicGuard,&pVm->sAllocator,sizeof(VmMagicGuard));` |
|      5259 | 2181 | `	pVm->pMagicSetThis = 0;` |
|      5259 | 2182 | `	SyBlobInit(&pVm->sMagicSetName,&pVm->sAllocator);` |
|      5259 | 2183 | `	pVm->pHookSetThis = 0;` |
|      5259 | 2184 | `	pVm->pHookSetAttr = 0;` |
|      5259 | 2185 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5259 | 2186 | `	SySetInit(&pVm->aHookRmw,&pVm->sAllocator,sizeof(VmHookRmw));` |
|      5259 | 2187 | `	pVm->pMagicCallThis = 0;` |
|      5259 | 2188 | `	pVm->pMagicCallClass = 0;` |
|      5259 | 2189 | `	SyBlobInit(&pVm->sMagicCallName,&pVm->sAllocator);` |
|      5259 | 2190 | `	pVm->pIdleCallFrames = 0;` |
|      5259 | 2191 | `	pVm->pIdleOperandStacks = 0;` |
|      5259 | 2192 | `	pVm->nIdleOperandStacks = 0;` |
|      5259 | 2193 | `	pVm->pIdleStackNodes = 0;` |
|      5259 | 2194 | `	SySetInit(&pVm->aFinallyAction,&pVm->sAllocator,sizeof(VmFinallyAction));` |
|      5259 | 2195 | `	pVm->bInlineTryCatch = 1; /* ROOT C: enable inline generator try/catch/finally */` |
|      5259 | 2196 | `	pVm->pPendingException = 0;` |
|      5259 | 2197 | `	pVm->pInflightException = 0;` |
|      5259 | 2198 | `	pVm->nInflightExcBase = 0;` |
|      5259 | 2199 | `	pVm->pResumeFrame = 0;` |
|      5259 | 2200 | `	pVm->iResumePc = 0;` |
|      5259 | 2201 | `	pVm->pResumeInstr = 0;` |
|      5259 | 2202 | `	pVm->iResumeStackDepth = 0;` |
|      5259 | 2203 | `	pVm->nBoundaryRc = 0;` |
|      5259 | 2204 | `	pVm->pConstEvalClass = 0;` |
|      5259 | 2205 | `	pVm->nConstEvalDepth = 0;` |
|      5259 | 2206 | `	pVm->pConstCycleAttr = 0;` |
|      5259 | 2207 | `	pVm->pConstCycleClass = 0;` |
|      5259 | 2208 | `	SySetReset(&pVm->aMagicGuard);` |
|      5259 | 2209 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 2210 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 2211 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 2212 | `	}` |
|      5259 | 2213 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|      5259 | 2214 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 2215 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 2216 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 2217 | `	}` |
|      5259 | 2218 | `	pVm->pHookSetAttr = 0;` |
|      5259 | 2219 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|      5259 | 2220 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 2221 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 2222 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 2223 | `	}` |
|      5259 | 2224 | `	pVm->pMagicCallClass = 0;` |
|      5259 | 2225 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|         - | 2226 | `	/* Configuration containers */` |
|      5259 | 2227 | `	SySetInit(&pVm->aFiles,&pVm->sAllocator,sizeof(SyString));` |
|      5259 | 2228 | `	SySetInit(&pVm->aPaths,&pVm->sAllocator,sizeof(SyString));` |
|      5259 | 2229 | `	SySetInit(&pVm->aIncluded,&pVm->sAllocator,sizeof(SyString));` |
|      5259 | 2230 | `	SySetInit(&pVm->aOB,&pVm->sAllocator,sizeof(VmObEntry));` |
|      5259 | 2231 | `	SySetInit(&pVm->aResponseHeaders,&pVm->sAllocator,sizeof(VmResponseHeader));` |
|      5259 | 2232 | `	pVm->iResponseStatus = 200;` |
|      5259 | 2233 | `	pVm->bHeadersSent = 0;` |
|      5259 | 2234 | `	SySetInit(&pVm->aIOstream,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|      5259 | 2235 | `	SySetInit(&pVm->aSuppressedIo,&pVm->sAllocator,sizeof(ph7_io_stream *));` |
|         - | 2236 | `	/* Error callbacks containers */` |
|      5259 | 2237 | `	PH7_MemObjInit(&(*pVm),&pVm->sExceptionCB);` |
|      5259 | 2238 | `	PH7_MemObjInit(&(*pVm),&pVm->sErrCB);` |
|      5259 | 2239 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|      5259 | 2240 | `	SySetInit(&pVm->aExceptionCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5259 | 2241 | `	SySetInit(&pVm->aErrCBSaved,&pVm->sAllocator,sizeof(VmHandlerSlot));` |
|      5259 | 2242 | `	PH7_MemObjInit(&(*pVm),&pVm->sAssertCallback);` |
|         - | 2243 | `	/* Recursion policy (BYTECODE.md stage 5). PHP call depth is heap-bound since` |
|         - | 2244 | `	 * the iterative executor, so the host default is UNBOUNDED (0) — real PHP runs` |
|         - | 2245 | `	 * deep userland recursion until memory_limit, and so must PHL; an embedder opts` |
|         - | 2246 | `	 * back into a cap via PH7_VM_CONFIG_RECURSION_DEPTH. What still needs guarding` |
|         - | 2247 | `	 * is NATIVE VmByteCodeExec nesting (eval/include towers, coroutine-resume` |
|         - | 2248 | `	 * chains, self-recursive C->PHP callbacks) — sized to the platform stack. */` |
|         - | 2249 | `#if defined(__WINNT__) \|\| defined(__UNIXES__)` |
|      5259 | 2250 | `	pVm->nMaxDepth = 0;         /* host: unbounded PHP call depth (memory-bound) */` |
|      5259 | 2251 | `	pVm->nMaxNativeDepth = 256; /* host: proven the safe ceiling under ASan (the` |
|         - | 2252 | `	                             * usort-in-comparator path overflows at 1024) */` |
|         - | 2253 | `#else` |
|         - | 2254 | `	/* Small-stack embedders (e.g. ESP32 16 KB task, 8 MB PSRAM): PHP recursion is` |
|         - | 2255 | `	 * iterative/heap-bound, but a tiny device still wants a runaway-recursion` |
|         - | 2256 | `	 * bound, so keep a PHP-depth default here (~3.5 KB/frame × 512 ≈ 1.8 MB PSRAM` |
|         - | 2257 | `	 * at max depth — BYTECODE.md §6). The embedder tunes both via config verbs. */` |
|         - | 2258 | `	pVm->nMaxDepth = 512;` |
|         - | 2259 | `	pVm->nMaxNativeDepth = 16;` |
|         - | 2260 | `#endif` |
|         - | 2261 | `	/* Default assertion flags: zend.assertions defaults to -1 on the php CLI, so` |
|         - | 2262 | `	 * assert() is compiled out (a no-op) unless -d zend.assertions=1 turns it on.` |
|         - | 2263 | `	 * ASSERT_ACTIVE (PH7_ASSERT_DISABLE) stays independently enabled, matching php. */` |
|      5259 | 2264 | `	pVm->iAssertFlags = PH7_ASSERT_ZEND_OFF;` |
|         - | 2265 | `	/* JSON return status */` |
|      5259 | 2266 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 2267 | `	/* PRNG context */` |
|      5259 | 2268 | `	SyRandomnessInit(&pVm->sPrng,0,0);` |
|         - | 2269 | `	/* MT19937 is seeded lazily on the first rand()/mt_rand() draw (or eagerly by` |
|         - | 2270 | `	 * srand()/mt_srand()), matching PHP's auto-seed-on-first-use behavior. */` |
|      5259 | 2271 | `	pVm->mtSeeded = FALSE;` |
|         - | 2272 | `	/* Install the null constant */` |
|      5259 | 2273 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5259 | 2274 | `	if( pObj == 0 ){` |
|       ! 0 | 2275 | `		rc = SXERR_MEM;` |
|       ! 0 | 2276 | `		goto Err;` |
|         - | 2277 | `	}` |
|      5259 | 2278 | `	PH7_MemObjInit(pVm,pObj);` |
|         - | 2279 | `	/* Install the boolean TRUE constant */` |
|      5259 | 2280 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5259 | 2281 | `	if( pObj == 0 ){` |
|       ! 0 | 2282 | `		rc = SXERR_MEM;` |
|       ! 0 | 2283 | `		goto Err;` |
|         - | 2284 | `	}` |
|      5259 | 2285 | `	PH7_MemObjInitFromBool(pVm,pObj,1);` |
|         - | 2286 | `	/* Install the boolean FALSE constant */` |
|      5259 | 2287 | `	pObj = PH7_ReserveConstObj(&(*pVm),0);` |
|      5259 | 2288 | `	if( pObj == 0 ){` |
|       ! 0 | 2289 | `		rc = SXERR_MEM;` |
|       ! 0 | 2290 | `		goto Err;` |
|         - | 2291 | `	}` |
|      5259 | 2292 | `	PH7_MemObjInitFromBool(pVm,pObj,0);` |
|         - | 2293 | `	/* Install a shared empty string constant so that every "" literal can` |
|         - | 2294 | `	 * reuse the same slot rather than allocating a new one.` |
|         - | 2295 | `	 * This mirrors the NULL/TRUE/FALSE handling above. */` |
|      5259 | 2296 | `	pObj = PH7_ReserveConstObj(&(*pVm),&pVm->nEmptyStringIdx);` |
|      5259 | 2297 | `	if( pObj == 0 ){` |
|       ! 0 | 2298 | `		rc = SXERR_MEM;` |
|       ! 0 | 2299 | `		goto Err;` |
|         - | 2300 | `	}` |
|      5259 | 2301 | `	PH7_MemObjInitFromString(pVm,pObj,0);` |
|         - | 2302 | `	/* Create the global frame */` |
|      5259 | 2303 | `	rc = VmEnterFrame(&(*pVm),0,0,0);` |
|      5259 | 2304 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2305 | `		goto Err;` |
|         - | 2306 | `	}` |
|         - | 2307 | `	/* Initialize the code generator */` |
|      5259 | 2308 | `	rc = PH7_InitCodeGenerator(pVm,pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5259 | 2309 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2310 | `		goto Err;` |
|         - | 2311 | `	}` |
|         - | 2312 | `	/* VM correctly initialized,set the magic number */` |
|      5259 | 2313 | `	pVm->nMagic = PH7_VM_INIT;` |
|         - | 2314 | `	/* Classes/functions defined by the embedded builtin chunks below are` |
|         - | 2315 | `	 * flagged INTERNAL (Reflection: isInternal() true, getFileName() false). */` |
|      5259 | 2316 | `	pVm->bCompilingBuiltin = 1;` |
|         - | 2317 | `	/* Compile the built-in class library (vm_builtin_lib.c owns the chunk) */` |
|      5259 | 2318 | `	PH7_VmInstallBuiltinLib(&(*pVm));` |
|         - | 2319 | `	/* bCompilingBuiltin stays set until the Reflection library below has` |
|         - | 2320 | `	 * compiled — its classes are internal too. */` |
|         - | 2321 | `	/* Cache built-in interface pointers used on hot dispatch paths */` |
|      5259 | 2322 | `	pVm->pArrayAccessClass = PH7_VmExtractClass(pVm,"ArrayAccess",sizeof("ArrayAccess")-1,0,0);` |
|      5259 | 2323 | `	pVm->pCountableClass   = PH7_VmExtractClass(pVm,"Countable",sizeof("Countable")-1,0,0);` |
|      5259 | 2324 | `	pVm->pStringableClass  = PH7_VmExtractClass(pVm,"Stringable",sizeof("Stringable")-1,0,0);` |
|      5259 | 2325 | `	pVm->pJsonSerializableClass = PH7_VmExtractClass(pVm,"JsonSerializable",sizeof("JsonSerializable")-1,0,0);` |
|      5259 | 2326 | `	pVm->pTraversableClass = PH7_VmExtractClass(pVm,"Traversable",sizeof("Traversable")-1,0,0);` |
|         - | 2327 | `	/* Initialize null-coalesce-assign scratch slot */` |
|      5259 | 2328 | `	pVm->pCoalesceObj = 0;` |
|      5259 | 2329 | `	pVm->bCoalesceArmed = 0;` |
|      5259 | 2330 | `	PH7_MemObjInit(pVm,&pVm->sCoalesceKey);` |
|         - | 2331 | `	/* Declare Fiber -- class, private slots and every method are C now, so it does` |
|         - | 2332 | `	 * not exist until this runs. Cache the pointer only AFTER: a NULL cache here` |
|         - | 2333 | ``	 * segfaults the first `new Fiber`. */`` |
|      5259 | 2334 | `	PH7_VmInstallFiberNative(&(*pVm));` |
|      5259 | 2335 | `	pVm->pFiberClass = PH7_VmExtractClass(pVm,"Fiber",5,0,0);` |
|         - | 2336 | `	/* Declare Closure -- class, the three hidden engine slots and all five methods` |
|         - | 2337 | `	 * are C now, so it does not exist until this runs. Cache the pointer only AFTER` |
|         - | 2338 | `	 * (rule 2): every closure created by the engine is an instance of it, and a NULL` |
|         - | 2339 | `	 * cache is a segfault on the first one. Its no-serialize rule rides the spec` |
|         - | 2340 | `	 * rather than being stamped on afterwards. */` |
|      5259 | 2341 | `	PH7_VmInstallClosureNative(&(*pVm));` |
|      5259 | 2342 | `	pVm->pClosureClass = PH7_VmExtractClass(pVm,"Closure",7,0,0);` |
|      5259 | 2343 | `	pVm->pClosureThis = 0; /* transient bound-$this slot, consumed per call */` |
|      5259 | 2344 | `	pVm->pClosureScope = 0; /* transient bound-scope slot, consumed per call */` |
|         - | 2345 | `	/* Cache the stdClass pointer ((object) cast target + dynamic-property owner) */` |
|      5259 | 2346 | `	pVm->pStdClass = PH7_VmExtractClass(pVm,"stdClass",sizeof("stdClass")-1,0,0);` |
|         - | 2347 | `	/* ... and unserialize()'s incomplete-object carrier, declared beside it. */` |
|      5259 | 2348 | `	pVm->pIncClass = PH7_VmExtractClass(pVm,"__PHP_Incomplete_Class",` |
|         - | 2349 | `		sizeof("__PHP_Incomplete_Class")-1,0,0);` |
|         - | 2350 | `	/* Declare Generator, THEN cache the pointer -- it no longer exists until the` |
|         - | 2351 | ``	 * native install creates it, and a NULL cache here segfaults the first `yield`.`` |
|         - | 2352 | `` 	 * Iterator must already be compiled: the install attaches `implements Iterator` `` |
|         - | 2353 | `	 * after the methods, which is why the class cannot live in the chunk. */` |
|      5259 | 2354 | `	PH7_VmInstallGeneratorNative(&(*pVm));` |
|      5259 | 2355 | `	pVm->pGeneratorClass = PH7_VmExtractClass(pVm,"Generator",9,0,0);` |
|         - | 2356 | `	/* InternalIterator: what a native IteratorAggregate answers where the PHP it` |
|         - | 2357 | `	 * replaced returned a Generator. Declared before the subsystems that hand one` |
|         - | 2358 | `	 * out (DatePeriod, WeakMap); Iterator comes from the builtin lib chunk above. */` |
|      5259 | 2359 | `	PH7_VmInstallNativeIterator(&(*pVm));` |
|         - | 2360 | `	/* Install the Reflection library (embedded classes + __reflect_* thunks).` |
|         - | 2361 | `	 * Still inside the bCompilingBuiltin window so its classes are flagged` |
|         - | 2362 | `	 * internal; the Traversable pointer above must already be cached. */` |
|      5259 | 2363 | `	PH7_VmInstallReflection(&(*pVm));` |
|      5259 | 2364 | `	PH7_VmInstallDateTime(&(*pVm));` |
|      5259 | 2365 | `	PH7_VmInstallSpl(&(*pVm));` |
|         - | 2366 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      5259 | 2367 | `	PH7_VmInstallHashContext(&(*pVm));` |
|         - | 2368 | `#endif` |
|         - | 2369 | `#ifndef PH7_DISABLE_DISK_IO` |
|         - | 2370 | `	/* php_user_filter and StreamBucket: the classes stream_filter_register()` |
|         - | 2371 | `	 * builds its filters out of. */` |
|      5259 | 2372 | `	PH7_VmInstallStreamFilter(&(*pVm));` |
|         - | 2373 | `#endif` |
|      5259 | 2374 | `	PH7_VmInstallTokenizer(&(*pVm));` |
|      5259 | 2375 | `	PH7_VmInstallSession(&(*pVm));` |
|      5259 | 2376 | `	PH7_VmInstallIni(&(*pVm));` |
|         - | 2377 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2378 | `	/* libxml2-backed surfaces: shared plumbing first, then the ext/xml push` |
|         - | 2379 | `	 * parser and the DOM and XMLWriter class libraries that build on it. */` |
|      5259 | 2380 | `	PH7_VmInstallLibxml(&(*pVm));` |
|      5259 | 2381 | `	PH7_VmInstallXml(&(*pVm));` |
|      5259 | 2382 | `	PH7_VmInstallDom(&(*pVm));` |
|      5259 | 2383 | `	PH7_VmInstallXmlWriter(&(*pVm));` |
|         - | 2384 | `#endif` |
|         - | 2385 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 2386 | ``	/* ext/pdo's class library first: `Pdo\Sqlite` extends PDO, so the driver's`` |
|         - | 2387 | `	 * installer needs the parent already mounted. */` |
|      5259 | 2388 | `	PH7_VmInstallPdo(&(*pVm));` |
|      5259 | 2389 | `	PH7_VmInstallPdoSqlite(&(*pVm));` |
|         - | 2390 | `#endif` |
|         - | 2391 | `#ifdef PH7_ENABLE_CURL` |
|         - | 2392 | `	/* ext/curl: the libcurl binding. */` |
|      5259 | 2393 | `	PH7_VmInstallCurl(&(*pVm));` |
|         - | 2394 | `#endif` |
|      5259 | 2395 | `	pVm->bCompilingBuiltin = 0;` |
|         - | 2396 | `	/* Reset the code generator */` |
|      5259 | 2397 | `	PH7_ResetCodeGenerator(&(*pVm),pEngine->xConf.xErr,pEngine->xConf.pErrData);` |
|      5259 | 2398 | `	return SXRET_OK;` |
|       ! 0 | 2399 | `Err:` |
|       ! 0 | 2400 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|       ! 0 | 2401 | `	return rc;` |
|      2632 | 2402 | `}` |
|         - | 2403 | `/*` |
|         - | 2404 | ` * Default VM output consumer callback.That is,all VM output is redirected to this` |
|         - | 2405 | ` * routine which store the output in an internal blob.` |
|         - | 2406 | ` * The output can be extracted later after program execution [ph7_vm_exec()] via` |
|         - | 2407 | ` * the [ph7_vm_config()] interface with a configuration verb set to` |
|         - | 2408 | ` * PH7_VM_CONFIG_EXTRACT_OUTPUT.` |
|         - | 2409 | ` * Refer to the official docurmentation for additional information.` |
|         - | 2410 | ` * Note that for performance reason it's preferable to install a VM output` |
|         - | 2411 | ` * consumer callback via (PH7_VM_CONFIG_OUTPUT) rather than waiting for the VM` |
|         - | 2412 | ` * to finish executing and extracting the output.` |
|         - | 2413 | ` */` |
|       348 | 2414 | `PH7_PRIVATE sxi32 PH7_VmBlobConsumer(` |
|         - | 2415 | `	const void *pOut,   /* VM Generated output*/` |
|         - | 2416 | `	unsigned int nLen,  /* Generated output length */` |
|         - | 2417 | `	void *pUserData     /* User private data */` |
|         - | 2418 | `	)` |
|         4 | 2419 | `{` |
|         - | 2420 | `	 sxi32 rc;` |
|         - | 2421 | `	 /* Store the output in an internal BLOB */` |
|       352 | 2422 | `	 rc = SyBlobAppend((SyBlob *)pUserData,pOut,nLen);` |
|       352 | 2423 | `	 return rc;` |
|         4 | 2424 | `}` |
|         - | 2425 | `/*` |
|         - | 2426 | ` * Track output length and mark headers as sent when output reaches` |
|         - | 2427 | ` * a real external consumer (not the internal blob or OB buffer).` |
|         - | 2428 | ` */` |
|    146958 | 2429 | `PH7_PRIVATE void VmTrackOutput(ph7_vm *pVm, sxu32 nLen)` |
|         5 | 2430 | `{` |
|    146963 | 2431 | `	ProcConsumer xCons = pVm->sVmConsumer.xConsumer;` |
|    146963 | 2432 | `	if( xCons != VmObConsumer ){` |
|     37059 | 2433 | `		pVm->nOutputLen += nLen;` |
|     37059 | 2434 | `		if( !pVm->bHeadersSent && xCons != PH7_VmBlobConsumer ){` |
|      1591 | 2435 | `			pVm->bHeadersSent = 1;` |
|       793 | 2436 | `		}` |
|     18527 | 2437 | `	}` |
|    146963 | 2438 | `}` |
|         - | 2439 | `/*` |
|         - | 2440 | ` * Static operand-stack depth analysis (BYTECODE.md stage 7).` |
|         - | 2441 | ` *` |
|         - | 2442 | ` * The safe upper bound on a body's operand-stack depth is its instruction count` |
|         - | 2443 | ` * (no instruction pushes more than one net slot), and that is what` |
|         - | 2444 | ` * VmNewOperandStack allocates by default. For DEEP recursion that over-allocates` |
|         - | 2445 | ` * badly — one operand stack per live frame, each sized to the whole body — so` |
|         - | 2446 | ` * this pass computes a TIGHT bound (typically single digits) for the common` |
|         - | 2447 | ` * shape of a recursive function, letting the OP_CALL path allocate small stacks.` |
|         - | 2448 | ` *` |
|         - | 2449 | ` * Undersizing an operand stack is a heap overflow, so the analysis is` |
|         - | 2450 | ` * conservative BY CONSTRUCTION:` |
|         - | 2451 | ` *   - Every modeled opcode uses pushmax = 1 (the engine invariant) and a popmin` |
|         - | 2452 | ` *     that never exceeds its real pop on any path (verified per handler). Over-` |
|         - | 2453 | ` *     estimating height is safe; the only unsafe direction — over-crediting a` |
|         - | 2454 | ` *     pop — makes height go negative, which triggers fallback.` |
|         - | 2455 | ` *   - A body is sized by this analysis only if EVERY instruction is in the` |
|         - | 2456 | ` *     verified modeled set (VmInstrStackEffect). Any other opcode (try/catch,` |
|         - | 2457 | ` *     yield, foreach, switch/match, spread, string/array builders, …) returns` |
|         - | 2458 | ` *     VM_STACK_UNMODELED for the whole body -> caller keeps the instruction-count` |
|         - | 2459 | ` *     bound. There is no partial/unsafe middle.` |
|         - | 2460 | ` *   - Control flow follows real edges (JMP/JZ/JNZ + the fused comparison-branch` |
|         - | 2461 | ` *     forms). An out-of-range jump, a negative height, or a height exceeding the` |
|         - | 2462 | ` *     instruction-count bound -> fallback.` |
|         - | 2463 | ` *   - VM_STACK_GUARD slack is still added by the operand-stack allocator on top` |
|         - | 2464 | ` *     of the returned depth, and the full corpus runs under ASan (which catches` |
|         - | 2465 | ` *     any undersize as a heap-buffer-overflow) as the standing validation.` |
|         - | 2466 | ` *` |
|         - | 2467 | `` * The exception-resume `pc =` reassignments inside some modeled handlers`` |
|         - | 2468 | ` * (STORE/CALL/DONE/comparisons) only fire when this activation OWNS a catch` |
|         - | 2469 | ` * frame — impossible in a modeled body, since OP_LOAD_EXCEPTION is unmodeled and` |
|         - | 2470 | ` * forces fallback — so they are dead in analyzed bodies and need no edge.` |
|         - | 2471 | ` *` |
|         - | 2472 | ` * Drift safety (for whoever adds an opcode or changes a handler's stack effect):` |
|         - | 2473 | ` * VmInstrStackEffect is a hand-maintained model that must stay in sync with the` |
|         - | 2474 | ` * real handlers. Two things keep a drift from becoming a silent undersize: a NEW` |
|         - | 2475 | `` * opcode is unmodeled by default (its `default:` return forces the safe`` |
|         - | 2476 | ` * instruction-count bound), so only *changing a modeled opcode's real pop count*` |
|         - | 2477 | ` * to exceed its popmin can undersize — and that is caught deterministically by` |
|         - | 2478 | ` * the standing ASan-over-corpus run (an undersize is a heap-buffer-overflow on` |
|         - | 2479 | ` * the operand stack). When touching a modeled handler's push/pop, re-check its` |
|         - | 2480 | ` * entry here.` |
|         - | 2481 | ` */` |
|         - | 2482 | `/*` |
|         - | 2483 | ` * Fill the stack effect of one modeled instruction: *pPush is its transient` |
|         - | 2484 | ` * push (0/1, added to the height for the peak), and the *pN successor edges` |
|         - | 2485 | ` * (absolute instruction index in aSucc[k], height delta in aDelta[k]). Returns` |
|         - | 2486 | ` * 1 if modeled, 0 if the opcode is outside the verified set (whole-body` |
|         - | 2487 | ` * fallback). pc is this instruction's own index (fall-through = pc+1).` |
|         - | 2488 | ` */` |
|     52420 | 2489 | `static int VmInstrStackEffect(VmInstr *pI, sxu32 pc, int *pPush, int *pN, sxu32 aSucc[2], sxi32 aDelta[2])` |
|         5 | 2490 | `{` |
|     52425 | 2491 | `	int push = 0, n = 0;` |
|         - | 2492 | `	sxi32 d;` |
|     52425 | 2493 | `	switch( pI->iOp ){` |
|         - | 2494 | `	/* Pushers (+1). LOAD pushes only with an inline name operand (p3 != 0); with` |
|         - | 2495 | `	 * the name taken from the stack (p3 == 0) it reuses that slot -> net 0. */` |
|      9862 | 2496 | `	case PH7_OP_LOADC:` |
|         - | 2497 | `	case PH7_OP_DUP:` |
|     19727 | 2498 | `		push = 1; aSucc[0] = pc + 1; aDelta[0] = 1; n = 1; break;` |
|      3657 | 2499 | `	case PH7_OP_LOAD:` |
|      7319 | 2500 | `		if( pI->p3 ){ push = 1; d = 1; }else{ push = 0; d = 0; }` |
|      7319 | 2501 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         9 | 2502 | `	case PH7_OP_LOAD_REF:` |
|        19 | 2503 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2504 | `	/* Binary ops: 2-in/1-out, computed in place then one pop -> net -1. */` |
|       578 | 2505 | `	case PH7_OP_ADD: case PH7_OP_SUB: case PH7_OP_MUL: case PH7_OP_DIV:` |
|         - | 2506 | `	case PH7_OP_MOD: case PH7_OP_POW: case PH7_OP_BAND: case PH7_OP_BOR:` |
|         - | 2507 | `	case PH7_OP_BXOR: case PH7_OP_SHL: case PH7_OP_SHR: case PH7_OP_SPACESHIP:` |
|      1161 | 2508 | `		aSucc[0] = pc + 1; aDelta[0] = -1; n = 1; break;` |
|         - | 2509 | `	/* Comparisons: value-form (iP2 == 0) pops 1 in place. Fused branch-form` |
|         - | 2510 | `	 * (iP2 != 0) pops 1 on the fall-through edge and 2 on the taken edge (-> iP2). */` |
|       204 | 2511 | `	case PH7_OP_LT: case PH7_OP_LE: case PH7_OP_GT: case PH7_OP_GE:` |
|         - | 2512 | `	case PH7_OP_EQ: case PH7_OP_NEQ: case PH7_OP_TEQ: case PH7_OP_TNE:` |
|       413 | 2513 | `		if( pI->iP2 == 0 ){` |
|       413 | 2514 | `			aSucc[0] = pc + 1; aDelta[0] = -1; n = 1;` |
|       209 | 2515 | `		}else{` |
|       ! 0 | 2516 | `			aSucc[0] = pc + 1; aDelta[0] = -1;` |
|       ! 0 | 2517 | `			aSucc[1] = pI->iP2; aDelta[1] = -2; n = 2;` |
|         - | 2518 | `		}` |
|       413 | 2519 | `		break;` |
|         - | 2520 | `	/* In-place unary / casts: net 0. (CVT_NULL aborts, CVT_ARRAY/CVT_OBJ are not` |
|         - | 2521 | `	 * verified here -> all three fall through to the unmodeled default.) */` |
|       147 | 2522 | `	case PH7_OP_LNOT: case PH7_OP_UMINUS: case PH7_OP_UPLUS: case PH7_OP_BITNOT:` |
|         - | 2523 | `	case PH7_OP_CVT_INT: case PH7_OP_CVT_REAL: case PH7_OP_CVT_STR:` |
|         - | 2524 | `	case PH7_OP_CVT_BOOL: case PH7_OP_CVT_NUMC:` |
|         - | 2525 | `	case PH7_OP_NOOP:` |
|       299 | 2526 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2527 | `	/* Stores: member (iP2) and name-from-stack (p3 == 0) pop 1; inline-name` |
|         - | 2528 | `	 * (p3 != 0) pops 0. The rvalue is left as the expression result either way. */` |
|       637 | 2529 | `	case PH7_OP_STORE:` |
|      1279 | 2530 | `		d = ( pI->iP2 \|\| pI->p3 == 0 ) ? -1 : 0;` |
|      1279 | 2531 | `		aSucc[0] = pc + 1; aDelta[0] = d; n = 1; break;` |
|         - | 2532 | `	/* Explicit multi-slot pops (operand-encoded count). */` |
|      1283 | 2533 | `	case PH7_OP_POP:` |
|         - | 2534 | `	case PH7_OP_CONSUME:` |
|      2571 | 2535 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2536 | `	/* Callee rotation: turns the call's operand region over in place — no slot is` |
|         - | 2537 | `	 * pushed and none is popped, on any path. */` |
|       ! 0 | 2538 | `	case PH7_OP_ROT_CALLEE:` |
|       ! 0 | 2539 | `		aSucc[0] = pc + 1; aDelta[0] = 0; n = 1; break;` |
|         - | 2540 | `	/* Call: net -iP1 (args + callable consumed, result reuses the callable slot).` |
|         - | 2541 | ``	 * Spread is excluded: OP_SPREAD is unmodeled, so a call with `...$x` — whose`` |
|         - | 2542 | `	 * true pop count is a runtime value — never reaches here. */` |
|       347 | 2543 | `	case PH7_OP_CALL:` |
|       699 | 2544 | `		aSucc[0] = pc + 1; aDelta[0] = -(sxi32)pI->iP1; n = 1; break;` |
|         - | 2545 | `	/* Jumps. */` |
|        91 | 2546 | `	case PH7_OP_JMP:` |
|       186 | 2547 | `		aSucc[0] = pI->iP2; aDelta[0] = 0; n = 1; break;` |
|       146 | 2548 | `	case PH7_OP_JZ: case PH7_OP_JNZ:` |
|       297 | 2549 | `		d = ( pI->iP1 == 0 ) ? -1 : 0; /* pops the condition on BOTH edges unless P1 says peek */` |
|       297 | 2550 | `		aSucc[0] = pc + 1; aDelta[0] = d; aSucc[1] = pI->iP2; aDelta[1] = d; n = 2; break;` |
|         - | 2551 | `	/* Terminal: ends the path (its optional result pop does not propagate). */` |
|      3190 | 2552 | `	case PH7_OP_DONE:` |
|      6385 | 2553 | `		n = 0; break;` |
|      6061 | 2554 | `	default:` |
|     12125 | 2555 | `		return 0; /* unmodeled opcode -> whole-body fallback */` |
|         - | 2556 | `	}` |
|     40305 | 2557 | `	*pPush = push; *pN = n;` |
|     40305 | 2558 | `	return 1;` |
|     26213 | 2559 | `}` |
|         - | 2560 | `/*` |
|         - | 2561 | ` * Compute a tight upper bound on the operand-stack depth of a compiled body, or` |
|         - | 2562 | ` * VM_STACK_UNMODELED to request the safe instruction-count bound. See the block` |
|         - | 2563 | ` * comment above. Never underestimates a modelable body's true peak depth.` |
|         - | 2564 | ` */` |
|     14338 | 2565 | `PH7_PRIVATE sxu32 VmComputeMaxStack(ph7_vm *pVm, VmInstr *aInstr, sxu32 nInstr)` |
|         5 | 2566 | `{` |
|         - | 2567 | `	void *pScratch;` |
|         - | 2568 | `	sxi32 *aH; sxu32 *aQ; unsigned char *aIn;` |
|         - | 2569 | `	sxu32 nQ, i, nIter, nCap;` |
|         - | 2570 | `	sxi32 iMax;` |
|         - | 2571 | `	int push, n, k;` |
|         - | 2572 | `	sxu32 succ[2]; sxi32 delta[2];` |
|     14343 | 2573 | `	if( nInstr == 0 \|\| nInstr > 8192 ){` |
|         - | 2574 | `		/* Empty, or large enough that the analysis cost/benefit isn't worth it. */` |
|       ! 0 | 2575 | `		return VM_STACK_UNMODELED;` |
|         - | 2576 | `	}` |
|         - | 2577 | `	/* Pre-scan: any unmodeled opcode -> bail before allocating scratch. */` |
|     47503 | 2578 | `	for( i = 0; i < nInstr; i++ ){` |
|     45285 | 2579 | `		if( !VmInstrStackEffect(&aInstr[i], i, &push, &n, succ, delta) ){` |
|     12125 | 2580 | `			return VM_STACK_UNMODELED;` |
|         - | 2581 | `		}` |
|     16584 | 2582 | `	}` |
|         - | 2583 | `	/* aH (entry height per pc), aQ (worklist), aIn (queued flag) share one lifetime` |
|         - | 2584 | `	 * and count -> one allocation, carved into three regions with the 4-byte arrays` |
|         - | 2585 | `	 * first (the byte array last needs no alignment). */` |
|      2223 | 2586 | `	pScratch = SyMemBackendAlloc(&pVm->sAllocator, nInstr * (sizeof(sxi32) + sizeof(sxu32) + 1));` |
|      2223 | 2587 | `	if( pScratch == 0 ){` |
|       ! 0 | 2588 | `		return VM_STACK_UNMODELED;` |
|         - | 2589 | `	}` |
|      2223 | 2590 | `	aH  = (sxi32 *)pScratch;` |
|      2223 | 2591 | `	aQ  = (sxu32 *)(aH + nInstr);` |
|      2223 | 2592 | `	aIn = (unsigned char *)(aQ + nInstr);` |
|     11309 | 2593 | `	for( i = 0; i < nInstr; i++ ){ aH[i] = -1; aIn[i] = 0; }` |
|      2223 | 2594 | `	aH[0] = 0; aQ[0] = 0; aIn[0] = 1; nQ = 1; iMax = 0;` |
|      2223 | 2595 | `	nIter = 0; nCap = nInstr * 16 + 1024; /* convergence backstop (fallback if hit) */` |
|      9363 | 2596 | `	while( nQ > 0 ){` |
|      7145 | 2597 | `		sxu32 pc = aQ[--nQ];` |
|         - | 2598 | `		sxi32 h;` |
|      7145 | 2599 | `		aIn[pc] = 0;` |
|      7145 | 2600 | `		h = aH[pc];` |
|      7145 | 2601 | `		if( ++nIter > nCap ){ iMax = -1; break; }` |
|      7145 | 2602 | `		(void)VmInstrStackEffect(&aInstr[pc], pc, &push, &n, succ, delta);` |
|      7145 | 2603 | `		if( h + push > iMax ){ iMax = h + push; }` |
|      7145 | 2604 | `		if( iMax > (sxi32)nInstr ){ iMax = -1; break; } /* over the safe bound: not worth it */` |
|     12081 | 2605 | `		for( k = 0; k < n; k++ ){` |
|      4941 | 2606 | `			sxi32 hn = h + delta[k];` |
|      4941 | 2607 | `			sxu32 t = succ[k];` |
|      4941 | 2608 | `			if( t >= nInstr \|\| hn < 0 ){ iMax = -1; break; } /* bad jump / imbalance */` |
|      4941 | 2609 | `			if( hn > aH[t] ){` |
|      4927 | 2610 | `				aH[t] = hn;` |
|      4927 | 2611 | `				if( !aIn[t] ){ aIn[t] = 1; aQ[nQ++] = t; }` |
|      2461 | 2612 | `			}` |
|      2473 | 2613 | `		}` |
|      7145 | 2614 | `		if( iMax < 0 ){ break; }` |
|         5 | 2615 | `	}` |
|      2223 | 2616 | `	SyMemBackendFree(&pVm->sAllocator, pScratch);` |
|      2223 | 2617 | `	return ( iMax < 0 ) ? VM_STACK_UNMODELED : (sxu32)iMax;` |
|      7173 | 2618 | `}` |
|         - | 2619 | `/*` |
|         - | 2620 | ` * Allocate a new operand stack so that we can start executing` |
|         - | 2621 | ` * our compiled PHP program.` |
|         - | 2622 | ` * Return a pointer to the operand stack (array of ph7_values)` |
|         - | 2623 | ` * on success. NULL (Fatal error) on failure.` |
|         - | 2624 | ` *` |
|         - | 2625 | ` * This is the RAW allocator (always mallocs + inits nInstr + VM_STACK_GUARD` |
|         - | 2626 | ` * slots). The OP_CALL hot path goes through VmOperandStackAlloc, which recycles a` |
|         - | 2627 | ` * parked buffer when it can and falls back to this; the other entries (top-level,` |
|         - | 2628 | ` * eval, coroutine, callbacks) call this directly.` |
|         - | 2629 | ` */` |
|   3475921 | 2630 | `PH7_PRIVATE ph7_value * VmNewOperandStack(` |
|         - | 2631 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 2632 | `	sxu32 nInstr /* Total numer of generated byte-code instructions */` |
|         - | 2633 | `	)` |
|         5 | 2634 | `{` |
|         - | 2635 | `	ph7_value *pStack;` |
|         - | 2636 | `  /* No instruction ever pushes more than a single element onto the` |
|         - | 2637 | `  ** stack and the stack never grows on successive executions of the` |
|         - | 2638 | `  ** same loop. So the total number of instructions is an upper bound` |
|         - | 2639 | `  ** on the maximum stack depth required.` |
|         - | 2640 | `  **` |
|         - | 2641 | `  ** Allocation all the stack space we will ever need.` |
|         - | 2642 | `  */` |
|   3475926 | 2643 | `	nInstr += VM_STACK_GUARD;` |
|   3475926 | 2644 | `	pStack = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,nInstr * sizeof(ph7_value));` |
|   3475926 | 2645 | `	if( pStack == 0 ){` |
|       ! 0 | 2646 | `		return 0;` |
|         - | 2647 | `	}` |
|         - | 2648 | `	/* Initialize the operand stack */` |
|  73596486 | 2649 | `	while( nInstr > 0 ){` |
|  70120565 | 2650 | `		PH7_MemObjInit(&(*pVm),&pStack[nInstr - 1]);` |
|  70120565 | 2651 | `		--nInstr;` |
|         5 | 2652 | `	}` |
|         - | 2653 | `	/* Ready for bytecode execution */` |
|   3475926 | 2654 | `	return pStack;` |
|   1738006 | 2655 | `}` |
|         - | 2656 | `/*` |
|         - | 2657 | ` * Operand-stack recycling (BYTECODE.md stage 7).` |
|         - | 2658 | ` *` |
|         - | 2659 | ` * After tight sizing, a PHP call still allocates + inits an operand stack on the` |
|         - | 2660 | ` * way in and frees it on the way out. For recursion and hot call loops the freed` |
|         - | 2661 | ` * stack is exactly the size the next call needs, so instead of freeing it at the` |
|         - | 2662 | ` * normal OP_CALL return (VmCallFinish) we park it on a small per-VM freelist and` |
|         - | 2663 | ` * hand it back to the next same-size call — skipping the buffer allocation and` |
|         - | 2664 | ` * the per-slot PH7_MemObjInit.` |
|         - | 2665 | ` *` |
|         - | 2666 | ` * The freelist holds plain allocator blocks (no header): a parked buffer is just` |
|         - | 2667 | ` * a ph7_value array whose slots were all released at recycle time, so it is` |
|         - | 2668 | ` * clean to reuse, cannot leak a stale value, and can still be raw-freed by the` |
|         - | 2669 | ` * cold/suspend/abort paths that never route through here. Head-only exact-size` |
|         - | 2670 | ` * match keeps it O(1) and memory-tight (a mismatched size allocates fresh rather` |
|         - | 2671 | ` * than over-allocating — deep recursion, whose freelist is empty during descent,` |
|         - | 2672 | ` * is unaffected). Length is capped so the pool can't grow without bound.` |
|         - | 2673 | ` *` |
|         - | 2674 | ` * The head-only match is tuned for the design target (recursion / a hot loop` |
|         - | 2675 | ` * calling one function — one size, ~total reuse). An alternating-size pattern` |
|         - | 2676 | ` * (a() then b() with different depths, repeatedly) never matches the head, so it` |
|         - | 2677 | ` * degrades to a fresh allocation every call — same as no pool, never worse; the` |
|         - | 2678 | ` * recursion case is the one worth the O(1) simplicity.` |
|         - | 2679 | ` */` |
|         - | 2680 | `typedef struct VmIdleStack VmIdleStack;` |
|         - | 2681 | `struct VmIdleStack {` |
|         - | 2682 | `	ph7_value *pStack;   /* Parked buffer (nCap slots, all released) */` |
|         - | 2683 | `	sxu32 nCap;          /* Its allocated slot count (VmNewOperandStack size) */` |
|         - | 2684 | `	VmIdleStack *pNext;  /* LIFO link */` |
|         - | 2685 | `};` |
|         - | 2686 | `#define VM_STACK_POOL_MAX 64      /* max buffers parked at once */` |
|         - | 2687 | `#define VM_STACK_POOL_MAXSLOTS 512 /* only pool buffers this small — bounds pool memory` |
|         - | 2688 | `                                    * (a large fallback-sized stack recursing would` |
|         - | 2689 | `                                    * otherwise park up to VM_STACK_POOL_MAX huge buffers;` |
|         - | 2690 | `                                    * the tight-sized hot case is far below this) */` |
|         - | 2691 | `/*` |
|         - | 2692 | ` * Allocate an operand stack of nSlots (+ VM_STACK_GUARD) usable slots, reusing a` |
|         - | 2693 | ` * parked same-size buffer when one is available (its slots are already clean).` |
|         - | 2694 | ` */` |
|    766226 | 2695 | `PH7_PRIVATE ph7_value * VmOperandStackAlloc(ph7_vm *pVm, sxu32 nSlots)` |
|         5 | 2696 | `{` |
|    766231 | 2697 | `	VmIdleStack *pIdle = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    766231 | 2698 | `	sxu32 nCap = nSlots + VM_STACK_GUARD;` |
|    766231 | 2699 | `	if( pIdle && pIdle->nCap == nCap ){` |
|    690247 | 2700 | `		ph7_value *pStack = pIdle->pStack;` |
|    690247 | 2701 | `		pVm->pIdleOperandStacks = pIdle->pNext;` |
|    690247 | 2702 | `		pVm->nIdleOperandStacks--;` |
|         - | 2703 | `		/* Keep the wrapper node on the spare-node freelist for the next recycle` |
|         - | 2704 | `		 * instead of returning it to the pool (mirrors pIdleCallFrames). */` |
|    690247 | 2705 | `		pIdle->pNext = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    690247 | 2706 | `		pVm->pIdleStackNodes = pIdle;` |
|    690247 | 2707 | `		return pStack; /* slots already released -> reusable without re-init */` |
|         - | 2708 | `	}` |
|     75989 | 2709 | `	return VmNewOperandStack(&(*pVm),nSlots);` |
|    383335 | 2710 | `}` |
|         - | 2711 | `/*` |
|         - | 2712 | ` * Return an operand stack to the freelist (or free it if the pool is full).` |
|         - | 2713 | ` * nCap is its full allocated slot count (== the VmNewOperandStack size). Every` |
|         - | 2714 | ` * slot is released so the parked buffer is clean for reuse and never retains a` |
|         - | 2715 | ` * live value.` |
|         - | 2716 | ` */` |
|    766026 | 2717 | `PH7_PRIVATE void VmOperandStackRecycle(ph7_vm *pVm, ph7_value *pStack, sxu32 nCap)` |
|         5 | 2718 | `{` |
|         - | 2719 | `	VmIdleStack *pIdle;` |
|         - | 2720 | `	sxu32 i;` |
|    766031 | 2721 | `	if( pStack == 0 ){` |
|       ! 0 | 2722 | `		return;` |
|         - | 2723 | `	}` |
|    766031 | 2724 | `	if( pVm->nIdleOperandStacks >= VM_STACK_POOL_MAX \|\| nCap > VM_STACK_POOL_MAXSLOTS ){` |
|     70936 | 2725 | `		SyMemBackendFree(&pVm->sAllocator,pStack);` |
|     70936 | 2726 | `		return;` |
|         - | 2727 | `	}` |
|         - | 2728 | `	/* Take a spare wrapper node (reused across cycles, mirroring pIdleCallFrames);` |
|         - | 2729 | `	 * pool-allocate only when the spare list is empty. */` |
|    695100 | 2730 | `	pIdle = (VmIdleStack *)pVm->pIdleStackNodes;` |
|    695100 | 2731 | `	if( pIdle ){` |
|    690247 | 2732 | `		pVm->pIdleStackNodes = pIdle->pNext;` |
|    345293 | 2733 | `	}else{` |
|      4858 | 2734 | `		pIdle = (VmIdleStack *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmIdleStack));` |
|      4858 | 2735 | `		if( pIdle == 0 ){` |
|       ! 0 | 2736 | `			SyMemBackendFree(&pVm->sAllocator,pStack);` |
|       ! 0 | 2737 | `			return;` |
|         - | 2738 | `		}` |
|         - | 2739 | `	}` |
|  45486613 | 2740 | `	for( i = 0; i < nCap; i++ ){` |
|  44791518 | 2741 | `		PH7_MemObjRelease(&pStack[i]);` |
|         - | 2742 | `		/* Reset the global-slot index to the "temporary / not a variable" marker.` |
|         - | 2743 | `		 * A released slot is already reusable (the dispatch reuses released slots` |
|         - | 2744 | `		 * mid-call, and every push sets nIdx before the slot is read), but marking` |
|         - | 2745 | `		 * it here means a stale index can never masquerade as a live variable slot` |
|         - | 2746 | `		 * across invocations — cheap defense in depth. */` |
|  44791518 | 2747 | `		pStack[i].nIdx = SXU32_HIGH;` |
|  22465390 | 2748 | `	}` |
|    695100 | 2749 | `	pIdle->pStack = pStack;` |
|    695100 | 2750 | `	pIdle->nCap = nCap;` |
|    695100 | 2751 | `	pIdle->pNext = (VmIdleStack *)pVm->pIdleOperandStacks;` |
|    695100 | 2752 | `	pVm->pIdleOperandStacks = pIdle;` |
|    695100 | 2753 | `	pVm->nIdleOperandStacks++;` |
|    383235 | 2754 | `}` |
|         - | 2755 | `/* Forward declaration */` |
|         - | 2756 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm);` |
|         - | 2757 | `/*` |
|         - | 2758 | ` * Prepare the Virtual Machine for byte-code execution.` |
|         - | 2759 | ` * This routine gets called by the PH7 engine after` |
|         - | 2760 | ` * successful compilation of the target PHP program.` |
|         - | 2761 | ` */` |
|      4660 | 2762 | `PH7_PRIVATE sxi32 PH7_VmMakeReady(` |
|         - | 2763 | `	ph7_vm *pVm /* Target VM */` |
|         - | 2764 | `	)` |
|         5 | 2765 | `{` |
|         - | 2766 | `	SyHashEntry *pEntry;` |
|         - | 2767 | `	sxi32 rc;` |
|      4665 | 2768 | `	if( pVm->nMagic != PH7_VM_INIT ){` |
|         - | 2769 | `		/* Initialize your VM first */` |
|       ! 0 | 2770 | `		return SXERR_CORRUPT;` |
|         - | 2771 | `	}` |
|         - | 2772 | `	/* Mark the VM ready for byte-code execution */` |
|      4665 | 2773 | `	pVm->nMagic = PH7_VM_RUN;` |
|         - | 2774 | `	/* Release the code generator now we have compiled our program, but keep its` |
|         - | 2775 | `	 * error consumer wired to the engine's: class mounting below (e.g. typed` |
|         - | 2776 | `	 * class-constant enforcement) still reports definition-time fatals through` |
|         - | 2777 | `	 * it, and the host VM output consumer is not installed until afterwards. */` |
|      4665 | 2778 | `	PH7_ResetCodeGenerator(pVm,pVm->pEngine->xConf.xErr,pVm->pEngine->xConf.pErrData);` |
|         - | 2779 | `	/* Emit the DONE instruction */` |
|      4665 | 2780 | `	rc = PH7_VmEmitInstr(&(*pVm),PH7_OP_DONE,0,0,0,0);` |
|      4665 | 2781 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 2782 | `		return SXERR_MEM;` |
|         - | 2783 | `	}` |
|         - | 2784 | `	/* Script return value */` |
|      4665 | 2785 | `	PH7_MemObjInit(&(*pVm),&pVm->sExec); /* Assume a NULL return value */` |
|         - | 2786 | `	/* Allocate a new operand stack */` |
|      4665 | 2787 | `	pVm->aOps = VmNewOperandStack(&(*pVm),SySetUsed(pVm->pByteContainer));` |
|      4665 | 2788 | `	if( pVm->aOps == 0 ){` |
|       ! 0 | 2789 | `		return SXERR_MEM;` |
|         - | 2790 | `	}` |
|         - | 2791 | `	/* Set the default VM output consumer callback and it's` |
|         - | 2792 | `	 * private data. */` |
|      4665 | 2793 | `	pVm->sVmConsumer.xConsumer = PH7_VmBlobConsumer;` |
|      4665 | 2794 | `	pVm->sVmConsumer.pUserData = &pVm->sConsumer;` |
|         - | 2795 | `	/* Allocate the reference table */` |
|      4665 | 2796 | `	pVm->nRefSize = 0x10; /* Must be a power of two for fast arithemtic */` |
|      4665 | 2797 | `	pVm->apRefObj = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * pVm->nRefSize);` |
|      4665 | 2798 | `	if( pVm->apRefObj == 0 ){` |
|         - | 2799 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2800 | `		return SXERR_MEM;` |
|         - | 2801 | `	}` |
|         - | 2802 | `	/* Zero the reference table */` |
|      4665 | 2803 | `	SyZero(pVm->apRefObj,sizeof(VmRefObj *) * pVm->nRefSize);` |
|         - | 2804 | `	/* Register special functions first [i.e: print, json_encode(), func_get_args(), die, etc.] */` |
|      4665 | 2805 | `	rc = VmRegisterSpecialFunction(&(*pVm));` |
|      4665 | 2806 | `	if( rc != SXRET_OK ){` |
|         - | 2807 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2808 | `		return rc;` |
|         - | 2809 | `	}` |
|         - | 2810 | `	/* Snapshot the runtime object-pool watermark. Everything reserved from this` |
|         - | 2811 | `	 * index up (the $GLOBALS array, the superglobals, class static/const slots and` |
|         - | 2812 | `	 * every object/variable created during execution) is per-exec state that` |
|         - | 2813 | `	 * ph7_vm_reset() releases and truncates away before rebuilding; everything` |
|         - | 2814 | `	 * below it is compile-time/init state that survives a reset. */` |
|      4665 | 2815 | `	pVm->nSuperBaseline = SySetUsed(&pVm->aMemObj);` |
|         - | 2816 | `	/* Create superglobals [i.e: $GLOBALS, $_GET, $_POST...] */` |
|      4665 | 2817 | `	rc = PH7_HashmapCreateSuper(&(*pVm));` |
|      4665 | 2818 | `	if( rc != SXRET_OK ){` |
|         - | 2819 | `		/* Don't worry about freeing memory, everything will be released shortly */` |
|       ! 0 | 2820 | `		return rc;` |
|         - | 2821 | `	}` |
|         - | 2822 | `	/* Register built-in constants [i.e: PHP_EOL, PHP_OS...] */` |
|      4665 | 2823 | `	PH7_RegisterBuiltInConstant(&(*pVm));` |
|         - | 2824 | `	/* Register the tokenizer T_* / TOKEN_PARSE constants */` |
|      4665 | 2825 | `	PH7_RegisterTokenizerConstants(&(*pVm));` |
|         - | 2826 | `	/* Register built-in functions [i.e: is_null(), array_diff(), strlen(), etc.] */` |
|      4665 | 2827 | `	PH7_RegisterBuiltInFunction(&(*pVm));` |
|         - | 2828 | `	/* Register HTTP response functions [i.e: header(), http_response_code(), etc.] */` |
|      4665 | 2829 | `	PH7_RegisterHttpResponseFunctions(&(*pVm));` |
|         - | 2830 | `#ifdef PH7_ENABLE_PCRE` |
|         - | 2831 | `	/* Register PCRE functions [i.e: preg_match(), preg_replace(), etc.] */` |
|      4665 | 2832 | `	PH7_RegisterPcreFunctions(&(*pVm));` |
|      4665 | 2833 | `	PH7_RegisterPcreConstants(&(*pVm));` |
|         - | 2834 | `#endif` |
|         - | 2835 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 2836 | `	/* Register the LIBXML_* / XML_*_NODE constants */` |
|      4665 | 2837 | `	PH7_RegisterLibxmlConstants(&(*pVm));` |
|         - | 2838 | `#endif` |
|         - | 2839 | `#ifdef PH7_ENABLE_CURL` |
|         - | 2840 | `	/* Register the CURLOPT_* / CURLINFO_* / CURLE_* family */` |
|      4665 | 2841 | `	PH7_RegisterCurlConstants(&(*pVm));` |
|         - | 2842 | `#endif` |
|         - | 2843 | `	/* Stamp PHP-8 minimum-arity metadata onto the registered builtins so the` |
|         - | 2844 | `	 * OP_CALL choke point can raise ArgumentCountError on too few arguments. */` |
|      4665 | 2845 | `	VmSetBuiltinArity(&(*pVm));` |
|         - | 2846 | `	/* Attach PHP-style parameter signatures for reflection over builtins */` |
|      4665 | 2847 | `	VmSetBuiltinSignatures(&(*pVm));` |
|         - | 2848 | `	/* Initialize and install static and constants class attributes.` |
|         - | 2849 | `	 * NOTE: the per-exec object graph created from nSuperBaseline onward (the` |
|         - | 2850 | `	 * global frame via VmEnterFrame above, the superglobals via CreateSuper, and` |
|         - | 2851 | `	 * these class static/const slots) is rebuilt on every ph7_vm_reset() — keep` |
|         - | 2852 | `	 * that function in sync when changing what is reserved here. */` |
|         - | 2853 | `	/* TWO passes, methods first: evaluating an attribute initializer can THROW,` |
|         - | 2854 | `	 * and building the Error instance for that throw calls Error::__construct —` |
|         - | 2855 | `	 * which only exists once ITS class has been method-mounted. hClass iterates` |
|         - | 2856 | `	 * in hash order, so a one-class-at-a-time loop could reach a user class's` |
|         - | 2857 | `	 * static default while the exception classes were still unmounted: the throw` |
|         - | 2858 | `	 * failed to construct its own exception, that failure threw again, and the` |
|         - | 2859 | `	 * pair recursed to the native-nesting cap. The visible result was a process` |
|         - | 2860 | `	 * that exited 255 with no diagnostic at all (mount runs before bErrReport).` |
|         - | 2861 | `	 * The passes are independent — attribute initializers reference constants and` |
|         - | 2862 | `	 * enum cases, which materialize on demand, never a method table. */` |
|      4665 | 2863 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    765607 | 2864 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    760947 | 2865 | `		rc = VmMountUserClassMethods(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    760947 | 2866 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 2867 | `			return rc;` |
|         - | 2868 | `		}` |
|         5 | 2869 | `	}` |
|      4665 | 2870 | `	SyHashResetLoopCursor(&pVm->hClass);` |
|    764949 | 2871 | `	while((pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|    760293 | 2872 | `		rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|    760293 | 2873 | `		if( rc != SXRET_OK ){` |
|         6 | 2874 | `			return rc;` |
|         - | 2875 | `		}` |
|         5 | 2876 | `	}` |
|         - | 2877 | `	/* Random number betwwen 0 and 1023 used to generate unique ID */` |
|      4661 | 2878 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 2879 | `	/* First object handle id handed out is 1 (matches PHP's first userland object #1) */` |
|      4661 | 2880 | `	pVm->nNextObjId = 1;` |
|         - | 2881 | `	/* VM is ready for bytecode execution */` |
|      4661 | 2882 | `	return SXRET_OK;` |
|      2335 | 2883 | `}` |
|         - | 2884 | `/*` |
|         - | 2885 | ` * Tear down the whole reference table. Unlinks every referenced object,` |
|         - | 2886 | ` * deleting the hash entries (frame variables) and array nodes it points at.` |
|         - | 2887 | ` * Called by ph7_vm_reset() while the frames and the object pool are still` |
|         - | 2888 | ` * intact: doing it first means a later release of a by-ref array does not leave` |
|         - | 2889 | ` * a dangling node pointer in some other object's reference record.` |
|         - | 2890 | ` */` |
|        16 | 2891 | `static void VmResetRefTable(ph7_vm *pVm)` |
|       ! 0 | 2892 | `{` |
|         - | 2893 | `	/* VmRefObjUnlink splices each node out of its apRefObj bucket and decrements` |
|         - | 2894 | `	 * nRefUsed, so draining the list leaves the bucket array empty and nRefUsed` |
|         - | 2895 | `	 * at 0 — no extra clearing needed. The bucket array and nRefSize survive. */` |
|       540 | 2896 | `	while( pVm->pRefList ){` |
|       524 | 2897 | `		VmRefObjUnlink(&(*pVm),pVm->pRefList);` |
|       ! 0 | 2898 | `	}` |
|        16 | 2899 | `}` |
|         - | 2900 | `/*` |
|         - | 2901 | ` * Release a standing per-exec ph7_value slot and re-initialise it to NULL.` |
|         - | 2902 | ` * The reset idiom for the VM's long-lived value fields (return value, the` |
|         - | 2903 | ` * error/exception handler callbacks, the assertion callback, the coalesce key).` |
|         - | 2904 | ` */` |
|        96 | 2905 | `static void VmReinitMemObj(ph7_vm *pVm,ph7_value *pObj)` |
|       ! 0 | 2906 | `{` |
|        96 | 2907 | `	PH7_MemObjRelease(pObj);` |
|        96 | 2908 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|        96 | 2909 | `}` |
|         - | 2910 | `/*` |
|         - | 2911 | ` * Empty a set_error_handler()/set_exception_handler() stack, releasing every` |
|         - | 2912 | ` * saved handler. The SySet itself keeps its buffer for the next request; the` |
|         - | 2913 | ` * whole thing dies with the VM allocator either way.` |
|         - | 2914 | ` */` |
|        32 | 2915 | `static void VmReleaseHandlerStack(SySet *pStack)` |
|       ! 0 | 2916 | `{` |
|        32 | 2917 | `	VmHandlerSlot *aSlot = (VmHandlerSlot *)SySetBasePtr(pStack);` |
|         - | 2918 | `	sxu32 n;` |
|        32 | 2919 | `	for( n = 0 ; n < SySetUsed(pStack) ; ++n ){` |
|       ! 0 | 2920 | `		PH7_MemObjRelease(&aSlot[n].sCb);` |
|       ! 0 | 2921 | `	}` |
|        32 | 2922 | `	SySetReset(pStack);` |
|        32 | 2923 | `}` |
|         - | 2924 | `/*` |
|         - | 2925 | ` * Reset a function's static-variable sentinels to SXU32_HIGH so the next call` |
|         - | 2926 | ` * re-reserves their slots and re-runs the initializers (PHP's per-request reset` |
|         - | 2927 | ` * of statics).` |
|         - | 2928 | ` */` |
|     15896 | 2929 | `static void VmResetFuncStatics(ph7_vm_func *pFunc)` |
|       ! 0 | 2930 | `{` |
|     15896 | 2931 | `	ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|         - | 2932 | `	sxu32 k;` |
|     15900 | 2933 | `	for( k = 0 ; k < SySetUsed(&pFunc->aStatic) ; ++k ){` |
|         4 | 2934 | `		aStatic[k].nIdx = SXU32_HIGH;` |
|         2 | 2935 | `	}` |
|     15896 | 2936 | `}` |
|         - | 2937 | `/*` |
|         - | 2938 | ` * Reset per-execution function-table state in a single pass over hFunction:` |
|         - | 2939 | ` *  - run-time closures (VM_FUNC_CLOSURE) are freed. Closure templates are never` |
|         - | 2940 | ` *    installed in hFunction (see compile.c) and closure names are unique, so any` |
|         - | 2941 | ` *    such entry is a standalone instance created by OP_LOAD_CLOSURE; it owns its` |
|         - | 2942 | ` *    captured environment values, its name buffer and its structure (the` |
|         - | 2943 | ` *    bytecode/args/static sets are shared with the template and must NOT be` |
|         - | 2944 | ` *    freed). Its template-shared static sentinels are reset too.` |
|         - | 2945 | ` *  - every other function (and its pNextName overloads, including class methods)` |
|         - | 2946 | ` *    has its static sentinels reset.` |
|         - | 2947 | ` * The head flag of each entry fully classifies it, so one walk handles both.` |
|         - | 2948 | ` * Deleting the just-returned entry mid-walk is safe: SyHashGetNextEntry advances` |
|         - | 2949 | ` * the cursor past it before returning and the delete never touches the cursor.` |
|         - | 2950 | ` */` |
|        16 | 2951 | `static void VmResetFunctionState(ph7_vm *pVm)` |
|       ! 0 | 2952 | `{` |
|         - | 2953 | `	SyHashEntry *pEntry;` |
|        16 | 2954 | `	SyHashResetLoopCursor(&pVm->hFunction);` |
|     15912 | 2955 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hFunction)) != 0 ){` |
|     15896 | 2956 | `		ph7_vm_func *pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|     15896 | 2957 | `		if( pFunc && (pFunc->iFlags & VM_FUNC_CLOSURE) ){` |
|         - | 2958 | `			/* Standalone run-time closure: reset its (template-shared) statics,` |
|         - | 2959 | `			 * release its captured-by-value environment, then free the entry,` |
|         - | 2960 | `			 * name buffer and structure. */` |
|         4 | 2961 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|         4 | 2962 | `			const char *zName = SyStringData(&pFunc->sName);` |
|         - | 2963 | `			sxu32 k;` |
|         4 | 2964 | `			VmResetFuncStatics(pFunc);` |
|         8 | 2965 | `			for( k = 0 ; k < SySetUsed(&pFunc->aClosureEnv) ; ++k ){` |
|         4 | 2966 | `				PH7_MemObjRelease(&aEnv[k].sValue);` |
|         2 | 2967 | `			}` |
|         4 | 2968 | `			SySetRelease(&pFunc->aClosureEnv);` |
|         - | 2969 | `			/* SyHashDeleteEntry2 frees only the entry, not the key buffer. */` |
|         4 | 2970 | `			SyHashDeleteEntry2(pEntry);` |
|         4 | 2971 | `			if( zName ){` |
|         4 | 2972 | `				SyMemBackendFree(&pVm->sAllocator,(void *)zName);` |
|         2 | 2973 | `			}` |
|         4 | 2974 | `			SyMemBackendPoolFree(&pVm->sAllocator,pFunc);` |
|         4 | 2975 | `			continue;` |
|         - | 2976 | `		}` |
|         - | 2977 | `		/* Named function: reset statics for every overload sharing this name. */` |
|     31784 | 2978 | `		while( pFunc ){` |
|     15892 | 2979 | `			VmResetFuncStatics(pFunc);` |
|     15892 | 2980 | `			pFunc = pFunc->pNextName;` |
|       ! 0 | 2981 | `		}` |
|       ! 0 | 2982 | `	}` |
|        16 | 2983 | `	pVm->closure_cnt = 0;` |
|        16 | 2984 | `}` |
|         - | 2985 | `/*` |
|         - | 2986 | ` * Free the typed-property enforcement slots left in hTypedSlot. Instance slots` |
|         - | 2987 | ` * are already gone (each object's destructor removed its own during the object` |
|         - | 2988 | ` * pool release above), so only the class *static* typed-property slots remain;` |
|         - | 2989 | ` * the class re-mount registers fresh ones.` |
|         - | 2990 | ` */` |
|        16 | 2991 | `static void VmResetTypedSlots(ph7_vm *pVm)` |
|       ! 0 | 2992 | `{` |
|         - | 2993 | `	SyHashEntry *pEntry;` |
|         - | 2994 | `	/* Common case: no class static typed properties — table already empty. */` |
|        16 | 2995 | `	if( SyHashTotalEntry(&pVm->hTypedSlot) == 0 ){` |
|        12 | 2996 | `		return;` |
|         - | 2997 | `	}` |
|         - | 2998 | `	/* Free each VmClassAttr payload in a plain walk (no entry deletion), then` |
|         - | 2999 | `	 * drop and re-init the table — SyHashRelease frees the entries themselves. */` |
|         4 | 3000 | `	SyHashResetLoopCursor(&pVm->hTypedSlot);` |
|        10 | 3001 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hTypedSlot)) != 0 ){` |
|         4 | 3002 | `		if( pEntry->pUserData ){` |
|         4 | 3003 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|         2 | 3004 | `		}` |
|       ! 0 | 3005 | `	}` |
|         4 | 3006 | `	SyHashRelease(&pVm->hTypedSlot);` |
|         4 | 3007 | `	SyHashInit(&pVm->hTypedSlot,&pVm->sAllocator,0,0);` |
|         4 | 3008 | `	pVm->nNativeSetSlot = 0;` |
|         8 | 3009 | `}` |
|         - | 3010 | `/*` |
|         - | 3011 | ` * php-visible id of a resource. PHL's resource value is a bare void*, so the` |
|         - | 3012 | ` * mapping lives in a per-VM registry: the first time a pointer is asked about it` |
|         - | 3013 | ` * takes the next id, and every later lookup returns the same one. That is what` |
|         - | 3014 | ` * makes (int)$res the id php prints, and what keeps two live resources from` |
|         - | 3015 | ` * comparing equal — both used to cast to 1.` |
|         - | 3016 | ` *` |
|         - | 3017 | `` * The record's own `pRes` field is the hash key: SyHash stores the key POINTER`` |
|         - | 3018 | ` * (it does not copy), so the key has to outlive the entry. Returns 0 when the` |
|         - | 3019 | ` * registry cannot grow, which renders as php's "closed/unknown" id rather than` |
|         - | 3020 | ` * aborting a cast.` |
|         - | 3021 | ` */` |
|       386 | 3022 | `PH7_PRIVATE sxu32 PH7_VmResourceId(ph7_vm *pVm,void *pRes)` |
|         3 | 3023 | `{` |
|         - | 3024 | `	SyHashEntry *pEntry;` |
|         - | 3025 | `	phl_res_id *pRec;` |
|       389 | 3026 | `	if( pVm == 0 \|\| pRes == 0 ){` |
|       ! 0 | 3027 | `		return 0;` |
|         - | 3028 | `	}` |
|       389 | 3029 | `	pEntry = SyHashGet(&pVm->hResourceId,(const void *)&pRes,sizeof(void *));` |
|       389 | 3030 | `	if( pEntry ){` |
|       341 | 3031 | `		return ((phl_res_id *)pEntry->pUserData)->nId;` |
|         - | 3032 | `	}` |
|        51 | 3033 | `	pRec = (phl_res_id *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(phl_res_id));` |
|        51 | 3034 | `	if( pRec == 0 ){` |
|       ! 0 | 3035 | `		return 0;` |
|         - | 3036 | `	}` |
|        51 | 3037 | `	pRec->pRes = pRes;` |
|        51 | 3038 | `	pRec->nId = pVm->nResourceIdNext++;` |
|        51 | 3039 | `	if( SyHashInsert(&pVm->hResourceId,(const void *)&pRec->pRes,sizeof(void *),pRec) != SXRET_OK ){` |
|       ! 0 | 3040 | `		SyMemBackendPoolFree(&pVm->sAllocator,pRec);` |
|       ! 0 | 3041 | `		return 0;` |
|         - | 3042 | `	}` |
|        51 | 3043 | `	return pRec->nId;` |
|       196 | 3044 | `}` |
|         - | 3045 | `/*` |
|         - | 3046 | ` * Drop the resource-id registry, freeing each phl_res_id record. Ids restart at` |
|         - | 3047 | ` * 1 for the next run, matching a fresh php process.` |
|         - | 3048 | ` */` |
|        16 | 3049 | `static void VmResetResourceIds(ph7_vm *pVm)` |
|       ! 0 | 3050 | `{` |
|         - | 3051 | `	SyHashEntry *pEntry;` |
|        16 | 3052 | `	if( SyHashTotalEntry(&pVm->hResourceId) == 0 ){` |
|        16 | 3053 | `		pVm->nResourceIdNext = 1;` |
|        16 | 3054 | `		return;` |
|         - | 3055 | `	}` |
|       ! 0 | 3056 | `	SyHashResetLoopCursor(&pVm->hResourceId);` |
|       ! 0 | 3057 | `	while( (pEntry = SyHashGetNextEntry(&pVm->hResourceId)) != 0 ){` |
|       ! 0 | 3058 | `		if( pEntry->pUserData ){` |
|       ! 0 | 3059 | `			SyMemBackendPoolFree(&pVm->sAllocator,pEntry->pUserData);` |
|       ! 0 | 3060 | `		}` |
|       ! 0 | 3061 | `	}` |
|       ! 0 | 3062 | `	SyHashRelease(&pVm->hResourceId);` |
|       ! 0 | 3063 | `	SyHashInit(&pVm->hResourceId,&pVm->sAllocator,0,0);` |
|       ! 0 | 3064 | `	pVm->nResourceIdNext = 1;` |
|         8 | 3065 | `}` |
|         - | 3066 | `/*` |
|         - | 3067 | ` * Reset a Virtual Machine to its post-compile (PH7_VmMakeReady) state so the` |
|         - | 3068 | ` * same compiled program can be executed again (compile-once / execute-many).` |
|         - | 3069 | ` *` |
|         - | 3070 | ` * Definitions are preserved (treated like compile-time state): the bytecode,` |
|         - | 3071 | ` * the operand stack, the function/class/interface tables, user-defined constants` |
|         - | 3072 | ` * (a re-run define() overwrites the value in place), included-file markers` |
|         - | 3073 | ` * (so include_once/require_once stay satisfied — definitions and their` |
|         - | 3074 | ` * define()s survive without re-compiling), the literal pool, the cached` |
|         - | 3075 | ` * interface pointers, the output-consumer configuration and the IO streams.` |
|         - | 3076 | ` *` |
|         - | 3077 | ` * Per-execution state is cleared: global variables and the global frame, the` |
|         - | 3078 | ` * superglobals (re-fed afterwards via PH7_VM_CONFIG_HTTP_REQUEST), function and` |
|         - | 3079 | ` * class statics, run-time closures, the output buffers and response headers, the` |
|         - | 3080 | ` * exception/error-handler state, the reference table and every object/array` |
|         - | 3081 | ` * reserved during the run.` |
|         - | 3082 | ` *` |
|         - | 3083 | ` * Object __destruct methods are NOT run during reset (see bInReset) — releasing` |
|         - | 3084 | ` * the pool runs engine-level teardown only, matching PH7's prior behaviour where` |
|         - | 3085 | ` * global-scope destructors never fired.` |
|         - | 3086 | ` */` |
|        16 | 3087 | `PH7_PRIVATE sxi32 PH7_VmReset(ph7_vm *pVm)` |
|       ! 0 | 3088 | `{` |
|         - | 3089 | `	sxu32 nWater,n;` |
|        16 | 3090 | `	if( pVm->nMagic != PH7_VM_RUN && pVm->nMagic != PH7_VM_EXEC ){` |
|       ! 0 | 3091 | `		return SXERR_CORRUPT;` |
|         - | 3092 | `	}` |
|        16 | 3093 | `	nWater = pVm->nSuperBaseline;` |
|         - | 3094 | `	/* The $GLOBALS array is normally protected from deletion; drop the guard so` |
|         - | 3095 | `	 * its hashmap is actually released below, then rebuilt by CreateSuper. */` |
|        16 | 3096 | `	pVm->pGlobal = 0;` |
|         - | 3097 | `	/* Defensive: a bound-closure $this transient is consumed within the same OP_CALL it is set,` |
|         - | 3098 | `	 * so it is normally 0 here. But if a prior request aborted (e.g. OOM) between set and consume,` |
|         - | 3099 | `	 * a stale pointer must not survive into the next reused (-S server) request — the object pool` |
|         - | 3100 | `	 * is about to be truncated, which would dangle it. Just null it (the pool free reclaims the` |
|         - | 3101 | `	 * object); unref'ing here would race the teardown below. */` |
|        16 | 3102 | `	pVm->pClosureThis = 0;` |
|        16 | 3103 | `	pVm->pClosureScope = 0;` |
|         - | 3104 | `	/* Suppress user __destruct while we tear down the per-exec object pool: the` |
|         - | 3105 | `	 * reference table is gone and $GLOBALS is nulled, so running arbitrary PHP` |
|         - | 3106 | `	 * here is unsafe (and could realloc aMemObj mid-release). Engine memory is` |
|         - | 3107 | `	 * still reclaimed. Mirrors prior behaviour (global destructors never ran). */` |
|        16 | 3108 | `	pVm->bInReset = 1;` |
|         - | 3109 | `	/* (1) Unlink the whole reference table while frames and objects are intact. */` |
|        16 | 3110 | `	VmResetRefTable(&(*pVm));` |
|         - | 3111 | `	/* (2) Free run-time closures and reset every function/method static sentinel` |
|         - | 3112 | `	 * in a single pass over hFunction. User-defined constants are treated like` |
|         - | 3113 | `	 * function/class registrations and intentionally persist across reuse (a` |
|         - | 3114 | `	 * re-run define() overwrites the value in place). */` |
|        16 | 3115 | `	VmResetFunctionState(&(*pVm));` |
|         - | 3116 | `	/* (3) Release every object/variable reserved during the run. Re-reading the` |
|         - | 3117 | `	 * used count each iteration tolerates a destructor reserving a fresh slot. */` |
|       569 | 3118 | `	for( n = nWater ; n < SySetUsed(&pVm->aMemObj) ; ++n ){` |
|       553 | 3119 | `		ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,n);` |
|       553 | 3120 | `		if( pObj ){` |
|       553 | 3121 | `			PH7_MemObjRelease(pObj);` |
|       272 | 3122 | `		}` |
|       272 | 3123 | `	}` |
|         - | 3124 | `	/* (4) Free the class static typed-property slots (instance ones are already` |
|         - | 3125 | `	 * gone — object release in step 3 removes each instance's own slot). */` |
|        16 | 3126 | `	VmResetTypedSlots(&(*pVm));` |
|         - | 3127 | `	/* (4b) Drop the resource-id registry: the resources it named are gone with` |
|         - | 3128 | `	 * the object pool, and a re-executed program should number from 1 again. */` |
|        16 | 3129 | `	VmResetResourceIds(&(*pVm));` |
|         - | 3130 | `	/* (5) Unwind any active frames back to none. */` |
|        32 | 3131 | `	while( pVm->pFrame ){` |
|        16 | 3132 | `		VmLeaveFrame(&(*pVm));` |
|       ! 0 | 3133 | `	}` |
|         - | 3134 | `	/* Object teardown is complete; user __destruct may run normally again. */` |
|        16 | 3135 | `	pVm->bInReset = 0;` |
|         - | 3136 | `	/* (6) Truncate the object pool back to the watermark and forget stale free` |
|         - | 3137 | `	 * slots (their indices no longer exist). */` |
|        16 | 3138 | `	SySetTruncate(&pVm->aMemObj,nWater);` |
|        16 | 3139 | `	SySetReset(&pVm->aFreeObj);` |
|         - | 3140 | `	/* (7) Reset the superglobal name table and namespace scratch. */` |
|        16 | 3141 | `	SyHashRelease(&pVm->hSuper);` |
|        16 | 3142 | `	SyHashInit(&pVm->hSuper,&pVm->sAllocator,0,0);` |
|         - | 3143 | `	/* (8) Drain remaining per-exec containers. */` |
|        16 | 3144 | `	SySetReset(&pVm->aSelf);` |
|         - | 3145 | `	/* Shutdown callbacks are normally drained+released by VmInvokeShutdownCallbacks` |
|         - | 3146 | `	 * at the end of exec; release any that survived an abandoned run (e.g. exit()` |
|         - | 3147 | `	 * inside a shutdown callback) so their owned callback/arg values don't leak. */` |
|        16 | 3148 | `	for( n = 0 ; n < SySetUsed(&pVm->aShutdown) ; ++n ){` |
|       ! 0 | 3149 | `		VmShutdownCB *pCB = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|       ! 0 | 3150 | `		if( pCB ){` |
|         - | 3151 | `			int iArg;` |
|       ! 0 | 3152 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3153 | `			for( iArg = 0 ; iArg < pCB->nArg ; ++iArg ){` |
|       ! 0 | 3154 | `				PH7_MemObjRelease(&pCB->aArg[iArg]);` |
|       ! 0 | 3155 | `			}` |
|       ! 0 | 3156 | `		}` |
|       ! 0 | 3157 | `	}` |
|        16 | 3158 | `	SySetReset(&pVm->aShutdown);` |
|         - | 3159 | `	/* Stage 2b: free any leftover per-activation exception clones (an` |
|         - | 3160 | `	 * aborted program can leave entries behind). */` |
|        16 | 3161 | `	VmExcReleaseAll(&(*pVm),&pVm->aException);` |
|        16 | 3162 | `	SySetReset(&pVm->aException);` |
|        16 | 3163 | `	SySetReset(&pVm->aFinallyAction);` |
|        16 | 3164 | `	pVm->pPendingException = 0;` |
|        16 | 3165 | `	pVm->pInflightException = 0;` |
|        16 | 3166 | `	pVm->nInflightExcBase = 0;` |
|        16 | 3167 | `	pVm->pResumeFrame = 0;` |
|        16 | 3168 | `	pVm->iResumePc = 0;` |
|        16 | 3169 | `	pVm->pResumeInstr = 0;` |
|        16 | 3170 | `	pVm->iResumeStackDepth = 0;` |
|        16 | 3171 | `	pVm->nBoundaryRc = 0;` |
|        16 | 3172 | `	pVm->pConstEvalClass = 0;` |
|        16 | 3173 | `	pVm->nConstEvalDepth = 0;` |
|        16 | 3174 | `	pVm->pConstCycleAttr = 0;` |
|        16 | 3175 | `	pVm->pConstCycleClass = 0;` |
|        16 | 3176 | `	SySetReset(&pVm->aMagicGuard);` |
|         - | 3177 | `	{` |
|         - | 3178 | `		/* Drop any pending write-back entries (each owns one instance ref;` |
|         - | 3179 | `		 * MAGIC entries own a name blob; scratch slots die with aMemObj) */` |
|        16 | 3180 | `		VmHookRmw *aRmw = (VmHookRmw *)SySetBasePtr(&pVm->aHookRmw);` |
|        16 | 3181 | `		sxu32 nRmw = SySetUsed(&pVm->aHookRmw);` |
|         - | 3182 | `		sxu32 iRmw;` |
|        16 | 3183 | `		for( iRmw = 0 ; iRmw < nRmw ; ++iRmw ){` |
|       ! 0 | 3184 | `			SyBlobRelease(&aRmw[iRmw].sName);` |
|       ! 0 | 3185 | `			PH7_ClassInstanceUnref(aRmw[iRmw].pThis);` |
|       ! 0 | 3186 | `		}` |
|        16 | 3187 | `		SySetReset(&pVm->aHookRmw);` |
|         - | 3188 | `	}` |
|        16 | 3189 | `	if( pVm->pMagicSetThis ){` |
|       ! 0 | 3190 | `		PH7_ClassInstanceUnref(pVm->pMagicSetThis);` |
|       ! 0 | 3191 | `		pVm->pMagicSetThis = 0;` |
|       ! 0 | 3192 | `	}` |
|        16 | 3193 | `	SyBlobRelease(&pVm->sMagicSetName);` |
|        16 | 3194 | `	if( pVm->pHookSetThis ){` |
|       ! 0 | 3195 | `		PH7_ClassInstanceUnref(pVm->pHookSetThis);` |
|       ! 0 | 3196 | `		pVm->pHookSetThis = 0;` |
|       ! 0 | 3197 | `	}` |
|        16 | 3198 | `	pVm->pHookSetAttr = 0;` |
|        16 | 3199 | `	pVm->nHookSetIdx = SXU32_HIGH;` |
|        16 | 3200 | `	if( pVm->pMagicCallThis ){` |
|       ! 0 | 3201 | `		PH7_ClassInstanceUnref(pVm->pMagicCallThis);` |
|       ! 0 | 3202 | `		pVm->pMagicCallThis = 0;` |
|       ! 0 | 3203 | `	}` |
|        16 | 3204 | `	pVm->pMagicCallClass = 0;` |
|        16 | 3205 | `	SyBlobRelease(&pVm->sMagicCallName);` |
|        16 | 3206 | `	pVm->nExceptDepth = 0;` |
|         - | 3207 | `	/* spl_autoload_register() callbacks are per request */` |
|        16 | 3208 | `	for( n = 0 ; n < SySetUsed(&pVm->aAutoload) ; ++n ){` |
|       ! 0 | 3209 | `		VmAutoloadCB *pCB = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       ! 0 | 3210 | `		if( pCB ){` |
|       ! 0 | 3211 | `			PH7_MemObjRelease(&pCB->sCallback);` |
|       ! 0 | 3212 | `		}` |
|       ! 0 | 3213 | `	}` |
|        16 | 3214 | `	SySetReset(&pVm->aAutoload);` |
|         - | 3215 | `	/* The reentrancy guard is empty outside an active autoload (the common case);` |
|         - | 3216 | `	 * only rebuild the table when an aborted autoload left entries behind. */` |
|        16 | 3217 | `	if( SyHashTotalEntry(&pVm->hAutoloadActive) ){` |
|       ! 0 | 3218 | `		SyHashRelease(&pVm->hAutoloadActive);` |
|       ! 0 | 3219 | `		SyHashInit(&pVm->hAutoloadActive,&pVm->sAllocator,0,0);` |
|       ! 0 | 3220 | `	SyHashInit(&pVm->hWeakCell,&pVm->sAllocator,0,0);` |
|       ! 0 | 3221 | `	}` |
|         - | 3222 | `	/* Output buffers */` |
|        16 | 3223 | `	for( n = 0 ; n < SySetUsed(&pVm->aOB) ; ++n ){` |
|       ! 0 | 3224 | `		VmObEntry *pOb = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|       ! 0 | 3225 | `		if( pOb ){` |
|       ! 0 | 3226 | `			PH7_MemObjRelease(&pOb->sCallback);` |
|       ! 0 | 3227 | `			SyBlobRelease(&pOb->sOB);` |
|       ! 0 | 3228 | `		}` |
|       ! 0 | 3229 | `	}` |
|        16 | 3230 | `	SySetReset(&pVm->aOB);` |
|        16 | 3231 | `	pVm->nObDepth = 0;` |
|         - | 3232 | `	/* (9) Rebuild the global frame and the superglobals. */` |
|         - | 3233 | `	{` |
|        16 | 3234 | `		sxi32 rc = VmEnterFrame(&(*pVm),0,0,0);` |
|        16 | 3235 | `		if( rc == SXRET_OK ){` |
|        16 | 3236 | `			rc = PH7_HashmapCreateSuper(&(*pVm));` |
|         8 | 3237 | `		}` |
|        16 | 3238 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 3239 | `			return rc;` |
|         - | 3240 | `		}` |
|         - | 3241 | `	}` |
|         - | 3242 | `	/* (10) Re-mount the static/const attribute slots of every class. First` |
|         - | 3243 | `	 * invalidate every const/static slot index across ALL classes: the object` |
|         - | 3244 | `	 * pool was truncated, so the old indexes are stale, and the mount loop` |
|         - | 3245 | `	 * (plus the on-demand constant evaluator it can trigger) skips attributes` |
|         - | 3246 | `	 * whose nIdx is already set. Enum case singletons re-materialize lazily. */` |
|         - | 3247 | `	{` |
|         - | 3248 | `		SyHashEntry *pEntry;` |
|        16 | 3249 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      2628 | 3250 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      2612 | 3251 | `			ph7_class *pClass = (ph7_class *)pEntry->pUserData;` |
|         - | 3252 | `			ph7_class_attr *pAttr;` |
|         - | 3253 | `			SyHashEntry *pAttrEntry;` |
|      2612 | 3254 | `			SyHashResetLoopCursor(&pClass->hAttr);` |
|     13794 | 3255 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hAttr)) != 0 ){` |
|      9876 | 3256 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      9876 | 3257 | `				if( pAttr->iFlags & (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC) ){` |
|         4 | 3258 | `					pAttr->nIdx = SXU32_HIGH;` |
|         4 | 3259 | `					pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|         2 | 3260 | `				}` |
|       ! 0 | 3261 | `			}` |
|         - | 3262 | `			/* Constants live in the separate hConst namespace; invalidate their` |
|         - | 3263 | `			 * slots too so VM reuse re-evaluates them. */` |
|      2612 | 3264 | `			SyHashResetLoopCursor(&pClass->hConst);` |
|      9188 | 3265 | `			while( (pAttrEntry = SyHashGetNextEntry(&pClass->hConst)) != 0 ){` |
|      6576 | 3266 | `				pAttr = (ph7_class_attr *)pAttrEntry->pUserData;` |
|      6576 | 3267 | `				pAttr->nIdx = SXU32_HIGH;` |
|      6576 | 3268 | `				pAttr->iFlags &= ~PH7_CLASS_ATTR_EVALING;` |
|       ! 0 | 3269 | `			}` |
|       ! 0 | 3270 | `		}` |
|        16 | 3271 | `		SyHashResetLoopCursor(&pVm->hClass);` |
|      2628 | 3272 | `		while( (pEntry = SyHashGetNextEntry(&pVm->hClass)) != 0 ){` |
|      2612 | 3273 | `			sxi32 rc = VmMountUserClassAttrs(&(*pVm),(ph7_class *)pEntry->pUserData);` |
|      2612 | 3274 | `			if( rc != SXRET_OK ){` |
|       ! 0 | 3275 | `				return rc;` |
|         - | 3276 | `			}` |
|       ! 0 | 3277 | `		}` |
|         - | 3278 | `	}` |
|         - | 3279 | `	/* (11) Reset the remaining scalar/per-exec fields. */` |
|        16 | 3280 | `	SyBlobReset(&pVm->sConsumer);` |
|        16 | 3281 | `	pVm->nOutputLen = 0;` |
|        16 | 3282 | `	VmReinitMemObj(&(*pVm),&pVm->sExec);` |
|        16 | 3283 | `	PH7_VmReleaseResponseHeaders(pVm);` |
|        16 | 3284 | `	pVm->iResponseStatus = 200;` |
|        16 | 3285 | `	pVm->bHeadersSent = 0;` |
|        16 | 3286 | `	pVm->bHttpContext = 0;` |
|        16 | 3287 | `	VmReinitMemObj(&(*pVm),&pVm->sExceptionCB);` |
|        16 | 3288 | `	VmReinitMemObj(&(*pVm),&pVm->sErrCB);` |
|        16 | 3289 | `	pVm->iErrCBLevels = PH7_E_ALL_MASK;` |
|        16 | 3290 | `	VmReleaseHandlerStack(&pVm->aExceptionCBSaved);` |
|        16 | 3291 | `	VmReleaseHandlerStack(&pVm->aErrCBSaved);` |
|        16 | 3292 | `	VmReinitMemObj(&(*pVm),&pVm->sAssertCallback);` |
|         - | 3293 | `	/* The session's userland save handler belongs to the request that installed` |
|         - | 3294 | `	 * it; a reused VM (the -S server's) must not route the next request's store` |
|         - | 3295 | `	 * through the previous script's object. */` |
|        16 | 3296 | `	VmReinitMemObj(&(*pVm),&pVm->sSessHandler);` |
|        16 | 3297 | `	pVm->bSessOpened = 0;` |
|        16 | 3298 | `	SyBlobReset(&pVm->sSessData);` |
|        16 | 3299 | `	pVm->json_rc = JSON_ERROR_NONE;` |
|         - | 3300 | `#ifdef PH7_ENABLE_PCRE` |
|        16 | 3301 | `	pVm->iPcreLastError = 0;` |
|         - | 3302 | `#endif` |
|         - | 3303 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3304 | `	/* Drop the libxml error queue and the previous request's documents */` |
|        16 | 3305 | `	PH7_LibxmlVmReset(&(*pVm));` |
|         - | 3306 | `#endif` |
|         - | 3307 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 3308 | `	/* Close the previous request's databases: a reused VM (the -S server's)` |
|         - | 3309 | `	 * must not answer the next request through a handle that request opened. */` |
|        16 | 3310 | `	PH7_PdoVmReset(&(*pVm));` |
|         - | 3311 | `#endif` |
|         - | 3312 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3313 | `	/* Same rule for the previous request's curl handles, which hold sockets` |
|         - | 3314 | `	 * and a connection cache of their own. */` |
|        16 | 3315 | `	PH7_CurlVmReset(&(*pVm));` |
|         - | 3316 | `#endif` |
|         - | 3317 | `	/* Drop the stream contexts this run created, the default one included: a` |
|         - | 3318 | `	 * reused VM (the -S server's) must not answer the next request from the` |
|         - | 3319 | `	 * previous one's stream_context_set_default(). */` |
|        16 | 3320 | `	PH7_StreamCtxVmReset(&(*pVm));` |
|         - | 3321 | `	/* And every filter INSTANCE it created: a chain that was never removed` |
|         - | 3322 | `	 * still owns memory the next request must not inherit. */` |
|        16 | 3323 | `	PH7_StreamFilterVmReset(&(*pVm));` |
|        16 | 3324 | `	pVm->iCmpCallbackExc = 0;` |
|        16 | 3325 | `	pVm->bHaltRequested = 0;` |
|        16 | 3326 | `	pVm->iExitStatus = 0;` |
|        16 | 3327 | `	pVm->nSpreadCallBase = 0;` |
|        16 | 3328 | `	VmSpreadCaptureReset(pVm);` |
|        16 | 3329 | `	pVm->nRecursionDepth = 0;` |
|        16 | 3330 | `	pVm->pActiveCtx = 0;` |
|        16 | 3331 | `	pVm->pCoalesceObj = 0;` |
|        16 | 3332 | `	pVm->bCoalesceArmed = 0;` |
|        16 | 3333 | `	VmReinitMemObj(&(*pVm),&pVm->sCoalesceKey);` |
|         - | 3334 | `	/* Re-roll the uniqid() seed, matching PH7_VmMakeReady(). */` |
|        16 | 3335 | `	pVm->unique_id = PH7_VmRandomNum(&(*pVm)) & 1023;` |
|         - | 3336 | `	/* Restart object handle ids per exec so a reused VM (e.g. the -S server)` |
|         - | 3337 | `	 * looks like a fresh process, matching PH7_VmMakeReady(). */` |
|        16 | 3338 | `	pVm->nNextObjId = 1;` |
|         - | 3339 | `	/* Set the ready flag */` |
|        16 | 3340 | `	pVm->nMagic = PH7_VM_RUN;` |
|        16 | 3341 | `	return SXRET_OK;` |
|         8 | 3342 | `}` |
|         - | 3343 | `/*` |
|         - | 3344 | ` * Release a Virtual Machine.` |
|         - | 3345 | ` * Every virtual machine must be destroyed in order to avoid memory leaks.` |
|         - | 3346 | ` */` |
|      4656 | 3347 | `PH7_PRIVATE sxi32 PH7_VmRelease(ph7_vm *pVm)` |
|         5 | 3348 | `{` |
|         - | 3349 | `	/* Set the stale magic number */` |
|      4661 | 3350 | `	pVm->nMagic = PH7_VM_STALE;` |
|         - | 3351 | `#ifdef PH7_ENABLE_LIBXML` |
|         - | 3352 | `	/* Free the libxml document registry (libxml2 allocations live outside` |
|         - | 3353 | `	 * SyMemBackend, so the wholesale release below would leak them). */` |
|      4661 | 3354 | `	PH7_LibxmlVmRelease(pVm);` |
|         - | 3355 | `#endif` |
|         - | 3356 | `#ifdef PH7_ENABLE_SQLITE` |
|         - | 3357 | `	/* Same rule for the sqlite3 handles behind still-open PDO objects. */` |
|      4661 | 3358 | `	PH7_PdoVmRelease(pVm);` |
|         - | 3359 | `#endif` |
|         - | 3360 | `#ifdef PH7_ENABLE_CURL` |
|         - | 3361 | `	/* Same rule for the libcurl handles behind still-open CurlHandle objects. */` |
|      4661 | 3362 | `	PH7_CurlVmRelease(pVm);` |
|         - | 3363 | `#endif` |
|         - | 3364 | `	/* Same rule for the OS directory streams behind still-open directory` |
|         - | 3365 | `	 * iterators: the DIR lives outside the backend. */` |
|      4661 | 3366 | `	PH7_SplDirVmRelease(pVm);` |
|         - | 3367 | `	/* Release the private memory subsystem */` |
|      4661 | 3368 | `	SyMemBackendRelease(&pVm->sAllocator);` |
|      4661 | 3369 | `	return SXRET_OK;` |
|         5 | 3370 | `}` |
|         - | 3371 | `/*` |
|         - | 3372 | ` * Initialize a foreign function call context.` |
|         - | 3373 | ` * The context in which a foreign function executes is stored in a ph7_context object.` |
|         - | 3374 | ` * A pointer to a ph7_context object is always first parameter to application-defined foreign` |
|         - | 3375 | ` * functions.` |
|         - | 3376 | ` * The application-defined foreign function implementation will pass this pointer through into` |
|         - | 3377 | ` * calls to dozens of interfaces,these includes ph7_result_int(), ph7_result_string(), ph7_result_value(),` |
|         - | 3378 | ` * ph7_context_new_scalar(), ph7_context_alloc_chunk(), ph7_context_output(), ph7_context_throw_error()` |
|         - | 3379 | ` * and many more. Refer to the C/C++ Interfaces documentation for additional information.` |
|         - | 3380 | ` */` |
|   2970592 | 3381 | `PH7_PRIVATE sxi32 VmInitCallContext(` |
|         - | 3382 | `	ph7_context *pOut,    /* Call Context */` |
|         - | 3383 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 3384 | `	ph7_user_func *pFunc, /* Foreign function to execute shortly */` |
|         - | 3385 | `	ph7_value *pRet,      /* Store return value here*/` |
|         - | 3386 | `	sxi32 iFlags          /* Control flags */` |
|         - | 3387 | `	)` |
|         5 | 3388 | `{` |
|   2970597 | 3389 | `	pOut->pFunc = pFunc;` |
|   2970597 | 3390 | `	pOut->pVm   = pVm;` |
|   2970597 | 3391 | `	SySetInit(&pOut->sVar,&pVm->sAllocator,sizeof(ph7_value *));` |
|   2970597 | 3392 | `	SySetInit(&pOut->sChunk,&pVm->sAllocator,sizeof(ph7_aux_data));` |
|         - | 3393 | `	/* Assume a null return value */` |
|   2970597 | 3394 | `	MemObjSetType(pRet,MEMOBJ_NULL);` |
|   2970597 | 3395 | `	pOut->pRet = pRet;` |
|   2970597 | 3396 | `	pOut->iFlags = iFlags;` |
|   2970597 | 3397 | `	pOut->nThrowRc = 0; /* Set by PH7_VmThrowException, read back by VmHostFuncThrowRc */` |
|   2970597 | 3398 | `	pOut->pArgMap = 0; /* Set by the OP_CALL dispatcher for named-arg-aware builtins */` |
|         - | 3399 | `	/* Native-method receiver: left empty here and filled in by the OP_CALL dispatcher` |
|         - | 3400 | `	 * for a VM_FUNC_NATIVE callee only, so a plain host function always sees 0. The` |
|         - | 3401 | `	 * sThis view stays uninitialized until PH7_ContextThisValue() asks for it —` |
|         - | 3402 | `	 * bThisInit is the gate, and VmReleaseCallContext tears it down. */` |
|   2970597 | 3403 | `	pOut->pThis = 0;` |
|   2970597 | 3404 | `	pOut->pCalledClass = 0;` |
|   2970597 | 3405 | `	pOut->bThisInit = 0;` |
|   2970597 | 3406 | `	return SXRET_OK;` |
|         5 | 3407 | `}` |
|         - | 3408 | `/*` |
|         - | 3409 | ` * Release a foreign function call context and cleanup the mess` |
|         - | 3410 | ` * left behind.` |
|         - | 3411 | ` */` |
|   2970592 | 3412 | `PH7_PRIVATE void VmReleaseCallContext(ph7_context *pCtx)` |
|         5 | 3413 | `{` |
|         - | 3414 | `	sxu32 n;` |
|   2970597 | 3415 | `	if( pCtx->bThisInit ){` |
|         - | 3416 | `		/* The lazy $this view. It only ever aliases the receiver (MEMOBJ_OBJ pointing` |
|         - | 3417 | `		 * at pThis without a refcount bump — see PH7_ContextThisValue), so releasing` |
|         - | 3418 | `		 * it must NOT unref the instance: clear the object flag first and let` |
|         - | 3419 | `		 * PH7_MemObjRelease free nothing but the (empty) blob. */` |
|      5641 | 3420 | `		pCtx->sThis.iFlags = MEMOBJ_NULL;` |
|      5641 | 3421 | `		pCtx->sThis.x.pOther = 0;` |
|      5641 | 3422 | `		PH7_MemObjRelease(&pCtx->sThis);` |
|      5641 | 3423 | `		pCtx->bThisInit = 0;` |
|      2818 | 3424 | `	}` |
|   2970597 | 3425 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|     17839 | 3426 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|     55855 | 3427 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|     38021 | 3428 | `			if( apObj[n] == 0 ){` |
|         - | 3429 | `				/* Already released */` |
|      4131 | 3430 | `				continue;` |
|         - | 3431 | `			}` |
|     33895 | 3432 | `			PH7_MemObjRelease(apObj[n]);` |
|     33895 | 3433 | `			SyMemBackendPoolFree(&pCtx->pVm->sAllocator,apObj[n]);` |
|     16950 | 3434 | `		}` |
|     17839 | 3435 | `		SySetRelease(&pCtx->sVar);` |
|      8917 | 3436 | `	}` |
|   2970597 | 3437 | `	if( SySetUsed(&pCtx->sChunk) > 0 ){` |
|         - | 3438 | `		ph7_aux_data *aAux;` |
|         - | 3439 | `		void *pChunk;` |
|         - | 3440 | `		/* Automatic release of dynamically allocated chunk` |
|         - | 3441 | `		 * using [ph7_context_alloc_chunk()].` |
|         - | 3442 | `		 */` |
|      2051 | 3443 | `		aAux = (ph7_aux_data *)SySetBasePtr(&pCtx->sChunk);` |
|      6957 | 3444 | `		for( n = 0; n < SySetUsed(&pCtx->sChunk) ; ++n ){` |
|      4911 | 3445 | `			pChunk = aAux[n].pAuxData;` |
|         - | 3446 | `			/* Release the chunk */` |
|      4911 | 3447 | `			if( pChunk ){` |
|      4703 | 3448 | `				SyMemBackendFree(&pCtx->pVm->sAllocator,pChunk);` |
|      2349 | 3449 | `			}` |
|      2458 | 3450 | `		}` |
|      2051 | 3451 | `		SySetRelease(&pCtx->sChunk);` |
|      1023 | 3452 | `	}` |
|   2970597 | 3453 | `}` |
|         - | 3454 | `/*` |
|         - | 3455 | ` * Release a ph7_value allocated from the body of a foreign function.` |
|         - | 3456 | ` * Refer to [ph7_context_release_value()] for additional information.` |
|         - | 3457 | ` */` |
|      4126 | 3458 | `PH7_PRIVATE void PH7_VmReleaseContextValue(` |
|         - | 3459 | `	ph7_context *pCtx, /* Call context */` |
|         - | 3460 | `	ph7_value *pValue  /* Release this value */` |
|         - | 3461 | `	)` |
|         5 | 3462 | `{` |
|      4131 | 3463 | `	if( pValue == 0 ){` |
|         - | 3464 | `		/* NULL value is a harmless operation */` |
|       ! 0 | 3465 | `		return;` |
|         - | 3466 | `	}` |
|      4131 | 3467 | `	if( SySetUsed(&pCtx->sVar) > 0 ){` |
|      4131 | 3468 | `		ph7_value **apObj = (ph7_value **)SySetBasePtr(&pCtx->sVar);` |
|         - | 3469 | `		sxu32 n;` |
|    437891 | 3470 | `		for( n = 0 ; n < SySetUsed(&pCtx->sVar) ; ++n ){` |
|    437891 | 3471 | `			if( apObj[n] == pValue ){` |
|      4131 | 3472 | `				PH7_MemObjRelease(pValue);` |
|      4131 | 3473 | `				SyMemBackendPoolFree(&pCtx->pVm->sAllocator,pValue);` |
|         - | 3474 | `				/* Mark as released */` |
|      4131 | 3475 | `				apObj[n] = 0;` |
|      4131 | 3476 | `				break;` |
|         - | 3477 | `			}` |
|    216885 | 3478 | `		}` |
|      2063 | 3479 | `	}` |
|      2068 | 3480 | `}` |
|         - | 3481 | `/*` |
|         - | 3482 | ` * Pop and release as many memory object from the operand stack.` |
|         - | 3483 | ` */` |
|  16830514 | 3484 | `PH7_PRIVATE void VmPopOperand(` |
|         - | 3485 | `	ph7_value **ppTos, /* Operand stack */` |
|         - | 3486 | `	sxi32 nPop         /* Total number of memory objects to pop */` |
|         - | 3487 | `	)` |
|         5 | 3488 | `{` |
|  16830519 | 3489 | `	ph7_value *pTos = *ppTos;` |
|  34914167 | 3490 | `	while( nPop > 0 ){` |
|  18083653 | 3491 | `		PH7_MemObjRelease(pTos);` |
|  18083653 | 3492 | `		pTos--;` |
|  18083653 | 3493 | `		nPop--;` |
|         5 | 3494 | `	}` |
|         - | 3495 | `	/* Top of the stack */` |
|  16830519 | 3496 | `	*ppTos = pTos;` |
|  16830519 | 3497 | `}` |
|         - | 3498 | `/*` |
|         - | 3499 | ` * Reserve a memory object.` |
|         - | 3500 | ` * Return a pointer to the raw ph7_value on success. NULL on failure.` |
|         - | 3501 | ` */` |
|  19458158 | 3502 | `PH7_PRIVATE ph7_value * PH7_ReserveMemObj(ph7_vm *pVm)` |
|         5 | 3503 | `{` |
|  19458163 | 3504 | `	ph7_value *pObj = 0;` |
|         - | 3505 | `	VmSlot *pSlot;` |
|         - | 3506 | `	sxu32 nIdx;` |
|         - | 3507 | `	/* Check for a free slot */` |
|  19458163 | 3508 | `	nIdx = SXU32_HIGH; /* cc warning */` |
|  19458163 | 3509 | `	pSlot = (VmSlot *)SySetPop(&pVm->aFreeObj);` |
|  19458163 | 3510 | `	if( pSlot ){` |
|  16439224 | 3511 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pSlot->nIdx);` |
|  16439224 | 3512 | `		nIdx = pSlot->nIdx;` |
|   8220936 | 3513 | `	}` |
|  19458163 | 3514 | `	if( pObj == 0 ){` |
|         - | 3515 | `		/* Reserve a new memory object */` |
|   3018944 | 3516 | `		pObj = VmReserveMemObj(&(*pVm),&nIdx);` |
|   3018944 | 3517 | `		if( pObj == 0 ){` |
|       ! 0 | 3518 | `			return 0;` |
|         - | 3519 | `		}` |
|   1509461 | 3520 | `	}` |
|         - | 3521 | `	/* Set a null default value */` |
|  19458163 | 3522 | `	PH7_MemObjInit(&(*pVm),pObj);` |
|  19458163 | 3523 | `	pObj->nIdx = nIdx;` |
|  19458163 | 3524 | `	return pObj;` |
|   9730402 | 3525 | `}` |
|         - | 3526 | `/*` |
|         - | 3527 | ` * Insert an entry by reference (not copy) in the given hashmap.` |
|         - | 3528 | ` */` |
|     64708 | 3529 | `PH7_PRIVATE sxi32 VmHashmapRefInsert(` |
|         - | 3530 | `	ph7_hashmap *pMap, /* Target hashmap */` |
|         - | 3531 | `	const char *zKey,  /* Entry key */` |
|         - | 3532 | `	sxu32 nByte,       /* Key length */` |
|         - | 3533 | `	sxu32 nRefIdx      /* Entry index in the object pool */` |
|         - | 3534 | `	)` |
|         5 | 3535 | `{` |
|         - | 3536 | `	ph7_value sKey;` |
|         - | 3537 | `	sxi32 rc;` |
|     64713 | 3538 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     64713 | 3539 | `	PH7_MemObjStringAppend(&sKey,zKey,nByte);` |
|         - | 3540 | `	/* Perform the insertion */` |
|     64713 | 3541 | `	rc = PH7_HashmapInsertByRef(&(*pMap),&sKey,nRefIdx);` |
|     64713 | 3542 | `	PH7_MemObjRelease(&sKey);` |
|     64713 | 3543 | `	return rc;` |
|         5 | 3544 | `}` |
|         - | 3545 | `/*` |
|         - | 3546 | `` * The write side of php 8.1's $GLOBALS semantics: `$GLOBALS['x'] = $v` (or`` |
|         - | 3547 | `` * `=& $v`) from ANY scope behaves like a global-frame `$x = $v`, so a new`` |
|         - | 3548 | ` * key must create a real global variable — linked into the bottom frame's` |
|         - | 3549 | ` * hVar and registered by reference in the $GLOBALS hashmap, exactly like a` |
|         - | 3550 | ` * variable created by top-level code — so later reads and writes alias one` |
|         - | 3551 | ` * slot. Called from the hashmap layer when an insertion targets pGlobal.` |
|         - | 3552 | ` *   - pValue mode (nRefIdx == SXU32_HIGH): the named global receives a copy` |
|         - | 3553 | ` *     of pValue (NULL pValue nullifies), overwriting an existing global or` |
|         - | 3554 | ` *     superglobal in place.` |
|         - | 3555 | ` *   - reference mode (nRefIdx != SXU32_HIGH): the name is bound to that` |
|         - | 3556 | ` *     existing memobj slot ($GLOBALS['y'] =& $x). An EXISTING name is` |
|         - | 3557 | ` *     RE-BOUND (PH7_VmRebindVarSlot), the same rule OP_STORE_REF applies to` |
|         - | 3558 | ` *     a plain variable.` |
|         - | 3559 | ` */` |
|       186 | 3560 | `PH7_PRIVATE sxi32 PH7_VmInstallGlobalVar(ph7_vm *pVm,const char *zName,sxu32 nByte,ph7_value *pValue,sxu32 nRefIdx)` |
|         4 | 3561 | `{` |
|       190 | 3562 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 3563 | `	SyHashEntry *pEntry;` |
|         - | 3564 | `	ph7_value *pObj;` |
|         - | 3565 | `	char *zDup;` |
|         - | 3566 | `	sxu32 nIdx;` |
|         - | 3567 | `	sxi32 rc;` |
|         - | 3568 | `	/* Walk down to the global frame */` |
|       212 | 3569 | `	while( pFrame->pParent ){` |
|        24 | 3570 | `		pFrame = pFrame->pParent;` |
|         2 | 3571 | `	}` |
|         - | 3572 | `	/* An existing global (or superglobal) is overwritten in place */` |
|       190 | 3573 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|       190 | 3574 | `	if( pEntry && (sxu32)SX_PTR_TO_INT(pEntry->pUserData) == pVm->nGlobalIdx ){` |
|         - | 3575 | `		/* $GLOBALS['GLOBALS'] = ... must NOT clobber the live $GLOBALS slot:` |
|         - | 3576 | `		 * php creates an ordinary symbol-table entry named GLOBALS while the` |
|         - | 3577 | `		 * auto-global keeps resolving to the array. Fall through to the` |
|         - | 3578 | `		 * create-a-real-entry path (the hSuper lookup still wins for reads` |
|         - | 3579 | `		 * of $GLOBALS itself). */` |
|         5 | 3580 | `		pEntry = 0;` |
|         2 | 3581 | `	}` |
|       190 | 3582 | `	if( pEntry == 0 ){` |
|       190 | 3583 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|        93 | 3584 | `	}` |
|       190 | 3585 | `	if( pEntry ){` |
|         5 | 3586 | `		if( nRefIdx != SXU32_HIGH ){` |
|         - | 3587 | ``			/* `$GLOBALS['y'] =& $x` on an EXISTING global re-binds it, exactly as`` |
|         - | 3588 | ``			 * `$y = &$x` in global scope does (PH7_VmRebindVarSlot). */`` |
|         3 | 3589 | `			PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nRefIdx);` |
|         3 | 3590 | `			return SXRET_OK;` |
|         - | 3591 | `		}` |
|         3 | 3592 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,(sxu32)SX_PTR_TO_INT(pEntry->pUserData));` |
|         3 | 3593 | `		if( pObj == 0 ){` |
|       ! 0 | 3594 | `			return SXERR_NOTFOUND;` |
|         - | 3595 | `		}` |
|         3 | 3596 | `		if( pValue ){` |
|         3 | 3597 | `			PH7_MemObjStore(pValue,pObj);` |
|         2 | 3598 | `		}else{` |
|       ! 0 | 3599 | `			PH7_MemObjToNull(pObj);` |
|         - | 3600 | `		}` |
|         3 | 3601 | `		return SXRET_OK;` |
|         - | 3602 | `	}` |
|       186 | 3603 | `	if( nRefIdx == SXU32_HIGH ){` |
|         - | 3604 | `		/* Reserve a fresh slot for the new global */` |
|       184 | 3605 | `		pObj = PH7_ReserveMemObj(&(*pVm));` |
|       184 | 3606 | `		if( pObj == 0 ){` |
|       ! 0 | 3607 | `			return SXERR_MEM;` |
|         - | 3608 | `		}` |
|       184 | 3609 | `		nIdx = pObj->nIdx;` |
|        94 | 3610 | `	}else{` |
|         - | 3611 | `		/* Reference assignment: bind the name to the existing slot */` |
|         3 | 3612 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nRefIdx);` |
|         3 | 3613 | `		if( pObj == 0 ){` |
|       ! 0 | 3614 | `			return SXERR_NOTFOUND;` |
|         - | 3615 | `		}` |
|         3 | 3616 | `		nIdx = nRefIdx;` |
|         - | 3617 | `	}` |
|       186 | 3618 | `	zDup = SyMemBackendStrDup(&pVm->sAllocator,zName,nByte);` |
|       186 | 3619 | `	if( zDup == 0 ){` |
|       ! 0 | 3620 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3621 | `			/* Return the reserved slot to the free pool (as VmExtractMemObj` |
|         - | 3622 | `			 * does) so an OOM here doesn't burn aMemObj slots. */` |
|         - | 3623 | `			VmSlot sFree;` |
|       ! 0 | 3624 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3625 | `			sFree.pUserData = 0;` |
|       ! 0 | 3626 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3627 | `		}` |
|       ! 0 | 3628 | `		return SXERR_MEM;` |
|         - | 3629 | `	}` |
|       186 | 3630 | `	rc = SyHashInsert(&pFrame->hVar,(const void *)zDup,nByte,SX_INT_TO_PTR(nIdx));` |
|       186 | 3631 | `	if( rc != SXRET_OK ){` |
|       ! 0 | 3632 | `		if( nRefIdx == SXU32_HIGH ){` |
|         - | 3633 | `			VmSlot sFree;` |
|       ! 0 | 3634 | `			sFree.nIdx = nIdx;` |
|       ! 0 | 3635 | `			sFree.pUserData = 0;` |
|       ! 0 | 3636 | `			SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       ! 0 | 3637 | `		}` |
|       ! 0 | 3638 | `		SyMemBackendFree(&pVm->sAllocator,zDup);` |
|       ! 0 | 3639 | `		return rc;` |
|         - | 3640 | `	}` |
|         - | 3641 | `	/* Register in the $GLOBALS array (by reference, like any global) */` |
|       186 | 3642 | `	VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|       186 | 3643 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       186 | 3644 | `	if( nRefIdx == SXU32_HIGH ){` |
|       184 | 3645 | `		pObj->nIdx = nIdx;` |
|       184 | 3646 | `		if( pValue ){` |
|       172 | 3647 | `			PH7_MemObjStore(pValue,pObj);` |
|        84 | 3648 | `		}` |
|        90 | 3649 | `	}` |
|       186 | 3650 | `	return SXRET_OK;` |
|        97 | 3651 | `}` |
|         - | 3652 | `/*` |
|         - | 3653 | ` * Extract a variable value from the top active VM frame.` |
|         - | 3654 | ` * Return a pointer to the variable value on success.` |
|         - | 3655 | ` * NULL otherwise (non-existent variable/Out-of-memory,...).` |
|         - | 3656 | ` */` |
|  11877941 | 3657 | `PH7_PRIVATE ph7_value * VmExtractMemObj(` |
|         - | 3658 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 3659 | `	const SyString *pName, /* Variable name */` |
|         - | 3660 | `	int bDup,              /* True to duplicate variable name */` |
|         - | 3661 | `	int bCreate            /* True to create the variable if non-existent */` |
|         - | 3662 | `	)` |
|         5 | 3663 | `{` |
|  11877946 | 3664 | `	int bNullify = FALSE;` |
|         - | 3665 | `	SyHashEntry *pEntry;` |
|         - | 3666 | `	VmFrame *pFrame;` |
|         - | 3667 | `	ph7_value *pObj;` |
|         - | 3668 | `	sxu32 nIdx;` |
|         - | 3669 | `	sxi32 rc;` |
|         - | 3670 | `	/* Point to the top active frame */` |
|  11877946 | 3671 | `	pFrame = pVm->pFrame;` |
|  11877946 | 3672 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|         - | 3673 | `	/* Perform the lookup */` |
|  11877946 | 3674 | `	if( pName == 0 \|\| pName->nByte < 1 ){` |
|         - | 3675 | `		static const SyString sAnnon = { " " , sizeof(char) };` |
|        18 | 3676 | `		pName = &sAnnon;` |
|         - | 3677 | `		/* Always nullify the object */` |
|        18 | 3678 | `		bNullify = TRUE;` |
|        18 | 3679 | `		bDup = FALSE;` |
|         8 | 3680 | `	}` |
|         - | 3681 | `	/* Check the superglobals table first */` |
|  11877946 | 3682 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)pName->zString,pName->nByte);` |
|  11877946 | 3683 | `	if( pEntry == 0 ){` |
|         - | 3684 | `		/* Query the top active frame */` |
|  11877234 | 3685 | `		pEntry = SyHashGet(&pFrame->hVar,(const void *)pName->zString,pName->nByte);` |
|  11877234 | 3686 | `		if( pEntry == 0 ){` |
|   1182277 | 3687 | `			char *zName = (char *)pName->zString;` |
|         - | 3688 | `			VmSlot sLocal;` |
|   1182277 | 3689 | `			if( !bCreate ){` |
|         - | 3690 | `				/* Do not create the variable,return NULL instead */` |
|      9829 | 3691 | `				return 0;` |
|         - | 3692 | `			}` |
|         - | 3693 | `			/* No such variable,automatically create a new one and install` |
|         - | 3694 | `			 * it in the current frame.` |
|         - | 3695 | `			 */` |
|   1172453 | 3696 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|   1172453 | 3697 | `			if( pObj == 0 ){` |
|       ! 0 | 3698 | `				return 0;` |
|         - | 3699 | `			}` |
|   1172453 | 3700 | `			nIdx = pObj->nIdx;` |
|   1172453 | 3701 | `			if( bDup ){` |
|         - | 3702 | `				/* Duplicate name */` |
|      4225 | 3703 | `				zName = SyMemBackendStrDup(&pVm->sAllocator,pName->zString,pName->nByte);` |
|      4225 | 3704 | `				if( zName == 0 ){` |
|       ! 0 | 3705 | `					return 0;` |
|         - | 3706 | `				}` |
|      2101 | 3707 | `			}` |
|         - | 3708 | `			/* Link to the top active VM frame */` |
|   1172453 | 3709 | `			rc = SyHashInsert(&pFrame->hVar,zName,pName->nByte,SX_INT_TO_PTR(nIdx));` |
|   1172453 | 3710 | `			if( rc != SXRET_OK ){` |
|         - | 3711 | `				/* Return the slot to the free pool */` |
|       ! 0 | 3712 | `				sLocal.nIdx = nIdx;` |
|       ! 0 | 3713 | `				sLocal.pUserData = 0;` |
|       ! 0 | 3714 | `				SySetPut(&pVm->aFreeObj,(const void *)&sLocal);` |
|       ! 0 | 3715 | `				return 0;` |
|         - | 3716 | `			}` |
|   1172453 | 3717 | `			if( pFrame->pParent != 0 ){` |
|         - | 3718 | `				/* Local variable */` |
|   1159221 | 3719 | `				sLocal.nIdx = nIdx;` |
|   1159221 | 3720 | `				SySetPut(&pFrame->sLocal,(const void *)&sLocal);` |
|    594156 | 3721 | `			}else if( !PH7_VmVarNameIsInternal(pName->zString,pName->nByte) ){` |
|         - | 3722 | `				/* Register in the $GLOBALS array */` |
|     13049 | 3723 | `				VmHashmapRefInsert(pVm->pGlobal,pName->zString,pName->nByte,nIdx);` |
|      6522 | 3724 | `			}` |
|         - | 3725 | `			/* Install in the reference table */` |
|   1172453 | 3726 | `			PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|         - | 3727 | `			/* Save object index */` |
|   1172453 | 3728 | `			pObj->nIdx = nIdx;` |
|    587540 | 3729 | `		}else{` |
|         - | 3730 | `			/* Extract variable contents */` |
|  10694962 | 3731 | `			nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|  10694962 | 3732 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|  10694962 | 3733 | `			if( bNullify && pObj ){` |
|         3 | 3734 | `				PH7_MemObjRelease(pObj);` |
|         1 | 3735 | `			}` |
|         - | 3736 | `		}` |
|   5941394 | 3737 | `	}else{` |
|         - | 3738 | `		/* Superglobal */` |
|       717 | 3739 | `		nIdx = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|       717 | 3740 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 3741 | `	}` |
|  11868122 | 3742 | `	return pObj;` |
|   5946662 | 3743 | `}` |
|         - | 3744 | `/*` |
|         - | 3745 | ` * Extract a superglobal variable such as $_GET,$_POST,$_HEADERS,....` |
|         - | 3746 | ` * Return a pointer to the variable value on success.NULL otherwise.` |
|         - | 3747 | ` */` |
|     42820 | 3748 | `PH7_PRIVATE ph7_value * PH7_VmExtractSuper(` |
|         - | 3749 | `	ph7_vm *pVm,       /* Target VM */` |
|         - | 3750 | `	const char *zName, /* Superglobal name: NOT NULL TERMINATED */` |
|         - | 3751 | `	sxu32 nByte        /* zName length */` |
|         - | 3752 | `	)` |
|         5 | 3753 | `{` |
|         - | 3754 | `	SyHashEntry *pEntry;` |
|         - | 3755 | `	ph7_value *pValue;` |
|         - | 3756 | `	sxu32 nIdx;` |
|         - | 3757 | `	/* Query the superglobal table */` |
|     42825 | 3758 | `	pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     42825 | 3759 | `	if( pEntry == 0 ){` |
|         - | 3760 | `		/* No such entry */` |
|       ! 0 | 3761 | `		return 0;` |
|         - | 3762 | `	}` |
|         - | 3763 | `	/* Extract the superglobal index in the global object pool */` |
|     42825 | 3764 | `	nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3765 | `	/* Extract the variable value  */` |
|     42825 | 3766 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|     42825 | 3767 | `	return pValue;` |
|     21415 | 3768 | `}` |
|         - | 3769 | `/*` |
|         - | 3770 | ` * Perform a raw hashmap insertion.` |
|         - | 3771 | ` * Refer to the [PH7_VmConfigure()] implementation for additional information.` |
|         - | 3772 | ` */` |
|     33406 | 3773 | `PH7_PRIVATE sxi32 PH7_VmHashmapInsert(` |
|         - | 3774 | `	ph7_hashmap *pMap,  /* Target hashmap  */` |
|         - | 3775 | `	const char *zKey,   /* Entry key */` |
|         - | 3776 | `	int nKeylen,        /* zKey length*/` |
|         - | 3777 | `	const char *zData,  /* Entry data */` |
|         - | 3778 | `	int nLen            /* zData length */` |
|         - | 3779 | `	)` |
|         5 | 3780 | `{` |
|         - | 3781 | `	ph7_value sKey,sValue;` |
|         - | 3782 | `	sxi32 rc;` |
|     33411 | 3783 | `	PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|     33411 | 3784 | `	PH7_MemObjInitFromString(pMap->pVm,&sValue,0);` |
|     33411 | 3785 | `	if( zKey ){` |
|     28711 | 3786 | `		if( nKeylen < 0 ){` |
|     28537 | 3787 | `			nKeylen = (int)SyStrlen(zKey);` |
|     14266 | 3788 | `		}` |
|     28711 | 3789 | `		PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKeylen);` |
|     14353 | 3790 | `	}` |
|     33411 | 3791 | `	if( zData ){` |
|     33411 | 3792 | `		if( nLen < 0 ){` |
|         - | 3793 | `			/* Compute length automatically */` |
|     18913 | 3794 | `			nLen = (int)SyStrlen(zData);` |
|      9454 | 3795 | `		}` |
|     33411 | 3796 | `		PH7_MemObjStringAppend(&sValue,zData,(sxu32)nLen);` |
|     16703 | 3797 | `	}` |
|         - | 3798 | `	/* Perform the insertion. A NULL zKey means "append": pass a NULL key, NOT the empty` |
|         - | 3799 | `	 * string sKey — an empty string is a real key now ($a[""]), it no longer collapses` |
|         - | 3800 | `	 * into an automatic index. $argv is built through here, so getting this wrong files` |
|         - | 3801 | `	 * every argument under "". */` |
|     33411 | 3802 | `	rc = PH7_HashmapInsert(&(*pMap),zKey ? &sKey : 0,&sValue);` |
|     33411 | 3803 | `	PH7_MemObjRelease(&sKey);` |
|     33411 | 3804 | `	PH7_MemObjRelease(&sValue);` |
|     33411 | 3805 | `	return rc;` |
|         5 | 3806 | `}` |
|         - | 3807 | `/*` |
|         - | 3808 | ` * Parse a php.ini boolean value the way zend_ini does: "on"/"yes"/"true"` |
|         - | 3809 | ` * (case-insensitive) are true, any other string is true iff it parses as a` |
|         - | 3810 | ` * non-zero integer ("1" -> on; "0"/""/"off"/"false"/"no" -> off).` |
|         - | 3811 | ` */` |
|        34 | 3812 | `static int VmIniBool(const char *zValue,sxu32 nValue)` |
|         4 | 3813 | `{` |
|        38 | 3814 | `	sxi64 iVal = 0;` |
|        38 | 3815 | `	if( nValue == 0 ){` |
|       ! 0 | 3816 | `		return 0;` |
|         - | 3817 | `	}` |
|        34 | 3818 | `	if( (nValue == 2 && SyStrnicmp(zValue,"on",2) == 0)` |
|        34 | 3819 | `	 \|\| (nValue == 3 && SyStrnicmp(zValue,"yes",3) == 0)` |
|        38 | 3820 | `	 \|\| (nValue == 4 && SyStrnicmp(zValue,"true",4) == 0) ){` |
|       ! 0 | 3821 | `		return 1;` |
|         - | 3822 | `	}` |
|        38 | 3823 | `	SyStrToInt64(zValue,nValue,(void *)&iVal,0);` |
|        38 | 3824 | `	return iVal != 0;` |
|        21 | 3825 | `}` |
|         - | 3826 | `/*` |
|         - | 3827 | ` * Configure a working virtual machine instance.` |
|         - | 3828 | ` *` |
|         - | 3829 | ` * This routine is used to configure a PH7 virtual machine obtained by a prior` |
|         - | 3830 | ` * successful call to one of the compile interface such as ph7_compile()` |
|         - | 3831 | ` * ph7_compile_v2() or ph7_compile_file().` |
|         - | 3832 | ` * The second argument to this function is an integer configuration option` |
|         - | 3833 | ` * that determines what property of the PH7 virtual machine is to be configured.` |
|         - | 3834 | ` * Subsequent arguments vary depending on the configuration option in the second` |
|         - | 3835 | ` * argument. There are many verbs but the most important are PH7_VM_CONFIG_OUTPUT,` |
|         - | 3836 | ` * PH7_VM_CONFIG_HTTP_REQUEST and PH7_VM_CONFIG_ARGV_ENTRY.` |
|         - | 3837 | ` * Refer to the official documentation for the list of allowed verbs.` |
|         - | 3838 | ` */` |
|    126778 | 3839 | `PH7_PRIVATE sxi32 PH7_VmConfigure(` |
|         - | 3840 | `	ph7_vm *pVm, /* Target VM */` |
|         - | 3841 | `	sxi32 nOp,   /* Configuration verb */` |
|         - | 3842 | `	va_list ap   /* Subsequent option arguments */` |
|         - | 3843 | `	)` |
|         5 | 3844 | `{` |
|    126783 | 3845 | `	sxi32 rc = SXRET_OK;` |
|    126783 | 3846 | `	switch(nOp){` |
|      2312 | 3847 | `	case PH7_VM_CONFIG_OUTPUT: {` |
|      4629 | 3848 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4629 | 3849 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3850 | `		/* VM output consumer callback */` |
|         - | 3851 | `#ifdef UNTRUST` |
|         - | 3852 | `		if( xConsumer == 0 ){` |
|         - | 3853 | `			rc = SXERR_CORRUPT;` |
|         - | 3854 | `			break;` |
|         - | 3855 | `		}` |
|         - | 3856 | `#endif` |
|         - | 3857 | `		/* Install the output consumer */` |
|      4629 | 3858 | `		pVm->sVmConsumer.xConsumer = xConsumer;` |
|      4629 | 3859 | `		pVm->sVmConsumer.pUserData = pUserData;` |
|      4629 | 3860 | `		break;` |
|         - | 3861 | `							   }` |
|      2312 | 3862 | `	case PH7_VM_CONFIG_ERR_STREAM: {` |
|      4629 | 3863 | `		ProcConsumer xConsumer = va_arg(ap,ProcConsumer);` |
|      4629 | 3864 | `		void *pUserData = va_arg(ap,void *);` |
|         - | 3865 | `		/* Diagnostics (stderr) consumer: runtime warnings/notices/deprecations and` |
|         - | 3866 | `		 * uncaught-exception fatals route their LOG copy here (gated by log_errors)` |
|         - | 3867 | `		 * instead of the program-output stream. */` |
|         - | 3868 | `#ifdef UNTRUST` |
|         - | 3869 | `		if( xConsumer == 0 ){` |
|         - | 3870 | `			rc = SXERR_CORRUPT;` |
|         - | 3871 | `			break;` |
|         - | 3872 | `		}` |
|         - | 3873 | `#endif` |
|      4629 | 3874 | `		pVm->sVmErrConsumer.xConsumer = xConsumer;` |
|      4629 | 3875 | `		pVm->sVmErrConsumer.pUserData = pUserData;` |
|      4629 | 3876 | `		break;` |
|         - | 3877 | `								   }` |
|      2328 | 3878 | `	case PH7_VM_CONFIG_IMPORT_PATH: {` |
|         - | 3879 | `		/* Import path */` |
|         - | 3880 | `		  const char *zPath;` |
|         - | 3881 | `		  SyString sPath;` |
|      4661 | 3882 | `		  zPath = va_arg(ap,const char *);` |
|         - | 3883 | `#if defined(UNTRUST)` |
|         - | 3884 | `		  if( zPath == 0 ){` |
|         - | 3885 | `			  rc = SXERR_EMPTY;` |
|         - | 3886 | `			  break;` |
|         - | 3887 | `		  }` |
|         - | 3888 | `#endif` |
|      4661 | 3889 | `		  SyStringInitFromBuf(&sPath,zPath,SyStrlen(zPath));` |
|         - | 3890 | `		  /* Remove trailing slashes and backslashes */` |
|         - | 3891 | `#ifdef __WINNT__` |
|         5 | 3892 | `		  SyStringTrimTrailingChar(&sPath,'\\');` |
|         - | 3893 | `#endif` |
|      9317 | 3894 | `		  SyStringTrimTrailingChar(&sPath,'/');` |
|         - | 3895 | `		  /* Remove leading and trailing white spaces */` |
|      4661 | 3896 | `		  SyStringFullTrim(&sPath);` |
|      4661 | 3897 | `		  if( sPath.nByte > 0 ){` |
|         - | 3898 | `			  /* Store the path in the corresponding conatiner */` |
|      4661 | 3899 | `			  rc = SySetPut(&pVm->aPaths,(const void *)&sPath);` |
|      2328 | 3900 | `		  }` |
|      4661 | 3901 | `		  break;` |
|         - | 3902 | `									 }` |
|      2335 | 3903 | `	case PH7_VM_CONFIG_ERR_REPORT:` |
|         - | 3904 | `		/* Run-Time Error report */` |
|      4675 | 3905 | `		pVm->bErrReport = 1;` |
|      4675 | 3906 | `		pVm->iErrMask = PH7_E_ALL_MASK; /* E_ALL (php 8: E_STRICT/2048 is no longer part of E_ALL) */` |
|      4675 | 3907 | `		break;` |
|         2 | 3908 | `	case PH7_VM_CONFIG_RECURSION_DEPTH:{` |
|         - | 3909 | `		/* PHP call-depth cap (OP_CALL frames). The host default is UNBOUNDED` |
|         - | 3910 | `		 * (nMaxDepth == 0) — PHP frames are heap-bound since the iterative` |
|         - | 3911 | `		 * executor, so recursion is limited by memory like the main PHP engine.` |
|         - | 3912 | `		 * This is an embedder opt-in: any non-negative value installs a cap of that` |
|         - | 3913 | `		 * many frames; 0 restores the unbounded default. No upper clamp (the old` |
|         - | 3914 | `		 * <1024 clamp guarded the native stack the recursion no longer grows — that` |
|         - | 3915 | `		 * role is now PH7_VM_CONFIG_NATIVE_DEPTH). A negative value is ignored (it` |
|         - | 3916 | `		 * would otherwise read as an enormous positive cap). */` |
|         5 | 3917 | `		int nDepth = va_arg(ap,int);` |
|         5 | 3918 | `		if( nDepth >= 0 ){` |
|         5 | 3919 | `			pVm->nMaxDepth = nDepth;` |
|         2 | 3920 | `		}` |
|         5 | 3921 | `		break;` |
|         - | 3922 | `									   }` |
|         5 | 3923 | `	case PH7_VM_CONFIG_NATIVE_DEPTH:{` |
|         - | 3924 | `		/* Native VmByteCodeExec nesting cap: the C-stack guard for the re-entry` |
|         - | 3925 | `		 * classes the trampoline does not flatten (eval/include towers, nested` |
|         - | 3926 | `		 * coroutine resume, self-recursive C->PHP callbacks). Sized to the` |
|         - | 3927 | `		 * platform stack; the default (256 host / 16 small-stack embedders) is` |
|         - | 3928 | `		 * set in VmInit. A value > 1 overrides it (1 would forbid any re-entry,` |
|         - | 3929 | `		 * so it is rejected as a footgun). */` |
|        11 | 3930 | `		int nDepth = va_arg(ap,int);` |
|        11 | 3931 | `		if( nDepth > 1 ){` |
|        11 | 3932 | `			pVm->nMaxNativeDepth = nDepth;` |
|         5 | 3933 | `		}` |
|        11 | 3934 | `		break;` |
|         - | 3935 | `									   }` |
|       ! 0 | 3936 | `	case PH7_VM_OUTPUT_LENGTH: {` |
|         - | 3937 | `		/* VM output length in bytes */` |
|       ! 0 | 3938 | `		sxu32 *pOut = va_arg(ap,sxu32 *);` |
|         - | 3939 | `#ifdef UNTRUST` |
|         - | 3940 | `		if( pOut == 0 ){` |
|         - | 3941 | `			rc = SXERR_CORRUPT;` |
|         - | 3942 | `			break;` |
|         - | 3943 | `		}` |
|         - | 3944 | `#endif` |
|       ! 0 | 3945 | `		*pOut = pVm->nOutputLen;` |
|       ! 0 | 3946 | `		break;` |
|         - | 3947 | `							   }` |
|         - | 3948 |  |
|     25692 | 3949 | `	case PH7_VM_CONFIG_CREATE_SUPER:` |
|         - | 3950 | `	case PH7_VM_CONFIG_CREATE_VAR: {` |
|         - | 3951 | `		/* Create a new superglobal/global variable */` |
|     51389 | 3952 | `		const char *zName = va_arg(ap,const char *);` |
|     51389 | 3953 | `		ph7_value *pValue = va_arg(ap,ph7_value *);` |
|         - | 3954 | `		SyHashEntry *pEntry;` |
|         - | 3955 | `		ph7_value *pObj;` |
|         - | 3956 | `		sxu32 nByte;` |
|         - | 3957 | `		sxu32 nIdx;` |
|         - | 3958 | `#ifdef UNTRUST` |
|         - | 3959 | `		if( SX_EMPTY_STR(zName) \|\| pValue == 0 ){` |
|         - | 3960 | `			rc = SXERR_CORRUPT;` |
|         - | 3961 | `			break;` |
|         - | 3962 | `		}` |
|         - | 3963 | `#endif` |
|     51389 | 3964 | `		nByte = SyStrlen(zName);` |
|     51389 | 3965 | `		if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3966 | `			/* Check if the superglobal is already installed */` |
|     46765 | 3967 | `			pEntry = SyHashGet(&pVm->hSuper,(const void *)zName,nByte);` |
|     23385 | 3968 | `		}else{` |
|         - | 3969 | `			/* Query the top active VM frame */` |
|      4629 | 3970 | `			pEntry = SyHashGet(&pVm->pFrame->hVar,(const void *)zName,nByte);` |
|         - | 3971 | `		}` |
|     51389 | 3972 | `		if( pEntry ){` |
|         - | 3973 | `			/* Variable already installed */` |
|       ! 0 | 3974 | `			nIdx = SX_PTR_TO_INT(pEntry->pUserData);` |
|         - | 3975 | `			/* Extract contents */` |
|       ! 0 | 3976 | `			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|       ! 0 | 3977 | `			if( pObj ){` |
|         - | 3978 | `				/* Overwrite old contents */` |
|       ! 0 | 3979 | `				PH7_MemObjStore(pValue,pObj);` |
|       ! 0 | 3980 | `			}` |
|       ! 0 | 3981 | `		}else{` |
|         - | 3982 | `			/* Install a new variable */` |
|     51389 | 3983 | `			pObj = PH7_ReserveMemObj(&(*pVm));` |
|     51389 | 3984 | `			if( pObj == 0 ){` |
|       ! 0 | 3985 | `				rc = SXERR_MEM;` |
|       ! 0 | 3986 | `				break;` |
|         - | 3987 | `			}` |
|     51389 | 3988 | `			nIdx = pObj->nIdx;` |
|         - | 3989 | `			/* Copy value */` |
|     51389 | 3990 | `			PH7_MemObjStore(pValue,pObj);` |
|     51389 | 3991 | `			if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|         - | 3992 | `				/* Install the superglobal */` |
|     46765 | 3993 | `				rc = SyHashInsert(&pVm->hSuper,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|     23385 | 3994 | `			}else{` |
|         - | 3995 | `				/* Install in the current frame */` |
|      4629 | 3996 | `				rc = SyHashInsert(&pVm->pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx));` |
|         - | 3997 | `			}` |
|     51389 | 3998 | `			if( rc == SXRET_OK ){` |
|         - | 3999 | `				SyHashEntry *pRef;` |
|     51389 | 4000 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER ){` |
|     46765 | 4001 | `					pRef = SyHashLastEntry(&pVm->hSuper);` |
|     23385 | 4002 | `				}else{` |
|      4629 | 4003 | `					pRef = SyHashLastEntry(&pVm->pFrame->hVar);` |
|         - | 4004 | `				}` |
|         - | 4005 | `				/* Install in the reference table */` |
|     51389 | 4006 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,pRef,0,0);` |
|     51389 | 4007 | `				if( nOp == PH7_VM_CONFIG_CREATE_SUPER \|\| pVm->pFrame->pParent == 0){` |
|         - | 4008 | `					/* Register in the $GLOBALS array */` |
|     51389 | 4009 | `					VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|     25692 | 4010 | `				}` |
|     25692 | 4011 | `			}` |
|         - | 4012 | `		}` |
|     51389 | 4013 | `		break;` |
|         - | 4014 | `									}` |
|     14266 | 4015 | `	case PH7_VM_CONFIG_SERVER_ATTR:` |
|         - | 4016 | `	case PH7_VM_CONFIG_ENV_ATTR:` |
|         - | 4017 | `	case PH7_VM_CONFIG_SESSION_ATTR:` |
|         - | 4018 | `	case PH7_VM_CONFIG_POST_ATTR:` |
|         - | 4019 | `	case PH7_VM_CONFIG_GET_ATTR:` |
|         - | 4020 | `	case PH7_VM_CONFIG_COOKIE_ATTR:` |
|         - | 4021 | `	case PH7_VM_CONFIG_HEADER_ATTR: {` |
|     28537 | 4022 | `		const char *zKey   = va_arg(ap,const char *);` |
|     28537 | 4023 | `		const char *zValue = va_arg(ap,const char *);` |
|     28537 | 4024 | `		int nLen = va_arg(ap,int);` |
|         - | 4025 | `		ph7_hashmap *pMap;` |
|         - | 4026 | `		ph7_value *pValue;` |
|     28537 | 4027 | `		if( nOp == PH7_VM_CONFIG_ENV_ATTR ){` |
|         - | 4028 | `			/* Extract the $_ENV superglobal */` |
|       ! 0 | 4029 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_ENV",sizeof("_ENV")-1);` |
|     28537 | 4030 | `		}else if(nOp == PH7_VM_CONFIG_POST_ATTR ){` |
|         - | 4031 | `			/* Extract the $_POST superglobal */` |
|       ! 0 | 4032 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_POST",sizeof("_POST")-1);` |
|     28537 | 4033 | `		}else if(nOp == PH7_VM_CONFIG_GET_ATTR ){` |
|         - | 4034 | `			/* Extract the $_GET superglobal */` |
|       ! 0 | 4035 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_GET",sizeof("_GET")-1);` |
|     28537 | 4036 | `		}else if(nOp == PH7_VM_CONFIG_COOKIE_ATTR ){` |
|         - | 4037 | `			/* Extract the $_COOKIE superglobal */` |
|       ! 0 | 4038 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_COOKIE",sizeof("_COOKIE")-1);` |
|     28537 | 4039 | `		}else if(nOp == PH7_VM_CONFIG_SESSION_ATTR ){` |
|         - | 4040 | `			/* Extract the $_SESSION superglobal */` |
|       ! 0 | 4041 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SESSION",sizeof("_SESSION")-1);` |
|     28537 | 4042 | `		}else if( nOp == PH7_VM_CONFIG_HEADER_ATTR ){` |
|         - | 4043 | `			/* Extract the $_HEADER superglobale */` |
|       ! 0 | 4044 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_HEADER",sizeof("_HEADER")-1);` |
|       ! 0 | 4045 | `		}else{` |
|         - | 4046 | `			/* Extract the $_SERVER superglobal */` |
|     28537 | 4047 | `			pValue = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|         - | 4048 | `		}` |
|     28537 | 4049 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 4050 | `			/* No such entry */` |
|       ! 0 | 4051 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4052 | `			break;` |
|         - | 4053 | `		}` |
|         - | 4054 | `		/* Point to the hashmap */` |
|     28537 | 4055 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 4056 | `		/* Perform the insertion */` |
|     28537 | 4057 | `		rc = PH7_VmHashmapInsert(pMap,zKey,-1,zValue,nLen);` |
|     28537 | 4058 | `		break;` |
|         - | 4059 | `								   }` |
|      2350 | 4060 | `	case PH7_VM_CONFIG_ARGV_ENTRY:{` |
|         - | 4061 | `		/* Script arguments */` |
|      4705 | 4062 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4063 | `		ph7_hashmap *pMap;` |
|         - | 4064 | `		ph7_value *pValue;` |
|         - | 4065 | `		sxu32 n;` |
|         - | 4066 | ``		/* An EMPTY argument is a real argv element — `phl s.php "" x` gives php`` |
|         - | 4067 | `		 * $argv[1] === "" and $argc 3. This used to reject it (SX_EMPTY_STR is` |
|         - | 4068 | `		 * true for "" as well as NULL), silently renumbering every later element` |
|         - | 4069 | `		 * and shortening $argc. Only a NULL is refused now. */` |
|      4705 | 4070 | `		if( zValue == 0 ){` |
|       ! 0 | 4071 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4072 | `			break;` |
|         - | 4073 | `		}` |
|         - | 4074 | `		/* Extract the $argv array */` |
|      4705 | 4075 | `		pValue = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4705 | 4076 | `		if( pValue == 0 \|\| (pValue->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|         - | 4077 | `			/* No such entry */` |
|       ! 0 | 4078 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4079 | `			break;` |
|         - | 4080 | `		}` |
|         - | 4081 | `		/* Point to the hashmap */` |
|      4705 | 4082 | `		pMap = (ph7_hashmap *)pValue->x.pOther;` |
|         - | 4083 | `		/* Perform the insertion */` |
|      4705 | 4084 | `		n = (sxu32)SyStrlen(zValue);` |
|      4705 | 4085 | `		rc = PH7_VmHashmapInsert(pMap,0,0,zValue,(int)n);` |
|      4705 | 4086 | `		break;` |
|         - | 4087 | `								  }` |
|      2312 | 4088 | `	case PH7_VM_CONFIG_SERVER_ARGV: {` |
|         - | 4089 | `		/* php CLI exposes the script arguments in $_SERVER['argv'] and their` |
|         - | 4090 | `		 * count in $_SERVER['argc'], in addition to the top-level $argv/$argc.` |
|         - | 4091 | `		 * Mirror the already-populated $argv array into $_SERVER once, after` |
|         - | 4092 | `		 * all PH7_VM_CONFIG_ARGV_ENTRY calls are done. */` |
|         - | 4093 | `		ph7_value *pArgv,*pServer;` |
|         - | 4094 | `		ph7_hashmap *pServerMap,*pArgvMap,*pDup;` |
|         - | 4095 | `		ph7_value sArgvVal,sKey,sCount;` |
|      4629 | 4096 | `		pArgv   = PH7_VmExtractSuper(&(*pVm),"argv",sizeof("argv")-1);` |
|      4629 | 4097 | `		pServer = PH7_VmExtractSuper(&(*pVm),"_SERVER",sizeof("_SERVER")-1);` |
|      4624 | 4098 | `		if( pArgv == 0 \|\| (pArgv->iFlags & MEMOBJ_HASHMAP) == 0` |
|      4629 | 4099 | `		 \|\| pServer == 0 \|\| (pServer->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       ! 0 | 4100 | `			rc = SXERR_NOTFOUND;` |
|       ! 0 | 4101 | `			break;` |
|         - | 4102 | `		}` |
|      4629 | 4103 | `		pArgvMap   = (ph7_hashmap *)pArgv->x.pOther;` |
|      4629 | 4104 | `		pServerMap = (ph7_hashmap *)pServer->x.pOther;` |
|         - | 4105 | `		/* Deep-copy $argv so $_SERVER['argv'] is an independent array. */` |
|      4629 | 4106 | `		pDup = PH7_NewHashmap(&(*pVm),0,0);` |
|      4629 | 4107 | `		if( pDup == 0 ){` |
|       ! 0 | 4108 | `			rc = SXERR_MEM;` |
|       ! 0 | 4109 | `			break;` |
|         - | 4110 | `		}` |
|      4629 | 4111 | `		PH7_HashmapDup(pArgvMap,pDup);` |
|      4629 | 4112 | `		PH7_MemObjInitFromArray(&(*pVm),&sArgvVal,pDup);` |
|      4629 | 4113 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4629 | 4114 | `		PH7_MemObjStringAppend(&sKey,"argv",sizeof("argv")-1);` |
|      4629 | 4115 | `		PH7_HashmapInsert(pServerMap,&sKey,&sArgvVal);` |
|      4629 | 4116 | `		PH7_MemObjRelease(&sArgvVal); /* drops the duplicated array */` |
|      4629 | 4117 | `		PH7_MemObjRelease(&sKey);` |
|         - | 4118 | `		/* $_SERVER['argc'] = count($argv). */` |
|      4629 | 4119 | `		PH7_MemObjInitFromInt(&(*pVm),&sCount,(sxi64)pArgvMap->nEntry);` |
|      4629 | 4120 | `		PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|      4629 | 4121 | `		PH7_MemObjStringAppend(&sKey,"argc",sizeof("argc")-1);` |
|      4629 | 4122 | `		PH7_HashmapInsert(pServerMap,&sKey,&sCount);` |
|      4629 | 4123 | `		PH7_MemObjRelease(&sCount);` |
|      4629 | 4124 | `		PH7_MemObjRelease(&sKey);` |
|      4629 | 4125 | `		rc = SXRET_OK;` |
|      4629 | 4126 | `		break;` |
|         - | 4127 | `								  }` |
|        53 | 4128 | `	case PH7_VM_CONFIG_INI_ENTRY: {` |
|         - | 4129 | `		/* A php.ini directive from the CLI (-d name=value or a -c file line).` |
|         - | 4130 | `		 * Copies are queued for the INI chunk's lazy seed; engine-level knobs` |
|         - | 4131 | `		 * apply immediately so they take effect even if the script never` |
|         - | 4132 | `		 * touches the INI API. */` |
|       110 | 4133 | `		const char *zName = va_arg(ap,const char *);` |
|       110 | 4134 | `		const char *zValue = va_arg(ap,const char *);` |
|         - | 4135 | `		VmIniEntry sEntry;` |
|         - | 4136 | `		char *zDupN,*zDupV;` |
|         - | 4137 | `		sxu32 nName,nValue;` |
|       110 | 4138 | `		if( SX_EMPTY_STR(zName) ){` |
|       ! 0 | 4139 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4140 | `			break;` |
|         - | 4141 | `		}` |
|       110 | 4142 | `		if( zValue == 0 ){` |
|       ! 0 | 4143 | `			zValue = "";` |
|       ! 0 | 4144 | `		}` |
|       110 | 4145 | `		nName = (sxu32)SyStrlen(zName);` |
|       110 | 4146 | `		nValue = (sxu32)SyStrlen(zValue);` |
|       110 | 4147 | `		zDupN = SyMemBackendStrDup(&pVm->sAllocator,zName,nName);` |
|       110 | 4148 | `		zDupV = SyMemBackendStrDup(&pVm->sAllocator,zValue,nValue);` |
|       110 | 4149 | `		if( zDupN == 0 \|\| zDupV == 0 ){` |
|       ! 0 | 4150 | `			rc = SXERR_MEM;` |
|       ! 0 | 4151 | `			break;` |
|         - | 4152 | `		}` |
|       110 | 4153 | `		SyStringInitFromBuf(&sEntry.sName,zDupN,nName);` |
|       110 | 4154 | `		SyStringInitFromBuf(&sEntry.sValue,zDupV,nValue);` |
|       110 | 4155 | `		rc = SySetPut(&pVm->aIniCli,(const void *)&sEntry);` |
|       110 | 4156 | `		if( rc == SXRET_OK ){` |
|       106 | 4157 | `			if( nName == sizeof("error_reporting")-1` |
|        78 | 4158 | `			 && SyMemcmp(zName,"error_reporting",nName) == 0 ){` |
|         4 | 4159 | `				sxi64 iLevel = 0;` |
|         4 | 4160 | `				SyStrToInt64(zValue,nValue,(void *)&iLevel,0);` |
|         4 | 4161 | `				pVm->bErrReport = iLevel != 0;` |
|       104 | 4162 | `			}else if( nName == sizeof("date.timezone")-1` |
|        51 | 4163 | `			 && SyMemcmp(zName,"date.timezone",nName) == 0` |
|       ! 0 | 4164 | `			 && nValue == 3` |
|         4 | 4165 | `			 && (SyStrnicmp(zValue,"UTC",3) == 0 \|\| SyStrnicmp(zValue,"GMT",3) == 0) ){` |
|       ! 0 | 4166 | `				SyMemcpy(zValue,pVm->zDefTz,3);` |
|       ! 0 | 4167 | `				pVm->zDefTz[3] = 0;` |
|       ! 0 | 4168 | `				pVm->nDefTz = 3;` |
|       102 | 4169 | `			}else if( nName == sizeof("zend.assertions")-1` |
|        92 | 4170 | `			 && SyMemcmp(zName,"zend.assertions",nName) == 0 ){` |
|         - | 4171 | `				/* zend.assertions is a compile-time switch: 1 makes assert()` |
|         - | 4172 | `				 * active, 0 or -1 makes it a no-op. Applied here so it takes` |
|         - | 4173 | `				 * effect even before the INI chunk is seeded. */` |
|        40 | 4174 | `				sxi64 iZend = 0;` |
|        40 | 4175 | `				SyStrToInt64(zValue,nValue,(void *)&iZend,0);` |
|        40 | 4176 | `				if( iZend >= 1 ){` |
|        40 | 4177 | `					pVm->iAssertFlags &= ~PH7_ASSERT_ZEND_OFF;` |
|        22 | 4178 | `				}else{` |
|       ! 0 | 4179 | `					pVm->iAssertFlags \|= PH7_ASSERT_ZEND_OFF;` |
|         - | 4180 | `				}` |
|        88 | 4181 | `			}else if( nName == sizeof("display_errors")-1` |
|        46 | 4182 | `			 && SyMemcmp(zName,"display_errors",nName) == 0 ){` |
|         - | 4183 | `				/* Mirror the display_errors gate C-side so it takes effect even` |
|         - | 4184 | `				 * if the script never touches the INI API (ini_set keeps it in` |
|         - | 4185 | `				 * sync at runtime via __ini_apply_err). */` |
|        22 | 4186 | `				pVm->bDisplayErrors = VmIniBool(zValue,nValue);` |
|        61 | 4187 | `			}else if( nName == sizeof("log_errors")-1` |
|        36 | 4188 | `			 && SyMemcmp(zName,"log_errors",nName) == 0 ){` |
|        20 | 4189 | `				pVm->bLogErrors = VmIniBool(zValue,nValue);` |
|        44 | 4190 | `			}else if( nName == sizeof("include_path")-1` |
|        26 | 4191 | `			 && SyMemcmp(zName,"include_path",nName) == 0` |
|        18 | 4192 | `			 && nValue > 0 ){` |
|         - | 4193 | `				/* The path SET is the store this directive names, and the INI` |
|         - | 4194 | ``				 * chunk's seed is lazy -- so `-d include_path=…` has to reach it`` |
|         - | 4195 | `				 * here or a script that never touches the INI API keeps looking` |
|         - | 4196 | `				 * in the default directory. Empty is refused, as php's` |
|         - | 4197 | `				 * OnUpdateStringUnempty refuses it. */` |
|        10 | 4198 | `				PH7_VmSetIncludePath(pVm,zValue,nValue);` |
|         4 | 4199 | `			}` |
|        53 | 4200 | `		}` |
|       110 | 4201 | `		break;` |
|         - | 4202 | `								  }` |
|       ! 0 | 4203 | `	case PH7_VM_CONFIG_ERR_LOG_HANDLER: {` |
|         - | 4204 | `		/* error_log() consumer */` |
|       ! 0 | 4205 | `		ProcErrLog xErrLog = va_arg(ap,ProcErrLog);` |
|       ! 0 | 4206 | `		pVm->xErrLog = xErrLog;` |
|       ! 0 | 4207 | `		break;` |
|         - | 4208 | `										}` |
|       ! 0 | 4209 | `	case PH7_VM_CONFIG_EXEC_VALUE: {` |
|         - | 4210 | `		/* Script return value */` |
|       ! 0 | 4211 | `		ph7_value **ppValue = va_arg(ap,ph7_value **);` |
|         - | 4212 | `#ifdef UNTRUST` |
|         - | 4213 | `		if( ppValue == 0 ){` |
|         - | 4214 | `			rc = SXERR_CORRUPT;` |
|         - | 4215 | `			break;` |
|         - | 4216 | `		}` |
|         - | 4217 | `#endif` |
|       ! 0 | 4218 | `		*ppValue = &pVm->sExec;` |
|       ! 0 | 4219 | `		break;` |
|         - | 4220 | `								   }` |
|      9330 | 4221 | `	case PH7_VM_CONFIG_IO_STREAM: {` |
|         - | 4222 | `		/* Register an IO stream device */` |
|     18665 | 4223 | `		const ph7_io_stream *pStream = va_arg(ap,const ph7_io_stream *);` |
|         - | 4224 | `		/* Make sure we are dealing with a valid IO stream */` |
|     18660 | 4225 | `		if( pStream == 0 \|\| pStream->zName == 0 \|\| pStream->zName[0] == 0 \|\|` |
|     18665 | 4226 | `			pStream->xOpen == 0 \|\| pStream->xRead == 0 ){` |
|         - | 4227 | `				/* Invalid stream */` |
|       ! 0 | 4228 | `				rc = SXERR_INVALID;` |
|       ! 0 | 4229 | `				break;` |
|         - | 4230 | `		}` |
|     18665 | 4231 | `		if( pVm->pDefStream == 0 && SyStrnicmp(pStream->zName,"file",sizeof("file")-1) == 0 ){` |
|         - | 4232 | `			/* Make the 'file://' stream the defaut stream device */` |
|      4665 | 4233 | `			pVm->pDefStream = pStream;` |
|      2330 | 4234 | `		}` |
|         - | 4235 | `		/* Insert in the appropriate container */` |
|     18665 | 4236 | `		rc = SySetPut(&pVm->aIOstream,(const void *)&pStream);` |
|     18665 | 4237 | `		break;` |
|         - | 4238 | `								  }` |
|        23 | 4239 | `	case PH7_VM_CONFIG_EXTRACT_OUTPUT: {` |
|         - | 4240 | `		/* Point to the VM internal output consumer buffer */` |
|        46 | 4241 | `		const void **ppOut = va_arg(ap,const void **);` |
|        46 | 4242 | `		unsigned int *pLen = va_arg(ap,unsigned int *);` |
|         - | 4243 | `#ifdef UNTRUST` |
|         - | 4244 | `		if( ppOut == 0 \|\| pLen == 0 ){` |
|         - | 4245 | `			rc = SXERR_CORRUPT;` |
|         - | 4246 | `			break;` |
|         - | 4247 | `		}` |
|         - | 4248 | `#endif` |
|        46 | 4249 | `		*ppOut = SyBlobData(&pVm->sConsumer);` |
|        46 | 4250 | `		*pLen  = SyBlobLength(&pVm->sConsumer);` |
|        46 | 4251 | `		break;` |
|         - | 4252 | `									   }` |
|        23 | 4253 | `	case PH7_VM_CONFIG_HTTP_REQUEST:{` |
|         - | 4254 | `		/* Raw HTTP request*/` |
|        46 | 4255 | `		const char *zRequest = va_arg(ap,const char *);` |
|        46 | 4256 | `		int nByte = va_arg(ap,int);` |
|        46 | 4257 | `		if( SX_EMPTY_STR(zRequest) ){` |
|       ! 0 | 4258 | `			rc = SXERR_EMPTY;` |
|       ! 0 | 4259 | `			break;` |
|         - | 4260 | `		}` |
|        46 | 4261 | `		if( nByte < 0 ){` |
|         - | 4262 | `			/* Compute length automatically */` |
|       ! 0 | 4263 | `			nByte = (int)SyStrlen(zRequest);` |
|       ! 0 | 4264 | `		}` |
|         - | 4265 | `		/* Process the request */` |
|        46 | 4266 | `		rc = PH7_VmHttpProcessRequest(&(*pVm),zRequest,nByte);` |
|         - | 4267 | `		/* Mark this VM as operating in HTTP context only on success */` |
|        46 | 4268 | `		if( rc == SXRET_OK ){` |
|        44 | 4269 | `			pVm->bHttpContext = 1;` |
|        22 | 4270 | `		}` |
|        46 | 4271 | `		break;` |
|         - | 4272 | `									}` |
|        23 | 4273 | `	case PH7_VM_CONFIG_RESPONSE_STATUS: {` |
|         - | 4274 | `		/* Extract HTTP response status code */` |
|        46 | 4275 | `		int *pStatus = va_arg(ap, int *);` |
|        46 | 4276 | `		if( pStatus ){` |
|        46 | 4277 | `			*pStatus = pVm->iResponseStatus;` |
|        23 | 4278 | `		}` |
|        46 | 4279 | `		break;` |
|         - | 4280 | `										}` |
|        23 | 4281 | `	case PH7_VM_CONFIG_RESPONSE_HEADERS: {` |
|         - | 4282 | `		/* Iterate response headers via callback */` |
|         - | 4283 | `		typedef int (*ProcHeaderConsumer)(const char *,unsigned int,const char *,unsigned int,void *);` |
|        46 | 4284 | `		ProcHeaderConsumer xCallback = va_arg(ap, ProcHeaderConsumer);` |
|        46 | 4285 | `		void *pUserData = va_arg(ap, void *);` |
|        46 | 4286 | `		if( xCallback ){` |
|        46 | 4287 | `			VmResponseHeader *aHdr = (VmResponseHeader *)SySetBasePtr(&pVm->aResponseHeaders);` |
|        46 | 4288 | `			sxu32 k, nHdr = SySetUsed(&pVm->aResponseHeaders);` |
|       112 | 4289 | `			for( k = 0; k < nHdr; k++ ){` |
|        99 | 4290 | `				rc = xCallback(aHdr[k].sName.zString, aHdr[k].sName.nByte,` |
|        66 | 4291 | `							   aHdr[k].sValue.zString, aHdr[k].sValue.nByte,` |
|        33 | 4292 | `							   pUserData);` |
|        66 | 4293 | `				if( rc != PH7_OK ){` |
|       ! 0 | 4294 | `					break;` |
|         - | 4295 | `				}` |
|        33 | 4296 | `			}` |
|        23 | 4297 | `		}` |
|        46 | 4298 | `		break;` |
|         - | 4299 | `										 }` |
|       ! 0 | 4300 | `	default:` |
|         - | 4301 | `		/* Unknown configuration option */` |
|       ! 0 | 4302 | `		rc = SXERR_UNKNOWN;` |
|       ! 0 | 4303 | `		break;` |
|         - | 4304 | `	}` |
|    126783 | 4305 | `	return rc;` |
|         5 | 4306 | `}` |
|         - | 4307 | `/* Forward declaration */` |
|         - | 4308 | `static const char * VmInstrToString(sxi32 nOp);` |
|         - | 4309 | `/*` |
|         - | 4310 | ` * This routine is used to dump PH7 byte-code instructions to a human readable` |
|         - | 4311 | ` * format.` |
|         - | 4312 | ` * The dump is redirected to the given consumer callback which is responsible` |
|         - | 4313 | ` * of consuming the generated dump perhaps redirecting it to its standard output` |
|         - | 4314 | ` * (STDOUT).` |
|         - | 4315 | ` */` |
|         2 | 4316 | `static sxi32 VmByteCodeDump(` |
|         - | 4317 | `	SySet *pByteCode,       /* Bytecode container */` |
|         - | 4318 | `	ProcConsumer xConsumer, /* Dump consumer callback */` |
|         - | 4319 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 4320 | `	)` |
|         1 | 4321 | `{` |
|         - | 4322 | `	static const char zDump[] = {` |
|         - | 4323 | `		"====================================================\n"` |
|         - | 4324 | `		"PH7 VM Dump\n"` |
|         - | 4325 | `		"====================================================\n"` |
|         - | 4326 | `	};` |
|         - | 4327 | `	VmInstr *pInstr,*pEnd;` |
|         3 | 4328 | `	sxi32 rc = SXRET_OK;` |
|         - | 4329 | `	sxu32 n;` |
|         - | 4330 | `	/* Point to the PH7 instructions */` |
|         3 | 4331 | `	pInstr = (VmInstr *)SySetBasePtr(pByteCode);` |
|         3 | 4332 | `	pEnd   = &pInstr[SySetUsed(pByteCode)];` |
|         3 | 4333 | `	n = 0;` |
|         3 | 4334 | `	xConsumer((const void *)zDump,sizeof(zDump)-1,pUserData);` |
|         - | 4335 | `	/* Dump instructions */` |
|         6 | 4336 | `	for(;;){` |
|        13 | 4337 | `		if( pInstr >= pEnd ){` |
|         - | 4338 | `			/* No more instructions */` |
|         3 | 4339 | `			break;` |
|         - | 4340 | `		}` |
|         - | 4341 | `		/* Format and call the consumer callback */` |
|        16 | 4342 | `		rc = SyProcFormat(xConsumer,pUserData,"%s %8d %8u %#8x [%u] L%u\n",` |
|        10 | 4343 | `			VmInstrToString(pInstr->iOp),pInstr->iP1,pInstr->iP2,` |
|        10 | 4344 | `			SX_PTR_TO_INT(pInstr->p3),n,pInstr->nLine);` |
|        11 | 4345 | `		if( rc != SXRET_OK ){` |
|         - | 4346 | `			/* Consumer routine request an operation abort */` |
|       ! 0 | 4347 | `			return rc;` |
|         - | 4348 | `		}` |
|        11 | 4349 | `		++n;` |
|        11 | 4350 | `		pInstr++; /* Next instruction in the stream */` |
|         1 | 4351 | `	}` |
|         3 | 4352 | `	return rc;` |
|         2 | 4353 | `}` |
|         - | 4354 | `/*` |
|         - | 4355 | ` * Save the execution state of a fiber/generator context.` |
|         - | 4356 | ` * This may be called multiple times as PH7_SUSPEND propagates up through` |
|         - | 4357 | ` * nested VmByteCodeExec calls. Each level overwrites pc/nTos with its own` |
|         - | 4358 | ` * values, so the last (outermost) call wins — which is the fiber's own level.` |
|         - | 4359 | ` * Frame detachment is NOT done here; it's handled by VmStartCtx/VmResumeCtx` |
|         - | 4360 | ` * when VmByteCodeExec returns.` |
|         - | 4361 | ` */` |
|      1684 | 4362 | `PH7_PRIVATE sxi32 VmSuspendCtx(` |
|         - | 4363 | `	ph7_vm *pVm,` |
|         - | 4364 | `	ph7_exec_ctx *pCtx,` |
|         - | 4365 | `	sxi32 pc,` |
|         - | 4366 | `	sxi32 nTos` |
|         - | 4367 | `	)` |
|         5 | 4368 | `{` |
|       842 | 4369 | `	(void)pVm; /* unused — frame detach moved to VmStartCtx/VmResumeCtx */` |
|      1689 | 4370 | `	pCtx->pc = pc;` |
|      1689 | 4371 | `	pCtx->nTos = nTos;` |
|      1689 | 4372 | `	pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      1689 | 4373 | `	return PH7_SUSPEND;` |
|         5 | 4374 | `}` |
|         - | 4375 | `/*` |
|         - | 4376 | ` * Resolve named-argument mapping.` |
|         - | 4377 | ` *` |
|         - | 4378 | ` * For each actual argument in the call, determine which formal parameter it` |
|         - | 4379 | ` * maps to (by name or by position).  On success, aSlot[i] contains the` |
|         - | 4380 | ` * formal-parameter index for actual arg i, -1 if it overflows into the` |
|         - | 4381 | ` * variadic collector, or -2 if still unresolved.  aUsed[k] is set to 1 for` |
|         - | 4382 | ` * every formal parameter that received a value.` |
|         - | 4383 | ` *` |
|         - | 4384 | ` * Returns SXRET_OK on success.  On error (duplicate, unknown parameter,` |
|         - | 4385 | ` * positional-overlaps-named) it raises php's CATCHABLE \Error via` |
|         - | 4386 | ` * VmThrowNamedArgError and returns that status: PH7_EXCEPTION (route it to the` |
|         - | 4387 | ` * enclosing try, as the OP_CALL arg-binding throws do) or PH7_ABORT.` |
|         - | 4388 | ` */` |
|       392 | 4389 | `PH7_PRIVATE sxi32 VmResolveNamedArgs(` |
|         - | 4390 | `	ph7_vm *pVm,` |
|         - | 4391 | `	VmCallArgMap *pMap,           /* Named-arg metadata from the instruction */` |
|         - | 4392 | `	ph7_vm_func_arg *aFormalArg,  /* Formal parameter array */` |
|         - | 4393 | `	sxu32 nNonVariadic,           /* Number of non-variadic formal params */` |
|         - | 4394 | `	sxi32 iVariadicIdx,           /* Index of the variadic param, or -1 */` |
|         - | 4395 | `	sxu32 nActual,                /* Number of actual arguments on the stack */` |
|         - | 4396 | `	sxi32 *aSlot,                 /* OUT: mapping actual->formal */` |
|         - | 4397 | `	sxu8  *aUsed                  /* OUT: which formals are used */` |
|         - | 4398 | `)` |
|         5 | 4399 | `{` |
|       397 | 4400 | `	sxi32 posIdx = 0;` |
|         - | 4401 | `	sxu32 i;` |
|       397 | 4402 | `	int bSeenNamed = 0;` |
|         - | 4403 | `	char zErrMsg[256];` |
|       397 | 4404 | `	SyZero(aUsed, nNonVariadic * sizeof(sxu8));` |
|      1355 | 4405 | `	for( i = 0; i < nActual; i++ ){` |
|       963 | 4406 | `		aSlot[i] = -2;` |
|       484 | 4407 | `	}` |
|      1343 | 4408 | `	for( i = 0; i < nActual; i++ ){` |
|      1254 | 4409 | `		if( i < pMap->nTotal && pMap->aNames[i].nByte > 0 ){` |
|         - | 4410 | `			/* Named argument — find formal by name */` |
|       599 | 4411 | `			int found = 0;` |
|       599 | 4412 | `			bSeenNamed = 1;` |
|         - | 4413 | `			sxu32 k;` |
|       903 | 4414 | `			for( k = 0; k < nNonVariadic; k++ ){` |
|       734 | 4415 | `				if( aFormalArg[k].sName.nByte == pMap->aNames[i].nByte` |
|       714 | 4416 | `					&& SyMemcmp(aFormalArg[k].sName.zString,` |
|       684 | 4417 | `						pMap->aNames[i].zString,` |
|      1026 | 4418 | `						pMap->aNames[i].nByte) == 0 ){` |
|       435 | 4419 | `					if( aUsed[k] ){` |
|        12 | 4420 | `						SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4421 | `							"Named parameter $%.*s overwrites previous argument",` |
|         6 | 4422 | `							(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         9 | 4423 | `						return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4424 | `					}` |
|       428 | 4425 | `					aSlot[i] = (sxi32)k;` |
|       428 | 4426 | `					aUsed[k] = 1;` |
|       428 | 4427 | `					found = 1;` |
|       428 | 4428 | `					break;` |
|         - | 4429 | `				}` |
|       157 | 4430 | `			}` |
|       593 | 4431 | `			if( !found ){` |
|       168 | 4432 | `				if( iVariadicIdx >= 0 ){` |
|       162 | 4433 | `					aSlot[i] = -1; /* goes to variadic with string key */` |
|        83 | 4434 | `				}else{` |
|        11 | 4435 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4436 | `						"Unknown named parameter $%.*s",` |
|         6 | 4437 | `						(int)pMap->aNames[i].nByte,pMap->aNames[i].zString);` |
|         8 | 4438 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4439 | `				}` |
|        79 | 4440 | `			}` |
|       296 | 4441 | `		}else{` |
|         - | 4442 | `			/* Positional argument. Source-syntax calls can't reach here after a` |
|         - | 4443 | `			 * named arg (the parser rejects it at compile time), but a call` |
|         - | 4444 | `			 * reconstructed from an array — call_user_func_array(['b'=>9, 'x']) —` |
|         - | 4445 | `			 * can, so enforce PHP's rule at this shared choke point. */` |
|       368 | 4446 | `			if( bSeenNamed ){` |
|       ! 0 | 4447 | `				return VmThrowNamedArgError(&(*pVm),` |
|         - | 4448 | `					"Cannot use positional argument after named argument",` |
|         - | 4449 | `					sizeof("Cannot use positional argument after named argument") - 1);` |
|         - | 4450 | `			}` |
|       368 | 4451 | `			if( (sxu32)posIdx < nNonVariadic ){` |
|        62 | 4452 | `				if( aUsed[posIdx] ){` |
|       ! 0 | 4453 | `					SyBufferFormat(zErrMsg,sizeof(zErrMsg),` |
|         - | 4454 | `						"Named parameter $%.*s overwrites previous argument",` |
|       ! 0 | 4455 | `						(int)aFormalArg[posIdx].sName.nByte,aFormalArg[posIdx].sName.zString);` |
|       ! 0 | 4456 | `					return VmThrowNamedArgError(&(*pVm),zErrMsg,(sxu32)SyStrlen(zErrMsg));` |
|         - | 4457 | `				}` |
|        62 | 4458 | `				aSlot[i] = posIdx;` |
|        62 | 4459 | `				aUsed[posIdx] = 1;` |
|       338 | 4460 | `			}else if( iVariadicIdx >= 0 ){` |
|       309 | 4461 | `				aSlot[i] = -1; /* overflow to variadic */` |
|       153 | 4462 | `			}` |
|       368 | 4463 | `			posIdx++;` |
|         - | 4464 | `		}` |
|       478 | 4465 | `	}` |
|       385 | 4466 | `	return SXRET_OK;` |
|       201 | 4467 | `}` |
|         - | 4468 | `/*` |
|         - | 4469 | ` * Is this value an object implementing Traversable (Iterator / IteratorAggregate` |
|         - | 4470 | ` * / Generator)? Used by the spread sites to decide whether to unpack it.` |
|         - | 4471 | ` */` |
|       634 | 4472 | `PH7_PRIVATE int VmValueIsTraversable(ph7_vm *pVm, ph7_value *pVal)` |
|         4 | 4473 | `{` |
|       638 | 4474 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pTraversableClass == 0 ){` |
|       626 | 4475 | `		return 0;` |
|         - | 4476 | `	}` |
|        14 | 4477 | `	return PH7_VmInstanceOf(((ph7_class_instance *)pVal->x.pOther)->pClass, pVm->pTraversableClass);` |
|       321 | 4478 | `}` |
|         - | 4479 | `/*` |
|         - | 4480 | `` * PH7_VmIteratorWalk step for array-literal Traversable spread `[...$it]`:`` |
|         - | 4481 | ` * merge each element with PHP 8.1 array-unpack key rules — string keys are` |
|         - | 4482 | ` * preserved (later wins), integer keys are renumbered.` |
|         - | 4483 | ` */` |
|        10 | 4484 | `PH7_PRIVATE sxi32 VmSpreadMergeStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4485 | `{` |
|        11 | 4486 | `	ph7_hashmap *pMap = (ph7_hashmap *)pUserData;` |
|         5 | 4487 | `	(void)pVm;` |
|        11 | 4488 | `	PH7_HashmapInsert(pMap, (pKey->iFlags & MEMOBJ_STRING) ? pKey : 0 /* auto-index */, pValue);` |
|        11 | 4489 | `	return SXRET_OK;` |
|         1 | 4490 | `}` |
|         - | 4491 | `/*` |
|         - | 4492 | `` * PH7_VmIteratorWalk step for call-argument Traversable spread `f(...$it)`:`` |
|         - | 4493 | ` * collect values positionally (keys ignored) into a temp array.` |
|         - | 4494 | ` */` |
|         6 | 4495 | `PH7_PRIVATE sxi32 VmSpreadValuesStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|         1 | 4496 | `{` |
|         3 | 4497 | `	(void)pVm; (void)pKey;` |
|         7 | 4498 | `	PH7_HashmapInsert((ph7_hashmap *)pUserData, 0 /* auto-index */, pValue);` |
|         7 | 4499 | `	return SXRET_OK;` |
|         1 | 4500 | `}` |
|         - | 4501 | `/*` |
|         - | 4502 | ` * Shared OP_SPREAD expansion tail: replace the stack slot holding an` |
|         - | 4503 | ` * array-to-unpack with the map's elements in insertion order, capturing one` |
|         - | 4504 | ` * VmSpreadRun (CALL/NEW derive their own arg-count growth from it). Used by both` |
|         - | 4505 | ` * the plain-array path (*ppTos holds the map value) and the materialized-Traversable` |
|         - | 4506 | ` * path (*ppTos holds the iterator object; pMap is the temp).` |
|         - | 4507 | ` * The map is kept alive across the walk — the stack slot may hold its ONLY` |
|         - | 4508 | ` * reference (literal / call-result temp), and releasing it mid-walk restores` |
|         - | 4509 | ` * the nodes' value slots to the freelist (the old open-coded copy then read` |
|         - | 4510 | ` * the reset slots and pushed nulls: f(...[1,2]) bound null/null). A` |
|         - | 4511 | ` * sole-owner temp's elements are deep-copied (PH7_MemObjStore) so nested` |
|         - | 4512 | ` * containers and blobs survive the trailing unref; a shared map keeps the` |
|         - | 4513 | ` * historical zero-copy aliasing (PH7_MemObjLoad) for f(...$var).` |
|         - | 4514 | ` */` |
|         - | 4515 | `/*` |
|         - | 4516 | ` * Record one OP_SPREAD expansion for PHP 8.1 named-key replay: a run anchored at` |
|         - | 4517 | ` * the first stack slot written (pFirst) plus one key entry per element, walked in` |
|         - | 4518 | ` * insertion order (string key -> named, integer key -> positional). Best-effort —` |
|         - | 4519 | ` * on OOM the capture is skipped and the call falls back to positional binding` |
|         - | 4520 | ` * (pre-8.1 behavior). Must run while pMap's nodes are still alive (before unref).` |
|         - | 4521 | ` */` |
|       612 | 4522 | `static void VmSpreadCaptureRun(ph7_vm *pVm, ph7_value *pFirst, ph7_hashmap *pMap, sxu32 nCount)` |
|         4 | 4523 | `{` |
|         - | 4524 | `	VmSpreadRun sRun;` |
|         - | 4525 | `	ph7_hashmap_node *pNode;` |
|         - | 4526 | `	sxu32 i;` |
|       616 | 4527 | `	sRun.pStart = pFirst;` |
|       616 | 4528 | `	sRun.nCount = nCount;` |
|       616 | 4529 | `	sRun.nKeyStart = SySetUsed(&pVm->aSpreadKey);` |
|       616 | 4530 | `	sRun.nBlobStart = SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       616 | 4531 | `	if( SySetPut(&pVm->aSpreadRun, (const void *)&sRun) != SXRET_OK ){` |
|       ! 0 | 4532 | `		return;` |
|         - | 4533 | `	}` |
|       616 | 4534 | `	pNode = pMap->pFirst;` |
|      2870 | 4535 | `	for( i = 0; i < nCount && pNode; i++ ){` |
|         - | 4536 | `		VmSpreadKey sKey;` |
|      2258 | 4537 | `		if( pNode->iType == HASHMAP_BLOB_NODE && SyBlobLength(&pNode->xKey.sKey) > 0 ){` |
|         - | 4538 | `			/* String key -> named argument. Copy the bytes so the name survives` |
|         - | 4539 | `			 * the source map's release before CALL replays them. */` |
|       101 | 4540 | `			sKey.nOff = (sxu32)SyBlobLength(&pVm->sSpreadKeyBlob);` |
|       101 | 4541 | `			sKey.nLen = (sxu32)SyBlobLength(&pNode->xKey.sKey);` |
|       101 | 4542 | `			SyBlobAppend(&pVm->sSpreadKeyBlob, SyBlobData(&pNode->xKey.sKey), sKey.nLen);` |
|        51 | 4543 | `		}else{` |
|         - | 4544 | `			/* Integer key (or empty-string key, treated positionally) */` |
|      2158 | 4545 | `			sKey.nOff = 0;` |
|      2158 | 4546 | `			sKey.nLen = 0;` |
|         - | 4547 | `		}` |
|      2258 | 4548 | `		SySetPut(&pVm->aSpreadKey, (const void *)&sKey);` |
|      2258 | 4549 | `		pNode = pNode->pPrev; /* forward link */` |
|      1131 | 4550 | `	}` |
|       310 | 4551 | `}` |
|         - | 4552 | `/* Clear the spread-key capture buffers (runs/keys/blob). aEffArgName is left` |
|         - | 4553 | ` * intact — it is rebuilt (and its blob dependency retired) at the next build. */` |
|        16 | 4554 | `static void VmSpreadCaptureReset(ph7_vm *pVm)` |
|       ! 0 | 4555 | `{` |
|        16 | 4556 | `	SySetReset(&pVm->aSpreadRun);` |
|        16 | 4557 | `	SySetReset(&pVm->aSpreadKey);` |
|        16 | 4558 | `	SyBlobReset(&pVm->sSpreadKeyBlob);` |
|        16 | 4559 | `}` |
|         - | 4560 | `/* Truncate THIS call's captured spread runs — the suffix [nSpreadCallBase, end)` |
|         - | 4561 | ` * that VmSpreadOwnExtra assigned to the dispatching CALL/NEW — restoring the` |
|         - | 4562 | ` * buffers to the enclosing call's state. A no-op when this call owns no run.` |
|         - | 4563 | ` * Every spread-bearing CALL/NEW MUST invoke this (directly, or via` |
|         - | 4564 | ` * VmBuildEffectiveArgMap which ends with it) before returning — an unconsumed run` |
|         - | 4565 | ` * outlives the call in the VM-global buffers and corrupts a later call in the` |
|         - | 4566 | ` * same interpreter (e.g. across .phpt files sharing one VM). Indexing by the` |
|         - | 4567 | ` * pre-computed run base (not a pStart scan) is what keeps an enclosing empty` |
|         - | 4568 | `` * `...[]` run — which shares its zero-width anchor with a nested call's base`` |
|         - | 4569 | ` * slot — from being consumed by that nested call. */` |
|      1112 | 4570 | `PH7_PRIVATE void VmSpreadConsume(ph7_vm *pVm)` |
|         4 | 4571 | `{` |
|      1116 | 4572 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|      1116 | 4573 | `	sxu32 rStart = pVm->nSpreadCallBase;` |
|         - | 4574 | `	VmSpreadRun *aRun;` |
|      1116 | 4575 | `	if( rStart >= nRun ){` |
|       518 | 4576 | `		return; /* this call owns no run (none captured, or already consumed) */` |
|         - | 4577 | `	}` |
|       602 | 4578 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       602 | 4579 | `	SySetTruncate(&pVm->aSpreadKey, aRun[rStart].nKeyStart);` |
|       602 | 4580 | `	if( aRun[rStart].nBlobStart <= SyBlobLength(&pVm->sSpreadKeyBlob) ){` |
|       602 | 4581 | `		pVm->sSpreadKeyBlob.nByte = aRun[rStart].nBlobStart;` |
|       299 | 4582 | `	}` |
|       602 | 4583 | `	SySetTruncate(&pVm->aSpreadRun, rStart);` |
|       560 | 4584 | `}` |
|         - | 4585 | `/*` |
|         - | 4586 | ` * Net operand-slot growth contributed by THIS call/NEW's own argument unpacks —` |
|         - | 4587 | ` * the captured spread runs anchored in this call's argument region at the top of` |
|         - | 4588 | ` * the stack. iP1 is the compile-time argument count; pTos points one past the` |
|         - | 4589 | ` * last pushed argument (the callable slot for CALL, the class-name slot for NEW).` |
|         - | 4590 | ` *` |
|         - | 4591 | ` * Walks the compile-time argument positions right-to-left, matching each against` |
|         - | 4592 | ` * the captured runs top-down: a position whose slots end at the current top is a` |
|         - | 4593 | ` * non-empty unpack (nCount slots, +nCount-1 net); an empty run anchored at the` |
|         - | 4594 | `` * current boundary is a `...[]` (one compile position, zero slots, -1 net); any`` |
|         - | 4595 | ` * other position is a single ordinary slot. Stopping after iP1 positions leaves` |
|         - | 4596 | ` * an ENCLOSING call's runs (which sit BELOW this call's arguments) untouched, so` |
|         - | 4597 | ` * they are counted only by that call. This replaces the old shared` |
|         - | 4598 | ` * pVm->iSpreadExtra accumulator, which a spread-bearing call nested in another` |
|         - | 4599 | `` * call's argument list (`h(...$a, k: g(...$b))`) both over-read and then zeroed.`` |
|         - | 4600 | ` *` |
|         - | 4601 | ` * ALSO records pVm->nSpreadCallBase — the index of the first run this walk` |
|         - | 4602 | ` * assigned to the call — so VmBuildEffectiveArgMap / VmSpreadConsume partition by` |
|         - | 4603 | ` * that exact boundary rather than re-deriving it from pStart (which is ambiguous` |
|         - | 4604 | `` * for a zero-width `...[]` run that shares a nested call's base slot).`` |
|         - | 4605 | ` */` |
|      1232 | 4606 | `PH7_PRIVATE sxi32 VmSpreadOwnExtra(ph7_vm *pVm, sxi32 iP1, ph7_value *pTos)` |
|         4 | 4607 | `{` |
|      1236 | 4608 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4609 | `	VmSpreadRun *aRun;` |
|      1236 | 4610 | `	ph7_value *pEnd = pTos;` |
|      1236 | 4611 | `	sxi32 nPos = iP1;` |
|      1236 | 4612 | `	sxi32 ri, extra = 0;` |
|      1236 | 4613 | `	if( nRun == 0 ){` |
|        15 | 4614 | `		pVm->nSpreadCallBase = 0;` |
|        15 | 4615 | `		return 0;` |
|         - | 4616 | `	}` |
|      1222 | 4617 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|      1222 | 4618 | `	ri = (sxi32)nRun - 1;` |
|      3166 | 4619 | `	while( nPos > 0 ){` |
|      1948 | 4620 | `		if( ri >= 0 && aRun[ri].nCount > 0 && aRun[ri].pStart + aRun[ri].nCount == pEnd ){` |
|         - | 4621 | `			/* A non-empty unpack occupying nCount slots. */` |
|      1096 | 4622 | `			pEnd = aRun[ri].pStart;` |
|      1096 | 4623 | `			extra += (sxi32)aRun[ri].nCount - 1;` |
|      1096 | 4624 | `			ri--;` |
|      1402 | 4625 | `		}else if( ri >= 0 && aRun[ri].nCount == 0 && aRun[ri].pStart == pEnd ){` |
|         - | 4626 | ``			/* An empty unpack (`...[]`): one compile position, zero slots. */`` |
|       135 | 4627 | `			extra -= 1;` |
|       135 | 4628 | `			ri--;` |
|        69 | 4629 | `		}else{` |
|         - | 4630 | `			/* An ordinary single-slot argument. */` |
|       724 | 4631 | `			pEnd--;` |
|         - | 4632 | `		}` |
|      1948 | 4633 | `		nPos--;` |
|         4 | 4634 | `	}` |
|         - | 4635 | `	/* Runs (ri, nRun) were matched to this call; ri is the last one left for an` |
|         - | 4636 | `	 * enclosing call (or -1). This call's runs begin at ri+1. */` |
|      1222 | 4637 | `	pVm->nSpreadCallBase = (sxu32)(ri + 1);` |
|      1222 | 4638 | `	return extra;` |
|       620 | 4639 | `}` |
|       612 | 4640 | `PH7_PRIVATE void VmSpreadExpandMap(ph7_vm *pVm, ph7_value **ppTos, ph7_hashmap *pMap, int bVarSource)` |
|         4 | 4641 | `{` |
|       616 | 4642 | `	ph7_value *pTos = *ppTos;` |
|       616 | 4643 | `	sxu32 nEntry = pMap->nEntry;` |
|       616 | 4644 | `	if( nEntry == 0 ){` |
|         - | 4645 | `		/* Nothing to unpack — remove the source from the stack */` |
|        69 | 4646 | `		VmSpreadCaptureRun(pVm, pTos, pMap, 0); /* empty run: keeps compile-arg alignment */` |
|        69 | 4647 | `		VmPopOperand(&pTos, 1);` |
|        36 | 4648 | `	}else{` |
|         - | 4649 | `		ph7_hashmap_node *pNode;` |
|         - | 4650 | `		ph7_value *pElem;` |
|         - | 4651 | `		sxu32 i;` |
|         - | 4652 | `		int bTemp;` |
|         - | 4653 | `		/* An unpacked element can be BOUND by reference (see the nIdx assignment below),` |
|         - | 4654 | `		 * so a source array that other variables share has to separate first — otherwise` |
|         - | 4655 | ``		 * the callee's write-back lands in the shared map and `$k = $j; f(...$j);` with`` |
|         - | 4656 | ``		 * `function f(&$x)` changes `$k` too. Separating a SOLE owner is a no-op, so the`` |
|         - | 4657 | `		 * ordinary spread pays nothing for this. */` |
|       546 | 4658 | `		if( bVarSource` |
|       467 | 4659 | `		 && pMap != pVm->pGlobal` |
|       388 | 4660 | `		 && ((*ppTos)->iFlags & MEMOBJ_HASHMAP)` |
|       392 | 4661 | `		 && (ph7_hashmap *)(*ppTos)->x.pOther == pMap ){` |
|         - | 4662 | `			/* Only when the stack slot IS this array: the Traversable path hands us a` |
|         - | 4663 | `			 * temporary map materialized from an ITERATOR, and the slot still holds the` |
|         - | 4664 | `			 * object. */` |
|       392 | 4665 | `			ph7_hashmap *pSep = PH7_HashmapCowSeparate(pVm,*ppTos);` |
|       392 | 4666 | `			if( pSep ){` |
|       392 | 4667 | `				pMap = pSep;` |
|       392 | 4668 | `				nEntry = pMap->nEntry;` |
|       194 | 4669 | `			}` |
|       194 | 4670 | `		}` |
|       550 | 4671 | `		pMap->iRef++;` |
|       550 | 4672 | `		bTemp = (pMap->iRef == 2); /* the stack slot held the only reference */` |
|         - | 4673 | `		/* Record the run + element keys before any release (nodes still alive).` |
|         - | 4674 | `		 * pTos is the source slot, which becomes the first element's slot. */` |
|       550 | 4675 | `		VmSpreadCaptureRun(pVm, pTos, pMap, nEntry);` |
|         - | 4676 | `		/* Overwrite the source slot with the first element */` |
|       550 | 4677 | `		pNode = pMap->pFirst;` |
|       550 | 4678 | `		pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|       550 | 4679 | `		PH7_MemObjRelease(pTos);` |
|       550 | 4680 | `		if( pElem ){` |
|       550 | 4681 | `			if( bTemp ){` |
|       152 | 4682 | `				PH7_MemObjStore(pElem, pTos);` |
|        77 | 4683 | `			}else{` |
|       400 | 4684 | `				PH7_MemObjLoad(pElem, pTos);` |
|         - | 4685 | `			}` |
|       273 | 4686 | `		}` |
|         - | 4687 | `		/* php binds an UNPACKED argument by reference when the callee asks for one: the` |
|         - | 4688 | ``		 * element itself is the lvalue, and `f(...$a)` with `function f(&$x)` writes back`` |
|         - | 4689 | ``		 * into `$a[0]`. Carrying the element's memory-object index is what lets the`` |
|         - | 4690 | `		 * ordinary by-ref binder do that — without it every unpacked argument looked like` |
|         - | 4691 | ``		 * a literal and the whole call was refused. A TEMPORARY source array (`f(...[1])`)`` |
|         - | 4692 | `		 * is exempt: its elements die with it, so they stay unbound (php writes into a` |
|         - | 4693 | `		 * temporary nothing can observe). The source array outlives the call — it is` |
|         - | 4694 | `		 * pinned on the operand stack until the arguments are consumed. */` |
|       550 | 4695 | `		pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH;` |
|       550 | 4696 | `		if( !bVarSource \|\| bTemp ){` |
|       160 | 4697 | `			pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|        79 | 4698 | `		}` |
|         - | 4699 | `		/* Traverse in insertion order (pPrev is the forward link` |
|         - | 4700 | `		 * in PHL's circular doubly-linked hashmap node list). */` |
|       550 | 4701 | `		pNode = pNode->pPrev;` |
|         - | 4702 | `		/* Push the remaining elements */` |
|      2258 | 4703 | `		for( i = 1; i < nEntry; i++ ){` |
|      1712 | 4704 | `			pTos++;` |
|      1712 | 4705 | `			PH7_MemObjInit(pVm, pTos);` |
|      1712 | 4706 | `			pElem = (ph7_value *)SySetAt(&pVm->aMemObj, pNode->nValIdx);` |
|      1712 | 4707 | `			if( pElem ){` |
|      1712 | 4708 | `				if( bTemp ){` |
|      1289 | 4709 | `					PH7_MemObjStore(pElem, pTos);` |
|       645 | 4710 | `				}else{` |
|       424 | 4711 | `					PH7_MemObjLoad(pElem, pTos);` |
|         - | 4712 | `				}` |
|       854 | 4713 | `			}` |
|      1712 | 4714 | `			pTos->nIdx = (bVarSource && !bTemp) ? pNode->nValIdx : SXU32_HIGH; /* see the first element */` |
|      1712 | 4715 | `			if( !bVarSource \|\| bTemp ){` |
|      1289 | 4716 | `				pTos->iFlags \|= MEMOBJ_AUX_CUFVAL;` |
|       644 | 4717 | `			}` |
|      1712 | 4718 | `			pNode = pNode->pPrev;` |
|       858 | 4719 | `		}` |
|       550 | 4720 | `		PH7_HashmapUnref(pMap);` |
|         - | 4721 | `	}` |
|       616 | 4722 | `	*ppTos = pTos;` |
|       616 | 4723 | `}` |
|         - | 4724 | `/*` |
|         - | 4725 | ` * Build the effective per-actual-slot argument-name map for a CALL/NEW whose` |
|         - | 4726 | `` * argument list contained an unpack (spread). `pCompile` is the compile-time`` |
|         - | 4727 | ` * VmCallArgMap (indexed by compile-time argument position); pArg/nActual describe` |
|         - | 4728 | ` * the flattened actual arguments on the stack. Replays the captured spread runs +` |
|         - | 4729 | ` * element keys, interleaving them with the compile-time names at their real` |
|         - | 4730 | ` * post-expansion positions, into pVm->aEffArgName (one SyString per actual slot).` |
|         - | 4731 | ` *` |
|         - | 4732 | ` * Per-call scope: OP_SPREAD accumulates runs from EVERY currently-evaluating call` |
|         - | 4733 | ` * (an inner spread-bearing call runs between two of an outer call's spreads), so a` |
|         - | 4734 | ` * call's runs are the contiguous SUFFIX whose pStart lands in [pArg, top). Runs` |
|         - | 4735 | ` * below pArg belong to an enclosing, not-yet-built call and are skipped. Before` |
|         - | 4736 | ` * returning, this call's runs (and their keys/blob bytes) are TRUNCATED away,` |
|         - | 4737 | ` * restoring the buffers to the enclosing call's state — this is the per-call reset` |
|         - | 4738 | ` * (there is no global lazy flag). MUST run once per spread call, hence the caller` |
|         - | 4739 | ` * invokes it against the FINAL argument base (methods rebuild after their` |
|         - | 4740 | ` * method-name slot pop shifts pArg).` |
|         - | 4741 | ` *` |
|         - | 4742 | ` * Returns 1 and fills *pEff (nTotal == nActual, aNames -> aEffArgName base,` |
|         - | 4743 | ` * bHasNamed set) when at least one actual slot is named; returns 0 to keep the` |
|         - | 4744 | ` * positional fast path. The returned names alias pVm->sSpreadKeyBlob (spread keys)` |
|         - | 4745 | ` * and the compile map (compile names); both stay valid until the next OP_SPREAD,` |
|         - | 4746 | ` * which is after this call's synchronous named-arg resolution.` |
|         - | 4747 | ` */` |
|       576 | 4748 | `static int VmBuildEffectiveArgMap(ph7_vm *pVm, VmCallArgMap *pCompile,` |
|         - | 4749 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pEff)` |
|         4 | 4750 | `{` |
|       580 | 4751 | `	sxu32 nRun = SySetUsed(&pVm->aSpreadRun);` |
|         - | 4752 | `	VmSpreadRun *aRun;` |
|         - | 4753 | `	VmSpreadKey *aKey;` |
|         - | 4754 | `	const char *zKeyBase;` |
|       580 | 4755 | `	sxu32 nTotal = pCompile ? pCompile->nTotal : 0;` |
|       580 | 4756 | `	int bAnyNamed = 0;` |
|         - | 4757 | `	sxu32 ai, ci, ri, rStart;` |
|       580 | 4758 | `	if( nRun == 0 ){` |
|         - | 4759 | `		/* No spread captured at all — the compile map is already aligned. */` |
|       ! 0 | 4760 | `		return 0;` |
|         - | 4761 | `	}` |
|       580 | 4762 | `	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);` |
|       580 | 4763 | `	aKey = (VmSpreadKey *)SySetBasePtr(&pVm->aSpreadKey);` |
|       580 | 4764 | `	zKeyBase = (const char *)SyBlobData(&pVm->sSpreadKeyBlob);` |
|         - | 4765 | `	/* This call's runs begin at the boundary VmSpreadOwnExtra assigned (runs below` |
|         - | 4766 | `	 * it belong to an enclosing, not-yet-built call). Indexing by that boundary —` |
|         - | 4767 | ``	 * rather than a pStart scan — is what keeps an enclosing zero-width `...[]` run`` |
|         - | 4768 | `	 * that shares this call's base slot from being mis-attributed here. */` |
|       580 | 4769 | `	ri = pVm->nSpreadCallBase;` |
|       580 | 4770 | `	rStart = ri;` |
|       580 | 4771 | `	if( rStart >= nRun ){` |
|         - | 4772 | `		/* No run anchored in this call's argument region — nothing to realign. */` |
|       ! 0 | 4773 | `		return 0;` |
|         - | 4774 | `	}` |
|       580 | 4775 | `	SySetReset(&pVm->aEffArgName);` |
|       580 | 4776 | `	ci = 0;` |
|       580 | 4777 | `	ai = 0;` |
|      1472 | 4778 | `	while( ai < nActual ){` |
|         - | 4779 | `		SyString sName;` |
|       896 | 4780 | `		SyZero(&sName, sizeof(sName)); /* positional by default (nByte == 0) */` |
|         - | 4781 | `		/* Empty runs (spread of []) anchored here consumed a compile arg but no` |
|         - | 4782 | `		 * slot — skip past their compile-name entry to keep alignment. */` |
|       908 | 4783 | `		while( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount == 0 ){` |
|        13 | 4784 | `			ci++; ri++;` |
|         1 | 4785 | `		}` |
|       896 | 4786 | `		if( ri < nRun && aRun[ri].pStart == &pArg[ai] && aRun[ri].nCount > 0 ){` |
|         - | 4787 | `			/* A run of spread elements: one name per element from its key. Keys` |
|         - | 4788 | `			 * are indexed by the run's own nKeyStart, so a non-matching (leaked)` |
|         - | 4789 | `			 * run never desyncs the key stream. */` |
|       550 | 4790 | `			sxu32 j, K = aRun[ri].nCount, ks = aRun[ri].nKeyStart;` |
|      2804 | 4791 | `			for( j = 0; j < K; j++ ){` |
|      2258 | 4792 | `				SyZero(&sName, sizeof(sName));` |
|      2258 | 4793 | `				if( aKey[ks + j].nLen > 0 ){` |
|       101 | 4794 | `					SyStringInitFromBuf(&sName, zKeyBase + aKey[ks + j].nOff, aKey[ks + j].nLen);` |
|       101 | 4795 | `					bAnyNamed = 1;` |
|        50 | 4796 | `				}` |
|      2258 | 4797 | `				SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|      1131 | 4798 | `			}` |
|       550 | 4799 | `			ai += K;` |
|       550 | 4800 | `			ci++; ri++;` |
|       277 | 4801 | `		}else{` |
|         - | 4802 | `			/* Non-spread compile argument: carry its compile-time name (if any). */` |
|       350 | 4803 | `			if( ci < nTotal && pCompile->aNames[ci].nByte > 0 ){` |
|        35 | 4804 | `				sName = pCompile->aNames[ci];` |
|        35 | 4805 | `				bAnyNamed = 1;` |
|        17 | 4806 | `			}` |
|       350 | 4807 | `			SySetPut(&pVm->aEffArgName, (const void *)&sName);` |
|       350 | 4808 | `			ai++;` |
|       350 | 4809 | `			ci++;` |
|         - | 4810 | `		}` |
|         4 | 4811 | `	}` |
|         - | 4812 | `	/* Consume this call's runs, restoring the buffers to the enclosing call's` |
|         - | 4813 | `	 * state. The just-built names still alias the (now logically-truncated) blob` |
|         - | 4814 | `	 * bytes until this call's synchronous resolution completes, before the next` |
|         - | 4815 | `	 * spread — the truncation only lowers the length, it does not free. */` |
|       580 | 4816 | `	VmSpreadConsume(pVm);` |
|       580 | 4817 | `	if( !bAnyNamed ){` |
|         - | 4818 | `		/* Every actual slot is positional — keep the fast positional path. */` |
|       504 | 4819 | `		return 0;` |
|         - | 4820 | `	}` |
|        77 | 4821 | `	pEff->bHasNamed = 1;` |
|        77 | 4822 | `	pEff->bIsNamespaced = pCompile ? pCompile->bIsNamespaced : 0;` |
|        77 | 4823 | `	pEff->bStrict = pCompile ? pCompile->bStrict : 0;` |
|        77 | 4824 | `	pEff->nOrigNameLit = pCompile ? pCompile->nOrigNameLit : 0;` |
|         - | 4825 | `	/* This map is only ever built for a SPREAD call, whose runtime positions do not` |
|         - | 4826 | `	 * match the ones the compiler classified — so it carries no argument shapes and` |
|         - | 4827 | `	 * the by-ref binders fall back to their runtime test. Zeroed explicitly: pStorage` |
|         - | 4828 | `	 * is the caller's stack local. */` |
|        77 | 4829 | `	pEff->bArgShapes = 0;` |
|        77 | 4830 | `	pEff->nNonLvalMask = 0;` |
|        77 | 4831 | `	pEff->nTempCallMask = 0;` |
|        77 | 4832 | `	if( pCompile ){` |
|        37 | 4833 | `		pEff->sAssertSrc = pCompile->sAssertSrc;` |
|        19 | 4834 | `	}else{` |
|        41 | 4835 | `		pEff->sAssertSrc.zString = 0;` |
|        41 | 4836 | `		pEff->sAssertSrc.nByte = 0;` |
|         - | 4837 | `	}` |
|        77 | 4838 | `	pEff->nTotal = nActual;` |
|        77 | 4839 | `	pEff->aNames = (SyString *)SySetBasePtr(&pVm->aEffArgName);` |
|        77 | 4840 | `	return 1;` |
|       292 | 4841 | `}` |
|         - | 4842 | `/*` |
|         - | 4843 | ` * One-stop resolver for a CALL/NEW dispatch site: returns the effective argument` |
|         - | 4844 | ` * name map (the built spread-key map when it contributes names, else the compile` |
|         - | 4845 | ` * map) AND guarantees this call's captured spread runs are consumed. The build is` |
|         - | 4846 | ``  * only meaningful with actual slots (nActual > 0); a net-zero-arg spread (`f(...[])` `` |
|         - | 4847 | ` * — one compile arg, zero elements) still captured an empty run that MUST be` |
|         - | 4848 | ` * truncated, else it desyncs a later call in the VM-global buffers. So consume` |
|         - | 4849 | ` * unconditionally for a spread call when the build didn't run (after a build that` |
|         - | 4850 | ` * ran, VmSpreadConsume already fired and the second call is a harmless no-op).` |
|         - | 4851 | ` * pArg must be the site's FINAL argument base.` |
|         - | 4852 | ` */` |
|   5253850 | 4853 | `PH7_PRIVATE VmCallArgMap *VmEffCallArgMap(ph7_vm *pVm, VmInstr *pInstr,` |
|         - | 4854 | `	ph7_value *pArg, sxu32 nActual, VmCallArgMap *pStorage)` |
|         5 | 4855 | `{` |
|   5253855 | 4856 | `	VmCallArgMap *pCompile = (VmCallArgMap *)pInstr->p3;` |
|   5253855 | 4857 | `	if( pInstr->iP2 == 0 ){` |
|   5253243 | 4858 | `		return pCompile; /* no spread: compile map already aligned, nothing captured */` |
|         - | 4859 | `	}` |
|       616 | 4860 | `	if( nActual > 0 && VmBuildEffectiveArgMap(pVm, pCompile, pArg, nActual, pStorage) ){` |
|        77 | 4861 | `		return pStorage;` |
|         - | 4862 | `	}` |
|       540 | 4863 | `	VmSpreadConsume(pVm);` |
|       540 | 4864 | `	return pCompile;` |
|   2627966 | 4865 | `}` |
|         - | 4866 | `/*` |
|         - | 4867 | ` * Raise the PHP "Cannot use <type> as array" warning for a non-array source used in a` |
|         - | 4868 | ` * list / array-destructuring assignment. Shared by the positional OP_LOAD_LIST path and the` |
|         - | 4869 | ` * keyed OP_LOAD_IDX (iP2=7) path. The CALLER decides whether to warn at all — both paths` |
|         - | 4870 | ` * silence ONLY null (a bool source warns, php 8) — this only maps the type name and emits.` |
|         - | 4871 | ` */` |
|        14 | 4872 | `PH7_PRIVATE void VmWarnCannotUseAsArray(ph7_vm *pVm, sxi32 iFlags)` |
|         2 | 4873 | `{` |
|        16 | 4874 | `	const char *zType = "unknown";` |
|         - | 4875 | `	char zMsg[64];` |
|        16 | 4876 | `	if( iFlags & MEMOBJ_STRING ){` |
|         6 | 4877 | `		zType = "string";` |
|        13 | 4878 | `	}else if( iFlags & MEMOBJ_REAL ){` |
|         - | 4879 | `		/* REAL before INT: a whole-valued real carries MEMOBJ_REAL\|MEMOBJ_INT (see the` |
|         - | 4880 | `		 * float-identity note), and PHP names it "float" here, not "int". A pure int has no` |
|         - | 4881 | `		 * REAL flag, so it still falls through to the int arm. */` |
|       ! 0 | 4882 | `		zType = "float";` |
|        11 | 4883 | `	}else if( iFlags & MEMOBJ_INT ){` |
|         9 | 4884 | `		zType = "int";` |
|         7 | 4885 | `	}else if( iFlags & MEMOBJ_BOOL ){` |
|         3 | 4886 | `		zType = "bool";` |
|         1 | 4887 | `	}else if( iFlags & MEMOBJ_OBJ ){` |
|       ! 0 | 4888 | `		zType = "object";` |
|       ! 0 | 4889 | `	}else if( iFlags & MEMOBJ_RES ){` |
|       ! 0 | 4890 | `		zType = "resource";` |
|       ! 0 | 4891 | `	}` |
|        16 | 4892 | `	SyBufferFormat(zMsg,sizeof(zMsg),"Cannot use %s as array",zType);` |
|        16 | 4893 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|        16 | 4894 | `}` |
|         - | 4895 | `/*` |
|         - | 4896 | `` * A member access in isset()/empty()/`??` context (OP_MEMBER iP2 = PH7_MEMBER_ISSET/EMPTY/COALESCE)`` |
|         - | 4897 | ` * is a silent lookup: a read-miss must not raise the "Undefined class attribute" / "Expecting class` |
|         - | 4898 | ` * instance" warnings, mirroring the array isset/empty path. What the access ANSWERS still differs` |
|         - | 4899 | ` * per context — see VmMemberCtxWantsValue.` |
|         - | 4900 | ` */` |
|     14012 | 4901 | `PH7_PRIVATE int VmMemberCtxIsLookup(sxi32 iP2)` |
|         5 | 4902 | `{` |
|     14017 | 4903 | `	return iP2 == PH7_MEMBER_ISSET \|\| iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         5 | 4904 | `}` |
|         - | 4905 | `/*` |
|         - | 4906 | ` * Of the three silent lookups, the two that take the property's VALUE rather than a truth:` |
|         - | 4907 | `` * empty() judges emptiness on the value, and `??` IS the value (php's BP_VAR_IS read). Only`` |
|         - | 4908 | ` * isset() stops at the truth.` |
|         - | 4909 | ` */` |
|       160 | 4910 | `PH7_PRIVATE int VmMemberCtxWantsValue(sxi32 iP2)` |
|         3 | 4911 | `{` |
|       163 | 4912 | `	return iP2 == PH7_MEMBER_EMPTY \|\| iP2 == PH7_MEMBER_COALESCE;` |
|         3 | 4913 | `}` |
|         - | 4914 | `/*` |
|         - | 4915 | ` * In-flight magic-accessor guard (band A #3a) — php's property guard.` |
|         - | 4916 | ` * A __get body reading the SAME property of the SAME instance must not` |
|         - | 4917 | ` * re-enter __get (php falls back to the undefined-property path); entries` |
|         - | 4918 | ` * are pushed around the dispatch and popped after, so unrelated nested` |
|         - | 4919 | ` * reads (other names / other instances) still dispatch.` |
|         - | 4920 | ` */` |
|      9635 | 4921 | `PH7_PRIVATE int VmMagicGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4922 | `{` |
|         - | 4923 | `	VmMagicGuard *aG;` |
|         - | 4924 | `	sxu32 nHash;` |
|         - | 4925 | `	sxu32 n;` |
|      9640 | 4926 | `	if( SySetUsed(&pVm->aMagicGuard) == 0 ){` |
|         - | 4927 | `		/* Common case (no accessor in flight): skip the name hash entirely —` |
|         - | 4928 | `		 * every hooked-property access consults the guard, often twice. */` |
|      9456 | 4929 | `		return FALSE;` |
|         - | 4930 | `	}` |
|       186 | 4931 | `	aG = (VmMagicGuard *)SySetBasePtr(&pVm->aMagicGuard);` |
|       186 | 4932 | `	nHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|       230 | 4933 | `	for( n = 0; n < SySetUsed(&pVm->aMagicGuard); ++n ){` |
|       186 | 4934 | `		if( aG[n].pThis == pThis && aG[n].nNameHash == nHash && aG[n].cKind == cKind ){` |
|       142 | 4935 | `			return TRUE;` |
|         - | 4936 | `		}` |
|        23 | 4937 | `	}` |
|        45 | 4938 | `	return FALSE;` |
|      4823 | 4939 | `}` |
|      6807 | 4940 | `PH7_PRIVATE void VmMagicGuardPush(ph7_vm *pVm,void *pThis,const SyString *pName,sxu8 cKind)` |
|         5 | 4941 | `{` |
|         - | 4942 | `	VmMagicGuard sG;` |
|      6812 | 4943 | `	sG.pThis = pThis;` |
|      6812 | 4944 | `	sG.nNameHash = SyBinHash((const void *)pName->zString,pName->nByte);` |
|      6812 | 4945 | `	sG.cKind = cKind;` |
|      6812 | 4946 | `	SySetPut(&pVm->aMagicGuard,(const void *)&sG);` |
|      6812 | 4947 | `}` |
|      6807 | 4948 | `PH7_PRIVATE void VmMagicGuardPop(ph7_vm *pVm)` |
|         5 | 4949 | `{` |
|      6812 | 4950 | `	(void)SySetPop(&pVm->aMagicGuard);` |
|      6812 | 4951 | `}` |
|         - | 4952 | `/*` |
|         - | 4953 | ` * Whether the instruction immediately following an OP_MEMBER that missed (property absent) is a` |
|         - | 4954 | ` * write/modify of the member slot that lands DIRECTLY on it — so a fresh property should be` |
|         - | 4955 | ` * auto-created (PHP-style auto-vivification) for the op to work. Covers the read-modify-write forms` |
|         - | 4956 | ` * whose op immediately follows OP_MEMBER: increment/decrement and the compound-assign family` |
|         - | 4957 | `` * (`$o->n++`, `$o->s .= "x"`, `$o->c += 1`), plus a plain member store. Subscript-writes`` |
|         - | 4958 | `` * (`$o->arr[] = x`, `$o->m[$k] = x`, `??=`) are NOT detectable here — the key sits between OP_MEMBER`` |
|         - | 4959 | ` * and OP_STORE_IDX — so those are marked by the compiler instead (OP_MEMBER iP2 == PH7_MEMBER_WRITE).` |
|         - | 4960 | ` * One-token lookahead only.` |
|         - | 4961 | ` */` |
|     13070 | 4962 | `PH7_PRIVATE int VmMemberNextIsWrite(const VmInstr *pNext)` |
|         5 | 4963 | `{` |
|     13075 | 4964 | `	switch( pNext->iOp ){` |
|       559 | 4965 | `		case PH7_OP_STORE:` |
|      1123 | 4966 | `			return pNext->iP2 != 0;                          /* member store ($o->p = v) */` |
|         6 | 4967 | `		case PH7_OP_STORE_REF:` |
|        13 | 4968 | `			return pNext->iP2 != 0;                          /* member ref store ($o->p =& $x) */` |
|        18 | 4969 | `		case PH7_OP_INCR: case PH7_OP_DECR:` |
|         - | 4970 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 4971 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 4972 | `		case PH7_OP_CAT_STORE:` |
|         - | 4973 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 4974 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|        37 | 4975 | `			return 1;` |
|      5951 | 4976 | `		default:` |
|     11909 | 4977 | `			return 0;` |
|         - | 4978 | `	}` |
|      6541 | 4979 | `}` |
|         - | 4980 | `/*` |
|         - | 4981 | ` * The READ-MODIFY-WRITE subset of VmMemberNextIsWrite: the ops that need the` |
|         - | 4982 | `` * member's CURRENT value before they produce the new one (`$o->n++`, `$o->n .= 'x'`,`` |
|         - | 4983 | `` * `$o->n += 1`). A plain store and a reference store are writes but not`` |
|         - | 4984 | ` * read-modify-writes — neither reads the member — so an accessor class must not` |
|         - | 4985 | ` * treat them as one.` |
|         - | 4986 | ` */` |
|       322 | 4987 | `PH7_PRIVATE int VmMemberNextIsRmw(const VmInstr *pNext)` |
|         5 | 4988 | `{` |
|       459 | 4989 | `	return pNext->iOp == PH7_OP_INCR \|\| pNext->iOp == PH7_OP_DECR` |
|       470 | 4990 | `	    \|\| VmNextIsCompoundAssign(pNext);` |
|         5 | 4991 | `}` |
|         - | 4992 | `/*` |
|         - | 4993 | `` * The `op=` family alone — php's ASSIGN_OP, which on a CONTAINER compiles to`` |
|         - | 4994 | ` * ASSIGN_DIM_OP / ASSIGN_OBJ_OP: read the element, compute, write it back` |
|         - | 4995 | `` * through the container's own handlers. `++`/`--` are deliberately NOT here:`` |
|         - | 4996 | ` * they are php's separate INC/DEC opcodes, and on an overloaded ELEMENT they` |
|         - | 4997 | `` * fetch for WRITING instead — which is why `$o['n'] += 2` stores through`` |
|         - | 4998 | `` * offsetSet where `$o['n']++` only notices.`` |
|         - | 4999 | ` */` |
|       548 | 5000 | `PH7_PRIVATE int VmNextIsCompoundAssign(const VmInstr *pNext)` |
|         5 | 5001 | `{` |
|       553 | 5002 | `	switch( pNext->iOp ){` |
|        58 | 5003 | `		case PH7_OP_ADD_STORE: case PH7_OP_SUB_STORE: case PH7_OP_MUL_STORE:` |
|         - | 5004 | `		case PH7_OP_DIV_STORE: case PH7_OP_MOD_STORE: case PH7_OP_POW_STORE:` |
|         - | 5005 | `		case PH7_OP_CAT_STORE:` |
|         - | 5006 | `		case PH7_OP_SHL_STORE: case PH7_OP_SHR_STORE:` |
|         - | 5007 | `		case PH7_OP_BAND_STORE: case PH7_OP_BOR_STORE: case PH7_OP_BXOR_STORE:` |
|       117 | 5008 | `			return 1;` |
|       216 | 5009 | `		default:` |
|       437 | 5010 | `			return 0;` |
|         - | 5011 | `	}` |
|       279 | 5012 | `}` |
|         - | 5013 | `/*` |
|         - | 5014 | ` * Whether execution is currently INSIDE one of pName's own hook bodies on this` |
|         - | 5015 | ` * instance. php's rule: within ANY hook of property x (get or set alike),` |
|         - | 5016 | `` * `$this->x` addresses the raw backing store for BOTH reads and writes — so`` |
|         - | 5017 | ` * every hook-dispatch decision checks both guard kinds, not just its own.` |
|         - | 5018 | ` */` |
|       546 | 5019 | `PH7_PRIVATE int VmHookGuardHeld(ph7_vm *pVm,void *pThis,const SyString *pName)` |
|         5 | 5020 | `{` |
|       551 | 5021 | `	return VmMagicGuardHeld(pVm,pThis,pName,'G') \|\| VmMagicGuardHeld(pVm,pThis,pName,'S');` |
|         5 | 5022 | `}` |
|         - | 5023 | `/*` |
|         - | 5024 | ` * Dispatch the SET side of a hooked-property write: php's read-only Error when` |
|         - | 5025 | ` * the property has no set hook, the asymmetric set-visibility check (php checks` |
|         - | 5026 | ` * it before the hook runs), then __phl_hook_set_NAME with pValue; a` |
|         - | 5027 | `` * `set => expr` hook's return value is stored into the BACKING slot through the`` |
|         - | 5028 | ` * ordinary typed enforcement. Errors park on the boundary rail (the caller's` |
|         - | 5029 | ` * opcode completes benignly; the fetch-point router lands them). Shared by the` |
|         - | 5030 | ` * plain-store consume (OP_STORE), the coalesce-assign consume (OP_NULLC_STORE)` |
|         - | 5031 | ` * and the read-modify-write write-back (VmHookRmwConsume). Does NOT release the` |
|         - | 5032 | ` * caller's reference on pHThis. Returns PH7_ABORT only for the enforcement's` |
|         - | 5033 | ` * abort path; SXRET_OK otherwise.` |
|         - | 5034 | ` */` |
|        76 | 5035 | `PH7_PRIVATE sxi32 VmHookSetDispatch(ph7_vm *pVm,ph7_class_instance *pHThis,ph7_class_attr *pHAttr,sxu32 nBackIdx,ph7_value *pValue)` |
|         4 | 5036 | `{` |
|         - | 5037 | `	char zHName[384];` |
|         - | 5038 | `	sxu32 nHName;` |
|         - | 5039 | `	ph7_class_method *pSetHook;` |
|        80 | 5040 | `	if( (pHAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) == 0 ){` |
|         - | 5041 | `		/* get-only hooked property: php's read-only Error */` |
|         - | 5042 | `		SyBlob sErrMsg;` |
|         5 | 5043 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|         5 | 5044 | `		SyBlobFormat(&sErrMsg,"Property %z::$%z is read-only",` |
|         4 | 5045 | `			&pHThis->pClass->sName,&pHAttr->sName);` |
|         5 | 5046 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|         5 | 5047 | `		return SXRET_OK;` |
|         - | 5048 | `	}` |
|        76 | 5049 | `	if( pHAttr->iFlags & (PH7_CLASS_ATTR_PRIVATE_SET\|PH7_CLASS_ATTR_PROTECTED_SET) ){` |
|         - | 5050 | `		/* Asymmetric set-visibility is checked BEFORE the hook dispatches (php) */` |
|       ! 0 | 5051 | `		sxi32 rcVis = VmCheckSetVisibility(&(*pVm),pHThis->pClass,pHAttr);` |
|       ! 0 | 5052 | `		if( rcVis != SXRET_OK ){` |
|       ! 0 | 5053 | `			VmBoundaryPark(&(*pVm),rcVis);` |
|       ! 0 | 5054 | `			return SXRET_OK;` |
|         - | 5055 | `		}` |
|       ! 0 | 5056 | `	}` |
|        76 | 5057 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_set_%z",&pHAttr->sName);` |
|        76 | 5058 | `	pSetHook = PH7_ClassExtractMethod(pHThis->pClass,zHName,nHName);` |
|        76 | 5059 | `	if( pSetHook ){` |
|         - | 5060 | `		ph7_value sHookRet;` |
|         - | 5061 | `		ph7_value *apHArg[1];` |
|        76 | 5062 | `		apHArg[0] = pValue;` |
|        76 | 5063 | `		PH7_MemObjInit(pVm,&sHookRet);` |
|        76 | 5064 | `		VmMagicGuardPush(pVm,(void *)pHThis,&pHAttr->sName,'S');` |
|        76 | 5065 | `		PH7_VmCallClassMethod(&(*pVm),pHThis,pSetHook,&sHookRet,1,apHArg);` |
|        76 | 5066 | `		VmMagicGuardPop(pVm);` |
|        72 | 5067 | `		if( (pSetHook->sFunc.iFlags & VM_FUNC_HOOK_SET_EXPR)` |
|        42 | 5068 | `		 && nBackIdx != SXU32_HIGH && pVm->nBoundaryRc == 0 ){` |
|         6 | 5069 | `			sxi32 rcH = VmEnforcePropertyTypeOnStore(&(*pVm),nBackIdx,&sHookRet,0);` |
|         6 | 5070 | `			if( rcH == SXRET_OK ){` |
|         6 | 5071 | `				ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,nBackIdx);` |
|         6 | 5072 | `				if( pBack ){` |
|         6 | 5073 | `					PH7_MemObjStore(&sHookRet,pBack);` |
|         4 | 5074 | `				}` |
|         2 | 5075 | `			}else if( rcH == PH7_ABORT ){` |
|       ! 0 | 5076 | `				PH7_MemObjRelease(&sHookRet);` |
|       ! 0 | 5077 | `				return PH7_ABORT;` |
|         - | 5078 | `			}` |
|         - | 5079 | `			/* PH7_EXCEPTION: the TypeError was routed/parked by the enforcement —` |
|         - | 5080 | `			 * the store is skipped, execution lands at the fetch point like any` |
|         - | 5081 | `			 * parked throw. */` |
|         2 | 5082 | `		}` |
|        76 | 5083 | `		PH7_MemObjRelease(&sHookRet);` |
|        36 | 5084 | `	}` |
|        76 | 5085 | `	return SXRET_OK;` |
|        42 | 5086 | `}` |
|         - | 5087 | `/*` |
|         - | 5088 | ` * Consume the top pending hook-RMW entry if it targets slot nIdx — called from` |
|         - | 5089 | ` * the tail of every read-modify-write opcode (++/--/compound-assign) with the` |
|         - | 5090 | ` * slot it just wrote. A non-matching top (an ordinary variable RMW running` |
|         - | 5091 | ` * inside a nested exec while an outer write-back is pending) is left alone.` |
|         - | 5092 | ` * On match: copy the computed value out of the scratch slot, return the slot` |
|         - | 5093 | ` * to the free pool, and dispatch the set side (VmHookSetDispatch) — unless a` |
|         - | 5094 | ` * throw parked during the modify op, which wins (php: the exception discards` |
|         - | 5095 | ` * the write). Returns SXERR_NOTFOUND when nothing was consumed, PH7_ABORT to` |
|         - | 5096 | ` * propagate the enforcement abort, SXRET_OK otherwise.` |
|         - | 5097 | ` */` |
|         - | 5098 | `/*` |
|         - | 5099 | ` * Hook-aware attribute read for the C-side object walks — foreach over an` |
|         - | 5100 | ` * object, get_object_vars(), json_encode(), var_export(): php dispatches the` |
|         - | 5101 | ` * GET hook on these surfaces, while var_dump / (array) / print_r / serialize` |
|         - | 5102 | ` * read the raw backing store. Fills pOut (an initialized ph7_value the caller` |
|         - | 5103 | ` * owns) with the hook's return value and returns SXRET_OK — or the call's` |
|         - | 5104 | ` * PH7_EXCEPTION/PH7_ABORT when the hook threw (also parked on the boundary` |
|         - | 5105 | ` * rail like any C-boundary callee). Returns SXERR_NOTFOUND when the property` |
|         - | 5106 | ` * has no get hook (or execution is inside one of its own hook bodies): the` |
|         - | 5107 | ` * caller reads the raw slot then.` |
|         - | 5108 | ` */` |
|       700 | 5109 | `PH7_PRIVATE sxi32 PH7_VmHookGetAttrValue(ph7_class_instance *pThis,VmClassAttr *pVmAttr,ph7_value *pOut)` |
|         5 | 5110 | `{` |
|       705 | 5111 | `	ph7_vm *pVm = pThis->pVm;` |
|         - | 5112 | `	char zHName[384];` |
|         - | 5113 | `	sxu32 nHName;` |
|         - | 5114 | `	ph7_class_method *pGetHook;` |
|         - | 5115 | `	sxi32 rc;` |
|       700 | 5116 | `	if( (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) == 0` |
|       445 | 5117 | `	 \|\| VmHookGuardHeld(pVm,(void *)pThis,&pVmAttr->pAttr->sName)` |
|       195 | 5118 | `	 \|\| pVm->nBoundaryRc != 0 ){` |
|         - | 5119 | `		/* The boundary-rc gate keeps a C-side walk (get_object_vars/var_export/` |
|         - | 5120 | `		 * foreach) from running FURTHER hooks after one already threw — php` |
|         - | 5121 | `		 * aborts the whole builtin at the first throw; the walk falls back to` |
|         - | 5122 | `		 * raw values whose output the routed throw then discards. */` |
|       515 | 5123 | `		return SXERR_NOTFOUND;` |
|         - | 5124 | `	}` |
|       195 | 5125 | `	nHName = SyBufferFormat(zHName,sizeof(zHName),"__phl_hook_get_%z",&pVmAttr->pAttr->sName);` |
|       195 | 5126 | `	pGetHook = PH7_ClassExtractMethod(pThis->pClass,zHName,nHName);` |
|       195 | 5127 | `	if( pGetHook == 0 ){` |
|       ! 0 | 5128 | `		return SXERR_NOTFOUND;` |
|         - | 5129 | `	}` |
|       195 | 5130 | `	VmMagicGuardPush(pVm,(void *)pThis,&pVmAttr->pAttr->sName,'G');` |
|       195 | 5131 | `	rc = PH7_VmCallClassMethod(&(*pVm),pThis,pGetHook,pOut,0,0);` |
|       195 | 5132 | `	VmMagicGuardPop(pVm);` |
|       195 | 5133 | `	return rc;` |
|       355 | 5134 | `}` |
|         - | 5135 | `/*` |
|         - | 5136 | ` * Release a hook-RMW SCRATCH slot: drop its contents and return the index to` |
|         - | 5137 | ` * the free pool. Scratch slots come from PH7_ReserveMemObj and are never` |
|         - | 5138 | ` * ref-linked, so this bypasses PH7_VmUnsetMemObj's VmRefObj bookkeeping.` |
|         - | 5139 | ` */` |
|       146 | 5140 | `PH7_PRIVATE void VmHookRmwFreeScratch(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5141 | `{` |
|       147 | 5142 | `	ph7_value *pScr = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|         - | 5143 | `	VmSlot sFree;` |
|       147 | 5144 | `	if( pScr ){` |
|       147 | 5145 | `		PH7_MemObjRelease(pScr);` |
|        73 | 5146 | `	}` |
|       147 | 5147 | `	sFree.nIdx = nIdx;` |
|       147 | 5148 | `	sFree.pUserData = 0;` |
|       147 | 5149 | `	SySetPut(&pVm->aFreeObj,(const void *)&sFree);` |
|       147 | 5150 | `}` |
|         - | 5151 | `/*` |
|         - | 5152 | ` * Drop the top pending write-back entry without dispatching its set side: the` |
|         - | 5153 | ` * arming statement was abandoned by a throw, or a ??= short-circuit jump` |
|         - | 5154 | ` * skipped its assign (php: the throw/skip discards the write). Releases the` |
|         - | 5155 | ` * scratch slot (RMW kind), the name blob (MAGIC kind) and the entry's` |
|         - | 5156 | ` * instance reference.` |
|         - | 5157 | ` */` |
|        24 | 5158 | `PH7_PRIVATE void VmHookRmwDropTop(ph7_vm *pVm)` |
|         2 | 5159 | `{` |
|        26 | 5160 | `	VmHookRmw *pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        26 | 5161 | `	if( pEnt == 0 ){` |
|         5 | 5162 | `		return;` |
|         - | 5163 | `	}` |
|        21 | 5164 | `	if( pEnt->nScratchIdx != SXU32_HIGH ){` |
|        13 | 5165 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nScratchIdx);` |
|         6 | 5166 | `	}` |
|        21 | 5167 | `	if( pEnt->iKind == VM_HOOK_PEND_RMW_DIM && pEnt->nBackIdx != SXU32_HIGH ){` |
|         - | 5168 | `		/* The DIM kind's nBackIdx is a reserved KEY slot of its own, not a` |
|         - | 5169 | `		 * property's backing store — this entry owns it. */` |
|         5 | 5170 | `		VmHookRmwFreeScratch(&(*pVm),pEnt->nBackIdx);` |
|         2 | 5171 | `	}` |
|        21 | 5172 | `	SyBlobRelease(&pEnt->sName);` |
|        21 | 5173 | `	PH7_ClassInstanceUnref(pEnt->pThis);` |
|        21 | 5174 | `	(void)SySetPop(&pVm->aHookRmw);` |
|        14 | 5175 | `}` |
|        84 | 5176 | `PH7_PRIVATE sxi32 VmHookRmwConsume(ph7_vm *pVm,sxu32 nIdx)` |
|         1 | 5177 | `{` |
|         - | 5178 | `	VmHookRmw sEnt;` |
|         - | 5179 | `	VmHookRmw *pEnt;` |
|         - | 5180 | `	ph7_value *pScr;` |
|         - | 5181 | `	ph7_value sVal;` |
|         - | 5182 | `	ph7_value sKey;` |
|        85 | 5183 | `	sxi32 rc = SXRET_OK;` |
|        85 | 5184 | `	pEnt = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|        85 | 5185 | `	if( pEnt == 0 \|\| !VM_HOOK_PEND_IS_RMW(pEnt->iKind) \|\| pEnt->nScratchIdx != nIdx ){` |
|       ! 0 | 5186 | `		return SXERR_NOTFOUND;` |
|         - | 5187 | `	}` |
|        85 | 5188 | `	sEnt = *pEnt;` |
|        85 | 5189 | `	(void)SySetPop(&pVm->aHookRmw);` |
|         - | 5190 | `	/* Copy the computed value out of the scratch slot, then free the slot` |
|         - | 5191 | `	 * (the set dispatch below may reserve slots — nothing may read the` |
|         - | 5192 | `	 * scratch index past this point). The DIM kind's KEY slot goes the same` |
|         - | 5193 | `	 * way, for the same reason: reserving relocates the aMemObj set. */` |
|        85 | 5194 | `	PH7_MemObjInit(pVm,&sVal);` |
|        85 | 5195 | `	pScr = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nScratchIdx);` |
|        85 | 5196 | `	if( pScr ){` |
|        85 | 5197 | `		PH7_MemObjStore(pScr,&sVal);` |
|        42 | 5198 | `	}` |
|        85 | 5199 | `	VmHookRmwFreeScratch(&(*pVm),sEnt.nScratchIdx);` |
|        85 | 5200 | `	sVal.nIdx = SXU32_HIGH;` |
|        85 | 5201 | `	PH7_MemObjInit(pVm,&sKey);` |
|        85 | 5202 | `	if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM && sEnt.nBackIdx != SXU32_HIGH ){` |
|        47 | 5203 | `		ph7_value *pKeySlot = (ph7_value *)SySetAt(&pVm->aMemObj,sEnt.nBackIdx);` |
|        47 | 5204 | `		if( pKeySlot ){` |
|        47 | 5205 | `			PH7_MemObjStore(pKeySlot,&sKey);` |
|        23 | 5206 | `		}` |
|        47 | 5207 | `		VmHookRmwFreeScratch(&(*pVm),sEnt.nBackIdx);` |
|        47 | 5208 | `		sKey.nIdx = SXU32_HIGH;` |
|        23 | 5209 | `	}` |
|        85 | 5210 | `	if( pVm->nBoundaryRc == 0 ){` |
|        85 | 5211 | `		if( sEnt.iKind == VM_HOOK_PEND_RMW_MAGIC ){` |
|         - | 5212 | `			/* Overloaded property: the write side is __set($name, $computed) —` |
|         - | 5213 | ``			 * php's second half of `$o->n++` on a class with both accessors. */`` |
|         - | 5214 | `			SyString sPropName;` |
|        25 | 5215 | `			SyStringInitFromBuf(&sPropName,SyBlobData(&sEnt.sName),SyBlobLength(&sEnt.sName));` |
|        25 | 5216 | `			VmMagicSetDispatch(&(*pVm),sEnt.pThis,&sPropName,&sVal);` |
|        73 | 5217 | `		}else if( sEnt.iKind == VM_HOOK_PEND_RMW_DIM ){` |
|         - | 5218 | `			/* ArrayAccess element: php's ASSIGN_DIM_OP writes the computed value` |
|         - | 5219 | `			 * back through offsetSet($key, $value). */` |
|        47 | 5220 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(sEnt.pThis->pClass,` |
|         - | 5221 | `				"offsetSet",sizeof("offsetSet")-1);` |
|        47 | 5222 | `			if( pSet ){` |
|         - | 5223 | `				ph7_value *apArg[2];` |
|        43 | 5224 | `				apArg[0] = &sKey;` |
|        43 | 5225 | `				apArg[1] = &sVal;` |
|        43 | 5226 | `				PH7_VmCallClassMethod(&(*pVm),sEnt.pThis,pSet,0,2,apArg);` |
|        22 | 5227 | `			}else{` |
|         - | 5228 | `				/* A container that answers a READ and has nowhere to put the write:` |
|         - | 5229 | `				 * a class carrying a native dimension handler (ph7_class::xDim) and` |
|         - | 5230 | `				 * no ArrayAccess, which is php's DOMNodeList. php's read-then-write` |
|         - | 5231 | ``				 * pair ends in the plain store's Error, so `$list[9] .= 'x'` says`` |
|         - | 5232 | ``				 * what `$list[9] = 'x'` says. Parked: this runs at an arithmetic`` |
|         - | 5233 | `				 * op's tail, not at a throw boundary. */` |
|         - | 5234 | `				char zMsg[256];` |
|         5 | 5235 | `				SyString *pName = &sEnt.pThis->pClass->sName;` |
|         7 | 5236 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|         - | 5237 | `					"Cannot use object of type %.*s as array",` |
|         4 | 5238 | `					(int)pName->nByte,pName->zString);` |
|         5 | 5239 | `				VmBoundaryPark(&(*pVm),VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg));` |
|         - | 5240 | `			}` |
|        24 | 5241 | `		}else{` |
|        15 | 5242 | `			rc = VmHookSetDispatch(&(*pVm),sEnt.pThis,sEnt.pAttr,sEnt.nBackIdx,&sVal);` |
|         - | 5243 | `		}` |
|        42 | 5244 | `	}` |
|        85 | 5245 | `	SyBlobRelease(&sEnt.sName);` |
|        85 | 5246 | `	PH7_MemObjRelease(&sKey);` |
|        85 | 5247 | `	PH7_MemObjRelease(&sVal);` |
|        85 | 5248 | `	PH7_ClassInstanceUnref(sEnt.pThis);` |
|        85 | 5249 | `	return rc;` |
|        43 | 5250 | `}` |
|         - | 5251 | `/*` |
|         - | 5252 | ` * Dispatch __set($name, $value) on pSetThis — the shared consume for a pending` |
|         - | 5253 | ` * magic-set: OP_STORE's plain-store transient and OP_NULLC_STORE's coalesce` |
|         - | 5254 | ` * entry both funnel here. The guard makes a same-name write inside __set fall` |
|         - | 5255 | ` * through to creation, like php. Does NOT release the caller's reference.` |
|         - | 5256 | ` */` |
|       400 | 5257 | `PH7_PRIVATE void VmMagicSetDispatch(ph7_vm *pVm,ph7_class_instance *pSetThis,const SyString *pName,ph7_value *pValue)` |
|         3 | 5258 | `{` |
|       403 | 5259 | `	ph7_class_method *pSetMeth = PH7_ClassExtractMethod(pSetThis->pClass,"__set",sizeof("__set")-1);` |
|       403 | 5260 | `	if( pSetMeth ){` |
|         - | 5261 | `		ph7_value sNameVal;` |
|         - | 5262 | `		ph7_value *apSetArg[2];` |
|       403 | 5263 | `		PH7_MemObjInitFromString(pVm,&sNameVal,pName);` |
|       403 | 5264 | `		sNameVal.nIdx = SXU32_HIGH;` |
|       403 | 5265 | `		apSetArg[0] = &sNameVal;` |
|       403 | 5266 | `		apSetArg[1] = pValue;` |
|       403 | 5267 | `		VmMagicGuardPush(pVm,(void *)pSetThis,pName,'s');` |
|       403 | 5268 | `		PH7_VmCallMagicMethod(&(*pVm),pSetThis,pSetMeth,0,2,apSetArg);` |
|       403 | 5269 | `		VmMagicGuardPop(pVm);` |
|       403 | 5270 | `		PH7_MemObjRelease(&sNameVal);` |
|       200 | 5271 | `	}` |
|       403 | 5272 | `}` |
|         - | 5273 | `/*` |
|         - | 5274 | ` * Abandon an ITERATOR-mode foreach step: release the aggregate owner (if this` |
|         - | 5275 | ` * was an IteratorAggregate foreach), free the step, pop it off the info's` |
|         - | 5276 | ` * step stack and drop the step's retain on the iterator instance. The single` |
|         - | 5277 | ` * home for this teardown — it runs on iterator exhaustion AND on every` |
|         - | 5278 | ` * iterator-protocol throw path (next/valid/current/key); a per-site copy that` |
|         - | 5279 | ` * drifts produces a leak or pool-masked use-after-free on exactly one throw` |
|         - | 5280 | ` * path (the SyHash-layout incident class).` |
|         - | 5281 | ` */` |
|         - | 5282 | `/*` |
|         - | 5283 | ` * Remove pStep's pointer from the per-statement aStep set. The step being torn` |
|         - | 5284 | ` * down is not necessarily the last one pushed — two generator/fiber instances` |
|         - | 5285 | ` * of the same foreach suspend and finish out of LIFO order — so find it by` |
|         - | 5286 | ` * pointer. Removal is ORDER-PRESERVING (shift the tail down): OP_FOREACH_STEP's` |
|         - | 5287 | ` * top-down scan relies on the running activation's step (always the most-recent` |
|         - | 5288 | ` * push for its statement) sitting ABOVE any leaked older step that may share a` |
|         - | 5289 | ` * recycled frame address. A swap-with-last would move a newer step below such a` |
|         - | 5290 | ` * leaked step and let the scan match the stale one. A no-op if the step was` |
|         - | 5291 | ` * never linked (INIT error path).` |
|         - | 5292 | ` */` |
|     32010 | 5293 | `PH7_PRIVATE void VmForeachStepUnlink(ph7_foreach_info *pInfo,ph7_foreach_step *pStep)` |
|         5 | 5294 | `{` |
|     32015 | 5295 | `	ph7_foreach_step **apStep = (ph7_foreach_step **)SySetBasePtr(&pInfo->aStep);` |
|     32015 | 5296 | `	sxu32 n = SySetUsed(&pInfo->aStep);` |
|         - | 5297 | `	sxu32 i;` |
|     34397 | 5298 | `	for( i = 0 ; i < n ; ++i ){` |
|     34397 | 5299 | `		if( apStep[i] == pStep ){` |
|     32023 | 5300 | `			for( ; i + 1 < n ; ++i ){` |
|         9 | 5301 | `				apStep[i] = apStep[i + 1];` |
|         5 | 5302 | `			}` |
|     32015 | 5303 | `			(void)SySetPop(&pInfo->aStep);` |
|     32015 | 5304 | `			return;` |
|         - | 5305 | `		}` |
|      1196 | 5306 | `	}` |
|     16010 | 5307 | `}` |
|       462 | 5308 | `PH7_PRIVATE void VmForeachStepAbandon(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,ph7_class_instance *pThis)` |
|         5 | 5309 | `{` |
|       467 | 5310 | `	if( pStep->pOwner ){` |
|       173 | 5311 | `		PH7_ClassInstanceUnref(pStep->pOwner);` |
|        85 | 5312 | `	}` |
|       467 | 5313 | `	VmForeachStepUnlink(pInfo,pStep);` |
|       467 | 5314 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|       467 | 5315 | `	PH7_ClassInstanceUnref(pThis);` |
|       467 | 5316 | `}` |
|         - | 5317 | `/*` |
|         - | 5318 | ` * Tear down a HASHMAP-mode foreach step: unhook its private cursor from the` |
|         - | 5319 | ` * map's active-step registry, free the step, optionally pop it off the info's` |
|         - | 5320 | ` * step stack, then drop the step's map reference. The single home for this` |
|         - | 5321 | ` * teardown (the hashmap twin of VmForeachStepAbandon above) — the ordering is` |
|         - | 5322 | ` * load-bearing: a step freed while still registered is walked by the next` |
|         - | 5323 | ` * PH7_HashmapUnlinkNode as a recycled pool slot (the SyHash-layout incident` |
|         - | 5324 | ` * class), and the unregister must precede the unref in case the step held the` |
|         - | 5325 | ` * map's last reference.` |
|         - | 5326 | ` */` |
|     31488 | 5327 | `PH7_PRIVATE void VmForeachHashmapStepRelease(ph7_vm *pVm,ph7_foreach_info *pInfo,ph7_foreach_step *pStep,int bPop)` |
|         5 | 5328 | `{` |
|     31493 | 5329 | `	ph7_hashmap *pMap = pStep->xIter.pMap;` |
|     31493 | 5330 | `	PH7_HashmapUnregisterForeachStep(pMap,pStep);` |
|     31493 | 5331 | `	if( bPop ){` |
|         - | 5332 | `		/* Remove by pointer, not position: an out-of-LIFO-order generator/fiber` |
|         - | 5333 | `		 * teardown may leave this step below newer ones on the shared aStep. */` |
|     31493 | 5334 | `		VmForeachStepUnlink(pInfo,pStep);` |
|     15744 | 5335 | `	}` |
|     31493 | 5336 | `	SyMemBackendPoolFree(&pVm->sAllocator,pStep);` |
|     31493 | 5337 | `	PH7_HashmapUnref(pMap);` |
|     31493 | 5338 | `}` |
|         - | 5339 | `/* VmExecState / VmCallRecord / VmCallFrame / VmParkedSegment structs moved to ph7int.h */` |
|         - | 5340 | `/*` |
|         - | 5341 | ` * Execute as much of a local PH7 bytecode program as we can then return.` |
|         - | 5342 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5343 | ` * See block-comment on that function for additional information.` |
|         - | 5344 | ` */` |
|   1491849 | 5345 | `PH7_PRIVATE sxi32 VmLocalExec(ph7_vm *pVm,SySet *pByteCode,ph7_value *pResult,int bReturnPropagates)` |
|         5 | 5346 | `{` |
|         - | 5347 | `	ph7_value *pStack;` |
|         - | 5348 | `	sxu32 nCap;` |
|         - | 5349 | `	sxi32 rc;` |
|         - | 5350 | `	/* Allocate a new operand stack */` |
|   1491854 | 5351 | `	pStack = VmNewOperandStack(&(*pVm),SySetUsed(pByteCode));` |
|   1491854 | 5352 | `	if( pStack == 0 ){` |
|       ! 0 | 5353 | `		return SXERR_MEM;` |
|         - | 5354 | `	}` |
|   1491854 | 5355 | `	nCap = SySetUsed(pByteCode) + VM_STACK_GUARD; /* what VmNewOperandStack handed out */` |
|         - | 5356 | `	/* Execute the program. A base-level OP_SPREAD may realloc pStack (updating it +` |
|         - | 5357 | `	 * nCap through the owner slots) — free whatever pStack ends up pointing at. */` |
|   1491854 | 5358 | `	rc = VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pByteCode),pStack,-1,&(*pResult),0,FALSE,0,0,bReturnPropagates,0,&pStack,&nCap,nCap);` |
|         - | 5359 | `	/* Free the operand stack */` |
|   1491854 | 5360 | `	SyMemBackendFree(&pVm->sAllocator,pStack);` |
|         - | 5361 | `	/* Execution result */` |
|   1491854 | 5362 | `	return rc;` |
|    745927 | 5363 | `}` |
|         - | 5364 | `/*` |
|         - | 5365 | ` * Did the mini-program VmLocalExec just ran end in a THROW that its HOST` |
|         - | 5366 | ` * statement must honour?` |
|         - | 5367 | ` *` |
|         - | 5368 | ` * A mini-program (a match arm or condition, a switch case expression, a` |
|         - | 5369 | ` * property default) is compiled into its own bytecode container but shares the` |
|         - | 5370 | ` * caller's VM frame, so an enclosing try/catch is found and its catch body runs` |
|         - | 5371 | ` * IN PLACE at the throw site — and then VmByteCodeExec unwinds out of the nested` |
|         - | 5372 | ` * exec with PH7_EXCEPTION, having recorded where the catching body should resume` |
|         - | 5373 | ` * (pResumeFrame / pInlineInstr). A host site that ignores that status carries on` |
|         - | 5374 | ` * as if the arm had produced a value: php ABANDONS the whole statement, PHL ran` |
|         - | 5375 | `` * the rest of it (`match` picked its default arm AFTER the catch, `switch` fell`` |
|         - | 5376 | ` * through to its default case). The status is what PH7_THROW_ROUTE_MIDEXPR` |
|         - | 5377 | ` * consumes; this predicate is its guard, named once so the host sites cannot` |
|         - | 5378 | ` * drift apart.` |
|         - | 5379 | ` *` |
|         - | 5380 | ` * The two recorded-resume fields are compared against a SNAPSHOT taken before` |
|         - | 5381 | ` * the nested exec, not against 0 — the same rule PH7_VmClassLookupRaised follows` |
|         - | 5382 | ` * for an autoloader's throw. Testing them for non-zero would misread a record` |
|         - | 5383 | ` * this opcode did not create as its own throw.` |
|         - | 5384 | ` *` |
|         - | 5385 | ` * PH7_ABORT is deliberately NOT folded in — it is not a throw and its host must` |
|         - | 5386 | ` * exit the loop, not route to a landing pad, so each caller screens it first.` |
|         - | 5387 | ` */` |
|         - | 5388 | `/*` |
|         - | 5389 | ` * Is this an AUTO-GLOBAL ($GLOBALS, $_SERVER, $_GET, ...)?` |
|         - | 5390 | ` *` |
|         - | 5391 | ` * php's auto-globals are visible in every scope without importing them, and it` |
|         - | 5392 | `` * REFUSES to let one be captured: `use ($GLOBALS)` is the compile fatal`` |
|         - | 5393 | `` * `Cannot use auto-global as lexical variable`, and an arrow function does not`` |
|         - | 5394 | ` * auto-capture one either. That is not cosmetic. A capture resolves the name` |
|         - | 5395 | ` * through VmExtractMemObj, which consults hSuper FIRST, so installing the` |
|         - | 5396 | ` * captured value writes over the superglobal's own slot: calling` |
|         - | 5397 | `` * `fn() => $GLOBALS['a']` replaced the live symbol-table view with the by-value`` |
|         - | 5398 | ` * SNAPSHOT taken when the closure was created, and every later global became` |
|         - | 5399 | ` * invisible to every reader in the program. extract() already screens for the` |
|         - | 5400 | ` * same reason (VmExtractIsProtected), but through hSuper, which cannot be used` |
|         - | 5401 | ` * here — hSuper is filled by PH7_VmMakeReady, which runs AFTER compilation.` |
|         - | 5402 | ` *` |
|         - | 5403 | `` * Hence the static list. It is php's nine plus PHL's own `_HEADER`, and it`` |
|         - | 5404 | `` * deliberately does NOT include `argv`: PHL installs $argv as a superglobal for`` |
|         - | 5405 | `` * convenience, but php's is an ordinary global and `use ($argv)` /`` |
|         - | 5406 | `` * `fn() => $argv` are legal there, so screening it would reject valid php.`` |
|         - | 5407 | ` */` |
|      3730 | 5408 | `PH7_PRIVATE int PH7_VmIsAutoGlobal(const char *zName,sxu32 nByte)` |
|         5 | 5409 | `{` |
|         - | 5410 | `	static const char *const azAuto[] = {` |
|         - | 5411 | `		"GLOBALS", "_SERVER", "_GET", "_POST", "_FILES",` |
|         - | 5412 | `		"_COOKIE", "_SESSION", "_REQUEST", "_ENV", "_HEADER"` |
|         - | 5413 | `	};` |
|         - | 5414 | `	sxu32 n;` |
|     40497 | 5415 | `	for( n = 0 ; n < SX_ARRAYSIZE(azAuto) ; ++n ){` |
|     36821 | 5416 | `		sxu32 nLen = (sxu32)SyStrlen(azAuto[n]);` |
|     36821 | 5417 | `		if( nLen == nByte && SyMemcmp(azAuto[n],zName,nByte) == 0 ){` |
|        56 | 5418 | `			return 1;` |
|         - | 5419 | `		}` |
|     18386 | 5420 | `	}` |
|      3681 | 5421 | `	return 0;` |
|      1870 | 5422 | `}` |
|     16962 | 5423 | `PH7_PRIVATE int VmLocalExecThrew(ph7_vm *pVm,sxi32 rc,const void *pResumeBefore,const void *pInlineBefore)` |
|         5 | 5424 | `{` |
|     16944 | 5425 | `	return rc == PH7_EXCEPTION` |
|     16939 | 5426 | `		\|\| (const void *)pVm->pResumeFrame != pResumeBefore` |
|     25420 | 5427 | `		\|\| (const void *)pVm->pInlineInstr != pInlineBefore;` |
|         5 | 5428 | `}` |
|         - | 5429 | `/*` |
|         - | 5430 | ` * Evaluate an attribute-argument bytecode with the attribute's DECLARING class` |
|         - | 5431 | `` * installed as the const-eval scope, so `self::`/`parent::`/`self::CONST` inside`` |
|         - | 5432 | ` * the argument resolve against that class (like php) rather than the reflection` |
|         - | 5433 | ` * machinery's scope. pDeclCls == 0 (a free function or global constant) leaves` |
|         - | 5434 | ` * the ambient scope untouched. Mirrors the property/const-initializer path.` |
|         - | 5435 | ` */` |
|       140 | 5436 | `PH7_PRIVATE sxi32 PH7_VmExecAttrArg(ph7_vm *pVm,SySet *pByteCode,ph7_class *pDeclCls,ph7_value *pResult)` |
|         1 | 5437 | `{` |
|       141 | 5438 | `	ph7_class *pSaveCtx = pVm->pConstEvalClass;` |
|       141 | 5439 | `	void *pSaveFrame = pVm->pConstEvalFrame;` |
|         - | 5440 | `	sxi32 rc;` |
|       141 | 5441 | `	if( pDeclCls ){` |
|       125 | 5442 | `		pVm->pConstEvalClass = pDeclCls;` |
|       125 | 5443 | `		pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pVm->pFrame);` |
|        62 | 5444 | `	}` |
|       141 | 5445 | `	rc = VmLocalExec(&(*pVm),pByteCode,pResult,FALSE);` |
|       141 | 5446 | `	pVm->pConstEvalClass = pSaveCtx;` |
|       141 | 5447 | `	pVm->pConstEvalFrame = pSaveFrame;` |
|       141 | 5448 | `	return rc;` |
|         1 | 5449 | `}` |
|         - | 5450 | `/*` |
|         - | 5451 | ` * Flush every still-open output buffer at the end of execution. php implicitly` |
|         - | 5452 | ` * ends+flushes all ob_start() levels on shutdown (normal end, exit()/die(), or` |
|         - | 5453 | ` * fatal); PHL used to DISCARD them, so a script that never called ob_end_flush()` |
|         - | 5454 | ` * — e.g. PHPUnit, which buffers its result summary and then exit()s with a` |
|         - | 5455 | ` * non-zero status — lost that output entirely.` |
|         - | 5456 | ` *` |
|         - | 5457 | ` * PH7_VmObFlushAll() does it the way php does: one FINAL operation per buffer,` |
|         - | 5458 | ` * innermost first, so each handler's answer is what the buffer under it is` |
|         - | 5459 | ` * handed.` |
|         - | 5460 | ` */` |
|      4670 | 5461 | `static void VmFlushOutputBuffers(ph7_vm *pVm)` |
|         5 | 5462 | `{` |
|      4675 | 5463 | `	PH7_VmObFlushAll(&(*pVm));` |
|      4675 | 5464 | `}` |
|         - | 5465 | `/*` |
|         - | 5466 | ` * Shutdown callbacks are kept in a stack and are registered using one` |
|         - | 5467 | ` * or more calls to [register_shutdown_function()].` |
|         - | 5468 | ` * These callbacks are invoked by the virtual machine when the program` |
|         - | 5469 | ` * execution ends.` |
|         - | 5470 | ` * Refer to the implementation of [register_shutdown_function()] for` |
|         - | 5471 | ` * additional information.` |
|         - | 5472 | ` */` |
|      4670 | 5473 | `static void VmInvokeShutdownCallbacks(ph7_vm *pVm)` |
|         5 | 5474 | `{` |
|         - | 5475 | `	VmShutdownCB *pEntry;` |
|         - | 5476 | `	ph7_value *apArg[10];` |
|         - | 5477 | `	sxu32 n,nEntry;` |
|         - | 5478 | `	int i;` |
|         - | 5479 | `	/* Point to the stack of registered callbacks */` |
|      4675 | 5480 | `	nEntry = SySetUsed(&pVm->aShutdown);` |
|     51375 | 5481 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(apArg) ; i++ ){` |
|     46705 | 5482 | `		apArg[i] = 0;` |
|     23355 | 5483 | `	}` |
|         - | 5484 | `	/* A halt that led us here is consumed; a fresh one set by a callback` |
|         - | 5485 | `	 * (i.e. exit() inside a shutdown function) skips the remaining` |
|         - | 5486 | `	 * callbacks, mirroring PHP.` |
|         - | 5487 | `	 */` |
|      4675 | 5488 | `	pVm->bHaltRequested = 0;` |
|      4697 | 5489 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|        27 | 5490 | `		pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5491 | `		if( pEntry ){` |
|         - | 5492 | `			/* Prepare callback arguments if any */` |
|        27 | 5493 | `			for( i = 0 ; i < pEntry->nArg ; i++ ){` |
|       ! 0 | 5494 | `				if( i >= (int)SX_ARRAYSIZE(apArg) ){` |
|       ! 0 | 5495 | `					break;` |
|         - | 5496 | `				}` |
|       ! 0 | 5497 | `				apArg[i] = &pEntry->aArg[i];` |
|       ! 0 | 5498 | `			}` |
|         - | 5499 | `			/* Invoke the callback */` |
|        27 | 5500 | `			PH7_VmCallUserFunction(&(*pVm),&pEntry->sCallback,pEntry->nArg,apArg,0);` |
|         - | 5501 | `			/*` |
|         - | 5502 | `			 * TICKET 1433-56: Try re-access the same entry since the invoked` |
|         - | 5503 | `			 * callback may call [register_shutdown_function()] in it's body.` |
|         - | 5504 | `			 */` |
|        27 | 5505 | `			pEntry = (VmShutdownCB *)SySetAt(&pVm->aShutdown,n);` |
|        27 | 5506 | `			if( pEntry ){` |
|        27 | 5507 | `				PH7_MemObjRelease(&pEntry->sCallback);` |
|        27 | 5508 | `				for( i = 0 ; i < pEntry->nArg ; ++i ){` |
|       ! 0 | 5509 | `					PH7_MemObjRelease(apArg[i]);` |
|       ! 0 | 5510 | `				}` |
|        11 | 5511 | `			}` |
|        27 | 5512 | `			if( pVm->bHaltRequested ){` |
|         - | 5513 | `				/* exit() inside the callback: skip the remaining callbacks */` |
|       ! 0 | 5514 | `				break;` |
|         - | 5515 | `			}` |
|        11 | 5516 | `		}` |
|        16 | 5517 | `	}` |
|      4675 | 5518 | `	SySetReset(&pVm->aShutdown);` |
|      4675 | 5519 | `}` |
|         - | 5520 | `/*` |
|         - | 5521 | ` * Execute as much of a PH7 bytecode program as we can then return.` |
|         - | 5522 | ` * This function is a wrapper around [VmByteCodeExec()].` |
|         - | 5523 | ` * See block-comment on that function for additional information.` |
|         - | 5524 | ` */` |
|      4670 | 5525 | `PH7_PRIVATE sxi32 PH7_VmByteCodeExec(ph7_vm *pVm)` |
|         5 | 5526 | `{` |
|         - | 5527 | `	/* Make sure we are ready to execute this program */` |
|      4675 | 5528 | `	if( pVm->nMagic != PH7_VM_RUN ){` |
|       ! 0 | 5529 | `		return pVm->nMagic == PH7_VM_EXEC ? SXERR_LOCKED /* Locked VM */ : SXERR_CORRUPT; /* Stale VM */` |
|         - | 5530 | `	}` |
|         - | 5531 | `	/* Set the execution magic number  */` |
|      4675 | 5532 | `	pVm->nMagic = PH7_VM_EXEC;` |
|         - | 5533 | `	/* Execute the program. A top-level OP_SPREAD may realloc the operand stack;` |
|         - | 5534 | `	 * pass &pVm->aOps so the growth updates the field that VM release frees. */` |
|         - | 5535 | `	{` |
|      4675 | 5536 | `		sxu32 nOpsCap = SySetUsed(pVm->pByteContainer) + VM_STACK_GUARD;` |
|      4675 | 5537 | `		VmByteCodeExec(&(*pVm),(VmInstr *)SySetBasePtr(pVm->pByteContainer),pVm->aOps,-1,&pVm->sExec,0,FALSE,0,0,FALSE,0,&pVm->aOps,&nOpsCap,nOpsCap);` |
|         - | 5538 | `	}` |
|         - | 5539 | `	/* Invoke any shutdown callbacks */` |
|      4675 | 5540 | `	VmInvokeShutdownCallbacks(&(*pVm));` |
|         - | 5541 | `	/* php flushes every still-open output buffer on shutdown — after the` |
|         - | 5542 | `	 * shutdown callbacks, which may still write into them. */` |
|      4675 | 5543 | `	VmFlushOutputBuffers(&(*pVm));` |
|         - | 5544 | `	/* An open session is written back LAST, from php's own module shutdown: after` |
|         - | 5545 | `	 * the script's shutdown callbacks (which may still write to $_SESSION) and` |
|         - | 5546 | `	 * after the buffers are flushed (which is why its diagnostics land outside` |
|         - | 5547 | `	 * them). */` |
|      4675 | 5548 | `	PH7_VmSessionShutdown(&(*pVm));` |
|         - | 5549 | `	/*` |
|         - | 5550 | `	 * TICKET 1433-100: Do not remove the PH7_VM_EXEC magic number` |
|         - | 5551 | `	 * so that any following call to [ph7_vm_exec()] without calling` |
|         - | 5552 | `	 * [ph7_vm_reset()] first would fail.` |
|         - | 5553 | `	 */` |
|      4675 | 5554 | `	return SXRET_OK;` |
|      2340 | 5555 | `}` |
|         - | 5556 | `/* ======================== Fiber Infrastructure ======================== */` |
|         - | 5557 | `/*` |
|         - | 5558 | ` * Invoke the installed VM output consumer callback to consume` |
|         - | 5559 | ` * the desired message.` |
|         - | 5560 | ` * Refer to the implementation of [ph7_context_output()] defined` |
|         - | 5561 | ` * in 'api.c' for additional information.` |
|         - | 5562 | ` */` |
|     38256 | 5563 | `PH7_PRIVATE sxi32 PH7_VmOutputConsume(` |
|         - | 5564 | `	ph7_vm *pVm,      /* Target VM */` |
|         - | 5565 | `	SyString *pString /* Message to output */` |
|         - | 5566 | `	)` |
|         5 | 5567 | `{` |
|     38261 | 5568 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|     38261 | 5569 | `	sxi32 rc = SXRET_OK;` |
|         - | 5570 | `	/* Call the output consumer */` |
|     38261 | 5571 | `	if( pString->nByte > 0 ){` |
|     38261 | 5572 | `		rc = pCons->xConsumer((const void *)pString->zString,pString->nByte,pCons->pUserData);` |
|     38261 | 5573 | `		VmTrackOutput(pVm, pString->nByte);` |
|     19128 | 5574 | `	}` |
|     38261 | 5575 | `	return rc;` |
|         5 | 5576 | `}` |
|         - | 5577 | `/*` |
|         - | 5578 | ` * Format a message and invoke the installed VM output consumer` |
|         - | 5579 | ` * callback to consume the formatted message.` |
|         - | 5580 | ` * Refer to the implementation of [ph7_context_output_format()] defined` |
|         - | 5581 | ` * in 'api.c' for additional information.` |
|         - | 5582 | ` */` |
|        30 | 5583 | `PH7_PRIVATE sxi32 PH7_VmOutputConsumeAp(` |
|         - | 5584 | `	ph7_vm *pVm,         /* Target VM */` |
|         - | 5585 | `	const char *zFormat, /* Formatted message to output */` |
|         - | 5586 | `	va_list ap           /* Variable list of arguments */` |
|         - | 5587 | `	)` |
|         2 | 5588 | `{` |
|        32 | 5589 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|        32 | 5590 | `	sxi32 rc = SXRET_OK;` |
|         - | 5591 | `	SyBlob sWorker;` |
|         - | 5592 | `	/* Format the message and call the output consumer */` |
|        32 | 5593 | `	SyBlobInit(&sWorker,&pVm->sAllocator);` |
|        32 | 5594 | `	SyBlobFormatAp(&sWorker,zFormat,ap);` |
|        32 | 5595 | `	if( SyBlobLength(&sWorker) > 0 ){` |
|         - | 5596 | `		/* Consume the formatted message */` |
|        32 | 5597 | `		rc = pCons->xConsumer(SyBlobData(&sWorker),SyBlobLength(&sWorker),pCons->pUserData);` |
|        15 | 5598 | `	}` |
|        32 | 5599 | `	VmTrackOutput(pVm, SyBlobLength(&sWorker));` |
|         - | 5600 | `	/* Release the working buffer */` |
|        32 | 5601 | `	SyBlobRelease(&sWorker);` |
|        32 | 5602 | `	return rc;` |
|         2 | 5603 | `}` |
|         - | 5604 | `/*` |
|         - | 5605 | ` * Return a string representation of the given PH7 OP code.` |
|         - | 5606 | ` * This function never fail and always return a pointer` |
|         - | 5607 | ` * to a null terminated string.` |
|         - | 5608 | ` */` |
|        10 | 5609 | `static const char * VmInstrToString(sxi32 nOp)` |
|         1 | 5610 | `{` |
|        11 | 5611 | `	const char *zOp = "Unknown     ";` |
|        11 | 5612 | `	switch(nOp){` |
|         3 | 5613 | `	case PH7_OP_DONE:       zOp = "DONE       "; break;` |
|       ! 0 | 5614 | `	case PH7_OP_HALT:       zOp = "HALT       "; break;` |
|       ! 0 | 5615 | `	case PH7_OP_LOAD:       zOp = "LOAD       "; break;` |
|         5 | 5616 | `	case PH7_OP_LOADC:      zOp = "LOADC      "; break;` |
|       ! 0 | 5617 | `	case PH7_OP_LOAD_MAP:   zOp = "LOAD_MAP   "; break;` |
|       ! 0 | 5618 | `	case PH7_OP_LOAD_LIST:  zOp = "LOAD_LIST  "; break;` |
|       ! 0 | 5619 | `	case PH7_OP_LOAD_IDX:   zOp = "LOAD_IDX   "; break;` |
|       ! 0 | 5620 | `	case PH7_OP_LOAD_CLOSURE:` |
|       ! 0 | 5621 | `		                    zOp = "LOAD_CLOSR "; break;` |
|       ! 0 | 5622 | `	case PH7_OP_LOAD_FCC:` |
|       ! 0 | 5623 | `		                    zOp = "LOAD_FCC   "; break;` |
|       ! 0 | 5624 | `	case PH7_OP_NOOP:       zOp = "NOOP       "; break;` |
|       ! 0 | 5625 | `	case PH7_OP_JMP:        zOp = "JMP        "; break;` |
|       ! 0 | 5626 | `	case PH7_OP_JZ:         zOp = "JZ         "; break;` |
|       ! 0 | 5627 | `	case PH7_OP_JNZ:        zOp = "JNZ        "; break;` |
|       ! 0 | 5628 | `	case PH7_OP_POP:        zOp = "POP        "; break;` |
|       ! 0 | 5629 | `	case PH7_OP_CAT:        zOp = "CAT        "; break;` |
|       ! 0 | 5630 | `	case PH7_OP_CVT_INT:    zOp = "CVT_INT    "; break;` |
|       ! 0 | 5631 | `	case PH7_OP_CVT_STR:    zOp = "CVT_STR    "; break;` |
|       ! 0 | 5632 | `	case PH7_OP_CVT_REAL:   zOp = "CVT_REAL   "; break;` |
|       ! 0 | 5633 | `	case PH7_OP_CALL:       zOp = "CALL       "; break;` |
|       ! 0 | 5634 | `	case PH7_OP_ROT_CALLEE: zOp = "ROT_CALLEE "; break;` |
|       ! 0 | 5635 | `	case PH7_OP_CALL_INIT:  zOp = "CALL_INIT  "; break;` |
|       ! 0 | 5636 | `	case PH7_OP_UMINUS:     zOp = "UMINUS     "; break;` |
|       ! 0 | 5637 | `	case PH7_OP_UPLUS:      zOp = "UPLUS      "; break;` |
|       ! 0 | 5638 | `	case PH7_OP_BITNOT:     zOp = "BITNOT     "; break;` |
|       ! 0 | 5639 | `	case PH7_OP_LNOT:       zOp = "LOGNOT     "; break;` |
|       ! 0 | 5640 | `	case PH7_OP_MUL:        zOp = "MUL        "; break;` |
|       ! 0 | 5641 | `	case PH7_OP_DIV:        zOp = "DIV        "; break;` |
|       ! 0 | 5642 | `	case PH7_OP_MOD:        zOp = "MOD        "; break;` |
|       ! 0 | 5643 | `	case PH7_OP_ADD:        zOp = "ADD        "; break;` |
|       ! 0 | 5644 | `	case PH7_OP_SUB:        zOp = "SUB        "; break;` |
|       ! 0 | 5645 | `	case PH7_OP_SHL:        zOp = "SHL        "; break;` |
|       ! 0 | 5646 | `	case PH7_OP_SHR:        zOp = "SHR        "; break;` |
|       ! 0 | 5647 | `	case PH7_OP_LT:         zOp = "LT         "; break;` |
|       ! 0 | 5648 | `	case PH7_OP_LE:         zOp = "LE         "; break;` |
|       ! 0 | 5649 | `	case PH7_OP_GT:         zOp = "GT         "; break;` |
|       ! 0 | 5650 | `	case PH7_OP_GE:         zOp = "GE         "; break;` |
|       ! 0 | 5651 | `	case PH7_OP_SPACESHIP:  zOp = "SPACESHIP  "; break;` |
|       ! 0 | 5652 | `	case PH7_OP_EQ:         zOp = "EQ         "; break;` |
|       ! 0 | 5653 | `	case PH7_OP_NEQ:        zOp = "NEQ        "; break;` |
|       ! 0 | 5654 | `	case PH7_OP_TEQ:        zOp = "TEQ        "; break;` |
|       ! 0 | 5655 | `	case PH7_OP_TNE:        zOp = "TNE        "; break;` |
|       ! 0 | 5656 | `	case PH7_OP_BAND:       zOp = "BITAND     "; break;` |
|       ! 0 | 5657 | `	case PH7_OP_BXOR:       zOp = "BITXOR     "; break;` |
|       ! 0 | 5658 | `	case PH7_OP_BOR:        zOp = "BITOR      "; break;` |
|       ! 0 | 5659 | `	case PH7_OP_LAND:       zOp = "LOGAND     "; break;` |
|       ! 0 | 5660 | `	case PH7_OP_LOR:        zOp = "LOGOR      "; break;` |
|       ! 0 | 5661 | `	case PH7_OP_LXOR:       zOp = "LOGXOR     "; break;` |
|       ! 0 | 5662 | `	case PH7_OP_STORE:      zOp = "STORE      "; break;` |
|       ! 0 | 5663 | `	case PH7_OP_STORE_IDX:  zOp = "STORE_IDX  "; break;` |
|       ! 0 | 5664 | `	case PH7_OP_STORE_IDX_REF:` |
|       ! 0 | 5665 | `		                    zOp = "STORE_IDX_R"; break;` |
|       ! 0 | 5666 | `	case PH7_OP_PULL:       zOp = "PULL       "; break;` |
|       ! 0 | 5667 | `	case PH7_OP_DUP:        zOp = "DUP        "; break;` |
|       ! 0 | 5668 | `	case PH7_OP_SWAP:       zOp = "SWAP       "; break;` |
|       ! 0 | 5669 | `	case PH7_OP_YIELD:      zOp = "YIELD      "; break;` |
|       ! 0 | 5670 | `	case PH7_OP_YIELD_FROM: zOp = "YIELD_FROM "; break;` |
|       ! 0 | 5671 | `	case PH7_OP_NULLC:      zOp = "NULLC      "; break;` |
|       ! 0 | 5672 | `	case PH7_OP_NULLC_JMP:  zOp = "NULLC_JMP  "; break;` |
|       ! 0 | 5673 | `	case PH7_OP_NULLC_STORE:zOp = "NULLC_STORE"; break;` |
|       ! 0 | 5674 | `	case PH7_OP_NULLSAFE_JMP:zOp = "NULLSAFE_JMP"; break;` |
|       ! 0 | 5675 | `	case PH7_OP_SPREAD:     zOp = "SPREAD     "; break;` |
|       ! 0 | 5676 | `	case PH7_OP_FLAG_SPREAD:zOp = "FLAG_SPREAD"; break;` |
|       ! 0 | 5677 | `	case PH7_OP_CVT_BOOL:   zOp = "CVT_BOOL   "; break;` |
|       ! 0 | 5678 | `	case PH7_OP_CVT_NULL:   zOp = "CVT_NULL   "; break;` |
|       ! 0 | 5679 | `	case PH7_OP_CVT_ARRAY:  zOp = "CVT_ARRAY  "; break;` |
|       ! 0 | 5680 | `	case PH7_OP_CVT_OBJ:    zOp = "CVT_OBJ    "; break;` |
|       ! 0 | 5681 | `	case PH7_OP_CVT_NUMC:   zOp = "CVT_NUMC   "; break;` |
|       ! 0 | 5682 | `	case PH7_OP_INCR:       zOp = "INCR       "; break;` |
|       ! 0 | 5683 | `	case PH7_OP_DECR:       zOp = "DECR       "; break;` |
|       ! 0 | 5684 | `	case PH7_OP_NEW:        zOp = "NEW        "; break;` |
|       ! 0 | 5685 | `	case PH7_OP_CLONE:      zOp = "CLONE      "; break;` |
|       ! 0 | 5686 | `	case PH7_OP_ADD_STORE:  zOp = "ADD_STORE  "; break;` |
|       ! 0 | 5687 | `	case PH7_OP_SUB_STORE:  zOp = "SUB_STORE  "; break;` |
|       ! 0 | 5688 | `	case PH7_OP_MUL_STORE:  zOp = "MUL_STORE  "; break;` |
|       ! 0 | 5689 | `	case PH7_OP_DIV_STORE:  zOp = "DIV_STORE  "; break;` |
|       ! 0 | 5690 | `	case PH7_OP_MOD_STORE:  zOp = "MOD_STORE  "; break;` |
|       ! 0 | 5691 | `	case PH7_OP_CAT_STORE:  zOp = "CAT_STORE  "; break;` |
|       ! 0 | 5692 | `	case PH7_OP_SHL_STORE:  zOp = "SHL_STORE  "; break;` |
|       ! 0 | 5693 | `	case PH7_OP_SHR_STORE:  zOp = "SHR_STORE  "; break;` |
|       ! 0 | 5694 | `	case PH7_OP_BAND_STORE: zOp = "BAND_STORE "; break;` |
|       ! 0 | 5695 | `	case PH7_OP_BOR_STORE:  zOp = "BOR_STORE  "; break;` |
|       ! 0 | 5696 | `	case PH7_OP_BXOR_STORE: zOp = "BXOR_STORE "; break;` |
|         5 | 5697 | `	case PH7_OP_CONSUME:    zOp = "CONSUME    "; break;` |
|       ! 0 | 5698 | `	case PH7_OP_LOAD_REF:   zOp = "LOAD_REF   "; break;` |
|       ! 0 | 5699 | `	case PH7_OP_STORE_REF:  zOp = "STORE_REF  "; break;` |
|       ! 0 | 5700 | `	case PH7_OP_MEMBER:     zOp = "MEMBER     "; break;` |
|       ! 0 | 5701 | `	case PH7_OP_UPLINK:     zOp = "UPLINK     "; break;` |
|       ! 0 | 5702 | `	case PH7_OP_ERR_CTRL:   zOp = "ERR_CTRL   "; break;` |
|       ! 0 | 5703 | `	case PH7_OP_UNSET_VAR:  zOp = "UNSET_VAR  "; break;` |
|       ! 0 | 5704 | `	case PH7_OP_IS_A:       zOp = "IS_A       "; break;` |
|       ! 0 | 5705 | `	case PH7_OP_SWITCH:     zOp = "SWITCH     "; break;` |
|       ! 0 | 5706 | `	case PH7_OP_MATCH:      zOp = "MATCH      "; break;` |
|       ! 0 | 5707 | `	case PH7_OP_CLASS_DEFER:zOp = "CLASS_DEFER"; break;` |
|       ! 0 | 5708 | `	case PH7_OP_LOAD_EXCEPTION:` |
|       ! 0 | 5709 | `		                    zOp = "LOAD_EXCEP "; break;` |
|       ! 0 | 5710 | `	case PH7_OP_POP_EXCEPTION:` |
|       ! 0 | 5711 | `		                    zOp = "POP_EXCEP  "; break;` |
|       ! 0 | 5712 | `	case PH7_OP_THROW:      zOp = "THROW      "; break;` |
|       ! 0 | 5713 | `	case PH7_OP_FOREACH_INIT:` |
|       ! 0 | 5714 | `		                    zOp = "4EACH_INIT "; break;` |
|       ! 0 | 5715 | `	case PH7_OP_FOREACH_STEP:` |
|       ! 0 | 5716 | `						    zOp = "4EACH_STEP "; break;` |
|       ! 0 | 5717 | `	default:` |
|       ! 0 | 5718 | `		break;` |
|         - | 5719 | `	}` |
|        11 | 5720 | `	return zOp;` |
|         1 | 5721 | `}` |
|         - | 5722 | `/*` |
|         - | 5723 | ` * Dump PH7 bytecodes instructions to a human readable format.` |
|         - | 5724 | ` * The xConsumer() callback which is an used defined function` |
|         - | 5725 | ` * is responsible of consuming the generated dump.` |
|         - | 5726 | ` */` |
|         2 | 5727 | `PH7_PRIVATE sxi32 PH7_VmDump(` |
|         - | 5728 | `	ph7_vm *pVm,            /* Target VM */` |
|         - | 5729 | `	ProcConsumer xConsumer, /* Output [i.e: dump] consumer callback */` |
|         - | 5730 | `	void *pUserData         /* Last argument to xConsumer() */` |
|         - | 5731 | `	)` |
|         1 | 5732 | `{` |
|         - | 5733 | `	sxi32 rc;` |
|         3 | 5734 | `	rc = VmByteCodeDump(pVm->pByteContainer,xConsumer,pUserData);` |
|         3 | 5735 | `	return rc;` |
|         1 | 5736 | `}` |
|         - | 5737 | `/*` |
|         - | 5738 | ` * Default constant expansion callback used by the 'const' statement if used` |
|         - | 5739 | ` * outside a class body [i.e: global or function scope].` |
|         - | 5740 | ` * Refer to the implementation of [PH7_CompileConstant()] defined` |
|         - | 5741 | ` * in 'compile.c' for additional information.` |
|         - | 5742 | ` */` |
|         2 | 5743 | `PH7_PRIVATE void PH7_VmExpandConstantValue(ph7_value *pVal,void *pUserData)` |
|         1 | 5744 | `{` |
|         3 | 5745 | `	SySet *pByteCode = (SySet *)pUserData;` |
|         - | 5746 | `	/* Evaluate and expand constant value */` |
|         3 | 5747 | `	VmLocalExec((ph7_vm *)SySetGetUserData(pByteCode),pByteCode,(ph7_value *)pVal,FALSE);` |
|         3 | 5748 | `}` |
|         - | 5749 | `/*` |
|         - | 5750 | ` * Section:` |
|         - | 5751 | ` *  Function handling functions.` |
|         - | 5752 | ` * Status:` |
|         - | 5753 | ` *    Stable.` |
|         - | 5754 | ` */` |
|         - | 5755 | `/* call_user_func and call_user_func_array moved to vm_builtin_class.c */` |
|         - | 5756 | `static const ph7_builtin_func aVmFunc[] = {` |
|         - | 5757 | `	{ "enum_exists",        vm_builtin_enum_exists },` |
|         - | 5758 | `	{ "func_num_args"  , vm_builtin_func_num_args },` |
|         - | 5759 | `	{ "func_get_arg"   , vm_builtin_func_get_arg  },` |
|         - | 5760 | `	{ "func_get_args"  , vm_builtin_func_get_args },` |
|         - | 5761 | `	{ "func_get_args_byref" , vm_builtin_func_get_args_byref },` |
|         - | 5762 | `	{ "function_exists", vm_builtin_func_exists   },` |
|         - | 5763 | `	{ "is_callable"    , vm_builtin_is_callable   },` |
|         - | 5764 | `	{ "get_defined_functions", vm_builtin_get_defined_func },` |
|         - | 5765 | `	{ "register_shutdown_function",vm_builtin_register_shutdown_function },` |
|         - | 5766 | `	{ "call_user_func",        vm_builtin_call_user_func   },` |
|         - | 5767 | `	{ "call_user_func_array",  vm_builtin_call_user_func_array    },` |
|         - | 5768 | `	{ "forward_static_call",   vm_builtin_call_user_func   },` |
|         - | 5769 | `	{ "forward_static_call_array",vm_builtin_call_user_func_array },` |
|         - | 5770 | `	    /* Constants management */` |
|         - | 5771 | `	{ "defined",  vm_builtin_defined              },` |
|         - | 5772 | `	{ "define",   vm_builtin_define               },` |
|         - | 5773 | `	{ "constant", vm_builtin_constant             },` |
|         - | 5774 | `	{ "get_defined_constants", vm_builtin_get_defined_constants },` |
|         - | 5775 | `	   /* Class/Object functions */` |
|         - | 5776 | `	{ "class_alias",     vm_builtin_class_alias       },` |
|         - | 5777 | `	{ "class_exists",    vm_builtin_class_exists      },` |
|         - | 5778 | `	{ "property_exists", vm_builtin_property_exists   },` |
|         - | 5779 | `	{ "method_exists",   vm_builtin_method_exists     },` |
|         - | 5780 | `	{ "interface_exists",vm_builtin_interface_exists  },` |
|         - | 5781 | `	{ "trait_exists",    vm_builtin_trait_exists      },` |
|         - | 5782 | `	{ "get_class",       vm_builtin_get_class         },` |
|         - | 5783 | `	{ "get_parent_class",vm_builtin_get_parent_class  },` |
|         - | 5784 | `	{ "get_called_class",vm_builtin_get_called_class  },` |
|         - | 5785 | `	{ "get_declared_classes",    vm_builtin_get_declared_classes   },` |
|         - | 5786 | `	{ "get_defined_classes",     vm_builtin_get_declared_classes    },` |
|         - | 5787 | `	{ "get_declared_interfaces", vm_builtin_get_declared_interfaces},` |
|         - | 5788 | `	{ "get_declared_traits",     vm_builtin_get_declared_traits    },` |
|         - | 5789 | `	{ "get_class_methods",       vm_builtin_get_class_methods },` |
|         - | 5790 | `	{ "get_class_vars",          vm_builtin_get_class_vars    },` |
|         - | 5791 | `	{ "get_object_vars",         vm_builtin_get_object_vars   },` |
|         - | 5792 | `	{ "get_mangled_object_vars", vm_builtin_get_mangled_object_vars },` |
|         - | 5793 | `	{ "is_subclass_of",          vm_builtin_is_subclass_of    },` |
|         - | 5794 | `	{ "is_a", vm_builtin_is_a },` |
|         - | 5795 | `	   /* php 8.5: clone is a real internal function (the clone-with call form) */` |
|         - | 5796 | `	{ "clone",           vm_builtin_clone             },` |
|         - | 5797 | `	   /* SPL object identity */` |
|         - | 5798 | `	{ "spl_object_id",   vm_builtin_spl_object_id   },` |
|         - | 5799 | `	{ "spl_object_hash", vm_builtin_spl_object_hash },` |
|         - | 5800 | `	   /* SPL Autoloading */` |
|         - | 5801 | `	{ "spl_autoload_register",   vm_builtin_spl_autoload_register   },` |
|         - | 5802 | `	{ "spl_autoload_unregister", vm_builtin_spl_autoload_unregister },` |
|         - | 5803 | `	{ "spl_autoload_functions",  vm_builtin_spl_autoload_functions  },` |
|         - | 5804 | `	{ "spl_autoload",            vm_builtin_spl_autoload            },` |
|         - | 5805 | `	   /* Random numbers/strings generators */` |
|         - | 5806 | `	{ "rand",          vm_builtin_rand            },` |
|         - | 5807 | `	{ "mt_rand",       vm_builtin_rand            },` |
|         - | 5808 | `	{ "rand_str",      vm_builtin_rand_str        },` |
|         - | 5809 | `	{ "getrandmax",    vm_builtin_getrandmax      },` |
|         - | 5810 | `	{ "mt_getrandmax", vm_builtin_getrandmax      },` |
|         - | 5811 | `	{ "random_int",    vm_builtin_random_int      },` |
|         - | 5812 | `	{ "random_bytes",  vm_builtin_random_bytes    },` |
|         - | 5813 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|         - | 5814 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|         - | 5815 | `	{ "uniqid",        vm_builtin_uniqid          },` |
|         - | 5816 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|         - | 5817 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|         - | 5818 | `	   /* Language constructs functions */` |
|         - | 5819 | `	{ "echo",  vm_builtin_echo                    },` |
|         - | 5820 | `	{ "print", vm_builtin_print                   },` |
|         - | 5821 | `	{ "exit",  vm_builtin_exit                    },` |
|         - | 5822 | `	{ "die",   vm_builtin_exit                    },` |
|         - | 5823 | `	{ "eval",  vm_builtin_eval                    },` |
|         - | 5824 | `	  /* Variable handling functions */` |
|         - | 5825 | `	{ "get_defined_vars",vm_builtin_get_defined_vars},` |
|         - | 5826 | `	{ "gettype",   vm_builtin_gettype              },` |
|         - | 5827 | `	{ "get_debug_type", vm_builtin_get_debug_type      },` |
|         - | 5828 | `	{ "settype",   vm_builtin_settype              },` |
|         - | 5829 | `	{ "get_resource_type", vm_builtin_get_resource_type},` |
|         - | 5830 | `	{ "get_resource_id", vm_builtin_get_resource_id},` |
|         - | 5831 | `	{ "isset",     vm_builtin_isset                },` |
|         - | 5832 | `	{ "unset",     vm_builtin_unset                },` |
|         - | 5833 | `	{ "var_dump",  vm_builtin_var_dump             },` |
|         - | 5834 | `	{ "print_r",   vm_builtin_print_r              },` |
|         - | 5835 | `	{ "var_export",vm_builtin_var_export           },` |
|         - | 5836 | `	  /* Ouput control functions */` |
|         - | 5837 | `	{ "flush",        vm_builtin_flush             },` |
|         - | 5838 | `	{ "ob_clean",     vm_builtin_ob_clean          },` |
|         - | 5839 | `	{ "ob_end_clean", vm_builtin_ob_end_clean      },` |
|         - | 5840 | `	{ "ob_end_flush", vm_builtin_ob_end_flush      },` |
|         - | 5841 | `	{ "ob_flush",     vm_builtin_ob_flush          },` |
|         - | 5842 | `	{ "ob_get_clean", vm_builtin_ob_get_clean      },` |
|         - | 5843 | `	{ "ob_get_contents", vm_builtin_ob_get_contents},` |
|         - | 5844 | `	{ "ob_get_flush",    vm_builtin_ob_get_flush   },` |
|         - | 5845 | `	{ "ob_get_length",   vm_builtin_ob_get_length  },` |
|         - | 5846 | `	{ "ob_get_level",    vm_builtin_ob_get_level   },` |
|         - | 5847 | `	{ "ob_implicit_flush", vm_builtin_ob_implicit_flush},` |
|         - | 5848 | `	{ "ob_get_status",     vm_builtin_ob_get_status },` |
|         - | 5849 | `	{ "ob_list_handlers",  vm_builtin_ob_list_handlers },` |
|         - | 5850 | `	{ "ob_start",          vm_builtin_ob_start     },` |
|         - | 5851 | `	  /* Assertion functions */` |
|         - | 5852 | `	{ "assert",          vm_builtin_assert         },` |
|         - | 5853 | `	  /* Error reporting functions */` |
|         - | 5854 | `	{ "trigger_error",vm_builtin_trigger_error     },` |
|         - | 5855 | `	{ "user_error",   vm_builtin_trigger_error     },` |
|         - | 5856 | `	{ "error_reporting",vm_builtin_error_reporting },` |
|         - | 5857 | `	{ "error_log",       vm_builtin_error_log      },` |
|         - | 5858 | `	{ "restore_exception_handler", vm_builtin_restore_exception_handler },` |
|         - | 5859 | `	{ "set_exception_handler",     vm_builtin_set_exception_handler     },` |
|         - | 5860 | `	{ "restore_error_handler", vm_builtin_restore_error_handler },` |
|         - | 5861 | `	{ "set_error_handler",vm_builtin_set_error_handler },` |
|         - | 5862 | `	{ "get_error_handler", vm_builtin_get_error_handler },` |
|         - | 5863 | `	{ "get_exception_handler", vm_builtin_get_exception_handler },` |
|         - | 5864 | `	{ "debug_backtrace",  vm_builtin_debug_backtrace},` |
|         - | 5865 | `	{ "error_get_last" ,  vm_builtin_error_get_last },` |
|         - | 5866 | `	{ "error_clear_last", vm_builtin_error_clear_last },` |
|         - | 5867 | `	{ "debug_print_backtrace", vm_builtin_debug_print_backtrace  },` |
|         - | 5868 | `	{ "debug_string_backtrace",vm_builtin_debug_string_backtrace },` |
|         - | 5869 | `	  /* Release info */` |
|         - | 5870 | `	{"ph7version",       vm_builtin_ph7_version  },` |
|         - | 5871 | `	{"phpversion",       vm_builtin_phpversion    },` |
|         - | 5872 | `	{"extension_loaded", vm_builtin_extension_loaded },` |
|         - | 5873 | `	{"get_loaded_extensions", vm_builtin_get_loaded_extensions },` |
|         - | 5874 | `	{"php_sapi_name",    vm_builtin_php_sapi_name },` |
|         - | 5875 | `	{"ph7credits",       vm_builtin_ph7_credits  },` |
|         - | 5876 | `	{"ph7info",          vm_builtin_ph7_credits  },` |
|         - | 5877 | `	{"ph7_info",         vm_builtin_ph7_credits  },` |
|         - | 5878 | `	{"phpinfo",          vm_builtin_ph7_credits  },` |
|         - | 5879 | `	{"ph7copyright",     vm_builtin_ph7_credits  },` |
|         - | 5880 | `	  /* hashmap */` |
|         - | 5881 | `	{"compact",          vm_builtin_compact       },` |
|         - | 5882 | `	{"extract",          vm_builtin_extract       },` |
|         - | 5883 | `	{"import_request_variables", vm_builtin_import_request_variables},` |
|         - | 5884 | `	  /* URL related function */` |
|         - | 5885 | `	{"parse_url",        vm_builtin_parse_url     },` |
|         - | 5886 | `	 /* Refer to 'builtin.c' for others string processing functions. */` |
|         - | 5887 | `	   /* Command line processing */` |
|         - | 5888 | `	{"getopt",         vm_builtin_getopt     },` |
|         - | 5889 | `	   /* JSON encoding/decoding */` |
|         - | 5890 | `	{"json_encode",    vm_builtin_json_encode },` |
|         - | 5891 | `	{"json_last_error",vm_builtin_json_last_error},` |
|         - | 5892 | `	{"json_last_error_msg",vm_builtin_json_last_error_msg},` |
|         - | 5893 | `	{"json_decode",    vm_builtin_json_decode },` |
|         - | 5894 | `	{"json_validate",  vm_builtin_json_validate },` |
|         - | 5895 | `	{"serialize",      vm_builtin_serialize },` |
|         - | 5896 | `	{"unserialize",    vm_builtin_unserialize },` |
|         - | 5897 | `	   /* Files/URI inclusion facility */` |
|         - | 5898 | `	{ "get_include_path",  vm_builtin_get_include_path },` |
|         - | 5899 | `	{ "set_include_path",  vm_builtin_set_include_path },` |
|         - | 5900 | `	{ "get_included_files",vm_builtin_get_included_files},` |
|         - | 5901 | `	{ "include",      vm_builtin_include          },` |
|         - | 5902 | `	{ "include_once", vm_builtin_include_once     },` |
|         - | 5903 | `	{ "require",      vm_builtin_require          },` |
|         - | 5904 | `	{ "require_once", vm_builtin_require_once     },` |
|         - | 5905 | `};` |
|         - | 5906 | `/*` |
|         - | 5907 | ` * Register the built-in VM functions defined above.` |
|         - | 5908 | ` */` |
|      4660 | 5909 | `static sxi32 VmRegisterSpecialFunction(ph7_vm *pVm)` |
|         5 | 5910 | `{` |
|         - | 5911 | `	sxi32 rc;` |
|         - | 5912 | `	sxu32 n;` |
|    591825 | 5913 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVmFunc) ; ++n ){` |
|         - | 5914 | `		/* Note that these special functions have access` |
|         - | 5915 | `		 * to the underlying virtual machine as their` |
|         - | 5916 | `		 * private data.` |
|         - | 5917 | `		 */` |
|    587165 | 5918 | `		rc = ph7_create_function(&(*pVm),aVmFunc[n].zName,aVmFunc[n].xFunc,&(*pVm));` |
|    587165 | 5919 | `		if( rc != SXRET_OK ){` |
|       ! 0 | 5920 | `			return rc;` |
|         - | 5921 | `		}` |
|    293585 | 5922 | `	}` |
|      4665 | 5923 | `	return SXRET_OK;` |
|      2335 | 5924 | `}` |
|         - | 5925 | `/*` |
|         - | 5926 | ` * Helper: Apply loadable filter to a class pointer.` |
|         - | 5927 | ` * Returns the first concrete (non-interface, non-abstract, non-trait) class` |
|         - | 5928 | ` * in the name collision chain, or NULL if none qualifies.` |
|         - | 5929 | ` */` |
|   8008485 | 5930 | `static ph7_class * VmFilterLoadableClass(ph7_class *pClass,sxi32 iLoadable)` |
|         5 | 5931 | `{` |
|   8008490 | 5932 | `	if( !iLoadable ){` |
|   5434721 | 5933 | `		return pClass;` |
|         - | 5934 | `	}` |
|   2573790 | 5935 | `	while(pClass){` |
|   2573774 | 5936 | `		if( (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) == 0 ){` |
|   2573758 | 5937 | `			return pClass;` |
|         - | 5938 | `		}` |
|        17 | 5939 | `		pClass = pClass->pNextName;` |
|         1 | 5940 | `	}` |
|        17 | 5941 | `	return 0;` |
|   4004246 | 5942 | `}` |
|         - | 5943 | `/*` |
|         - | 5944 | ` * Trigger the autoload mechanism for a class that was not found.` |
|         - | 5945 | ` * Iterates through registered spl_autoload callbacks, calling each one` |
|         - | 5946 | ` * with the class name. After each callback, checks if the class is now` |
|         - | 5947 | ` * registered in the VM's class table.` |
|         - | 5948 | ` * Returns a pointer to the class on success, NULL on failure.` |
|         - | 5949 | ` * Uses hAutoloadActive to prevent infinite recursion.` |
|         - | 5950 | ` */` |
|       454 | 5951 | `static ph7_class * VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         5 | 5952 | `{` |
|         - | 5953 | `	VmAutoloadCB *pEntry;` |
|         - | 5954 | `	ph7_value sArg,sResult;` |
|         - | 5955 | `	SyHashEntry *pHashEntry;` |
|         - | 5956 | `	ph7_class *pClass;` |
|         - | 5957 | `	sxu32 n,nEntry;` |
|       459 | 5958 | `	nEntry = SySetUsed(&pVm->aAutoload);` |
|       459 | 5959 | `	if( nEntry < 1 ){` |
|       317 | 5960 | `		return 0;` |
|         - | 5961 | `	}` |
|         - | 5962 | `	/* Reentrancy guard: check if this class is already being autoloaded */` |
|       147 | 5963 | `	if( SyHashGet(&pVm->hAutoloadActive,(const void *)zName,nByte) != 0 ){` |
|         3 | 5964 | `		return 0; /* Already in progress, prevent infinite recursion */` |
|         - | 5965 | `	}` |
|         - | 5966 | `	/* Mark this class as being autoloaded */` |
|       145 | 5967 | `	SyHashInsert(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|         - | 5968 | `	/* Prepare the class name argument */` |
|       145 | 5969 | `	PH7_MemObjInit(pVm,&sArg);` |
|       145 | 5970 | `	PH7_MemObjInit(pVm,&sResult);` |
|       145 | 5971 | `	PH7_MemObjStringAppend(&sArg,zName,nByte);` |
|       145 | 5972 | `	pClass = 0;` |
|       267 | 5973 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|         - | 5974 | `		ph7_value *apArg[1];` |
|       155 | 5975 | `		pEntry = (VmAutoloadCB *)SySetAt(&pVm->aAutoload,n);` |
|       155 | 5976 | `		if( pEntry == 0 ){` |
|       ! 0 | 5977 | `			continue;` |
|         - | 5978 | `		}` |
|       155 | 5979 | `		apArg[0] = &sArg;` |
|       155 | 5980 | `		if( PH7_VmCallUserFunction(pVm,&pEntry->sCallback,1,apArg,&sResult) != SXRET_OK ){` |
|         - | 5981 | `			/* Callback could not be invoked — skip to next autoloader */` |
|        25 | 5982 | `			continue;` |
|         - | 5983 | `		}` |
|         - | 5984 | `		/* Check if the class is now available */` |
|       133 | 5985 | `		pHashEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|       133 | 5986 | `		if( pHashEntry ){` |
|        33 | 5987 | `			pClass = VmFilterLoadableClass((ph7_class *)pHashEntry->pUserData,iLoadable);` |
|        33 | 5988 | `			if( pClass ){` |
|        33 | 5989 | `				break;` |
|         - | 5990 | `			}` |
|       ! 0 | 5991 | `		}` |
|        55 | 5992 | `	}` |
|       145 | 5993 | `	PH7_MemObjRelease(&sArg);` |
|       145 | 5994 | `	PH7_MemObjRelease(&sResult);` |
|         - | 5995 | `	/* Remove reentrancy guard */` |
|       145 | 5996 | `	SyHashDeleteEntry(&pVm->hAutoloadActive,(const void *)zName,nByte,0);` |
|       145 | 5997 | `	return pClass;` |
|       232 | 5998 | `}` |
|         - | 5999 | `/*` |
|         - | 6000 | ` * Trigger autoload for external callers (e.g. class_exists).` |
|         - | 6001 | ` * Same as VmTriggerAutoload but exposed as PH7_PRIVATE.` |
|         - | 6002 | ` */` |
|        46 | 6003 | `PH7_PRIVATE ph7_class * PH7_VmTriggerAutoload(ph7_vm *pVm,const char *zName,sxu32 nByte,sxi32 iLoadable)` |
|         4 | 6004 | `{` |
|        50 | 6005 | `	return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         4 | 6006 | `}` |
|         - | 6007 | `/*` |
|         - | 6008 | ` * php: a leading '\' anchors a class/interface/trait/enum name to the global` |
|         - | 6009 | ` * namespace root. A stored name never carries one (the compiler qualifies to` |
|         - | 6010 | ` * "Foo\Bar", and a top-level class stores as "Foo"), so strip EXACTLY ONE` |
|         - | 6011 | ` * leading backslash before a hClass lookup — only one, since a literal` |
|         - | 6012 | ` * "\\Foo" names a non-global "\Foo" php also fails to find. Stripping a` |
|         - | 6013 | ` * lookup key is universally safe: no stored key begins with '\', so it can` |
|         - | 6014 | ` * only turn a failing lookup into a match, never break an existing one.` |
|         - | 6015 | `` * Shared by PH7_VmExtractClass (the central resolver — string `new`,`` |
|         - | 6016 | ` * is_a/is_subclass_of/method_exists via PH7_VmExtractClassFromValue,` |
|         - | 6017 | ` * enum_exists via VmExtractEnumClass, instanceof over a string) and the` |
|         - | 6018 | ` * *_exists()/class_alias() builtins that hash hClass directly.` |
|         - | 6019 | ` */` |
|   9562735 | 6020 | `PH7_PRIVATE void PH7_VmClassNameAnchor(const char **pzName,sxu32 *pnByte)` |
|         5 | 6021 | `{` |
|   9562740 | 6022 | `	if( *pnByte > 0 && (*pzName)[0] == '\\' ){` |
|        72 | 6023 | `		(*pzName)++;` |
|        72 | 6024 | `		(*pnByte)--;` |
|        34 | 6025 | `	}` |
|   9562740 | 6026 | `}` |
|         - | 6027 | `/*` |
|         - | 6028 | ` * Check if the given name refer to an installed class.` |
|         - | 6029 | ` * Return a pointer to that class on success. NULL on failure.` |
|         - | 6030 | ` */` |
|   8008871 | 6031 | `PH7_PRIVATE ph7_class * PH7_VmExtractClass(` |
|         - | 6032 | `	ph7_vm *pVm,        /* Target VM */` |
|         - | 6033 | `	const char *zName,  /* Name of the target class */` |
|         - | 6034 | `	sxu32 nByte,        /* zName length */` |
|         - | 6035 | `	sxi32 iLoadable,    /* TRUE to return only loadable class` |
|         - | 6036 | `						 * [i.e: no abstract classes or interfaces]` |
|         - | 6037 | `						 */` |
|         - | 6038 | `	sxi32 iNest         /* Nesting level (Not used) */` |
|         - | 6039 | `	)` |
|         5 | 6040 | `{` |
|         - | 6041 | `	SyHashEntry *pEntry;` |
|         - | 6042 | `	ph7_class *pClass;` |
|   4004434 | 6043 | `	SXUNUSED(iNest);` |
|         - | 6044 | `	/* Exact class lookup.` |
|         - | 6045 | `	 * Static names are already namespace-qualified by the compiler.` |
|         - | 6046 | `	 * Dynamic names (from variables) use exact match only, matching PHP behavior.` |
|         - | 6047 | `	 * A dynamic name may carry a leading '\' (the global-namespace anchor) — strip` |
|         - | 6048 | `	 * one so "\Foo" resolves to the stored "Foo" (php-exact; see the helper above). */` |
|   8008876 | 6049 | `	PH7_VmClassNameAnchor(&zName,&nByte);` |
|         - | 6050 | `	/* An empty stripped name names no class: neither a truly empty "" nor a lone` |
|         - | 6051 | `	 * "\" is looked up or handed to the autoloader (php 8.5.11, GH-23232). */` |
|   8008876 | 6052 | `	if( nByte < 1 ){` |
|         8 | 6053 | `		return 0;` |
|         - | 6054 | `	}` |
|   8008870 | 6055 | `	pEntry = SyHashGet(&pVm->hClass,(const void *)zName,nByte);` |
|   8008870 | 6056 | `	if( pEntry == 0 ){` |
|         - | 6057 | `		/* Class not found in hash table — try autoload before giving up */` |
|       413 | 6058 | `		return VmTriggerAutoload(pVm,zName,nByte,iLoadable);` |
|         - | 6059 | `	}` |
|   8008462 | 6060 | `	pClass = (ph7_class *)pEntry->pUserData;` |
|   8008462 | 6061 | `	return VmFilterLoadableClass(pClass,iLoadable);` |
|   4004439 | 6062 | `}` |
|         - | 6063 | `/*` |
|         - | 6064 | ` * Reference Table Implementation` |
|         - | 6065 | ` * Status: stable <chm@symisc.net>` |
|         - | 6066 | ` * Intro` |
|         - | 6067 | ` *  The implementation of the reference mechanism in the PH7 engine` |
|         - | 6068 | ` *  differ greatly from the one used by the zend engine. That is,` |
|         - | 6069 | ` *  the reference implementation is consistent,solid and it's` |
|         - | 6070 | ` *  behavior resemble the C++ reference mechanism.` |
|         - | 6071 | ` *  Refer to the official for more information on this powerful` |
|         - | 6072 | ` *  extension.` |
|         - | 6073 | ` */` |
|         - | 6074 | `/*` |
|         - | 6075 | ` * Allocate a new reference entry.` |
|         - | 6076 | ` */` |
|  19452972 | 6077 | `static VmRefObj * VmNewRefObj(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6078 | `{` |
|         - | 6079 | `	VmRefObj *pRef;` |
|         - | 6080 | `	/* Allocate a new instance */` |
|  19452977 | 6081 | `	pRef = (VmRefObj *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmRefObj));` |
|  19452977 | 6082 | `	if( pRef == 0 ){` |
|       ! 0 | 6083 | `		return 0;` |
|         - | 6084 | `	}` |
|         - | 6085 | `	/* Zero the structure */` |
|  19452977 | 6086 | `	SyZero(pRef,sizeof(VmRefObj));` |
|         - | 6087 | `	/* Initialize fields */` |
|  19452977 | 6088 | `	SySetInit(&pRef->aReference,&pVm->sAllocator,sizeof(SyHashEntry *));` |
|  19452977 | 6089 | `	SySetInit(&pRef->aArrEntries,&pVm->sAllocator,sizeof(ph7_hashmap_node *));` |
|  19452977 | 6090 | `	pRef->nIdx = nIdx;` |
|  19452977 | 6091 | `	return pRef;` |
|   9727809 | 6092 | `}` |
|         - | 6093 | `/*` |
|         - | 6094 | ` * Default hash function used by the reference table` |
|         - | 6095 | ` * for lookup/insertion operations.` |
|         - | 6096 | ` */` |
| 105170644 | 6097 | `static sxu32 VmRefHash(sxu32 nIdx)` |
|         5 | 6098 | `{` |
|         - | 6099 | `	/* Calculate the hash based on the memory object index */` |
| 105170649 | 6100 | `	return nIdx ^ (nIdx << 8) ^ (nIdx >> 8);` |
|         5 | 6101 | `}` |
|         - | 6102 | `/*` |
|         - | 6103 | ` * Check if a memory object [i.e: a variable] is already installed` |
|         - | 6104 | ` * in the reference table.` |
|         - | 6105 | ` * Return a pointer to the entry (VmRefObj instance) on success.NULL` |
|         - | 6106 | ` * otherwise.` |
|         - | 6107 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6108 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6109 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6110 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6111 | ` * Refer to the official for more information on this powerful` |
|         - | 6112 | ` * extension.` |
|         - | 6113 | ` */` |
|  64792898 | 6114 | `PH7_PRIVATE VmRefObj * VmRefObjExtract(ph7_vm *pVm,sxu32 nObjIdx)` |
|         5 | 6115 | `{` |
|         - | 6116 | `	VmRefObj *pRef;` |
|         - | 6117 | `	sxu32 nBucket;` |
|         - | 6118 | `	/* Point to the appropriate bucket */` |
|  64792903 | 6119 | `	nBucket = VmRefHash(nObjIdx) & (pVm->nRefSize - 1);` |
|         - | 6120 | `	/* Perform the lookup */` |
|  64792903 | 6121 | `	pRef = pVm->apRefObj[nBucket];` |
| 206636679 | 6122 | `	for(;;){` |
| 413249200 | 6123 | `		if( pRef == 0 ){` |
|  19453095 | 6124 | `			break;` |
|         - | 6125 | `		}` |
| 393796110 | 6126 | `		if( pRef->nIdx == nObjIdx ){` |
|         - | 6127 | `			/* Entry found */` |
|  45339813 | 6128 | `			return pRef;` |
|         - | 6129 | `		}` |
|         - | 6130 | `		/* Point to the next entry */` |
| 348456302 | 6131 | `		pRef = pRef->pNextCollide;` |
|         5 | 6132 | `	}` |
|         - | 6133 | `	/* No such entry,return NULL */` |
|  19453095 | 6134 | `	return 0;` |
|  32401783 | 6135 | `}` |
|         - | 6136 | `/*` |
|         - | 6137 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6138 | ` *` |
|         - | 6139 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6140 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6141 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6142 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6143 | ` * Refer to the official for more information on this powerful` |
|         - | 6144 | ` * extension.` |
|         - | 6145 | ` */` |
|  19452972 | 6146 | `static sxi32 VmRefObjInsert(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6147 | `{` |
|         - | 6148 | `	sxu32 nBucket;` |
|  19452977 | 6149 | `	if( pVm->nRefUsed * 3 >= pVm->nRefSize ){` |
|         - | 6150 | `		VmRefObj **apNew;` |
|         - | 6151 | `		sxu32 nNew;` |
|         - | 6152 | `		/* Allocate a larger table */` |
|     11956 | 6153 | `		nNew = pVm->nRefSize << 1;` |
|     11956 | 6154 | `		apNew = (VmRefObj **)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmRefObj *) * nNew);` |
|     11956 | 6155 | `		if( apNew ){` |
|     11956 | 6156 | `			VmRefObj *pEntry = pVm->pRefList;` |
|         - | 6157 | `			sxu32 n;` |
|         - | 6158 | `			/* Zero the structure */` |
|     11956 | 6159 | `			SyZero((void *)apNew,nNew * sizeof(VmRefObj *));` |
|         - | 6160 | `			/* Rehash all referenced entries */` |
|   4398491 | 6161 | `			for( n = 0 ; n < pVm->nRefUsed ; ++n ){` |
|         - | 6162 | `				/* Remove old collision links */` |
|   4386540 | 6163 | `				pEntry->pNextCollide = pEntry->pPrevCollide = 0;` |
|         - | 6164 | `				/* Point to the appropriate bucket */` |
|   4386540 | 6165 | `				nBucket = VmRefHash(pEntry->nIdx) & (nNew - 1);` |
|         - | 6166 | `				/* Insert the entry  */` |
|   4386540 | 6167 | `				pEntry->pNextCollide = apNew[nBucket];` |
|   4386540 | 6168 | `				if( apNew[nBucket] ){` |
|   3585243 | 6169 | `					apNew[nBucket]->pPrevCollide = pEntry;` |
|   1792619 | 6170 | `				}` |
|   4386540 | 6171 | `				apNew[nBucket] = pEntry;` |
|         - | 6172 | `				/* Point to the next entry */` |
|   4386540 | 6173 | `				pEntry = pEntry->pNext;` |
|   2193251 | 6174 | `			}` |
|         - | 6175 | `			/* Release the old table */` |
|     11956 | 6176 | `			SyMemBackendFree(&pVm->sAllocator,pVm->apRefObj);` |
|         - | 6177 | `			/* Install the new one */` |
|     11956 | 6178 | `			pVm->apRefObj = apNew;` |
|     11956 | 6179 | `			pVm->nRefSize = nNew;` |
|      5975 | 6180 | `		}` |
|      5975 | 6181 | `	}` |
|         - | 6182 | `	/* Point to the appropriate bucket */` |
|  19452977 | 6183 | `	nBucket = VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1);` |
|         - | 6184 | `	/* Insert the entry */` |
|  19452977 | 6185 | `	pRef->pNextCollide = pVm->apRefObj[nBucket];` |
|  19452977 | 6186 | `	if( pVm->apRefObj[nBucket] ){` |
|  17285556 | 6187 | `		pVm->apRefObj[nBucket]->pPrevCollide = pRef;` |
|   8644204 | 6188 | `	}` |
|  19452977 | 6189 | `	pVm->apRefObj[nBucket] = pRef;` |
|  19452977 | 6190 | `	MACRO_LD_PUSH(pVm->pRefList,pRef);` |
|  19452977 | 6191 | `	pVm->nRefUsed++;` |
|  19452977 | 6192 | `	return SXRET_OK;` |
|         5 | 6193 | `}` |
|         - | 6194 | `/*` |
|         - | 6195 | ` * Destroy a memory object [i.e: a variable] and remove it from` |
|         - | 6196 | ` * the reference table.` |
|         - | 6197 | ` * This function is invoked when the user perform an unset` |
|         - | 6198 | ` * call [i.e: unset($var); ].` |
|         - | 6199 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6200 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6201 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6202 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6203 | ` * Refer to the official for more information on this powerful` |
|         - | 6204 | ` * extension.` |
|         - | 6205 | ` */` |
|  18529154 | 6206 | `PH7_PRIVATE sxi32 VmRefObjUnlink(ph7_vm *pVm,VmRefObj *pRef)` |
|         5 | 6207 | `{` |
|         - | 6208 | `	ph7_hashmap_node **apNode;` |
|         - | 6209 | `	SyHashEntry **apEntry;` |
|         - | 6210 | `	sxu32 n;` |
|         - | 6211 | `	/* Point to the reference table */` |
|  18529159 | 6212 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  18529159 | 6213 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|         - | 6214 | `	/* Unlink the entry from the reference table */` |
|  19697939 | 6215 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1168785 | 6216 | `		if( apEntry[n] ){` |
|       175 | 6217 | `			SyHashDeleteEntry2(apEntry[n]);` |
|        87 | 6218 | `		}` |
|    585706 | 6219 | `	}` |
|  26368308 | 6220 | `	for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   7839154 | 6221 | `		if( apNode[n] ){` |
|       509 | 6222 | `			PH7_HashmapUnlinkNode(apNode[n],FALSE);` |
|       254 | 6223 | `		}` |
|   3919598 | 6224 | `	}` |
|  18529159 | 6225 | `	if( pRef->pPrevCollide ){` |
|   1990920 | 6226 | `		pRef->pPrevCollide->pNextCollide = pRef->pNextCollide;` |
|    995171 | 6227 | `	}else{` |
|  16538244 | 6228 | `		pVm->apRefObj[VmRefHash(pRef->nIdx) & (pVm->nRefSize - 1)] = pRef->pNextCollide;` |
|         - | 6229 | `	}` |
|  18529159 | 6230 | `	if( pRef->pNextCollide ){` |
|  15403021 | 6231 | `		pRef->pNextCollide->pPrevCollide = pRef->pPrevCollide;` |
|   7702962 | 6232 | `	}` |
|  18529159 | 6233 | `	MACRO_LD_REMOVE(pVm->pRefList,pRef);` |
|         - | 6234 | `	/* Release the node */` |
|  18529159 | 6235 | `	SySetRelease(&pRef->aReference);` |
|  18529159 | 6236 | `	SySetRelease(&pRef->aArrEntries);` |
|  18529159 | 6237 | `	SyMemBackendPoolFree(&pVm->sAllocator,pRef);` |
|  18529159 | 6238 | `	pVm->nRefUsed--;` |
|  18529159 | 6239 | `	return SXRET_OK;` |
|         5 | 6240 | `}` |
|         - | 6241 | `/*` |
|         - | 6242 | ` * Install a memory object [i.e: a variable] in the reference table.` |
|         - | 6243 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6244 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6245 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6246 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6247 | ` * Refer to the official for more information on this powerful` |
|         - | 6248 | ` * extension.` |
|         - | 6249 | ` */` |
|  19521760 | 6250 | `PH7_PRIVATE sxi32 PH7_VmRefObjInstall(` |
|         - | 6251 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6252 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6253 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6254 | `	ph7_hashmap_node *pMapEntry, /* != NULL if the memory object is an array entry */` |
|         - | 6255 | `	sxi32 iFlags                 /* Control flags */` |
|         - | 6256 | `	)` |
|         5 | 6257 | `{` |
|  19521765 | 6258 | `	VmFrame *pFrame = pVm->pFrame;` |
|         - | 6259 | `	VmRefObj *pRef;` |
|         - | 6260 | `	/* Check if the referenced object already exists */` |
|  19521765 | 6261 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|  19521765 | 6262 | `	if( pRef == 0 ){` |
|         - | 6263 | `		/* Create a new entry */` |
|  19452977 | 6264 | `		pRef = VmNewRefObj(&(*pVm),nIdx);` |
|  19452977 | 6265 | `		if( pRef == 0 ){` |
|       ! 0 | 6266 | `			return SXERR_MEM;` |
|         - | 6267 | `		}` |
|  19452977 | 6268 | `		pRef->iFlags = iFlags;` |
|         - | 6269 | `		/* Install the entry */` |
|  19452977 | 6270 | `		VmRefObjInsert(&(*pVm),pRef);` |
|   9727804 | 6271 | `	}` |
|  19521765 | 6272 | `	pFrame = VmSkipExceptionFrames(pFrame);` |
|  19521765 | 6273 | `	if( pFrame->pParent != 0 && pEntry ){` |
|         - | 6274 | `		VmSlot sRef;` |
|         - | 6275 | `		/* Local frame,record referenced entry so that it can` |
|         - | 6276 | `		 * be deleted when we leave this frame.` |
|         - | 6277 | `		 */` |
|   1162721 | 6278 | `		sRef.nIdx = nIdx;` |
|   1162721 | 6279 | `		sRef.pUserData = pEntry;` |
|   1162721 | 6280 | `		if( SXRET_OK != SySetPut(&pFrame->sRef,(const void *)&sRef)) {` |
|       ! 0 | 6281 | `			pEntry = 0; /* Do not record this entry */` |
|       ! 0 | 6282 | `		}` |
|    582669 | 6283 | `	}` |
|  19521765 | 6284 | `	if( pEntry ){` |
|         - | 6285 | `		/* Address of the hash-entry (into a row a dead holder left behind — a name can` |
|         - | 6286 | `		 * be RE-BOUND to the same slot any number of times, and a set that only ever` |
|         - | 6287 | `		 * grew made both the install and the holder count O(rows)) */` |
|   1227713 | 6288 | `		SyHashEntry **apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   1227713 | 6289 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   1228377 | 6290 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|       669 | 6291 | `			if( apEntry[n] == pEntry ){` |
|       ! 0 | 6292 | `				nFree = SXU32_HIGH; /* already recorded: never file one holder twice */` |
|       ! 0 | 6293 | `				break;` |
|         - | 6294 | `			}` |
|       669 | 6295 | `			if( apEntry[n] == 0 && nFree == SXU32_HIGH ){` |
|       258 | 6296 | `				nFree = n;` |
|       127 | 6297 | `			}` |
|       337 | 6298 | `		}` |
|   1227713 | 6299 | `		if( n >= SySetUsed(&pRef->aReference) ){` |
|   1227713 | 6300 | `			if( nFree != SXU32_HIGH ){` |
|       258 | 6301 | `				apEntry[nFree] = pEntry;` |
|       131 | 6302 | `			}else{` |
|   1227459 | 6303 | `				SySetPut(&pRef->aReference,(const void *)&pEntry);` |
|         - | 6304 | `			}` |
|    615165 | 6305 | `		}` |
|    615165 | 6306 | `	}` |
|  19521765 | 6307 | `	if( pMapEntry ){` |
|         - | 6308 | `		/* Address of the hashmap node [i.e: Array entry] — same row reuse */` |
|   7995818 | 6309 | `		ph7_hashmap_node **apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|   7995818 | 6310 | `		sxu32 n, nFree = SXU32_HIGH;` |
|   7996498 | 6311 | `		for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|       685 | 6312 | `			if( apNode[n] == pMapEntry ){` |
|       ! 0 | 6313 | `				nFree = SXU32_HIGH;` |
|       ! 0 | 6314 | `				break;` |
|         - | 6315 | `			}` |
|       685 | 6316 | `			if( apNode[n] == 0 && nFree == SXU32_HIGH ){` |
|         3 | 6317 | `				nFree = n;` |
|         1 | 6318 | `			}` |
|       345 | 6319 | `		}` |
|   7995818 | 6320 | `		if( n >= SySetUsed(&pRef->aArrEntries) ){` |
|   7995818 | 6321 | `			if( nFree != SXU32_HIGH ){` |
|         3 | 6322 | `				apNode[nFree] = pMapEntry;` |
|         2 | 6323 | `			}else{` |
|   7995816 | 6324 | `				SySetPut(&pRef->aArrEntries,(const void *)&pMapEntry);` |
|         - | 6325 | `			}` |
|   3997917 | 6326 | `		}` |
|   3997917 | 6327 | `	}` |
|  19521765 | 6328 | `	return SXRET_OK;` |
|   9762203 | 6329 | `}` |
|         - | 6330 | `/*` |
|         - | 6331 | ` * Remove a memory object [i.e: a variable] from the reference table.` |
|         - | 6332 | ` * The implementation of the reference mechanism in the PH7 engine` |
|         - | 6333 | ` * differ greatly from the one used by the zend engine. That is,` |
|         - | 6334 | ` * the reference implementation is consistent,solid and it's` |
|         - | 6335 | ` * behavior resemble the C++ reference mechanism.` |
|         - | 6336 | ` * Refer to the official for more information on this powerful` |
|         - | 6337 | ` * extension.` |
|         - | 6338 | ` */` |
|   8994975 | 6339 | `PH7_PRIVATE sxi32 PH7_VmRefObjRemove(` |
|         - | 6340 | `	ph7_vm *pVm,                 /* Target VM */` |
|         - | 6341 | `	sxu32 nIdx,                  /* Memory object index in the global object pool */` |
|         - | 6342 | `	SyHashEntry *pEntry,         /* Hash entry of this object */` |
|         - | 6343 | `	ph7_hashmap_node *pMapEntry  /* != NULL if the memory object is an array entry */` |
|         - | 6344 | `	)` |
|         5 | 6345 | `{` |
|         - | 6346 | `	VmRefObj *pRef;` |
|         - | 6347 | `	sxu32 n;` |
|         - | 6348 | `	/* Check if the referenced object already exists */` |
|   8994980 | 6349 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   8994980 | 6350 | `	if( pRef == 0 ){` |
|         - | 6351 | `		/* Not such entry */` |
|         5 | 6352 | `		return SXERR_NOTFOUND;` |
|         - | 6353 | `	}` |
|         - | 6354 | `	/* Remove the desired entry */` |
|   8994976 | 6355 | `	if( pEntry ){` |
|         - | 6356 | `		SyHashEntry **apEntry;` |
|   1162689 | 6357 | `		apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|   2325771 | 6358 | `		for( n = 0 ; n < SySetUsed(&pRef->aReference) ; n++ ){` |
|   1163087 | 6359 | `			if( apEntry[n] == pEntry ){` |
|         - | 6360 | `				/* Nullify the entry */` |
|   1162687 | 6361 | `				apEntry[n] = 0;` |
|         - | 6362 | `				/*` |
|         - | 6363 | `				 * NOTE:` |
|         - | 6364 | `				 * In future releases,think to add a free pool of entries,so that` |
|         - | 6365 | `				 * we avoid wasting spaces.` |
|         - | 6366 | `				 */` |
|    582652 | 6367 | `			}` |
|    582857 | 6368 | `		}` |
|    582653 | 6369 | `	}` |
|   8994976 | 6370 | `	if( pMapEntry ){` |
|         - | 6371 | `		ph7_hashmap_node **apNode;` |
|   7832292 | 6372 | `		apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  15665459 | 6373 | `		for(n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; n++ ){` |
|   7833172 | 6374 | `			if( apNode[n] == pMapEntry ){` |
|         - | 6375 | `				/* nullify the entry */` |
|   7832292 | 6376 | `				apNode[n] = 0;` |
|   3916162 | 6377 | `			}` |
|   3916607 | 6378 | `		}` |
|   3916162 | 6379 | `	}` |
|   8994976 | 6380 | `	return SXRET_OK;` |
|   4498822 | 6381 | `}` |
|         - | 6382 | `/*` |
|         - | 6383 | ` * How many LIVE holders still refer to a memory-object slot: the symbol-table` |
|         - | 6384 | ` * names bound to it plus the array nodes pointing at it. php refcounts a` |
|         - | 6385 | ` * reference set and keeps the VALUE alive while any holder remains, so this is` |
|         - | 6386 | ` * the count every "may I release this slot?" decision asks for.` |
|         - | 6387 | ` *` |
|         - | 6388 | ` * A row is only a holder while it is non-NULL (every holder's death nullifies` |
|         - | 6389 | ` * its own row through PH7_VmRefObjRemove) and, for a node, while it still points` |
|         - | 6390 | ` * HERE — a slot index travels through the free list, so a record can outlive the` |
|         - | 6391 | ` * node that filed the row (the same filter VmUnsetVarByName applies).` |
|         - | 6392 | ` */` |
|   9898258 | 6393 | `PH7_PRIVATE sxu32 PH7_VmSlotHolderCount(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6394 | `{` |
|         - | 6395 | `	ph7_hashmap_node **apNode;` |
|         - | 6396 | `	SyHashEntry **apEntry;` |
|         - | 6397 | `	VmRefObj *pRef;` |
|   9898263 | 6398 | `	sxu32 n, nLive = 0;` |
|   9898263 | 6399 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6400 | `		return 0;` |
|         - | 6401 | `	}` |
|   9898263 | 6402 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   9898263 | 6403 | `	if( pRef == 0 ){` |
|         3 | 6404 | `		return 0;` |
|         - | 6405 | `	}` |
|   9898261 | 6406 | `	if( pRef->nPin > 0 ){` |
|         - | 6407 | `		/* Holders the table cannot name, counted: reference-bound properties. */` |
|        13 | 6408 | `		nLive += pRef->nPin;` |
|   9898255 | 6409 | `	}else if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|         - | 6410 | ``		/* A permanent pin — a `use (&$x)` capture, a static, an enum case. It is the`` |
|         - | 6411 | ``		 * reason the slot is alive, so it counts as a holder: `$o->p = &$a[0]` leaves`` |
|         - | 6412 | `		 * that element a reference for as long as the property aliases it, exactly as` |
|         - | 6413 | `		 * php's refcount does. */` |
|         5 | 6414 | `		nLive++;` |
|         2 | 6415 | `	}` |
|   9898261 | 6416 | `	apEntry = (SyHashEntry **)SySetBasePtr(&pRef->aReference);` |
|  11070493 | 6417 | `	for( n = 0 ; n < SySetUsed(&pRef->aReference) ; ++n ){` |
|   1172237 | 6418 | `		if( apEntry[n] ){` |
|       248 | 6419 | `			nLive++;` |
|       122 | 6420 | `		}` |
|    587432 | 6421 | `	}` |
|   9898261 | 6422 | `	apNode = (ph7_hashmap_node **)SySetBasePtr(&pRef->aArrEntries);` |
|  18638919 | 6423 | `	for( n = 0 ; n < SySetUsed(&pRef->aArrEntries) ; ++n ){` |
|   8740663 | 6424 | `		if( apNode[n] && apNode[n]->nValIdx == nIdx ){` |
|    901348 | 6425 | `			nLive++;` |
|    450679 | 6426 | `		}` |
|   4370360 | 6427 | `	}` |
|   9898261 | 6428 | `	return nLive;` |
|   4950471 | 6429 | `}` |
|         - | 6430 | `/*` |
|         - | 6431 | ` * Release a slot whose last holder just went away. A no-op while anything still` |
|         - | 6432 | ` * holds it (php frees the value with the last reference, not with the first one` |
|         - | 6433 | ` * to die) and for a slot deliberately pinned past its frame (VM_REF_IDX_KEEP:` |
|         - | 6434 | `` * `use (&$x)` captures, statics, reference-bound properties).`` |
|         - | 6435 | ` */` |
|   7842005 | 6436 | `PH7_PRIVATE void PH7_VmReleaseUnheldSlot(ph7_vm *pVm,sxu32 nIdx)` |
|         5 | 6437 | `{` |
|         - | 6438 | `	VmRefObj *pRef;` |
|   7842010 | 6439 | `	if( nIdx == SXU32_HIGH ){` |
|       ! 0 | 6440 | `		return;` |
|         - | 6441 | `	}` |
|   7842010 | 6442 | `	pRef = VmRefObjExtract(&(*pVm),nIdx);` |
|   7842010 | 6443 | `	if( pRef ){` |
|   7841966 | 6444 | `		if( pRef->iFlags & VM_REF_IDX_KEEP ){` |
|        61 | 6445 | `			return; /* pinned past its frame — its holder is not in the table */` |
|         - | 6446 | `		}` |
|   7841906 | 6447 | `		if( PH7_VmSlotHolderCount(&(*pVm),nIdx) > 0 ){` |
|      3513 | 6448 | `			return; /* somebody still holds it */` |
|         - | 6449 | `		}` |
|   3919214 | 6450 | `	}` |
|         - | 6451 | `	/* No record at all means nothing was ever registered against the slot, which is` |
|         - | 6452 | `	 * the same answer as a count of zero — release it (this is what every caller did` |
|         - | 6453 | `	 * unconditionally before the holder rule). */` |
|   7838440 | 6454 | `	PH7_VmUnsetMemObj(&(*pVm),nIdx,FALSE);` |
|         - | 6455 | `	/* The slot is back in the free pool; drop its stale local-teardown entry so a` |
|         - | 6456 | `	 * later reuse of the index is not double-freed (see VmDropFrameLocalSlot). */` |
|   7838440 | 6457 | `	VmDropFrameLocalSlot(&(*pVm),nIdx);` |
|   3921026 | 6458 | `}` |
|         - | 6459 | `/*` |
|         - | 6460 | ` * Drop a frame's record of "this NAME refers to this slot" (sRef), used when the name` |
|         - | 6461 | ` * stops referring to it — a rebind, or an unset of the name. The rows are otherwise only` |
|         - | 6462 | ` * consumed at frame exit, so a name re-bound (or re-created) in a loop files one per step` |
|         - | 6463 | ` * and the set is pure growth; they are also pointers to a symbol-table entry that unset()` |
|         - | 6464 | ` * frees, which the teardown would later compare against a live entry at the same address.` |
|         - | 6465 | ` */` |
|      9740 | 6466 | `PH7_PRIVATE void VmDropFrameRefEntry(ph7_vm *pVm,sxu32 nIdx,SyHashEntry *pEntry)` |
|         5 | 6467 | `{` |
|         - | 6468 | `	VmFrame *pFrame;` |
|     22581 | 6469 | `	for( pFrame = pVm->pFrame ; pFrame ; pFrame = pFrame->pParent ){` |
|     12841 | 6470 | `		VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     12841 | 6471 | `		sxu32 n = 0;` |
|     25161 | 6472 | `		while( n < SySetUsed(&pFrame->sRef) ){` |
|     12322 | 6473 | `			if( aSlot[n].nIdx == nIdx && aSlot[n].pUserData == (void *)pEntry ){` |
|         - | 6474 | `				/* Swap-remove: teardown order over sRef is immaterial. Re-test the` |
|         - | 6475 | `				 * same index — it now holds the row swapped in from the tail. */` |
|      3094 | 6476 | `				aSlot[n] = aSlot[SySetUsed(&pFrame->sRef)-1];` |
|      3094 | 6477 | `				(void)SySetPop(&pFrame->sRef);` |
|      3094 | 6478 | `				continue;` |
|         - | 6479 | `			}` |
|      9230 | 6480 | `			n++;` |
|         2 | 6481 | `		}` |
|      6423 | 6482 | `	}` |
|      9745 | 6483 | `}` |
|         - | 6484 | `/*` |
|         - | 6485 | ` * Is this the name of an ENGINE temporary rather than a variable the program wrote?` |
|         - | 6486 | ` *` |
|         - | 6487 | ` * The compiler parks a step's value in a synthetic local when a construct's target` |
|         - | 6488 | ` * cannot be installed by name — foreach's list()/[...] destructuring and its` |
|         - | 6489 | `` * non-name `as` targets. php has no such variable at all (its temporaries live in`` |
|         - | 6490 | ` * compiled slots), so these must not surface as locals: they were showing up in` |
|         - | 6491 | ` * get_defined_vars() and, at file scope, in $GLOBALS. The bracketed spelling is` |
|         - | 6492 | ` * what makes the name unwritable in source, so it is also what identifies it here.` |
|         - | 6493 | ` */` |
|     23010 | 6494 | `PH7_PRIVATE int PH7_VmVarNameIsInternal(const char *zName,sxu32 nByte)` |
|         5 | 6495 | `{` |
|     23015 | 6496 | `	return nByte > 0 && zName[0] == '[';` |
|         5 | 6497 | `}` |
|         - | 6498 | `/*` |
|         - | 6499 | ` * Bind a NAME to an existing slot, creating the symbol-table entry when the name is` |
|         - | 6500 | `` * new and RE-BINDING it when it is not. This is what a by-reference `foreach` does to`` |
|         - | 6501 | ` * its value variable on every step, and it goes through the reference table like any` |
|         - | 6502 | ` * other alias: a binding that is not registered there is not a HOLDER, so the element` |
|         - | 6503 | `` * it aliases did not count as referenced (no `&` in var_dump, and an array COPY quietly`` |
|         - | 6504 | ` * stopped sharing it) and nothing kept its value alive when the array let go.` |
|         - | 6505 | ` */` |
|       534 | 6506 | `PH7_PRIVATE void PH7_VmBindVarSlot(ph7_vm *pVm,VmFrame *pFrame,const char *zName,sxu32 nByte,` |
|         - | 6507 | `	sxu32 nIdx)` |
|         5 | 6508 | `{` |
|       539 | 6509 | `	SyHashEntry *pEntry = SyHashGet(&pFrame->hVar,(const void *)zName,nByte);` |
|       539 | 6510 | `	if( pEntry ){` |
|        95 | 6511 | `		PH7_VmRebindVarSlot(&(*pVm),pFrame,pEntry,zName,nByte,nIdx);` |
|        95 | 6512 | `		return;` |
|         - | 6513 | `	}` |
|       447 | 6514 | `	if( SXRET_OK != SyHashInsert(&pFrame->hVar,(const void *)zName,nByte,SX_INT_TO_PTR(nIdx)) ){` |
|       ! 0 | 6515 | `		return;` |
|         - | 6516 | `	}` |
|       447 | 6517 | `	if( pFrame->pParent == 0 && !PH7_VmVarNameIsInternal(zName,nByte) ){` |
|         - | 6518 | `		/* A global is also an entry of the $GLOBALS view */` |
|        41 | 6519 | `		VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|        19 | 6520 | `	}` |
|       447 | 6521 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrame->hVar),0,0);` |
|       272 | 6522 | `}` |
|         - | 6523 | `/*` |
|         - | 6524 | `` * Point an EXISTING symbol-table entry at another slot — php's `=&` on a name that`` |
|         - | 6525 | `` * is already bound (`$y = 2; $y = &$x;`, `$r = &$a[$k]` a second time round a loop,`` |
|         - | 6526 | ` * a by-ref parameter re-bound inside the callee). php drops the name's old binding,` |
|         - | 6527 | ` * releasing its value when nothing else holds it, and aliases the new slot; PH7` |
|         - | 6528 | ``  * refused the whole statement with `Referenced variable name '%z' already exists` `` |
|         - | 6529 | ` * and left the OLD binding standing, so every later write through the name went to` |
|         - | 6530 | ` * the wrong variable.` |
|         - | 6531 | ` *` |
|         - | 6532 | ` * The entry's ADDRESS is deliberately reused rather than deleted and re-inserted:` |
|         - | 6533 | ` * the frame's sRef teardown set records that pointer, and the reference table` |
|         - | 6534 | ` * compares it by identity.` |
|         - | 6535 | ` */` |
|      3176 | 6536 | `PH7_PRIVATE void PH7_VmRebindVarSlot(` |
|         - | 6537 | `	ph7_vm *pVm,          /* Target VM */` |
|         - | 6538 | `	VmFrame *pFrame,      /* Frame owning the symbol-table entry */` |
|         - | 6539 | `	SyHashEntry *pEntry,  /* The existing name binding */` |
|         - | 6540 | `	const char *zName,    /* Variable name */` |
|         - | 6541 | `	sxu32 nByte,          /* Name length */` |
|         - | 6542 | `	sxu32 nIdx            /* Slot the name must alias from now on */` |
|         - | 6543 | `	)` |
|         3 | 6544 | `{` |
|      3179 | 6545 | `	sxu32 nOld = (sxu32)SX_PTR_TO_INT(pEntry->pUserData);` |
|      3179 | 6546 | `	if( nOld == nIdx ){` |
|         - | 6547 | ``		/* Already this slot: `$r = &$x` twice over is a no-op, not a rebind */`` |
|       ! 0 | 6548 | `		return;` |
|         - | 6549 | `	}` |
|         - | 6550 | `	/* Forget this name in the old slot's reference record, and in the frame's own` |
|         - | 6551 | `	 * "release this reference at exit" set */` |
|      3179 | 6552 | `	PH7_VmRefObjRemove(&(*pVm),nOld,pEntry,0);` |
|      3179 | 6553 | `	VmDropFrameRefEntry(&(*pVm),nOld,pEntry);` |
|      3179 | 6554 | `	pEntry->pUserData = SX_INT_TO_PTR(nIdx);` |
|      3179 | 6555 | `	if( pFrame->pParent == 0 ){` |
|         - | 6556 | `		/* A global is ALSO a $GLOBALS entry pointing at the old slot; re-point that node.` |
|         - | 6557 | `		 * Done against the node directly rather than through VmHashmapRefInsert, whose` |
|         - | 6558 | `		 * ph7_value key allocates a blob on every call — this runs once per step of a` |
|         - | 6559 | ``		 * global-scope `foreach ($a as &$v)`. */`` |
|       105 | 6560 | `		ph7_hashmap_node *pGlobalNode = 0;` |
|       102 | 6561 | `		if( SXRET_OK == HashmapLookupBlobKey(pVm->pGlobal,(const void *)zName,nByte,&pGlobalNode)` |
|       105 | 6562 | `		 && pGlobalNode ){` |
|       105 | 6563 | `			if( pGlobalNode->nValIdx != nIdx ){` |
|       105 | 6564 | `				PH7_VmRefObjRemove(&(*pVm),pGlobalNode->nValIdx,0,pGlobalNode);` |
|       105 | 6565 | `				pGlobalNode->nValIdx = nIdx;` |
|       105 | 6566 | `				PH7_VmRefObjInstall(&(*pVm),nIdx,0,pGlobalNode,0);` |
|        51 | 6567 | `			}` |
|        54 | 6568 | `		}else{` |
|         - | 6569 | `			/* No node under that exact blob key (a numeric name is filed as an INT key) */` |
|       ! 0 | 6570 | `			VmHashmapRefInsert(pVm->pGlobal,zName,nByte,nIdx);` |
|         - | 6571 | `		}` |
|        51 | 6572 | `	}` |
|      3179 | 6573 | `	PH7_VmRefObjInstall(&(*pVm),nIdx,pEntry,0,0);` |
|         - | 6574 | `	/* The old value dies with its last holder — and only then */` |
|      3179 | 6575 | `	PH7_VmReleaseUnheldSlot(&(*pVm),nOld);` |
|      1591 | 6576 | `}` |
|         - | 6577 | `/*` |
|         - | 6578 | ` * Is there a SCHEME at the front of this name, and how long is it?` |
|         - | 6579 | ` *` |
|         - | 6580 | ` * php reads one only at the START, and only as a URL scheme: a run of` |
|         - | 6581 | ` * [A-Za-z0-9+.-] at least TWO characters long, followed immediately by "://".` |
|         - | 6582 | ` * PHL used to hunt for the first "://" ANYWHERE in the name and then trim` |
|         - | 6583 | ` * whitespace off whatever preceded it, which made four ordinary FILENAMES into` |
|         - | 6584 | ` * URLs: " php://memory" and "php ://memory" opened the memory stream php opens` |
|         - | 6585 | ` * a file called that, "./sub://z" and "a b://c" were looked up as schemes` |
|         - | 6586 | ` * "./sub" and "a b". The two-character minimum is php's, and it is what keeps a` |
|         - | 6587 | ` * Windows drive letter ("C://tmp") a path rather than a "C" scheme.` |
|         - | 6588 | ` */` |
|    106309 | 6589 | `static int VmUrlScheme(const char *zIn,int nByte,int *pnScheme)` |
|         5 | 6590 | `{` |
|    106314 | 6591 | `	int i = 0;` |
|    616232 | 6592 | `	while( i < nByte ){` |
|    616188 | 6593 | `		int c = zIn[i];` |
|    616183 | 6594 | `		if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= '0' && c <= '9')` |
|    106707 | 6595 | `		 \|\| c == '+' \|\| c == '-' \|\| c == '.' ){` |
|    509923 | 6596 | `			i++;` |
|    509923 | 6597 | `			continue;` |
|         - | 6598 | `		}` |
|    106270 | 6599 | `		break;` |
|       ! 0 | 6600 | `	}` |
|         - | 6601 | `	/* php also accepts a scheme with NOTHING after it ("zzz://" is an unknown` |
|         - | 6602 | `	 * wrapper, not a file called "zzz://"), which the old scan refused. */` |
|    106314 | 6603 | `	if( i > 1 && i + 2 < nByte && zIn[i] == ':' && zIn[i+1] == '/' && zIn[i+2] == '/' ){` |
|       641 | 6604 | `		*pnScheme = i;` |
|       641 | 6605 | `		return 1;` |
|         - | 6606 | `	}` |
|    105678 | 6607 | `	return 0;` |
|     53175 | 6608 | `}` |
|         - | 6609 | `/*` |
|         - | 6610 | ` * The same question from outside vm.c: how long is the scheme, or 0 for a name` |
|         - | 6611 | ` * that has none. stream_resolve_include_path() asks it to decide whether a name` |
|         - | 6612 | ` * is walkable at all.` |
|         - | 6613 | ` */` |
|        52 | 6614 | `PH7_PRIVATE int PH7_VmUrlSchemeLen(const char *zIn,int nByte)` |
|         2 | 6615 | `{` |
|        54 | 6616 | `	int nScheme = 0;` |
|        54 | 6617 | `	if( zIn == 0 ){` |
|       ! 0 | 6618 | `		return 0;` |
|         - | 6619 | `	}` |
|        54 | 6620 | `	if( nByte < 0 ){` |
|        25 | 6621 | `		nByte = (int)SyStrlen(zIn);` |
|        12 | 6622 | `	}` |
|        54 | 6623 | `	return VmUrlScheme(zIn,nByte,&nScheme) ? nScheme : 0;` |
|        28 | 6624 | `}` |
|         - | 6625 | `/*` |
|         - | 6626 | ` * The bytes the FILE wrapper is handed for a file:// URL.` |
|         - | 6627 | ` *` |
|         - | 6628 | ` * A file:// URL has an AUTHORITY, and php only accepts two of them: an empty` |
|         - | 6629 | `` * one and `localhost` (case-insensitively, and only with its slash). Anything`` |
|         - | 6630 | ` * else is a remote host it refuses to reach -- where PHL stripped exactly` |
|         - | 6631 | `` * "file://" and opened whatever was left, so `file://tmp/passwd` silently read`` |
|         - | 6632 | ` * the RELATIVE path tmp/passwd. What survives the strip is the LAST slash of` |
|         - | 6633 | `` * the leading run, so `file:////x` is /x, `file://localhost//x` is /x, and`` |
|         - | 6634 | `` * `file://` on its own is the root directory.`` |
|         - | 6635 | ` *` |
|         - | 6636 | ` * Returns 0 for an authority php will not reach; the caller answers "no` |
|         - | 6637 | ` * wrapper", as php does.` |
|         - | 6638 | ` */` |
|        60 | 6639 | `static int VmFileUrlPath(const char *zIn,int nByte,int nScheme,const char **pzPath)` |
|         4 | 6640 | `{` |
|         - | 6641 | `	static const char zLocal[] = "file://localhost/";` |
|        64 | 6642 | `	const char *zPath = &zIn[nScheme+1]; /* the first slash of "://" */` |
|        64 | 6643 | `	const char *zEnd = &zIn[nByte];` |
|        60 | 6644 | `	if( nScheme + 3 < nByte && zIn[nScheme+3] != '/'` |
|         - | 6645 | `#ifdef __WINNT__` |
|         - | 6646 | ``	 /* php's own Windows allowance: `file://C:/x` is a DRIVE, not a host. */`` |
|         4 | 6647 | `	 && !(nScheme + 4 < nByte && zIn[nScheme+4] == ':')` |
|         - | 6648 | `#endif` |
|         - | 6649 | `	){` |
|        32 | 6650 | `		if( nByte < (int)sizeof(zLocal)-1` |
|        35 | 6651 | `		 \|\| SyStrnicmp(zIn,zLocal,(sxu32)sizeof(zLocal)-1) != 0 ){` |
|        31 | 6652 | `			return 0; /* a host this build (and php) will not fetch from */` |
|         - | 6653 | `		}` |
|         4 | 6654 | `		zPath = &zIn[nScheme+3+sizeof("localhost")-1];` |
|         2 | 6655 | `	}` |
|        92 | 6656 | `	while( &zPath[1] < zEnd && zPath[1] == '/' ){` |
|        60 | 6657 | `		zPath++;` |
|         2 | 6658 | `	}` |
|        34 | 6659 | `	*pzPath = zPath;` |
|        34 | 6660 | `	return 1;` |
|        34 | 6661 | `}` |
|         - | 6662 | `/*` |
|         - | 6663 | ` * The same rule for the VFS side, which stats and unlinks a name without ever` |
|         - | 6664 | ` * going through a stream device. It carried a second, shorter copy of the` |
|         - | 6665 | `` * strip -- no slash-run collapse and nothing for a bare `file://` -- so`` |
|         - | 6666 | `` * `is_dir('file://')` was false where php names the root. A host this build`` |
|         - | 6667 | ` * will not reach is handed back UNCHANGED: the syscall then fails on a name` |
|         - | 6668 | ` * that is not a path, which is the FALSE php answers for it.` |
|         - | 6669 | ` */` |
|     68249 | 6670 | `PH7_PRIVATE const char * PH7_VmFileUrlLocalPath(const char *zPath)` |
|         5 | 6671 | `{` |
|         - | 6672 | `	const char *zOut;` |
|     68254 | 6673 | `	int nByte,nScheme = 0;` |
|     68254 | 6674 | `	if( zPath == 0 ){` |
|       ! 0 | 6675 | `		return zPath;` |
|         - | 6676 | `	}` |
|     68254 | 6677 | `	nByte = (int)SyStrlen(zPath);` |
|     68249 | 6678 | `	if( !VmUrlScheme(zPath,nByte,&nScheme)` |
|     34154 | 6679 | `	 \|\| nScheme != (int)sizeof("file")-1` |
|        30 | 6680 | `	 \|\| SyStrnicmp(zPath,"file",sizeof("file")-1) != 0 ){` |
|     68232 | 6681 | `		return zPath;` |
|         - | 6682 | `	}` |
|        25 | 6683 | `	if( !VmFileUrlPath(zPath,nByte,nScheme,&zOut) ){` |
|         6 | 6684 | `		return zPath;` |
|         - | 6685 | `	}` |
|         - | 6686 | `#ifdef __WINNT__` |
|         - | 6687 | `	/* The one piece that is only true here: the leading slash php's own strip` |
|         - | 6688 | `	 * leaves in front of a DRIVE is not part of a Windows path, so` |
|         - | 6689 | `	 * file:///C:/x and file://localhost/C:/x both name C:/x. */` |
|         2 | 6690 | `	if( zOut[0] == '/' && zOut[1] != 0 && zOut[2] == ':' ){` |
|         2 | 6691 | `		zOut++;` |
|         - | 6692 | `	}` |
|         - | 6693 | `#endif` |
|        20 | 6694 | `	return zOut;` |
|     34146 | 6695 | `}` |
|         - | 6696 | `#if !defined(PH7_DISABLE_BUILTIN_FUNC) \|\| !defined(PH7_DISABLE_DISK_IO)` |
|         - | 6697 | `/*` |
|         - | 6698 | ` * Has a script taken this device out of service with stream_wrapper_unregister()?` |
|         - | 6699 | ` */` |
|     38196 | 6700 | `PH7_PRIVATE int PH7_VmStreamDeviceSuppressed(ph7_vm *pVm,const ph7_io_stream *pStream)` |
|         5 | 6701 | `{` |
|     38201 | 6702 | `	const ph7_io_stream **apOff = (const ph7_io_stream **)SySetBasePtr(&pVm->aSuppressedIo);` |
|         - | 6703 | `	sxu32 n;` |
|     38291 | 6704 | `	for( n = 0 ; n < SySetUsed(&pVm->aSuppressedIo) ; n++ ){` |
|       133 | 6705 | `		if( apOff[n] == pStream ){` |
|        43 | 6706 | `			return 1;` |
|         - | 6707 | `		}` |
|        46 | 6708 | `	}` |
|     38159 | 6709 | `	return 0;` |
|     19102 | 6710 | `}` |
|         - | 6711 | `/*` |
|         - | 6712 | ` * The device currently answering to a scheme name, or NULL. The scan runs to` |
|         - | 6713 | ` * the END rather than stopping at the first hit: once a built-in has been` |
|         - | 6714 | ` * unregistered a userland wrapper can be registered under the same name, both` |
|         - | 6715 | ` * sit in the list, and the LIVE one is the later of the two.` |
|         - | 6716 | ` */` |
|     37972 | 6717 | `PH7_PRIVATE ph7_io_stream * PH7_VmFindStreamDevice(ph7_vm *pVm,const char *zName,int nName)` |
|         5 | 6718 | `{` |
|     37977 | 6719 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|     37977 | 6720 | `	ph7_io_stream *pHit = 0;` |
|     37977 | 6721 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|     37977 | 6722 | `	if( nName < 0 ){` |
|       ! 0 | 6723 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6724 | `	}` |
|    189989 | 6725 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|    152017 | 6726 | `		ph7_io_stream *pStream = apStream[n];` |
|    152012 | 6727 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|    113978 | 6728 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|    114049 | 6729 | `			continue;` |
|         - | 6730 | `		}` |
|     37973 | 6731 | `		if( PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|        15 | 6732 | `			continue;` |
|         - | 6733 | `		}` |
|     37959 | 6734 | `		pHit = pStream;` |
|     18981 | 6735 | `	}` |
|     37977 | 6736 | `	return pHit;` |
|         5 | 6737 | `}` |
|         - | 6738 | `/*` |
|         - | 6739 | ` * Is this scheme one the build HAS but the script has switched off? php words` |
|         - | 6740 | ` * that differently from a scheme nothing was ever registered under -- but only` |
|         - | 6741 | ` * for file://, whose plain-files fallback is the branch that reports it.` |
|         - | 6742 | ` */` |
|         2 | 6743 | `PH7_PRIVATE int PH7_VmStreamSchemeDisabled(ph7_vm *pVm,const char *zName,int nName)` |
|         1 | 6744 | `{` |
|         3 | 6745 | `	ph7_io_stream **apStream = (ph7_io_stream **)SySetBasePtr(&pVm->aIOstream);` |
|         3 | 6746 | `	sxu32 n,nEntry = SySetUsed(&pVm->aIOstream);` |
|         3 | 6747 | `	int bSeen = 0;` |
|         3 | 6748 | `	if( nName < 0 ){` |
|       ! 0 | 6749 | `		nName = (int)SyStrlen(zName);` |
|       ! 0 | 6750 | `	}` |
|        13 | 6751 | `	for( n = 0 ; n < nEntry ; n++ ){` |
|        11 | 6752 | `		ph7_io_stream *pStream = apStream[n];` |
|        10 | 6753 | `		if( (int)SyStrlen(pStream->zName) != nName` |
|         8 | 6754 | `		 \|\| SyStrnicmp(pStream->zName,zName,(sxu32)nName) != 0 ){` |
|         9 | 6755 | `			continue;` |
|         - | 6756 | `		}` |
|         3 | 6757 | `		if( !PH7_VmStreamDeviceSuppressed(pVm,pStream) ){` |
|       ! 0 | 6758 | `			return 0; /* it is live */` |
|         - | 6759 | `		}` |
|         3 | 6760 | `		bSeen = 1;` |
|         2 | 6761 | `	}` |
|         3 | 6762 | `	return bSeen;` |
|         2 | 6763 | `}` |
|         - | 6764 | `/*` |
|         - | 6765 | ` * Extract the IO stream device associated with a given scheme.` |
|         - | 6766 | ` * Return a pointer to an instance of ph7_io_stream when the scheme` |
|         - | 6767 | ` * have an associated IO stream registered with it. NULL otherwise.` |
|         - | 6768 | ` * If no scheme:// is avalilable then the file:// scheme is assumed.` |
|         - | 6769 | ` * For more information on how to register IO stream devices,please` |
|         - | 6770 | ` * refer to the official documentation.` |
|         - | 6771 | ` */` |
|     37984 | 6772 | `PH7_PRIVATE const ph7_io_stream * PH7_VmGetStreamDevice(` |
|         - | 6773 | `	ph7_vm *pVm,           /* Target VM */` |
|         - | 6774 | `	const char **pzDevice, /* Full path,URI,... */` |
|         - | 6775 | `	int nByte              /* *pzDevice length*/` |
|         - | 6776 | `	)` |
|         5 | 6777 | `{` |
|         - | 6778 | `	const char *zIn,*zNext;` |
|         - | 6779 | `	ph7_io_stream *pStream;` |
|     37989 | 6780 | `	int nScheme = 0;` |
|         - | 6781 | `	/* Check if a scheme [i.e: file://,http://,zip://...] is available */` |
|     37989 | 6782 | `	zIn = *pzDevice;` |
|     37989 | 6783 | `	if( !VmUrlScheme(zIn,nByte,&nScheme) ){` |
|         - | 6784 | `		/* No scheme: php's default is the plain-files wrapper, and it is the` |
|         - | 6785 | `		 * SAME slot file:// names -- so a script that unregisters file:// loses` |
|         - | 6786 | `		 * the bare-path open too, and one that registers its own wrapper over` |
|         - | 6787 | `		 * file:// gets bare paths routed through it. Looking the name up rather` |
|         - | 6788 | `		 * than answering pDefStream is what makes both true. */` |
|     37413 | 6789 | `		return PH7_VmFindStreamDevice(pVm,"file",(int)sizeof("file")-1);` |
|         - | 6790 | `	}` |
|       581 | 6791 | `	zNext = &zIn[nScheme+sizeof("://")-1];` |
|         - | 6792 | `	/* php applies the file:// authority rules by the SCHEME NAME, before it` |
|         - | 6793 | `	 * cares who is registered under it -- a userland wrapper that replaced` |
|         - | 6794 | `	 * file:// is handed the stripped path too. */` |
|       581 | 6795 | `	if( nScheme == (int)sizeof("file")-1 && SyStrnicmp(zIn,"file",sizeof("file")-1) == 0 ){` |
|        28 | 6796 | `		if( !VmFileUrlPath(zIn,nByte,nScheme,&zNext) ){` |
|        13 | 6797 | `			return 0;` |
|         - | 6798 | `		}` |
|         7 | 6799 | `	}` |
|       569 | 6800 | `	pStream = PH7_VmFindStreamDevice(pVm,zIn,nScheme);` |
|       569 | 6801 | `	if( pStream == 0 ){` |
|         - | 6802 | `		/* No such stream -- or one a script has taken out of service. */` |
|        19 | 6803 | `		return 0;` |
|         - | 6804 | `	}` |
|       553 | 6805 | `	*pzDevice = zNext;` |
|       553 | 6806 | `	return pStream;` |
|     18996 | 6807 | `}` |
|         - | 6808 | `/*` |
|         - | 6809 | ` * Why did PH7_VmGetStreamDevice() answer nothing for this name? php raises a` |
|         - | 6810 | ` * REASON of its own before the operation's own failure, and the two a caller` |
|         - | 6811 | ` * can hit here are different sentences. Re-derived from the name rather than` |
|         - | 6812 | ` * threaded out of the lookup, so every call site stays one line.` |
|         - | 6813 | ` *` |
|         - | 6814 | ` * Answers TRUE for the file:// authority php will not reach; otherwise FALSE` |
|         - | 6815 | ` * with *pnScheme set to the length of the scheme that has no wrapper.` |
|         - | 6816 | ` */` |
|        24 | 6817 | `PH7_PRIVATE int PH7_VmStreamDeviceIsRemoteHost(const char *zUri,int nByte,int *pnScheme)` |
|         2 | 6818 | `{` |
|         - | 6819 | `	const char *zPath;` |
|        26 | 6820 | `	int nScheme = 0;` |
|        26 | 6821 | `	if( nByte < 0 ){` |
|        26 | 6822 | `		nByte = (int)SyStrlen(zUri);` |
|        12 | 6823 | `	}` |
|        26 | 6824 | `	if( !VmUrlScheme(zUri,nByte,&nScheme) ){` |
|         3 | 6825 | `		*pnScheme = 0;` |
|         3 | 6826 | `		return 0;` |
|         - | 6827 | `	}` |
|        24 | 6828 | `	*pnScheme = nScheme;` |
|        31 | 6829 | `	return nScheme == (int)sizeof("file")-1` |
|        18 | 6830 | `		&& SyStrnicmp(zUri,"file",sizeof("file")-1) == 0` |
|        29 | 6831 | `		&& !VmFileUrlPath(zUri,nByte,nScheme,&zPath);` |
|        14 | 6832 | `}` |
|         - | 6833 | `#endif /* PH7_DISABLE_BUILTIN_FUNC \|\| PH7_DISABLE_DISK_IO */` |
|         - | 6834 | `/* HTTP/URI routines moved to vm_http.c */` |
|         - | 6835 |  |
