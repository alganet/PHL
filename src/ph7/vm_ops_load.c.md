# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 671/786 lines (85.37%)

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
|     42 |  139 | `			pFrameLocal = pVm->pFrame;` |
|     42 |  140 | `			pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  141 | `			/* Query the local frame */` |
|     42 |  142 | `			pEntry = SyHashGet(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte);` |
|     42 |  143 | `			if( pEntry ){` |
|    ! 0 |  144 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Referenced variable name '%z' already exists",&sName);` |
|    ! 0 |  145 | `			}else{` |
|     42 |  146 | `				rc = SyHashInsert(&pFrameLocal->hVar,(const void *)sName.zString,sName.nByte,SX_INT_TO_PTR(nIdx));` |
|     42 |  147 | `				if( pFrameLocal->pParent == 0 ){` |
|      - |  148 | `					/* Insert in the $GLOBALS array */` |
|     38 |  149 | `					VmHashmapRefInsert(pVm->pGlobal,sName.zString,sName.nByte,nIdx);` |
|     18 |  150 | `				}` |
|     42 |  151 | `				if( rc == SXRET_OK ){` |
|     42 |  152 | `					PH7_VmRefObjInstall(&(*pVm),nIdx,SyHashLastEntry(&pFrameLocal->hVar),0,0);` |
|     20 |  153 | `				}` |
|      - |  154 | `			}` |
|      - |  155 | `		}` |
|     20 |  156 | `	}` |
|     42 |  157 | `	VM_EXIT_BREAK;` |
|    ! 0 |  158 | `	VM_EXIT_BREAK;` |
|     29 |  159 | `}` |
|      - |  160 |  |
|      - |  161 | `/*` |
|      - |  162 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  163 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  164 | ` */` |
| 240828 |  165 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  166 | `{` |
| 240833 |  167 | `	ph7_value *pTos = pState->pTos;` |
| 240833 |  168 | `	ph7_value *pStack = pState->pStack;` |
| 240833 |  169 | `	VmInstr *aInstr = pState->aInstr;` |
| 240833 |  170 | `	sxi32 pc = pState->pc;` |
|      - |  171 | `	sxi32 rc;` |
| 120414 |  172 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 240833 |  173 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  174 | `	ph7_value *pKey;` |
|      - |  175 | `	sxu32 nIdx;` |
| 240833 |  176 | `	if( pInstr->iP1 ){` |
|      - |  177 | `		/* Key is next on stack */` |
|  69345 |  178 | `		pKey = pTos;` |
|  69345 |  179 | `		pTos--;` |
|  34675 |  180 | `	}else{` |
| 171493 |  181 | `		pKey = 0;` |
|      - |  182 | `	}` |
|      - |  183 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  184 | `		 * normalizes to ""); PHL rejects it. */` |
| 240833 |  185 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69147 |  186 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103721 |  187 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69142 |  188 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|  69147 |  189 | `			if( bNull \|\| bLossyFloat ){` |
|      - |  190 | `				sxi32 rcSc;` |
|      5 |  191 | `				const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 |  192 | `				                         : "Cannot access offset of type float on array";` |
|      5 |  193 | `				PH7_MemObjRelease(pKey);` |
|      5 |  194 | `				VmPopOperand(&pTos,1);` |
|      5 |  195 | `				rcSc = VmThrowFromVm(&(*pVm),"TypeError",zErr,(sxu32)SyStrlen(zErr));` |
|      5 |  196 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  197 | `				rc = rcSc;` |
|      5 |  198 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  199 | `			}` |
|  34569 |  200 | `		}` |
| 240829 |  201 | `	nIdx = pTos->nIdx;` |
|      - |  202 | `	{` |
|      - |  203 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  204 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  205 | `		 * the backing variable slot at nIdx. */` |
| 240829 |  206 | `		ph7_class_instance *pInst = 0;` |
| 240829 |  207 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  208 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 240786 |  209 | `		}else if( nIdx != SXU32_HIGH ){` |
| 240743 |  210 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 240743 |  211 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  212 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  213 | `			}` |
| 120369 |  214 | `		}` |
| 240829 |  215 | `		if( pInst ){` |
|     89 |  216 | `			ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|     89 |  217 | `			if( pArrayAccess && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  218 | `				ph7_class_method *pMeth;` |
|      - |  219 | `				ph7_value sNullKey;` |
|      - |  220 | `				ph7_value *apArg[2];` |
|     87 |  221 | `				if( pInstr->iOp == PH7_OP_STORE_IDX_REF ){` |
|    ! 0 |  222 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|      - |  223 | `						"Cannot assign by reference to overloaded object");` |
|    ! 0 |  224 | `					if( pKey ){ PH7_MemObjRelease(pKey); }` |
|    ! 0 |  225 | `					VmPopOperand(&pTos,2); /* container + value */` |
|    ! 0 |  226 | `					VM_EXIT_BREAK;` |
|      - |  227 | `				}` |
|     87 |  228 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  229 | `					"offsetSet",sizeof("offsetSet")-1);` |
|      - |  230 | `				/* Pop container; pTos now points to the value */` |
|     87 |  231 | `				VmPopOperand(&pTos,1);` |
|     87 |  232 | `				if( pKey == 0 ){` |
|     10 |  233 | `					PH7_MemObjInit(&(*pVm),&sNullKey);` |
|     10 |  234 | `					apArg[0] = &sNullKey;` |
|      6 |  235 | `				}else{` |
|     79 |  236 | `					apArg[0] = pKey;` |
|      - |  237 | `				}` |
|     87 |  238 | `				apArg[1] = pTos;` |
|     87 |  239 | `				if( pMeth ){` |
|     87 |  240 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,0,2,apArg);` |
|     42 |  241 | `				}` |
|     87 |  242 | `				if( pKey ){` |
|     79 |  243 | `					PH7_MemObjRelease(pKey);` |
|     41 |  244 | `				}else{` |
|     10 |  245 | `					PH7_MemObjRelease(&sNullKey);` |
|      - |  246 | `				}` |
|      - |  247 | `				/* Pop the value */` |
|     87 |  248 | `				VmPopOperand(&pTos,1);` |
|     87 |  249 | `				VM_EXIT_BREAK;` |
|      - |  250 | `			}` |
|      - |  251 | `			/* Object without ArrayAccess: PHP throws a fatal Error rather` |
|      - |  252 | `			 * than silently coercing the object into a hashmap (which is` |
|      - |  253 | `			 * what the legacy PH7 fall-through would do via MemObjToHashmap` |
|      - |  254 | `			 * a few lines below). Match PHP. */` |
|      - |  255 | `			{` |
|      - |  256 | `				char zMsg[256];` |
|      3 |  257 | `				SyString *pName = &pInst->pClass->sName;` |
|      4 |  258 | `				sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  259 | `					"Cannot use object of type %.*s as array",` |
|      2 |  260 | `					(int)pName->nByte,pName->zString);` |
|      3 |  261 | `				rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  262 | `				if( pKey ){ PH7_MemObjRelease(pKey); }` |
|      3 |  263 | `				VmPopOperand(&pTos,2); /* container + value */` |
|      3 |  264 | `				if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      3 |  265 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  266 | `			}` |
|      - |  267 | `		}` |
|      - |  268 | `	}` |
| 240743 |  269 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  270 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  271 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  272 | `		 * checking true sharing count, then re-add after separation. */` |
| 240607 |  273 | `		if( nIdx != SXU32_HIGH ){` |
| 240607 |  274 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 360908 |  275 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 240607 |  276 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  277 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  278 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  279 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  280 | `				 * refcounts if the backing array was already separated. */` |
| 240607 |  281 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 240607 |  282 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 240607 |  283 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 240607 |  284 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 240607 |  285 | `					pTos->x.pOther = pMap;` |
| 120306 |  286 | `				}else{` |
|      - |  287 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  288 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  289 | `					pMap = pCur;` |
|      - |  290 | `				}` |
| 120306 |  291 | `			}else{` |
|    ! 0 |  292 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  293 | `			}` |
| 120306 |  294 | `		}else{` |
|    ! 0 |  295 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  296 | `		}` |
| 240607 |  297 | `		if( pMap->iRef < 2 ){` |
|      - |  298 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  299 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  300 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  301 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  302 | `			pMap->iRef = 2;` |
|    ! 0 |  303 | `		}` |
| 120306 |  304 | `	}else{` |
|      - |  305 | `		ph7_value *pObj;` |
|    139 |  306 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    139 |  307 | `		if( pObj == 0 ){` |
|    ! 0 |  308 | `			if( pKey ){` |
|    ! 0 |  309 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  310 | `			}` |
|    ! 0 |  311 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  312 | `			VM_EXIT_BREAK;` |
|      - |  313 | `		}` |
|      - |  314 | `		/* Phase#1: Load the array */` |
|    139 |  315 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
|    107 |  316 | `			VmPopOperand(&pTos,1);` |
|    107 |  317 | `			if( (pTos->iFlags&MEMOBJ_STRING) == 0 ){` |
|      - |  318 | `				/* Force a string cast */` |
|    ! 0 |  319 | `				PH7_MemObjToString(pTos);` |
|    ! 0 |  320 | `			}` |
|    107 |  321 | `			if( pKey == 0 ){` |
|      - |  322 | `				/* Append string */` |
|      3 |  323 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|      3 |  324 | `					SyBlobAppend(&pObj->sBlob,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      1 |  325 | `				}` |
|      2 |  326 | `			}else{` |
|      - |  327 | `				sxi64 iOfft;` |
|      - |  328 | `				sxi64 nLen;` |
|    105 |  329 | `				if((pKey->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  330 | `					/* Force an int cast */` |
|    ! 0 |  331 | `					PH7_MemObjToInteger(pKey);` |
|    ! 0 |  332 | `				}` |
|    105 |  333 | `				iOfft = pKey->x.iVal;` |
|    105 |  334 | `				nLen = (sxi64)SyBlobLength(&pObj->sBlob);` |
|    105 |  335 | `				if( iOfft < 0 ){` |
|      - |  336 | `					/* php 7.1: a negative offset writes back from the end. */` |
|      5 |  337 | `					iOfft += nLen;` |
|      5 |  338 | `					if( iOfft < 0 ){` |
|      4 |  339 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Illegal string offset %qd",` |
|      1 |  340 | `							pKey->x.iVal);` |
|      3 |  341 | `						PH7_MemObjRelease(pKey);` |
|      3 |  342 | `						VM_EXIT_BREAK;` |
|      - |  343 | `					}` |
|      1 |  344 | `				}` |
|    103 |  345 | `				if( SyBlobLength(&pTos->sBlob) > 0 ){` |
|    103 |  346 | `					const char *zBlob = (const char *)SyBlobData(&pTos->sBlob);` |
|    103 |  347 | `					if( SyBlobLength(&pTos->sBlob) > 1 ){` |
|      3 |  348 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      - |  349 | `							"Only the first byte will be assigned to the string offset");` |
|      1 |  350 | `					}` |
|    103 |  351 | `					if( iOfft >= nLen ){` |
|      - |  352 | `						/* php PADS WITH SPACES up to the offset. PH7 simply appended the` |
|      - |  353 | `						 * byte, so "abc" with [6]="Z" became "abcZ" rather than "abc   Z"` |
|      - |  354 | `						 * -- a silently wrong string. */` |
|      - |  355 | `						sxi64 nPad;` |
|      9 |  356 | `						for( nPad = nLen ; nPad < iOfft ; ++nPad ){` |
|      7 |  357 | `							SyBlobAppend(&pObj->sBlob," ",sizeof(char));` |
|      4 |  358 | `						}` |
|      3 |  359 | `						SyBlobAppend(&pObj->sBlob,(const void *)zBlob,sizeof(char));` |
|      2 |  360 | `					}else{` |
|    101 |  361 | `						char *zData = (char *)SyBlobData(&pObj->sBlob);` |
|    101 |  362 | `						zData[iOfft] = zBlob[0];` |
|      - |  363 | `					}` |
|     51 |  364 | `				}` |
|      - |  365 | `			}` |
|    105 |  366 | `			if( pKey ){` |
|    103 |  367 | `			  PH7_MemObjRelease(pKey);` |
|     51 |  368 | `			}` |
|    105 |  369 | `			VM_EXIT_BREAK;` |
|     33 |  370 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  371 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  372 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  373 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  374 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  375 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     33 |  376 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     33 |  377 | `			if( bScalar ){` |
|      - |  378 | `				sxi32 rcSc;` |
|      8 |  379 | `				if( pKey ){` |
|      5 |  380 | `					PH7_MemObjRelease(pKey);` |
|      2 |  381 | `				}` |
|      8 |  382 | `				VmPopOperand(&pTos,1);` |
|      8 |  383 | `				rcSc = VmThrowFromVm(&(*pVm),"Error","Cannot use a scalar value as an array",` |
|      - |  384 | `					sizeof("Cannot use a scalar value as an array")-1);` |
|      8 |  385 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      8 |  386 | `				rc = rcSc;` |
|      8 |  387 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  388 | `			}` |
|      - |  389 | `			/* Force a hashmap cast  */` |
|     27 |  390 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     27 |  391 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  392 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  393 | `				VM_EXIT_ABORT;` |
|      - |  394 | `			}` |
|     12 |  395 | `		}` |
|      - |  396 | `		/* COW separate the backing variable before mutation */` |
|     27 |  397 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  398 | `	}` |
| 240631 |  399 | `	VmPopOperand(&pTos,1);` |
|      - |  400 | `	/* Phase#2: Perform the insertion */` |
| 240631 |  401 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
|     19 |  402 | `		if( pMap == pVm->pGlobal ){` |
|      - |  403 | `			/* php 8.1: $GLOBALS['y'] =& $x binds the global $y to $x's` |
|      - |  404 | `			 * slot; an append has no name to bind (catchable Error). */` |
|      3 |  405 | `			if( pKey == 0 ){` |
|    ! 0 |  406 | `				rc = PH7_VmThrowGlobalsAppendError(&(*pVm));` |
|    ! 0 |  407 | `			}else{` |
|      3 |  408 | `				if( (pKey->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  409 | `					PH7_MemObjToString(pKey);` |
|    ! 0 |  410 | `				}` |
|      3 |  411 | `				if( SyBlobLength(&pKey->sBlob) < 1 ){` |
|      - |  412 | `					/* Pathological empty name: keep the legacy diagnostic */` |
|    ! 0 |  413 | `					PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  414 | `						"$GLOBALS is a read-only array,insertion is forbidden");` |
|    ! 0 |  415 | `					rc = SXRET_OK;` |
|    ! 0 |  416 | `				}else{` |
|      4 |  417 | `					rc = PH7_VmInstallGlobalVar(&(*pVm),` |
|      2 |  418 | `						(const char *)SyBlobData(&pKey->sBlob),SyBlobLength(&pKey->sBlob),` |
|      2 |  419 | `						0,pTos->nIdx);` |
|      - |  420 | `				}` |
|      - |  421 | `			}` |
|      2 |  422 | `		}else{` |
|      - |  423 | `			/* Insertion by reference */` |
|     17 |  424 | `			rc = PH7_HashmapInsertByRef(pMap,pKey,pTos->nIdx);` |
|      - |  425 | `		}` |
|     10 |  426 | `	}else{` |
| 240613 |  427 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  428 | `	}` |
| 240631 |  429 | `	if( pKey ){` |
|  69155 |  430 | `		PH7_MemObjRelease(pKey);` |
|  34575 |  431 | `	}` |
|      - |  432 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  433 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  434 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 240631 |  435 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 240627 |  436 | `	VM_EXIT_BREAK;` |
|    ! 0 |  437 | `	VM_EXIT_BREAK;` |
| 120419 |  438 | `}` |
|      - |  439 |  |
|      - |  440 | `/*` |
|      - |  441 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  442 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  443 | ` */` |
|   1146 |  444 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  445 | `{` |
|   1151 |  446 | `	ph7_value *pTos = pState->pTos;` |
|   1151 |  447 | `	ph7_value *pStack = pState->pStack;` |
|   1151 |  448 | `	VmInstr *aInstr = pState->aInstr;` |
|   1151 |  449 | `	sxi32 pc = pState->pc;` |
|      - |  450 | `	sxi32 rc;` |
|    573 |  451 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1151 |  452 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  453 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  454 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  455 | `	 * plain anonymous function with no captured environment. */` |
|   1151 |  456 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  457 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  458 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  459 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  460 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  461 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1151 |  462 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1151 |  463 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  464 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  465 | `		ph7_vm_func *pClosure;` |
|      - |  466 | `		char *zName;` |
|      - |  467 | `		sxu32 mLen;` |
|      - |  468 | `		sxu32 n;` |
|      - |  469 | `		/* Create a new VM function */` |
|   1141 |  470 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  471 | `		/* Generate an unique closure name */` |
|   1141 |  472 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1141 |  473 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  474 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  475 | `			VM_EXIT_ABORT;` |
|      - |  476 | `		}` |
|   1141 |  477 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1141 |  478 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  479 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  480 | `		}` |
|      - |  481 | `		/* Zero the stucture */` |
|   1141 |  482 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  483 | `		/* Perform a structure assignment on read-only items */` |
|   1141 |  484 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1141 |  485 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1141 |  486 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1141 |  487 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  488 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  489 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1141 |  490 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1141 |  491 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1141 |  492 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1141 |  493 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1141 |  494 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1141 |  495 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1141 |  496 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1141 |  497 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1141 |  498 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1141 |  499 | `		if( pClosure->pUserData == 0 ){` |
|      - |  500 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  501 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  502 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1141 |  503 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    568 |  504 | `		}` |
|      - |  505 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  506 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  507 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  508 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1141 |  509 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  510 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  511 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1141 |  512 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1141 |  513 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1141 |  514 | `		pClosure->sFile = pFunc->sFile;` |
|   1141 |  515 | `		pClosure->nLine = pFunc->nLine;` |
|   1141 |  516 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1141 |  517 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  518 | `		/* Register the closure */` |
|   1141 |  519 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  520 | `		/* Set up closure environment */` |
|   1141 |  521 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1141 |  522 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2443 |  523 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  524 | `			ph7_value *pValue;` |
|   1307 |  525 | `			pEnv = &aEnv[n];` |
|   1307 |  526 | `			sEnv.sName  = pEnv->sName;` |
|   1307 |  527 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1307 |  528 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1307 |  529 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1302 |  530 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    704 |  531 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     48 |  532 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  533 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  534 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  535 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  536 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  537 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  538 | `				 * aliases the name to this slot instead of copying a value. */` |
|     70 |  539 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     70 |  540 | `				if( pValue ){` |
|     70 |  541 | `					sEnv.nIdx = pValue->nIdx;` |
|     70 |  542 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     34 |  543 | `				}` |
|     36 |  544 | `			}else{` |
|      - |  545 | `				/* Standard pass by value */` |
|   1239 |  546 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1239 |  547 | `				if( pValue ){` |
|      - |  548 | `					/* Copy imported value */` |
|    156 |  549 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|     76 |  550 | `				}` |
|      - |  551 | `			}` |
|      - |  552 | `			/* Insert the imported variable */` |
|   1307 |  553 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    656 |  554 | `		}` |
|   1141 |  555 | `		pTarget = pClosure;` |
|    568 |  556 | `	}` |
|      - |  557 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  558 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  559 | `	 * path when the closure is dispatched by name. */` |
|   1151 |  560 | `	pTos++;` |
|      - |  561 | `	{` |
|   1151 |  562 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1151 |  563 | `		if( pCloObj ){` |
|   1151 |  564 | `			pCloObj->iRef++;` |
|   1151 |  565 | `			pTos->x.pOther = pCloObj;` |
|   1151 |  566 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    578 |  567 | `		}else{` |
|      - |  568 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  569 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  570 | `		}` |
|      - |  571 | `	}` |
|   1151 |  572 | `	VM_EXIT_BREAK;` |
|    ! 0 |  573 | `	VM_EXIT_BREAK;` |
|    578 |  574 | `}` |
|      - |  575 |  |
|      - |  576 |  |
|      - |  577 | `/*` |
|      - |  578 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  579 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  580 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  581 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  582 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  583 | ` */` |
|     14 |  584 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  585 | `{` |
|     17 |  586 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  587 | `}` |
|      - |  588 | `/*` |
|      - |  589 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  590 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  591 | ` */` |
| 665430 |  592 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  593 | `{` |
| 665435 |  594 | `	ph7_value *pTos = pState->pTos;` |
| 665435 |  595 | `	ph7_value *pStack = pState->pStack;` |
| 665435 |  596 | `	VmInstr *aInstr = pState->aInstr;` |
| 665435 |  597 | `	sxi32 pc = pState->pc;` |
|      - |  598 | `	sxi32 rc;` |
| 333078 |  599 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 665435 |  600 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 665435 |  601 | `	ph7_hashmap *pMap = 0;` |
|      - |  602 | `	ph7_value *pIdx;` |
| 665435 |  603 | `	pIdx = 0;` |
| 665435 |  604 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  605 | `		if( !pInstr->iP2){` |
|      - |  606 | `			/* No available index,load NULL */` |
|    ! 0 |  607 | `			if( pTos >= pStack ){` |
|    ! 0 |  608 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  609 | `			}else{` |
|      - |  610 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  611 | `				pTos++;` |
|    ! 0 |  612 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  613 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  614 | `			}` |
|      - |  615 | `			/* Emit a notice */` |
|    ! 0 |  616 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  617 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  618 | `			VM_EXIT_BREAK;` |
|      - |  619 | `		}` |
|      2 |  620 | `	}else{` |
| 665433 |  621 | `		pIdx = pTos;` |
| 665433 |  622 | `		pTos--;` |
|      - |  623 | `	}` |
| 665435 |  624 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  625 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  626 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  627 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  628 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|    ! 0 |  629 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|    ! 0 |  630 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|    ! 0 |  631 | `		}` |
|    ! 0 |  632 | `		if( pIdx ){` |
|      - |  633 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  634 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|    ! 0 |  635 | `			PH7_MemObjRelease(pIdx);` |
|    ! 0 |  636 | `		}` |
|    ! 0 |  637 | `		PH7_MemObjRelease(pTos);` |
|    ! 0 |  638 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  639 | `		VM_EXIT_BREAK;` |
|      - |  640 | `	}` |
| 665435 |  641 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  642 | `		/* String access */` |
| 519413 |  643 | `		if( pIdx ){` |
|      - |  644 | `			sxi64 iOfft;` |
| 519413 |  645 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 519413 |  646 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  647 | `				/* Force an int cast */` |
|    ! 0 |  648 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  649 | `			}` |
| 519413 |  650 | `			iOfft = pIdx->x.iVal;` |
|      - |  651 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  652 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  653 | `			 * number, ran past the end and quietly produced NULL. */` |
| 519413 |  654 | `			if( iOfft < 0 ){` |
|      7 |  655 | `				iOfft += nLen;` |
|      3 |  656 | `			}` |
| 519417 |  657 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  658 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  659 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  660 | `				 * silently produced NULL in both cases). */` |
|      - |  661 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  662 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  663 | `				 * lookups and must stay silent. */` |
|     11 |  664 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  665 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  666 | `				PH7_MemObjRelease(pTos);` |
|     10 |  667 | `				if( bQuiet ){` |
|      8 |  668 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  669 | `				}else{` |
|      3 |  670 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  671 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  672 | `						pIdx->x.iVal);` |
|      - |  673 | `				}` |
|      6 |  674 | `			}else{` |
| 519405 |  675 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 519405 |  676 | `				int c = zData[iOfft];` |
| 519405 |  677 | `				PH7_MemObjRelease(pTos);` |
| 519405 |  678 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 519405 |  679 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  680 | `			}` |
| 260075 |  681 | `		}else{` |
|      - |  682 | `			/* No available index,load NULL */` |
|    ! 0 |  683 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  684 | `		}` |
| 519413 |  685 | `		VM_EXIT_BREAK;` |
|      - |  686 | `	}` |
| 146027 |  687 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  688 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  689 | `		 * iP2 codes:` |
|      - |  690 | `		 *   0 = read       → offsetGet` |
|      - |  691 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  692 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  693 | `		 *   4 = isset()    → offsetExists` |
|      - |  694 | `		 *   5 = unset()    → offsetUnset` |
|      - |  695 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  696 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  697 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  698 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  699 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  700 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  701 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  702 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  703 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  704 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  705 | `			ph7_class_method *pMeth;` |
|      - |  706 | `			ph7_value sResult;` |
|      - |  707 | `			ph7_value *apArg[1];` |
|    181 |  708 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  709 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  710 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  711 | `					"Cannot use [] for reading");` |
|    ! 0 |  712 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  713 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  714 | `				VM_EXIT_BREAK;` |
|      - |  715 | `			}` |
|    181 |  716 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  717 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  718 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  719 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  720 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  721 | `				apArg[0] = pIdx;` |
|     81 |  722 | `				if( pMeth ){` |
|     81 |  723 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  724 | `				}` |
|    143 |  725 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  726 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  727 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  728 | `				apArg[0] = pIdx;` |
|     20 |  729 | `				if( pMeth ){` |
|     20 |  730 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  731 | `				}` |
|     12 |  732 | `			}else{` |
|     89 |  733 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  734 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  735 | `				apArg[0] = pIdx;` |
|     89 |  736 | `				if( pMeth ){` |
|     89 |  737 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  738 | `				}` |
|      - |  739 | `			}` |
|    181 |  740 | `			if( pInstr->iP2 == 4 ){` |
|      - |  741 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  742 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  743 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  744 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  745 | `				PH7_MemObjRelease(pTos);` |
|     53 |  746 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  747 | `				if( bExists ){` |
|     28 |  748 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  749 | `					pTos->x.iVal = 1;` |
|     16 |  750 | `				}else{` |
|     29 |  751 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  752 | `				}` |
|    157 |  753 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  754 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  755 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  756 | `				PH7_MemObjRelease(pTos);` |
|     20 |  757 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  758 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  759 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  760 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  761 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  762 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  763 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  764 | `				 * coalesce takes the default, the real value on a hit. */` |
|     23 |  765 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     23 |  766 | `				PH7_MemObjRelease(&sResult);` |
|     23 |  767 | `				PH7_MemObjRelease(pTos);` |
|     23 |  768 | `				pTos->nIdx = SXU32_HIGH;` |
|     23 |  769 | `				if( !bExists ){` |
|      9 |  770 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      6 |  771 | `				}else{` |
|     17 |  772 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  773 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  774 | `					ph7_value sValue;` |
|     17 |  775 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     17 |  776 | `					apArg[0] = pIdx;` |
|     17 |  777 | `					if( pGet ){` |
|     17 |  778 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  779 | `					}` |
|     17 |  780 | `					PH7_MemObjStore(&sValue,pTos);` |
|     17 |  781 | `					PH7_MemObjRelease(&sValue);` |
|      - |  782 | `				}` |
|     23 |  783 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     23 |  784 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  785 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  786 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  787 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  788 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  789 | `				 *     and push NULL.` |
|      - |  790 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  791 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  792 | `				int bShouldArm = !bExists;` |
|      - |  793 | `				ph7_value sValue;` |
|     10 |  794 | `				PH7_MemObjRelease(&sResult);` |
|      - |  795 | `				/* Reset any prior arming defensively */` |
|     10 |  796 | `				VmCoalesceDisarm(pVm);` |
|     10 |  797 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  798 | `				if( bExists ){` |
|      5 |  799 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  800 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  801 | `					apArg[0] = pIdx;` |
|      5 |  802 | `					if( pGet ){` |
|      5 |  803 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  804 | `					}` |
|      5 |  805 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  806 | `						bShouldArm = 1;` |
|      1 |  807 | `					}` |
|      2 |  808 | `				}` |
|     10 |  809 | `				PH7_MemObjRelease(pTos);` |
|     10 |  810 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  811 | `				if( bShouldArm ){` |
|      - |  812 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  813 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  814 | `					 * intervening expression evaluation. */` |
|      8 |  815 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  816 | `					if( pIdx ){` |
|      8 |  817 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  818 | `					}` |
|      8 |  819 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  820 | `					pInst->iRef++;` |
|      8 |  821 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  822 | `				}else{` |
|      3 |  823 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  824 | `				}` |
|     10 |  825 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  826 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  827 | `				VM_EXIT_BREAK;` |
|    ! 0 |  828 | `			}else{` |
|      - |  829 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  830 | `				PH7_MemObjRelease(pTos);` |
|     89 |  831 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  832 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  833 | `			}` |
|    153 |  834 | `			PH7_MemObjRelease(&sResult);` |
|    153 |  835 | `			if( pIdx ){` |
|    153 |  836 | `				PH7_MemObjRelease(pIdx);` |
|     74 |  837 | `			}` |
|    153 |  838 | `			VM_EXIT_BREAK;` |
|      - |  839 | `		}` |
|      - |  840 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  841 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  842 | `		if( pInst ){` |
|      - |  843 | `			char zMsg[256];` |
|      3 |  844 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  845 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  846 | `				"Cannot use object of type %.*s as array",` |
|      2 |  847 | `				(int)pName->nByte,pName->zString);` |
|      3 |  848 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  849 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  850 | `			PH7_MemObjRelease(pTos);` |
|      3 |  851 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  852 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  853 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  854 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  855 | `			 * execution carried on inside the try block. */` |
|      3 |  856 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  857 | `		}` |
|    ! 0 |  858 | `	}` |
| 145849 |  859 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  860 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  861 | `			ph7_value *pObj;` |
|     23 |  862 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  863 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  864 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  865 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  866 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  867 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  868 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  869 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  870 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  871 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - |  872 | `				 * it is not a bool). */` |
|     23 |  873 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - |  874 | `					SyBlob sErrMsg;` |
|      7 |  875 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 |  876 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - |  877 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 |  878 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 |  879 | `					if( pIdx ){` |
|      7 |  880 | `						PH7_MemObjRelease(pIdx);` |
|      3 |  881 | `					}` |
|      7 |  882 | `					PH7_MemObjRelease(pTos);` |
|      7 |  883 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 |  884 | `					VM_EXIT_BREAK;` |
|      - |  885 | `				}` |
|     17 |  886 | `				PH7_MemObjToHashmap(pObj);` |
|     17 |  887 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 |  888 | `			}` |
|      8 |  889 | `		}` |
|      8 |  890 | `	}` |
| 145843 |  891 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - |  892 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - |  893 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - |  894 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
| 145838 |  895 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 145832 |  896 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  34041 |  897 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  51062 |  898 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  34036 |  899 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  34041 |  900 | `		if( bNull \|\| bLossyFloat ){` |
|      - |  901 | `			SyBlob sErrMsg;` |
|      5 |  902 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 |  903 | `			SyBlobAppend(&sErrMsg,` |
|      2 |  904 | `				bNull ? "Cannot access offset of type null on array"` |
|      - |  905 | `				      : "Cannot access offset of type float on array",` |
|      4 |  906 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - |  907 | `				                      : "Cannot access offset of type float on array"));` |
|      5 |  908 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 |  909 | `			PH7_MemObjRelease(pIdx);` |
|      5 |  910 | `			PH7_MemObjRelease(pTos);` |
|      5 |  911 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 |  912 | `			VM_EXIT_BREAK;` |
|      - |  913 | `		}` |
|  17016 |  914 | `	}` |
| 145839 |  915 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 145829 |  916 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - |  917 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - |  918 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - |  919 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - |  920 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - |  921 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - |  922 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1263 |  923 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    629 |  924 | `		}` |
|      - |  925 | `		/* Point to the hashmap */` |
| 145829 |  926 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 145829 |  927 | `		if( pIdx ){` |
|      - |  928 | `			/* Load the desired entry */` |
| 145827 |  929 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  72911 |  930 | `		}` |
| 145829 |  931 | `		if( pInstr->iP2 == 3 ){` |
|      - |  932 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - |  933 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - |  934 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - |  935 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - |  936 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - |  937 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - |  938 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - |  939 | `			 * correct for the outermost write. */` |
|     21 |  940 | `			int needWrite = (rc != SXRET_OK);` |
|     21 |  941 | `			if( !needWrite && pNode ){` |
|     13 |  942 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 |  943 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 |  944 | `					needWrite = 1;` |
|      3 |  945 | `				}` |
|      6 |  946 | `			}` |
|     21 |  947 | `			if( needWrite ){` |
|     15 |  948 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 |  949 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - |  950 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - |  951 | `					 * into the new map's storage. */` |
|      7 |  952 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 |  953 | `					if( pIdx ){` |
|      7 |  954 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 |  955 | `					}` |
|      3 |  956 | `				}` |
|      7 |  957 | `			}` |
|     10 |  958 | `		}` |
| 145829 |  959 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - |  960 | `			/* Create a new empty entry */` |
|    324 |  961 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 |  962 | `			if( rc == SXRET_OK ){` |
|      - |  963 | `				/* Point to the last inserted entry */` |
|    321 |  964 | `				pNode = pMap->pLast;` |
|    161 |  965 | `			}else{` |
|      - |  966 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - |  967 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - |  968 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - |  969 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 |  970 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - |  971 | `			}` |
|    160 |  972 | `		}` |
|  72911 |  973 | `	}` |
| 145832 |  974 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  39838 |  975 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      8 |  976 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - |  977 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - |  978 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - |  979 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - |  980 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - |  981 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - |  982 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - |  983 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - |  984 | `		 * STRING key quoted. */` |
|      - |  985 | `		SyBlob sMsg;` |
|      8 |  986 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 |  987 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - |  988 | `			SyString sKey;` |
|      6 |  989 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  990 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 |  991 | `			}` |
|      6 |  992 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      6 |  993 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      4 |  994 | `		}else{` |
|      3 |  995 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 |  996 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  997 | `			}` |
|      3 |  998 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - |  999 | `		}` |
|      8 | 1000 | `		SyBlobNullAppend(&sMsg);` |
|      8 | 1001 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 | 1002 | `		SyBlobRelease(&sMsg);` |
|      3 | 1003 | `	}` |
| 145838 | 1004 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  72927 | 1005 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1006 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1007 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1008 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1009 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1010 | `			VmArithTypeName(pTos));` |
|      3 | 1011 | `	}` |
| 145843 | 1012 | `	if( pIdx ){` |
| 145843 | 1013 | `		PH7_MemObjRelease(pIdx);` |
|  72919 | 1014 | `	}` |
| 145843 | 1015 | `	if( rc == SXRET_OK ){` |
|      - | 1016 | `		/* Load entry contents */` |
|  66179 | 1017 | `		if( pMap->iRef < 2 ){` |
|      - | 1018 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1019 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1020 | `			 */` |
|    101 | 1021 | `			pTos->nIdx = SXU32_HIGH;` |
|    101 | 1022 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     52 | 1023 | `		}else{` |
|  66081 | 1024 | `			pTos->nIdx = pNode->nValIdx;` |
|  66081 | 1025 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  66081 | 1026 | `			PH7_HashmapUnref(pMap);` |
|      - | 1027 | `		}` |
|  33092 | 1028 | `	}else{` |
|      - | 1029 | `		/* No such entry,load NULL */` |
|  79669 | 1030 | `		PH7_MemObjRelease(pTos);` |
|  79669 | 1031 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1032 | `	}` |
| 145843 | 1033 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1034 | `	VM_EXIT_BREAK;` |
| 333089 | 1035 | `}` |
|      - | 1036 |  |
|      - | 1037 | `/*` |
|      - | 1038 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1039 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1040 | ` */` |
|  73740 | 1041 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1042 | `{` |
|  73745 | 1043 | `	ph7_value *pTos = pState->pTos;` |
|  73745 | 1044 | `	ph7_value *pStack = pState->pStack;` |
|  73745 | 1045 | `	VmInstr *aInstr = pState->aInstr;` |
|  73745 | 1046 | `	sxi32 pc = pState->pc;` |
|      - | 1047 | `	sxi32 rc;` |
|  36870 | 1048 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1049 | `	ph7_hashmap *pMap;` |
|      - | 1050 | `	/* Allocate a new hashmap instance */` |
|  73745 | 1051 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73745 | 1052 | `	if( pMap == 0 ){` |
|    ! 0 | 1053 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1054 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1055 | `		VM_EXIT_ABORT;` |
|      - | 1056 | `	}` |
|  73745 | 1057 | `	if( pInstr->iP1 > 0 ){` |
|  10939 | 1058 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  10939 | 1059 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1060 | `		/* Perform the insertion */` |
|  41303 | 1061 | `		while( pEntry < pTos ){` |
|  30387 | 1062 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1063 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1064 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1065 | `				 * renumbered. Same routine that backs array_merge. */` |
|    683 | 1066 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1067 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1068 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1069 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1070 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1071 | `						 * map dangling. */` |
|    ! 0 | 1072 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1073 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1074 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1075 | `						break;` |
|      1 | 1076 | `					}` |
|    354 | 1077 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1078 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1079 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1080 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1081 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1082 | `						rcSpread = rcW;` |
|    ! 0 | 1083 | `						break;` |
|      - | 1084 | `					}` |
|      3 | 1085 | `				}else{` |
|      - | 1086 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     21 | 1087 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     21 | 1088 | `					break;` |
|      1 | 1089 | `				}` |
|  30038 | 1090 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1091 | `				/* Insertion by reference */` |
|    181 | 1092 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1093 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1094 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1095 | `					);` |
|     61 | 1096 | `			}else{` |
|      - | 1097 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1098 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1099 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1100 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1101 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1102 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  29587 | 1103 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1104 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11201 | 1105 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16800 | 1106 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11196 | 1107 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11201 | 1108 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1109 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1110 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1111 | `							SyBlob sErrMsg;` |
|      5 | 1112 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1113 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1114 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1115 | `						}` |
|   5598 | 1116 | `					}` |
|      - | 1117 | `				/* Standard insertion */` |
|  44378 | 1118 | `				PH7_HashmapInsert(pMap,` |
|  29582 | 1119 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  14791 | 1120 | `					&pEntry[1]` |
|      - | 1121 | `				);` |
|      - | 1122 | `			}` |
|      - | 1123 | `			/* Next pair on the stack */` |
|  30369 | 1124 | `			pEntry += 2;` |
|      5 | 1125 | `		}` |
|      - | 1126 | `		/* Pop P1 elements */` |
|  10939 | 1127 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  10939 | 1128 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1129 | `			/* Discard the partially-built map and propagate the exception. */` |
|     21 | 1130 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     21 | 1131 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1132 | `				VM_EXIT_ABORT;` |
|      - | 1133 | `			}` |
|      - | 1134 | `			{` |
|      - | 1135 | `				sxi32 iRp;` |
|     21 | 1136 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1137 | `					pc = iRp;` |
|      6 | 1138 | `					VM_EXIT_BREAK;` |
|      - | 1139 | `				}` |
|      - | 1140 | `			}` |
|     15 | 1141 | `			VM_EXIT_EXCEPTION;` |
|      - | 1142 | `		}` |
|   5458 | 1143 | `	}` |
|      - | 1144 | `	/* Push the hashmap */` |
|  73727 | 1145 | `	pTos++;` |
|  73727 | 1146 | `	pTos->nIdx = SXU32_HIGH;` |
|  73727 | 1147 | `	pTos->x.pOther = pMap;` |
|  73727 | 1148 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73727 | 1149 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1150 | `	VM_EXIT_BREAK;` |
|  36875 | 1151 | `}` |
|      - | 1152 |  |
|      - | 1153 | `/*` |
|      - | 1154 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1155 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1156 | ` */` |
|    264 | 1157 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1158 | `{` |
|    269 | 1159 | `	ph7_value *pTos = pState->pTos;` |
|    269 | 1160 | `	ph7_value *pStack = pState->pStack;` |
|    269 | 1161 | `	VmInstr *aInstr = pState->aInstr;` |
|    269 | 1162 | `	sxi32 pc = pState->pc;` |
|      - | 1163 | `	sxi32 rc;` |
|    132 | 1164 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1165 | `	ph7_value *pEntry;` |
|    269 | 1166 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1167 | `		/* Empty list,break immediately */` |
|    ! 0 | 1168 | `		VM_EXIT_BREAK;` |
|      - | 1169 | `	}` |
|    269 | 1170 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1171 | `#ifdef UNTRUST` |
|      - | 1172 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1173 | `		VM_EXIT_ABORT;` |
|      - | 1174 | `	}` |
|      - | 1175 | `#endif` |
|    269 | 1176 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1177 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1178 | `		ph7_hashmap_node *pNode;` |
|      - | 1179 | `		ph7_value sKey,*pObj;` |
|      - | 1180 | `		/* Start Copying */` |
|    265 | 1181 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1182 | `		while( pEntry <= pTos ){` |
|    551 | 1183 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1184 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1185 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1186 | `					if( rc == SXRET_OK ){` |
|      - | 1187 | `						/* Store node value */` |
|    523 | 1188 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1189 | `					}else{` |
|      - | 1190 | `						/* Undefined array key */` |
|      - | 1191 | `						char zMsg[128];` |
|    ! 0 | 1192 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1193 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1194 | `						PH7_MemObjRelease(pObj);` |
|      - | 1195 | `					}` |
|    259 | 1196 | `				}` |
|    259 | 1197 | `			}` |
|    551 | 1198 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1199 | `			pEntry++;` |
|      5 | 1200 | `		}` |
|    135 | 1201 | `	}else{` |
|      - | 1202 | `		/* Source is not an array */` |
|      - | 1203 | `		ph7_value *pObj;` |
|     13 | 1204 | `		while( pEntry <= pTos ){` |
|      9 | 1205 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|      9 | 1206 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|      9 | 1207 | `					PH7_MemObjRelease(pObj);` |
|      4 | 1208 | `				}` |
|      4 | 1209 | `			}` |
|      9 | 1210 | `			pEntry++;` |
|      1 | 1211 | `		}` |
|      5 | 1212 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1213 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|    ! 0 | 1214 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|    ! 0 | 1215 | `		}` |
|      - | 1216 | `	}` |
|    269 | 1217 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    269 | 1218 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1219 | `	VM_EXIT_BREAK;` |
|    137 | 1220 | `}` |
|      - | 1221 |  |
|      - | 1222 | `/*` |
|      - | 1223 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1224 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1225 | ` */` |
|   6788 | 1226 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1227 | `{` |
|   6793 | 1228 | `	ph7_value *pTos = pState->pTos;` |
|   6793 | 1229 | `	ph7_value *pStack = pState->pStack;` |
|   6793 | 1230 | `	VmInstr *aInstr = pState->aInstr;` |
|   6793 | 1231 | `	sxi32 pc = pState->pc;` |
|      - | 1232 | `	sxi32 rc;` |
|   3394 | 1233 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1234 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6793 | 1235 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6793 | 1236 | `	if( pName && pVm->pFrame ){` |
|      - | 1237 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1238 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1239 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6793 | 1240 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6793 | 1241 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6793 | 1242 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1243 | `			VM_EXIT_ABORT;` |
|      - | 1244 | `		}` |
|      - | 1245 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1246 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1247 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1248 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6791 | 1249 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1250 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1251 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1252 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1253 | `				VM_EXIT_ABORT;` |
|      - | 1254 | `			}` |
|      3 | 1255 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1256 | `		}` |
|   3392 | 1257 | `	}` |
|   6789 | 1258 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1259 | `	VM_EXIT_BREAK;` |
|   3399 | 1260 | `}` |
|      - | 1261 |  |
