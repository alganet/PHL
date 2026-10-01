# src/ph7/vm_ops_arith.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 730/806 lines (90.57%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <math.h>  /* pow */` |
|       - |    8 | `/*` |
|       - |    9 | ` * Section:` |
|       - |   10 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|       - |   11 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|       - |   12 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|       - |   13 | ` *    its labels (same idiom as VmCallFinish).` |
|       - |   14 | ` * Status:` |
|       - |   15 | ` *    Stable.` |
|       - |   16 | ` */` |
|       - |   17 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   18 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   19 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   20 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   21 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   22 | `#include "vm_dispatch.h"` |
|       - |   23 | `#define sState (*pState)` |
|       - |   24 |  |
|       - |   25 | `/*` |
|       - |   26 | ` * A native compare handler REFUSED the pair the operator just asked about` |
|       - |   27 | ` * (php throws DateException comparing two different KINDS of DateTimeZone).` |
|       - |   28 | ` * PH7_MemObjCmp only recorded it -- it has no throw boundary of its own -- so` |
|       - |   29 | ` * the operator raises it here, where the expression's value would have landed:` |
|       - |   30 | ` * both operands go, a null stands in for the result the way every other` |
|       - |   31 | ` * mid-expression throw leaves one, and the status routes to the catching try.` |
|       - |   32 | ` * Used by the four LOOSE comparison arms; the strict pair never asks a handler.` |
|       - |   33 | ` */` |
|       - |   34 | `#define VM_CMP_REFUSAL_ROUTE() \` |
|       - |   35 | `	if( PH7_CmpRefusalPending(pVm) ){ \` |
|       - |   36 | `		sxi32 _rcCmp; \` |
|       - |   37 | `		VmPopOperand(&pTos,1); \` |
|       - |   38 | `		PH7_MemObjRelease(pTos); \` |
|       - |   39 | `		MemObjSetType(pTos,MEMOBJ_NULL); \` |
|       - |   40 | `		pTos->nIdx = SXU32_HIGH; \` |
|       - |   41 | `		_rcCmp = PH7_CmpRefusalRaise(pVm); \` |
|       - |   42 | `		if( _rcCmp == SXERR_ABORT ){ VM_EXIT_ABORT; } \` |
|       - |   43 | `		PH7_THROW_ROUTE_MIDEXPR(_rcCmp) \` |
|       - |   44 | `	}` |
|       - |   45 |  |
|       - |   46 | `/*` |
|       - |   47 | ` * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of` |
|       - |   48 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   49 | ` */` |
|     118 |   50 | `PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |   51 | `{` |
|     121 |   52 | `	ph7_value *pTos = pState->pTos;` |
|     121 |   53 | `	ph7_value *pStack = pState->pStack;` |
|     121 |   54 | `	VmInstr *aInstr = pState->aInstr;` |
|     121 |   55 | `	sxi32 pc = pState->pc;` |
|       - |   56 | `	sxi32 rc;` |
|      59 |   57 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     121 |   58 | `	ph7_value *pNos = &pTos[-1];` |
|       - |   59 | `	ph7_value *pObj;` |
|       - |   60 | `	sxu32 nIdx;` |
|       - |   61 | `#ifdef UNTRUST` |
|       - |   62 | `	if( pNos < pStack ){` |
|       - |   63 | `		VM_EXIT_ABORT;` |
|       - |   64 | `	}` |
|       - |   65 | `#endif` |
|     121 |   66 | `	if( SySetUsed(&pVm->aHookRmw) > 0 ){` |
|       - |   67 | ``		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a`` |
|       - |   68 | `		 * COAL entry targeting exactly THIS store (owner + pc identity — a` |
|       - |   69 | `		 * stale entry from an abandoned statement can never match, and nested` |
|       - |   70 | `		 * arms in the RHS were consumed/dropped above this one). Dispatch the` |
|       - |   71 | `		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=` |
|       - |   72 | `		 * residual: pre-fix the assign bypassed __set through the normal slot` |
|       - |   73 | `		 * path). The RHS stays as the expression result. */` |
|      15 |   74 | `		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);` |
|      14 |   75 | `		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr` |
|      14 |   76 | `		 && pTop->nPc == (sxu32)pc` |
|      15 |   77 | `		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK \|\| pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){` |
|      15 |   78 | `			VmHookRmw sPend = *pTop;` |
|      15 |   79 | `			(void)SySetPop(&pVm->aHookRmw);` |
|      15 |   80 | `			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){` |
|      11 |   81 | `				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);` |
|      11 |   82 | `				if( rcHs == PH7_ABORT ){` |
|     ! 0 |   83 | `					SyBlobRelease(&sPend.sName);` |
|     ! 0 |   84 | `					PH7_ClassInstanceUnref(sPend.pThis);` |
|     ! 0 |   85 | `					VM_EXIT_ABORT;` |
|       - |   86 | `				}` |
|       6 |   87 | `			}else{` |
|       - |   88 | `				SyString sSetName;` |
|       5 |   89 | `				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));` |
|       5 |   90 | `				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);` |
|       - |   91 | `			}` |
|      15 |   92 | `			SyBlobRelease(&sPend.sName);` |
|      15 |   93 | `			PH7_ClassInstanceUnref(sPend.pThis);` |
|      15 |   94 | `			PH7_MemObjStore(pTos,pNos);` |
|      15 |   95 | `			pNos->nIdx = SXU32_HIGH;` |
|      15 |   96 | `			VmPopOperand(&pTos,1);` |
|      15 |   97 | `			VM_EXIT_BREAK;` |
|       - |   98 | `		}` |
|     ! 0 |   99 | `	}` |
|       - |  100 | `	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3` |
|       - |  101 | `	 * armed pVm with the (object, key) on a missing key. Dispatch to` |
|       - |  102 | `	 * offsetSet instead of writing through the synthetic pNos->nIdx. */` |
|     107 |  103 | `	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){` |
|      10 |  104 | `		ph7_class_instance *pInst = pVm->pCoalesceObj;` |
|      10 |  105 | `		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  106 | `			"offsetSet",sizeof("offsetSet")-1);` |
|       - |  107 | `		ph7_value *apArg[2];` |
|      10 |  108 | `		apArg[0] = &pVm->sCoalesceKey;` |
|      10 |  109 | `		apArg[1] = pTos;` |
|      10 |  110 | `		if( pSet == 0 ){` |
|       - |  111 | `			/* A container that answered the READ and has no ArrayAccess: its own` |
|       - |  112 | `			 * dimension handler is offered the store first -- php's` |
|       - |  113 | `			 * SimpleXMLElement takes it. One that stores nothing (DOMNodeList,` |
|       - |  114 | `			 * PDORow) leaves it, and the store is then the same one` |
|       - |  115 | ``			 * `$list[9] = 'x'` performs, so it takes the same Error. */`` |
|       - |  116 | `			char zMsg[256];` |
|       - |  117 | `			sxu32 nMsg;` |
|       - |  118 | `			PH7_NativeDimCtx sDim;` |
|       3 |  119 | `			if( PH7_ClassNativeDimStore(pInst,PH7_NATIVE_DIM_WRITE,` |
|       3 |  120 | `				&pVm->sCoalesceKey,pTos,&sDim) && sDim.zThrowClass == 0 ){` |
|     ! 0 |  121 | `				PH7_MemObjStore(pTos,pNos);` |
|     ! 0 |  122 | `				VmPopOperand(&pTos,1);` |
|     ! 0 |  123 | `				VmCoalesceDisarm(pVm);` |
|     ! 0 |  124 | `				VM_EXIT_BREAK;` |
|       - |  125 | `			}` |
|       3 |  126 | `			if( sDim.zThrowClass ){` |
|     ! 0 |  127 | `				rc = VmThrowFromVm(&(*pVm),sDim.zThrowClass,sDim.zThrowMsg,` |
|     ! 0 |  128 | `					(sxu32)SyStrlen(sDim.zThrowMsg));` |
|     ! 0 |  129 | `				VmPopOperand(&pTos,1);` |
|     ! 0 |  130 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  131 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 |  132 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 |  133 | `				VmCoalesceDisarm(pVm);` |
|     ! 0 |  134 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 |  135 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  136 | `			}` |
|       4 |  137 | `			nMsg = PH7_ClassNativeDimRefusal(pInst,PH7_NATIVE_DIM_WRITE,` |
|       1 |  138 | `				zMsg,sizeof(zMsg));` |
|       3 |  139 | `			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);` |
|       3 |  140 | `			VmPopOperand(&pTos,1);` |
|       3 |  141 | `			PH7_MemObjRelease(pTos);` |
|       3 |  142 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  143 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  144 | `			VmCoalesceDisarm(pVm);` |
|       3 |  145 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  146 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  147 | `		}` |
|       8 |  148 | `		PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);` |
|       - |  149 | `		/* Leave RHS as the expression result (replace pNos with pTos). */` |
|       8 |  150 | `		PH7_MemObjStore(pTos,pNos);` |
|       8 |  151 | `		VmPopOperand(&pTos,1);` |
|       - |  152 | `		/* Disarm and release the cached instance ref + key. */` |
|       8 |  153 | `		VmCoalesceDisarm(pVm);` |
|       8 |  154 | `		VM_EXIT_BREAK;` |
|       - |  155 | `	}` |
|      98 |  156 | `	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){` |
|       - |  157 | ``		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —`` |
|       - |  158 | ``		 * `??=` is not an assign-op — padding with spaces when the offset is past`` |
|       - |  159 | `		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset` |
|       - |  160 | `		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:` |
|       - |  161 | ``		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the`` |
|       - |  162 | `		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested` |
|       - |  163 | ``		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an`` |
|       - |  164 | `		 * offset the quiet peek let through raises at the store. */` |
|      43 |  165 | `		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;` |
|      64 |  166 | `		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH` |
|      42 |  167 | `			? (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pNos->nIdx) : 0;` |
|      43 |  168 | `		sxi64 iOfft = 0;` |
|       - |  169 | `		SyBlob sTypeMsg;` |
|       - |  170 | `		int eOfft;` |
|      43 |  171 | `		SyBlobInit(&sTypeMsg,&pVm->sAllocator);` |
|      43 |  172 | `		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,` |
|       - |  173 | `			&iOfft,&sTypeMsg);` |
|      43 |  174 | `		if( eOfft == VM_STROFF_REJECT ){` |
|       - |  175 | `			sxi32 rcSo;` |
|       3 |  176 | `			VmPopOperand(&pTos,1);` |
|       3 |  177 | `			PH7_MemObjRelease(pTos);` |
|       3 |  178 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  179 | `			pTos->nIdx = SXU32_HIGH;` |
|       3 |  180 | `			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|       3 |  181 | `			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  182 | `			rc = rcSo;` |
|       3 |  183 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  184 | `		}` |
|      41 |  185 | `		SyBlobRelease(&sTypeMsg);` |
|       - |  186 | `		/* The RHS takes the same user-visible string coercion as a plain store. */` |
|       - |  187 | `		{` |
|      41 |  188 | `			sxi32 rcSv = PH7_MemObjToStringUV(pTos);` |
|      41 |  189 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|       - |  190 | `		}` |
|      41 |  191 | `		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){` |
|      41 |  192 | `			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){` |
|       - |  193 | `				sxi32 rcEm;` |
|       5 |  194 | `				VmPopOperand(&pTos,1);` |
|       5 |  195 | `				PH7_MemObjRelease(pTos);` |
|       5 |  196 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 |  197 | `				pTos->nIdx = SXU32_HIGH;` |
|       5 |  198 | `				rcEm = VmThrowFromVm(&(*pVm),"Error",` |
|       - |  199 | `					"Cannot assign an empty string to a string offset",` |
|       - |  200 | `					sizeof("Cannot assign an empty string to a string offset")-1);` |
|       5 |  201 | `				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 |  202 | `				rc = rcEm;` |
|       5 |  203 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  204 | `			}` |
|      18 |  205 | `		}` |
|       - |  206 | `		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does` |
|       - |  207 | `		 * not free what the carrier owns, so release it here — every other exit` |
|       - |  208 | `		 * from this arm routes through PH7_MemObjRelease, which does. */` |
|      37 |  209 | `		VmFreeCoalStrOff(pCoalOff);` |
|      37 |  210 | `		pNos->x.pOther = 0;` |
|      37 |  211 | `		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;` |
|       - |  212 | `		/* Leave the RHS as the expression's value, like every other arm. */` |
|      37 |  213 | `		PH7_MemObjStore(pTos,pNos);` |
|      37 |  214 | `		pNos->nIdx = SXU32_HIGH;` |
|      37 |  215 | `		VmPopOperand(&pTos,1);` |
|      37 |  216 | `		VM_EXIT_BREAK;` |
|       - |  217 | `	}` |
|      56 |  218 | `	nIdx = pNos->nIdx;` |
|      56 |  219 | `	if( nIdx == SXU32_HIGH ){` |
|       - |  220 | ``		/* A read-modify-write THROUGH a temporary (`f()[0] .= "x"`, `mk()->p += 1`):`` |
|       - |  221 | `		 * php computes it, drops it with the temporary and stays silent. Every case` |
|       - |  222 | `		 * that IS a refusal — a class constant, a hooked or handler-backed property —` |
|       - |  223 | `		 * is decided before the VM sees it. */` |
|      56 |  224 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,nIdx)) != 0 ){` |
|      56 |  225 | `		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);` |
|      54 |  226 | `		PH7_MemObjStore(pTos,pObj);` |
|      26 |  227 | `	}` |
|      54 |  228 | `	PH7_MemObjStore(pTos,pNos);` |
|      54 |  229 | `	VmPopOperand(&pTos,1);` |
|      54 |  230 | `	VM_EXIT_BREAK;` |
|     ! 0 |  231 | `	VM_EXIT_BREAK;` |
|      62 |  232 | `}` |
|       - |  233 |  |
|       - |  234 | `/*` |
|       - |  235 | ` * OP_DIV: body moved verbatim from the OP_DIV arm of` |
|       - |  236 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  237 | ` */` |
|     958 |  238 | `PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |  239 | `{` |
|       - |  240 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  241 | `	int rcNa;` |
|     961 |  242 | `	const char *zArCls = "TypeError";` |
|     961 |  243 | `	ph7_value *pTos = pState->pTos;` |
|     961 |  244 | `	ph7_value *pStack = pState->pStack;` |
|     961 |  245 | `	VmInstr *aInstr = pState->aInstr;` |
|     961 |  246 | `	sxi32 pc = pState->pc;` |
|       - |  247 | `	sxi32 rc;` |
|     479 |  248 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     961 |  249 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  250 | `	{` |
|       - |  251 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  252 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  253 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  254 | `		SyBlob sArMsg;` |
|     961 |  255 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     961 |  256 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"/",pNos,&zArCls,&sArMsg);` |
|     961 |  257 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  258 | `			sxi32 rcAr;` |
|     161 |  259 | `			VmPopOperand(&pTos,1);` |
|     161 |  260 | `			PH7_MemObjRelease(pTos);` |
|     161 |  261 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     161 |  262 | `			pTos->nIdx = SXU32_HIGH;` |
|     241 |  263 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  264 | `				SyBlobLength(&sArMsg));` |
|     161 |  265 | `			SyBlobRelease(&sArMsg);` |
|     241 |  266 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     161 |  267 | `			rc = rcAr;` |
|     161 |  268 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  269 | `		}` |
|     801 |  270 | `		SyBlobRelease(&sArMsg);` |
|       - |  271 | `	}` |
|     801 |  272 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  273 | `		VmPopOperand(&pTos,1);` |
|       3 |  274 | `		VM_EXIT_BREAK;` |
|       - |  275 | `	}` |
|       - |  276 | `	ph7_real a,b,r;` |
|       - |  277 | `#ifdef UNTRUST` |
|       - |  278 | `	if( pNos < pStack ){` |
|       - |  279 | `		VM_EXIT_ABORT;` |
|       - |  280 | `	}` |
|       - |  281 | `#endif` |
|       - |  282 | ``	/* php's `/`: an int/int division whose remainder is 0 yields an *int*`` |
|       - |  283 | `	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact` |
|       - |  284 | `	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.` |
|       - |  285 | `	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which` |
|       - |  286 | `	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */` |
|     799 |  287 | `	PH7_MemObjToNumeric(pTos);` |
|     799 |  288 | `	PH7_MemObjToNumeric(pNos);` |
|     799 |  289 | `	if( ((pTos->iFlags\|pNos->iFlags) & MEMOBJ_REAL) == 0 ){` |
|     730 |  290 | `		sxi64 ia = pNos->x.iVal;` |
|     730 |  291 | `		sxi64 ib = pTos->x.iVal;` |
|     730 |  292 | `		sxi64 iQuot = 0;` |
|     730 |  293 | `		int bExact = 0;` |
|     730 |  294 | `		if( ib == 0 ){` |
|      74 |  295 | `			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|      76 |  296 | `			PH7_DISPATCH_ENFORCE_RC(rc)` |
|     658 |  297 | `		}else if( ib == -1 ){` |
|       - |  298 | ``			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose`` |
|       - |  299 | `			 * magnitude does not fit -- that one leaves bExact clear and takes the` |
|       - |  300 | `			 * float path below, as php does. The divisor has to be screened BEFORE` |
|       - |  301 | ``			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1`` |
|       - |  302 | `			 * alongside the remainder, so testing the remainder first trapped` |
|       - |  303 | `			 * (SIGFPE) on exactly the value the guard was written to protect.` |
|       - |  304 | `			 * OP_MOD screens the same hazard the same way. */` |
|       - |  305 | `#ifdef PH7_OMIT_FLOATING_POINT` |
|       - |  306 | `			/* The integer-only build has no float to promote to (as OP_ADD's` |
|       - |  307 | ``			 * overflow arm) and its `real` division is this same trapping integer`` |
|       - |  308 | `			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */` |
|       - |  309 | `			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;` |
|       - |  310 | `			bExact = 1;` |
|       - |  311 | `#else` |
|      19 |  312 | `			if( ia != SMALLEST_INT64 ){` |
|      15 |  313 | `				iQuot = -ia;` |
|      15 |  314 | `				bExact = 1;` |
|       8 |  315 | `			}` |
|       - |  316 | `#endif` |
|     649 |  317 | `		}else if( ia % ib == 0 ){` |
|     130 |  318 | `			iQuot = ia / ib;` |
|     130 |  319 | `			bExact = 1;` |
|      64 |  320 | `		}` |
|     658 |  321 | `		if( bExact ){` |
|     144 |  322 | `			pNos->x.iVal = iQuot;` |
|     144 |  323 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|     144 |  324 | `			VmPopOperand(&pTos,1);` |
|     144 |  325 | `			VM_EXIT_BREAK;` |
|       - |  326 | `		}` |
|     257 |  327 | `	}` |
|       - |  328 | `	/* Force the operands to be real */` |
|     584 |  329 | `	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     567 |  330 | `		PH7_MemObjToReal(pTos);` |
|     283 |  331 | `	}` |
|     584 |  332 | `	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     515 |  333 | `		PH7_MemObjToReal(pNos);` |
|     257 |  334 | `	}` |
|       - |  335 | `	/* Perform the requested operation */` |
|     584 |  336 | `	a = pNos->rVal;` |
|     584 |  337 | `	b = pTos->rVal;` |
|     584 |  338 | `	if( b == 0 ){` |
|       - |  339 | `		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  340 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|       3 |  341 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");` |
|       3 |  342 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  343 | `	}else{` |
|     582 |  344 | `		r = a/b;` |
|       - |  345 | `		/* Push the result */` |
|     582 |  346 | `		pNos->rVal = r;` |
|     582 |  347 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  348 | `	}` |
|     582 |  349 | `	VmPopOperand(&pTos,1);` |
|     582 |  350 | `	VM_EXIT_BREAK;` |
|     ! 0 |  351 | `	VM_EXIT_BREAK;` |
|     482 |  352 | `}` |
|       - |  353 |  |
|       - |  354 | `/*` |
|       - |  355 | ` * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of` |
|       - |  356 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  357 | ` */` |
|     406 |  358 | `PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 |  359 | `{` |
|       - |  360 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  361 | `	int rcNa;` |
|     407 |  362 | `	const char *zArCls = "TypeError";` |
|     407 |  363 | `	ph7_value *pTos = pState->pTos;` |
|     407 |  364 | `	ph7_value *pStack = pState->pStack;` |
|     407 |  365 | `	VmInstr *aInstr = pState->aInstr;` |
|     407 |  366 | `	sxi32 pc = pState->pc;` |
|       - |  367 | `	sxi32 rc;` |
|     203 |  368 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     407 |  369 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  370 | `	ph7_value *pObj;` |
|       - |  371 | `	sxi64 a,b,r;` |
|       - |  372 | `#ifdef UNTRUST` |
|       - |  373 | `	if( pNos < pStack ){` |
|       - |  374 | `		VM_EXIT_ABORT;` |
|       - |  375 | `	}` |
|       - |  376 | `#endif` |
|       - |  377 | `	{` |
|       - |  378 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  379 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  380 | `		SyBlob sArMsg;` |
|     407 |  381 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     407 |  382 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"%",pNos,&zArCls,&sArMsg);` |
|     407 |  383 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  384 | `			sxi32 rcAr;` |
|     151 |  385 | `			VmPopOperand(&pTos,1);` |
|     151 |  386 | `			PH7_MemObjRelease(pTos);` |
|     151 |  387 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     151 |  388 | `			pTos->nIdx = SXU32_HIGH;` |
|     226 |  389 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      75 |  390 | `				SyBlobLength(&sArMsg));` |
|     151 |  391 | `			SyBlobRelease(&sArMsg);` |
|     226 |  392 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     151 |  393 | `			rc = rcAr;` |
|     151 |  394 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  395 | `		}` |
|     257 |  396 | `		SyBlobRelease(&sArMsg);` |
|       - |  397 | `	}` |
|     257 |  398 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  399 | `		goto mod_store_write;` |
|       - |  400 | `	}` |
|       - |  401 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     257 |  402 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     257 |  403 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  404 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     257 |  405 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     257 |  406 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     113 |  407 | `		PH7_MemObjToInteger(pTos);` |
|      56 |  408 | `	}` |
|     257 |  409 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     111 |  410 | `		PH7_MemObjToInteger(pNos);` |
|      55 |  411 | `	}` |
|       - |  412 | `	/* Perform the requested operation */` |
|     257 |  413 | `	a = pTos->x.iVal;` |
|     257 |  414 | `	b = pNos->x.iVal;` |
|     257 |  415 | `	if( b == 0 ){` |
|       - |  416 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  417 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      71 |  418 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      71 |  419 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  420 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|     187 |  421 | `	}else if( b == -1 ){` |
|       - |  422 | ``		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap`` |
|       - |  423 | `		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */` |
|       3 |  424 | `		r = 0;` |
|       2 |  425 | `	}else{` |
|     185 |  426 | `		r = a%b;` |
|       - |  427 | `	}` |
|       - |  428 | `	/* Push the result */` |
|     187 |  429 | `	pNos->x.iVal = r;` |
|     187 |  430 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|      93 |  431 | `mod_store_write:` |
|     187 |  432 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  433 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     186 |  434 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     185 |  435 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     185 |  436 | `		PH7_MemObjStore(pNos,pObj);` |
|      92 |  437 | `	}` |
|     187 |  438 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     187 |  439 | `	VmPopOperand(&pTos,1);` |
|     187 |  440 | `	VM_EXIT_BREAK;` |
|     ! 0 |  441 | `	VM_EXIT_BREAK;` |
|     204 |  442 | `}` |
|       - |  443 |  |
|       - |  444 | `/*` |
|       - |  445 | ` * OP_MOD: body moved verbatim from the OP_MOD arm of` |
|       - |  446 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  447 | ` */` |
|    2332 |  448 | `PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  449 | `{` |
|       - |  450 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  451 | `	int rcNa;` |
|    2337 |  452 | `	const char *zArCls = "TypeError";` |
|    2337 |  453 | `	ph7_value *pTos = pState->pTos;` |
|    2337 |  454 | `	ph7_value *pStack = pState->pStack;` |
|    2337 |  455 | `	VmInstr *aInstr = pState->aInstr;` |
|    2337 |  456 | `	sxi32 pc = pState->pc;` |
|       - |  457 | `	sxi32 rc;` |
|    1166 |  458 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    2337 |  459 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  460 | `	{` |
|       - |  461 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  462 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  463 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  464 | `		SyBlob sArMsg;` |
|    2337 |  465 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    2337 |  466 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"%",pNos,&zArCls,&sArMsg);` |
|    2337 |  467 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  468 | `			sxi32 rcAr;` |
|     163 |  469 | `			VmPopOperand(&pTos,1);` |
|     163 |  470 | `			PH7_MemObjRelease(pTos);` |
|     163 |  471 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     163 |  472 | `			pTos->nIdx = SXU32_HIGH;` |
|     244 |  473 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      81 |  474 | `				SyBlobLength(&sArMsg));` |
|     163 |  475 | `			SyBlobRelease(&sArMsg);` |
|     244 |  476 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     163 |  477 | `			rc = rcAr;` |
|     163 |  478 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  479 | `		}` |
|    2175 |  480 | `		SyBlobRelease(&sArMsg);` |
|       - |  481 | `	}` |
|    2175 |  482 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       3 |  483 | `		VmPopOperand(&pTos,1);` |
|       3 |  484 | `		VM_EXIT_BREAK;` |
|       - |  485 | `	}` |
|       - |  486 | `	sxi64 a,b,r;` |
|       - |  487 | `#ifdef UNTRUST` |
|       - |  488 | `	if( pNos < pStack ){` |
|       - |  489 | `		VM_EXIT_ABORT;` |
|       - |  490 | `	}` |
|       - |  491 | `#endif` |
|       - |  492 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    2173 |  493 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    2173 |  494 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    2159 |  495 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    2159 |  496 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    2157 |  497 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     117 |  498 | `		PH7_MemObjToInteger(pTos);` |
|      58 |  499 | `	}` |
|    2157 |  500 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     122 |  501 | `		PH7_MemObjToInteger(pNos);` |
|      60 |  502 | `	}` |
|       - |  503 | `	/* Perform the requested operation */` |
|    2157 |  504 | `	a = pNos->x.iVal;` |
|    2157 |  505 | `	b = pTos->x.iVal;` |
|    2157 |  506 | `	if( b == 0 ){` |
|       - |  507 | `		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),` |
|       - |  508 | `		 * not the old non-catchable warning that continued with a 0 result. */` |
|      76 |  509 | `		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");` |
|      80 |  510 | `		PH7_DISPATCH_ENFORCE_RC(rc)` |
|     ! 0 |  511 | `		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */` |
|    2083 |  512 | `	}else if( b == -1 ){` |
|       - |  513 | ``		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed`` |
|       - |  514 | `		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU` |
|       - |  515 | `		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the` |
|       - |  516 | `		 * remainder. php's result here is 0. */` |
|       5 |  517 | `		r = 0;` |
|       3 |  518 | `	}else{` |
|    2079 |  519 | `		r = a%b;` |
|       - |  520 | `	}` |
|       - |  521 | `	/* Push the result */` |
|    2083 |  522 | `	pNos->x.iVal = r;` |
|    2083 |  523 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    2083 |  524 | `	VmPopOperand(&pTos,1);` |
|    2083 |  525 | `	VM_EXIT_BREAK;` |
|     ! 0 |  526 | `	VM_EXIT_BREAK;` |
|    1171 |  527 | `}` |
|       - |  528 |  |
|       - |  529 | `/*` |
|       - |  530 | ` * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of` |
|       - |  531 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  532 | ` */` |
|     408 |  533 | `PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  534 | `{` |
|       - |  535 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  536 | `	int rcNa;` |
|     410 |  537 | `	const char *zArCls = "TypeError";` |
|     410 |  538 | `	ph7_value *pTos = pState->pTos;` |
|     410 |  539 | `	ph7_value *pStack = pState->pStack;` |
|     410 |  540 | `	VmInstr *aInstr = pState->aInstr;` |
|     410 |  541 | `	sxi32 pc = pState->pc;` |
|       - |  542 | `	sxi32 rc;` |
|     204 |  543 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     410 |  544 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  545 | `	ph7_value *pObj;` |
|       - |  546 | `#ifdef UNTRUST` |
|       - |  547 | `	if( pNos < pStack ){` |
|       - |  548 | `		VM_EXIT_ABORT;` |
|       - |  549 | `	}` |
|       - |  550 | `#endif` |
|       - |  551 | `	{` |
|       - |  552 | `		/* php's operand contract: a compound-assign with a non-numeric string,` |
|       - |  553 | `		 * array, object or resource operand is a TypeError too. */` |
|       - |  554 | `		SyBlob sArMsg;` |
|     410 |  555 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|     410 |  556 | `		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"-",pNos,&zArCls,&sArMsg);` |
|     410 |  557 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  558 | `			sxi32 rcAr;` |
|     153 |  559 | `			VmPopOperand(&pTos,1);` |
|     153 |  560 | `			PH7_MemObjRelease(pTos);` |
|     153 |  561 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     153 |  562 | `			pTos->nIdx = SXU32_HIGH;` |
|     229 |  563 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      76 |  564 | `				SyBlobLength(&sArMsg));` |
|     153 |  565 | `			SyBlobRelease(&sArMsg);` |
|     229 |  566 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     153 |  567 | `			rc = rcAr;` |
|     153 |  568 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  569 | `		}` |
|     258 |  570 | `		SyBlobRelease(&sArMsg);` |
|       - |  571 | `	}` |
|     258 |  572 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|     ! 0 |  573 | `		goto sub_store_write;` |
|       - |  574 | `	}` |
|       - |  575 | `	/* Force the operands to be numeric (see OP_SUB) */` |
|     258 |  576 | `	PH7_MemObjToNumeric(pTos);` |
|     258 |  577 | `	PH7_MemObjToNumeric(pNos);` |
|     386 |  578 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  579 | `		/* Floating point arithemic */` |
|       - |  580 | `		ph7_real a,b,r;` |
|     ! 0 |  581 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  582 | `			PH7_MemObjToReal(pTos);` |
|     ! 0 |  583 | `		}` |
|     ! 0 |  584 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|     ! 0 |  585 | `			PH7_MemObjToReal(pNos);` |
|     ! 0 |  586 | `		}` |
|     ! 0 |  587 | `		a = pTos->rVal;` |
|     ! 0 |  588 | `		b = pNos->rVal;` |
|     ! 0 |  589 | `		r = a - b;` |
|       - |  590 | `		/* Push the result */` |
|     ! 0 |  591 | `		pNos->rVal = r;` |
|     ! 0 |  592 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  593 | `		/* Try to get an integer representation */` |
|     ! 0 |  594 | `		PH7_MemObjTryInteger(pNos);` |
|     ! 0 |  595 | `	}else{` |
|       - |  596 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  597 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  598 | `		sxi64 a,b,r;` |
|     258 |  599 | `		a = pTos->x.iVal;` |
|     258 |  600 | `		b = pNos->x.iVal;` |
|     258 |  601 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  602 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       7 |  603 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|       7 |  604 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  605 | `#else` |
|       - |  606 | `			pNos->x.iVal = r;` |
|       - |  607 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  608 | `#endif` |
|       4 |  609 | `		}else{` |
|     252 |  610 | `			pNos->x.iVal = r;` |
|     252 |  611 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  612 | `		}` |
|       - |  613 | `	}` |
|     128 |  614 | `sub_store_write:` |
|     258 |  615 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  616 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     258 |  617 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     258 |  618 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     258 |  619 | `		PH7_MemObjStore(pNos,pObj);` |
|     128 |  620 | `	}` |
|     258 |  621 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     258 |  622 | `	VmPopOperand(&pTos,1);` |
|     258 |  623 | `	VM_EXIT_BREAK;` |
|     ! 0 |  624 | `	VM_EXIT_BREAK;` |
|     206 |  625 | `}` |
|       - |  626 |  |
|       - |  627 | `/*` |
|       - |  628 | ` * OP_SUB: body moved verbatim from the OP_SUB arm of` |
|       - |  629 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  630 | ` */` |
|   61564 |  631 | `PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  632 | `{` |
|       - |  633 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  634 | `	int rcNa;` |
|   61569 |  635 | `	const char *zArCls = "TypeError";` |
|   61569 |  636 | `	ph7_value *pTos = pState->pTos;` |
|   61569 |  637 | `	ph7_value *pStack = pState->pStack;` |
|   61569 |  638 | `	VmInstr *aInstr = pState->aInstr;` |
|   61569 |  639 | `	sxi32 pc = pState->pc;` |
|       - |  640 | `	sxi32 rc;` |
|   31085 |  641 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   61569 |  642 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  643 | `#ifdef UNTRUST` |
|       - |  644 | `	if( pNos < pStack ){` |
|       - |  645 | `		VM_EXIT_ABORT;` |
|       - |  646 | `	}` |
|       - |  647 | `#endif` |
|       - |  648 | `	{` |
|       - |  649 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  650 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  651 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands. */` |
|       - |  652 | `		SyBlob sArMsg;` |
|   61569 |  653 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|   61569 |  654 | `		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"-",pNos,&zArCls,&sArMsg);` |
|   61569 |  655 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  656 | `			sxi32 rcAr;` |
|     162 |  657 | `			VmPopOperand(&pTos,1);` |
|     162 |  658 | `			PH7_MemObjRelease(pTos);` |
|     162 |  659 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     162 |  660 | `			pTos->nIdx = SXU32_HIGH;` |
|     242 |  661 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|      80 |  662 | `				SyBlobLength(&sArMsg));` |
|     162 |  663 | `			SyBlobRelease(&sArMsg);` |
|     242 |  664 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     162 |  665 | `			rc = rcAr;` |
|     162 |  666 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  667 | `		}` |
|   61409 |  668 | `		SyBlobRelease(&sArMsg);` |
|       - |  669 | `	}` |
|   61409 |  670 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 |  671 | `		VmPopOperand(&pTos,1);` |
|       5 |  672 | `		VM_EXIT_BREAK;` |
|       - |  673 | `	}` |
|       - |  674 | `	/* Force the operands to be numeric. Without this a string operand fell through` |
|       - |  675 | `	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"` |
|       - |  676 | `	 * quietly evaluated to 0. */` |
|   61405 |  677 | `	PH7_MemObjToNumeric(pTos);` |
|   61405 |  678 | `	PH7_MemObjToNumeric(pNos);` |
|   61405 |  679 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - |  680 | `		/* Floating point arithemic */` |
|       - |  681 | `		ph7_real a,b,r;` |
|     157 |  682 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      21 |  683 | `			PH7_MemObjToReal(pTos);` |
|      10 |  684 | `		}` |
|     157 |  685 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       9 |  686 | `			PH7_MemObjToReal(pNos);` |
|       4 |  687 | `		}` |
|     157 |  688 | `		a = pNos->rVal;` |
|     157 |  689 | `		b = pTos->rVal;` |
|     157 |  690 | `		r = a - b;` |
|       - |  691 | `		/* Push the result */` |
|     157 |  692 | `		pNos->rVal = r;` |
|     157 |  693 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  694 | `		/* Try to get an integer representation */` |
|     157 |  695 | `		PH7_MemObjTryInteger(pNos);` |
|      80 |  696 | `	}else{` |
|       - |  697 | `		/* Integer arithmetic; PHP promotes an overflowing difference to float.` |
|       - |  698 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - |  699 | `		sxi64 a,b,r;` |
|   61251 |  700 | `		a = pNos->x.iVal;` |
|   61251 |  701 | `		b = pTos->x.iVal;` |
|   61251 |  702 | `		if( PH7_SUB_OVERFLOW64(a,b,&r) ){` |
|       - |  703 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 |  704 | `			pNos->rVal = (ph7_real)a - (ph7_real)b;` |
|      11 |  705 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - |  706 | `#else` |
|       - |  707 | `			pNos->x.iVal = r;` |
|       - |  708 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  709 | `#endif` |
|       6 |  710 | `		}else{` |
|   61241 |  711 | `			pNos->x.iVal = r;` |
|   61241 |  712 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - |  713 | `		}` |
|       - |  714 | `	}` |
|   61405 |  715 | `	VmPopOperand(&pTos,1);` |
|   61405 |  716 | `	VM_EXIT_BREAK;` |
|     ! 0 |  717 | `	VM_EXIT_BREAK;` |
|   31090 |  718 | `}` |
|       - |  719 |  |
|       - |  720 | `/*` |
|       - |  721 | `` * php's `**`, as a value operation.`` |
|       - |  722 | ` *` |
|       - |  723 | ` * In php pow() IS the exponentiation operator -- both compile to the same` |
|       - |  724 | ` * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and` |
|       - |  725 | ` * every edge value have to come from ONE place here too. This is that place:` |
|       - |  726 | ` * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call` |
|       - |  727 | ` * it, after the caller has run VmArithOperandCheck() over the two operands (the` |
|       - |  728 | ` * two sites word their throw differently -- one settles an operand stack first,` |
|       - |  729 | ` * the other is inside a C builtin -- so the CHECK stays with the caller and only` |
|       - |  730 | ` * the arithmetic is shared).` |
|       - |  731 | ` *` |
|       - |  732 | ` * pBase and pExp are converted in place, which is what the opcode arm already` |
|       - |  733 | ` * did to its stack slots; pOut may alias either of them and is written last.` |
|       - |  734 | ` */` |
|     790 |  735 | `PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)` |
|       3 |  736 | `{` |
|       - |  737 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|       - |  738 | `	int bBothInt;` |
|     793 |  739 | `	int usedInt = 0;` |
|       - |  740 | `	ph7_real a, b, r;` |
|       - |  741 | `#endif` |
|     793 |  742 | `	sxi64 base_i = 0, exp_i = 0;` |
|     793 |  743 | `	PH7_MemObjToNumeric(pBase);` |
|     793 |  744 | `	PH7_MemObjToNumeric(pExp);` |
|       - |  745 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    1547 |  746 | `	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&` |
|     754 |  747 | `	           ((pExp->iFlags & MEMOBJ_REAL) == 0);` |
|     793 |  748 | `	if( bBothInt ){` |
|     749 |  749 | `		base_i = pBase->x.iVal;` |
|     749 |  750 | `		exp_i  = pExp->x.iVal;` |
|     373 |  751 | `	}` |
|     793 |  752 | `	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){` |
|     757 |  753 | `		PH7_MemObjToReal(pBase);` |
|     377 |  754 | `	}` |
|     793 |  755 | `	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){` |
|     785 |  756 | `		PH7_MemObjToReal(pExp);` |
|     391 |  757 | `	}` |
|     793 |  758 | `	a = pBase->rVal;` |
|     793 |  759 | `	b = pExp->rVal;` |
|     793 |  760 | `	r = pow(a, b);` |
|       - |  761 | `	/* int ** non-negative int is php's OWN loop, not a call to pow(), and the` |
|       - |  762 | `	 * difference is visible in the answer twice over. php multiplies in` |
|       - |  763 | ``	 * `pow_function_base`'s doubling loop and, the moment a step OVERFLOWS, finishes`` |
|       - |  764 | ``	 * in DOUBLE space FROM THERE — `dval * pow(l2, i)` with whatever exponent is`` |
|       - |  765 | `	 * left — rather than re-computing pow(base, exp) from the original operands.` |
|       - |  766 | ``	 * That carries the accumulated SIGN, so `(-3) ** PHP_INT_MAX` is -INF where`` |
|       - |  767 | ``	 * pow() answers +INF, and it rounds differently, so `3 ** 100` is`` |
|       - |  768 | ``	 * 5.1537752073201141e+47 where pow() gives ...132e+47 and `10 ** 64` is`` |
|       - |  769 | `	 * 1.0000000000000002e+64 where pow() gives exactly 1e+64. 33 rows of a 380-row` |
|       - |  770 | `	 * base×exponent sweep were wrong, sign included.` |
|       - |  771 | `	 * The overflowing product is the DOUBLE product of the two operands, which is` |
|       - |  772 | `	 * what php's ZEND_SIGNED_MULTIPLY_LONG hands back wherever it detects the` |
|       - |  773 | `	 * overflow with a builtin (gcc/clang) or with _mul128 (MSVC) — the two` |
|       - |  774 | `	 * platforms PHL builds on. */` |
|     793 |  775 | `	if( bBothInt && exp_i >= 0 ){` |
|     689 |  776 | `		sxi64 l1 = 1, l2 = base_i, i = exp_i, iProd;` |
|     689 |  777 | `		if( i == 0 ){` |
|       - |  778 | `			/* Anything to the 0 is int 1 — php answers before it looks at the base. */` |
|     149 |  779 | `			pOut->x.iVal = 1;` |
|     149 |  780 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|     149 |  781 | `			usedInt = 1;` |
|     615 |  782 | `		}else if( l2 == 0 ){` |
|      93 |  783 | `			pOut->x.iVal = 0;` |
|      93 |  784 | `			MemObjSetType(pOut, MEMOBJ_INT);` |
|      93 |  785 | `			usedInt = 1;` |
|      47 |  786 | `		}else{` |
|    3475 |  787 | `			while( i >= 1 ){` |
|    3475 |  788 | `				if( i % 2 ){` |
|    1775 |  789 | `					--i;` |
|    1775 |  790 | `					if( PH7_MUL_OVERFLOW64(l1, l2, &iProd) ){` |
|      53 |  791 | `						r = ((ph7_real)l1 * (ph7_real)l2) * pow((ph7_real)l2,(ph7_real)i);` |
|      53 |  792 | `						break;` |
|       - |  793 | `					}` |
|    1723 |  794 | `					l1 = iProd;` |
|     863 |  795 | `				}else{` |
|    1703 |  796 | `					i /= 2;` |
|    1703 |  797 | `					if( PH7_MUL_OVERFLOW64(l2, l2, &iProd) ){` |
|      45 |  798 | `						r = (ph7_real)l1 * pow((ph7_real)l2 * (ph7_real)l2,(ph7_real)i);` |
|      45 |  799 | `						break;` |
|       - |  800 | `					}` |
|    1659 |  801 | `					l2 = iProd;` |
|       - |  802 | `				}` |
|    3379 |  803 | `				if( i == 0 ){` |
|     353 |  804 | `					pOut->x.iVal = l1;` |
|     353 |  805 | `					MemObjSetType(pOut, MEMOBJ_INT);` |
|     353 |  806 | `					usedInt = 1;` |
|     353 |  807 | `					break;` |
|       - |  808 | `				}` |
|       3 |  809 | `			}` |
|       - |  810 | `		}` |
|     343 |  811 | `	}` |
|     793 |  812 | `	if( !usedInt ){` |
|     201 |  813 | `		pOut->rVal = r;` |
|     201 |  814 | `		MemObjSetType(pOut, MEMOBJ_REAL);` |
|     100 |  815 | `	}` |
|       - |  816 | `#else` |
|       - |  817 | `	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().` |
|       - |  818 | `	 * Exponentiation by squaring with silent wrap on overflow, matching` |
|       - |  819 | `	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.` |
|       - |  820 | `	 * Negative exponents yield 0 since fractional results cannot be` |
|       - |  821 | `	 * represented. */` |
|       - |  822 | `	base_i = pBase->x.iVal;` |
|       - |  823 | `	exp_i  = pExp->x.iVal;` |
|       - |  824 | `	{` |
|       - |  825 | `		sxi64 result_i = 1;` |
|       - |  826 | `		sxi64 cur_base = base_i;` |
|       - |  827 | `		sxi64 cur_exp  = exp_i;` |
|       - |  828 | `		if( cur_exp < 0 ){` |
|       - |  829 | `			result_i = 0;` |
|       - |  830 | `		}else{` |
|       - |  831 | `			while( cur_exp > 0 ){` |
|       - |  832 | `				if( cur_exp & 1 ){` |
|       - |  833 | `					result_i *= cur_base;` |
|       - |  834 | `				}` |
|       - |  835 | `				cur_exp >>= 1;` |
|       - |  836 | `				if( cur_exp > 0 ){` |
|       - |  837 | `					cur_base *= cur_base;` |
|       - |  838 | `				}` |
|       - |  839 | `			}` |
|       - |  840 | `		}` |
|       - |  841 | `		pOut->x.iVal = result_i;` |
|       - |  842 | `		MemObjSetType(pOut, MEMOBJ_INT);` |
|       - |  843 | `	}` |
|       - |  844 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|     793 |  845 | `}` |
|       - |  846 | `/*` |
|       - |  847 | ` * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of` |
|       - |  848 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  849 | ` */` |
|    1068 |  850 | `PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       2 |  851 | `{` |
|       - |  852 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - |  853 | `	int rcNa;` |
|    1070 |  854 | `	const char *zArCls = "TypeError";` |
|    1070 |  855 | `	ph7_value *pTos = pState->pTos;` |
|    1070 |  856 | `	ph7_value *pStack = pState->pStack;` |
|    1070 |  857 | `	VmInstr *aInstr = pState->aInstr;` |
|    1070 |  858 | `	sxi32 pc = pState->pc;` |
|       - |  859 | `	sxi32 rc;` |
|     534 |  860 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    1070 |  861 | `	ph7_value *pNos = &pTos[-1];` |
|    1070 |  862 | `	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);` |
|       - |  863 | `	/* Operand order convention (matches DIV/SUB_STORE):` |
|       - |  864 | `	 *   POW:       base = pNos (evaluated first),   exp = pTos` |
|       - |  865 | `	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos` |
|       - |  866 | `	 */` |
|    1070 |  867 | `	ph7_value *pBase = bStore ? pTos : pNos;` |
|    1070 |  868 | `	ph7_value *pExp  = bStore ? pNos : pTos;` |
|       - |  869 | `	{` |
|       - |  870 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - |  871 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - |  872 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - |  873 | `		 * The message names the operands in SOURCE order, so it takes the same` |
|       - |  874 | ``		 * base/exponent convention as the operation: `$x **= "abc"` is`` |
|       - |  875 | ``		 * `int ** string`, the way `$x ** "abc"` is. */`` |
|       - |  876 | `		SyBlob sArMsg;` |
|    1070 |  877 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    1070 |  878 | `		rcNa = VmArithOperandStep(&(*pVm),pBase,pExp,"**",pNos,&zArCls,&sArMsg);` |
|    1070 |  879 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - |  880 | `			sxi32 rcAr;` |
|     322 |  881 | `			VmPopOperand(&pTos,1);` |
|     322 |  882 | `			PH7_MemObjRelease(pTos);` |
|     322 |  883 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     322 |  884 | `			pTos->nIdx = SXU32_HIGH;` |
|     482 |  885 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     160 |  886 | `				SyBlobLength(&sArMsg));` |
|     322 |  887 | `			SyBlobRelease(&sArMsg);` |
|     482 |  888 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     322 |  889 | `			rc = rcAr;` |
|     326 |  890 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  891 | `		}` |
|     749 |  892 | `		SyBlobRelease(&sArMsg);` |
|       - |  893 | `	}` |
|       - |  894 | `#ifdef UNTRUST` |
|       - |  895 | `	if( pNos < pStack ){` |
|       - |  896 | `		VM_EXIT_ABORT;` |
|       - |  897 | `	}` |
|       - |  898 | `#endif` |
|     749 |  899 | `	if( rcNa == PH7_ARITH_ORDINARY ){` |
|     745 |  900 | `		PH7_MemObjPow(pBase,pExp,pNos);` |
|     372 |  901 | `	}` |
|     749 |  902 | `	if( bStore ){` |
|       - |  903 | `		ph7_value *pObj;` |
|     275 |  904 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - |  905 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     274 |  906 | `		}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     273 |  907 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     273 |  908 | `			PH7_MemObjStore(pNos,pObj);` |
|     136 |  909 | `		}` |
|     137 |  910 | `	}` |
|     749 |  911 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     749 |  912 | `	VmPopOperand(&pTos,1);` |
|     749 |  913 | `	VM_EXIT_BREAK;` |
|     ! 0 |  914 | `	VM_EXIT_BREAK;` |
|     536 |  915 | `}` |
|       - |  916 |  |
|       - |  917 | `/*` |
|       - |  918 | ` * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of` |
|       - |  919 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  920 | ` */` |
|     786 |  921 | `PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 |  922 | `{` |
|     790 |  923 | `	ph7_value *pTos = pState->pTos;` |
|     790 |  924 | `	ph7_value *pStack = pState->pStack;` |
|     790 |  925 | `	VmInstr *aInstr = pState->aInstr;` |
|     790 |  926 | `	sxi32 pc = pState->pc;` |
|       - |  927 | `	sxi32 rc;` |
|     393 |  928 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     790 |  929 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  930 | `#ifdef UNTRUST` |
|       - |  931 | `	if( pNos < pStack ){` |
|       - |  932 | `		VM_EXIT_ABORT;` |
|       - |  933 | `	}` |
|       - |  934 | `#endif` |
|       - |  935 | `	/* php answers the UNORDERED comparison -- a NaN, or two arrays neither of` |
|       - |  936 | `	 * which contains the other -- with 1 whichever way round it is asked, and` |
|       - |  937 | `	 * PH7_MemObjCmp does the same, so the spaceship needs no case of its own. */` |
|     790 |  938 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|     792 |  939 | `	VM_CMP_REFUSAL_ROUTE()` |
|     782 |  940 | `	rc = (rc > 0) - (rc < 0);   /* normalize to exactly -1, 0 or 1 */` |
|     782 |  941 | `	VmPopOperand(&pTos,1);` |
|     782 |  942 | `	PH7_MemObjRelease(pTos);` |
|     782 |  943 | `	pTos->x.iVal = rc;` |
|     782 |  944 | `	MemObjSetType(pTos,MEMOBJ_INT);` |
|     782 |  945 | `	VM_EXIT_BREAK;` |
|     ! 0 |  946 | `	VM_EXIT_BREAK;` |
|     397 |  947 | `}` |
|       - |  948 |  |
|       - |  949 | `/*` |
|       - |  950 | ` * OP_GE: body moved verbatim from the OP_GE arm of` |
|       - |  951 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  952 | ` */` |
|  310943 |  953 | `PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  954 | `{` |
|  310948 |  955 | `	ph7_value *pTos = pState->pTos;` |
|  310948 |  956 | `	ph7_value *pStack = pState->pStack;` |
|  310948 |  957 | `	VmInstr *aInstr = pState->aInstr;` |
|  310948 |  958 | `	sxi32 pc = pState->pc;` |
|       - |  959 | `	sxi32 rc;` |
|  155794 |  960 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  310948 |  961 | `	ph7_value *pNos = &pTos[-1];` |
|       - |  962 | `	/* Perform the comparison and act accordingly */` |
|       - |  963 | `#ifdef UNTRUST` |
|       - |  964 | `	if( pNos < pStack ){` |
|       - |  965 | `		VM_EXIT_ABORT;` |
|       - |  966 | `	}` |
|       - |  967 | `#endif` |
|       - |  968 | ``	/* `$a > $b` is php's `$b < $a` -- asked from the OTHER SIDE, not read off`` |
|       - |  969 | `	 * this side's sign. The two differ exactly where the comparison is` |
|       - |  970 | `	 * UNORDERED and php answers 1 both ways (a NaN against a number or a` |
|       - |  971 | `	 * string; two same-sized arrays neither of which contains the other): a` |
|       - |  972 | ``	 * greater-than read off `rc > 0` calls both of those TRUE, where php --`` |
|       - |  973 | ``	 * asking `$b < $a` and getting 1 again -- calls them false, as it does`` |
|       - |  974 | `	 * every other relational operator on such a pair. */` |
|  310948 |  975 | `	rc = PH7_MemObjCmp(pTos,pNos,FALSE,0);` |
|  310948 |  976 | `	VM_CMP_REFUSAL_ROUTE()` |
|  310948 |  977 | `	if( pInstr->iOp == PH7_OP_GE ){` |
|  305745 |  978 | `		rc = rc <= 0;` |
|  153200 |  979 | `	}else{` |
|    5208 |  980 | `		rc = rc < 0;` |
|       - |  981 | `	}` |
|  310948 |  982 | `	VmPopOperand(&pTos,1);` |
|  310948 |  983 | `	if( !pInstr->iP2 ){` |
|       - |  984 | `		/* Push comparison result without taking the jump */` |
|  310948 |  985 | `		PH7_MemObjRelease(pTos);` |
|  310948 |  986 | `		pTos->x.iVal = rc;` |
|       - |  987 | `		/* Invalidate any prior representation */` |
|  310948 |  988 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  155799 |  989 | `	}else{` |
|     ! 0 |  990 | `		if( rc ){` |
|       - |  991 | `			/* Jump to the desired location */` |
|     ! 0 |  992 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 |  993 | `			VmPopOperand(&pTos,1);` |
|     ! 0 |  994 | `		}` |
|       - |  995 | `	}` |
|  310948 |  996 | `	VM_EXIT_BREAK;` |
|     ! 0 |  997 | `	VM_EXIT_BREAK;` |
|  155799 |  998 | `}` |
|       - |  999 |  |
|       - | 1000 | `/*` |
|       - | 1001 | ` * OP_LE: body moved verbatim from the OP_LE arm of` |
|       - | 1002 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1003 | ` */` |
| 1094684 | 1004 | `PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1005 | `{` |
| 1094689 | 1006 | `	ph7_value *pTos = pState->pTos;` |
| 1094689 | 1007 | `	ph7_value *pStack = pState->pStack;` |
| 1094689 | 1008 | `	VmInstr *aInstr = pState->aInstr;` |
| 1094689 | 1009 | `	sxi32 pc = pState->pc;` |
|       - | 1010 | `	sxi32 rc;` |
|  548582 | 1011 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1094689 | 1012 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1013 | `	/* Perform the comparison and act accordingly */` |
|       - | 1014 | `#ifdef UNTRUST` |
|       - | 1015 | `	if( pNos < pStack ){` |
|       - | 1016 | `		VM_EXIT_ABORT;` |
|       - | 1017 | `	}` |
|       - | 1018 | `#endif` |
|       - | 1019 | `	/* An unordered pair answers 1 here too, so both spellings are false for it` |
|       - | 1020 | `	 * without a case of their own (see OP_GT/OP_GE above). */` |
| 1094689 | 1021 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
| 1094689 | 1022 | `	VM_CMP_REFUSAL_ROUTE()` |
| 1094685 | 1023 | `	if( pInstr->iOp == PH7_OP_LE ){` |
|   92176 | 1024 | `		rc = rc < 1;` |
|   46410 | 1025 | `	}else{` |
| 1002514 | 1026 | `		rc = rc < 0;` |
|       - | 1027 | `	}` |
| 1094685 | 1028 | `	VmPopOperand(&pTos,1);` |
| 1094685 | 1029 | `	if( !pInstr->iP2 ){` |
|       - | 1030 | `		/* Push comparison result without taking the jump */` |
| 1094685 | 1031 | `		PH7_MemObjRelease(pTos);` |
| 1094685 | 1032 | `		pTos->x.iVal = rc;` |
|       - | 1033 | `		/* Invalidate any prior representation */` |
| 1094685 | 1034 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  548585 | 1035 | `	}else{` |
|     ! 0 | 1036 | `		if( rc ){` |
|       - | 1037 | `			/* Jump to the desired location */` |
|     ! 0 | 1038 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1039 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1040 | `		}` |
|       - | 1041 | `	}` |
| 1094685 | 1042 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1043 | `	VM_EXIT_BREAK;` |
|  548587 | 1044 | `}` |
|       - | 1045 |  |
|       - | 1046 | `/*` |
|       - | 1047 | ` * OP_TNE: body moved verbatim from the OP_TNE arm of` |
|       - | 1048 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1049 | ` */` |
| 1212671 | 1050 | `PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1051 | `{` |
| 1212676 | 1052 | `	ph7_value *pTos = pState->pTos;` |
| 1212676 | 1053 | `	ph7_value *pStack = pState->pStack;` |
| 1212676 | 1054 | `	VmInstr *aInstr = pState->aInstr;` |
| 1212676 | 1055 | `	sxi32 pc = pState->pc;` |
|       - | 1056 | `	sxi32 rc;` |
|  606631 | 1057 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1212676 | 1058 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1059 | `	/* Perform the comparison and act accordingly */` |
|       - | 1060 | `#ifdef UNTRUST` |
|       - | 1061 | `	if( pNos < pStack ){` |
|       - | 1062 | `		VM_EXIT_ABORT;` |
|       - | 1063 | `	}` |
|       - | 1064 | `#endif` |
| 1212676 | 1065 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
| 1212676 | 1066 | `	rc = rc != 0;` |
| 1212676 | 1067 | `	VmPopOperand(&pTos,1);` |
| 1212676 | 1068 | `	if( !pInstr->iP2 ){` |
|       - | 1069 | `		/* Push comparison result without taking the jump */` |
| 1212676 | 1070 | `		PH7_MemObjRelease(pTos);` |
| 1212676 | 1071 | `		pTos->x.iVal = rc;` |
|       - | 1072 | `		/* Invalidate any prior representation */` |
| 1212676 | 1073 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  606636 | 1074 | `	}else{` |
|     ! 0 | 1075 | `		if( rc ){` |
|       - | 1076 | `			/* Jump to the desired location */` |
|     ! 0 | 1077 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1078 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1079 | `		}` |
|       - | 1080 | `	}` |
| 1212676 | 1081 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1082 | `	VM_EXIT_BREAK;` |
|       5 | 1083 | `}` |
|       - | 1084 |  |
|       - | 1085 | `/*` |
|       - | 1086 | ` * OP_TEQ: body moved verbatim from the OP_TEQ arm of` |
|       - | 1087 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1088 | ` */` |
|  935116 | 1089 | `PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1090 | `{` |
|  935121 | 1091 | `	ph7_value *pTos = pState->pTos;` |
|  935121 | 1092 | `	ph7_value *pStack = pState->pStack;` |
|  935121 | 1093 | `	VmInstr *aInstr = pState->aInstr;` |
|  935121 | 1094 | `	sxi32 pc = pState->pc;` |
|       - | 1095 | `	sxi32 rc;` |
|  468236 | 1096 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  935121 | 1097 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1098 | `	/* Perform the comparison and act accordingly */` |
|       - | 1099 | `#ifdef UNTRUST` |
|       - | 1100 | `	if( pNos < pStack ){` |
|       - | 1101 | `		VM_EXIT_ABORT;` |
|       - | 1102 | `	}` |
|       - | 1103 | `#endif` |
|       - | 1104 | ``	/* `NAN === NAN` is false in php, and the comparator says so: an unordered`` |
|       - | 1105 | `	 * pair is 1, never 0. */` |
|  935121 | 1106 | `	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);` |
|  935121 | 1107 | `	rc = rc == 0;` |
|  935121 | 1108 | `	VmPopOperand(&pTos,1);` |
|  935121 | 1109 | `	if( !pInstr->iP2 ){` |
|       - | 1110 | `		/* Push comparison result without taking the jump */` |
|  935121 | 1111 | `		PH7_MemObjRelease(pTos);` |
|  935121 | 1112 | `		pTos->x.iVal = rc;` |
|       - | 1113 | `		/* Invalidate any prior representation */` |
|  935121 | 1114 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  468241 | 1115 | `	}else{` |
|     ! 0 | 1116 | `		if( rc ){` |
|       - | 1117 | `			/* Jump to the desired location */` |
|     ! 0 | 1118 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1119 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1120 | `		}` |
|       - | 1121 | `	}` |
|  935121 | 1122 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1123 | `	VM_EXIT_BREAK;` |
|       5 | 1124 | `}` |
|       - | 1125 |  |
|       - | 1126 | `/*` |
|       - | 1127 | ` * OP_NEQ: body moved verbatim from the OP_NEQ arm of` |
|       - | 1128 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1129 | ` */` |
|   16789 | 1130 | `PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1131 | `{` |
|   16794 | 1132 | `	ph7_value *pTos = pState->pTos;` |
|   16794 | 1133 | `	ph7_value *pStack = pState->pStack;` |
|   16794 | 1134 | `	VmInstr *aInstr = pState->aInstr;` |
|   16794 | 1135 | `	sxi32 pc = pState->pc;` |
|       - | 1136 | `	sxi32 rc;` |
|    8393 | 1137 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   16794 | 1138 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1139 | `	/* Perform the comparison and act accordingly */` |
|       - | 1140 | `#ifdef UNTRUST` |
|       - | 1141 | `	if( pNos < pStack ){` |
|       - | 1142 | `		VM_EXIT_ABORT;` |
|       - | 1143 | `	}` |
|       - | 1144 | `#endif` |
|   16794 | 1145 | `	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);` |
|   16796 | 1146 | `	VM_CMP_REFUSAL_ROUTE()` |
|   16774 | 1147 | `	if( pInstr->iOp == PH7_OP_EQ ){` |
|   14212 | 1148 | `		rc = rc == 0;` |
|    7108 | 1149 | `	}else{` |
|    2567 | 1150 | `		rc = rc != 0;` |
|       - | 1151 | `	}` |
|   16774 | 1152 | `	VmPopOperand(&pTos,1);` |
|   16774 | 1153 | `	if( !pInstr->iP2 ){` |
|       - | 1154 | `		/* Push comparison result without taking the jump */` |
|   16774 | 1155 | `		PH7_MemObjRelease(pTos);` |
|   16774 | 1156 | `		pTos->x.iVal = rc;` |
|       - | 1157 | `		/* Invalidate any prior representation */` |
|   16774 | 1158 | `		MemObjSetType(pTos,MEMOBJ_BOOL);` |
|    8388 | 1159 | `	}else{` |
|     ! 0 | 1160 | `		if( rc ){` |
|       - | 1161 | `			/* Jump to the desired location */` |
|     ! 0 | 1162 | `			pc = pInstr->iP2 - 1;` |
|     ! 0 | 1163 | `			VmPopOperand(&pTos,1);` |
|     ! 0 | 1164 | `		}` |
|       - | 1165 | `	}` |
|   16774 | 1166 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1167 | `	VM_EXIT_BREAK;` |
|    8398 | 1168 | `}` |
|       - | 1169 |  |
|       - | 1170 | `/*` |
|       - | 1171 | ` * OP_LOR: body moved verbatim from the OP_LOR arm of` |
|       - | 1172 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1173 | ` */` |
|  394428 | 1174 | `PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1175 | `{` |
|  394433 | 1176 | `	ph7_value *pTos = pState->pTos;` |
|  394433 | 1177 | `	ph7_value *pStack = pState->pStack;` |
|  394433 | 1178 | `	VmInstr *aInstr = pState->aInstr;` |
|  394433 | 1179 | `	sxi32 pc = pState->pc;` |
|       - | 1180 | `	sxi32 rc;` |
|  197516 | 1181 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  394433 | 1182 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1183 | `	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */` |
|       - | 1184 | `#ifdef UNTRUST` |
|       - | 1185 | `	if( pNos < pStack ){` |
|       - | 1186 | `		VM_EXIT_ABORT;` |
|       - | 1187 | `	}` |
|       - | 1188 | `#endif` |
|       - | 1189 | `	/* Force a boolean cast */` |
|  394433 | 1190 | `	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|      10 | 1191 | `		PH7_MemObjToBool(pTos);` |
|       4 | 1192 | `	}` |
|  394433 | 1193 | `	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){` |
|     ! 0 | 1194 | `		PH7_MemObjToBool(pNos);` |
|     ! 0 | 1195 | `	}` |
|  394433 | 1196 | `	v1 = pNos->x.iVal == 0 ? 1 : 0;` |
|  394433 | 1197 | `	v2 = pTos->x.iVal == 0 ? 1 : 0;` |
|  394433 | 1198 | `	if( pInstr->iOp == PH7_OP_LAND ){` |
|       - | 1199 | `		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };` |
|   79438 | 1200 | `		v1 = and_logic[v1*3+v2];` |
|   39712 | 1201 | `	}else{` |
|       - | 1202 | `		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };` |
|  315000 | 1203 | `		v1 = or_logic[v1*3+v2];` |
|       - | 1204 | `	}` |
|  394433 | 1205 | `	if( v1 == 2 ){` |
|     ! 0 | 1206 | `		v1 = 1;` |
|     ! 0 | 1207 | `	}` |
|  394433 | 1208 | `	VmPopOperand(&pTos,1);` |
|  394433 | 1209 | `	pTos->x.iVal = v1 == 0 ? 1 : 0;` |
|  394433 | 1210 | `	MemObjSetType(pTos,MEMOBJ_BOOL);` |
|  394433 | 1211 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1212 | `	VM_EXIT_BREAK;` |
|       5 | 1213 | `}` |
|       - | 1214 |  |
|       - | 1215 | `/*` |
|       - | 1216 | ` * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of` |
|       - | 1217 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1218 | ` */` |
|     824 | 1219 | `PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 | 1220 | `{` |
|     825 | 1221 | `	ph7_value *pTos = pState->pTos;` |
|     825 | 1222 | `	ph7_value *pStack = pState->pStack;` |
|     825 | 1223 | `	VmInstr *aInstr = pState->aInstr;` |
|     825 | 1224 | `	sxi32 pc = pState->pc;` |
|       - | 1225 | `	sxi32 rc;` |
|     412 | 1226 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     825 | 1227 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1228 | `	ph7_value *pObj;` |
|       - | 1229 | `	sxi64 a,r;` |
|       - | 1230 | `#ifdef UNTRUST` |
|       - | 1231 | `	if( pNos < pStack ){` |
|       - | 1232 | `		VM_EXIT_ABORT;` |
|       - | 1233 | `	}` |
|       - | 1234 | `#endif` |
|       - | 1235 | `	/* (The string-offset lvalue rejection happens in the dispatch arm that calls` |
|       - | 1236 | `	 * this handler, beside the other eleven compound stores.) */` |
|     978 | 1237 | `	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)` |
|       - | 1238 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|     519 | 1239 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|     519 | 1240 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1241 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|     519 | 1242 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|     519 | 1243 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     223 | 1244 | `		PH7_MemObjToInteger(pTos);` |
|     111 | 1245 | `	}` |
|     519 | 1246 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     221 | 1247 | `		PH7_MemObjToInteger(pNos);` |
|     110 | 1248 | `	}` |
|       - | 1249 | `	/* Perform the requested operation */` |
|     519 | 1250 | `	a = pTos->x.iVal;` |
|     519 | 1251 | `	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)` |
|       - | 1252 | `	/* Push the result */` |
|     471 | 1253 | `	pNos->x.iVal = r;` |
|     471 | 1254 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|     471 | 1255 | `	if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1256 | `		/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     470 | 1257 | `	}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     469 | 1258 | `		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     469 | 1259 | `		PH7_MemObjStore(pNos,pObj);` |
|     234 | 1260 | `	}` |
|     471 | 1261 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|     471 | 1262 | `	VmPopOperand(&pTos,1);` |
|     471 | 1263 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1264 | `	VM_EXIT_BREAK;` |
|     413 | 1265 | `}` |
|       - | 1266 |  |
|       - | 1267 | `/*` |
|       - | 1268 | ` * OP_SHR: body moved verbatim from the OP_SHR arm of` |
|       - | 1269 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1270 | ` */` |
|    3359 | 1271 | `PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 | 1272 | `{` |
|    3362 | 1273 | `	ph7_value *pTos = pState->pTos;` |
|    3362 | 1274 | `	ph7_value *pStack = pState->pStack;` |
|    3362 | 1275 | `	VmInstr *aInstr = pState->aInstr;` |
|    3362 | 1276 | `	sxi32 pc = pState->pc;` |
|       - | 1277 | `	sxi32 rc;` |
|    1679 | 1278 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    3362 | 1279 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1280 | `	sxi64 a,r;` |
|       - | 1281 | `#ifdef UNTRUST` |
|       - | 1282 | `	if( pNos < pStack ){` |
|       - | 1283 | `		VM_EXIT_ABORT;` |
|       - | 1284 | `	}` |
|       - | 1285 | `#endif` |
|    3529 | 1286 | `	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)` |
|       - | 1287 | `	/* Force the operands to be integer (php deprecates a lossy float here) */` |
|    3028 | 1288 | `	rc = VmRejectFloatOperand(&(*pVm),pNos);` |
|    3028 | 1289 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    3024 | 1290 | `	rc = VmRejectFloatOperand(&(*pVm),pTos);` |
|    3024 | 1291 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
|    3024 | 1292 | `	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1293 | `		PH7_MemObjToInteger(pTos);` |
|     118 | 1294 | `	}` |
|    3024 | 1295 | `	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){` |
|     237 | 1296 | `		PH7_MemObjToInteger(pNos);` |
|     118 | 1297 | `	}` |
|       - | 1298 | `	/* Perform the requested operation */` |
|    3024 | 1299 | `	a = pNos->x.iVal;` |
|    3024 | 1300 | `	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)` |
|       - | 1301 | `	/* Push the result */` |
|    2970 | 1302 | `	pNos->x.iVal = r;` |
|    2970 | 1303 | `	MemObjSetType(pNos,MEMOBJ_INT);` |
|    2970 | 1304 | `	VmPopOperand(&pTos,1);` |
|    2970 | 1305 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1306 | `	VM_EXIT_BREAK;` |
|    1682 | 1307 | `}` |
|       - | 1308 |  |
|       - | 1309 | `/*` |
|       - | 1310 | ` * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of` |
|       - | 1311 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1312 | ` */` |
|    7398 | 1313 | `PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1314 | `{` |
|       - | 1315 | `	/* php's do_operation: an operand whose class declares one decides the pair. */` |
|       - | 1316 | `	int rcNa;` |
|    7403 | 1317 | `	const char *zArCls = "TypeError";` |
|    7403 | 1318 | `	ph7_value *pTos = pState->pTos;` |
|    7403 | 1319 | `	ph7_value *pStack = pState->pStack;` |
|    7403 | 1320 | `	VmInstr *aInstr = pState->aInstr;` |
|    7403 | 1321 | `	sxi32 pc = pState->pc;` |
|       - | 1322 | `	sxi32 rc;` |
|    3692 | 1323 | `	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    7403 | 1324 | `	ph7_value *pNos = &pTos[-1];` |
|       - | 1325 | `	{` |
|       - | 1326 | `		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,` |
|       - | 1327 | `		 * object or resource operand is a TypeError, not a silent 0. Settle the stack` |
|       - | 1328 | `		 * BEFORE throwing, so the catch does not run over the abandoned operands.` |
|       - | 1329 | `		 * MULTIPLICATION is commutative and the RESULT does not care which operand is` |
|       - | 1330 | `		 * which, but the MESSAGE does: it names them in SOURCE order, and a compound` |
|       - | 1331 | ``		 * assign puts its lvalue on the TOP of the stack (`$x *= [1]` is `int * array`,`` |
|       - | 1332 | ``		 * the way `$x * [1]` is). */`` |
|    7403 | 1333 | `		ph7_value *pMulL = (pInstr->iOp == PH7_OP_MUL_STORE) ? pTos : pNos;` |
|    7403 | 1334 | `		ph7_value *pMulR = (pInstr->iOp == PH7_OP_MUL_STORE) ? pNos : pTos;` |
|       - | 1335 | `		SyBlob sArMsg;` |
|    7403 | 1336 | `		SyBlobInit(&sArMsg,&pVm->sAllocator);` |
|    7403 | 1337 | `		rcNa = VmArithOperandStep(&(*pVm),pMulL,pMulR,"*",pNos,&zArCls,&sArMsg);` |
|    7403 | 1338 | `		if( rcNa == PH7_ARITH_REFUSED ){` |
|       - | 1339 | `			sxi32 rcAr;` |
|     316 | 1340 | `			VmPopOperand(&pTos,1);` |
|     316 | 1341 | `			PH7_MemObjRelease(pTos);` |
|     316 | 1342 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|     316 | 1343 | `			pTos->nIdx = SXU32_HIGH;` |
|     473 | 1344 | `			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),` |
|     157 | 1345 | `				SyBlobLength(&sArMsg));` |
|     316 | 1346 | `			SyBlobRelease(&sArMsg);` |
|     473 | 1347 | `			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     316 | 1348 | `			rc = rcAr;` |
|     318 | 1349 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1350 | `		}` |
|    7089 | 1351 | `		SyBlobRelease(&sArMsg);` |
|       - | 1352 | `	}` |
|    7089 | 1353 | `	if( rcNa == PH7_ARITH_HANDLED ){` |
|       5 | 1354 | `		goto mul_store_write;` |
|       - | 1355 | `	}` |
|       - | 1356 | `	/* Force the operand to be numeric */` |
|       - | 1357 | `#ifdef UNTRUST` |
|       - | 1358 | `	if( pNos < pStack ){` |
|       - | 1359 | `		VM_EXIT_ABORT;` |
|       - | 1360 | `	}` |
|       - | 1361 | `#endif` |
|    7085 | 1362 | `	PH7_MemObjToNumeric(pTos);` |
|    7085 | 1363 | `	PH7_MemObjToNumeric(pNos);` |
|       - | 1364 | `	/* Perform the requested operation */` |
|   10618 | 1365 | `	if( MEMOBJ_REAL & (pTos->iFlags\|pNos->iFlags) ){` |
|       - | 1366 | `		/* Floating point arithemic */` |
|       - | 1367 | `		ph7_real a,b,r;` |
|      36 | 1368 | `		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){` |
|      26 | 1369 | `			PH7_MemObjToReal(pTos);` |
|      12 | 1370 | `		}` |
|      36 | 1371 | `		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){` |
|       5 | 1372 | `			PH7_MemObjToReal(pNos);` |
|       2 | 1373 | `		}` |
|      36 | 1374 | `		a = pNos->rVal;` |
|      36 | 1375 | `		b = pTos->rVal;` |
|      36 | 1376 | `		r = a * b;` |
|       - | 1377 | `		/* Push the result */` |
|      36 | 1378 | `		pNos->rVal = r;` |
|      36 | 1379 | `		MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1380 | `		/* Try to get an integer representation */` |
|      36 | 1381 | `		PH7_MemObjTryInteger(pNos);` |
|      19 | 1382 | `	}else{` |
|       - | 1383 | `		/* Integer arithmetic; PHP promotes an overflowing product to float.` |
|       - | 1384 | `		 * The integer-only build wraps like OP_POW's OMIT path. */` |
|       - | 1385 | `		sxi64 a,b,r;` |
|    7051 | 1386 | `		a = pNos->x.iVal;` |
|    7051 | 1387 | `		b = pTos->x.iVal;` |
|    7051 | 1388 | `		if( PH7_MUL_OVERFLOW64(a,b,&r) ){` |
|       - | 1389 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      57 | 1390 | `			pNos->rVal = (ph7_real)a * (ph7_real)b;` |
|      57 | 1391 | `			MemObjSetType(pNos,MEMOBJ_REAL);` |
|       - | 1392 | `#else` |
|       - | 1393 | `			pNos->x.iVal = r;` |
|       - | 1394 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1395 | `#endif` |
|      29 | 1396 | `		}else{` |
|    6995 | 1397 | `			pNos->x.iVal = r;` |
|    6995 | 1398 | `			MemObjSetType(pNos,MEMOBJ_INT);` |
|       - | 1399 | `		}` |
|       - | 1400 | `	}` |
|    3549 | 1401 | `mul_store_write:` |
|    7089 | 1402 | `	if( pInstr->iOp == PH7_OP_MUL_STORE ){` |
|       - | 1403 | `		ph7_value *pObj;` |
|     335 | 1404 | `		if( pTos->nIdx == SXU32_HIGH ){` |
|       - | 1405 | `			/* A read-modify-write THROUGH a temporary: php drops it in silence. */` |
|     334 | 1406 | `		}else if( (pObj = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|     333 | 1407 | `			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);` |
|     333 | 1408 | `			PH7_MemObjStore(pNos,pObj);` |
|     165 | 1409 | `		}` |
|     166 | 1410 | `	}` |
|    7089 | 1411 | `	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);` |
|    7089 | 1412 | `	VmPopOperand(&pTos,1);` |
|    7089 | 1413 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1414 | `	VM_EXIT_BREAK;` |
|    3697 | 1415 | `}` |
|       - | 1416 |  |
