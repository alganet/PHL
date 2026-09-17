# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 743/849 lines (87.51%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   21 | `#include "vm_dispatch.h"` |
|      - |   22 | `#define sState (*pState)` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * OP_STORE_REF: body moved verbatim from the OP_STORE_REF arm of` |
|      - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   27 | ` */` |
|     52 |   28 | `PH7_PRIVATE VmOpRc VmExecOpStoreRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   29 | `{` |
|     55 |   30 | `	ph7_value *pTos = pState->pTos;` |
|     55 |   31 | `	ph7_value *pStack = pState->pStack;` |
|     55 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|     55 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|     26 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     55 |   36 | `	 SyString sName = { 0 , 0 };` |
|      - |   37 | `	 VmFrame *pFrameLocal;` |
|      - |   38 | `	SyHashEntry *pEntry;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pTos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     55 |   45 | `	if( pInstr->iP2 == 1 ){` |
|      - |   46 | ``		/* Member reference target: `$o->p =& $x` / `self::$s =& $x`. The`` |
|      - |   47 | `		 * preceding OP_MEMBER (PH7_MEMBER_REF_TARGET) resolved the property slot` |
|      - |   48 | `		 * and stashed it. Stack: [ ... , source, member-result(top) ]. Rebind the` |
|      - |   49 | `		 * property's nIdx to alias the source variable's slot and pin that slot` |
|      - |   50 | `		 * past its owning frame (like a use(&$x) capture) so neither frame` |
|      - |   51 | `		 * teardown nor a later unset recycles it while the property aliases it. */` |
|     11 |   52 | `		ph7_value *pSrc = &pTos[-1];` |
|     11 |   53 | `		sxu32 nSrcIdx = pSrc->nIdx;` |
|     11 |   54 | `		VmClassAttr *pVmAttr = pVm->pRefTargetAttr;` |
|     11 |   55 | `		ph7_class_attr *pStAttr = pVm->pRefTargetStaticAttr;` |
|     11 |   56 | `		if( nSrcIdx == SXU32_HIGH ){` |
|      - |   57 | ``			/* php: the RHS of `=&` must be a variable, not a constant expression.`` |
|      - |   58 | `			 * (The compiler already rejects the obvious literal forms.) */` |
|    ! 0 |   59 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |   60 | `				"Reference operator require a variable not a constant as it's right operand");` |
|     11 |   61 | `		}else if( pVmAttr ){` |
|      9 |   62 | `			sxu32 nOldIdx = pVmAttr->nIdx;` |
|      9 |   63 | `			if( nOldIdx != nSrcIdx ){` |
|      9 |   64 | `				if( (pVmAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|      - |   65 | `					/* Release this property's own (unshared) slot before repointing.` |
|      - |   66 | `					 * A reference-bound property bypasses typed coercion in php, so` |
|      - |   67 | `					 * drop any typed-slot enforcement entry too. */` |
|      9 |   68 | `					if( pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      5 |   69 | `						SyHashDeleteEntry(&pVm->hTypedSlot,(const void *)&nOldIdx,sizeof(sxu32),0);` |
|      2 |   70 | `					}` |
|      9 |   71 | `					PH7_VmUnsetMemObj(&(*pVm),nOldIdx,TRUE);` |
|      4 |   72 | `				}` |
|      9 |   73 | `				pVmAttr->nIdx = nSrcIdx;` |
|      9 |   74 | `				pVmAttr->iState \|= VM_CLASS_ATTR_REFBOUND;` |
|      9 |   75 | `				pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      9 |   76 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      5 |   77 | `			}` |
|      7 |   78 | `		}else if( pStAttr ){` |
|      3 |   79 | `			if( pStAttr->nIdx != nSrcIdx ){` |
|      3 |   80 | `				pStAttr->nIdx = nSrcIdx;` |
|      3 |   81 | `				VmPinMemObjSlot(&(*pVm),nSrcIdx);` |
|      1 |   82 | `			}` |
|      1 |   83 | `		}` |
|     11 |   84 | `		if( pVm->pRefTargetThis ){` |
|      9 |   85 | `			PH7_ClassInstanceUnref(pVm->pRefTargetThis);` |
|      4 |   86 | `		}` |
|     11 |   87 | `		pVm->pRefTargetAttr = 0;` |
|     11 |   88 | `		pVm->pRefTargetStaticAttr = 0;` |
|     11 |   89 | `		pVm->pRefTargetThis = 0;` |
|      - |   90 | `		/* Pop the member-result; leave the source as the expression value. */` |
|     11 |   91 | `		VmPopOperand(&pTos,1);` |
|     11 |   92 | `		VM_EXIT_BREAK;` |
|      - |   93 | `	}` |
|     45 |   94 | `	if( pInstr->p3 == 0 ){` |
|      - |   95 | `		char *zName;` |
|      - |   96 | `		/* Take the variable name from the Next on the stack */` |
|    ! 0 |   97 | `		if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|      - |   98 | `			/* Force a string cast */` |
|    ! 0 |   99 | `			PH7_MemObjToString(pTos);` |
|    ! 0 |  100 | `		}` |
|    ! 0 |  101 | `		if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    ! 0 |  102 | `			zName = SyMemBackendStrDup(&pVm->sAllocator,` |
|    ! 0 |  103 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  104 | `			if( zName ){` |
|    ! 0 |  105 | `				SyStringInitFromBuf(&sName,zName,SyBlobLength(&pTos->sBlob));` |
|    ! 0 |  106 | `			}` |
|    ! 0 |  107 | `		}` |
|    ! 0 |  108 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  109 | `		pTos--;` |
|    ! 0 |  110 | `	}else{` |
|     45 |  111 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|      - |  112 | `	}` |
|     45 |  113 | `	nIdx = pTos->nIdx;` |
|     45 |  114 | `	if(nIdx == SXU32_HIGH ){` |
|    ! 0 |  115 | `		if( (pTos->iFlags & (MEMOBJ_OBJ\|MEMOBJ_HASHMAP\|MEMOBJ_RES)) == 0 ){` |
|    ! 0 |  116 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  117 | `				"Reference operator require a variable not a constant as it's right operand");` |
|    ! 0 |  118 | `		}else{` |
|      - |  119 | `			ph7_value *pObj;` |
|      - |  120 | `			/* Extract the desired variable and if not available dynamically create it */` |
|    ! 0 |  121 | `			pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);` |
|    ! 0 |  122 | `			if( pObj == 0 ){` |
|    ! 0 |  123 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|      - |  124 | `					"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);` |
|    ! 0 |  125 | `				VM_EXIT_ABORT;` |
|      - |  126 | `			}` |
|      - |  127 | `			/* Perform the store operation */` |
|    ! 0 |  128 | `			PH7_MemObjStore(pTos,pObj);` |
|    ! 0 |  129 | `			pTos->nIdx = pObj->nIdx;` |
|    ! 0 |  130 | `		}` |
|     45 |  131 | `	}else if( sName.nByte > 0){` |
|     45 |  132 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
|      - |  133 | `			/* php 8.1's non-catchable fatal (compile-time there) */` |
|      3 |  134 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot acquire reference to $GLOBALS");` |
|      3 |  135 | `			pVm->iExitStatus = 255;` |
|      3 |  136 | `			pVm->bHaltRequested = 1;` |
|      3 |  137 | `			VM_EXIT_ABORT;` |
|    ! 0 |  138 | `		}else{` |
|     43 |  139 | `			pFrameLocal = pVm->pFrame;` |
|     43 |  140 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  141 | `			/* Query the local frame */` |
|     43 |  142 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|     43 |  143 | `			if( pEntry ){` |
|    ! 0 |  144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|    ! 0 |  145 | `			}else{` |
|     43 |  146 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|     43 |  147 | `				if( pFrameLocal->pParent == 0 ){` |
|      - |  148 | `					/* Insert in the $GLOBALS array */` |
|     39 |  149 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|     18 |  150 | `				}` |
|     43 |  151 | `				if( rc == SXRET_OK ){` |
|     43 |  152 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     20 |  153 | `				}` |
|      - |  154 | `			}` |
|      - |  155 | `		}` |
|     20 |  156 | `	}` |
|     43 |  157 | `	VM_EXIT_BREAK;` |
|    ! 0 |  158 | `	VM_EXIT_BREAK;` |
|     29 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/* LOAD_IDX's own iP2 context codes (they do NOT line up with PH7_MEMBER_*). */` |
|      - |  162 | `#define VM_IDX_CTX_ISSET 4` |
|      - |  163 | `#define VM_IDX_CTX_UNSET 5` |
|      - |  164 | `#define VM_IDX_CTX_EMPTY 6` |
|      - |  165 | `/*` |
|      - |  166 | ` * php rejects an OBJECT or an ARRAY used as an array offset, naming the offending` |
|      - |  167 | ` * type the way get_debug_type() does — the CLASS name for an object, "array" for` |
|      - |  168 | ` * an array — and wording the failure by context:` |
|      - |  169 | ` *` |
|      - |  170 | ` *   read/write   Cannot access offset of type Foo on array` |
|      - |  171 | ` *   isset/empty  Cannot access offset of type Foo in isset or empty` |
|      - |  172 | ` *   unset        Cannot unset offset of type Foo on array` |
|      - |  173 | ` *` |
|      - |  174 | ` * A RESOURCE is deliberately absent: php does not reject it, it warns` |
|      - |  175 | ` * ("Resource ID#N used as offset, casting to integer (N)") and uses the id as an` |
|      - |  176 | ` * integer key. VmOffsetResourceWarn() below handles that half.` |
|      - |  177 | ` *` |
|      - |  178 | ` * Returns TRUE and fills pMsg when the key must be rejected. iCtx is the` |
|      - |  179 | ` * instruction's iP2 (any value other than the isset/unset/empty codes reads as` |
|      - |  180 | ` * an access).` |
|      - |  181 | ` */` |
| 226658 |  182 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|      5 |  183 | `{` |
|      - |  184 | `	const char *zType;` |
| 226663 |  185 | `	SyString *pClass = 0;` |
| 226663 |  186 | `	if( pKey == 0 ){` |
|    ! 0 |  187 | `		return FALSE;` |
|      - |  188 | `	}` |
| 226663 |  189 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|     15 |  190 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|     15 |  191 | `		if( pInst && pInst->pClass ){` |
|     15 |  192 | `			pClass = &pInst->pClass->sName;` |
|      7 |  193 | `		}` |
|     15 |  194 | `		zType = "object";` |
| 226656 |  195 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  196 | `		zType = "array";` |
|      3 |  197 | `	}else{` |
| 226645 |  198 | `		return FALSE;` |
|      - |  199 | `	}` |
|     19 |  200 | `	SyBlobInit(pMsg,&pVm->sAllocator);` |
|     19 |  201 | `	if( iCtx == VM_IDX_CTX_UNSET ){` |
|      3 |  202 | `		SyBlobAppend(pMsg,"Cannot unset offset of type ",sizeof("Cannot unset offset of type ")-1);` |
|      2 |  203 | `	}else{` |
|     17 |  204 | `		SyBlobAppend(pMsg,"Cannot access offset of type ",sizeof("Cannot access offset of type ")-1);` |
|      - |  205 | `	}` |
|     19 |  206 | `	if( pClass ){` |
|     15 |  207 | `		SyBlobAppend(pMsg,pClass->zString,pClass->nByte);` |
|      8 |  208 | `	}else{` |
|      5 |  209 | `		SyBlobAppend(pMsg,zType,(sxu32)SyStrlen(zType));` |
|      - |  210 | `	}` |
|     19 |  211 | `	if( iCtx == VM_IDX_CTX_ISSET \|\| iCtx == VM_IDX_CTX_EMPTY ){` |
|      7 |  212 | `		SyBlobAppend(pMsg," in isset or empty",sizeof(" in isset or empty")-1);` |
|      4 |  213 | `	}else{` |
|     13 |  214 | `		SyBlobAppend(pMsg," on array",sizeof(" on array")-1);` |
|      - |  215 | `	}` |
|     19 |  216 | `	return TRUE;` |
| 113334 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|      - |  220 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|      - |  221 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|      - |  222 | ` * takes over.` |
|      - |  223 | ` */` |
| 226640 |  224 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  225 | `{` |
|      - |  226 | `	sxu32 nId;` |
| 226645 |  227 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
| 226641 |  228 | `		return;` |
|      - |  229 | `	}` |
|      5 |  230 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      7 |  231 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      2 |  232 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      5 |  233 | `	PH7_MemObjRelease(pKey);` |
|      5 |  234 | `	pKey->x.iVal = (sxi64)nId;` |
|      5 |  235 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
| 113325 |  236 | `}` |
|      - |  237 | `/*` |
|      - |  238 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  239 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  240 | ` */` |
| 242150 |  241 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  242 | `{` |
| 242155 |  243 | `	ph7_value *pTos = pState->pTos;` |
| 242155 |  244 | `	ph7_value *pStack = pState->pStack;` |
| 242155 |  245 | `	VmInstr *aInstr = pState->aInstr;` |
| 242155 |  246 | `	sxi32 pc = pState->pc;` |
|      - |  247 | `	sxi32 rc;` |
| 121075 |  248 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 242155 |  249 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  250 | `	ph7_value *pKey;` |
|      - |  251 | `	sxu32 nIdx;` |
| 242155 |  252 | `	if( pInstr->iP1 ){` |
|      - |  253 | `		/* Key is next on stack */` |
|  69445 |  254 | `		pKey = pTos;` |
|  69445 |  255 | `		pTos--;` |
|  34725 |  256 | `	}else{` |
| 172715 |  257 | `		pKey = 0;` |
|      - |  258 | `	}` |
|      - |  259 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  260 | `		 * normalizes to ""); PHL rejects it. */` |
| 242155 |  261 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69247 |  262 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103871 |  263 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69242 |  264 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|      - |  265 | `			SyBlob sTypeMsg;` |
|      - |  266 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|      - |  267 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  69247 |  268 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|      - |  269 | `				sxi32 rcSc;` |
|      5 |  270 | `				PH7_MemObjRelease(pKey);` |
|      5 |  271 | `				VmPopOperand(&pTos,1);` |
|      5 |  272 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      9 |  273 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  274 | `				rc = rcSc;` |
|      5 |  275 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  276 | `			}` |
|  69243 |  277 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|  69243 |  278 | `			if( bNull \|\| bLossyFloat ){` |
|      - |  279 | `				sxi32 rcSc;` |
|      5 |  280 | `				const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 |  281 | `				                         : "Cannot access offset of type float on array";` |
|      5 |  282 | `				PH7_MemObjRelease(pKey);` |
|      5 |  283 | `				VmPopOperand(&pTos,1);` |
|      5 |  284 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      5 |  285 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  286 | `				rc = rcSc;` |
|      5 |  287 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  288 | `			}` |
|  34617 |  289 | `		}` |
| 242147 |  290 | `	nIdx = pTos->nIdx;` |
|      - |  291 | `	{` |
|      - |  292 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  293 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  294 | `		 * the backing variable slot at nIdx. */` |
| 242147 |  295 | `		ph7_class_instance *pInst = 0;` |
| 242147 |  296 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  297 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 242104 |  298 | `		}else if( nIdx != SXU32_HIGH ){` |
| 242061 |  299 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 242061 |  300 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  301 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  302 | `			}` |
| 121028 |  303 | `		}` |
| 242147 |  304 | `		if( pInst ){` |
|     89 |  305 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  306 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  307 | `				ph7_class_method *pMeth;` |
|      - |  308 | `				ph7_value sNullKey;` |
|      - |  309 | `				ph7_value *apArg[2];` |
|     87 |  310 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  311 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  312 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  313 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  314 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  315 | `					VM_EXIT_BREAK;` |
|      - |  316 | `				}` |
|     87 |  317 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  318 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  319 | `				/* Pop container; pTos now points to the value */` |
|     87 |  320 | `				VmPopOperand(&pTos,1);` |
|     87 |  321 | `				if( pKey == 0 ){` |
|     10 |  322 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  323 | `					apArg[0] = &sNullKey;` |
|      6 |  324 | `				}else{` |
|     79 |  325 | `					apArg[0] = pKey;` |
|      - |  326 | `				}` |
|     87 |  327 | `				apArg[1] = pTos;` |
|     87 |  328 | `				if( pMeth ){` |
|     87 |  329 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  330 | `				}` |
|     87 |  331 | `				if( pKey ){` |
|     79 |  332 | `					PH7_MemObjRelease(pKey);` |
|     41 |  333 | `				}else{` |
|     10 |  334 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  335 | `				}` |
|      - |  336 | `				/* Pop the value */` |
|     87 |  337 | `				VmPopOperand(&pTos,1);` |
|     87 |  338 | `				VM_EXIT_BREAK;` |
|      - |  339 | `			}` |
|      - |  340 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  341 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  342 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  343 | `			 * a few lines below). Match PHP. */` |
|      - |  344 | `			{` |
|      - |  345 | `				char zMsg[256];` |
|      3 |  346 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  347 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  348 | `					"Cannot use object of type %.*s as array",` |
|      2 |  349 | `					(int)pName->nByte,pName->zString);` |
|      3 |  350 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  351 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  352 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  353 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  354 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  355 | `			}` |
|      - |  356 | `		}` |
|      - |  357 | `	}` |
| 242061 |  358 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  359 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  360 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  361 | `		 * checking true sharing count, then re-add after separation. */` |
| 241925 |  362 | `		if( nIdx != SXU32_HIGH ){` |
| 241925 |  363 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 362885 |  364 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 241925 |  365 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  366 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  367 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  368 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  369 | `				 * refcounts if the backing array was already separated. */` |
| 241925 |  370 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 241925 |  371 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 241925 |  372 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 241925 |  373 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 241925 |  374 | `					pTos->x.pOther = pMap;` |
| 120965 |  375 | `				}else{` |
|      - |  376 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  377 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  378 | `					pMap = pCur;` |
|      - |  379 | `				}` |
| 120965 |  380 | `			}else{` |
|    ! 0 |  381 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  382 | `			}` |
| 120965 |  383 | `		}else{` |
|    ! 0 |  384 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  385 | `		}` |
| 241925 |  386 | `		if( pMap->iRef < 2 ){` |
|      - |  387 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  388 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  389 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  390 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  391 | `			pMap->iRef = 2;` |
|    ! 0 |  392 | `		}` |
| 120965 |  393 | `	}else{` |
|      - |  394 | `		ph7_value *pObj;` |
|    139 |  395 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    139 |  396 | `		if( pObj == 0 ){` |
|    ! 0 |  397 | `			if( pKey ){` |
|    ! 0 |  398 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  399 | `			}` |
|    ! 0 |  400 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  401 | `			VM_EXIT_BREAK;` |
|      - |  402 | `		}` |
|      - |  403 | `		/* Phase#1: Load the array */` |
|    139 |  404 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  405 | `			VmPopOperand(&pTos,1);` |
|    107 |  406 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  407 | `				/* Force a string cast */` |
|    ! 0 |  408 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  409 | `			}` |
|    107 |  410 | `			if( pKey == 0 ){` |
|      - |  411 | ``				/* `$s[] = 'x'` on a STRING: php raises the catchable Error`` |
|      - |  412 | `				 * "[] operator not supported for strings" and leaves the string` |
|      - |  413 | `				 * untouched. PHL silently APPENDED, so code that meant to build` |
|      - |  414 | `				 * an array from a variable holding a string quietly produced a` |
|      - |  415 | `				 * longer string instead of failing — a wrong answer, not a` |
|      - |  416 | `				 * missing diagnostic. */` |
|      - |  417 | `				SyBlob sErrMsg;` |
|      3 |  418 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      3 |  419 | `				SyBlobAppend(&sErrMsg,"[] operator not supported for strings",` |
|      - |  420 | `					sizeof("[] operator not supported for strings")-1);` |
|      3 |  421 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      3 |  422 | `				VM_EXIT_BREAK;` |
|    ! 0 |  423 | `			}else{` |
|      - |  424 | `				sxi64 iOfft;` |
|      - |  425 | `				sxi64 nLen;` |
|    105 |  426 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  427 | `					/* Force an int cast */` |
|    ! 0 |  428 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  429 | `				}` |
|    105 |  430 | `				iOfft = pKey->x.iVal;` |
|    105 |  431 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  432 | `				if( iOfft < 0 ){` |
|      - |  433 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  434 | `					iOfft += nLen;` |
|      5 |  435 | `					if( iOfft < 0 ){` |
|      4 |  436 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  437 | `							pKey->x.iVal);` |
|      3 |  438 | `						PH7_MemObjRelease(pKey);` |
|      3 |  439 | `						VM_EXIT_BREAK;` |
|      - |  440 | `					}` |
|      1 |  441 | `				}` |
|    103 |  442 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  443 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  444 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  445 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  446 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  447 | `					}` |
|    103 |  448 | `					if( iOfft >= nLen ){` |
|      - |  449 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  450 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  451 | `						 * -- a silently wrong string. */` |
|      - |  452 | `						sxi64 nPad;` |
|      9 |  453 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  454 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  455 | `						}` |
|      3 |  456 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  457 | `					}else{` |
|    101 |  458 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  459 | `						zData[iOfft] = zBlob[0];` |
|      - |  460 | `					}` |
|     51 |  461 | `				}` |
|      - |  462 | `			}` |
|    103 |  463 | `			if( pKey ){` |
|    103 |  464 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  465 | `			}` |
|    103 |  466 | `			VM_EXIT_BREAK;` |
|     33 |  467 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  468 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  469 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  470 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  471 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  472 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     33 |  473 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     33 |  474 | `			if( bScalar ){` |
|      - |  475 | `				sxi32 rcSc;` |
|      8 |  476 | `				if( pKey ){` |
|      5 |  477 | `					PH7_MemObjRelease(pKey);` |
|      2 |  478 | `				}` |
|      8 |  479 | `				VmPopOperand(&pTos,1);` |
|      8 |  480 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  481 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  482 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  483 | `				rc = rcSc;` |
|      8 |  484 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  485 | `			}` |
|      - |  486 | `			/* Force a hashmap cast  */` |
|     27 |  487 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     27 |  488 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  489 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  490 | `				VM_EXIT_ABORT;` |
|      - |  491 | `			}` |
|     12 |  492 | `		}` |
|      - |  493 | `		/* COW separate the backing variable before mutation */` |
|     27 |  494 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  495 | `	}` |
| 241949 |  496 | `	VmPopOperand(&pTos,1);` |
|      - |  497 | `	/* Phase#2: Perform the insertion */` |
| 241949 |  498 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  499 | `		if( pMap == pVm->pGlobal ){` |
|      - |  500 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  501 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  502 | `			if( pKey == 0 ){` |
|    ! 0 |  503 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  504 | `			}else{` |
|      3 |  505 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  506 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  507 | `				}` |
|      3 |  508 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  509 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  510 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  511 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  512 | `					rc = SXRET_OK;` |
|    ! 0 |  513 | `				}else{` |
|      4 |  514 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  515 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  516 | `						0,pTos->nIdx);` |
|      - |  517 | `				}` |
|      - |  518 | `			}` |
|      2 |  519 | `		}else{` |
|      - |  520 | `			/* Insertion by reference */` |
|     17 |  521 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  522 | `		}` |
|     10 |  523 | `	}else{` |
| 241931 |  524 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  525 | `	}` |
| 241949 |  526 | `	if( pKey ){` |
|  69251 |  527 | `		PH7_MemObjRelease(pKey);` |
|  34623 |  528 | `	}` |
|      - |  529 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  530 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  531 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 241949 |  532 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 241945 |  533 | `	VM_EXIT_BREAK;` |
|    ! 0 |  534 | `	VM_EXIT_BREAK;` |
| 121080 |  535 | `}` |
|      - |  536 |  |
|      - |  537 | `/*` |
|      - |  538 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  539 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  540 | ` */` |
|   1178 |  541 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  542 | `{` |
|   1183 |  543 | `	ph7_value *pTos = pState->pTos;` |
|   1183 |  544 | `	ph7_value *pStack = pState->pStack;` |
|   1183 |  545 | `	VmInstr *aInstr = pState->aInstr;` |
|   1183 |  546 | `	sxi32 pc = pState->pc;` |
|      - |  547 | `	sxi32 rc;` |
|    589 |  548 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1183 |  549 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  550 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  551 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  552 | `	 * plain anonymous function with no captured environment. */` |
|   1183 |  553 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  554 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  555 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  556 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  557 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  558 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1183 |  559 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1183 |  560 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  561 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  562 | `		ph7_vm_func *pClosure;` |
|      - |  563 | `		char *zName;` |
|      - |  564 | `		sxu32 mLen;` |
|      - |  565 | `		sxu32 n;` |
|      - |  566 | `		/* Create a new VM function */` |
|   1173 |  567 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  568 | `		/* Generate an unique closure name */` |
|   1173 |  569 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1173 |  570 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  571 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  572 | `			VM_EXIT_ABORT;` |
|      - |  573 | `		}` |
|   1173 |  574 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1173 |  575 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  576 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  577 | `		}` |
|      - |  578 | `		/* Zero the stucture */` |
|   1173 |  579 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  580 | `		/* Perform a structure assignment on read-only items */` |
|   1173 |  581 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1173 |  582 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1173 |  583 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1173 |  584 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  585 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  586 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1173 |  587 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1173 |  588 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1173 |  589 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1173 |  590 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1173 |  591 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1173 |  592 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1173 |  593 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1173 |  594 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1173 |  595 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1173 |  596 | `		if( pClosure->pUserData == 0 ){` |
|      - |  597 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  598 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  599 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1173 |  600 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    584 |  601 | `		}` |
|      - |  602 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  603 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  604 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  605 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1173 |  606 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  607 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  608 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1173 |  609 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1173 |  610 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1173 |  611 | `		pClosure->sFile = pFunc->sFile;` |
|   1173 |  612 | `		pClosure->nLine = pFunc->nLine;` |
|   1173 |  613 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1173 |  614 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  615 | `		/* Register the closure */` |
|   1173 |  616 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  617 | `		/* Set up closure environment */` |
|   1173 |  618 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1173 |  619 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2547 |  620 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  621 | `			ph7_value *pValue;` |
|   1379 |  622 | `			pEnv = &aEnv[n];` |
|   1379 |  623 | `			sEnv.sName  = pEnv->sName;` |
|   1379 |  624 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1379 |  625 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1379 |  626 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1374 |  627 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    755 |  628 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     63 |  629 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  630 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  631 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  632 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  633 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  634 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  635 | `				 * aliases the name to this slot instead of copying a value. */` |
|     95 |  636 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     95 |  637 | `				if( pValue ){` |
|     95 |  638 | `					sEnv.nIdx = pValue->nIdx;` |
|     95 |  639 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     46 |  640 | `				}` |
|     49 |  641 | `			}else{` |
|      - |  642 | `				/* Standard pass by value */` |
|   1287 |  643 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1287 |  644 | `				if( pValue ){` |
|      - |  645 | `					/* Copy imported value */` |
|    175 |  646 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|     85 |  647 | `				}` |
|      - |  648 | `			}` |
|      - |  649 | `			/* Insert the imported variable */` |
|   1379 |  650 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    692 |  651 | `		}` |
|   1173 |  652 | `		pTarget = pClosure;` |
|    584 |  653 | `	}` |
|      - |  654 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  655 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  656 | `	 * path when the closure is dispatched by name. */` |
|   1183 |  657 | `	pTos++;` |
|      - |  658 | `	{` |
|   1183 |  659 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1183 |  660 | `		if( pCloObj ){` |
|   1183 |  661 | `			pCloObj->iRef++;` |
|   1183 |  662 | `			pTos->x.pOther = pCloObj;` |
|   1183 |  663 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    594 |  664 | `		}else{` |
|      - |  665 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  666 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  667 | `		}` |
|      - |  668 | `	}` |
|   1183 |  669 | `	VM_EXIT_BREAK;` |
|    ! 0 |  670 | `	VM_EXIT_BREAK;` |
|    594 |  671 | `}` |
|      - |  672 |  |
|      - |  673 |  |
|      - |  674 | `/*` |
|      - |  675 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  676 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  677 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  678 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  679 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  680 | ` */` |
|     14 |  681 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  682 | `{` |
|     17 |  683 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  684 | `}` |
|      - |  685 | `/*` |
|      - |  686 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  687 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  688 | ` */` |
| 679049 |  689 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  690 | `{` |
| 679054 |  691 | `	ph7_value *pTos = pState->pTos;` |
| 679054 |  692 | `	ph7_value *pStack = pState->pStack;` |
| 679054 |  693 | `	VmInstr *aInstr = pState->aInstr;` |
| 679054 |  694 | `	sxi32 pc = pState->pc;` |
|      - |  695 | `	sxi32 rc;` |
| 339895 |  696 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 679054 |  697 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 679054 |  698 | `	ph7_hashmap *pMap = 0;` |
|      - |  699 | `	ph7_value *pIdx;` |
| 679054 |  700 | `	pIdx = 0;` |
| 679054 |  701 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  702 | `		if( !pInstr->iP2){` |
|      - |  703 | `			/* No available index,load NULL */` |
|    ! 0 |  704 | `			if( pTos >= pStack ){` |
|    ! 0 |  705 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  706 | `			}else{` |
|      - |  707 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  708 | `				pTos++;` |
|    ! 0 |  709 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  710 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  711 | `			}` |
|      - |  712 | `			/* Emit a notice */` |
|    ! 0 |  713 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  714 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  715 | `			VM_EXIT_BREAK;` |
|      - |  716 | `		}` |
|      2 |  717 | `	}else{` |
| 679052 |  718 | `		pIdx = pTos;` |
| 679052 |  719 | `		pTos--;` |
|      - |  720 | `	}` |
| 679054 |  721 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  722 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  723 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  724 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  725 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|      7 |  726 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  727 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|      2 |  728 | `		}` |
|      7 |  729 | `		if( pIdx ){` |
|      - |  730 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  731 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|      7 |  732 | `			PH7_MemObjRelease(pIdx);` |
|      3 |  733 | `		}` |
|      7 |  734 | `		PH7_MemObjRelease(pTos);` |
|      7 |  735 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      7 |  736 | `		VM_EXIT_BREAK;` |
|      - |  737 | `	}` |
| 679048 |  738 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  739 | `		/* String access */` |
| 532636 |  740 | `		if( pIdx ){` |
|      - |  741 | `			sxi64 iOfft;` |
| 532636 |  742 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 532636 |  743 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  744 | `				/* Force an int cast */` |
|    ! 0 |  745 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  746 | `			}` |
| 532636 |  747 | `			iOfft = pIdx->x.iVal;` |
|      - |  748 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  749 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  750 | `			 * number, ran past the end and quietly produced NULL. */` |
| 532636 |  751 | `			if( iOfft < 0 ){` |
|      7 |  752 | `				iOfft += nLen;` |
|      3 |  753 | `			}` |
| 532640 |  754 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  755 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  756 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  757 | `				 * silently produced NULL in both cases). */` |
|      - |  758 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  759 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  760 | `				 * lookups and must stay silent. */` |
|     11 |  761 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  762 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  763 | `				PH7_MemObjRelease(pTos);` |
|     10 |  764 | `				if( bQuiet ){` |
|      8 |  765 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  766 | `				}else{` |
|      3 |  767 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  768 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  769 | `						pIdx->x.iVal);` |
|      - |  770 | `				}` |
|      6 |  771 | `			}else{` |
| 532628 |  772 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 532628 |  773 | `				int c = zData[iOfft];` |
| 532628 |  774 | `				PH7_MemObjRelease(pTos);` |
| 532628 |  775 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 532628 |  776 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  777 | `			}` |
| 266694 |  778 | `		}else{` |
|      - |  779 | `			/* No available index,load NULL */` |
|    ! 0 |  780 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  781 | `		}` |
| 532636 |  782 | `		VM_EXIT_BREAK;` |
|      - |  783 | `	}` |
| 146417 |  784 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  785 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  786 | `		 * iP2 codes:` |
|      - |  787 | `		 *   0 = read       → offsetGet` |
|      - |  788 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  789 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  790 | `		 *   4 = isset()    → offsetExists` |
|      - |  791 | `		 *   5 = unset()    → offsetUnset` |
|      - |  792 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  793 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  794 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  795 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  796 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  797 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  798 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  799 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  800 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  801 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  802 | `			ph7_class_method *pMeth;` |
|      - |  803 | `			ph7_value sResult;` |
|      - |  804 | `			ph7_value *apArg[1];` |
|    181 |  805 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  806 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  807 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  808 | `					"Cannot use [] for reading");` |
|    ! 0 |  809 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  810 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  811 | `				VM_EXIT_BREAK;` |
|      - |  812 | `			}` |
|    181 |  813 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  814 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  815 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  816 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  817 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  818 | `				apArg[0] = pIdx;` |
|     81 |  819 | `				if( pMeth ){` |
|     81 |  820 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  821 | `				}` |
|    143 |  822 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  823 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  824 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  825 | `				apArg[0] = pIdx;` |
|     20 |  826 | `				if( pMeth ){` |
|     20 |  827 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  828 | `				}` |
|     12 |  829 | `			}else{` |
|     89 |  830 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  831 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  832 | `				apArg[0] = pIdx;` |
|     89 |  833 | `				if( pMeth ){` |
|     89 |  834 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  835 | `				}` |
|      - |  836 | `			}` |
|    181 |  837 | `			if( pInstr->iP2 == 4 ){` |
|      - |  838 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  839 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  840 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  841 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  842 | `				PH7_MemObjRelease(pTos);` |
|     53 |  843 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  844 | `				if( bExists ){` |
|     28 |  845 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  846 | `					pTos->x.iVal = 1;` |
|     16 |  847 | `				}else{` |
|     29 |  848 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  849 | `				}` |
|    157 |  850 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  851 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  852 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  853 | `				PH7_MemObjRelease(pTos);` |
|     20 |  854 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  855 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  856 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  857 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  858 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  859 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  860 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  861 | `				 * coalesce takes the default, the real value on a hit. */` |
|     23 |  862 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     23 |  863 | `				PH7_MemObjRelease(&sResult);` |
|     23 |  864 | `				PH7_MemObjRelease(pTos);` |
|     23 |  865 | `				pTos->nIdx = SXU32_HIGH;` |
|     23 |  866 | `				if( !bExists ){` |
|      9 |  867 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      6 |  868 | `				}else{` |
|     17 |  869 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  870 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  871 | `					ph7_value sValue;` |
|     17 |  872 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     17 |  873 | `					apArg[0] = pIdx;` |
|     17 |  874 | `					if( pGet ){` |
|     17 |  875 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  876 | `					}` |
|     17 |  877 | `					PH7_MemObjStore(&sValue,pTos);` |
|     17 |  878 | `					PH7_MemObjRelease(&sValue);` |
|      - |  879 | `				}` |
|     23 |  880 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     23 |  881 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  882 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  883 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  884 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  885 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  886 | `				 *     and push NULL.` |
|      - |  887 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  888 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  889 | `				int bShouldArm = !bExists;` |
|      - |  890 | `				ph7_value sValue;` |
|     10 |  891 | `				PH7_MemObjRelease(&sResult);` |
|      - |  892 | `				/* Reset any prior arming defensively */` |
|     10 |  893 | `				VmCoalesceDisarm(pVm);` |
|     10 |  894 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  895 | `				if( bExists ){` |
|      5 |  896 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  897 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  898 | `					apArg[0] = pIdx;` |
|      5 |  899 | `					if( pGet ){` |
|      5 |  900 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  901 | `					}` |
|      5 |  902 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  903 | `						bShouldArm = 1;` |
|      1 |  904 | `					}` |
|      2 |  905 | `				}` |
|     10 |  906 | `				PH7_MemObjRelease(pTos);` |
|     10 |  907 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  908 | `				if( bShouldArm ){` |
|      - |  909 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  910 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  911 | `					 * intervening expression evaluation. */` |
|      8 |  912 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  913 | `					if( pIdx ){` |
|      8 |  914 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  915 | `					}` |
|      8 |  916 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  917 | `					pInst->iRef++;` |
|      8 |  918 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  919 | `				}else{` |
|      3 |  920 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  921 | `				}` |
|     10 |  922 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  923 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  924 | `				VM_EXIT_BREAK;` |
|    ! 0 |  925 | `			}else{` |
|      - |  926 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  927 | `				PH7_MemObjRelease(pTos);` |
|     89 |  928 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  929 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  930 | `			}` |
|    153 |  931 | `			PH7_MemObjRelease(&sResult);` |
|    153 |  932 | `			if( pIdx ){` |
|    153 |  933 | `				PH7_MemObjRelease(pIdx);` |
|     74 |  934 | `			}` |
|    153 |  935 | `			VM_EXIT_BREAK;` |
|      - |  936 | `		}` |
|      - |  937 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  938 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  939 | `		if( pInst ){` |
|      - |  940 | `			char zMsg[256];` |
|      3 |  941 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  942 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  943 | `				"Cannot use object of type %.*s as array",` |
|      2 |  944 | `				(int)pName->nByte,pName->zString);` |
|      3 |  945 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  946 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  947 | `			PH7_MemObjRelease(pTos);` |
|      3 |  948 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  949 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  950 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  951 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  952 | `			 * execution carried on inside the try block. */` |
|      3 |  953 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  954 | `		}` |
|    ! 0 |  955 | `	}` |
| 146239 |  956 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  957 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  958 | `			ph7_value *pObj;` |
|     23 |  959 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  960 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  961 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  962 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  963 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  964 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  965 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  966 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  967 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  968 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - |  969 | `				 * it is not a bool). */` |
|     23 |  970 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - |  971 | `					SyBlob sErrMsg;` |
|      7 |  972 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 |  973 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - |  974 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 |  975 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 |  976 | `					if( pIdx ){` |
|      7 |  977 | `						PH7_MemObjRelease(pIdx);` |
|      3 |  978 | `					}` |
|      7 |  979 | `					PH7_MemObjRelease(pTos);` |
|      7 |  980 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 |  981 | `					VM_EXIT_BREAK;` |
|      - |  982 | `				}` |
|     17 |  983 | `				PH7_MemObjToHashmap(pObj);` |
|     17 |  984 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 |  985 | `			}` |
|      8 |  986 | `		}` |
|      8 |  987 | `	}` |
| 146233 |  988 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - |  989 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - |  990 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - |  991 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|      - |  992 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|      - |  993 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|      - |  994 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|      - |  995 | `	 * accepted with a warning and becomes its integer id. */` |
| 146233 |  996 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - |  997 | `		SyBlob sTypeMsg;` |
| 146221 |  998 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,pInstr->iP2,&sTypeMsg) ){` |
|     13 |  999 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|     13 | 1000 | `			PH7_MemObjRelease(pIdx);` |
|     13 | 1001 | `			PH7_MemObjRelease(pTos);` |
|     13 | 1002 | `			pTos->nIdx = SXU32_HIGH;` |
|     13 | 1003 | `			VM_EXIT_BREAK;` |
|      - | 1004 | `		}` |
| 146209 | 1005 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  73102 | 1006 | `	}` |
| 146216 | 1007 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 146210 | 1008 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  33897 | 1009 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  50846 | 1010 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  33892 | 1011 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  33897 | 1012 | `		if( bNull \|\| bLossyFloat ){` |
|      - | 1013 | `			SyBlob sErrMsg;` |
|      5 | 1014 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1015 | `			SyBlobAppend(&sErrMsg,` |
|      2 | 1016 | `				bNull ? "Cannot access offset of type null on array"` |
|      - | 1017 | `				      : "Cannot access offset of type float on array",` |
|      4 | 1018 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - | 1019 | `				                      : "Cannot access offset of type float on array"));` |
|      5 | 1020 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 | 1021 | `			PH7_MemObjRelease(pIdx);` |
|      5 | 1022 | `			PH7_MemObjRelease(pTos);` |
|      5 | 1023 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 | 1024 | `			VM_EXIT_BREAK;` |
|      - | 1025 | `		}` |
|  16944 | 1026 | `	}` |
| 146217 | 1027 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 146207 | 1028 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - | 1029 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - | 1030 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - | 1031 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - | 1032 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - | 1033 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - | 1034 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1265 | 1035 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    630 | 1036 | `		}` |
|      - | 1037 | `		/* Point to the hashmap */` |
| 146207 | 1038 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 146207 | 1039 | `		if( pIdx ){` |
|      - | 1040 | `			/* Load the desired entry */` |
| 146205 | 1041 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  73100 | 1042 | `		}` |
| 146207 | 1043 | `		if( pInstr->iP2 == 3 ){` |
|      - | 1044 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - | 1045 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - | 1046 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - | 1047 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - | 1048 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - | 1049 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - | 1050 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - | 1051 | `			 * correct for the outermost write. */` |
|     21 | 1052 | `			int needWrite = (rc != SXRET_OK);` |
|     21 | 1053 | `			if( !needWrite && pNode ){` |
|     13 | 1054 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 | 1055 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 | 1056 | `					needWrite = 1;` |
|      3 | 1057 | `				}` |
|      6 | 1058 | `			}` |
|     21 | 1059 | `			if( needWrite ){` |
|     15 | 1060 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 | 1061 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - | 1062 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - | 1063 | `					 * into the new map's storage. */` |
|      7 | 1064 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 | 1065 | `					if( pIdx ){` |
|      7 | 1066 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 | 1067 | `					}` |
|      3 | 1068 | `				}` |
|      7 | 1069 | `			}` |
|     10 | 1070 | `		}` |
| 146207 | 1071 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - | 1072 | `			/* Create a new empty entry */` |
|    324 | 1073 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 | 1074 | `			if( rc == SXRET_OK ){` |
|      - | 1075 | `				/* Point to the last inserted entry */` |
|    321 | 1076 | `				pNode = pMap->pLast;` |
|    161 | 1077 | `			}else{` |
|      - | 1078 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - | 1079 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - | 1080 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - | 1081 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 | 1082 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - | 1083 | `			}` |
|    160 | 1084 | `		}` |
|  73100 | 1085 | `	}` |
| 146210 | 1086 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  40304 | 1087 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      8 | 1088 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1089 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - | 1090 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - | 1091 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - | 1092 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - | 1093 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - | 1094 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - | 1095 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - | 1096 | `		 * STRING key quoted. */` |
|      - | 1097 | `		SyBlob sMsg;` |
|      8 | 1098 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 | 1099 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - | 1100 | `			SyString sKey;` |
|      6 | 1101 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1102 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 | 1103 | `			}` |
|      6 | 1104 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      6 | 1105 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      4 | 1106 | `		}else{` |
|      3 | 1107 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1108 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1109 | `			}` |
|      3 | 1110 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1111 | `		}` |
|      8 | 1112 | `		SyBlobNullAppend(&sMsg);` |
|      8 | 1113 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 | 1114 | `		SyBlobRelease(&sMsg);` |
|      3 | 1115 | `	}` |
| 146216 | 1116 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  73116 | 1117 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1118 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1119 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1120 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1121 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1122 | `			VmArithTypeName(pTos));` |
|      3 | 1123 | `	}` |
| 146221 | 1124 | `	if( pIdx ){` |
| 146221 | 1125 | `		PH7_MemObjRelease(pIdx);` |
|  73108 | 1126 | `	}` |
| 146221 | 1127 | `	if( rc == SXRET_OK ){` |
|      - | 1128 | `		/* Load entry contents */` |
|  65625 | 1129 | `		if( pMap->iRef < 2 ){` |
|      - | 1130 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1131 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1132 | `			 */` |
|    117 | 1133 | `			pTos->nIdx = SXU32_HIGH;` |
|    117 | 1134 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     60 | 1135 | `		}else{` |
|  65511 | 1136 | `			pTos->nIdx = pNode->nValIdx;` |
|  65511 | 1137 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  65511 | 1138 | `			PH7_HashmapUnref(pMap);` |
|      - | 1139 | `		}` |
|  32815 | 1140 | `	}else{` |
|      - | 1141 | `		/* No such entry,load NULL */` |
|  80601 | 1142 | `		PH7_MemObjRelease(pTos);` |
|  80601 | 1143 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1144 | `	}` |
| 146221 | 1145 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1146 | `	VM_EXIT_BREAK;` |
| 339906 | 1147 | `}` |
|      - | 1148 |  |
|      - | 1149 | `/*` |
|      - | 1150 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1151 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1152 | ` */` |
|  73856 | 1153 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1154 | `{` |
|  73861 | 1155 | `	ph7_value *pTos = pState->pTos;` |
|  73861 | 1156 | `	ph7_value *pStack = pState->pStack;` |
|  73861 | 1157 | `	VmInstr *aInstr = pState->aInstr;` |
|  73861 | 1158 | `	sxi32 pc = pState->pc;` |
|      - | 1159 | `	sxi32 rc;` |
|  36928 | 1160 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1161 | `	ph7_hashmap *pMap;` |
|      - | 1162 | `	/* Allocate a new hashmap instance */` |
|  73861 | 1163 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73861 | 1164 | `	if( pMap == 0 ){` |
|    ! 0 | 1165 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1166 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1167 | `		VM_EXIT_ABORT;` |
|      - | 1168 | `	}` |
|  73861 | 1169 | `	if( pInstr->iP1 > 0 ){` |
|  11013 | 1170 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  11013 | 1171 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1172 | `		/* Perform the insertion */` |
|  41801 | 1173 | `		while( pEntry < pTos ){` |
|  30811 | 1174 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1175 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1176 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1177 | `				 * renumbered. Same routine that backs array_merge. */` |
|    682 | 1178 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1179 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1180 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1181 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1182 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1183 | `						 * map dangling. */` |
|    ! 0 | 1184 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1185 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1186 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1187 | `						break;` |
|      1 | 1188 | `					}` |
|    353 | 1189 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1190 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1191 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1192 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1193 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1194 | `						rcSpread = rcW;` |
|    ! 0 | 1195 | `						break;` |
|      - | 1196 | `					}` |
|      3 | 1197 | `				}else{` |
|      - | 1198 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     20 | 1199 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     20 | 1200 | `					break;` |
|      1 | 1201 | `				}` |
|  30462 | 1202 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1203 | `				/* Insertion by reference */` |
|    181 | 1204 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1205 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1206 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1207 | `					);` |
|     61 | 1208 | `			}else{` |
|      - | 1209 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1210 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1211 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1212 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1213 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1214 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  30011 | 1215 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1216 | `						/* An object/array literal key is php's TypeError, a resource one` |
|      - | 1217 | `						 * warns and becomes its id — same rules as a subscript. */` |
|      - | 1218 | `						SyBlob sTypeMsg;` |
|  11205 | 1219 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|      3 | 1220 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|      2 | 1221 | `						}else{` |
|  11203 | 1222 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|      - | 1223 | `						}` |
|      - | 1224 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11205 | 1225 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16806 | 1226 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11200 | 1227 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11205 | 1228 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1229 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1230 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1231 | `							SyBlob sErrMsg;` |
|      5 | 1232 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1233 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1234 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1235 | `						}` |
|   5600 | 1236 | `					}` |
|      - | 1237 | `				/* Standard insertion */` |
|  45014 | 1238 | `				PH7_HashmapInsert(pMap,` |
|  30006 | 1239 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  15003 | 1240 | `					&pEntry[1]` |
|      - | 1241 | `				);` |
|      - | 1242 | `			}` |
|      - | 1243 | `			/* Next pair on the stack */` |
|  30793 | 1244 | `			pEntry += 2;` |
|      5 | 1245 | `		}` |
|      - | 1246 | `		/* Pop P1 elements */` |
|  11013 | 1247 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  11013 | 1248 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1249 | `			/* Discard the partially-built map and propagate the exception. */` |
|     20 | 1250 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     20 | 1251 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1252 | `				VM_EXIT_ABORT;` |
|      - | 1253 | `			}` |
|      - | 1254 | `			{` |
|      - | 1255 | `				sxi32 iRp;` |
|     20 | 1256 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1257 | `					pc = iRp;` |
|      6 | 1258 | `					VM_EXIT_BREAK;` |
|      - | 1259 | `				}` |
|      - | 1260 | `			}` |
|     15 | 1261 | `			VM_EXIT_EXCEPTION;` |
|      - | 1262 | `		}` |
|   5495 | 1263 | `	}` |
|      - | 1264 | `	/* Push the hashmap */` |
|  73843 | 1265 | `	pTos++;` |
|  73843 | 1266 | `	pTos->nIdx = SXU32_HIGH;` |
|  73843 | 1267 | `	pTos->x.pOther = pMap;` |
|  73843 | 1268 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73843 | 1269 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1270 | `	VM_EXIT_BREAK;` |
|  36933 | 1271 | `}` |
|      - | 1272 |  |
|      - | 1273 | `/*` |
|      - | 1274 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1275 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1276 | ` */` |
|    266 | 1277 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1278 | `{` |
|    271 | 1279 | `	ph7_value *pTos = pState->pTos;` |
|    271 | 1280 | `	ph7_value *pStack = pState->pStack;` |
|    271 | 1281 | `	VmInstr *aInstr = pState->aInstr;` |
|    271 | 1282 | `	sxi32 pc = pState->pc;` |
|      - | 1283 | `	sxi32 rc;` |
|    133 | 1284 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1285 | `	ph7_value *pEntry;` |
|    271 | 1286 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1287 | `		/* Empty list,break immediately */` |
|    ! 0 | 1288 | `		VM_EXIT_BREAK;` |
|      - | 1289 | `	}` |
|    271 | 1290 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1291 | `#ifdef UNTRUST` |
|      - | 1292 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1293 | `		VM_EXIT_ABORT;` |
|      - | 1294 | `	}` |
|      - | 1295 | `#endif` |
|    271 | 1296 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1297 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1298 | `		ph7_hashmap_node *pNode;` |
|      - | 1299 | `		ph7_value sKey,*pObj;` |
|      - | 1300 | `		/* Start Copying */` |
|    265 | 1301 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1302 | `		while( pEntry <= pTos ){` |
|    551 | 1303 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1304 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1305 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1306 | `					if( rc == SXRET_OK ){` |
|      - | 1307 | `						/* Store node value */` |
|    523 | 1308 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1309 | `					}else{` |
|      - | 1310 | `						/* Undefined array key */` |
|      - | 1311 | `						char zMsg[128];` |
|    ! 0 | 1312 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1313 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1314 | `						PH7_MemObjRelease(pObj);` |
|      - | 1315 | `					}` |
|    259 | 1316 | `				}` |
|    259 | 1317 | `			}` |
|    551 | 1318 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1319 | `			pEntry++;` |
|      5 | 1320 | `		}` |
|    135 | 1321 | `	}else{` |
|      - | 1322 | `		/* Source is not an array */` |
|      - | 1323 | `		ph7_value *pObj;` |
|     18 | 1324 | `		while( pEntry <= pTos ){` |
|     12 | 1325 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|     12 | 1326 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|     12 | 1327 | `					PH7_MemObjRelease(pObj);` |
|      5 | 1328 | `				}` |
|      5 | 1329 | `			}` |
|     12 | 1330 | `			pEntry++;` |
|      2 | 1331 | `		}` |
|      8 | 1332 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1333 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|      3 | 1334 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      1 | 1335 | `		}` |
|      - | 1336 | `	}` |
|    271 | 1337 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    271 | 1338 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1339 | `	VM_EXIT_BREAK;` |
|    138 | 1340 | `}` |
|      - | 1341 |  |
|      - | 1342 | `/*` |
|      - | 1343 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1344 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1345 | ` */` |
|   6762 | 1346 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1347 | `{` |
|   6767 | 1348 | `	ph7_value *pTos = pState->pTos;` |
|   6767 | 1349 | `	ph7_value *pStack = pState->pStack;` |
|   6767 | 1350 | `	VmInstr *aInstr = pState->aInstr;` |
|   6767 | 1351 | `	sxi32 pc = pState->pc;` |
|      - | 1352 | `	sxi32 rc;` |
|   3381 | 1353 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1354 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6767 | 1355 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6767 | 1356 | `	if( pName && pVm->pFrame ){` |
|      - | 1357 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1358 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1359 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6767 | 1360 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6767 | 1361 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6767 | 1362 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1363 | `			VM_EXIT_ABORT;` |
|      - | 1364 | `		}` |
|      - | 1365 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1366 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1367 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1368 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6765 | 1369 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1370 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1371 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1372 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1373 | `				VM_EXIT_ABORT;` |
|      - | 1374 | `			}` |
|      3 | 1375 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1376 | `		}` |
|   3379 | 1377 | `	}` |
|   6763 | 1378 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1379 | `	VM_EXIT_BREAK;` |
|   3386 | 1380 | `}` |
|      - | 1381 |  |
