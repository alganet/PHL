# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 640/755 lines (84.77%)

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
|    102 |   29 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   30 | `{` |
|    105 |   31 | `	ph7_value *pTos = pState->pTos;` |
|    105 |   32 | `	ph7_value *pStack = pState->pStack;` |
|    105 |   33 | `	VmInstr *aInstr = pState->aInstr;` |
|    105 |   34 | `	sxi32 pc = pState->pc;` |
|      - |   35 | `	sxi32 rc;` |
|     51 |   36 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    105 |   37 | `	ph7_value *pNos = &pTos[-1];` |
|      - |   38 | `	ph7_value *pObj;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pNos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|    105 |   45 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
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
|     93 |   82 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|      5 |   83 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|      5 |   84 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |   85 | `			"offsetSet",sizeof("offsetSet")-1);` |
|      - |   86 | `		ph7_value *apArg[2];` |
|      5 |   87 | `		apArg[0] = &pVm->sCoalesceKey;` |
|      5 |   88 | `		apArg[1] = pTos;` |
|      5 |   89 | `		if( pSet ){` |
|      5 |   90 | `			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|      2 |   91 | `		}` |
|      - |   92 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|      5 |   93 | `		PH7_MemObjStore(pTos,pNos);` |
|      5 |   94 | `		VmPopOperand(&pTos,1);` |
|      - |   95 | `		/* Disarm and release the cached instance ref + key. */` |
|      5 |   96 | `		VmCoalesceDisarm(pVm);` |
|      5 |   97 | `		VM_EXIT_BREAK;` |
|      - |   98 | `	}` |
|     88 |   99 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|      - |  100 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|      - |  101 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|      - |  102 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|      - |  103 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|      - |  104 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|      - |  105 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|      - |  106 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|      - |  107 | `		 * offset the quiet peek let through raises at the store. */` |
|     43 |  108 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|     64 |  109 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|     42 |  110 | `			? (ph7_value *)SySetAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|     43 |  111 | `		sxi64 iOfft = 0;` |
|      - |  112 | `		SyBlob sTypeMsg;` |
|      - |  113 | `		int eOfft;` |
|     43 |  114 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|     43 |  115 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|      - |  116 | `			&iOfft,&sTypeMsg);` |
|     43 |  117 | `		if( eOfft == VM_STROFF_REJECT ){` |
|      - |  118 | `			sxi32 rcSo;` |
|      3 |  119 | `			VmPopOperand(&pTos,1);` |
|      3 |  120 | `			PH7_MemObjRelease(pTos);` |
|      3 |  121 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  122 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  123 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      3 |  124 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  125 | `			rc = rcSo;` |
|      3 |  126 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  127 | `		}` |
|     41 |  128 | `		SyBlobRelease(&sTypeMsg);` |
|      - |  129 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|      - |  130 | `		{` |
|     41 |  131 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|     41 |  132 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|      - |  133 | `		}` |
|     41 |  134 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|     41 |  135 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|      - |  136 | `				sxi32 rcEm;` |
|      5 |  137 | `				VmPopOperand(&pTos,1);` |
|      5 |  138 | `				PH7_MemObjRelease(pTos);` |
|      5 |  139 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  140 | `				pTos->nIdx = SXU32_HIGH;` |
|      5 |  141 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|      - |  142 | `					"Cannot assign an empty string to a string offset",` |
|      - |  143 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|      5 |  144 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  145 | `				rc = rcEm;` |
|      5 |  146 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  147 | `			}` |
|     18 |  148 | `		}` |
|      - |  149 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|      - |  150 | `		 * not free what the carrier owns, so release it here — every other exit` |
|      - |  151 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|     37 |  152 | `		VmFreeCoalStrOff(pCoalOff);` |
|     37 |  153 | `		pNos->x.pOther = 0;` |
|     37 |  154 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|      - |  155 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|     37 |  156 | `		PH7_MemObjStore(pTos,pNos);` |
|     37 |  157 | `		pNos->nIdx = SXU32_HIGH;` |
|     37 |  158 | `		VmPopOperand(&pTos,1);` |
|     37 |  159 | `		VM_EXIT_BREAK;` |
|      - |  160 | `	}` |
|     46 |  161 | `	nIdx = pNos->nIdx;` |
|     46 |  162 | `	if( nIdx == SXU32_HIGH ){` |
|    ! 0 |  163 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  164 | `			"Cannot perform assignment on a constant class attribute");` |
|     46 |  165 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|     46 |  166 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|     46 |  167 | `		PH7_MemObjStore(pTos,pObj);` |
|     22 |  168 | `	}` |
|     46 |  169 | `	PH7_MemObjStore(pTos,pNos);` |
|     46 |  170 | `	VmPopOperand(&pTos,1);` |
|     46 |  171 | `	VM_EXIT_BREAK;` |
|    ! 0 |  172 | `	VM_EXIT_BREAK;` |
|     54 |  173 | `}` |
|      - |  174 |  |
|      - |  175 | `/*` |
|      - |  176 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|      - |  177 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  178 | ` */` |
|    140 |  179 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |  180 | `{` |
|    143 |  181 | `	ph7_value *pTos = pState->pTos;` |
|    143 |  182 | `	ph7_value *pStack = pState->pStack;` |
|    143 |  183 | `	VmInstr *aInstr = pState->aInstr;` |
|    143 |  184 | `	sxi32 pc = pState->pc;` |
|      - |  185 | `	sxi32 rc;` |
|     70 |  186 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    143 |  187 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  188 | `	{` |
|      - |  189 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  190 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  191 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  192 | `		SyBlob sArMsg;` |
|    143 |  193 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    143 |  194 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){` |
|      - |  195 | `			sxi32 rcAr;` |
|    ! 0 |  196 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  197 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  198 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  199 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  200 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  201 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  202 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  203 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  204 | `			rc = rcAr;` |
|    ! 0 |  205 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  206 | `		}` |
|    143 |  207 | `		SyBlobRelease(&sArMsg);` |
|      - |  208 | `	}` |
|      - |  209 | `	ph7_real a,b,r;` |
|      - |  210 | `#ifdef UNTRUST` |
|      - |  211 | `	if( pNos < pStack ){` |
|      - |  212 | `		VM_EXIT_ABORT;` |
|      - |  213 | `	}` |
|      - |  214 | `#endif` |
|      - |  215 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|      - |  216 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|      - |  217 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|      - |  218 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|      - |  219 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|    143 |  220 | `	PH7_MemObjToNumeric(pTos);` |
|    143 |  221 | `	PH7_MemObjToNumeric(pNos);` |
|    143 |  222 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     79 |  223 | `		sxi64 ia = pNos->x.iVal;` |
|     79 |  224 | `		sxi64 ib = pTos->x.iVal;` |
|     79 |  225 | `		sxi64 iQuot = 0;` |
|     79 |  226 | `		int bExact = 0;` |
|     79 |  227 | `		if( ib == 0 ){` |
|      8 |  228 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|     12 |  229 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     72 |  230 | `		}else if( ib == -1 ){` |
|      - |  231 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|      - |  232 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|      - |  233 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|      - |  234 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|      - |  235 | `			 * alongside the remainder, so testing the remainder first trapped` |
|      - |  236 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|      - |  237 | `			 * OP_MOD screens the same hazard the same way. */` |
|      - |  238 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|      - |  239 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|      - |  240 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|      - |  241 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|      - |  242 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|      - |  243 | `			bExact = 1;` |
|      - |  244 | `#else` |
|     19 |  245 | `			if( ia != SMALLEST_INT64 ){` |
|     15 |  246 | `				iQuot = -ia;` |
|     15 |  247 | `				bExact = 1;` |
|      8 |  248 | `			}` |
|      - |  249 | `#endif` |
|     63 |  250 | `		}else if( ia % ib == 0 ){` |
|     22 |  251 | `			iQuot = ia / ib;` |
|     22 |  252 | `			bExact = 1;` |
|     10 |  253 | `		}` |
|     72 |  254 | `		if( bExact ){` |
|     36 |  255 | `			pNos->x.iVal = iQuot;` |
|     36 |  256 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     36 |  257 | `			VmPopOperand(&pTos,1);` |
|     36 |  258 | `			VM_EXIT_BREAK;` |
|      - |  259 | `		}` |
|     18 |  260 | `	}` |
|      - |  261 | `	/* Force the operands to be real */` |
|    102 |  262 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     89 |  263 | `		PH7_MemObjToReal(pTos);` |
|     44 |  264 | `	}` |
|    102 |  265 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     37 |  266 | `		PH7_MemObjToReal(pNos);` |
|     18 |  267 | `	}` |
|      - |  268 | `	/* Perform the requested operation */` |
|    102 |  269 | `	a = pNos->rVal;` |
|    102 |  270 | `	b = pTos->rVal;` |
|    102 |  271 | `	if( b == 0 ){` |
|      - |  272 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  273 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      3 |  274 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      3 |  275 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  276 | `	}else{` |
|    100 |  277 | `		r = a/b;` |
|      - |  278 | `		/* Push the result */` |
|    100 |  279 | `		pNos->rVal = r;` |
|    100 |  280 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  281 | `	}` |
|    100 |  282 | `	VmPopOperand(&pTos,1);` |
|    100 |  283 | `	VM_EXIT_BREAK;` |
|    ! 0 |  284 | `	VM_EXIT_BREAK;` |
|     73 |  285 | `}` |
|      - |  286 |  |
|      - |  287 | `/*` |
|      - |  288 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|      - |  289 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  290 | ` */` |
|     10 |  291 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  292 | `{` |
|     11 |  293 | `	ph7_value *pTos = pState->pTos;` |
|     11 |  294 | `	ph7_value *pStack = pState->pStack;` |
|     11 |  295 | `	VmInstr *aInstr = pState->aInstr;` |
|     11 |  296 | `	sxi32 pc = pState->pc;` |
|      - |  297 | `	sxi32 rc;` |
|      5 |  298 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     11 |  299 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  300 | `	ph7_value *pObj;` |
|      - |  301 | `	sxi64 a,b,r;` |
|      - |  302 | `#ifdef UNTRUST` |
|      - |  303 | `	if( pNos < pStack ){` |
|      - |  304 | `		VM_EXIT_ABORT;` |
|      - |  305 | `	}` |
|      - |  306 | `#endif` |
|      - |  307 | `	{` |
|      - |  308 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  309 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  310 | `		SyBlob sArMsg;` |
|     11 |  311 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     11 |  312 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  313 | `			sxi32 rcAr;` |
|    ! 0 |  314 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  315 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  316 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  317 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  318 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  319 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  320 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  321 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  322 | `			rc = rcAr;` |
|    ! 0 |  323 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  324 | `		}` |
|     11 |  325 | `		SyBlobRelease(&sArMsg);` |
|      - |  326 | `	}` |
|      - |  327 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     11 |  328 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     11 |  329 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     11 |  330 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     11 |  331 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     11 |  332 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  333 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 |  334 | `	}` |
|     11 |  335 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  336 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 |  337 | `	}` |
|      - |  338 | `	/* Perform the requested operation */` |
|     11 |  339 | `	a = pTos->x.iVal;` |
|     11 |  340 | `	b = pNos->x.iVal;` |
|     11 |  341 | `	if( b == 0 ){` |
|      - |  342 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  343 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      5 |  344 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      5 |  345 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  346 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|      7 |  347 | `	}else if( b == -1 ){` |
|      - |  348 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|      - |  349 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|      3 |  350 | `		r = 0;` |
|      2 |  351 | `	}else{` |
|      5 |  352 | `		r = a%b;` |
|      - |  353 | `	}` |
|      - |  354 | `	/* Push the result */` |
|      7 |  355 | `	pNos->x.iVal = r;` |
|      7 |  356 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      7 |  357 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  358 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|      7 |  359 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      7 |  360 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|      7 |  361 | `		PH7_MemObjStore(pNos,pObj);` |
|      3 |  362 | `	}` |
|      7 |  363 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|      7 |  364 | `	VmPopOperand(&pTos,1);` |
|      7 |  365 | `	VM_EXIT_BREAK;` |
|    ! 0 |  366 | `	VM_EXIT_BREAK;` |
|      6 |  367 | `}` |
|      - |  368 |  |
|      - |  369 | `/*` |
|      - |  370 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|      - |  371 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  372 | ` */` |
|   1004 |  373 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  374 | `{` |
|   1009 |  375 | `	ph7_value *pTos = pState->pTos;` |
|   1009 |  376 | `	ph7_value *pStack = pState->pStack;` |
|   1009 |  377 | `	VmInstr *aInstr = pState->aInstr;` |
|   1009 |  378 | `	sxi32 pc = pState->pc;` |
|      - |  379 | `	sxi32 rc;` |
|    502 |  380 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1009 |  381 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  382 | `	{` |
|      - |  383 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  384 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  385 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  386 | `		SyBlob sArMsg;` |
|   1009 |  387 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   1009 |  388 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){` |
|      - |  389 | `			sxi32 rcAr;` |
|      3 |  390 | `			VmPopOperand(&pTos,1);` |
|      3 |  391 | `			PH7_MemObjRelease(pTos);` |
|      3 |  392 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  393 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  394 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  395 | `				SyBlobLength(&sArMsg));` |
|      3 |  396 | `			SyBlobRelease(&sArMsg);` |
|      4 |  397 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  398 | `			rc = rcAr;` |
|      5 |  399 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  400 | `		}` |
|   1007 |  401 | `		SyBlobRelease(&sArMsg);` |
|      - |  402 | `	}` |
|      - |  403 | `	sxi64 a,b,r;` |
|      - |  404 | `#ifdef UNTRUST` |
|      - |  405 | `	if( pNos < pStack ){` |
|      - |  406 | `		VM_EXIT_ABORT;` |
|      - |  407 | `	}` |
|      - |  408 | `#endif` |
|      - |  409 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|   1007 |  410 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|   1007 |  411 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    993 |  412 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    993 |  413 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    991 |  414 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      3 |  415 | `		PH7_MemObjToInteger(pTos);` |
|      1 |  416 | `	}` |
|    991 |  417 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      7 |  418 | `		PH7_MemObjToInteger(pNos);` |
|      3 |  419 | `	}` |
|      - |  420 | `	/* Perform the requested operation */` |
|    991 |  421 | `	a = pNos->x.iVal;` |
|    991 |  422 | `	b = pTos->x.iVal;` |
|    991 |  423 | `	if( b == 0 ){` |
|      - |  424 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|      - |  425 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|     10 |  426 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|     18 |  427 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|    ! 0 |  428 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|    983 |  429 | `	}else if( b == -1 ){` |
|      - |  430 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|      - |  431 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|      - |  432 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|      - |  433 | `		 * remainder. php's result here is 0. */` |
|      5 |  434 | `		r = 0;` |
|      3 |  435 | `	}else{` |
|    979 |  436 | `		r = a%b;` |
|      - |  437 | `	}` |
|      - |  438 | `	/* Push the result */` |
|    983 |  439 | `	pNos->x.iVal = r;` |
|    983 |  440 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    983 |  441 | `	VmPopOperand(&pTos,1);` |
|    983 |  442 | `	VM_EXIT_BREAK;` |
|    ! 0 |  443 | `	VM_EXIT_BREAK;` |
|    507 |  444 | `}` |
|      - |  445 |  |
|      - |  446 | `/*` |
|      - |  447 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|      - |  448 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  449 | ` */` |
|     14 |  450 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      2 |  451 | `{` |
|     16 |  452 | `	ph7_value *pTos = pState->pTos;` |
|     16 |  453 | `	ph7_value *pStack = pState->pStack;` |
|     16 |  454 | `	VmInstr *aInstr = pState->aInstr;` |
|     16 |  455 | `	sxi32 pc = pState->pc;` |
|      - |  456 | `	sxi32 rc;` |
|      7 |  457 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     16 |  458 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  459 | `	ph7_value *pObj;` |
|      - |  460 | `#ifdef UNTRUST` |
|      - |  461 | `	if( pNos < pStack ){` |
|      - |  462 | `		VM_EXIT_ABORT;` |
|      - |  463 | `	}` |
|      - |  464 | `#endif` |
|      - |  465 | `	{` |
|      - |  466 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|      - |  467 | `		 * array, object or resource operand is a TypeError too. */` |
|      - |  468 | `		SyBlob sArMsg;` |
|     16 |  469 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     16 |  470 | `		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  471 | `			sxi32 rcAr;` |
|      3 |  472 | `			VmPopOperand(&pTos,1);` |
|      3 |  473 | `			PH7_MemObjRelease(pTos);` |
|      3 |  474 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  475 | `			pTos->nIdx = SXU32_HIGH;` |
|      4 |  476 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      1 |  477 | `				SyBlobLength(&sArMsg));` |
|      3 |  478 | `			SyBlobRelease(&sArMsg);` |
|      4 |  479 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  480 | `			rc = rcAr;` |
|      3 |  481 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  482 | `		}` |
|     14 |  483 | `		SyBlobRelease(&sArMsg);` |
|      - |  484 | `	}` |
|      - |  485 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     14 |  486 | `	PH7_MemObjToNumeric(pTos);` |
|     14 |  487 | `	PH7_MemObjToNumeric(pNos);` |
|     14 |  488 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  489 | `		/* Floating point arithemic */` |
|      - |  490 | `		ph7_real a,b,r;` |
|    ! 0 |  491 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  492 | `			PH7_MemObjToReal(pTos);` |
|    ! 0 |  493 | `		}` |
|    ! 0 |  494 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|    ! 0 |  495 | `			PH7_MemObjToReal(pNos);` |
|    ! 0 |  496 | `		}` |
|    ! 0 |  497 | `		a = pTos->rVal;` |
|    ! 0 |  498 | `		b = pNos->rVal;` |
|    ! 0 |  499 | `		r = a - b;` |
|      - |  500 | `		/* Push the result */` |
|    ! 0 |  501 | `		pNos->rVal = r;` |
|    ! 0 |  502 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  503 | `		/* Try to get an integer representation */` |
|    ! 0 |  504 | `		PH7_MemObjTryInteger(pNos);` |
|    ! 0 |  505 | `	}else{` |
|      - |  506 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  507 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  508 | `		sxi64 a,b,r;` |
|     14 |  509 | `		a = pTos->x.iVal;` |
|     14 |  510 | `		b = pNos->x.iVal;` |
|     14 |  511 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  512 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      3 |  513 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      3 |  514 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  515 | `#else` |
|      - |  516 | `			pNos->x.iVal = r;` |
|      - |  517 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  518 | `#endif` |
|      2 |  519 | `		}else{` |
|     12 |  520 | `			pNos->x.iVal = r;` |
|     12 |  521 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  522 | `		}` |
|      - |  523 | `	}` |
|     14 |  524 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  525 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     14 |  526 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     14 |  527 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     14 |  528 | `		PH7_MemObjStore(pNos,pObj);` |
|      6 |  529 | `	}` |
|     14 |  530 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     14 |  531 | `	VmPopOperand(&pTos,1);` |
|     14 |  532 | `	VM_EXIT_BREAK;` |
|    ! 0 |  533 | `	VM_EXIT_BREAK;` |
|      9 |  534 | `}` |
|      - |  535 |  |
|      - |  536 | `/*` |
|      - |  537 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|      - |  538 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  539 | ` */` |
|  50467 |  540 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  541 | `{` |
|  50472 |  542 | `	ph7_value *pTos = pState->pTos;` |
|  50472 |  543 | `	ph7_value *pStack = pState->pStack;` |
|  50472 |  544 | `	VmInstr *aInstr = pState->aInstr;` |
|  50472 |  545 | `	sxi32 pc = pState->pc;` |
|      - |  546 | `	sxi32 rc;` |
|  25402 |  547 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  50472 |  548 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  549 | `#ifdef UNTRUST` |
|      - |  550 | `	if( pNos < pStack ){` |
|      - |  551 | `		VM_EXIT_ABORT;` |
|      - |  552 | `	}` |
|      - |  553 | `#endif` |
|      - |  554 | `	{` |
|      - |  555 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  556 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  557 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  558 | `		SyBlob sArMsg;` |
|  50472 |  559 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|  50472 |  560 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){` |
|      - |  561 | `			sxi32 rcAr;` |
|    ! 0 |  562 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  563 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 |  564 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  565 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  566 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 |  567 | `				SyBlobLength(&sArMsg));` |
|    ! 0 |  568 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 |  569 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 |  570 | `			rc = rcAr;` |
|    ! 0 |  571 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  572 | `		}` |
|  50472 |  573 | `		SyBlobRelease(&sArMsg);` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|      - |  576 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|      - |  577 | `	 * quietly evaluated to 0. */` |
|  50472 |  578 | `	PH7_MemObjToNumeric(pTos);` |
|  50472 |  579 | `	PH7_MemObjToNumeric(pNos);` |
|  50472 |  580 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - |  581 | `		/* Floating point arithemic */` |
|      - |  582 | `		ph7_real a,b,r;` |
|    140 |  583 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     21 |  584 | `			PH7_MemObjToReal(pTos);` |
|     10 |  585 | `		}` |
|    140 |  586 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      7 |  587 | `			PH7_MemObjToReal(pNos);` |
|      3 |  588 | `		}` |
|    140 |  589 | `		a = pNos->rVal;` |
|    140 |  590 | `		b = pTos->rVal;` |
|    140 |  591 | `		r = a - b;` |
|      - |  592 | `		/* Push the result */` |
|    140 |  593 | `		pNos->rVal = r;` |
|    140 |  594 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  595 | `		/* Try to get an integer representation */` |
|    140 |  596 | `		PH7_MemObjTryInteger(pNos);` |
|     71 |  597 | `	}else{` |
|      - |  598 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|      - |  599 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - |  600 | `		sxi64 a,b,r;` |
|  50334 |  601 | `		a = pNos->x.iVal;` |
|  50334 |  602 | `		b = pTos->x.iVal;` |
|  50334 |  603 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|      - |  604 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      7 |  605 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      7 |  606 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - |  607 | `#else` |
|      - |  608 | `			pNos->x.iVal = r;` |
|      - |  609 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  610 | `#endif` |
|      4 |  611 | `		}else{` |
|  50328 |  612 | `			pNos->x.iVal = r;` |
|  50328 |  613 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - |  614 | `		}` |
|      - |  615 | `	}` |
|  50472 |  616 | `	VmPopOperand(&pTos,1);` |
|  50472 |  617 | `	VM_EXIT_BREAK;` |
|    ! 0 |  618 | `	VM_EXIT_BREAK;` |
|  25407 |  619 | `}` |
|      - |  620 |  |
|      - |  621 | `/*` |
|      - |  622 | `` * php's `**`, as a value operation.`` |
|      - |  623 | ` *` |
|      - |  624 | ` * In php pow() IS the exponentiation operator -- both compile to the same` |
|      - |  625 | ` * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and` |
|      - |  626 | ` * every edge value have to come from ONE place here too. This is that place:` |
|      - |  627 | ` * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call` |
|      - |  628 | ` * it, after the caller has run VmArithOperandCheck() over the two operands (the` |
|      - |  629 | ` * two sites word their throw differently -- one settles an operand stack first,` |
|      - |  630 | ` * the other is inside a C builtin -- so the CHECK stays with the caller and only` |
|      - |  631 | ` * the arithmetic is shared).` |
|      - |  632 | ` *` |
|      - |  633 | ` * pBase and pExp are converted in place, which is what the opcode arm already` |
|      - |  634 | ` * did to its stack slots; pOut may alias either of them and is written last.` |
|      - |  635 | ` */` |
|    222 |  636 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)` |
|      3 |  637 | `{` |
|      - |  638 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  639 | `	int bBothInt;` |
|    225 |  640 | `	int usedInt = 0;` |
|      - |  641 | `	ph7_real a, b, r;` |
|      - |  642 | `#endif` |
|    225 |  643 | `	sxi64 base_i = 0, exp_i = 0;` |
|    225 |  644 | `	PH7_MemObjToNumeric(pBase);` |
|    225 |  645 | `	PH7_MemObjToNumeric(pExp);` |
|      - |  646 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    413 |  647 | `	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&` |
|    188 |  648 | `	           ((pExp->iFlags & MEMOBJ_REAL) == 0);` |
|    225 |  649 | `	if( bBothInt ){` |
|    185 |  650 | `		base_i = pBase->x.iVal;` |
|    185 |  651 | `		exp_i  = pExp->x.iVal;` |
|     91 |  652 | `	}` |
|    225 |  653 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|    191 |  654 | `		PH7_MemObjToReal(pBase);` |
|     94 |  655 | `	}` |
|    225 |  656 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|    219 |  657 | `		PH7_MemObjToReal(pExp);` |
|    108 |  658 | `	}` |
|    225 |  659 | `	a = pBase->rVal;` |
|    225 |  660 | `	b = pExp->rVal;` |
|    225 |  661 | `	r = pow(a, b);` |
|      - |  662 | `	/* Match PHP: int**non-negative-int stays int when the exact result` |
|      - |  663 | `	 * fits in sxi64. Use exponentiation by squaring with overflow checks` |
|      - |  664 | `	 * rather than casting the double back, because the boundary 2^63 is` |
|      - |  665 | `	 * representable as double but not as signed int64. */` |
|    225 |  666 | `	if( bBothInt && exp_i >= 0 ){` |
|    175 |  667 | `		sxi64 result_i = 1;` |
|    175 |  668 | `		sxi64 cur_base = base_i;` |
|    175 |  669 | `		sxi64 cur_exp  = exp_i;` |
|    175 |  670 | `		int overflow = 0;` |
|    583 |  671 | `		while( cur_exp > 0 ){` |
|    427 |  672 | `			if( cur_exp & 1 ){` |
|    275 |  673 | `				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){` |
|      7 |  674 | `					overflow = 1;` |
|      7 |  675 | `					break;` |
|      - |  676 | `				}` |
|    133 |  677 | `			}` |
|    421 |  678 | `			cur_exp >>= 1;` |
|    421 |  679 | `			if( cur_exp > 0 ){` |
|    279 |  680 | `				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){` |
|     11 |  681 | `					overflow = 1;` |
|     11 |  682 | `					break;` |
|      - |  683 | `				}` |
|    133 |  684 | `			}` |
|      3 |  685 | `		}` |
|    175 |  686 | `		if( !overflow ){` |
|    159 |  687 | `			pOut->x.iVal = result_i;` |
|    159 |  688 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|    159 |  689 | `			usedInt = 1;` |
|     78 |  690 | `		}` |
|     86 |  691 | `	}` |
|    225 |  692 | `	if( !usedInt ){` |
|     67 |  693 | `		pOut->rVal = r;` |
|     67 |  694 | `		MemObjSetType(pOut, MEMOBJ_REAL);` |
|     33 |  695 | `	}` |
|      - |  696 | `#else` |
|      - |  697 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|      - |  698 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|      - |  699 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|      - |  700 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|      - |  701 | `	 * represented. */` |
|      - |  702 | `	base_i = pBase->x.iVal;` |
|      - |  703 | `	exp_i  = pExp->x.iVal;` |
|      - |  704 | `	{` |
|      - |  705 | `		sxi64 result_i = 1;` |
|      - |  706 | `		sxi64 cur_base = base_i;` |
|      - |  707 | `		sxi64 cur_exp  = exp_i;` |
|      - |  708 | `		if( cur_exp < 0 ){` |
|      - |  709 | `			result_i = 0;` |
|      - |  710 | `		}else{` |
|      - |  711 | `			while( cur_exp > 0 ){` |
|      - |  712 | `				if( cur_exp & 1 ){` |
|      - |  713 | `					result_i *= cur_base;` |
|      - |  714 | `				}` |
|      - |  715 | `				cur_exp >>= 1;` |
|      - |  716 | `				if( cur_exp > 0 ){` |
|      - |  717 | `					cur_base *= cur_base;` |
|      - |  718 | `				}` |
|      - |  719 | `			}` |
|      - |  720 | `		}` |
|      - |  721 | `		pOut->x.iVal = result_i;` |
|      - |  722 | `		MemObjSetType(pOut, MEMOBJ_INT);` |
|      - |  723 | `	}` |
|      - |  724 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    225 |  725 | `}` |
|      - |  726 | `/*` |
|      - |  727 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|      - |  728 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  729 | ` */` |
|    186 |  730 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 |  731 | `{` |
|    187 |  732 | `	ph7_value *pTos = pState->pTos;` |
|    187 |  733 | `	ph7_value *pStack = pState->pStack;` |
|    187 |  734 | `	VmInstr *aInstr = pState->aInstr;` |
|    187 |  735 | `	sxi32 pc = pState->pc;` |
|      - |  736 | `	sxi32 rc;` |
|     93 |  737 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    187 |  738 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  739 | `	{` |
|      - |  740 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - |  741 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - |  742 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - |  743 | `		SyBlob sArMsg;` |
|    187 |  744 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    187 |  745 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){` |
|      - |  746 | `			sxi32 rcAr;` |
|      5 |  747 | `			VmPopOperand(&pTos,1);` |
|      5 |  748 | `			PH7_MemObjRelease(pTos);` |
|      5 |  749 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  750 | `			pTos->nIdx = SXU32_HIGH;` |
|      7 |  751 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|      2 |  752 | `				SyBlobLength(&sArMsg));` |
|      5 |  753 | `			SyBlobRelease(&sArMsg);` |
|      7 |  754 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  755 | `			rc = rcAr;` |
|      5 |  756 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  757 | `		}` |
|    183 |  758 | `		SyBlobRelease(&sArMsg);` |
|      - |  759 | `	}` |
|    183 |  760 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|      - |  761 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|      - |  762 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|      - |  763 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|      - |  764 | `	 */` |
|    183 |  765 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    183 |  766 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|      - |  767 | `#ifdef UNTRUST` |
|      - |  768 | `	if( pNos < pStack ){` |
|      - |  769 | `		VM_EXIT_ABORT;` |
|      - |  770 | `	}` |
|      - |  771 | `#endif` |
|    183 |  772 | `	PH7_MemObjPow(pBase,pExp,pNos);` |
|    183 |  773 | `	if( bStore ){` |
|      - |  774 | `		ph7_value *pObj;` |
|     25 |  775 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 |  776 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     25 |  777 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     25 |  778 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     25 |  779 | `			PH7_MemObjStore(pNos,pObj);` |
|     12 |  780 | `		}` |
|     12 |  781 | `	}` |
|    183 |  782 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    183 |  783 | `	VmPopOperand(&pTos,1);` |
|    183 |  784 | `	VM_EXIT_BREAK;` |
|    ! 0 |  785 | `	VM_EXIT_BREAK;` |
|     94 |  786 | `}` |
|      - |  787 |  |
|      - |  788 | `/*` |
|      - |  789 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|      - |  790 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  791 | ` */` |
|    566 |  792 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  793 | `{` |
|    571 |  794 | `	ph7_value *pTos = pState->pTos;` |
|    571 |  795 | `	ph7_value *pStack = pState->pStack;` |
|    571 |  796 | `	VmInstr *aInstr = pState->aInstr;` |
|    571 |  797 | `	sxi32 pc = pState->pc;` |
|      - |  798 | `	sxi32 rc;` |
|    283 |  799 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    571 |  800 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  801 | `#ifdef UNTRUST` |
|      - |  802 | `	if( pNos < pStack ){` |
|      - |  803 | `		VM_EXIT_ABORT;` |
|      - |  804 | `	}` |
|      - |  805 | `#endif` |
|    571 |  806 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|    571 |  807 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      - |  808 | `		/* NaN involved: PHP returns 1 for all NaN spaceship comparisons */` |
|      7 |  809 | `		rc = 1;` |
|      4 |  810 | `	}else{` |
|      - |  811 | `		/* Normalize to exactly -1, 0, or 1 */` |
|    565 |  812 | `		rc = (rc > 0) - (rc < 0);` |
|      - |  813 | `	}` |
|    571 |  814 | `	VmPopOperand(&pTos,1);` |
|    571 |  815 | `	PH7_MemObjRelease(pTos);` |
|    571 |  816 | `	pTos->x.iVal = rc;` |
|    571 |  817 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|    571 |  818 | `	VM_EXIT_BREAK;` |
|    ! 0 |  819 | `	VM_EXIT_BREAK;` |
|      5 |  820 | `}` |
|      - |  821 |  |
|      - |  822 | `/*` |
|      - |  823 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|      - |  824 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  825 | ` */` |
| 272325 |  826 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  827 | `{` |
| 272330 |  828 | `	ph7_value *pTos = pState->pTos;` |
| 272330 |  829 | `	ph7_value *pStack = pState->pStack;` |
| 272330 |  830 | `	VmInstr *aInstr = pState->aInstr;` |
| 272330 |  831 | `	sxi32 pc = pState->pc;` |
|      - |  832 | `	sxi32 rc;` |
| 136389 |  833 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 272330 |  834 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  835 | `	/* Perform the comparison and act accordingly */` |
|      - |  836 | `#ifdef UNTRUST` |
|      - |  837 | `	if( pNos < pStack ){` |
|      - |  838 | `		VM_EXIT_ABORT;` |
|      - |  839 | `	}` |
|      - |  840 | `#endif` |
| 272330 |  841 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 272330 |  842 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  843 | `		rc = 0;` |
| 272326 |  844 | `	}else if( pInstr->iOp == PH7_OP_GE ){` |
| 271210 |  845 | `		rc = rc >= 0;` |
| 135836 |  846 | `	}else{` |
|   1117 |  847 | `		rc = rc > 0;` |
|      - |  848 | `	}` |
| 272330 |  849 | `	VmPopOperand(&pTos,1);` |
| 272330 |  850 | `	if( !pInstr->iP2 ){` |
|      - |  851 | `		/* Push comparison result without taking the jump */` |
| 272330 |  852 | `		PH7_MemObjRelease(pTos);` |
| 272330 |  853 | `		pTos->x.iVal = rc;` |
|      - |  854 | `		/* Invalidate any prior representation */` |
| 272330 |  855 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 136394 |  856 | `	}else{` |
|    ! 0 |  857 | `		if( rc ){` |
|      - |  858 | `			/* Jump to the desired location */` |
|    ! 0 |  859 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  860 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  861 | `		}` |
|      - |  862 | `	}` |
| 272330 |  863 | `	VM_EXIT_BREAK;` |
|    ! 0 |  864 | `	VM_EXIT_BREAK;` |
|      5 |  865 | `}` |
|      - |  866 |  |
|      - |  867 | `/*` |
|      - |  868 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|      - |  869 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  870 | ` */` |
| 574173 |  871 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  872 | `{` |
| 574178 |  873 | `	ph7_value *pTos = pState->pTos;` |
| 574178 |  874 | `	ph7_value *pStack = pState->pStack;` |
| 574178 |  875 | `	VmInstr *aInstr = pState->aInstr;` |
| 574178 |  876 | `	sxi32 pc = pState->pc;` |
|      - |  877 | `	sxi32 rc;` |
| 287732 |  878 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 574178 |  879 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  880 | `	/* Perform the comparison and act accordingly */` |
|      - |  881 | `#ifdef UNTRUST` |
|      - |  882 | `	if( pNos < pStack ){` |
|      - |  883 | `		VM_EXIT_ABORT;` |
|      - |  884 | `	}` |
|      - |  885 | `#endif` |
| 574178 |  886 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 574178 |  887 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      9 |  888 | `		rc = 0;` |
| 574174 |  889 | `	}else if( pInstr->iOp == PH7_OP_LE ){` |
|  79594 |  890 | `		rc = rc < 1;` |
|  40020 |  891 | `	}else{` |
| 494581 |  892 | `		rc = rc < 0;` |
|      - |  893 | `	}` |
| 574178 |  894 | `	VmPopOperand(&pTos,1);` |
| 574178 |  895 | `	if( !pInstr->iP2 ){` |
|      - |  896 | `		/* Push comparison result without taking the jump */` |
| 574178 |  897 | `		PH7_MemObjRelease(pTos);` |
| 574178 |  898 | `		pTos->x.iVal = rc;` |
|      - |  899 | `		/* Invalidate any prior representation */` |
| 574178 |  900 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 287737 |  901 | `	}else{` |
|    ! 0 |  902 | `		if( rc ){` |
|      - |  903 | `			/* Jump to the desired location */` |
|    ! 0 |  904 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  905 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  906 | `		}` |
|      - |  907 | `	}` |
| 574178 |  908 | `	VM_EXIT_BREAK;` |
|    ! 0 |  909 | `	VM_EXIT_BREAK;` |
|      5 |  910 | `}` |
|      - |  911 |  |
|      - |  912 | `/*` |
|      - |  913 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|      - |  914 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  915 | ` */` |
| 606139 |  916 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  917 | `{` |
| 606144 |  918 | `	ph7_value *pTos = pState->pTos;` |
| 606144 |  919 | `	ph7_value *pStack = pState->pStack;` |
| 606144 |  920 | `	VmInstr *aInstr = pState->aInstr;` |
| 606144 |  921 | `	sxi32 pc = pState->pc;` |
|      - |  922 | `	sxi32 rc;` |
| 303295 |  923 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 606144 |  924 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  925 | `	/* Perform the comparison and act accordingly */` |
|      - |  926 | `#ifdef UNTRUST` |
|      - |  927 | `	if( pNos < pStack ){` |
|      - |  928 | `		VM_EXIT_ABORT;` |
|      - |  929 | `	}` |
|      - |  930 | `#endif` |
| 606144 |  931 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 606144 |  932 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  933 | `		rc = 1;` |
|      2 |  934 | `	}else{` |
| 606142 |  935 | `		rc = rc != 0;` |
|      - |  936 | `	}` |
| 606144 |  937 | `	VmPopOperand(&pTos,1);` |
| 606144 |  938 | `	if( !pInstr->iP2 ){` |
|      - |  939 | `		/* Push comparison result without taking the jump */` |
| 606144 |  940 | `		PH7_MemObjRelease(pTos);` |
| 606144 |  941 | `		pTos->x.iVal = rc;` |
|      - |  942 | `		/* Invalidate any prior representation */` |
| 606144 |  943 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 303300 |  944 | `	}else{` |
|    ! 0 |  945 | `		if( rc ){` |
|      - |  946 | `			/* Jump to the desired location */` |
|    ! 0 |  947 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  948 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  949 | `		}` |
|      - |  950 | `	}` |
| 606144 |  951 | `	VM_EXIT_BREAK;` |
|    ! 0 |  952 | `	VM_EXIT_BREAK;` |
|      5 |  953 | `}` |
|      - |  954 |  |
|      - |  955 | `/*` |
|      - |  956 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|      - |  957 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  958 | ` */` |
| 688669 |  959 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  960 | `{` |
| 688674 |  961 | `	ph7_value *pTos = pState->pTos;` |
| 688674 |  962 | `	ph7_value *pStack = pState->pStack;` |
| 688674 |  963 | `	VmInstr *aInstr = pState->aInstr;` |
| 688674 |  964 | `	sxi32 pc = pState->pc;` |
|      - |  965 | `	sxi32 rc;` |
| 344955 |  966 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 688674 |  967 | `	ph7_value *pNos = &pTos[-1];` |
|      - |  968 | `	/* Perform the comparison and act accordingly */` |
|      - |  969 | `#ifdef UNTRUST` |
|      - |  970 | `	if( pNos < pStack ){` |
|      - |  971 | `		VM_EXIT_ABORT;` |
|      - |  972 | `	}` |
|      - |  973 | `#endif` |
| 688674 |  974 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 688674 |  975 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|      3 |  976 | `		rc = 0;` |
|      2 |  977 | `	}else{` |
| 688672 |  978 | `		rc = rc == 0;` |
|      - |  979 | `	}` |
| 688674 |  980 | `	VmPopOperand(&pTos,1);` |
| 688674 |  981 | `	if( !pInstr->iP2 ){` |
|      - |  982 | `		/* Push comparison result without taking the jump */` |
| 688674 |  983 | `		PH7_MemObjRelease(pTos);` |
| 688674 |  984 | `		pTos->x.iVal = rc;` |
|      - |  985 | `		/* Invalidate any prior representation */` |
| 688674 |  986 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 344960 |  987 | `	}else{` |
|    ! 0 |  988 | `		if( rc ){` |
|      - |  989 | `			/* Jump to the desired location */` |
|    ! 0 |  990 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 |  991 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  992 | `		}` |
|      - |  993 | `	}` |
| 688674 |  994 | `	VM_EXIT_BREAK;` |
|    ! 0 |  995 | `	VM_EXIT_BREAK;` |
|      5 |  996 | `}` |
|      - |  997 |  |
|      - |  998 | `/*` |
|      - |  999 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|      - | 1000 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1001 | ` */` |
|  13055 | 1002 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1003 | `{` |
|  13060 | 1004 | `	ph7_value *pTos = pState->pTos;` |
|  13060 | 1005 | `	ph7_value *pStack = pState->pStack;` |
|  13060 | 1006 | `	VmInstr *aInstr = pState->aInstr;` |
|  13060 | 1007 | `	sxi32 pc = pState->pc;` |
|      - | 1008 | `	sxi32 rc;` |
|   6526 | 1009 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  13060 | 1010 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1011 | `	/* Perform the comparison and act accordingly */` |
|      - | 1012 | `#ifdef UNTRUST` |
|      - | 1013 | `	if( pNos < pStack ){` |
|      - | 1014 | `		VM_EXIT_ABORT;` |
|      - | 1015 | `	}` |
|      - | 1016 | `#endif` |
|  13060 | 1017 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|  13060 | 1018 | `	if( VmIsUnorderedCmp(pNos,pTos) ){` |
|     26 | 1019 | `		rc = pInstr->iOp == PH7_OP_EQ ? 0 : 1;` |
|  13048 | 1020 | `	}else if( pInstr->iOp == PH7_OP_EQ ){` |
|  11711 | 1021 | `		rc = rc == 0;` |
|   5857 | 1022 | `	}else{` |
|   1330 | 1023 | `		rc = rc != 0;` |
|      - | 1024 | `	}` |
|  13060 | 1025 | `	VmPopOperand(&pTos,1);` |
|  13060 | 1026 | `	if( !pInstr->iP2 ){` |
|      - | 1027 | `		/* Push comparison result without taking the jump */` |
|  13060 | 1028 | `		PH7_MemObjRelease(pTos);` |
|  13060 | 1029 | `		pTos->x.iVal = rc;` |
|      - | 1030 | `		/* Invalidate any prior representation */` |
|  13060 | 1031 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   6531 | 1032 | `	}else{` |
|    ! 0 | 1033 | `		if( rc ){` |
|      - | 1034 | `			/* Jump to the desired location */` |
|    ! 0 | 1035 | `			pc = pInstr->iP2 - 1;` |
|    ! 0 | 1036 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1037 | `		}` |
|      - | 1038 | `	}` |
|  13060 | 1039 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1040 | `	VM_EXIT_BREAK;` |
|      5 | 1041 | `}` |
|      - | 1042 |  |
|      - | 1043 | `/*` |
|      - | 1044 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|      - | 1045 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1046 | ` */` |
| 342189 | 1047 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1048 | `{` |
| 342194 | 1049 | `	ph7_value *pTos = pState->pTos;` |
| 342194 | 1050 | `	ph7_value *pStack = pState->pStack;` |
| 342194 | 1051 | `	VmInstr *aInstr = pState->aInstr;` |
| 342194 | 1052 | `	sxi32 pc = pState->pc;` |
|      - | 1053 | `	sxi32 rc;` |
| 171318 | 1054 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 342194 | 1055 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1056 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|      - | 1057 | `#ifdef UNTRUST` |
|      - | 1058 | `	if( pNos < pStack ){` |
|      - | 1059 | `		VM_EXIT_ABORT;` |
|      - | 1060 | `	}` |
|      - | 1061 | `#endif` |
|      - | 1062 | `	/* Force a boolean cast */` |
| 342194 | 1063 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      5 | 1064 | `		PH7_MemObjToBool(pTos);` |
|      2 | 1065 | `	}` |
| 342194 | 1066 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|    ! 0 | 1067 | `		PH7_MemObjToBool(pNos);` |
|    ! 0 | 1068 | `	}` |
| 342194 | 1069 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
| 342194 | 1070 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
| 342194 | 1071 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|      - | 1072 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|  60509 | 1073 | `		v1 = and_logic[v1*3+v2];` |
|  30258 | 1074 | `	}else{` |
|      - | 1075 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
| 281690 | 1076 | `		v1 = or_logic[v1*3+v2];` |
|      - | 1077 | `	}` |
| 342194 | 1078 | `	if( v1 == 2 ){` |
|    ! 0 | 1079 | `		v1 = 1;` |
|    ! 0 | 1080 | `	}` |
| 342194 | 1081 | `	VmPopOperand(&pTos,1);` |
| 342194 | 1082 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
| 342194 | 1083 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
| 342194 | 1084 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1085 | `	VM_EXIT_BREAK;` |
|      5 | 1086 | `}` |
|      - | 1087 |  |
|      - | 1088 | `/*` |
|      - | 1089 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|      - | 1090 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1091 | ` */` |
|     36 | 1092 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      1 | 1093 | `{` |
|     37 | 1094 | `	ph7_value *pTos = pState->pTos;` |
|     37 | 1095 | `	ph7_value *pStack = pState->pStack;` |
|     37 | 1096 | `	VmInstr *aInstr = pState->aInstr;` |
|     37 | 1097 | `	sxi32 pc = pState->pc;` |
|      - | 1098 | `	sxi32 rc;` |
|     18 | 1099 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     37 | 1100 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1101 | `	ph7_value *pObj;` |
|      - | 1102 | `	sxi64 a,r;` |
|      - | 1103 | `#ifdef UNTRUST` |
|      - | 1104 | `	if( pNos < pStack ){` |
|      - | 1105 | `		VM_EXIT_ABORT;` |
|      - | 1106 | `	}` |
|      - | 1107 | `#endif` |
|      - | 1108 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|      - | 1109 | `	 * this handler, beside the other eleven compound stores.) */` |
|     40 | 1110 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|      - | 1111 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     31 | 1112 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     31 | 1113 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     31 | 1114 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     31 | 1115 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     31 | 1116 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1117 | `		PH7_MemObjToInteger(pTos);` |
|    ! 0 | 1118 | `	}` |
|     31 | 1119 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1120 | `		PH7_MemObjToInteger(pNos);` |
|    ! 0 | 1121 | `	}` |
|      - | 1122 | `	/* Perform the requested operation */` |
|     31 | 1123 | `	a = pTos->x.iVal;` |
|     33 | 1124 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|      - | 1125 | `	/* Push the result */` |
|     27 | 1126 | `	pNos->x.iVal = r;` |
|     27 | 1127 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     27 | 1128 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1129 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     27 | 1130 | `	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     27 | 1131 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     27 | 1132 | `		PH7_MemObjStore(pNos,pObj);` |
|     13 | 1133 | `	}` |
|     27 | 1134 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     27 | 1135 | `	VmPopOperand(&pTos,1);` |
|     27 | 1136 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1137 | `	VM_EXIT_BREAK;` |
|     19 | 1138 | `}` |
|      - | 1139 |  |
|      - | 1140 | `/*` |
|      - | 1141 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|      - | 1142 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1143 | ` */` |
|    110 | 1144 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 | 1145 | `{` |
|    113 | 1146 | `	ph7_value *pTos = pState->pTos;` |
|    113 | 1147 | `	ph7_value *pStack = pState->pStack;` |
|    113 | 1148 | `	VmInstr *aInstr = pState->aInstr;` |
|    113 | 1149 | `	sxi32 pc = pState->pc;` |
|      - | 1150 | `	sxi32 rc;` |
|     55 | 1151 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    113 | 1152 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1153 | `	sxi64 a,r;` |
|      - | 1154 | `#ifdef UNTRUST` |
|      - | 1155 | `	if( pNos < pStack ){` |
|      - | 1156 | `		VM_EXIT_ABORT;` |
|      - | 1157 | `	}` |
|      - | 1158 | `#endif` |
|    122 | 1159 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|      - | 1160 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     95 | 1161 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     95 | 1162 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     90 | 1163 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     90 | 1164 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     90 | 1165 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1166 | `		PH7_MemObjToInteger(pTos);` |
|      4 | 1167 | `	}` |
|     90 | 1168 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|      9 | 1169 | `		PH7_MemObjToInteger(pNos);` |
|      4 | 1170 | `	}` |
|      - | 1171 | `	/* Perform the requested operation */` |
|     90 | 1172 | `	a = pNos->x.iVal;` |
|     90 | 1173 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|      - | 1174 | `	/* Push the result */` |
|     80 | 1175 | `	pNos->x.iVal = r;` |
|     80 | 1176 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     80 | 1177 | `	VmPopOperand(&pTos,1);` |
|     80 | 1178 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1179 | `	VM_EXIT_BREAK;` |
|     58 | 1180 | `}` |
|      - | 1181 |  |
|      - | 1182 | `/*` |
|      - | 1183 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|      - | 1184 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1185 | ` */` |
|   3048 | 1186 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1187 | `{` |
|   3053 | 1188 | `	ph7_value *pTos = pState->pTos;` |
|   3053 | 1189 | `	ph7_value *pStack = pState->pStack;` |
|   3053 | 1190 | `	VmInstr *aInstr = pState->aInstr;` |
|   3053 | 1191 | `	sxi32 pc = pState->pc;` |
|      - | 1192 | `	sxi32 rc;` |
|   1524 | 1193 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   3053 | 1194 | `	ph7_value *pNos = &pTos[-1];` |
|      - | 1195 | `	{` |
|      - | 1196 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|      - | 1197 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|      - | 1198 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|      - | 1199 | `		SyBlob sArMsg;` |
|   3053 | 1200 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   3053 | 1201 | `		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"*",&sArMsg) != SXRET_OK ){` |
|      - | 1202 | `			sxi32 rcAr;` |
|    ! 0 | 1203 | `			VmPopOperand(&pTos,1);` |
|    ! 0 | 1204 | `			PH7_MemObjRelease(pTos);` |
|    ! 0 | 1205 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 | 1206 | `			pTos->nIdx = SXU32_HIGH;` |
|    ! 0 | 1207 | `			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),` |
|    ! 0 | 1208 | `				SyBlobLength(&sArMsg));` |
|    ! 0 | 1209 | `			SyBlobRelease(&sArMsg);` |
|    ! 0 | 1210 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    ! 0 | 1211 | `			rc = rcAr;` |
|    ! 0 | 1212 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1213 | `		}` |
|   3053 | 1214 | `		SyBlobRelease(&sArMsg);` |
|      - | 1215 | `	}` |
|      - | 1216 | `	/* Force the operand to be numeric */` |
|      - | 1217 | `#ifdef UNTRUST` |
|      - | 1218 | `	if( pNos < pStack ){` |
|      - | 1219 | `		VM_EXIT_ABORT;` |
|      - | 1220 | `	}` |
|      - | 1221 | `#endif` |
|   3053 | 1222 | `	PH7_MemObjToNumeric(pTos);` |
|   3053 | 1223 | `	PH7_MemObjToNumeric(pNos);` |
|      - | 1224 | `	/* Perform the requested operation */` |
|   3053 | 1225 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|      - | 1226 | `		/* Floating point arithemic */` |
|      - | 1227 | `		ph7_real a,b,r;` |
|     33 | 1228 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     23 | 1229 | `			PH7_MemObjToReal(pTos);` |
|     11 | 1230 | `		}` |
|     33 | 1231 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      5 | 1232 | `			PH7_MemObjToReal(pNos);` |
|      2 | 1233 | `		}` |
|     33 | 1234 | `		a = pNos->rVal;` |
|     33 | 1235 | `		b = pTos->rVal;` |
|     33 | 1236 | `		r = a * b;` |
|      - | 1237 | `		/* Push the result */` |
|     33 | 1238 | `		pNos->rVal = r;` |
|     33 | 1239 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1240 | `		/* Try to get an integer representation */` |
|     33 | 1241 | `		PH7_MemObjTryInteger(pNos);` |
|     17 | 1242 | `	}else{` |
|      - | 1243 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|      - | 1244 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|      - | 1245 | `		sxi64 a,b,r;` |
|   3021 | 1246 | `		a = pNos->x.iVal;` |
|   3021 | 1247 | `		b = pTos->x.iVal;` |
|   3021 | 1248 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|      - | 1249 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|     13 | 1250 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|     13 | 1251 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|      - | 1252 | `#else` |
|      - | 1253 | `			pNos->x.iVal = r;` |
|      - | 1254 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1255 | `#endif` |
|      7 | 1256 | `		}else{` |
|   3009 | 1257 | `			pNos->x.iVal = r;` |
|   3009 | 1258 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|      - | 1259 | `		}` |
|      - | 1260 | `	}` |
|   3053 | 1261 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|      - | 1262 | `		ph7_value *pObj;` |
|     55 | 1263 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|    ! 0 | 1264 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");` |
|     55 | 1265 | `		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     55 | 1266 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     55 | 1267 | `			PH7_MemObjStore(pNos,pObj);` |
|     26 | 1268 | `		}` |
|     26 | 1269 | `	}` |
|   3053 | 1270 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|   3053 | 1271 | `	VmPopOperand(&pTos,1);` |
|   3053 | 1272 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1273 | `	VM_EXIT_BREAK;` |
|   1529 | 1274 | `}` |
|      - | 1275 |  |
