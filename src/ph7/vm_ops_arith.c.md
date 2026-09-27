# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 635/750 lines (84.67%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <math.h>  /* pow */` |
|      - |    8 | `/*` |
|      - |    9 | ` * Section:` |
|      - |   10 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   11 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   12 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   13 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   14 | ` * Status:` |
|      - |   15 | ` *    Stable.` |
|      - |   16 | ` */` |
|      - |   17 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   18 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   19 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   20 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   21 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   22 | `#include "vm_dispatch.h"` |
|      - |   23 | `#define sState (*pState)` |
|      - |   24 |  |
|      - |   25 | `/*` |
|      - |   26 | ` * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of` |
|      - |   27 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   28 | ` */` |
|    108 |   29 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   30 | `{` |
|    111 |   31 | `	ph7_value *pTos = pState->pTos;` |
|    111 |   32 | `	ph7_value *pStack = pState->pStack;` |
|    111 |   33 | `	VmInstr *aInstr = pState->aInstr;` |
|    111 |   34 | `	sxi32 pc = pState->pc;` |
|      - |   35 | `	sxi32 rc;` |
|     54 |   36 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    111 |   37 | `	ph7_value *pNos = &pTos[-1];` |
|      - |   38 | `	ph7_value *pObj;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pNos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|    111 |   45 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|      - |   46 | ``		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a`` |
|      - |   47 | `		 * COAL entry targeting exactly THIS store (owner + pc identity — a` |
|      - |   48 | `		 * stale entry from an abandoned statement can never match, and nested` |
|      - |   49 | `		 * arms in the RHS were consumed/dropped above this one). Dispatch the` |
|      - |   50 | `		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=` |
|      - |   51 | `		 * residual: pre-fix the assign bypassed __set through the normal slot` |
|      - |   52 | `		 * path). The RHS stays as the expression result. */` |
|     13 |   53 | `		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|     12 |   54 | `		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr` |
|     12 |   55 | `		 && pTop->nPc == (sxu32)pc` |
|     13 |   56 | `		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK \|\| pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){` |
|     13 |   57 | `			VmHookRmw sPend = *pTop;` |
|     13 |   58 | `			(void)SySetPop(&pVm->aHookRmw);` |
|     13 |   59 | `			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){` |
|     11 |   60 | `				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);` |
|     11 |   61 | `				if( rcHs == PH7_ABORT ){` |
|    ! 0 |   62 | `					SyBlobRelease(&sPend.sName);` |
|    ! 0 |   63 | `					PH7_ClassInstanceUnref(sPend.pThis);` |
|    ! 0 |   64 | `					VM_EXIT_ABORT;` |
|      - |   65 | `				}` |
|      6 |   66 | `			}else{` |
|      - |   67 | `				SyString sSetName;` |
|      3 |   68 | `				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));` |
|      3 |   69 | `				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);` |
|      - |   70 | `			}` |
|     13 |   71 | `			SyBlobRelease(&sPend.sName);` |
|     13 |   72 | `			PH7_ClassInstanceUnref(sPend.pThis);` |
|     13 |   73 | `			PH7_MemObjStore(pTos,pNos);` |
|     13 |   74 | `			pNos->nIdx = SXU32_HIGH;` |
|     13 |   75 | `			VmPopOperand(&pTos,1);` |
|     13 |   76 | `			VM_EXIT_BREAK;` |
|      - |   77 | `		}` |
|    ! 0 |   78 | `	}` |
|      - |   79 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|      - |   80 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|      - |   81 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|     99 |   82 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|     10 |   83 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|     10 |   84 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |   85 | `			"offsetSet",sizeof("offsetSet")-1);` |
|      - |   86 | `		ph7_value *apArg[2];` |
|     10 |   87 | `		apArg[0] = &pVm->sCoalesceKey;` |
|     10 |   88 | `		apArg[1] = pTos;` |
|     10 |   89 | `		if( pSet == 0 ){` |
|      - |   90 | `			/* A container that answered the READ and has nowhere to put the write:` |
|      - |   91 | `			 * a class carrying a native dimension handler (ph7_class::xDim) and no` |
|      - |   92 | `			 * ArrayAccess, which is php's DOMNodeList. The store is the same one` |
|      - |   93 | ``			 * `$list[9] = 'x'` performs, so it takes the same Error. */`` |
|      - |   94 | `			char zMsg[256];` |
|      3 |   95 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |   96 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |   97 | `				"Cannot use object of type %.*s as array",` |
|      2 |   98 | `				(int)pName->nByte,pName->zString);` |
|      3 |   99 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|      3 |  100 | `			VmPopOperand(&pTos,1);` |
|      3 |  101 | `			PH7_MemObjRelease(pTos);` |
|      3 |  102 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  103 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  104 | `			VmCoalesceDisarm(pVm);` |
|      3 |  105 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  106 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  107 | `		}` |
|      8 |  108 | `		PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|      - |  109 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|      8 |  110 | `		PH7_MemObjStore(pTos,pNos);` |
|      8 |  111 | `		VmPopOperand(&pTos,1);` |
|      - |  112 | `		/* Disarm and release the cached instance ref + key. */` |
|      8 |  113 | `		VmCoalesceDisarm(pVm);` |
|      8 |  114 | `		VM_EXIT_BREAK;` |
|      - |  115 | `	}` |
|     90 |  116 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|      - |  117 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|      - |  118 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|      - |  119 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|      - |  120 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|      - |  121 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|      - |  122 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|      - |  123 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|      - |  124 | `		 * offset the quiet peek let through raises at the store. */` |
|     43 |  125 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|     64 |  126 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|     42 |  127 | `			? (ph7_value *)SySetAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|     43 |  128 | `		sxi64 iOfft = 0;` |
|      - |  129 | `		SyBlob sTypeMsg;` |
|      - |  130 | `		int eOfft;` |
|     43 |  131 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|     43 |  132 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|      - |  133 | `			&iOfft,&sTypeMsg);` |
|     43 |  134 | `		if( eOfft == VM_STROFF_REJECT ){` |
|      - |  135 | `			sxi32 rcSo;` |
|      3 |  136 | `			VmPopOperand(&pTos,1);` |
|      3 |  137 | `			PH7_MemObjRelease(pTos);` |
|      3 |  138 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  139 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  140 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      3 |  141 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  142 | `			rc = rcSo;` |
|      3 |  143 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  144 | `		}` |
|     41 |  145 | `		SyBlobRelease(&sTypeMsg);` |
|      - |  146 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|      - |  147 | `		{` |
|     41 |  148 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     41 |  149 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      - |  150 | `		}` |
|     41 |  151 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|     41 |  152 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|      - |  153 | `				sxi32 rcEm;` |
|      5 |  154 | `				VmPopOperand(&pTos,1);` |
|      5 |  155 | `				PH7_MemObjRelease(pTos);` |
|      5 |  156 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  157 | `				pTos->nIdx = SXU32_HIGH;` |
|      5 |  158 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|      - |  159 | `					"Cannot assign an empty string to a string offset",` |
|      - |  160 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|      5 |  161 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  162 | `				rc = rcEm;` |
|      5 |  163 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  164 | `			}` |
|     18 |  165 | `		}` |
|      - |  166 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|      - |  167 | `		 * not free what the carrier owns, so release it here — every other exit` |
|      - |  168 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|     37 |  169 | `		VmFreeCoalStrOff(pCoalOff);` |
|     37 |  170 | `		pNos->x.pOther = 0;` |
|     37 |  171 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|      - |  172 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|     37 |  173 | `		PH7_MemObjStore(pTos,pNos);` |
|     37 |  174 | `		pNos->nIdx = SXU32_HIGH;` |
|     37 |  175 | `		VmPopOperand(&pTos,1);` |
|     37 |  176 | `		VM_EXIT_BREAK;` |
|      - |  177 | `	}` |
|     48 |  178 | `	nIdx = pNos->nIdx;` |
|     48 |  179 | `	if( nIdx == SXU32_HIGH ){` |
|    ! 0 |  180 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  181 | `			"Cannot perform assignment on a constant class attribute");` |
|     48 |  182 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     48 |  183 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     48 |  184 | `		PH7_MemObjStore(pTos,pObj);` |
|     23 |  185 | `	}` |
|     48 |  186 | `	PH7_MemObjStore(pTos,pNos);` |
|     48 |  187 | `	VmPopOperand(&pTos,1);` |
|     48 |  188 | `	VM_EXIT_BREAK;` |
|    ! 0 |  189 | `	VM_EXIT_BREAK;` |
|     57 |  190 | `}` |
|      - |  191 |  |
|      - |  192 | `/*` |
|      - |  193 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|      - |  194 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  195 | ` */` |
|    542 |  196 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |  197 | `{` |
|    545 |  198 | `	ph7_value *pTos = pState->pTos;` |
|    545 |  199 | `	ph7_value *pStack = pState->pStack;` |
|    545 |  200 | `	VmInstr *aInstr = pState->aInstr;` |
|    545 |  201 | `	sxi32 pc = pState->pc;` |
|      - |  202 | `	sxi32 rc;` |
|    271 |  203 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    545 |  204 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  205 | `	{` |
|      - |  206 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  207 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  208 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  209 | `		SyBlob sArMsg;` |
|    545 |  210 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    545 |  211 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){` |
|      - |  212 | `			sxi32 rcAr;` |
|    ! 0 |  213 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  214 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  215 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  216 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  217 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  218 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  219 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  220 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  221 | `			rc = rcAr;` |
|    ! 0 |  222 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  223 | `		}` |
|    545 |  224 | `		SyBlobRelease(&sArMsg);` |
|      - |  225 | `	}` |
|      - |  226 | `	ph7_real a,b,r;` |
|      - |  227 | `#ifdef UNTRUST` |
|      - |  228 | `	if( pNos < pStack ){` |
|      - |  229 | `		VM_EXIT_ABORT;` |
|      - |  230 | `	}` |
|      - |  231 | `#endif` |
|      - |  232 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|      - |  233 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|      - |  234 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|      - |  235 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|      - |  236 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|    545 |  237 | `	PH7_MemObjToNumeric(pTos);` |
|    545 |  238 | `	PH7_MemObjToNumeric(pNos);` |
|    545 |  239 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|    480 |  240 | `		sxi64 ia = pNos->x.iVal;` |
|    480 |  241 | `		sxi64 ib = pTos->x.iVal;` |
|    480 |  242 | `		sxi64 iQuot = 0;` |
|    480 |  243 | `		int bExact = 0;` |
|    480 |  244 | `		if( ib == 0 ){` |
|      8 |  245 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|     12 |  246 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|    474 |  247 | `		}else if( ib == -1 ){` |
|      - |  248 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|      - |  249 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|      - |  250 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|      - |  251 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|      - |  252 | `			 * alongside the remainder, so testing the remainder first trapped` |
|      - |  253 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|      - |  254 | `			 * OP_MOD screens the same hazard the same way. */` |
|      - |  255 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|      - |  256 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|      - |  257 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|      - |  258 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|      - |  259 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|      - |  260 | `			bExact = 1;` |
|      - |  261 | `#else` |
|     19 |  262 | `			if( ia != SMALLEST_INT64 ){` |
|     15 |  263 | `				iQuot = -ia;` |
|     15 |  264 | `				bExact = 1;` |
|      8 |  265 | `			}` |
|      - |  266 | `#endif` |
|    465 |  267 | `		}else if( ia % ib == 0 ){` |
|     24 |  268 | `			iQuot = ia / ib;` |
|     24 |  269 | `			bExact = 1;` |
|     11 |  270 | `		}` |
|    474 |  271 | `		if( bExact ){` |
|     38 |  272 | `			pNos->x.iVal = iQuot;` |
|     38 |  273 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     38 |  274 | `			VmPopOperand(&pTos,1);` |
|     38 |  275 | `			VM_EXIT_BREAK;` |
|      - |  276 | `		}` |
|    218 |  277 | `	}` |
|      - |  278 | `	/* Force the operands to be real */` |
|    502 |  279 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    489 |  280 | `		PH7_MemObjToReal(pTos);` |
|    244 |  281 | `	}` |
|    502 |  282 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    437 |  283 | `		PH7_MemObjToReal(pNos);` |
|    218 |  284 | `	}` |
|      - |  285 | `	/* Perform the requested operation */` |
|    502 |  286 | `	a = pNos->rVal;` |
|    502 |  287 | `	b = pTos->rVal;` |
|    502 |  288 | `	if( b == 0 ){` |
|      - |  289 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  290 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      3 |  291 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      3 |  292 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  293 | `	}else{` |
|    500 |  294 | `		r = a/b;` |
|      - |  295 | `		/* Push the result */` |
|    500 |  296 | `		pNos->rVal = r;` |
|    500 |  297 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  298 | `	}` |
|    500 |  299 | `	VmPopOperand(&pTos,1);` |
|    500 |  300 | `	VM_EXIT_BREAK;` |
|    ! 0 |  301 | `	VM_EXIT_BREAK;` |
|    274 |  302 | `}` |
|      - |  303 |  |
|      - |  304 | `/*` |
|      - |  305 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|      - |  306 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  307 | ` */` |
|     10 |  308 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  309 | `{` |
|     11 |  310 | `	ph7_value *pTos = pState->pTos;` |
|     11 |  311 | `	ph7_value *pStack = pState->pStack;` |
|     11 |  312 | `	VmInstr *aInstr = pState->aInstr;` |
|     11 |  313 | `	sxi32 pc = pState->pc;` |
|      - |  314 | `	sxi32 rc;` |
|      5 |  315 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     11 |  316 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  317 | `	ph7_value *pObj;` |
|      - |  318 | `	sxi64 a,b,r;` |
|      - |  319 | `#ifdef UNTRUST` |
|      - |  320 | `	if( pNos < pStack ){` |
|      - |  321 | `		VM_EXIT_ABORT;` |
|      - |  322 | `	}` |
|      - |  323 | `#endif` |
|      - |  324 | `	{` |
|      - |  325 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  326 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  327 | `		SyBlob sArMsg;` |
|     11 |  328 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     11 |  329 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  330 | `			sxi32 rcAr;` |
|    ! 0 |  331 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  332 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  333 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  334 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  335 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  336 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  337 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  338 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  339 | `			rc = rcAr;` |
|    ! 0 |  340 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  341 | `		}` |
|     11 |  342 | `		SyBlobRelease(&sArMsg);` |
|      - |  343 | `	}` |
|      - |  344 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     11 |  345 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     11 |  346 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     11 |  347 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     11 |  348 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     11 |  349 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  350 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 |  351 | `	}` |
|     11 |  352 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  353 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  354 | `	}` |
|      - |  355 | `	/* Perform the requested operation */` |
|     11 |  356 | `	a = pTos->x.iVal;` |
|     11 |  357 | `	b = pNos->x.iVal;` |
|     11 |  358 | `	if( b == 0 ){` |
|      - |  359 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  360 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      5 |  361 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      5 |  362 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  363 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|      7 |  364 | `	}else if( b == -1 ){` |
|      - |  365 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|      - |  366 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|      3 |  367 | `		r = 0;` |
|      2 |  368 | `	}else{` |
|      5 |  369 | `		r = a%b;` |
|      - |  370 | `	}` |
|      - |  371 | `	/* Push the result */` |
|      7 |  372 | `	pNos->x.iVal = r;` |
|      7 |  373 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      7 |  374 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  375 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      7 |  376 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      7 |  377 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      7 |  378 | `		PH7_MemObjStore(pNos,pObj);` |
|      3 |  379 | `	}` |
|      7 |  380 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      7 |  381 | `	VmPopOperand(&pTos,1);` |
|      7 |  382 | `	VM_EXIT_BREAK;` |
|    ! 0 |  383 | `	VM_EXIT_BREAK;` |
|      6 |  384 | `}` |
|      - |  385 |  |
|      - |  386 | `/*` |
|      - |  387 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|      - |  388 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  389 | ` */` |
|   1056 |  390 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  391 | `{` |
|   1061 |  392 | `	ph7_value *pTos = pState->pTos;` |
|   1061 |  393 | `	ph7_value *pStack = pState->pStack;` |
|   1061 |  394 | `	VmInstr *aInstr = pState->aInstr;` |
|   1061 |  395 | `	sxi32 pc = pState->pc;` |
|      - |  396 | `	sxi32 rc;` |
|    528 |  397 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1061 |  398 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  399 | `	{` |
|      - |  400 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  401 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  402 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  403 | `		SyBlob sArMsg;` |
|   1061 |  404 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   1061 |  405 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  406 | `			sxi32 rcAr;` |
|      3 |  407 | `			VmPopOperand(&pTos,1);` |
|      3 |  408 | `			PH7_MemObjRelease(pTos);` |
|      3 |  409 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  410 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  411 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  412 | `				SyBlobLength(&sArMsg));` |
|      3 |  413 | `			SyBlobRelease(&sArMsg);` |
|      4 |  414 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  415 | `			rc = rcAr;` |
|      5 |  416 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  417 | `		}` |
|   1059 |  418 | `		SyBlobRelease(&sArMsg);` |
|      - |  419 | `	}` |
|      - |  420 | `	sxi64 a,b,r;` |
|      - |  421 | `#ifdef UNTRUST` |
|      - |  422 | `	if( pNos < pStack ){` |
|      - |  423 | `		VM_EXIT_ABORT;` |
|      - |  424 | `	}` |
|      - |  425 | `#endif` |
|      - |  426 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|   1059 |  427 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|   1059 |  428 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1045 |  429 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|   1045 |  430 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|   1043 |  431 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 |  432 | `		PH7_MemObjToInteger(pTos);` |
|      1 |  433 | `	}` |
|   1043 |  434 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      7 |  435 | `		PH7_MemObjToInteger(pNos);` |
|      3 |  436 | `	}` |
|      - |  437 | `	/* Perform the requested operation */` |
|   1043 |  438 | `	a = pNos->x.iVal;` |
|   1043 |  439 | `	b = pTos->x.iVal;` |
|   1043 |  440 | `	if( b == 0 ){` |
|      - |  441 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  442 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|     10 |  443 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|     18 |  444 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  445 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|   1035 |  446 | `	}else if( b == -1 ){` |
|      - |  447 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|      - |  448 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|      - |  449 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|      - |  450 | `		 * remainder. php's result here is 0. */` |
|      5 |  451 | `		r = 0;` |
|      3 |  452 | `	}else{` |
|   1031 |  453 | `		r = a%b;` |
|      - |  454 | `	}` |
|      - |  455 | `	/* Push the result */` |
|   1035 |  456 | `	pNos->x.iVal = r;` |
|   1035 |  457 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|   1035 |  458 | `	VmPopOperand(&pTos,1);` |
|   1035 |  459 | `	VM_EXIT_BREAK;` |
|    ! 0 |  460 | `	VM_EXIT_BREAK;` |
|    533 |  461 | `}` |
|      - |  462 |  |
|      - |  463 | `/*` |
|      - |  464 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|      - |  465 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  466 | ` */` |
|     14 |  467 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 |  468 | `{` |
|     16 |  469 | `	ph7_value *pTos = pState->pTos;` |
|     16 |  470 | `	ph7_value *pStack = pState->pStack;` |
|     16 |  471 | `	VmInstr *aInstr = pState->aInstr;` |
|     16 |  472 | `	sxi32 pc = pState->pc;` |
|      - |  473 | `	sxi32 rc;` |
|      7 |  474 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     16 |  475 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  476 | `	ph7_value *pObj;` |
|      - |  477 | `#ifdef UNTRUST` |
|      - |  478 | `	if( pNos < pStack ){` |
|      - |  479 | `		VM_EXIT_ABORT;` |
|      - |  480 | `	}` |
|      - |  481 | `#endif` |
|      - |  482 | `	{` |
|      - |  483 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  484 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  485 | `		SyBlob sArMsg;` |
|     16 |  486 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     16 |  487 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  488 | `			sxi32 rcAr;` |
|      3 |  489 | `			VmPopOperand(&pTos,1);` |
|      3 |  490 | `			PH7_MemObjRelease(pTos);` |
|      3 |  491 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  492 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  493 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  494 | `				SyBlobLength(&sArMsg));` |
|      3 |  495 | `			SyBlobRelease(&sArMsg);` |
|      4 |  496 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  497 | `			rc = rcAr;` |
|      3 |  498 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  499 | `		}` |
|     14 |  500 | `		SyBlobRelease(&sArMsg);` |
|      - |  501 | `	}` |
|      - |  502 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     14 |  503 | `	PH7_MemObjToNumeric(pTos);` |
|     14 |  504 | `	PH7_MemObjToNumeric(pNos);` |
|     14 |  505 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  506 | `		/* Floating point arithemic */` |
|      - |  507 | `		ph7_real a,b,r;` |
|    ! 0 |  508 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  509 | `			PH7_MemObjToReal(pTos);` |
|    ! 0 |  510 | `		}` |
|    ! 0 |  511 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  512 | `			PH7_MemObjToReal(pNos);` |
|    ! 0 |  513 | `		}` |
|    ! 0 |  514 | `		a = pTos->rVal;` |
|    ! 0 |  515 | `		b = pNos->rVal;` |
|    ! 0 |  516 | `		r = a - b;` |
|      - |  517 | `		/* Push the result */` |
|    ! 0 |  518 | `		pNos->rVal = r;` |
|    ! 0 |  519 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  520 | `		/* Try to get an integer representation */` |
|    ! 0 |  521 | `		PH7_MemObjTryInteger(pNos);` |
|    ! 0 |  522 | `	}else{` |
|      - |  523 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  524 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  525 | `		sxi64 a,b,r;` |
|     14 |  526 | `		a = pTos->x.iVal;` |
|     14 |  527 | `		b = pNos->x.iVal;` |
|     14 |  528 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  529 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      3 |  530 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      3 |  531 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  532 | `#else` |
|      - |  533 | `			pNos->x.iVal = r;` |
|      - |  534 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  535 | `#endif` |
|      2 |  536 | `		}else{` |
|     12 |  537 | `			pNos->x.iVal = r;` |
|     12 |  538 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  539 | `		}` |
|      - |  540 | `	}` |
|     14 |  541 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  542 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     14 |  543 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     14 |  544 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     14 |  545 | `		PH7_MemObjStore(pNos,pObj);` |
|      6 |  546 | `	}` |
|     14 |  547 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     14 |  548 | `	VmPopOperand(&pTos,1);` |
|     14 |  549 | `	VM_EXIT_BREAK;` |
|    ! 0 |  550 | `	VM_EXIT_BREAK;` |
|      9 |  551 | `}` |
|      - |  552 |  |
|      - |  553 | `/*` |
|      - |  554 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|      - |  555 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  556 | ` */` |
|  50481 |  557 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  558 | `{` |
|  50486 |  559 | `	ph7_value *pTos = pState->pTos;` |
|  50486 |  560 | `	ph7_value *pStack = pState->pStack;` |
|  50486 |  561 | `	VmInstr *aInstr = pState->aInstr;` |
|  50486 |  562 | `	sxi32 pc = pState->pc;` |
|      - |  563 | `	sxi32 rc;` |
|  25409 |  564 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  50486 |  565 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  566 | `#ifdef UNTRUST` |
|      - |  567 | `	if( pNos < pStack ){` |
|      - |  568 | `		VM_EXIT_ABORT;` |
|      - |  569 | `	}` |
|      - |  570 | `#endif` |
|      - |  571 | `	{` |
|      - |  572 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  573 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  574 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  575 | `		SyBlob sArMsg;` |
|  50486 |  576 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|  50486 |  577 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  578 | `			sxi32 rcAr;` |
|    ! 0 |  579 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  580 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  581 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  582 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  583 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  584 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  585 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  586 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  587 | `			rc = rcAr;` |
|    ! 0 |  588 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  589 | `		}` |
|  50486 |  590 | `		SyBlobRelease(&sArMsg);` |
|      - |  591 | `	}` |
|      - |  592 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|      - |  593 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|      - |  594 | `	 * quietly evaluated to 0. */` |
|  50486 |  595 | `	PH7_MemObjToNumeric(pTos);` |
|  50486 |  596 | `	PH7_MemObjToNumeric(pNos);` |
|  50486 |  597 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  598 | `		/* Floating point arithemic */` |
|      - |  599 | `		ph7_real a,b,r;` |
|    142 |  600 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     21 |  601 | `			PH7_MemObjToReal(pTos);` |
|     10 |  602 | `		}` |
|    142 |  603 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      9 |  604 | `			PH7_MemObjToReal(pNos);` |
|      4 |  605 | `		}` |
|    142 |  606 | `		a = pNos->rVal;` |
|    142 |  607 | `		b = pTos->rVal;` |
|    142 |  608 | `		r = a - b;` |
|      - |  609 | `		/* Push the result */` |
|    142 |  610 | `		pNos->rVal = r;` |
|    142 |  611 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  612 | `		/* Try to get an integer representation */` |
|    142 |  613 | `		PH7_MemObjTryInteger(pNos);` |
|     72 |  614 | `	}else{` |
|      - |  615 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  616 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  617 | `		sxi64 a,b,r;` |
|  50346 |  618 | `		a = pNos->x.iVal;` |
|  50346 |  619 | `		b = pTos->x.iVal;` |
|  50346 |  620 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  621 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      7 |  622 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      7 |  623 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  624 | `#else` |
|      - |  625 | `			pNos->x.iVal = r;` |
|      - |  626 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  627 | `#endif` |
|      4 |  628 | `		}else{` |
|  50340 |  629 | `			pNos->x.iVal = r;` |
|  50340 |  630 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  631 | `		}` |
|      - |  632 | `	}` |
|  50486 |  633 | `	VmPopOperand(&pTos,1);` |
|  50486 |  634 | `	VM_EXIT_BREAK;` |
|    ! 0 |  635 | `	VM_EXIT_BREAK;` |
|  25414 |  636 | `}` |
|      - |  637 |  |
|      - |  638 | `/*` |
|      - |  639 | `` * php's `**`, as a value operation.`` |
|      - |  640 | ` *` |
|      - |  641 | ` * In php pow() IS the exponentiation operator -- both compile to the same` |
|      - |  642 | ` * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and` |
|      - |  643 | ` * every edge value have to come from ONE place here too. This is that place:` |
|      - |  644 | ` * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call` |
|      - |  645 | ` * it, after the caller has run VmArithOperandCheck() over the two operands (the` |
|      - |  646 | ` * two sites word their throw differently -- one settles an operand stack first,` |
|      - |  647 | ` * the other is inside a C builtin -- so the CHECK stays with the caller and only` |
|      - |  648 | ` * the arithmetic is shared).` |
|      - |  649 | ` *` |
|      - |  650 | ` * pBase and pExp are converted in place, which is what the opcode arm already` |
|      - |  651 | ` * did to its stack slots; pOut may alias either of them and is written last.` |
|      - |  652 | ` */` |
|    222 |  653 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)` |
|      3 |  654 | `{` |
|      - |  655 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  656 | `	int bBothInt;` |
|    225 |  657 | `	int usedInt = 0;` |
|      - |  658 | `	ph7_real a, b, r;` |
|      - |  659 | `#endif` |
|    225 |  660 | `	sxi64 base_i = 0, exp_i = 0;` |
|    225 |  661 | `	PH7_MemObjToNumeric(pBase);` |
|    225 |  662 | `	PH7_MemObjToNumeric(pExp);` |
|      - |  663 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    413 |  664 | `	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&` |
|    188 |  665 | `	           ((pExp->iFlags & MEMOBJ_REAL) == 0);` |
|    225 |  666 | `	if( bBothInt ){` |
|    185 |  667 | `		base_i = pBase->x.iVal;` |
|    185 |  668 | `		exp_i  = pExp->x.iVal;` |
|     91 |  669 | `	}` |
|    225 |  670 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|    191 |  671 | `		PH7_MemObjToReal(pBase);` |
|     94 |  672 | `	}` |
|    225 |  673 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|    219 |  674 | `		PH7_MemObjToReal(pExp);` |
|    108 |  675 | `	}` |
|    225 |  676 | `	a = pBase->rVal;` |
|    225 |  677 | `	b = pExp->rVal;` |
|    225 |  678 | `	r = pow(a, b);` |
|      - |  679 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|      - |  680 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|      - |  681 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|      - |  682 | `	 * representable as double but not as signed int64. */` |
|    225 |  683 | `	if( bBothInt && exp_i >= 0 ){` |
|    175 |  684 | `		sxi64 result_i = 1;` |
|    175 |  685 | `		sxi64 cur_base = base_i;` |
|    175 |  686 | `		sxi64 cur_exp  = exp_i;` |
|    175 |  687 | `		int overflow = 0;` |
|    583 |  688 | `		while( cur_exp > 0 ){` |
|    427 |  689 | `			if( cur_exp & 1 ){` |
|    275 |  690 | `				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){` |
|      7 |  691 | `					overflow = 1;` |
|      7 |  692 | `					break;` |
|      - |  693 | `				}` |
|    133 |  694 | `			}` |
|    421 |  695 | `			cur_exp >>= 1;` |
|    421 |  696 | `			if( cur_exp > 0 ){` |
|    279 |  697 | `				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){` |
|     11 |  698 | `					overflow = 1;` |
|     11 |  699 | `					break;` |
|      - |  700 | `				}` |
|    133 |  701 | `			}` |
|      3 |  702 | `		}` |
|    175 |  703 | `		if( !overflow ){` |
|    159 |  704 | `			pOut->x.iVal = result_i;` |
|    159 |  705 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|    159 |  706 | `			usedInt = 1;` |
|     78 |  707 | `		}` |
|     86 |  708 | `	}` |
|    225 |  709 | `	if( !usedInt ){` |
|     67 |  710 | `		pOut->rVal = r;` |
|     67 |  711 | `		MemObjSetType(pOut, MEMOBJ_REAL);` |
|     33 |  712 | `	}` |
|      - |  713 | `#else` |
|      - |  714 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|      - |  715 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|      - |  716 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|      - |  717 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|      - |  718 | `	 * represented. */` |
|      - |  719 | `	base_i = pBase->x.iVal;` |
|      - |  720 | `	exp_i  = pExp->x.iVal;` |
|      - |  721 | `	{` |
|      - |  722 | `		sxi64 result_i = 1;` |
|      - |  723 | `		sxi64 cur_base = base_i;` |
|      - |  724 | `		sxi64 cur_exp  = exp_i;` |
|      - |  725 | `		if( cur_exp < 0 ){` |
|      - |  726 | `			result_i = 0;` |
|      - |  727 | `		}else{` |
|      - |  728 | `			while( cur_exp > 0 ){` |
|      - |  729 | `				if( cur_exp & 1 ){` |
|      - |  730 | `					result_i *= cur_base;` |
|      - |  731 | `				}` |
|      - |  732 | `				cur_exp >>= 1;` |
|      - |  733 | `				if( cur_exp > 0 ){` |
|      - |  734 | `					cur_base *= cur_base;` |
|      - |  735 | `				}` |
|      - |  736 | `			}` |
|      - |  737 | `		}` |
|      - |  738 | `		pOut->x.iVal = result_i;` |
|      - |  739 | `		MemObjSetType(pOut, MEMOBJ_INT);` |
|      - |  740 | `	}` |
|      - |  741 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    225 |  742 | `}` |
|      - |  743 | `/*` |
|      - |  744 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|      - |  745 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  746 | ` */` |
|    186 |  747 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  748 | `{` |
|    187 |  749 | `	ph7_value *pTos = pState->pTos;` |
|    187 |  750 | `	ph7_value *pStack = pState->pStack;` |
|    187 |  751 | `	VmInstr *aInstr = pState->aInstr;` |
|    187 |  752 | `	sxi32 pc = pState->pc;` |
|      - |  753 | `	sxi32 rc;` |
|     93 |  754 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    187 |  755 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  756 | `	{` |
|      - |  757 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  758 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  759 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  760 | `		SyBlob sArMsg;` |
|    187 |  761 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    187 |  762 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){` |
|      - |  763 | `			sxi32 rcAr;` |
|      5 |  764 | `			VmPopOperand(&pTos,1);` |
|      5 |  765 | `			PH7_MemObjRelease(pTos);` |
|      5 |  766 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  767 | `			pTos->nIdx = SXU32_HIGH;` |
|      7 |  768 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      2 |  769 | `				SyBlobLength(&sArMsg));` |
|      5 |  770 | `			SyBlobRelease(&sArMsg);` |
|      7 |  771 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  772 | `			rc = rcAr;` |
|      5 |  773 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  774 | `		}` |
|    183 |  775 | `		SyBlobRelease(&sArMsg);` |
|      - |  776 | `	}` |
|    183 |  777 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|      - |  778 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|      - |  779 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|      - |  780 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|      - |  781 | `	 */` |
|    183 |  782 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    183 |  783 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|      - |  784 | `#ifdef UNTRUST` |
|      - |  785 | `	if( pNos < pStack ){` |
|      - |  786 | `		VM_EXIT_ABORT;` |
|      - |  787 | `	}` |
|      - |  788 | `#endif` |
|    183 |  789 | `	PH7_MemObjPow(pBase,pExp,pNos);` |
|    183 |  790 | `	if( bStore ){` |
|      - |  791 | `		ph7_value *pObj;` |
|     25 |  792 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  793 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     25 |  794 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     25 |  795 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     25 |  796 | `			PH7_MemObjStore(pNos,pObj);` |
|     12 |  797 | `		}` |
|     12 |  798 | `	}` |
|    183 |  799 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    183 |  800 | `	VmPopOperand(&pTos,1);` |
|    183 |  801 | `	VM_EXIT_BREAK;` |
|    ! 0 |  802 | `	VM_EXIT_BREAK;` |
|     94 |  803 | `}` |
|      - |  804 |  |
|      - |  805 | `/*` |
|      - |  806 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|      - |  807 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  808 | ` */` |
|    714 |  809 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      4 |  810 | `{` |
|    718 |  811 | `	ph7_value *pTos = pState->pTos;` |
|    718 |  812 | `	ph7_value *pStack = pState->pStack;` |
|    718 |  813 | `	VmInstr *aInstr = pState->aInstr;` |
|    718 |  814 | `	sxi32 pc = pState->pc;` |
|      - |  815 | `	sxi32 rc;` |
|    357 |  816 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    718 |  817 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  818 | `#ifdef UNTRUST` |
|      - |  819 | `	if( pNos < pStack ){` |
|      - |  820 | `		VM_EXIT_ABORT;` |
|      - |  821 | `	}` |
|      - |  822 | `#endif` |
|      - |  823 | `	/* php answers the UNORDERED comparison -- a NaN, or two arrays neither of` |
|      - |  824 | `	 * which contains the other -- with 1 whichever way round it is asked, and` |
|      - |  825 | `	 * PH7_MemObjCmp does the same, so the spaceship needs no case of its own. */` |
|    718 |  826 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    718 |  827 | `	rc = (rc > 0) - (rc < 0);   /* normalize to exactly -1, 0 or 1 */` |
|    718 |  828 | `	VmPopOperand(&pTos,1);` |
|    718 |  829 | `	PH7_MemObjRelease(pTos);` |
|    718 |  830 | `	pTos->x.iVal = rc;` |
|    718 |  831 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|    718 |  832 | `	VM_EXIT_BREAK;` |
|    ! 0 |  833 | `	VM_EXIT_BREAK;` |
|      4 |  834 | `}` |
|      - |  835 |  |
|      - |  836 | `/*` |
|      - |  837 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|      - |  838 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  839 | ` */` |
| 272544 |  840 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  841 | `{` |
| 272549 |  842 | `	ph7_value *pTos = pState->pTos;` |
| 272549 |  843 | `	ph7_value *pStack = pState->pStack;` |
| 272549 |  844 | `	VmInstr *aInstr = pState->aInstr;` |
| 272549 |  845 | `	sxi32 pc = pState->pc;` |
|      - |  846 | `	sxi32 rc;` |
| 136497 |  847 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 272549 |  848 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  849 | `	/* Perform the comparison and act accordingly */` |
|      - |  850 | `#ifdef UNTRUST` |
|      - |  851 | `	if( pNos < pStack ){` |
|      - |  852 | `		VM_EXIT_ABORT;` |
|      - |  853 | `	}` |
|      - |  854 | `#endif` |
|      - |  855 | ``	/* `$a > $b` is php's `$b < $a` -- asked from the OTHER SIDE, not read off`` |
|      - |  856 | `	 * this side's sign. The two differ exactly where the comparison is` |
|      - |  857 | `	 * UNORDERED and php answers 1 both ways (a NaN against a number or a` |
|      - |  858 | `	 * string; two same-sized arrays neither of which contains the other): a` |
|      - |  859 | ``	 * greater-than read off `rc > 0` calls both of those TRUE, where php --`` |
|      - |  860 | ``	 * asking `$b < $a` and getting 1 again -- calls them false, as it does`` |
|      - |  861 | `	 * every other relational operator on such a pair. */` |
| 272549 |  862 | `	rc = PH7_MemObjCmp(pTos,pNos,FALSE,0);` |
| 272549 |  863 | `	if( pInstr->iOp == PH7_OP_GE ){` |
| 271258 |  864 | `		rc = rc <= 0;` |
| 135860 |  865 | `	}else{` |
|   1296 |  866 | `		rc = rc < 0;` |
|      - |  867 | `	}` |
| 272549 |  868 | `	VmPopOperand(&pTos,1);` |
| 272549 |  869 | `	if( !pInstr->iP2 ){` |
|      - |  870 | `		/* Push comparison result without taking the jump */` |
| 272549 |  871 | `		PH7_MemObjRelease(pTos);` |
| 272549 |  872 | `		pTos->x.iVal = rc;` |
|      - |  873 | `		/* Invalidate any prior representation */` |
| 272549 |  874 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 136502 |  875 | `	}else{` |
|    ! 0 |  876 | `		if( rc ){` |
|      - |  877 | `			/* Jump to the desired location */` |
|    ! 0 |  878 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  879 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  880 | `		}` |
|      - |  881 | `	}` |
| 272549 |  882 | `	VM_EXIT_BREAK;` |
|    ! 0 |  883 | `	VM_EXIT_BREAK;` |
|      5 |  884 | `}` |
|      - |  885 |  |
|      - |  886 | `/*` |
|      - |  887 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|      - |  888 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  889 | ` */` |
| 574993 |  890 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  891 | `{` |
| 574998 |  892 | `	ph7_value *pTos = pState->pTos;` |
| 574998 |  893 | `	ph7_value *pStack = pState->pStack;` |
| 574998 |  894 | `	VmInstr *aInstr = pState->aInstr;` |
| 574998 |  895 | `	sxi32 pc = pState->pc;` |
|      - |  896 | `	sxi32 rc;` |
| 288139 |  897 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 574998 |  898 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  899 | `	/* Perform the comparison and act accordingly */` |
|      - |  900 | `#ifdef UNTRUST` |
|      - |  901 | `	if( pNos < pStack ){` |
|      - |  902 | `		VM_EXIT_ABORT;` |
|      - |  903 | `	}` |
|      - |  904 | `#endif` |
|      - |  905 | `	/* An unordered pair answers 1 here too, so both spellings are false for it` |
|      - |  906 | `	 * without a case of their own (see OP_GT/OP_GE above). */` |
| 574998 |  907 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 574998 |  908 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|  80043 |  909 | `		rc = rc < 1;` |
|  40243 |  910 | `	}else{` |
| 494960 |  911 | `		rc = rc < 0;` |
|      - |  912 | `	}` |
| 574998 |  913 | `	VmPopOperand(&pTos,1);` |
| 574998 |  914 | `	if( !pInstr->iP2 ){` |
|      - |  915 | `		/* Push comparison result without taking the jump */` |
| 574998 |  916 | `		PH7_MemObjRelease(pTos);` |
| 574998 |  917 | `		pTos->x.iVal = rc;` |
|      - |  918 | `		/* Invalidate any prior representation */` |
| 574998 |  919 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 288144 |  920 | `	}else{` |
|    ! 0 |  921 | `		if( rc ){` |
|      - |  922 | `			/* Jump to the desired location */` |
|    ! 0 |  923 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  924 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  925 | `		}` |
|      - |  926 | `	}` |
| 574998 |  927 | `	VM_EXIT_BREAK;` |
|    ! 0 |  928 | `	VM_EXIT_BREAK;` |
|      5 |  929 | `}` |
|      - |  930 |  |
|      - |  931 | `/*` |
|      - |  932 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|      - |  933 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  934 | ` */` |
| 642393 |  935 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  936 | `{` |
| 642398 |  937 | `	ph7_value *pTos = pState->pTos;` |
| 642398 |  938 | `	ph7_value *pStack = pState->pStack;` |
| 642398 |  939 | `	VmInstr *aInstr = pState->aInstr;` |
| 642398 |  940 | `	sxi32 pc = pState->pc;` |
|      - |  941 | `	sxi32 rc;` |
| 321419 |  942 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 642398 |  943 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  944 | `	/* Perform the comparison and act accordingly */` |
|      - |  945 | `#ifdef UNTRUST` |
|      - |  946 | `	if( pNos < pStack ){` |
|      - |  947 | `		VM_EXIT_ABORT;` |
|      - |  948 | `	}` |
|      - |  949 | `#endif` |
| 642398 |  950 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 642398 |  951 | `	rc = rc != 0;` |
| 642398 |  952 | `	VmPopOperand(&pTos,1);` |
| 642398 |  953 | `	if( !pInstr->iP2 ){` |
|      - |  954 | `		/* Push comparison result without taking the jump */` |
| 642398 |  955 | `		PH7_MemObjRelease(pTos);` |
| 642398 |  956 | `		pTos->x.iVal = rc;` |
|      - |  957 | `		/* Invalidate any prior representation */` |
| 642398 |  958 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 321424 |  959 | `	}else{` |
|    ! 0 |  960 | `		if( rc ){` |
|      - |  961 | `			/* Jump to the desired location */` |
|    ! 0 |  962 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  963 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  964 | `		}` |
|      - |  965 | `	}` |
| 642398 |  966 | `	VM_EXIT_BREAK;` |
|    ! 0 |  967 | `	VM_EXIT_BREAK;` |
|      5 |  968 | `}` |
|      - |  969 |  |
|      - |  970 | `/*` |
|      - |  971 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|      - |  972 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  973 | ` */` |
| 728878 |  974 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  975 | `{` |
| 728883 |  976 | `	ph7_value *pTos = pState->pTos;` |
| 728883 |  977 | `	ph7_value *pStack = pState->pStack;` |
| 728883 |  978 | `	VmInstr *aInstr = pState->aInstr;` |
| 728883 |  979 | `	sxi32 pc = pState->pc;` |
|      - |  980 | `	sxi32 rc;` |
| 365056 |  981 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 728883 |  982 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  983 | `	/* Perform the comparison and act accordingly */` |
|      - |  984 | `#ifdef UNTRUST` |
|      - |  985 | `	if( pNos < pStack ){` |
|      - |  986 | `		VM_EXIT_ABORT;` |
|      - |  987 | `	}` |
|      - |  988 | `#endif` |
|      - |  989 | ``	/* `NAN === NAN` is false in php, and the comparator says so: an unordered`` |
|      - |  990 | `	 * pair is 1, never 0. */` |
| 728883 |  991 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 728883 |  992 | `	rc = rc == 0;` |
| 728883 |  993 | `	VmPopOperand(&pTos,1);` |
| 728883 |  994 | `	if( !pInstr->iP2 ){` |
|      - |  995 | `		/* Push comparison result without taking the jump */` |
| 728883 |  996 | `		PH7_MemObjRelease(pTos);` |
| 728883 |  997 | `		pTos->x.iVal = rc;` |
|      - |  998 | `		/* Invalidate any prior representation */` |
| 728883 |  999 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 365061 | 1000 | `	}else{` |
|    ! 0 | 1001 | `		if( rc ){` |
|      - | 1002 | `			/* Jump to the desired location */` |
|    ! 0 | 1003 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 | 1004 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1005 | `		}` |
|      - | 1006 | `	}` |
| 728883 | 1007 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1008 | `	VM_EXIT_BREAK;` |
|      5 | 1009 | `}` |
|      - | 1010 |  |
|      - | 1011 | `/*` |
|      - | 1012 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|      - | 1013 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1014 | ` */` |
|  13586 | 1015 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1016 | `{` |
|  13591 | 1017 | `	ph7_value *pTos = pState->pTos;` |
|  13591 | 1018 | `	ph7_value *pStack = pState->pStack;` |
|  13591 | 1019 | `	VmInstr *aInstr = pState->aInstr;` |
|  13591 | 1020 | `	sxi32 pc = pState->pc;` |
|      - | 1021 | `	sxi32 rc;` |
|   6790 | 1022 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  13591 | 1023 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1024 | `	/* Perform the comparison and act accordingly */` |
|      - | 1025 | `#ifdef UNTRUST` |
|      - | 1026 | `	if( pNos < pStack ){` |
|      - | 1027 | `		VM_EXIT_ABORT;` |
|      - | 1028 | `	}` |
|      - | 1029 | `#endif` |
|  13591 | 1030 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|  13591 | 1031 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|  12177 | 1032 | `		rc = rc == 0;` |
|   6089 | 1033 | `	}else{` |
|   1419 | 1034 | `		rc = rc != 0;` |
|      - | 1035 | `	}` |
|  13591 | 1036 | `	VmPopOperand(&pTos,1);` |
|  13591 | 1037 | `	if( !pInstr->iP2 ){` |
|      - | 1038 | `		/* Push comparison result without taking the jump */` |
|  13591 | 1039 | `		PH7_MemObjRelease(pTos);` |
|  13591 | 1040 | `		pTos->x.iVal = rc;` |
|      - | 1041 | `		/* Invalidate any prior representation */` |
|  13591 | 1042 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   6795 | 1043 | `	}else{` |
|    ! 0 | 1044 | `		if( rc ){` |
|      - | 1045 | `			/* Jump to the desired location */` |
|    ! 0 | 1046 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 | 1047 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1048 | `		}` |
|      - | 1049 | `	}` |
|  13591 | 1050 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1051 | `	VM_EXIT_BREAK;` |
|      5 | 1052 | `}` |
|      - | 1053 |  |
|      - | 1054 | `/*` |
|      - | 1055 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|      - | 1056 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1057 | ` */` |
| 345481 | 1058 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1059 | `{` |
| 345486 | 1060 | `	ph7_value *pTos = pState->pTos;` |
| 345486 | 1061 | `	ph7_value *pStack = pState->pStack;` |
| 345486 | 1062 | `	VmInstr *aInstr = pState->aInstr;` |
| 345486 | 1063 | `	sxi32 pc = pState->pc;` |
|      - | 1064 | `	sxi32 rc;` |
| 172959 | 1065 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 345486 | 1066 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1067 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|      - | 1068 | `#ifdef UNTRUST` |
|      - | 1069 | `	if( pNos < pStack ){` |
|      - | 1070 | `		VM_EXIT_ABORT;` |
|      - | 1071 | `	}` |
|      - | 1072 | `#endif` |
|      - | 1073 | `	/* Force a boolean cast */` |
| 345486 | 1074 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      5 | 1075 | `		PH7_MemObjToBool(pTos);` |
|      2 | 1076 | `	}` |
| 345486 | 1077 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 | 1078 | `		PH7_MemObjToBool(pNos);` |
|    ! 0 | 1079 | `	}` |
| 345486 | 1080 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
| 345486 | 1081 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
| 345486 | 1082 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|      - | 1083 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|  62367 | 1084 | `		v1 = and_logic[v1*3+v2];` |
|  31187 | 1085 | `	}else{` |
|      - | 1086 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
| 283124 | 1087 | `		v1 = or_logic[v1*3+v2];` |
|      - | 1088 | `	}` |
| 345486 | 1089 | `	if( v1 == 2 ){` |
|    ! 0 | 1090 | `		v1 = 1;` |
|    ! 0 | 1091 | `	}` |
| 345486 | 1092 | `	VmPopOperand(&pTos,1);` |
| 345486 | 1093 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
| 345486 | 1094 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 345486 | 1095 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1096 | `	VM_EXIT_BREAK;` |
|      5 | 1097 | `}` |
|      - | 1098 |  |
|      - | 1099 | `/*` |
|      - | 1100 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|      - | 1101 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1102 | ` */` |
|     36 | 1103 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 | 1104 | `{` |
|     37 | 1105 | `	ph7_value *pTos = pState->pTos;` |
|     37 | 1106 | `	ph7_value *pStack = pState->pStack;` |
|     37 | 1107 | `	VmInstr *aInstr = pState->aInstr;` |
|     37 | 1108 | `	sxi32 pc = pState->pc;` |
|      - | 1109 | `	sxi32 rc;` |
|     18 | 1110 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     37 | 1111 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1112 | `	ph7_value *pObj;` |
|      - | 1113 | `	sxi64 a,r;` |
|      - | 1114 | `#ifdef UNTRUST` |
|      - | 1115 | `	if( pNos < pStack ){` |
|      - | 1116 | `		VM_EXIT_ABORT;` |
|      - | 1117 | `	}` |
|      - | 1118 | `#endif` |
|      - | 1119 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|      - | 1120 | `	 * this handler, beside the other eleven compound stores.) */` |
|     40 | 1121 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|      - | 1122 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     31 | 1123 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     31 | 1124 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     31 | 1125 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     31 | 1126 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     31 | 1127 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1128 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 | 1129 | `	}` |
|     31 | 1130 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1131 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 | 1132 | `	}` |
|      - | 1133 | `	/* Perform the requested operation */` |
|     31 | 1134 | `	a = pTos->x.iVal;` |
|     33 | 1135 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|      - | 1136 | `	/* Push the result */` |
|     27 | 1137 | `	pNos->x.iVal = r;` |
|     27 | 1138 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     27 | 1139 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1140 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     27 | 1141 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     27 | 1142 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     27 | 1143 | `		PH7_MemObjStore(pNos,pObj);` |
|     13 | 1144 | `	}` |
|     27 | 1145 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     27 | 1146 | `	VmPopOperand(&pTos,1);` |
|     27 | 1147 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1148 | `	VM_EXIT_BREAK;` |
|     19 | 1149 | `}` |
|      - | 1150 |  |
|      - | 1151 | `/*` |
|      - | 1152 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|      - | 1153 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1154 | ` */` |
|    110 | 1155 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 | 1156 | `{` |
|    112 | 1157 | `	ph7_value *pTos = pState->pTos;` |
|    112 | 1158 | `	ph7_value *pStack = pState->pStack;` |
|    112 | 1159 | `	VmInstr *aInstr = pState->aInstr;` |
|    112 | 1160 | `	sxi32 pc = pState->pc;` |
|      - | 1161 | `	sxi32 rc;` |
|     55 | 1162 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    112 | 1163 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1164 | `	sxi64 a,r;` |
|      - | 1165 | `#ifdef UNTRUST` |
|      - | 1166 | `	if( pNos < pStack ){` |
|      - | 1167 | `		VM_EXIT_ABORT;` |
|      - | 1168 | `	}` |
|      - | 1169 | `#endif` |
|    121 | 1170 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|      - | 1171 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     94 | 1172 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     94 | 1173 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     90 | 1174 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     90 | 1175 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     90 | 1176 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1177 | `		PH7_MemObjToInteger(pTos);` |
|      4 | 1178 | `	}` |
|     90 | 1179 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1180 | `		PH7_MemObjToInteger(pNos);` |
|      4 | 1181 | `	}` |
|      - | 1182 | `	/* Perform the requested operation */` |
|     90 | 1183 | `	a = pNos->x.iVal;` |
|     90 | 1184 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|      - | 1185 | `	/* Push the result */` |
|     80 | 1186 | `	pNos->x.iVal = r;` |
|     80 | 1187 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     80 | 1188 | `	VmPopOperand(&pTos,1);` |
|     80 | 1189 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1190 | `	VM_EXIT_BREAK;` |
|     57 | 1191 | `}` |
|      - | 1192 |  |
|      - | 1193 | `/*` |
|      - | 1194 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|      - | 1195 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1196 | ` */` |
|   3060 | 1197 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1198 | `{` |
|   3065 | 1199 | `	ph7_value *pTos = pState->pTos;` |
|   3065 | 1200 | `	ph7_value *pStack = pState->pStack;` |
|   3065 | 1201 | `	VmInstr *aInstr = pState->aInstr;` |
|   3065 | 1202 | `	sxi32 pc = pState->pc;` |
|      - | 1203 | `	sxi32 rc;` |
|   1530 | 1204 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   3065 | 1205 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1206 | `	{` |
|      - | 1207 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - | 1208 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - | 1209 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - | 1210 | `		SyBlob sArMsg;` |
|   3065 | 1211 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   3065 | 1212 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"*",&sArMsg) != SXRET_OK ){` |
|      - | 1213 | `			sxi32 rcAr;` |
|    ! 0 | 1214 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1215 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 | 1216 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 | 1217 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 | 1218 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 | 1219 | `				SyBlobLength(&sArMsg));` |
|    ! 0 | 1220 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 | 1221 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 | 1222 | `			rc = rcAr;` |
|    ! 0 | 1223 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1224 | `		}` |
|   3065 | 1225 | `		SyBlobRelease(&sArMsg);` |
|      - | 1226 | `	}` |
|      - | 1227 | `	/* Force the operand to be numeric */` |
|      - | 1228 | `#ifdef UNTRUST` |
|      - | 1229 | `	if( pNos < pStack ){` |
|      - | 1230 | `		VM_EXIT_ABORT;` |
|      - | 1231 | `	}` |
|      - | 1232 | `#endif` |
|   3065 | 1233 | `	PH7_MemObjToNumeric(pTos);` |
|   3065 | 1234 | `	PH7_MemObjToNumeric(pNos);` |
|      - | 1235 | `	/* Perform the requested operation */` |
|   3065 | 1236 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - | 1237 | `		/* Floating point arithemic */` |
|      - | 1238 | `		ph7_real a,b,r;` |
|     33 | 1239 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     23 | 1240 | `			PH7_MemObjToReal(pTos);` |
|     11 | 1241 | `		}` |
|     33 | 1242 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 | 1243 | `			PH7_MemObjToReal(pNos);` |
|      2 | 1244 | `		}` |
|     33 | 1245 | `		a = pNos->rVal;` |
|     33 | 1246 | `		b = pTos->rVal;` |
|     33 | 1247 | `		r = a * b;` |
|      - | 1248 | `		/* Push the result */` |
|     33 | 1249 | `		pNos->rVal = r;` |
|     33 | 1250 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1251 | `		/* Try to get an integer representation */` |
|     33 | 1252 | `		PH7_MemObjTryInteger(pNos);` |
|     17 | 1253 | `	}else{` |
|      - | 1254 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|      - | 1255 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - | 1256 | `		sxi64 a,b,r;` |
|   3033 | 1257 | `		a = pNos->x.iVal;` |
|   3033 | 1258 | `		b = pTos->x.iVal;` |
|   3033 | 1259 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|      - | 1260 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     13 | 1261 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|     13 | 1262 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1263 | `#else` |
|      - | 1264 | `			pNos->x.iVal = r;` |
|      - | 1265 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1266 | `#endif` |
|      7 | 1267 | `		}else{` |
|   3021 | 1268 | `			pNos->x.iVal = r;` |
|   3021 | 1269 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1270 | `		}` |
|      - | 1271 | `	}` |
|   3065 | 1272 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|      - | 1273 | `		ph7_value *pObj;` |
|     55 | 1274 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1275 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     55 | 1276 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     55 | 1277 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     55 | 1278 | `			PH7_MemObjStore(pNos,pObj);` |
|     26 | 1279 | `		}` |
|     26 | 1280 | `	}` |
|   3065 | 1281 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|   3065 | 1282 | `	VmPopOperand(&pTos,1);` |
|   3065 | 1283 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1284 | `	VM_EXIT_BREAK;` |
|   1535 | 1285 | `}` |
|      - | 1286 |  |
