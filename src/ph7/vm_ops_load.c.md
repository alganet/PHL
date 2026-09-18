# src/ph7/vm_ops_load.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 755/861 lines (87.69%)

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
|      4 |   29 | `{` |
|     56 |   30 | `	ph7_value *pTos = pState->pTos;` |
|     56 |   31 | `	ph7_value *pStack = pState->pStack;` |
|     56 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|     56 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|     26 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     56 |   36 | `	 SyString sName = { 0 , 0 };` |
|      - |   37 | `	 VmFrame *pFrameLocal;` |
|      - |   38 | `	SyHashEntry *pEntry;` |
|      - |   39 | `	sxu32 nIdx;` |
|      - |   40 | `#ifdef UNTRUST` |
|      - |   41 | `	if( pTos < pStack ){` |
|      - |   42 | `		VM_EXIT_ABORT;` |
|      - |   43 | `	}` |
|      - |   44 | `#endif` |
|     56 |   45 | `	if( pInstr->iP2 == 1 ){` |
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
|     46 |   94 | `	if( pInstr->p3 == 0 ){` |
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
|     46 |  111 | `		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|      - |  112 | `	}` |
|     46 |  113 | `	nIdx = pTos->nIdx;` |
|     46 |  114 | `	if(nIdx == SXU32_HIGH ){` |
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
|     46 |  131 | `	}else if( sName.nByte > 0){` |
|     46 |  132 | `		if( (pTos->iFlags & MEMOBJ_HASHMAP) && (pVm->pGlobal == (ph7_hashmap *)pTos->x.pOther) ){` |
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
|     30 |  159 | `}` |
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
| 226992 |  182 | `static int VmOffsetTypeRejected(ph7_vm *pVm,ph7_value *pKey,int iCtx,SyBlob *pMsg)` |
|      5 |  183 | `{` |
|      - |  184 | `	const char *zType;` |
| 226997 |  185 | `	SyString *pClass = 0;` |
| 226997 |  186 | `	if( pKey == 0 ){` |
|    ! 0 |  187 | `		return FALSE;` |
|      - |  188 | `	}` |
| 226997 |  189 | `	if( pKey->iFlags & MEMOBJ_OBJ ){` |
|     15 |  190 | `		ph7_class_instance *pInst = (ph7_class_instance *)pKey->x.pOther;` |
|     15 |  191 | `		if( pInst && pInst->pClass ){` |
|     15 |  192 | `			pClass = &pInst->pClass->sName;` |
|      7 |  193 | `		}` |
|     15 |  194 | `		zType = "object";` |
| 226990 |  195 | `	}else if( pKey->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  196 | `		zType = "array";` |
|      3 |  197 | `	}else{` |
| 226979 |  198 | `		return FALSE;` |
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
| 113501 |  217 | `}` |
|      - |  218 | `/*` |
|      - |  219 | ` * php's other half of the offset-type rules: a RESOURCE offset is accepted, with` |
|      - |  220 | `` * `Warning: Resource ID#N used as offset, casting to integer (N)`, and the key`` |
|      - |  221 | ` * becomes that integer. Rewrites pKey in place so the normal integer-key path` |
|      - |  222 | ` * takes over.` |
|      - |  223 | ` */` |
| 226974 |  224 | `static void VmOffsetResourceWarn(ph7_vm *pVm,ph7_value *pKey)` |
|      5 |  225 | `{` |
|      - |  226 | `	sxu32 nId;` |
| 226979 |  227 | `	if( pKey == 0 \|\| (pKey->iFlags & MEMOBJ_RES) == 0 ){` |
| 226975 |  228 | `		return;` |
|      - |  229 | `	}` |
|      5 |  230 | `	nId = PH7_VmResourceId(pVm,pKey->x.pOther);` |
|      7 |  231 | `	VmErrorFormat(&(*pVm),PH7_CTX_WARNING,` |
|      2 |  232 | `		"Resource ID#%u used as offset, casting to integer (%u)",nId,nId);` |
|      5 |  233 | `	PH7_MemObjRelease(pKey);` |
|      5 |  234 | `	pKey->x.iVal = (sxi64)nId;` |
|      5 |  235 | `	MemObjSetType(pKey,MEMOBJ_INT);` |
| 113492 |  236 | `}` |
|      - |  237 | `/*` |
|      - |  238 | ` * OP_STORE_IDX_REF: body moved verbatim from the OP_STORE_IDX_REF arm of` |
|      - |  239 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  240 | ` */` |
| 243262 |  241 | `PH7_PRIVATE VmOpRc VmExecOpStoreIdxRef(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  242 | `{` |
| 243267 |  243 | `	ph7_value *pTos = pState->pTos;` |
| 243267 |  244 | `	ph7_value *pStack = pState->pStack;` |
| 243267 |  245 | `	VmInstr *aInstr = pState->aInstr;` |
| 243267 |  246 | `	sxi32 pc = pState->pc;` |
|      - |  247 | `	sxi32 rc;` |
| 121631 |  248 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 243267 |  249 | `	ph7_hashmap *pMap = 0; /* cc  warning */` |
|      - |  250 | `	ph7_value *pKey;` |
|      - |  251 | `	sxu32 nIdx;` |
| 243267 |  252 | `	if( pInstr->iP1 ){` |
|      - |  253 | `		/* Key is next on stack */` |
|  69499 |  254 | `		pKey = pTos;` |
|  69499 |  255 | `		pTos--;` |
|  34752 |  256 | `	}else{` |
| 173773 |  257 | `		pKey = 0;` |
|      - |  258 | `	}` |
|      - |  259 | `		/* php only DEPRECATES a lossy-float / null write subscript (then truncates /` |
|      - |  260 | `		 * normalizes to ""); PHL rejects it. */` |
| 243267 |  261 | `		if( pKey && (pTos->iFlags & MEMOBJ_HASHMAP) ){` |
|  69301 |  262 | `			int bNull = (pKey->iFlags & MEMOBJ_NULL) != 0;` |
| 103952 |  263 | `			int bLossyFloat = (pKey->iFlags & MEMOBJ_REAL) != 0` |
|  69296 |  264 | `				&& pKey->rVal != (ph7_real)(sxi64)pKey->rVal;` |
|      - |  265 | `			SyBlob sTypeMsg;` |
|      - |  266 | `			/* An object/array key is php's TypeError; a resource key warns and` |
|      - |  267 | `			 * becomes its integer id. Both used to be stringified silently. */` |
|  69301 |  268 | `			if( VmOffsetTypeRejected(&(*pVm),pKey,0,&sTypeMsg) ){` |
|      - |  269 | `				sxi32 rcSc;` |
|      5 |  270 | `				PH7_MemObjRelease(pKey);` |
|      5 |  271 | `				VmPopOperand(&pTos,1);` |
|      5 |  272 | `				rcSc = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);` |
|      9 |  273 | `				if( rcSc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      5 |  274 | `				rc = rcSc;` |
|      5 |  275 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  276 | `			}` |
|  69297 |  277 | `			VmOffsetResourceWarn(&(*pVm),pKey);` |
|  69297 |  278 | `			if( bNull \|\| bLossyFloat ){` |
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
|  34644 |  289 | `		}` |
| 243259 |  290 | `	nIdx = pTos->nIdx;` |
|      - |  291 | `	{` |
|      - |  292 | `		/* ArrayAccess::offsetSet dispatch.` |
|      - |  293 | `		 * Container may be on the stack as MEMOBJ_OBJ, or referenced via` |
|      - |  294 | `		 * the backing variable slot at nIdx. */` |
| 243259 |  295 | `		ph7_class_instance *pInst = 0;` |
| 243259 |  296 | `		if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     89 |  297 | `			pInst = (ph7_class_instance *)pTos->x.pOther;` |
| 243216 |  298 | `		}else if( nIdx != SXU32_HIGH ){` |
| 243173 |  299 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 243173 |  300 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  301 | `				pInst = (ph7_class_instance *)pBacking->x.pOther;` |
|    ! 0 |  302 | `			}` |
| 121584 |  303 | `		}` |
| 243259 |  304 | `		if( pInst ){` |
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
| 243173 |  358 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
|      - |  359 | `		/* Hashmap already loaded on stack — COW separate the backing variable.` |
|      - |  360 | `		 * The stack holds a temporary ref (from LOAD), so undo it before` |
|      - |  361 | `		 * checking true sharing count, then re-add after separation. */` |
| 243037 |  362 | `		if( nIdx != SXU32_HIGH ){` |
| 243037 |  363 | `			ph7_value *pBacking = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
| 364553 |  364 | `			if( pBacking && (pBacking->iFlags & MEMOBJ_HASHMAP) ){` |
| 243037 |  365 | `				ph7_hashmap *pCur = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  366 | `				/* Only adjust refcount / perform COW if the backing variable` |
|      - |  367 | `				 * is still sharing the same hashmap instance. This mirrors` |
|      - |  368 | `				 * the guard used by PH7_OP_LOAD_IDX and avoids corrupting` |
|      - |  369 | `				 * refcounts if the backing array was already separated. */` |
| 243037 |  370 | `				if( pBacking->x.pOther == (void *)pCur ){` |
| 243037 |  371 | `					pCur->iRef--;  /* Undo stack ref to reveal true sharing count */` |
| 243037 |  372 | `					pMap = PH7_HashmapCowSeparate(&(*pVm),pBacking);` |
| 243037 |  373 | `					pMap->iRef++;  /* Re-add stack ref */` |
| 243037 |  374 | `					pTos->x.pOther = pMap;` |
| 121521 |  375 | `				}else{` |
|      - |  376 | `					/* Backing variable no longer points at pCur: skip COW here` |
|      - |  377 | `					 * and operate on the hashmap currently on the stack. */` |
|    ! 0 |  378 | `					pMap = pCur;` |
|      - |  379 | `				}` |
| 121521 |  380 | `			}else{` |
|    ! 0 |  381 | `				pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  382 | `			}` |
| 121521 |  383 | `		}else{` |
|    ! 0 |  384 | `			pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      - |  385 | `		}` |
| 243037 |  386 | `		if( pMap->iRef < 2 ){` |
|      - |  387 | `			/* TICKET 1433-48: Prevent garbage collection during insertion.` |
|      - |  388 | `			 * This inflation is safe with COW: VmPopOperand below will call` |
|      - |  389 | `			 * PH7_HashmapUnref, bringing iRef back down. Between here and there,` |
|      - |  390 | `			 * no code checks iRef for COW decisions. */` |
|    ! 0 |  391 | `			pMap->iRef = 2;` |
|    ! 0 |  392 | `		}` |
| 121521 |  393 | `	}else{` |
|      - |  394 | `		ph7_value *pObj;` |
|    138 |  395 | `		pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);` |
|    138 |  396 | `		if( pObj == 0 ){` |
|    ! 0 |  397 | `			if( pKey ){` |
|    ! 0 |  398 | `			  PH7_MemObjRelease(pKey);` |
|    ! 0 |  399 | `			}` |
|    ! 0 |  400 | `			VmPopOperand(&pTos,1);` |
|    ! 0 |  401 | `			VM_EXIT_BREAK;` |
|      - |  402 | `		}` |
|      - |  403 | `		/* Phase#1: Load the array */` |
|    138 |  404 | `		if( (pObj->iFlags & MEMOBJ_STRING) && (pInstr->iOp != PH7_OP_STORE_IDX_REF) ){` |
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
|     32 |  467 | `		}else if( (pObj->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  468 | `			/* php: only NULL and FALSE auto-vivify into an array. Writing an index into an` |
|      - |  469 | `			 * int/float/resource/TRUE is a catchable Error -- PH7 quietly REPLACED the value` |
|      - |  470 | `			 * with an array, destroying it ($x = 5; $x[0] = 1; left $x === [1]). */` |
|      - |  471 | `			/* php auto-vivifies false into an array with an 8.1 DEPRECATION; PHL` |
|      - |  472 | `			 * rejects any scalar base, false included (null still auto-vivifies). */` |
|     32 |  473 | `			int bScalar = (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0;` |
|     32 |  474 | `			if( bScalar ){` |
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
|     26 |  487 | `			rc = PH7_MemObjToHashmap(pObj);` |
|     26 |  488 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  489 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while creating a new array");` |
|    ! 0 |  490 | `				VM_EXIT_ABORT;` |
|      - |  491 | `			}` |
|     12 |  492 | `		}` |
|      - |  493 | `		/* COW separate the backing variable before mutation */` |
|     26 |  494 | `		pMap = PH7_HashmapCowSeparate(&(*pVm),pObj);` |
|      - |  495 | `	}` |
| 243061 |  496 | `	VmPopOperand(&pTos,1);` |
|      - |  497 | `	/* Phase#2: Perform the insertion */` |
| 243061 |  498 | `	if( pInstr->iOp == PH7_OP_STORE_IDX_REF && pTos->nIdx != SXU32_HIGH ){` |
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
| 243043 |  524 | `		rc = PH7_HashmapInsert(pMap,pKey,pTos);` |
|      - |  525 | `	}` |
| 243061 |  526 | `	if( pKey ){` |
|  69305 |  527 | `		PH7_MemObjRelease(pKey);` |
|  34650 |  528 | `	}` |
|      - |  529 | `	/* An append onto the occupied saturated auto-index threw php's catchable` |
|      - |  530 | `	 * Error (PH7_VmThrowArrayNextIndexError) — dispatch it like any other` |
|      - |  531 | `	 * store-path throw. Plain failures (OOM) keep their existing routes. */` |
| 243061 |  532 | `	PH7_DISPATCH_ENFORCE_RC(rc)` |
| 243057 |  533 | `	VM_EXIT_BREAK;` |
|    ! 0 |  534 | `	VM_EXIT_BREAK;` |
| 121636 |  535 | `}` |
|      - |  536 |  |
|      - |  537 | `/*` |
|      - |  538 | ` * OP_LOAD_CLOSURE: body moved verbatim from the OP_LOAD_CLOSURE arm of` |
|      - |  539 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  540 | ` */` |
|   1218 |  541 | `PH7_PRIVATE VmOpRc VmExecOpLoadClosure(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  542 | `{` |
|   1223 |  543 | `	ph7_value *pTos = pState->pTos;` |
|   1223 |  544 | `	ph7_value *pStack = pState->pStack;` |
|   1223 |  545 | `	VmInstr *aInstr = pState->aInstr;` |
|   1223 |  546 | `	sxi32 pc = pState->pc;` |
|      - |  547 | `	sxi32 rc;` |
|    609 |  548 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   1223 |  549 | `	ph7_vm_func *pFunc = (ph7_vm_func *)pInstr->p3;` |
|      - |  550 | `	/* The function whose name the Closure object will wrap: a fresh per-instantiation` |
|      - |  551 | `	 * copy for a real closure (built below), or the shared lambda function itself for a` |
|      - |  552 | `	 * plain anonymous function with no captured environment. */` |
|   1223 |  553 | `	ph7_vm_func *pTarget = pFunc;` |
|      - |  554 | `	/* A no-capture lambda declared inside a class method is not VM_FUNC_CLOSURE` |
|      - |  555 | `	 * (its env is empty), yet php still binds the creation-site class as its scope` |
|      - |  556 | ``	 * so `self::`/private access inside the body works. Detect the enclosing class`` |
|      - |  557 | `	 * here and route such a lambda through the per-instance closure path (a global` |
|      - |  558 | `	 * no-capture lambda peeks NULL and stays a shared function). */` |
|   1223 |  559 | `	ph7_class *pLoadScope = (pFunc->iFlags & VM_FUNC_CLOSURE) ? 0 : PH7_VmPeekDeclaringClass(pVm);` |
|   1223 |  560 | `	if( (pFunc->iFlags & VM_FUNC_CLOSURE) \|\| pLoadScope ){` |
|      - |  561 | `		ph7_vm_func_closure_env *aEnv,*pEnv,sEnv;` |
|      - |  562 | `		ph7_vm_func *pClosure;` |
|      - |  563 | `		char *zName;` |
|      - |  564 | `		sxu32 mLen;` |
|      - |  565 | `		sxu32 n;` |
|      - |  566 | `		/* Create a new VM function */` |
|   1213 |  567 | `		pClosure = (ph7_vm_func *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(ph7_vm_func));` |
|      - |  568 | `		/* Generate an unique closure name */` |
|   1213 |  569 | `		zName = (char *)SyMemBackendAlloc(&pVm->sAllocator,sizeof("[closure_]")+64);` |
|   1213 |  570 | `		if( pClosure == 0 \|\| zName == 0){` |
|    ! 0 |  571 | `			PH7_VmThrowError(pVm,0,E_ERROR,"Fatal: PH7 is running out of memory while creating closure environment");` |
|    ! 0 |  572 | `			VM_EXIT_ABORT;` |
|      - |  573 | `		}` |
|   1213 |  574 | `		mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|   1213 |  575 | `		while( SyHashGet(&pVm->hFunction,zName,mLen) != 0 && mLen < (sizeof("[closure_]")+60/* not 64 */) ){` |
|    ! 0 |  576 | `			mLen = SyBufferFormat(zName,sizeof("[closure_]")+64,"[closure_%d]",pVm->closure_cnt++);` |
|    ! 0 |  577 | `		}` |
|      - |  578 | `		/* Zero the stucture */` |
|   1213 |  579 | `		SyZero(pClosure,sizeof(ph7_vm_func));` |
|      - |  580 | `		/* Perform a structure assignment on read-only items */` |
|   1213 |  581 | `		pClosure->aArgs = pFunc->aArgs;` |
|   1213 |  582 | `		pClosure->aByteCode = pFunc->aByteCode;` |
|   1213 |  583 | `		pClosure->aStatic = pFunc->aStatic;` |
|   1213 |  584 | `		pClosure->iFlags = pFunc->iFlags;` |
|      - |  585 | `		/* An in-class no-capture lambda routed here (pLoadScope set) must be a` |
|      - |  586 | `		 * real closure so PH7_VmClassMemberAccess honors its pUserData scope. */` |
|   1213 |  587 | `		pClosure->iFlags \|= VM_FUNC_CLOSURE;` |
|   1213 |  588 | `		pClosure->pUserData = pFunc->pUserData;` |
|   1213 |  589 | `		pClosure->sSignature = pFunc->sSignature;` |
|   1213 |  590 | `		pClosure->nReturnType = pFunc->nReturnType;` |
|   1213 |  591 | `		pClosure->sReturnClass = pFunc->sReturnClass;` |
|   1213 |  592 | `		pClosure->aReturnUnion = pFunc->aReturnUnion;` |
|   1213 |  593 | `		pClosure->sReturnTypeName = pFunc->sReturnTypeName;` |
|   1213 |  594 | `		pClosure->bStrictTypes = pFunc->bStrictTypes;` |
|   1213 |  595 | `		pClosure->nMaxStack = pFunc->nMaxStack;` |
|   1213 |  596 | `		if( pClosure->pUserData == 0 ){` |
|      - |  597 | `			/* Stamp the creation-site class scope (see PH7_VmPeekDeclaringClass):` |
|      - |  598 | `			 * a closure made in a method — or in another closure, whose own stamp` |
|      - |  599 | `			 * the peek reads — resolves self::/parent:: against it like php. */` |
|   1213 |  600 | `			pClosure->pUserData = (void *)PH7_VmPeekDeclaringClass(pVm);` |
|    604 |  601 | `		}` |
|      - |  602 | ``		/* Capture the creation-site late-static-binding class so `static::` inside`` |
|      - |  603 | `		 * the closure body resolves like php (the "called class", which may differ` |
|      - |  604 | `		 * from the declaring scope stamped above — e.g. a closure made in an` |
|      - |  605 | `		 * inherited method). A closure made outside any class captures NULL. */` |
|   1213 |  606 | `		pClosure->pLsbClass = (void *)PH7_VmPeekTopClass(pVm);` |
|      - |  607 | `		/* Reflection descriptor fields (getDocComment/getAttributes/getStartLine/` |
|      - |  608 | `		 * getEndLine/getFileName read the INSTANTIATED copy via $__fn) */` |
|   1213 |  609 | `		pClosure->aAttrs = pFunc->aAttrs;` |
|   1213 |  610 | `		pClosure->sDoc = pFunc->sDoc;` |
|   1213 |  611 | `		pClosure->sFile = pFunc->sFile;` |
|   1213 |  612 | `		pClosure->nLine = pFunc->nLine;` |
|   1213 |  613 | `		pClosure->nEndLine = pFunc->nEndLine;` |
|   1213 |  614 | `		SyStringInitFromBuf(&pClosure->sName,zName,mLen);` |
|      - |  615 | `		/* Register the closure */` |
|   1213 |  616 | `		PH7_VmInstallUserFunction(pVm,pClosure,0);` |
|      - |  617 | `		/* Set up closure environment */` |
|   1213 |  618 | `		SySetInit(&pClosure->aClosureEnv,&pVm->sAllocator,sizeof(ph7_vm_func_closure_env));` |
|   1213 |  619 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|   2665 |  620 | `		for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; ++n ){` |
|      - |  621 | `			ph7_value *pValue;` |
|   1457 |  622 | `			pEnv = &aEnv[n];` |
|   1457 |  623 | `			sEnv.sName  = pEnv->sName;` |
|   1457 |  624 | `			sEnv.iFlags = pEnv->iFlags;` |
|   1457 |  625 | `			sEnv.nLine = pEnv->nLine;` |
|   1457 |  626 | `			sEnv.nIdx = SXU32_HIGH;` |
|   1457 |  627 | `			PH7_MemObjInit(pVm,&sEnv.sValue);` |
|   1452 |  628 | `			if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == VM_FUNC_ARG_BY_REF` |
|    795 |  629 | `			 && !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     64 |  630 | `				&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|      - |  631 | `				/* Capture by reference: bind the env entry to the variable's` |
|      - |  632 | `				 * memory slot — creating a fresh null variable when missing,` |
|      - |  633 | ``				 * as php does (`use (&$f)` before $f is assigned) — and pin`` |
|      - |  634 | `				 * the slot past the creating frame's teardown so the closure` |
|      - |  635 | `				 * can outlive its birth scope. The call-time env install` |
|      - |  636 | `				 * aliases the name to this slot instead of copying a value. */` |
|     97 |  637 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,TRUE);` |
|     97 |  638 | `				if( pValue ){` |
|     97 |  639 | `					sEnv.nIdx = pValue->nIdx;` |
|     97 |  640 | `					VmPinMemObjSlot(pVm,pValue->nIdx);` |
|     47 |  641 | `				}` |
|     50 |  642 | `			}else{` |
|      - |  643 | `				/* Standard pass by value */` |
|   1363 |  644 | `				pValue = VmExtractMemObj(pVm,&sEnv.sName,FALSE,FALSE);` |
|   1363 |  645 | `				if( pValue ){` |
|      - |  646 | `					/* Copy imported value */` |
|    187 |  647 | `					PH7_MemObjStore(pValue,&sEnv.sValue);` |
|   1272 |  648 | `				}else if( (sEnv.iFlags & (VM_FUNC_ARG_BY_REF\|VM_FUNC_ARG_IGNORE)) == 0` |
|    609 |  649 | `					&& !(SyStringLength(&sEnv.sName) == sizeof("this")-1` |
|     16 |  650 | `						&& SyMemcmp(SyStringData(&sEnv.sName),"this",sizeof("this")-1) == 0) ){` |
|     33 |  651 | `					if( pFunc->iFlags & VM_FUNC_ARROW ){` |
|      - |  652 | `						/* An arrow function auto-captures free variables by value, but` |
|      - |  653 | `						 * php does NOT capture one that is UNDEFINED at creation: the` |
|      - |  654 | `						 * isolated body scope then simply has no such variable, so a` |
|      - |  655 | `						 * read of it there raises the normal "Undefined variable"` |
|      - |  656 | `						 * warning (and reflection's getClosureUsedVariables omits it).` |
|      - |  657 | `						 * Skip installing the capture so the body READ — not the` |
|      - |  658 | `						 * creation — warns, matching php. (A later assignment to the` |
|      - |  659 | `						 * outer variable does not retro-capture: arrow scope is` |
|      - |  660 | `						 * isolated.) An explicit by-value use() instead warns here and` |
|      - |  661 | `						 * binds NULL, handled just below. */` |
|     23 |  662 | `						continue;` |
|      - |  663 | `					}` |
|      - |  664 | ``					/* php reads a by-value `use ($q)` capture AT CLOSURE CREATION and`` |
|      - |  665 | `					 * warns when the variable is undefined there (the by-ref form` |
|      - |  666 | ``					 * `use (&$q)` above stays silent — it creates the binding). The`` |
|      - |  667 | `					 * auto-injected $this (VM_FUNC_ARG_IGNORE) never warns. The` |
|      - |  668 | `					 * capture still proceeds as NULL, as php does. php attributes the` |
|      - |  669 | `					 * warning to the capture's own line (which can differ from the` |
|      - |  670 | `					 * OP_LOAD_CLOSURE instruction line when the use-clause wraps), so` |
|      - |  671 | `					 * borrow the recorded line for the emission and restore it. */` |
|     11 |  672 | `					sxu32 nSavedLine = pVm->nCurLine;` |
|     11 |  673 | `					if( sEnv.nLine ){` |
|     11 |  674 | `						pVm->nCurLine = sEnv.nLine;` |
|      5 |  675 | `					}` |
|     11 |  676 | `					VmErrorFormat(pVm,PH7_CTX_WARNING,"Undefined variable $%z",&sEnv.sName);` |
|     11 |  677 | `					pVm->nCurLine = nSavedLine;` |
|      5 |  678 | `				}` |
|      - |  679 | `			}` |
|      - |  680 | `			/* Insert the imported variable */` |
|   1437 |  681 | `			SySetPut(&pClosure->aClosureEnv,(const void *)&sEnv);` |
|    721 |  682 | `		}` |
|   1213 |  683 | `		pTarget = pClosure;` |
|    604 |  684 | `	}` |
|      - |  685 | `	/* Wrap the target function in a Closure object and push it. Its captured environment` |
|      - |  686 | ``	 * (incl. any `$this`) stays in pTarget->aClosureEnv and is delivered by the normal call`` |
|      - |  687 | `	 * path when the closure is dispatched by name. */` |
|   1223 |  688 | `	pTos++;` |
|      - |  689 | `	{` |
|   1223 |  690 | `		ph7_class_instance *pCloObj = VmCreateClosure(pVm, &pTarget->sName, 0, 0);` |
|   1223 |  691 | `		if( pCloObj ){` |
|   1223 |  692 | `			pCloObj->iRef++;` |
|   1223 |  693 | `			pTos->x.pOther = pCloObj;` |
|   1223 |  694 | `			MemObjSetType(pTos, MEMOBJ_OBJ);` |
|    614 |  695 | `		}else{` |
|      - |  696 | `			/* OOM fallback: the name string is still a usable callable. */` |
|    ! 0 |  697 | `			PH7_MemObjStringAppend(pTos, pTarget->sName.zString, pTarget->sName.nByte);` |
|      - |  698 | `		}` |
|      - |  699 | `	}` |
|   1223 |  700 | `	VM_EXIT_BREAK;` |
|    ! 0 |  701 | `	VM_EXIT_BREAK;` |
|    614 |  702 | `}` |
|      - |  703 |  |
|      - |  704 |  |
|      - |  705 | `/*` |
|      - |  706 | `` * TRUE when this LOAD_IDX feeds a `??`: the coalesce test is the very next`` |
|      - |  707 | `` * instruction. php evaluates the whole left operand of `??` in isset-context,`` |
|      - |  708 | ` * so the access must stay SILENT and, when it misses, must yield NULL — an` |
|      - |  709 | ``  * out-of-range string offset that yielded "" instead made `$s[99] ?? $d` `` |
|      - |  710 | ` * evaluate to "" rather than $d, a wrong answer rather than a stray notice.` |
|      - |  711 | ` */` |
|     14 |  712 | `static int VmIdxFeedsCoalesce(const VmInstr *pInstr)` |
|      3 |  713 | `{` |
|     17 |  714 | `	return (pInstr+1)->iOp == PH7_OP_NULLC \|\| (pInstr+1)->iOp == PH7_OP_NULLC_JMP;` |
|      3 |  715 | `}` |
|      - |  716 | `/*` |
|      - |  717 | ` * OP_LOAD_IDX: body moved verbatim from the OP_LOAD_IDX arm of` |
|      - |  718 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  719 | ` */` |
| 679483 |  720 | `PH7_PRIVATE VmOpRc VmExecOpLoadIdx(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  721 | `{` |
| 679488 |  722 | `	ph7_value *pTos = pState->pTos;` |
| 679488 |  723 | `	ph7_value *pStack = pState->pStack;` |
| 679488 |  724 | `	VmInstr *aInstr = pState->aInstr;` |
| 679488 |  725 | `	sxi32 pc = pState->pc;` |
|      - |  726 | `	sxi32 rc;` |
| 340115 |  727 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 679488 |  728 | `	ph7_hashmap_node *pNode = 0; /* cc warning */` |
| 679488 |  729 | `	ph7_hashmap *pMap = 0;` |
|      - |  730 | `	ph7_value *pIdx;` |
| 679488 |  731 | `	pIdx = 0;` |
| 679488 |  732 | `	if( pInstr->iP1 == 0 ){` |
|      3 |  733 | `		if( !pInstr->iP2){` |
|      - |  734 | `			/* No available index,load NULL */` |
|    ! 0 |  735 | `			if( pTos >= pStack ){` |
|    ! 0 |  736 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  737 | `			}else{` |
|      - |  738 | `				/* TICKET 1433-020: Empty stack */` |
|    ! 0 |  739 | `				pTos++;` |
|    ! 0 |  740 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    ! 0 |  741 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  742 | `			}` |
|      - |  743 | `			/* Emit a notice */` |
|    ! 0 |  744 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_NOTICE,` |
|      - |  745 | `				"Array: Attempt to access an undefined index,PH7 is loading NULL");` |
|    ! 0 |  746 | `			VM_EXIT_BREAK;` |
|      - |  747 | `		}` |
|      2 |  748 | `	}else{` |
| 679486 |  749 | `		pIdx = pTos;` |
| 679486 |  750 | `		pTos--;` |
|      - |  751 | `	}` |
| 679488 |  752 | `	if( pInstr->iP2 == 7 && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|      - |  753 | ``		/* Keyed list destructuring `["k"=>$v] = $src` from a NON-array source: yield NULL`` |
|      - |  754 | `		 * (never char-index a string), warning once per key — matching PHP, which warns per` |
|      - |  755 | `		 * key here. A NULL source is silent; unlike the positional OP_LOAD_LIST path, a bool` |
|      - |  756 | `		 * source DOES warn (PHP warns for bool in keyed destructuring). */` |
|      7 |  757 | `		if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  758 | `			VmWarnCannotUseAsArray(&(*pVm),pTos->iFlags);` |
|      2 |  759 | `		}` |
|      7 |  760 | `		if( pIdx ){` |
|      - |  761 | `			/* Release the key (a string literal for keyed destructuring), like the` |
|      - |  762 | `			 * normal hashmap-read exit below — otherwise its blob is orphaned. */` |
|      7 |  763 | `			PH7_MemObjRelease(pIdx);` |
|      3 |  764 | `		}` |
|      7 |  765 | `		PH7_MemObjRelease(pTos);` |
|      7 |  766 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      7 |  767 | `		VM_EXIT_BREAK;` |
|      - |  768 | `	}` |
| 679482 |  769 | `	if( pTos->iFlags & MEMOBJ_STRING ){` |
|      - |  770 | `		/* String access */` |
| 532804 |  771 | `		if( pIdx ){` |
|      - |  772 | `			sxi64 iOfft;` |
| 532804 |  773 | `			sxi64 nLen = (sxi64)SyBlobLength(&pTos->sBlob);` |
| 532804 |  774 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|      - |  775 | `				/* Force an int cast */` |
|    ! 0 |  776 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 |  777 | `			}` |
| 532804 |  778 | `			iOfft = pIdx->x.iVal;` |
|      - |  779 | `			/* php 7.1: a NEGATIVE offset counts back from the end ($s[-1] is the last` |
|      - |  780 | `			 * character). The offset used to be cast to UNSIGNED, so -1 became a huge` |
|      - |  781 | `			 * number, ran past the end and quietly produced NULL. */` |
| 532804 |  782 | `			if( iOfft < 0 ){` |
|      7 |  783 | `				iOfft += nLen;` |
|      3 |  784 | `			}` |
| 532808 |  785 | `			if( iOfft < 0 \|\| iOfft >= nLen ){` |
|      - |  786 | `				/* Out of range. In an isset()/empty() lookup php answers FALSE, so load` |
|      - |  787 | `				 * NULL there; everywhere else it WARNS and yields the empty string (PH7` |
|      - |  788 | `				 * silently produced NULL in both cases). */` |
|      - |  789 | `				/* LOAD_IDX carries its OWN iP2 codes, which do NOT line up with the` |
|      - |  790 | `				 * PH7_MEMBER_* ones: 4 = isset, 5 = unset, 6 = empty. All three are` |
|      - |  791 | `				 * lookups and must stay silent. */` |
|     11 |  792 | `				int bQuiet = pInstr->iP2 == 4 \|\| pInstr->iP2 == 5 \|\| pInstr->iP2 == 6` |
|     11 |  793 | `					\|\| pInstr->iP2 == 8 \|\| VmIdxFeedsCoalesce(pInstr);` |
|     10 |  794 | `				PH7_MemObjRelease(pTos);` |
|     10 |  795 | `				if( bQuiet ){` |
|      8 |  796 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  797 | `				}else{` |
|      3 |  798 | `					MemObjSetType(pTos,MEMOBJ_STRING);` |
|      4 |  799 | `					VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Uninitialized string offset %qd",` |
|      1 |  800 | `						pIdx->x.iVal);` |
|      - |  801 | `				}` |
|      6 |  802 | `			}else{` |
| 532796 |  803 | `				const char *zData = (const char *)SyBlobData(&pTos->sBlob);` |
| 532796 |  804 | `				int c = zData[iOfft];` |
| 532796 |  805 | `				PH7_MemObjRelease(pTos);` |
| 532796 |  806 | `				MemObjSetType(pTos,MEMOBJ_STRING);` |
| 532796 |  807 | `				SyBlobAppend(&pTos->sBlob,(const void *)&c,sizeof(char));` |
|      - |  808 | `			}` |
| 266781 |  809 | `		}else{` |
|      - |  810 | `			/* No available index,load NULL */` |
|    ! 0 |  811 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      - |  812 | `		}` |
| 532804 |  813 | `		VM_EXIT_BREAK;` |
|      - |  814 | `	}` |
| 146683 |  815 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|      - |  816 | `		/* Object subscript: ArrayAccess dispatch.` |
|      - |  817 | `		 * iP2 codes:` |
|      - |  818 | `		 *   0 = read       → offsetGet` |
|      - |  819 | `		 *   3 = ??= peek   → offsetExists; offsetGet on hit; arm coalesce` |
|      - |  820 | `		 *                    target on miss for the upcoming NULLC_STORE` |
|      - |  821 | `		 *   4 = isset()    → offsetExists` |
|      - |  822 | `		 *   5 = unset()    → offsetUnset` |
|      - |  823 | `		 *   6 = empty()    → offsetExists, then offsetGet on hit` |
|      - |  824 | ``		 *   8 = `??` read  → same probe as 6 (offsetExists, then offsetGet on a`` |
|      - |  825 | `		 *                    hit) and no diagnostics anywhere in this op: php` |
|      - |  826 | ``		 *                    evaluates the whole left operand of `??` in`` |
|      - |  827 | `		 *                    isset-context, and calling offsetGet blindly also` |
|      - |  828 | `		 *                    surfaced warnings raised INSIDE a userland` |
|      - |  829 | `		 *                    offsetGet (e.g. ArrayObject's own array read). */` |
|    183 |  830 | `		ph7_class_instance *pInst = (ph7_class_instance *)pTos->x.pOther;` |
|    183 |  831 | `		ph7_class *pArrayAccess = pVm->pArrayAccessClass;` |
|    183 |  832 | `		if( pArrayAccess && pInst && PH7_VmInstanceOf(pInst->pClass,pArrayAccess) ){` |
|      - |  833 | `			ph7_class_method *pMeth;` |
|      - |  834 | `			ph7_value sResult;` |
|      - |  835 | `			ph7_value *apArg[1];` |
|    181 |  836 | `			if( (pInstr->iP2 == 0 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8) && pIdx == 0 ){` |
|      - |  837 | ``				/* `$obj[]` read — PHP rejects this. */`` |
|    ! 0 |  838 | `				PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,` |
|      - |  839 | `					"Cannot use [] for reading");` |
|    ! 0 |  840 | `				PH7_MemObjRelease(pTos);` |
|    ! 0 |  841 | `				pTos->nIdx = SXU32_HIGH;` |
|    ! 0 |  842 | `				VM_EXIT_BREAK;` |
|      - |  843 | `			}` |
|    181 |  844 | `			PH7_MemObjInit(&(*pVm),&sResult);` |
|    181 |  845 | `			if( pInstr->iP2 == 4 \|\| pInstr->iP2 == 6 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 8 ){` |
|      - |  846 | ``				/* isset, empty, ??= and `??` all start with offsetExists. */`` |
|     81 |  847 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  848 | `					"offsetExists",sizeof("offsetExists")-1);` |
|     81 |  849 | `				apArg[0] = pIdx;` |
|     81 |  850 | `				if( pMeth ){` |
|     81 |  851 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     43 |  852 | `				}` |
|    143 |  853 | `			}else if( pInstr->iP2 == 5 ){` |
|     20 |  854 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  855 | `					"offsetUnset",sizeof("offsetUnset")-1);` |
|     20 |  856 | `				apArg[0] = pIdx;` |
|     20 |  857 | `				if( pMeth ){` |
|     20 |  858 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|      8 |  859 | `				}` |
|     12 |  860 | `			}else{` |
|     89 |  861 | `				pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  862 | `					"offsetGet",sizeof("offsetGet")-1);` |
|     89 |  863 | `				apArg[0] = pIdx;` |
|     89 |  864 | `				if( pMeth ){` |
|     89 |  865 | `					PH7_VmCallClassMethod(&(*pVm),pInst,pMeth,&sResult,pIdx ? 1 : 0,apArg);` |
|     42 |  866 | `				}` |
|      - |  867 | `			}` |
|    181 |  868 | `			if( pInstr->iP2 == 4 ){` |
|      - |  869 | `				/* isset: push MEMOBJ_BOOL so vm_builtin_isset reports the` |
|      - |  870 | `				 * right truth value AND skips its "Expecting a variable not` |
|      - |  871 | `				 * a constant" warning (keyed on MEMOBJ_BOOL). */` |
|     53 |  872 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     53 |  873 | `				PH7_MemObjRelease(pTos);` |
|     53 |  874 | `				pTos->nIdx = SXU32_HIGH;` |
|     53 |  875 | `				if( bExists ){` |
|     28 |  876 | `					MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     28 |  877 | `					pTos->x.iVal = 1;` |
|     16 |  878 | `				}else{` |
|     29 |  879 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  880 | `				}` |
|    157 |  881 | `			}else if( pInstr->iP2 == 5 ){` |
|      - |  882 | `				/* offsetUnset return is discarded; push NULL so the trailing` |
|      - |  883 | `				 * vm_builtin_unset is a harmless no-op. */` |
|     20 |  884 | `				PH7_MemObjRelease(pTos);` |
|     20 |  885 | `				pTos->nIdx = SXU32_HIGH;` |
|     20 |  886 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|    125 |  887 | `			}else if( pInstr->iP2 == 6 \|\| pInstr->iP2 == 8 ){` |
|      - |  888 | `				/* empty: if offsetExists is false, push NULL so empty=true` |
|      - |  889 | `				 * without calling offsetGet. If true, call offsetGet and` |
|      - |  890 | `				 * push the value so PH7_builtin_empty evaluates emptiness.` |
|      - |  891 | ``				 * `??` (8) needs the identical shape: NULL on a miss so the`` |
|      - |  892 | `				 * coalesce takes the default, the real value on a hit. */` |
|     22 |  893 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     22 |  894 | `				PH7_MemObjRelease(&sResult);` |
|     22 |  895 | `				PH7_MemObjRelease(pTos);` |
|     22 |  896 | `				pTos->nIdx = SXU32_HIGH;` |
|     22 |  897 | `				if( !bExists ){` |
|      8 |  898 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      5 |  899 | `				}else{` |
|     16 |  900 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  901 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      - |  902 | `					ph7_value sValue;` |
|     16 |  903 | `					PH7_MemObjInit(&(*pVm),&sValue);` |
|     16 |  904 | `					apArg[0] = pIdx;` |
|     16 |  905 | `					if( pGet ){` |
|     16 |  906 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      7 |  907 | `					}` |
|     16 |  908 | `					PH7_MemObjStore(&sValue,pTos);` |
|     16 |  909 | `					PH7_MemObjRelease(&sValue);` |
|      - |  910 | `				}` |
|     22 |  911 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     22 |  912 | `				VM_EXIT_BREAK; /* skip the duplicate sResult release below */` |
|     97 |  913 | `			}else if( pInstr->iP2 == 3 ){` |
|      - |  914 | `				/* ?? null-coalesce peek: emulate PHP semantics —` |
|      - |  915 | `				 *   if !offsetExists OR offsetGet() === null → arm` |
|      - |  916 | `				 *     coalesce slot (NULLC_STORE will call offsetSet)` |
|      - |  917 | `				 *     and push NULL.` |
|      - |  918 | `				 *   else → push offsetGet's value (NULLC_JMP skips). */` |
|     10 |  919 | `				int bExists = ph7_value_to_bool(&sResult);` |
|     10 |  920 | `				int bShouldArm = !bExists;` |
|      - |  921 | `				ph7_value sValue;` |
|     10 |  922 | `				PH7_MemObjRelease(&sResult);` |
|      - |  923 | `				/* Reset any prior arming defensively */` |
|     10 |  924 | `				VmCoalesceDisarm(pVm);` |
|     10 |  925 | `				PH7_MemObjInit(&(*pVm),&sValue);` |
|     10 |  926 | `				if( bExists ){` |
|      5 |  927 | `					ph7_class_method *pGet = PH7_ClassExtractMethod(pInst->pClass,` |
|      - |  928 | `						"offsetGet",sizeof("offsetGet")-1);` |
|      5 |  929 | `					apArg[0] = pIdx;` |
|      5 |  930 | `					if( pGet ){` |
|      5 |  931 | `						PH7_VmCallClassMethod(&(*pVm),pInst,pGet,&sValue,pIdx ? 1 : 0,apArg);` |
|      2 |  932 | `					}` |
|      5 |  933 | `					if( sValue.iFlags & MEMOBJ_NULL ){` |
|      3 |  934 | `						bShouldArm = 1;` |
|      1 |  935 | `					}` |
|      2 |  936 | `				}` |
|     10 |  937 | `				PH7_MemObjRelease(pTos);` |
|     10 |  938 | `				pTos->nIdx = SXU32_HIGH;` |
|     10 |  939 | `				if( bShouldArm ){` |
|      - |  940 | `					/* Arm: remember (object, key) so NULLC_STORE dispatches` |
|      - |  941 | `					 * to offsetSet. Hold a ref on the instance to survive` |
|      - |  942 | `					 * intervening expression evaluation. */` |
|      8 |  943 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      8 |  944 | `					if( pIdx ){` |
|      8 |  945 | `						PH7_MemObjStore(pIdx,&pVm->sCoalesceKey);` |
|      3 |  946 | `					}` |
|      8 |  947 | `					pVm->pCoalesceObj = pInst;` |
|      8 |  948 | `					pInst->iRef++;` |
|      8 |  949 | `					pVm->bCoalesceArmed = 1;` |
|      5 |  950 | `				}else{` |
|      3 |  951 | `					PH7_MemObjStore(&sValue,pTos);` |
|      - |  952 | `				}` |
|     10 |  953 | `				PH7_MemObjRelease(&sValue);` |
|     10 |  954 | `				if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|     10 |  955 | `				VM_EXIT_BREAK;` |
|    ! 0 |  956 | `			}else{` |
|      - |  957 | `				/* offsetGet: replace pTos with the returned value. */` |
|     89 |  958 | `				PH7_MemObjRelease(pTos);` |
|     89 |  959 | `				PH7_MemObjStore(&sResult,pTos);` |
|     89 |  960 | `				pTos->nIdx = SXU32_HIGH;` |
|      - |  961 | `			}` |
|    153 |  962 | `			PH7_MemObjRelease(&sResult);` |
|    153 |  963 | `			if( pIdx ){` |
|    153 |  964 | `				PH7_MemObjRelease(pIdx);` |
|     74 |  965 | `			}` |
|    153 |  966 | `			VM_EXIT_BREAK;` |
|      - |  967 | `		}` |
|      - |  968 | `		/* Object without ArrayAccess: PHP throws fatal Error in all subscript` |
|      - |  969 | `		 * contexts (read, isset, unset, empty). Match it. */` |
|      3 |  970 | `		if( pInst ){` |
|      - |  971 | `			char zMsg[256];` |
|      3 |  972 | `			SyString *pName = &pInst->pClass->sName;` |
|      4 |  973 | `			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - |  974 | `				"Cannot use object of type %.*s as array",` |
|      2 |  975 | `				(int)pName->nByte,pName->zString);` |
|      3 |  976 | `			rc = VmThrowFromVm(pVm,"Error",zMsg,nMsg);` |
|      3 |  977 | `			if( pIdx ){ PH7_MemObjRelease(pIdx); }` |
|      3 |  978 | `			PH7_MemObjRelease(pTos);` |
|      3 |  979 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      3 |  980 | `			pTos->nIdx = SXU32_HIGH;` |
|      3 |  981 | `			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      - |  982 | ``			/* `break` used to resume at the NEXT instruction: the catch ran and then`` |
|      - |  983 | `			 * execution carried on inside the try block. */` |
|      3 |  984 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  985 | `		}` |
|    ! 0 |  986 | `	}` |
| 146505 |  987 | `	if( (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) && (pTos->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     23 |  988 | `		if( pTos->nIdx != SXU32_HIGH ){` |
|      - |  989 | `			ph7_value *pObj;` |
|     23 |  990 | `			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){` |
|      - |  991 | `				/* php 8 write-context auto-vivify rules: NULL converts to array` |
|      - |  992 | `				 * silently; FALSE converts with the 8.1 deprecation; any other` |
|      - |  993 | `				 * scalar base — int/float/true/resource — is php's catchable` |
|      - |  994 | `				 * "Cannot use a scalar value as an array" Error and the variable` |
|      - |  995 | `				 * stays untouched (pre-fix the base was silently CONVERTED,` |
|      - |  996 | ``				 * corrupting e.g. `$i = 5; $i[0]++` into array(0 => 6); string`` |
|      - |  997 | `				 * bases were intercepted by the string-offset paths above). */` |
|      - |  998 | `				/* php auto-converts false to an array with an 8.1 DEPRECATION; PHL` |
|      - |  999 | `				 * rejects it like any other scalar base (null still auto-vivifies —` |
|      - | 1000 | `				 * it is not a bool). */` |
|     23 | 1001 | `				if( (pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_RES\|MEMOBJ_BOOL)) != 0 ){` |
|      - | 1002 | `					SyBlob sErrMsg;` |
|      7 | 1003 | `					SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      7 | 1004 | `					SyBlobAppend(&sErrMsg,"Cannot use a scalar value as an array",` |
|      - | 1005 | `						sizeof("Cannot use a scalar value as an array")-1);` |
|      7 | 1006 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      7 | 1007 | `					if( pIdx ){` |
|      7 | 1008 | `						PH7_MemObjRelease(pIdx);` |
|      3 | 1009 | `					}` |
|      7 | 1010 | `					PH7_MemObjRelease(pTos);` |
|      7 | 1011 | `					pTos->nIdx = SXU32_HIGH;` |
|      7 | 1012 | `					VM_EXIT_BREAK;` |
|      - | 1013 | `				}` |
|     17 | 1014 | `				PH7_MemObjToHashmap(pObj);` |
|     17 | 1015 | `				PH7_MemObjLoad(pObj,pTos);` |
|      8 | 1016 | `			}` |
|      8 | 1017 | `		}` |
|      8 | 1018 | `	}` |
| 146499 | 1019 | `	rc = SXERR_NOTFOUND; /* Assume the index is invalid */` |
|      - | 1020 | `	/* php only DEPRECATES a lossy-float subscript / null offset (then truncates /` |
|      - | 1021 | `	 * normalizes to ""); PHL rejects them on a READ or WRITE (iP2 0/1) and stays` |
|      - | 1022 | ``	 * lenient in isset()/empty()/`??`/unset(), where a throw would be wrong. */`` |
|      - | 1023 | `	/* An object/array key is rejected in EVERY context, including isset()/empty()/` |
|      - | 1024 | `	 * unset() where php still throws (only the wording changes) — unlike the` |
|      - | 1025 | `	 * null/float deprecations below, which stay lenient there. A resource key is` |
|      - | 1026 | `	 * accepted with a warning and becomes its integer id. */` |
| 146499 | 1027 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx ){` |
|      - | 1028 | `		SyBlob sTypeMsg;` |
| 146487 | 1029 | `		if( VmOffsetTypeRejected(&(*pVm),pIdx,pInstr->iP2,&sTypeMsg) ){` |
|     13 | 1030 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|     13 | 1031 | `			PH7_MemObjRelease(pIdx);` |
|     13 | 1032 | `			PH7_MemObjRelease(pTos);` |
|     13 | 1033 | `			pTos->nIdx = SXU32_HIGH;` |
|     13 | 1034 | `			VM_EXIT_BREAK;` |
|      - | 1035 | `		}` |
| 146475 | 1036 | `		VmOffsetResourceWarn(&(*pVm),pIdx);` |
|  73235 | 1037 | `	}` |
| 146482 | 1038 | `	if( (pTos->iFlags & MEMOBJ_HASHMAP) && pIdx` |
| 146476 | 1039 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 1) ){` |
|  33929 | 1040 | `		int bNull = (pIdx->iFlags & MEMOBJ_NULL) != 0;` |
|  50894 | 1041 | `		int bLossyFloat = (pIdx->iFlags & MEMOBJ_REAL) != 0` |
|  33924 | 1042 | `			&& pIdx->rVal != (ph7_real)(sxi64)pIdx->rVal;` |
|  33929 | 1043 | `		if( bNull \|\| bLossyFloat ){` |
|      - | 1044 | `			SyBlob sErrMsg;` |
|      5 | 1045 | `			SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1046 | `			SyBlobAppend(&sErrMsg,` |
|      2 | 1047 | `				bNull ? "Cannot access offset of type null on array"` |
|      - | 1048 | `				      : "Cannot access offset of type float on array",` |
|      4 | 1049 | `				(sxu32)SyStrlen(bNull ? "Cannot access offset of type null on array"` |
|      - | 1050 | `				                      : "Cannot access offset of type float on array"));` |
|      5 | 1051 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      5 | 1052 | `			PH7_MemObjRelease(pIdx);` |
|      5 | 1053 | `			PH7_MemObjRelease(pTos);` |
|      5 | 1054 | `			pTos->nIdx = SXU32_HIGH;` |
|      5 | 1055 | `			VM_EXIT_BREAK;` |
|      - | 1056 | `		}` |
|  16960 | 1057 | `	}` |
| 146483 | 1058 | `	if( pTos->iFlags & MEMOBJ_HASHMAP ){` |
| 146473 | 1059 | `		if( pInstr->iP2 == 1 \|\| pInstr->iP2 == 5 ){` |
|      - | 1060 | `			/* Write-context access (iP2 = create-if-missing).  COW-separate` |
|      - | 1061 | `			 * the parent so nested writes like $b[0][0] = 99 don't leak` |
|      - | 1062 | `			 * through shared outer arrays.  Read-only loads (iP2 == 0) must` |
|      - | 1063 | `			 * NOT separate — that would defeat COW on every element read.` |
|      - | 1064 | `			 * iP2=5 is unset-context, treated like iP2=1 for arrays so the` |
|      - | 1065 | `			 * trailing unset() builtin can drop the slot via pTos->nIdx. */` |
|   1265 | 1066 | `			PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|    630 | 1067 | `		}` |
|      - | 1068 | `		/* Point to the hashmap */` |
| 146473 | 1069 | `		pMap = (ph7_hashmap *)pTos->x.pOther;` |
| 146473 | 1070 | `		if( pIdx ){` |
|      - | 1071 | `			/* Load the desired entry */` |
| 146471 | 1072 | `			rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|  73233 | 1073 | `		}` |
| 146473 | 1074 | `		if( pInstr->iP2 == 3 ){` |
|      - | 1075 | `			/* Null coalescing assign peek mode: separate only when we will` |
|      - | 1076 | `			 * actually write back. If the looked-up value is non-null, the` |
|      - | 1077 | `			 * caller's NULLC_JMP will short-circuit and no store happens, so` |
|      - | 1078 | `			 * the parent can stay shared. If the value is null or the key is` |
|      - | 1079 | `			 * missing, separate and re-lookup so the upcoming NULLC_STORE` |
|      - | 1080 | `			 * writes into our own copy. Inner levels of a nested LHS still` |
|      - | 1081 | `			 * use iP2 == 1 (eager separation), which keeps the cascade` |
|      - | 1082 | `			 * correct for the outermost write. */` |
|     21 | 1083 | `			int needWrite = (rc != SXRET_OK);` |
|     21 | 1084 | `			if( !needWrite && pNode ){` |
|     13 | 1085 | `				ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|     13 | 1086 | `				if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      7 | 1087 | `					needWrite = 1;` |
|      3 | 1088 | `				}` |
|      6 | 1089 | `			}` |
|     21 | 1090 | `			if( needWrite ){` |
|     15 | 1091 | `				PH7_HashmapCowSeparate(&(*pVm),pTos);` |
|     15 | 1092 | `				if( pMap != (ph7_hashmap *)pTos->x.pOther ){` |
|      - | 1093 | `					/* The map was actually copied — re-lookup so pNode points` |
|      - | 1094 | `					 * into the new map's storage. */` |
|      7 | 1095 | `					pMap = (ph7_hashmap *)pTos->x.pOther;` |
|      7 | 1096 | `					if( pIdx ){` |
|      7 | 1097 | `						rc = PH7_HashmapLookup(pMap,pIdx,&pNode);` |
|      3 | 1098 | `					}` |
|      3 | 1099 | `				}` |
|      7 | 1100 | `			}` |
|     10 | 1101 | `		}` |
| 146473 | 1102 | `		if( rc != SXRET_OK && (pInstr->iP2 == 1 \|\| pInstr->iP2 == 3 \|\| pInstr->iP2 == 5) ){` |
|      - | 1103 | `			/* Create a new empty entry */` |
|    324 | 1104 | `			rc = PH7_HashmapInsert(pMap,pIdx,0);` |
|    324 | 1105 | `			if( rc == SXRET_OK ){` |
|      - | 1106 | `				/* Point to the last inserted entry */` |
|    321 | 1107 | `				pNode = pMap->pLast;` |
|    161 | 1108 | `			}else{` |
|      - | 1109 | ``				/* An append lvalue (`$a[][...] = v`) whose saturated auto-index`` |
|      - | 1110 | `				 * is occupied threw php's catchable Error. Dispatch it here —` |
|      - | 1111 | `				 * falling through with a stale pMap->pLast is what silently` |
|      - | 1112 | `				 * overwrote $a[PHP_INT_MAX]. */` |
|      3 | 1113 | `				PH7_DISPATCH_ENFORCE_RC(rc)` |
|      - | 1114 | `			}` |
|    160 | 1115 | `		}` |
|  73233 | 1116 | `	}` |
| 146476 | 1117 | `	if( rc != SXRET_OK && pIdx && (pInstr->iP2 == 2 \|\| pInstr->iP2 == 0)` |
|  40412 | 1118 | `	 && (pTos->iFlags & MEMOBJ_HASHMAP)` |
|      8 | 1119 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1120 | ``		/* `$a['k'] ?? $d` compiles its LHS as a plain read (iP2 == 0) followed`` |
|      - | 1121 | `		 * by NULLC/NULLC_JMP — php does NOT warn there, so peek ahead and stay` |
|      - | 1122 | `		 * silent (same guard the magic-accessor read path uses). */` |
|      - | 1123 | `		/* php warns when a missing key is READ (iP2 == 0) or destructured` |
|      - | 1124 | `		 * (iP2 == 2). isset/empty/??/unset (iP2 3-6) and write-context` |
|      - | 1125 | `		 * vivification (iP2 == 1) stay silent, as does a read on a non-array` |
|      - | 1126 | `		 * base (already diagnosed above). php prints an INT key bare and a` |
|      - | 1127 | `		 * STRING key quoted. */` |
|      - | 1128 | `		SyBlob sMsg;` |
|      8 | 1129 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      8 | 1130 | `		if( pIdx->iFlags & (MEMOBJ_STRING\|MEMOBJ_BOOL\|MEMOBJ_NULL) ){` |
|      - | 1131 | `			SyString sKey;` |
|      6 | 1132 | `			if( (pIdx->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 1133 | `				PH7_MemObjToString(pIdx);` |
|    ! 0 | 1134 | `			}` |
|      6 | 1135 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pIdx->sBlob),SyBlobLength(&pIdx->sBlob));` |
|      6 | 1136 | `			SyBlobFormat(&sMsg,"Undefined array key \"%z\"",&sKey);` |
|      4 | 1137 | `		}else{` |
|      3 | 1138 | `			if( (pIdx->iFlags & MEMOBJ_INT) == 0 ){` |
|    ! 0 | 1139 | `				PH7_MemObjToInteger(pIdx);` |
|    ! 0 | 1140 | `			}` |
|      3 | 1141 | `			SyBlobFormat(&sMsg,"Undefined array key %qd",pIdx->x.iVal);` |
|      - | 1142 | `		}` |
|      8 | 1143 | `		SyBlobNullAppend(&sMsg);` |
|      8 | 1144 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      8 | 1145 | `		SyBlobRelease(&sMsg);` |
|      3 | 1146 | `	}` |
| 146482 | 1147 | `	if( (pTos->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0` |
|  73249 | 1148 | `	 && (pInstr->iP2 == 0 \|\| pInstr->iP2 == 2)` |
|     16 | 1149 | `	 && !VmIdxFeedsCoalesce(pInstr) ){` |
|      - | 1150 | `		/* Subscripting a scalar base is a WARNING in php ("Trying to access array offset` |
|      - | 1151 | `		 * on int") that yields NULL. PH7 yielded NULL in silence. */` |
|     11 | 1152 | `		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Trying to access array offset on %s",` |
|      3 | 1153 | `			VmArithTypeName(pTos));` |
|      3 | 1154 | `	}` |
| 146487 | 1155 | `	if( pIdx ){` |
| 146487 | 1156 | `		PH7_MemObjRelease(pIdx);` |
|  73241 | 1157 | `	}` |
| 146487 | 1158 | `	if( rc == SXRET_OK ){` |
|      - | 1159 | `		/* Load entry contents */` |
|  65675 | 1160 | `		if( pMap->iRef < 2 ){` |
|      - | 1161 | `			/* TICKET 1433-42: Array will be deleted shortly,so we will make a copy` |
|      - | 1162 | `			 * of the entry value,rather than pointing to it.` |
|      - | 1163 | `			 */` |
|    117 | 1164 | `			pTos->nIdx = SXU32_HIGH;` |
|    117 | 1165 | `			PH7_HashmapExtractNodeValue(pNode,pTos,TRUE);` |
|     60 | 1166 | `		}else{` |
|  65561 | 1167 | `			pTos->nIdx = pNode->nValIdx;` |
|  65561 | 1168 | `			PH7_HashmapExtractNodeValue(pNode,pTos,FALSE);` |
|  65561 | 1169 | `			PH7_HashmapUnref(pMap);` |
|      - | 1170 | `		}` |
|  32840 | 1171 | `	}else{` |
|      - | 1172 | `		/* No such entry,load NULL */` |
|  80817 | 1173 | `		PH7_MemObjRelease(pTos);` |
|  80817 | 1174 | `		pTos->nIdx = SXU32_HIGH;` |
|      - | 1175 | `	}` |
| 146487 | 1176 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1177 | `	VM_EXIT_BREAK;` |
| 340126 | 1178 | `}` |
|      - | 1179 |  |
|      - | 1180 | `/*` |
|      - | 1181 | ` * OP_LOAD_MAP: body moved verbatim from the OP_LOAD_MAP arm of` |
|      - | 1182 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1183 | ` */` |
|  73990 | 1184 | `PH7_PRIVATE VmOpRc VmExecOpLoadMap(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1185 | `{` |
|  73995 | 1186 | `	ph7_value *pTos = pState->pTos;` |
|  73995 | 1187 | `	ph7_value *pStack = pState->pStack;` |
|  73995 | 1188 | `	VmInstr *aInstr = pState->aInstr;` |
|  73995 | 1189 | `	sxi32 pc = pState->pc;` |
|      - | 1190 | `	sxi32 rc;` |
|  36995 | 1191 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1192 | `	ph7_hashmap *pMap;` |
|      - | 1193 | `	/* Allocate a new hashmap instance */` |
|  73995 | 1194 | `	pMap = PH7_NewHashmap(&(*pVm),0,0);` |
|  73995 | 1195 | `	if( pMap == 0 ){` |
|    ! 0 | 1196 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1197 | `			"Fatal, PH7 engine is running out of memory while loading array at instruction #:%d",pc);` |
|    ! 0 | 1198 | `		VM_EXIT_ABORT;` |
|      - | 1199 | `	}` |
|  73995 | 1200 | `	if( pInstr->iP1 > 0 ){` |
|  11029 | 1201 | `		ph7_value *pEntry = &pTos[-pInstr->iP1+1]; /* Point to the first entry */` |
|  11029 | 1202 | `		sxi32 rcSpread = SXRET_OK;` |
|      - | 1203 | `		/* Perform the insertion */` |
|  41841 | 1204 | `		while( pEntry < pTos ){` |
|  30835 | 1205 | `			if( pEntry[1].iFlags & MEMOBJ_AUX_SPREAD ){` |
|      - | 1206 | `				/* Array unpacking: '...$expr'. Merge entries with PHP 8.1` |
|      - | 1207 | `				 * semantics — string keys preserved (later wins), int keys` |
|      - | 1208 | `				 * renumbered. Same routine that backs array_merge. */` |
|    683 | 1209 | `				if( pEntry[1].iFlags & MEMOBJ_HASHMAP ){` |
|    659 | 1210 | `					sxi32 rcMerge = PH7_HashmapMerge((ph7_hashmap *)pEntry[1].x.pOther,pMap);` |
|    659 | 1211 | `					if( rcMerge != SXRET_OK ){` |
|      - | 1212 | `						/* Merge failure (OOM): match the PH7_NewHashmap OOM` |
|      - | 1213 | `						 * path — emit fatal and abort, leaving no partial` |
|      - | 1214 | `						 * map dangling. */` |
|    ! 0 | 1215 | `						VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|    ! 0 | 1216 | `							"Fatal, PH7 engine is running out of memory while spreading array at instruction #:%d",pc);` |
|    ! 0 | 1217 | `						rcSpread = PH7_ABORT;` |
|    ! 0 | 1218 | `						break;` |
|      1 | 1219 | `					}` |
|    354 | 1220 | `				}else if( VmValueIsTraversable(pVm,&pEntry[1]) ){` |
|      - | 1221 | `					/* Traversable unpacking (PHP 8.1): walk it into the map using the` |
|      - | 1222 | `					 * same key rules as array spread (string keys kept, int renumbered). */` |
|      5 | 1223 | `					sxi32 rcW = PH7_VmIteratorWalk(&(*pVm),&pEntry[1],VmSpreadMergeStep,pMap);` |
|      5 | 1224 | `					if( rcW == PH7_EXCEPTION \|\| rcW == PH7_ABORT ){` |
|    ! 0 | 1225 | `						rcSpread = rcW;` |
|    ! 0 | 1226 | `						break;` |
|      - | 1227 | `					}` |
|      3 | 1228 | `				}else{` |
|      - | 1229 | `					/* Throw a catchable Error matching PHP semantics. */` |
|     21 | 1230 | `					rcSpread = VmThrowSpreadError(&(*pVm),&pEntry[1]);` |
|     21 | 1231 | `					break;` |
|      1 | 1232 | `				}` |
|  30486 | 1233 | `			}else if( pEntry[1].iFlags & MEMOBJ_REFERENCE ){` |
|      - | 1234 | `				/* Insertion by reference */` |
|    181 | 1235 | `				PH7_HashmapInsertByRef(pMap,` |
|    120 | 1236 | `					(pEntry->iFlags & MEMOBJ_NULL) ? 0 /* Automatic index assign */ : pEntry,` |
|    120 | 1237 | `					(sxu32)pEntry[1].x.iVal` |
|      - | 1238 | `					);` |
|     61 | 1239 | `			}else{` |
|      - | 1240 | `				/* An explicit key in an array LITERAL gets the same php diagnostics a` |
|      - | 1241 | `				 * subscript does — a float key that truncates deprecates, and so does an` |
|      - | 1242 | `				 * explicit null key. Only the subscript sites (LOAD_IDX/STORE_IDX) used to` |
|      - | 1243 | ``				 * emit these, so `[1.5 => "v"]` and `[null => "v"]` were silent. A key that`` |
|      - | 1244 | `				 * is ABSENT (auto-index) is a NULL slot here, not a null key, so the` |
|      - | 1245 | `				 * MEMOBJ_NULL check below is what tells the two apart. */` |
|  30035 | 1246 | `					if( (pEntry->iFlags & MEMOBJ_AUX_NOKEY) == 0 ){` |
|      - | 1247 | `						/* An object/array literal key is php's TypeError, a resource one` |
|      - | 1248 | `						 * warns and becomes its id — same rules as a subscript. */` |
|      - | 1249 | `						SyBlob sTypeMsg;` |
|  11219 | 1250 | `						if( VmOffsetTypeRejected(&(*pVm),pEntry,0,&sTypeMsg) ){` |
|      3 | 1251 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg));` |
|      2 | 1252 | `						}else{` |
|  11217 | 1253 | `							VmOffsetResourceWarn(&(*pVm),pEntry);` |
|      - | 1254 | `						}` |
|      - | 1255 | `						/* php only DEPRECATES a lossy-float / null literal key; PHL rejects it. */` |
|  11219 | 1256 | `						int bNull = (pEntry->iFlags & MEMOBJ_NULL) != 0;` |
|  16827 | 1257 | `						int bLossyFloat = (pEntry->iFlags & MEMOBJ_REAL) != 0` |
|  11214 | 1258 | `							&& pEntry->rVal != (ph7_real)(sxi64)pEntry->rVal;` |
|  11219 | 1259 | `						if( bNull \|\| bLossyFloat ){` |
|      5 | 1260 | `							const char *zErr = bNull ? "Cannot access offset of type null on array"` |
|      2 | 1261 | `							                         : "Cannot access offset of type float on array";` |
|      - | 1262 | `							SyBlob sErrMsg;` |
|      5 | 1263 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      5 | 1264 | `							SyBlobAppend(&sErrMsg,zErr,(sxu32)SyStrlen(zErr));` |
|      5 | 1265 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));` |
|      2 | 1266 | `						}` |
|   5607 | 1267 | `					}` |
|      - | 1268 | `				/* Standard insertion */` |
|  45050 | 1269 | `				PH7_HashmapInsert(pMap,` |
|  30030 | 1270 | `					(pEntry->iFlags & MEMOBJ_AUX_NOKEY) ? 0 /* Automatic index assign */ : pEntry,` |
|  15015 | 1271 | `					&pEntry[1]` |
|      - | 1272 | `				);` |
|      - | 1273 | `			}` |
|      - | 1274 | `			/* Next pair on the stack */` |
|  30817 | 1275 | `			pEntry += 2;` |
|      5 | 1276 | `		}` |
|      - | 1277 | `		/* Pop P1 elements */` |
|  11029 | 1278 | `		VmPopOperand(&pTos,pInstr->iP1);` |
|  11029 | 1279 | `		if( rcSpread != SXRET_OK ){` |
|      - | 1280 | `			/* Discard the partially-built map and propagate the exception. */` |
|     21 | 1281 | `			PH7_HashmapRelease(pMap,TRUE);` |
|     21 | 1282 | `			if( rcSpread == PH7_ABORT ){` |
|    ! 0 | 1283 | `				VM_EXIT_ABORT;` |
|      - | 1284 | `			}` |
|      - | 1285 | `			{` |
|      - | 1286 | `				sxi32 iRp;` |
|     21 | 1287 | `				if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      6 | 1288 | `					pc = iRp;` |
|      6 | 1289 | `					VM_EXIT_BREAK;` |
|      - | 1290 | `				}` |
|      - | 1291 | `			}` |
|     15 | 1292 | `			VM_EXIT_EXCEPTION;` |
|      - | 1293 | `		}` |
|   5503 | 1294 | `	}` |
|      - | 1295 | `	/* Push the hashmap */` |
|  73977 | 1296 | `	pTos++;` |
|  73977 | 1297 | `	pTos->nIdx = SXU32_HIGH;` |
|  73977 | 1298 | `	pTos->x.pOther = pMap;` |
|  73977 | 1299 | `	MemObjSetType(pTos,MEMOBJ_HASHMAP);` |
|  73977 | 1300 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1301 | `	VM_EXIT_BREAK;` |
|  37000 | 1302 | `}` |
|      - | 1303 |  |
|      - | 1304 | `/*` |
|      - | 1305 | ` * OP_LOAD_LIST: body moved verbatim from the OP_LOAD_LIST arm of` |
|      - | 1306 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1307 | ` */` |
|    266 | 1308 | `PH7_PRIVATE VmOpRc VmExecOpLoadList(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1309 | `{` |
|    271 | 1310 | `	ph7_value *pTos = pState->pTos;` |
|    271 | 1311 | `	ph7_value *pStack = pState->pStack;` |
|    271 | 1312 | `	VmInstr *aInstr = pState->aInstr;` |
|    271 | 1313 | `	sxi32 pc = pState->pc;` |
|      - | 1314 | `	sxi32 rc;` |
|    133 | 1315 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1316 | `	ph7_value *pEntry;` |
|    271 | 1317 | `	if( pInstr->iP1 <= 0 ){` |
|      - | 1318 | `		/* Empty list,break immediately */` |
|    ! 0 | 1319 | `		VM_EXIT_BREAK;` |
|      - | 1320 | `	}` |
|    271 | 1321 | `	pEntry = &pTos[-pInstr->iP1+1];` |
|      - | 1322 | `#ifdef UNTRUST` |
|      - | 1323 | `	if( &pEntry[-1] < pStack ){` |
|      - | 1324 | `		VM_EXIT_ABORT;` |
|      - | 1325 | `	}` |
|      - | 1326 | `#endif` |
|    271 | 1327 | `	if( pEntry[-1].iFlags & MEMOBJ_HASHMAP ){` |
|    265 | 1328 | `		ph7_hashmap *pMap = (ph7_hashmap *)pEntry[-1].x.pOther;` |
|      - | 1329 | `		ph7_hashmap_node *pNode;` |
|      - | 1330 | `		ph7_value sKey,*pObj;` |
|      - | 1331 | `		/* Start Copying */` |
|    265 | 1332 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,0);` |
|    811 | 1333 | `		while( pEntry <= pTos ){` |
|    551 | 1334 | `			if( pEntry->nIdx != SXU32_HIGH /* Variable not constant */  ){` |
|    523 | 1335 | `				rc = PH7_HashmapLookup(pMap,&sKey,&pNode);` |
|    523 | 1336 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|    523 | 1337 | `					if( rc == SXRET_OK ){` |
|      - | 1338 | `						/* Store node value */` |
|    523 | 1339 | `						PH7_HashmapExtractNodeValue(pNode,pObj,TRUE);` |
|    264 | 1340 | `					}else{` |
|      - | 1341 | `						/* Undefined array key */` |
|      - | 1342 | `						char zMsg[128];` |
|    ! 0 | 1343 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Undefined array key %d",(int)sKey.x.iVal);` |
|    ! 0 | 1344 | `						PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,zMsg);` |
|    ! 0 | 1345 | `						PH7_MemObjRelease(pObj);` |
|      - | 1346 | `					}` |
|    259 | 1347 | `				}` |
|    259 | 1348 | `			}` |
|    551 | 1349 | `			sKey.x.iVal++; /* Next numeric index */` |
|    551 | 1350 | `			pEntry++;` |
|      5 | 1351 | `		}` |
|    135 | 1352 | `	}else{` |
|      - | 1353 | `		/* Source is not an array */` |
|      - | 1354 | `		ph7_value *pObj;` |
|     18 | 1355 | `		while( pEntry <= pTos ){` |
|     12 | 1356 | `			if( pEntry->nIdx != SXU32_HIGH ){` |
|     12 | 1357 | `				if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nIdx)) != 0 ){` |
|     12 | 1358 | `					PH7_MemObjRelease(pObj);` |
|      5 | 1359 | `				}` |
|      5 | 1360 | `			}` |
|     12 | 1361 | `			pEntry++;` |
|      2 | 1362 | `		}` |
|      8 | 1363 | `		if( (pTos[-pInstr->iP1].iFlags & (MEMOBJ_NULL\|MEMOBJ_BOOL)) == 0 ){` |
|      - | 1364 | `			/* Positional list destructuring silences null+bool; warn for the rest. */` |
|      3 | 1365 | `			VmWarnCannotUseAsArray(&(*pVm),pTos[-pInstr->iP1].iFlags);` |
|      1 | 1366 | `		}` |
|      - | 1367 | `	}` |
|    271 | 1368 | `	VmPopOperand(&pTos,pInstr->iP1);` |
|    271 | 1369 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1370 | `	VM_EXIT_BREAK;` |
|    138 | 1371 | `}` |
|      - | 1372 |  |
|      - | 1373 | `/*` |
|      - | 1374 | ` * OP_UNSET_VAR: body moved verbatim from the OP_UNSET_VAR arm of` |
|      - | 1375 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - | 1376 | ` */` |
|   6754 | 1377 | `PH7_PRIVATE VmOpRc VmExecOpUnsetVar(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 | 1378 | `{` |
|   6759 | 1379 | `	ph7_value *pTos = pState->pTos;` |
|   6759 | 1380 | `	ph7_value *pStack = pState->pStack;` |
|   6759 | 1381 | `	VmInstr *aInstr = pState->aInstr;` |
|   6759 | 1382 | `	sxi32 pc = pState->pc;` |
|      - | 1383 | `	sxi32 rc;` |
|   3377 | 1384 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - | 1385 | `	/* unset($name): p3 is the variable name. Drops the NAME only — see VmUnsetVarByName */` |
|   6759 | 1386 | `	SyString *pName = (SyString *)pInstr->p3;` |
|   6759 | 1387 | `	if( pName && pVm->pFrame ){` |
|      - | 1388 | `		/* Inside a try{} the VM pushes an EXCEPTION frame; variables live in the body` |
|      - | 1389 | `		 * frame below it, so skip past it exactly as every other variable path does.` |
|      - | 1390 | `		 * Without this, unset($x) inside a try silently found nothing and did nothing. */` |
|   6759 | 1391 | `		VmFrame *pVarFrame = VmSkipExceptionFrames(pVm->pFrame);` |
|   6759 | 1392 | `		sxi32 rcU = VmUnsetVarByName(&(*pVm),pVarFrame,pName->zString,pName->nByte);` |
|   6759 | 1393 | `		if( rcU == PH7_ABORT ){` |
|      3 | 1394 | `			VM_EXIT_ABORT;` |
|      - | 1395 | `		}` |
|      - | 1396 | `		/* Releasing the last holder can run a __destruct(), and that destructor may` |
|      - | 1397 | `		 * throw. Such a throw is PARKED in nBoundaryRc by the boundary rail; consume it` |
|      - | 1398 | `		 * here and route it, or the catch runs and execution resumes inside the try` |
|      - | 1399 | `		 * ("resumed-dtor" instead of php's "caught-dtor"). */` |
|   6757 | 1400 | `		if( pVm->nBoundaryRc != 0 ){` |
|      3 | 1401 | `			rc = pVm->nBoundaryRc;` |
|      3 | 1402 | `			pVm->nBoundaryRc = 0;` |
|      3 | 1403 | `			if( rc == PH7_ABORT ){` |
|    ! 0 | 1404 | `				VM_EXIT_ABORT;` |
|      - | 1405 | `			}` |
|      3 | 1406 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - | 1407 | `		}` |
|   3375 | 1408 | `	}` |
|   6755 | 1409 | `	VM_EXIT_BREAK;` |
|    ! 0 | 1410 | `	VM_EXIT_BREAK;` |
|   3382 | 1411 | `}` |
|      - | 1412 |  |
