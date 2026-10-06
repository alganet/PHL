# src/ph7/vm_builtin_ob.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 614/639 lines (96.09%)

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
|      - |    8 | ` * Output buffering, php's model.` |
|      - |    9 | ` *` |
|      - |   10 | ` * A buffer holds the RAW bytes written into it and its handler runs on the way` |
|      - |   11 | ` * OUT — at a flush, a clean, or when the buffer is removed — not on the way in.` |
|      - |   12 | ` * The engine used to filter at WRITE time, which is a different program in four` |
|      - |   13 | ` * visible ways: ob_get_contents() answered filtered text php answers raw,` |
|      - |   14 | ``  * a handler saw one call per echo instead of one per operation, the `$phase` `` |
|      - |   15 | ` * argument that tells it WHICH operation was always 0, and $chunk_size (which` |
|      - |   16 | ` * only means anything to a write-out) was declared and never read.` |
|      - |   17 | ` *` |
|      - |   18 | ` * VmObPerform() is that operation: it takes the raw bytes out of the buffer, runs` |
|      - |   19 | ` * the handler over them once with the phase php would pass, and hands the answer` |
|      - |   20 | ` * to VmObDeliver(), which is the buffer BELOW or the real output.` |
|      - |   21 | ` */` |
|      - |   22 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry);` |
|      - |   23 | `static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen);` |
|      - |   24 | `static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen);` |
|      - |   25 | `static ph7_int64 VmObInitSize(VmObEntry *pEntry);` |
|      - |   26 | `static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming);` |
|      - |   27 | `static void VmObDeprecateOutput(ph7_vm *pVm,VmObEntry *pEntry);` |
|      - |   28 | `/* The shutdown flush's caller: php names it, with no function behind it. */` |
|      - |   29 | `static SyString sObShutdown = { "PHP Request Shutdown", sizeof("PHP Request Shutdown")-1 };` |
|      - |   30 | `/*` |
|      - |   31 | ` * TRUE while an output handler's own body is running.` |
|      - |   32 | ` *` |
|      - |   33 | ` * php discards everything a handler prints and refuses every ob call that would` |
|      - |   34 | ` * mutate the stack under it. The test is not merely "a handler is running": when` |
|      - |   35 | ` * the handler THROWS, this engine runs the enclosing catch IN PLACE, before the` |
|      - |   36 | ` * dispatch call returns — and that catch is ordinary code in the frame that` |
|      - |   37 | ` * called ob_flush(), so what IT prints belongs in the buffer and the ob calls it` |
|      - |   38 | ` * makes are allowed. Comparing the running frame against the one that entered` |
|      - |   39 | ` * the handler tells the two apart.` |
|      - |   40 | ` */` |
| 282614 |   41 | `static int VmObInHandler(ph7_vm *pVm)` |
|      5 |   42 | `{` |
|      - |   43 | `	VmFrame *pCur;` |
| 282619 |   44 | `	if( pVm->nObDepth < 1 ){` |
| 282467 |   45 | `		return 0;` |
|      - |   46 | `	}` |
|    156 |   47 | `	if( (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) == 0 ){` |
|      - |   48 | `		/* The handler's own body — a php function running in its own frame, or a C` |
|      - |   49 | `		 * builtin handler running in the caller's, which pushes none at all. */` |
|    152 |   50 | `		return 1;` |
|      - |   51 | `	}` |
|      - |   52 | `	/* A CATCH body, which runs in an exception frame. This engine runs the catch` |
|      - |   53 | `	 * for a throw inside the handler IN PLACE, before the dispatch call returns, so` |
|      - |   54 | `	 * a catch is only "inside the handler" when it belongs to a frame BELOW the one` |
|      - |   55 | `	 * that entered it — the handler catching its own throw. A catch in that frame,` |
|      - |   56 | `	 * or in any frame above it, is ordinary code: what it prints belongs in the` |
|      - |   57 | `	 * buffer and the ob calls it makes are allowed. */` |
|      5 |   58 | `	pCur = VmSkipExceptionFrames(pVm->pFrame);` |
|      5 |   59 | `	if( pCur == pVm->pObFrame ){` |
|    ! 0 |   60 | `		return 0;` |
|      - |   61 | `	}` |
|      9 |   62 | `	while( pCur ){` |
|      9 |   63 | `		if( pCur == pVm->pObFrame ){` |
|      5 |   64 | `			return 1;` |
|      - |   65 | `		}` |
|      5 |   66 | `		pCur = pCur->pParent;` |
|      1 |   67 | `	}` |
|    ! 0 |   68 | `	return 0;` |
| 141312 |   69 | `}` |
|      - |   70 | `/*` |
|      - |   71 | ` * How many buffers the ob functions can SEE right now. php truncates the stack at` |
|      - |   72 | ` * the buffer whose handler is running: from inside one, ob_get_level() answers` |
|      - |   73 | ` * that buffer's level and ob_get_contents() its bytes, not those of whatever was` |
|      - |   74 | ` * stacked on top of it (reachable when a chunked buffer writes out while an inner` |
|      - |   75 | ` * buffer is open).` |
|      - |   76 | ` */` |
|    472 |   77 | `static sxu32 VmObVisible(ph7_vm *pVm)` |
|      5 |   78 | `{` |
|    477 |   79 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|    477 |   80 | `	if( VmObInHandler(pVm) && pVm->nObActive > 0 && pVm->nObActive < nUsed ){` |
|      9 |   81 | `		return pVm->nObActive;` |
|      - |   82 | `	}` |
|    469 |   83 | `	return nUsed;` |
|    241 |   84 | `}` |
|      - |   85 | `static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw,ph7_class_instance **ppExc);` |
|      - |   86 | `static sxi32 VmObRethrow(ph7_vm *pVm,ph7_class_instance *pExc,sxi32 rc);` |
|      - |   87 | `/*` |
|      - |   88 | ` * Perform one output-buffer operation on the buffer at index nIdx.` |
|      - |   89 | ` *` |
|      - |   90 | ` *   iOp   one of PH7_OB_WRITE / PH7_OB_FLUSH / PH7_OB_CLEAN, optionally with` |
|      - |   91 | ` *         PH7_OB_FINAL (the buffer is going away). PH7_OB_START is added here` |
|      - |   92 | ` *         while the handler has not run yet, exactly as php does.` |
|      - |   93 | ` *   pRaw  when non-NULL, receives a copy of the buffer as it was BEFORE the` |
|      - |   94 | ` *         handler ran — ob_get_clean()/ob_get_flush() answer that, not the` |
|      - |   95 | ` *         handler's output.` |
|      - |   96 | ` *   ppExc when non-NULL, receives the exception the handler threw (one` |
|      - |   97 | ` *         reference) instead of having it raised here: a door that removes the` |
|      - |   98 | ` *         buffer raises it after the removal (VmObRethrow), as php's pending` |
|      - |   99 | ` *         exception surfaces only once the whole operation is over.` |
|      - |  100 | ` *` |
|      - |  101 | ` * The buffer is left empty; removing it is the caller's job. A CLEAN still runs` |
|      - |  102 | ` * the handler (php gives it the chance to reset its own state) and then throws` |
|      - |  103 | ` * the answer away.` |
|      - |  104 | ` */` |
|  12470 |  105 | `static sxi32 VmObPerform(ph7_vm *pVm,sxu32 nIdx,int iOp,SyBlob *pRaw,ph7_class_instance **ppExc)` |
|      5 |  106 | `{` |
|  12475 |  107 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|  12475 |  108 | `	ph7_class_instance *pExc = 0;` |
|      - |  109 | `	SyBlob sData;` |
|  12475 |  110 | `	sxi32 rc = PH7_OK;` |
|      - |  111 | `	sxu32 nRawLen;` |
|  12475 |  112 | `	int bDrop = 0;` |
|  12475 |  113 | `	if( ppExc ){` |
|  12397 |  114 | `		*ppExc = 0;` |
|   6196 |  115 | `	}` |
|  12475 |  116 | `	if( pEntry == 0 ){` |
|    ! 0 |  117 | `		return PH7_OK;` |
|      - |  118 | `	}` |
|      - |  119 | `	/* Take the bytes OUT of the entry before anything else runs: the handler is` |
|      - |  120 | `	 * php code, and php code reaching back into the buffer stack reallocates it —` |
|      - |  121 | `	 * every pointer into the set, this entry's own blob included, dies with it. */` |
|  12475 |  122 | `	SyBlobInit(&sData,&pVm->sAllocator);` |
|  12475 |  123 | `	if( SyBlobLength(&pEntry->sOB) > 0 ){` |
|   7187 |  124 | `		SyBlobDup(&pEntry->sOB,&sData);` |
|   7187 |  125 | `		if( pRaw ){` |
|   6919 |  126 | `			SyBlobDup(&pEntry->sOB,pRaw);` |
|   3457 |  127 | `		}` |
|   3591 |  128 | `	}` |
|  12475 |  129 | `	nRawLen = SyBlobLength(&sData);` |
|  12475 |  130 | `	if( !ph7_value_is_callable(&pEntry->sCallback) ){` |
|      - |  131 | `		/* No handler: php's internal one, which "runs" for every operation and is` |
|      - |  132 | `		 * always taken to have produced its output. ob_get_status() reports both` |
|      - |  133 | `		 * bits from the first operation on, empty buffer included. */` |
|  12149 |  134 | `		pEntry->iFlags \|= PH7_OB_STARTED \| PH7_OB_PROCESSED;` |
|   6072 |  135 | `	}` |
|  12470 |  136 | `	if( ph7_value_is_callable(&pEntry->sCallback)` |
|   6403 |  137 | `		&& (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){` |
|      - |  138 | `		ph7_value sArg,sPhase,sResult,*apArg[2];` |
|    313 |  139 | `		int iPhase = iOp \| ((pEntry->iFlags & PH7_OB_STARTED) ? 0 : PH7_OB_START);` |
|    313 |  140 | `		int bRefused = 0;` |
|      - |  141 | `		ph7_value sCallback;` |
|      - |  142 | `		sxi32 rcCall;` |
|      - |  143 | `		/* The callback is copied out for the same reason the bytes are. */` |
|    313 |  144 | `		PH7_MemObjInit(pVm,&sCallback);` |
|    313 |  145 | `		PH7_MemObjStore(&pEntry->sCallback,&sCallback);` |
|      - |  146 | `		/* Marked failed for the DURATION of the call, and cleared again when it comes` |
|      - |  147 | `		 * back with an answer. A disabled buffer is transparent, and that is exactly` |
|      - |  148 | `		 * what this buffer is while its handler runs: whatever the in-place catch for` |
|      - |  149 | `		 * a throwing handler prints belongs to the level BELOW, which is where php —` |
|      - |  150 | `		 * whose catch runs after the operation finished — puts it too. */` |
|    313 |  151 | `		pEntry->iFlags \|= PH7_OB_DISABLED;` |
|    313 |  152 | `		PH7_MemObjInitFromString(pVm,&sArg,0);` |
|    313 |  153 | `		PH7_MemObjStringAppend(&sArg,(const char *)SyBlobData(&sData),SyBlobLength(&sData));` |
|      - |  154 | `		/* php calls the handler as ($buffer, int $phase) — a handler declaring` |
|      - |  155 | `		 * both as required must not trip the arity check. */` |
|    313 |  156 | `		PH7_MemObjInitFromInt(pVm,&sPhase,iPhase);` |
|    313 |  157 | `		apArg[0] = &sArg;` |
|    313 |  158 | `		apArg[1] = &sPhase;` |
|    313 |  159 | `		PH7_MemObjInit(pVm,&sResult);` |
|      - |  160 | `		/* What the handler prints lands back in this buffer, after the bytes it was` |
|      - |  161 | `		 * handed (VmObConsumer): an answer discards it, a failure sends it on. It` |
|      - |  162 | `		 * may not open a buffer of its own.` |
|      - |  163 | `		 * Through the callback dispatcher, not PH7_VmCallUserFunction: php builds` |
|      - |  164 | `		 * both arguments itself and passes them BY VALUE, so a handler declaring` |
|      - |  165 | ``		 * `&$buffer` gets php's warning and a copy rather than the fatal a direct`` |
|      - |  166 | `		 * call raises. Behind a throw fence: php's exception stays PENDING until` |
|      - |  167 | `		 * the operation is over, so the caller's catch must not run in place` |
|      - |  168 | `		 * ahead of the bytes this failure still delivers. */` |
|      - |  169 | `		{` |
|    313 |  170 | `			VmFrame *pSaveFrame = pVm->pObFrame;` |
|    313 |  171 | `			sxu32 nSaveActive = pVm->nObActive;` |
|    313 |  172 | `			int bSaveRefused = pVm->bObRefused;` |
|    313 |  173 | `			sxi32 nBrcIn = pVm->nBoundaryRc;` |
|    313 |  174 | `			sxu32 nFenceIn = pVm->nThrowFence;` |
|    313 |  175 | `			ph7_class_instance *pExcIn = pVm->pFencedExc;` |
|    313 |  176 | `			pVm->pObFrame = pVm->pFrame;` |
|    313 |  177 | `			pVm->nObActive = nIdx + 1;` |
|    313 |  178 | `			pVm->bObRefused = 0;` |
|    313 |  179 | `			pVm->pFencedExc = 0;` |
|    313 |  180 | `			pVm->nThrowFence = SySetUsed(&pVm->aException) + 1;` |
|    313 |  181 | `			pVm->nObDepth++;` |
|    313 |  182 | `			rcCall = PH7_VmCallCallbackByValue(pVm,&sCallback,2,apArg,&sResult,0);` |
|    313 |  183 | `			pVm->nObDepth--;` |
|    313 |  184 | `			pVm->nThrowFence = nFenceIn;` |
|    313 |  185 | `			pExc = pVm->pFencedExc;` |
|    313 |  186 | `			pVm->pFencedExc = pExcIn;` |
|    313 |  187 | `			if( pExc ){` |
|     25 |  188 | `				if( pVm->nBoundaryRc == PH7_ABORT && nBrcIn != PH7_ABORT ){` |
|    ! 0 |  189 | `					PH7_ClassInstanceUnref(pExc);` |
|    ! 0 |  190 | `					pExc = 0;` |
|    ! 0 |  191 | `				}else{` |
|     25 |  192 | `					pVm->nBoundaryRc = nBrcIn; /* nothing was caught in place yet */` |
|      - |  193 | `				}` |
|     11 |  194 | `			}` |
|    313 |  195 | `			if( pExc && pVm->pCalleeName == &sObShutdown ){` |
|      - |  196 | `				/* With no frame left to throw into, php's throw is fatal right` |
|      - |  197 | `				 * there and the request bails out: nothing more is delivered. */` |
|      3 |  198 | `				bDrop = 1;` |
|      1 |  199 | `			}` |
|    313 |  200 | `			bRefused = pVm->bObRefused;` |
|    313 |  201 | `			pVm->bObRefused = bSaveRefused;` |
|    313 |  202 | `			pVm->nObActive = nSaveActive;` |
|    313 |  203 | `			pVm->pObFrame = pSaveFrame;` |
|      - |  204 | `		}` |
|      - |  205 | `		/* php code ran: the slot may have moved, or gone. */` |
|    313 |  206 | `		pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|    313 |  207 | `		if( pEntry ){` |
|      - |  208 | `			/* Set AFTER the call: php's own STARTED is not visible to the first` |
|      - |  209 | `			 * invocation, only to the ones that follow it. */` |
|    313 |  210 | `			pEntry->iFlags \|= PH7_OB_STARTED;` |
|    154 |  211 | `		}` |
|    313 |  212 | `		if( pEntry && (pEntry->iFlags & PH7_OB_PRODUCED) && !PH7_CALLBACK_UNWOUND(rcCall) && pExc == 0 ){` |
|      - |  213 | `			/* php 8.4: the handler printed, and answered. Raised while` |
|      - |  214 | `			 * the buffer is still disabled, so the diagnostic's own display lands` |
|      - |  215 | `			 * in the level below -- and, the handler still counting as running,` |
|      - |  216 | `			 * marks THAT buffer in turn. A handler that threw or exited is not` |
|      - |  217 | `			 * deprecated. */` |
|     86 |  218 | `			pEntry->iFlags &= ~PH7_OB_PRODUCED;` |
|     86 |  219 | `			VmObDeprecateOutput(pVm,pEntry);` |
|     86 |  220 | `			pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|     41 |  221 | `		}` |
|    308 |  222 | `		if( PH7_CALLBACK_UNWOUND(rcCall) \|\| pExc` |
|    288 |  223 | `			\|\| (ph7_value_is_bool(&sResult) && !ph7_value_to_bool(&sResult)) ){` |
|      - |  224 | `			/* php's two FAILURE shapes — the handler answered FALSE, or it threw` |
|      - |  225 | `			 * (or exited) and never answered at all. Both send the ORIGINAL bytes,` |
|      - |  226 | ``			 * so `sData` keeps them, and both leave the handler`` |
|      - |  227 | `			 * DISABLED: it is not called again (which is what keeps a throwing` |
|      - |  228 | `			 * handler from throwing a second time out of the shutdown flush) and the` |
|      - |  229 | `			 * buffer stops buffering. What the handler printed goes on with them. */` |
|     53 |  230 | `			if( pEntry && SyBlobLength(&pEntry->sOB) > nRawLen ){` |
|     62 |  231 | `				SyBlobAppend(&sData,(const char *)SyBlobData(&pEntry->sOB) + nRawLen,` |
|     40 |  232 | `					SyBlobLength(&pEntry->sOB) - nRawLen);` |
|     23 |  233 | `			}` |
|    288 |  234 | `		}else if( ph7_value_is_bool(&sResult) ){` |
|      - |  235 | `			/* TRUE: "no data" — the operation produces nothing at all, and php` |
|      - |  236 | `			 * counts that as the handler having processed the buffer. */` |
|      9 |  237 | `			if( pEntry ){` |
|      9 |  238 | `				pEntry->iFlags &= ~PH7_OB_DISABLED;` |
|      9 |  239 | `				pEntry->iFlags \|= PH7_OB_PROCESSED;` |
|      4 |  240 | `			}` |
|      9 |  241 | `			SyBlobReset(&sData);` |
|      5 |  242 | `		}else{` |
|      - |  243 | `			/* Anything else is cast to a string, NULL included — php's own` |
|      - |  244 | `			 * user-visible conversion, so an ARRAY comes out as "Array" WITH the` |
|      - |  245 | `			 * warning and an object with no __toString() throws. */` |
|      - |  246 | `			const char *zOut;` |
|      - |  247 | `			int nOut;` |
|    255 |  248 | `			PH7_MemObjToStringUV(&sResult);` |
|    255 |  249 | `			zOut = ph7_value_to_string(&sResult,&nOut);` |
|    255 |  250 | `			SyBlobReset(&sData);` |
|    255 |  251 | `			if( nOut > 0 ){` |
|    205 |  252 | `				SyBlobAppend(&sData,zOut,(sxu32)nOut);` |
|    100 |  253 | `			}` |
|    255 |  254 | `			if( pEntry ){` |
|    255 |  255 | `				pEntry->iFlags &= ~PH7_OB_DISABLED;` |
|    255 |  256 | `				pEntry->iFlags \|= PH7_OB_PROCESSED;` |
|    125 |  257 | `			}` |
|      - |  258 | `		}` |
|    313 |  259 | `		PH7_MemObjRelease(&sArg);` |
|    313 |  260 | `		PH7_MemObjRelease(&sPhase);` |
|    313 |  261 | `		PH7_MemObjRelease(&sResult);` |
|    313 |  262 | `		PH7_MemObjRelease(&sCallback);` |
|      - |  263 | `		/* A throw or an exit() inside the handler is the CALLER's to act on: the` |
|      - |  264 | `		 * enclosing catch runs, or the program ends. */` |
|    313 |  265 | `		if( PH7_CALLBACK_UNWOUND(rcCall) ){` |
|     27 |  266 | `			rc = rcCall;` |
|     12 |  267 | `		}` |
|      - |  268 | `		/* An ob call the handler was not allowed to make ends the request, and php` |
|      - |  269 | `		 * delivers nothing more. An ordinary exit() from inside one is NOT that:` |
|      - |  270 | `		 * php still sends what the buffer held. */` |
|    313 |  271 | `		if( bRefused ){` |
|      6 |  272 | `			bDrop = 1;` |
|      2 |  273 | `		}` |
|    154 |  274 | `	}` |
|      - |  275 | `	/* The buffer is empty now: what the operation took, and what the handler` |
|      - |  276 | `	 * printed after it, went with the answer or with the failure. Re-resolved` |
|      - |  277 | `	 * because php code may have moved the set. */` |
|  12475 |  278 | `	pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|  12475 |  279 | `	if( pEntry ){` |
|  12475 |  280 | `		SyBlobReset(&pEntry->sOB);` |
|   6235 |  281 | `	}` |
|  12475 |  282 | `	if( (iOp & PH7_OB_CLEAN) == 0 && SyBlobLength(&sData) > 0 && !bDrop ){` |
|    219 |  283 | `		sxi32 rcOut = VmObDeliver(pVm,nIdx,SyBlobData(&sData),SyBlobLength(&sData));` |
|    219 |  284 | `		if( rc == PH7_OK ){` |
|    207 |  285 | `			rc = rcOut;` |
|    101 |  286 | `		}` |
|    107 |  287 | `	}` |
|  12475 |  288 | `	SyBlobRelease(&sData);` |
|  12475 |  289 | `	if( ppExc ){` |
|  12397 |  290 | `		*ppExc = pExc;` |
|  12397 |  291 | `		return rc;` |
|      - |  292 | `	}` |
|     82 |  293 | `	return VmObRethrow(pVm,pExc,rc);` |
|   6240 |  294 | `}` |
|      - |  295 | `/*` |
|      - |  296 | ` * Raise the exception a handler threw (VmObPerform held it back), consuming the` |
|      - |  297 | ` * reference; rc passes through when there is none.` |
|      - |  298 | ` */` |
|  12388 |  299 | `static sxi32 VmObRethrow(ph7_vm *pVm,ph7_class_instance *pExc,sxi32 rc)` |
|      5 |  300 | `{` |
|      - |  301 | `	VmFrame *pFrame;` |
|  12393 |  302 | `	if( pExc == 0 ){` |
|  12371 |  303 | `		return rc;` |
|      - |  304 | `	}` |
|     25 |  305 | `	pFrame = pVm->pFrame;` |
|     25 |  306 | `	if( pFrame ){` |
|     25 |  307 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|     25 |  308 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|     11 |  309 | `	}` |
|     25 |  310 | `	rc = VmThrowException(&(*pVm),pExc);` |
|     25 |  311 | `	PH7_ClassInstanceUnref(pExc);` |
|     25 |  312 | `	rc = rc == SXERR_ABORT ? PH7_ABORT : PH7_EXCEPTION;` |
|      - |  313 | `	/* The callers with no status channel (an echo that filled a chunked buffer)` |
|      - |  314 | `	 * have it routed at the next fetch, as the dispatcher would have. */` |
|     25 |  315 | `	VmBoundaryPark(&(*pVm),rc);` |
|     25 |  316 | `	return rc;` |
|   6199 |  317 | `}` |
|      - |  318 | `/*` |
|      - |  319 | ` * Hand nLen bytes to the buffer at index iIdx, or to whatever is under it.` |
|      - |  320 | ` *` |
|      - |  321 | ` * php's stack is a stack — a nested flush lands in the enclosing buffer, not on` |
|      - |  322 | ` * stdout — and a DISABLED buffer is transparent: once a handler has failed php` |
|      - |  323 | ` * stops buffering through it (the level is still there and still counts, but` |
|      - |  324 | ` * everything written to it passes straight down), which is why the catch that` |
|      - |  325 | ` * follows a throwing handler prints immediately and ob_get_contents() answers "".` |
|      - |  326 | ` */` |
| 257272 |  327 | `static sxi32 VmObSink(ph7_vm *pVm,sxi32 iIdx,const void *pData,sxu32 nLen)` |
|      5 |  328 | `{` |
|      - |  329 | `	sxi32 rc;` |
| 257315 |  330 | `	while( iIdx >= 0 ){` |
| 257215 |  331 | `		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,(sxu32)iIdx);` |
| 257215 |  332 | `		if( pEntry == 0 ){` |
|    ! 0 |  333 | `			break; /* the buffer went away underneath: fall through to the output */` |
|      - |  334 | `		}` |
| 257215 |  335 | `		if( (pEntry->iFlags & PH7_OB_DISABLED) == 0 ){` |
| 257177 |  336 | `			if( pVm->bObRaising && nLen > 0 ){` |
|      5 |  337 | `				pEntry->iFlags \|= PH7_OB_PRODUCED;` |
|      2 |  338 | `			}` |
| 257177 |  339 | `			VmObGrow(pEntry,nLen);` |
| 257177 |  340 | `			SyBlobAppend(&pEntry->sOB,pData,nLen);` |
|      - |  341 | `			/* A buffer with a chunk size writes out as soon as it holds one. */` |
| 257177 |  342 | `			if( pEntry->nChunk > 0 && SyBlobLength(&pEntry->sOB) >= pEntry->nChunk ){` |
|     23 |  343 | `				return VmObPerform(pVm,(sxu32)iIdx,PH7_OB_WRITE,0,0);` |
|      - |  344 | `			}` |
| 257157 |  345 | `			return PH7_OK;` |
|      - |  346 | `		}` |
|     41 |  347 | `		iIdx--;` |
|      3 |  348 | `	}` |
|      - |  349 | `	/* Call the VM output consumer */` |
|    104 |  350 | `	rc = pVm->sVmConsumer.xDef(pData,(unsigned int)nLen,pVm->sVmConsumer.pDefData);` |
|      - |  351 | `	/* Increment VM output counter */` |
|    104 |  352 | `	pVm->nOutputLen += nLen;` |
|    104 |  353 | `	if( rc != PH7_ABORT ){` |
|    104 |  354 | `		rc = PH7_OK;` |
|     50 |  355 | `	}` |
|    104 |  356 | `	return rc;` |
| 128641 |  357 | `}` |
|      - |  358 | `/*` |
|      - |  359 | ` * Deliver what is leaving the buffer at nIdx to whatever is under it.` |
|      - |  360 | ` */` |
|    214 |  361 | `static sxi32 VmObDeliver(ph7_vm *pVm,sxu32 nIdx,const void *pData,sxu32 nLen)` |
|      5 |  362 | `{` |
|    219 |  363 | `	return VmObSink(pVm,(sxi32)nIdx - 1,pData,nLen);` |
|      5 |  364 | `}` |
|      - |  365 | `/*` |
|      - |  366 | ` * Output Buffer(OB) default VM consumer routine.All VM output is now redirected` |
|      - |  367 | ` * to a stackable internal buffer,until the user call [ob_get_clean(),ob_end_clean(),...].` |
|      - |  368 | ` * Refer to the implementation of [ob_start()] for more information.` |
|      - |  369 | ` */` |
| 257162 |  370 | `PH7_PRIVATE int VmObConsumer(const void *pData,unsigned int nDataLen,void *pUserData)` |
|      5 |  371 | `{` |
| 257167 |  372 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
| 257167 |  373 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
| 257167 |  374 | `	if( nUsed < 1 ){` |
|      - |  375 | `		/* CAN'T HAPPEN */` |
|    ! 0 |  376 | `		return PH7_OK;` |
|      - |  377 | `	}` |
| 257167 |  378 | `	if( VmObInHandler(pVm) ){` |
|      - |  379 | `		/* Inside a handler: php stores it in the topmost buffer still taking` |
|      - |  380 | `		 * bytes (the running one counts: it is disabled here only for the` |
|      - |  381 | `		 * duration of its call), with no write-out, remembers that something` |
|      - |  382 | `		 * was written there, and deprecates the handler for it afterwards. In` |
|      - |  383 | `		 * the running buffer it sits after the bytes the handler was handed. */` |
|    108 |  384 | `		if( nDataLen > 0 ){` |
|      - |  385 | `			sxi32 i;` |
|    108 |  386 | `			for( i = (sxi32)nUsed - 1 ; i >= 0 ; i-- ){` |
|    108 |  387 | `				VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,(sxu32)i);` |
|    108 |  388 | `				if( (pEntry->iFlags & PH7_OB_DISABLED) == 0 \|\| (sxu32)i + 1 == pVm->nObActive ){` |
|    108 |  389 | `					pEntry->iFlags \|= PH7_OB_PRODUCED;` |
|    108 |  390 | `					VmObGrow(pEntry,nDataLen);` |
|    108 |  391 | `					SyBlobAppend(&pEntry->sOB,pData,nDataLen);` |
|    108 |  392 | `					break;` |
|      - |  393 | `				}` |
|    ! 0 |  394 | `			}` |
|     52 |  395 | `		}` |
|    108 |  396 | `		return PH7_OK;` |
|      - |  397 | `	}` |
| 257063 |  398 | `	return VmObSink(pVm,(sxi32)nUsed - 1,pData,nDataLen);` |
| 128586 |  399 | `}` |
|      - |  400 | `/*` |
|      - |  401 | ` * Pop the topmost buffer and release it, restoring the default consumer when the` |
|      - |  402 | ` * stack empties out.` |
|      - |  403 | ` */` |
|  12394 |  404 | `static void VmObPop(ph7_vm *pVm)` |
|      5 |  405 | `{` |
|  12399 |  406 | `	VmObEntry *pEntry = (VmObEntry *)SySetPop(&pVm->aOB);` |
|  12399 |  407 | `	if( pEntry ){` |
|  12399 |  408 | `		VmObRestore(pVm,pEntry);` |
|   6197 |  409 | `	}` |
|  12399 |  410 | `}` |
|      - |  411 | `/*` |
|      - |  412 | ` * Restore the default consumer.` |
|      - |  413 | ` * Refer to the implementation of [ob_end_clean()] for more` |
|      - |  414 | ` * information.` |
|      - |  415 | ` */` |
|  12394 |  416 | `static void VmObRestore(ph7_vm *pVm,VmObEntry *pEntry)` |
|      5 |  417 | `{` |
|  12399 |  418 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  12399 |  419 | `	if( SySetUsed(&pVm->aOB) < 1 ){` |
|      - |  420 | `		/* No more stackable OB */` |
|  11871 |  421 | `		pCons->xConsumer = pCons->xDef;` |
|  11871 |  422 | `		pCons->pUserData = pCons->pDefData;` |
|   5933 |  423 | `	}` |
|      - |  424 | `	/* Release OB data */` |
|  12399 |  425 | `	PH7_MemObjRelease(&pEntry->sCallback);` |
|  12399 |  426 | `	SyBlobRelease(&pEntry->sOB);` |
|  12399 |  427 | `}` |
|      - |  428 | `/*` |
|      - |  429 | ` * php ends and FLUSHES every still-open buffer at shutdown — innermost first, so` |
|      - |  430 | ` * an inner handler's answer is what the outer one is handed. A script that never` |
|      - |  431 | ` * called ob_end_flush() (PHPUnit, which buffers its summary and then exit()s with` |
|      - |  432 | ` * a non-zero status) would otherwise lose that output entirely.` |
|      - |  433 | ` */` |
|   6885 |  434 | `PH7_PRIVATE void PH7_VmObFlushAll(ph7_vm *pVm)` |
|      5 |  435 | `{` |
|   6890 |  436 | `	SyString *pSaveCallee = pVm->pCalleeName;` |
|   6890 |  437 | `	pVm->pCalleeName = &sObShutdown;` |
|   6974 |  438 | `	while( SySetUsed(&pVm->aOB) > 0 ){` |
|      - |  439 | `		ph7_class_instance *pExc;` |
|     88 |  440 | `		VmObPerform(pVm,SySetUsed(&pVm->aOB) - 1,PH7_OB_FINAL,0,&pExc);` |
|     88 |  441 | `		VmObPop(pVm);` |
|     88 |  442 | `		if( pExc ){` |
|      - |  443 | `			/* A handler threw: php reports it fatal and bails out of the flush,` |
|      - |  444 | `			 * discarding every buffer still open without running its handler. */` |
|      3 |  445 | `			VmObRethrow(pVm,pExc,PH7_OK);` |
|      5 |  446 | `			while( SySetUsed(&pVm->aOB) > 0 ){` |
|      3 |  447 | `				VmObPop(pVm);` |
|      1 |  448 | `			}` |
|      1 |  449 | `		}` |
|      4 |  450 | `	}` |
|   6890 |  451 | `	pVm->pCalleeName = pSaveCallee;` |
|   6890 |  452 | `	pVm->nObDepth = 0;` |
|   6890 |  453 | `}` |
|      - |  454 | `/*` |
|      - |  455 | ` * php refuses every ob call that MUTATES the stack while a handler is running —` |
|      - |  456 | ` * the handler IS an operation on that stack, so cleaning, flushing, removing or` |
|      - |  457 | ` * pushing under it has nowhere sane to land. The read-only members` |
|      - |  458 | ` * (ob_get_contents/ob_get_length/ob_get_level/ob_list_handlers) are allowed and` |
|      - |  459 | ` * still answer for the buffer being processed.` |
|      - |  460 | ` *` |
|      - |  461 | ` * Returns TRUE when the call was refused; the caller returns PH7_ABORT.` |
|      - |  462 | ` */` |
|  24880 |  463 | `static int VmObRefuseInHandler(ph7_context *pCtx)` |
|      5 |  464 | `{` |
|  24885 |  465 | `	ph7_vm *pVm = pCtx->pVm;` |
|  24885 |  466 | `	if( !VmObInHandler(pVm) ){` |
|  24881 |  467 | `		return 0;` |
|      - |  468 | `	}` |
|      - |  469 | `	/* The context prefixes "name(): " itself, so each member names itself. */` |
|      6 |  470 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_ERR,` |
|      - |  471 | `		"Cannot use output buffering in output buffering display handlers");` |
|      6 |  472 | `	ph7_result_bool(pCtx,0);` |
|      6 |  473 | `	pVm->iExitStatus = 255;` |
|      6 |  474 | `	pVm->bHaltRequested = 1;` |
|      6 |  475 | `	pVm->bObRefused = 1;` |
|      6 |  476 | `	return 1;` |
|  12445 |  477 | `}` |
|      - |  478 | `/*` |
|      - |  479 | ` * php's name for one buffer's handler: the callable's own display name (a plain` |
|      - |  480 | `` * function name, `Class::method`, `{closure:file:line}`) or, with no handler at`` |
|      - |  481 | ` * all, the literal "default output handler". It reaches ob_list_handlers(),` |
|      - |  482 | ` * ob_get_status() and the refusal notices below — where PHL used to answer` |
|      - |  483 | ` * "Class Method" for every array callback and "default output handler" for every` |
|      - |  484 | ` * CLOSURE, so a closure handler was indistinguishable from none.` |
|      - |  485 | ` */` |
|    308 |  486 | `static void VmObHandlerName(ph7_vm *pVm,VmObEntry *pEntry,SyBlob *pOut)` |
|      4 |  487 | `{` |
|    312 |  488 | `	if( ph7_value_is_callable(&pEntry->sCallback) ){` |
|    182 |  489 | `		PH7_VmCallableName(pVm,&pEntry->sCallback,pOut);` |
|    182 |  490 | `		if( SyBlobLength(pOut) > 0 ){` |
|    182 |  491 | `			return;` |
|      - |  492 | `		}` |
|    ! 0 |  493 | `	}` |
|    133 |  494 | `	SyBlobAppend(pOut,"default output handler",sizeof("default output handler")-1);` |
|    158 |  495 | `}` |
|      - |  496 | `/*` |
|      - |  497 | ` * TRUE when the running builtin is not the innermost function: it called back` |
|      - |  498 | `` * into php code (`array_map('g', ...)`), and php names `g` for what g's echo does.`` |
|      - |  499 | ` * pCalleeName is restored only when the builtin RETURNS, so the frame entered on` |
|      - |  500 | ` * its behalf is what tells the two apart.` |
|      - |  501 | ` */` |
|     72 |  502 | `static int VmObCalleeIsBelow(ph7_vm *pVm)` |
|      4 |  503 | `{` |
|     76 |  504 | `	VmFrame *pFrame = pVm->pFrame ? VmSkipExceptionFrames(pVm->pFrame) : 0;` |
|     77 |  505 | `	return pFrame && pFrame->pParent && (pFrame->iFlags & VM_FRAME_NATIVE_CALLER)` |
|    108 |  506 | `		&& pFrame->pNativeCaller == pVm->pCalleeName;` |
|      4 |  507 | `}` |
|      - |  508 | `/*` |
|      - |  509 | ` * php's "Producing output from user output handler %s is deprecated", qualified` |
|      - |  510 | ` * the way php_error_docref() qualifies it: by the function running when the` |
|      - |  511 | ` * handler was invoked -- the ob_* member, the function whose echo filled a` |
|      - |  512 | ` * chunked buffer ("main" at the top level), or "PHP Request Shutdown" for the` |
|      - |  513 | ` * shutdown flush, which names itself through pCalleeName.` |
|      - |  514 | ` */` |
|     82 |  515 | `static void VmObDeprecateOutput(ph7_vm *pVm,VmObEntry *pEntry)` |
|      4 |  516 | `{` |
|      - |  517 | `	SyBlob sName,sWho,sMsg;` |
|      - |  518 | `	SyString sFunc;` |
|     86 |  519 | `	int bSave = pVm->bObRaising;` |
|     86 |  520 | `	int bShutdown = pVm->pCalleeName == &sObShutdown;` |
|     86 |  521 | `	sxu8 bSaveNoLoc = pVm->bNoFrameLoc;` |
|     86 |  522 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|     86 |  523 | `	SyBlobInit(&sWho,&pVm->sAllocator);` |
|     86 |  524 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     86 |  525 | `	VmObHandlerName(pVm,pEntry,&sName);` |
|     86 |  526 | `	if( bShutdown ){` |
|      - |  527 | `		/* No function and no frame: php words the caller itself, with no "()",` |
|      - |  528 | `		 * and reports it "in Unknown on line 0". */` |
|      6 |  529 | `		SyBlobAppend(&sMsg,"PHP Request Shutdown: ",sizeof("PHP Request Shutdown: ")-1);` |
|      6 |  530 | `		SyStringInitFromBuf(&sFunc,"",0);` |
|      6 |  531 | `		pVm->bNoFrameLoc = 1;` |
|     84 |  532 | `	}else if( pVm->pCalleeName && pVm->pCalleeName->nByte > 0 && !VmObCalleeIsBelow(pVm) ){` |
|     74 |  533 | `		SyStringInitFromBuf(&sFunc,pVm->pCalleeName->zString,pVm->pCalleeName->nByte);` |
|     39 |  534 | `	}else{` |
|     10 |  535 | `		PH7_VmActiveFuncName(pVm,&sWho);` |
|     10 |  536 | `		SyStringInitFromBuf(&sFunc,SyBlobData(&sWho),SyStrlen((const char *)SyBlobData(&sWho)));` |
|      - |  537 | `	}` |
|     86 |  538 | `	SyBlobFormat(&sMsg,"Producing output from user output handler %.*s is deprecated",` |
|     82 |  539 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|     86 |  540 | `	SyBlobNullAppend(&sMsg);` |
|     86 |  541 | `	pVm->bObRaising = 1;` |
|     86 |  542 | `	PH7_VmThrowError(pVm,&sFunc,E_DEPRECATED,(const char *)SyBlobData(&sMsg));` |
|     86 |  543 | `	pVm->bObRaising = bSave;` |
|     86 |  544 | `	pVm->bNoFrameLoc = bSaveNoLoc;` |
|     86 |  545 | `	SyBlobRelease(&sMsg);` |
|     86 |  546 | `	SyBlobRelease(&sWho);` |
|     86 |  547 | `	SyBlobRelease(&sName);` |
|     86 |  548 | `}` |
|      - |  549 | `/*` |
|      - |  550 | ` * Copy one buffer's bytes out. Anything that can run php code — a notice reaching` |
|      - |  551 | ` * a user error handler included — may realloc the buffer stack, so nothing holds` |
|      - |  552 | ` * a VmObEntry pointer across it.` |
|      - |  553 | ` */` |
|   7262 |  554 | `static void VmObSnapshot(ph7_vm *pVm,sxu32 nIdx,SyBlob *pOut)` |
|      5 |  555 | `{` |
|   7267 |  556 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|   7267 |  557 | `	if( pEntry && SyBlobLength(&pEntry->sOB) > 0 ){` |
|   6923 |  558 | `		SyBlobAppend(pOut,SyBlobData(&pEntry->sOB),SyBlobLength(&pEntry->sOB));` |
|   3459 |  559 | `	}` |
|   7267 |  560 | `}` |
|      - |  561 | `/*` |
|      - |  562 | ` * ob_start()'s $flags decide what may be done to the buffer afterwards, and every` |
|      - |  563 | ` * member tests its own bit before it touches anything: CLEANABLE for ob_clean(),` |
|      - |  564 | ` * FLUSHABLE for ob_flush(), REMOVABLE for the four that take the buffer away. A` |
|      - |  565 | ` * refused operation does NOT run the handler and leaves the buffer exactly as it` |
|      - |  566 | `` * was. The argument was declared in `aBuiltinSig[]` and read by nothing, so a`` |
|      - |  567 | ` * buffer opened as un-removable — the standard way a framework pins its own` |
|      - |  568 | ` * output layer in place — could be torn out by any library that called` |
|      - |  569 | ` * ob_end_clean().` |
|      - |  570 | ` *` |
|      - |  571 | `` * `zWhat` is php's verb for this member ("delete"/"flush"/"discard"/"send").`` |
|      - |  572 | ` * Returns TRUE when the operation may proceed.` |
|      - |  573 | ` */` |
|  12474 |  574 | `static int VmObAllows(ph7_context *pCtx,sxu32 nIdx,int iNeed,const char *zWhat)` |
|      5 |  575 | `{` |
|  12479 |  576 | `	ph7_vm *pVm = pCtx->pVm;` |
|  12479 |  577 | `	VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nIdx);` |
|      - |  578 | `	SyBlob sName;` |
|  12479 |  579 | `	if( pEntry == 0 \|\| (pEntry->iFlags & iNeed) != 0 ){` |
|  12371 |  580 | `		return 1;` |
|      - |  581 | `	}` |
|    110 |  582 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    110 |  583 | `	VmObHandlerName(pVm,pEntry,&sName);` |
|    164 |  584 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|     54 |  585 | `		"Failed to %s buffer of %.*s (%u)",zWhat,` |
|    108 |  586 | `		(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName),nIdx);` |
|    110 |  587 | `	SyBlobRelease(&sName);` |
|    110 |  588 | `	return 0;` |
|   6242 |  589 | `}` |
|      - |  590 | `/*` |
|      - |  591 | ` * bool ob_clean(void)` |
|      - |  592 | ` *  This function discards the contents of the output buffer.` |
|      - |  593 | ` *  This function does not destroy the output buffer like ob_end_clean() does.` |
|      - |  594 | ` * Parameter` |
|      - |  595 | ` *  None` |
|      - |  596 | ` * Return` |
|      - |  597 | ` *  TRUE on success, FALSE (with a notice) when no buffer is active. This used to` |
|      - |  598 | `` *  return NOTHING, so `if (!ob_clean())` fired on the successful call.`` |
|      - |  599 | ` */` |
|     30 |  600 | `PH7_PRIVATE int vm_builtin_ob_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  601 | `{` |
|     34 |  602 | `	ph7_vm *pVm = pCtx->pVm;` |
|     34 |  603 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  604 | `	sxi32 rc;` |
|     15 |  605 | `	SXUNUSED(nArg); /* cc warning */` |
|     15 |  606 | `	SXUNUSED(apArg);` |
|     34 |  607 | `	if( VmObRefuseInHandler(pCtx) ){` |
|    ! 0 |  608 | `		return PH7_ABORT;` |
|      - |  609 | `	}` |
|     34 |  610 | `	if( nUsed < 1 ){` |
|      3 |  611 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  612 | `			"Failed to delete buffer. No buffer to delete");` |
|      3 |  613 | `		ph7_result_bool(pCtx,0);` |
|      3 |  614 | `		return PH7_OK;` |
|      - |  615 | `	}` |
|     32 |  616 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_CLEANABLE,"delete") ){` |
|      9 |  617 | `		ph7_result_bool(pCtx,0);` |
|      9 |  618 | `		return PH7_OK;` |
|      - |  619 | `	}` |
|     24 |  620 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN,0,0);` |
|     24 |  621 | `	ph7_result_bool(pCtx,1);` |
|     24 |  622 | `	return rc;` |
|     19 |  623 | `}` |
|      - |  624 | `/*` |
|      - |  625 | ` * bool ob_end_clean(void)` |
|      - |  626 | ` *  Clean (erase) the output buffer and turn off output buffering` |
|      - |  627 | ` *  This function discards the contents of the topmost output buffer and turns` |
|      - |  628 | ` *  off this output buffering. If you want to further process the buffer's contents` |
|      - |  629 | ` *  you have to call ob_get_contents() before ob_end_clean() as the buffer contents` |
|      - |  630 | ` *  are discarded when ob_end_clean() is called.` |
|      - |  631 | ` * Parameter` |
|      - |  632 | ` *  None` |
|      - |  633 | ` * Return` |
|      - |  634 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first that you called` |
|      - |  635 | ` *  the function without an active buffer or that for some reason a buffer could not be deleted` |
|      - |  636 | ` * (possible for special buffer)` |
|      - |  637 | ` */` |
|   5000 |  638 | `PH7_PRIVATE int vm_builtin_ob_end_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  639 | `{` |
|   5005 |  640 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  641 | `	ph7_class_instance *pExc;` |
|   5005 |  642 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  643 | `	sxi32 rc;` |
|   2500 |  644 | `	SXUNUSED(nArg); /* cc warning */` |
|   2500 |  645 | `	SXUNUSED(apArg);` |
|   5005 |  646 | `	if( VmObRefuseInHandler(pCtx) ){` |
|    ! 0 |  647 | `		return PH7_ABORT;` |
|      - |  648 | `	}` |
|   5005 |  649 | `	if( nUsed < 1 ){` |
|      - |  650 | `		/* No such OB,return FALSE */` |
|      6 |  651 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  652 | `			"Failed to delete buffer. No buffer to delete");` |
|      6 |  653 | `		ph7_result_bool(pCtx,0);` |
|      6 |  654 | `		return PH7_OK;` |
|      - |  655 | `	}` |
|   5001 |  656 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){` |
|     24 |  657 | `		ph7_result_bool(pCtx,0);` |
|     24 |  658 | `		return PH7_OK;` |
|      - |  659 | `	}` |
|   4979 |  660 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN\|PH7_OB_FINAL,0,&pExc);` |
|   4979 |  661 | `	VmObPop(pVm);` |
|   4979 |  662 | `	ph7_result_bool(pCtx,1);` |
|   4979 |  663 | `	return VmObRethrow(pVm,pExc,rc);` |
|   2505 |  664 | `}` |
|      - |  665 | `/*` |
|      - |  666 | ` * string ob_get_contents(void)` |
|      - |  667 | ` *  Gets the contents of the output buffer without clearing it.` |
|      - |  668 | ` * Parameter` |
|      - |  669 | ` *  None` |
|      - |  670 | ` * Return` |
|      - |  671 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|      - |  672 | ` *  The bytes are the RAW ones: php runs the handler on the way out, so what a` |
|      - |  673 | ` *  handler will make of them has not happened yet.` |
|      - |  674 | ` */` |
|     26 |  675 | `PH7_PRIVATE int vm_builtin_ob_get_contents(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  676 | `{` |
|     30 |  677 | `	ph7_vm *pVm = pCtx->pVm;` |
|     30 |  678 | `	sxu32 nSeen = VmObVisible(pVm);` |
|     30 |  679 | `	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;` |
|     30 |  680 | `	if( pOb == 0 ){` |
|      - |  681 | `		/* No active OB,return FALSE */` |
|      3 |  682 | `		ph7_result_bool(pCtx,0);` |
|      1 |  683 | `		SXUNUSED(nArg); /* cc warning */` |
|      1 |  684 | `		SXUNUSED(apArg);` |
|      2 |  685 | `	}else{` |
|      - |  686 | `		/* Return contents */` |
|     28 |  687 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&pOb->sOB),(int)SyBlobLength(&pOb->sOB));` |
|      - |  688 | `	}` |
|     30 |  689 | `	return PH7_OK;` |
|      4 |  690 | `}` |
|      - |  691 | `/*` |
|      - |  692 | ` * string ob_get_clean(void)` |
|      - |  693 | ` *  Get current buffer contents and delete current output buffer.` |
|      - |  694 | ` * Parameter` |
|      - |  695 | ` *  None` |
|      - |  696 | ` * Return` |
|      - |  697 | ` *  This will return the contents of the output buffer or FALSE, if output buffering isn't active.` |
|      - |  698 | ` */` |
|   7238 |  699 | `PH7_PRIVATE int vm_builtin_ob_get_clean(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  700 | `{` |
|   7243 |  701 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  702 | `	ph7_class_instance *pExc;` |
|   7243 |  703 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  704 | `	SyBlob sRaw;` |
|      - |  705 | `	sxi32 rc;` |
|   7243 |  706 | `	if( VmObRefuseInHandler(pCtx) ){` |
|    ! 0 |  707 | `		return PH7_ABORT;` |
|      - |  708 | `	}` |
|   7243 |  709 | `	if( nUsed < 1 ){` |
|      - |  710 | `		/* No active OB,return FALSE. php reports every other empty-stack call and` |
|      - |  711 | `		 * stays silent for this one. */` |
|      3 |  712 | `		ph7_result_bool(pCtx,0);` |
|      1 |  713 | `		SXUNUSED(nArg); /* cc warning */` |
|      1 |  714 | `		SXUNUSED(apArg);` |
|      3 |  715 | `		return PH7_OK;` |
|      - |  716 | `	}` |
|   7241 |  717 | `	SyBlobInit(&sRaw,&pVm->sAllocator);` |
|      - |  718 | `	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user` |
|      - |  719 | `	 * error handler, which is php code that may print into this very buffer, and` |
|      - |  720 | `	 * php answers the contents as they were when the call was made. */` |
|   7241 |  721 | `	VmObSnapshot(pVm,nUsed - 1,&sRaw);` |
|   7241 |  722 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"discard") ){` |
|      - |  723 | `		/* php reports the CLEAN and the REMOVAL separately, and still answers the` |
|      - |  724 | `		 * contents it could not take away — as they were BEFORE those reports,` |
|      - |  725 | `		 * which may run a user error handler that writes into this very buffer. */` |
|     26 |  726 | `		VmObAllows(pCtx,nUsed - 1,0,"delete");` |
|     26 |  727 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|     26 |  728 | `		SyBlobRelease(&sRaw);` |
|     26 |  729 | `		return PH7_OK;` |
|      - |  730 | `	}` |
|   7217 |  731 | `	SyBlobReset(&sRaw);` |
|   7217 |  732 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_CLEAN\|PH7_OB_FINAL,&sRaw,&pExc);` |
|   7217 |  733 | `	VmObPop(pVm);` |
|   7217 |  734 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw)); /* Will make it's own copy */` |
|   7217 |  735 | `	SyBlobRelease(&sRaw);` |
|   7217 |  736 | `	return VmObRethrow(pVm,pExc,rc);` |
|   3624 |  737 | `}` |
|      - |  738 | `/*` |
|      - |  739 | ` * string ob_get_flush(void)` |
|      - |  740 | ` *  Flush the output buffer, return it as a string and turn off output buffering.` |
|      - |  741 | ` * Parameter` |
|      - |  742 | ` *  None` |
|      - |  743 | ` * Return` |
|      - |  744 | ` *  The contents of the output buffer, or FALSE if output buffering isn't active.` |
|      - |  745 | ` * Note` |
|      - |  746 | ` *  This used to be registered as ob_get_clean(), which DISCARDS the buffer: the` |
|      - |  747 | ` *  string came back correctly and the output it names never reached the terminal.` |
|      - |  748 | ` */` |
|     28 |  749 | `PH7_PRIVATE int vm_builtin_ob_get_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  750 | `{` |
|     32 |  751 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  752 | `	ph7_class_instance *pExc;` |
|     32 |  753 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  754 | `	SyBlob sRaw;` |
|      - |  755 | `	sxi32 rc;` |
|     32 |  756 | `	if( VmObRefuseInHandler(pCtx) ){` |
|    ! 0 |  757 | `		return PH7_ABORT;` |
|      - |  758 | `	}` |
|     32 |  759 | `	if( nUsed < 1 ){` |
|      - |  760 | `		/* No active OB,return FALSE. php's silent member here is ob_get_CLEAN;` |
|      - |  761 | `		 * this one reports, and the two wordings are php's own. */` |
|      3 |  762 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  763 | `			"Failed to delete and flush buffer. No buffer to delete or flush");` |
|      3 |  764 | `		ph7_result_bool(pCtx,0);` |
|      1 |  765 | `		SXUNUSED(nArg); /* cc warning */` |
|      1 |  766 | `		SXUNUSED(apArg);` |
|      3 |  767 | `		return PH7_OK;` |
|      - |  768 | `	}` |
|     30 |  769 | `	SyBlobInit(&sRaw,&pVm->sAllocator);` |
|      - |  770 | `	/* Snapshot BEFORE the refusal is even tested: the notices below reach a user` |
|      - |  771 | `	 * error handler, which is php code that may print into this very buffer, and` |
|      - |  772 | `	 * php answers the contents as they were when the call was made. */` |
|     30 |  773 | `	VmObSnapshot(pVm,nUsed - 1,&sRaw);` |
|     30 |  774 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){` |
|     10 |  775 | `		VmObAllows(pCtx,nUsed - 1,0,"delete");` |
|     10 |  776 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|     10 |  777 | `		SyBlobRelease(&sRaw);` |
|     10 |  778 | `		return PH7_OK;` |
|      - |  779 | `	}` |
|     22 |  780 | `	SyBlobReset(&sRaw);` |
|     22 |  781 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,&sRaw,&pExc);` |
|     22 |  782 | `	VmObPop(pVm);` |
|      - |  783 | `	/* The answer is the RAW buffer, not what the handler made of it */` |
|     22 |  784 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sRaw),(int)SyBlobLength(&sRaw));` |
|     22 |  785 | `	SyBlobRelease(&sRaw);` |
|     22 |  786 | `	return VmObRethrow(pVm,pExc,rc);` |
|     18 |  787 | `}` |
|      - |  788 | `/*` |
|      - |  789 | ` * int ob_get_length(void)` |
|      - |  790 | ` *  Return the length of the output buffer.` |
|      - |  791 | ` * Parameter` |
|      - |  792 | ` *  None` |
|      - |  793 | ` * Return` |
|      - |  794 | ` *  Returns the length of the output buffer contents or FALSE if no buffering is active.` |
|      - |  795 | ` */` |
|     10 |  796 | `PH7_PRIVATE int vm_builtin_ob_get_length(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  797 | `{` |
|     13 |  798 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  799 | `	sxu32 nSeen = VmObVisible(pVm);` |
|     13 |  800 | `	VmObEntry *pOb = nSeen > 0 ? (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1) : 0;` |
|     13 |  801 | `	if( pOb == 0 ){` |
|      - |  802 | `		/* No active OB,return FALSE */` |
|      3 |  803 | `		ph7_result_bool(pCtx,0);` |
|      1 |  804 | `		SXUNUSED(nArg); /* cc warning */` |
|      1 |  805 | `		SXUNUSED(apArg);` |
|      2 |  806 | `	}else{` |
|      - |  807 | `		/* Return OB length */` |
|     11 |  808 | `		ph7_result_int64(pCtx,(ph7_int64)SyBlobLength(&pOb->sOB));` |
|      - |  809 | `	}` |
|     13 |  810 | `	return PH7_OK;` |
|      3 |  811 | `}` |
|      - |  812 | `/*` |
|      - |  813 | ` * int ob_get_level(void)` |
|      - |  814 | ` *  Returns the nesting level of the output buffering mechanism.` |
|      - |  815 | ` * Parameter` |
|      - |  816 | ` *  None` |
|      - |  817 | ` * Return` |
|      - |  818 | ` *  Returns the level of nested output buffering handlers or zero if output buffering is not active.` |
|      - |  819 | ` */` |
|    310 |  820 | `PH7_PRIVATE int vm_builtin_ob_get_level(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  821 | `{` |
|    315 |  822 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  823 | `	int iNest;` |
|    155 |  824 | `	SXUNUSED(nArg); /* cc warning */` |
|    155 |  825 | `	SXUNUSED(apArg);` |
|      - |  826 | `	/* Nesting level */` |
|    315 |  827 | `	iNest = (int)VmObVisible(pVm);` |
|      - |  828 | `	/* Return the nesting value */` |
|    315 |  829 | `	ph7_result_int(pCtx,iNest);` |
|    315 |  830 | `	return PH7_OK;` |
|      5 |  831 | `}` |
|      - |  832 | `/*` |
|      - |  833 | ` * bool ob_start([ callback $output_callback[, int $chunk_size = 0[, int $flags]]] )` |
|      - |  834 | ` * This function will turn output buffering on. While output buffering is active no output` |
|      - |  835 | ` *  is sent from the script (other than headers), instead the output is stored in an internal` |
|      - |  836 | ` *  buffer.` |
|      - |  837 | ` * Parameter` |
|      - |  838 | ` *  $output_callback` |
|      - |  839 | ` *   An optional output_callback function may be specified. This function takes a string` |
|      - |  840 | ` *   as a parameter and should return a string. The function is called when the buffer is` |
|      - |  841 | ` *   flushed, cleaned or removed, and its second argument says WHICH of those it is (the` |
|      - |  842 | ` *   PHP_OUTPUT_HANDLER_* phase bits). Returning FALSE sends the original bytes and` |
|      - |  843 | ` *   disables the handler for good; returning TRUE sends nothing.` |
|      - |  844 | ` *  $chunk_size` |
|      - |  845 | ` *   Write out as soon as the buffer holds this many bytes.` |
|      - |  846 | ` * Return` |
|      - |  847 | ` *   Returns TRUE on success or FALSE on failure.` |
|      - |  848 | ` */` |
|  12422 |  849 | `PH7_PRIVATE int vm_builtin_ob_start(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  850 | `{` |
|  12427 |  851 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  852 | `	VmObEntry sOb;` |
|      - |  853 | `	sxi32 rc;` |
|      - |  854 | `	/* php has nowhere to put a buffer opened from inside a handler — that handler` |
|      - |  855 | `	 * is mid-operation on the stack this would push onto — and refuses outright. */` |
|  12427 |  856 | `	if( VmObRefuseInHandler(pCtx) ){` |
|      3 |  857 | `		return PH7_ABORT;` |
|      - |  858 | `	}` |
|      - |  859 | `	/* php screens the handler BEFORE it opens the buffer, and a handler it cannot` |
|      - |  860 | `	 * call is a refusal rather than a silent downgrade to the default one: the` |
|      - |  861 | `	 * warning names why (zend's own callable reason) and the notice says no buffer` |
|      - |  862 | `	 * was created. This used to answer TRUE with a buffer whose handler never ran,` |
|      - |  863 | ``	 * so `if (!ob_start('my_filter'))` never fired on a misspelled name and the`` |
|      - |  864 | `	 * output came out unfiltered. */` |
|  12425 |  865 | `	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){` |
|    293 |  866 | `		rc = PH7_VmCallableDeprecation(pVm,apArg[0]);` |
|    293 |  867 | `		if( rc != PH7_OK ){` |
|      3 |  868 | `			return rc; /* the handler threw on it: no buffer, no warning */` |
|      - |  869 | `		}` |
|    291 |  870 | `		if( !PH7_VmIsCallable(pVm,apArg[0],TRUE) ){` |
|      - |  871 | `			char zBuf[256];` |
|     25 |  872 | `			const char *zReason = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));` |
|     25 |  873 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,"%s",zReason);` |
|     25 |  874 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,"Failed to create buffer");` |
|     25 |  875 | `			ph7_result_bool(pCtx,0);` |
|     25 |  876 | `			return PH7_OK;` |
|      - |  877 | `		}` |
|    131 |  878 | `	}` |
|      - |  879 | `	/* Initialize the OB entry */` |
|  12399 |  880 | `	PH7_MemObjInit(pCtx->pVm,&sOb.sCallback);` |
|  12399 |  881 | `	SyBlobInit(&sOb.sOB,&pVm->sAllocator);` |
|      - |  882 | `	/* php keeps whatever it is given except the two nibbles it reserves for` |
|      - |  883 | `	 * itself — the phase bits and the STARTED/DISABLED/PROCESSED state — and` |
|      - |  884 | `	 * reports the rest back verbatim, sign included. */` |
|  12399 |  885 | `	sOb.iFlags = nArg > 2 ? (ph7_value_to_int64(apArg[2]) & PH7_OB_FLAGMASK) : PH7_OB_STDFLAGS;` |
|  12399 |  886 | `	sOb.nChunk = 0;` |
|  12399 |  887 | `	sOb.nSize = 0;` |
|  12399 |  888 | `	if( nArg > 0 && (apArg[0]->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) ){` |
|      - |  889 | `		/* Save the callback name for later invocation (MEMOBJ_OBJ = a Closure callback). */` |
|    267 |  890 | `		PH7_MemObjStore(apArg[0],&sOb.sCallback);` |
|    267 |  891 | `		sOb.iFlags \|= PH7_OB_USER;` |
|    131 |  892 | `	}` |
|  12399 |  893 | `	if( nArg > 1 ){` |
|    156 |  894 | `		ph7_int64 nChunk = ph7_value_to_int64(apArg[1]);` |
|      - |  895 | `		/* A negative chunk size is no chunk size at all, which is what php` |
|      - |  896 | `		 * reports back for one. */` |
|    156 |  897 | `		if( nChunk > 0 ){` |
|     35 |  898 | `			sOb.nChunk = nChunk;` |
|     16 |  899 | `		}` |
|     76 |  900 | `	}` |
|  12399 |  901 | `	sOb.nSize = VmObInitSize(&sOb);` |
|      - |  902 | `	/* Push in the stack */` |
|  12399 |  903 | `	rc = SySetPut(&pVm->aOB,(const void *)&sOb);` |
|  12399 |  904 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  905 | `		PH7_MemObjRelease(&sOb.sCallback);` |
|    ! 0 |  906 | `	}else{` |
|  12399 |  907 | `		ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|      - |  908 | `		/* Substitute the default VM consumer */` |
|  12399 |  909 | `		if( pCons->xConsumer != VmObConsumer ){` |
|  11871 |  910 | `			pCons->xDef = pCons->xConsumer;` |
|  11871 |  911 | `			pCons->pDefData = pCons->pUserData;` |
|      - |  912 | `			/* Install the new consumer */` |
|  11871 |  913 | `			pCons->xConsumer = VmObConsumer;` |
|  11871 |  914 | `			pCons->pUserData = pVm;` |
|   5933 |  915 | `		}` |
|      - |  916 | `	}` |
|  12399 |  917 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|  12399 |  918 | `	return PH7_OK;` |
|   6216 |  919 | `}` |
|      - |  920 | `/*` |
|      - |  921 | ` * bool ob_flush(void)` |
|      - |  922 | ` *  Flush (send) the output buffer.` |
|      - |  923 | ` * Parameter` |
|      - |  924 | ` *  None` |
|      - |  925 | ` * Return` |
|      - |  926 | ` *  TRUE on success, FALSE (with a notice) when no buffer is active.` |
|      - |  927 | ` */` |
|     50 |  928 | `PH7_PRIVATE int vm_builtin_ob_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  929 | `{` |
|     54 |  930 | `	ph7_vm *pVm = pCtx->pVm;` |
|     54 |  931 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  932 | `	sxi32 rc;` |
|     25 |  933 | `	SXUNUSED(nArg); /* cc warning */` |
|     25 |  934 | `	SXUNUSED(apArg);` |
|     54 |  935 | `	if( VmObRefuseInHandler(pCtx) ){` |
|      3 |  936 | `		return PH7_ABORT;` |
|      - |  937 | `	}` |
|     52 |  938 | `	if( nUsed < 1 ){` |
|      3 |  939 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  940 | `			"Failed to flush buffer. No buffer to flush");` |
|      3 |  941 | `		ph7_result_bool(pCtx,0);` |
|      3 |  942 | `		return PH7_OK;` |
|      - |  943 | `	}` |
|     50 |  944 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_FLUSHABLE,"flush") ){` |
|      9 |  945 | `		ph7_result_bool(pCtx,0);` |
|      9 |  946 | `		return PH7_OK;` |
|      - |  947 | `	}` |
|     42 |  948 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FLUSH,0,0);` |
|     42 |  949 | `	ph7_result_bool(pCtx,1);` |
|     42 |  950 | `	return rc;` |
|     29 |  951 | `}` |
|      - |  952 | `/*` |
|      - |  953 | ` * void flush(void)` |
|      - |  954 | ` *  Flush the output layer to the SAPI. It does NOT touch the userland output` |
|      - |  955 | `` *  buffers: `ob_start(); echo "a"; flush();` leaves "a" in the buffer under php,`` |
|      - |  956 | ` *  where this used to be registered as ob_flush() and SENT it — so a progress-bar` |
|      - |  957 | ` *  idiom (echo, flush(), keep working) emptied a buffer the script meant to read` |
|      - |  958 | ` *  back later, and ob_get_contents() answered "".` |
|      - |  959 | ` *  This engine's own consumer writes with an unbuffered write(2)/WriteFile(), so` |
|      - |  960 | ` *  there is nothing left to push and the call is a no-op that answers nothing,` |
|      - |  961 | `` *  which is php's `void` return.`` |
|      - |  962 | ` * Parameter` |
|      - |  963 | ` *  None` |
|      - |  964 | ` * Return` |
|      - |  965 | ` *  No value is returned.` |
|      - |  966 | ` */` |
|      2 |  967 | `PH7_PRIVATE int vm_builtin_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  968 | `{` |
|      1 |  969 | `	SXUNUSED(pCtx);` |
|      1 |  970 | `	SXUNUSED(nArg); /* cc warning */` |
|      1 |  971 | `	SXUNUSED(apArg);` |
|      3 |  972 | `	return PH7_OK;` |
|      1 |  973 | `}` |
|      - |  974 | `/*` |
|      - |  975 | ` * bool ob_end_flush(void)` |
|      - |  976 | ` *  Flush (send) the output buffer and turn off output buffering.` |
|      - |  977 | ` * Parameter` |
|      - |  978 | ` *  None` |
|      - |  979 | ` * Return` |
|      - |  980 | ` *  Returns TRUE on success or FALSE on failure. Reasons for failure are first` |
|      - |  981 | ` *  that you called the function without an active buffer or that for some reason` |
|      - |  982 | ` *  a buffer could not be deleted (possible for special buffer).` |
|      - |  983 | ` */` |
|    112 |  984 | `PH7_PRIVATE int vm_builtin_ob_end_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  985 | `{` |
|    117 |  986 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  987 | `	ph7_class_instance *pExc;` |
|    117 |  988 | `	sxu32 nUsed = SySetUsed(&pVm->aOB);` |
|      - |  989 | `	sxi32 rc;` |
|     56 |  990 | `	SXUNUSED(nArg); /* cc warning */` |
|     56 |  991 | `	SXUNUSED(apArg);` |
|    117 |  992 | `	if( VmObRefuseInHandler(pCtx) ){` |
|    ! 0 |  993 | `		return PH7_ABORT;` |
|      - |  994 | `	}` |
|    117 |  995 | `	if( nUsed < 1 ){` |
|      - |  996 | `		/* Empty stack,return FALSE */` |
|      3 |  997 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_NOTICE,` |
|      - |  998 | `			"Failed to delete and flush buffer. No buffer to delete or flush");` |
|      3 |  999 | `		ph7_result_bool(pCtx,0);` |
|      3 | 1000 | `		return PH7_OK;` |
|      - | 1001 | `	}` |
|    115 | 1002 | `	if( !VmObAllows(pCtx,nUsed - 1,PH7_OB_REMOVABLE,"send") ){` |
|      7 | 1003 | `		ph7_result_bool(pCtx,0);` |
|      7 | 1004 | `		return PH7_OK;` |
|      - | 1005 | `	}` |
|    109 | 1006 | `	rc = VmObPerform(pVm,nUsed - 1,PH7_OB_FINAL,0,&pExc);` |
|    109 | 1007 | `	VmObPop(pVm);` |
|      - | 1008 | `	/* Return true */` |
|    109 | 1009 | `	ph7_result_bool(pCtx,1);` |
|    109 | 1010 | `	return VmObRethrow(pVm,pExc,rc);` |
|     61 | 1011 | `}` |
|      - | 1012 | `/*` |
|      - | 1013 | ` * void ob_implicit_flush([int $flag = true ])` |
|      - | 1014 | ` *  ob_implicit_flush() will turn implicit flushing on or off.` |
|      - | 1015 | ` *  Implicit flushing will result in a flush operation after every` |
|      - | 1016 | ` *  output call, so that explicit calls to flush() will no longer be needed.` |
|      - | 1017 | ` * Parameter` |
|      - | 1018 | ` *  $flag` |
|      - | 1019 | ` *   TRUE to turn implicit flushing on, FALSE otherwise.` |
|      - | 1020 | ` * Return` |
|      - | 1021 | ` *   Nothing` |
|      - | 1022 | ` */` |
|      4 | 1023 | `PH7_PRIVATE int vm_builtin_ob_implicit_flush(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1024 | `{` |
|      - | 1025 | `	/* NOTE: As of this version,this function is a no-op.` |
|      - | 1026 | `	 * PH7 is smart enough to flush it's internal buffer when appropriate.` |
|      - | 1027 | `	 */` |
|      2 | 1028 | `	SXUNUSED(pCtx);` |
|      2 | 1029 | `	SXUNUSED(nArg); /* cc warning */` |
|      2 | 1030 | `	SXUNUSED(apArg);` |
|      5 | 1031 | `	return PH7_OK;` |
|      1 | 1032 | `}` |
|      - | 1033 | `/*` |
|      - | 1034 | ` * php's buffer SIZE bookkeeping, reproduced so ob_get_status() answers php's` |
|      - | 1035 | ` * number rather than this engine's blob capacity. The initial allocation is` |
|      - | 1036 | ` * 16 KB, or the chunk size rounded up to a 4 KB boundary when one was asked for;` |
|      - | 1037 | ` * a write that would not fit grows it by whichever is larger of that initial size` |
|      - | 1038 | ` * and the shortfall rounded the same way.` |
|      - | 1039 | ` */` |
|      - | 1040 | `#define PH7_OB_ALIGN(n)   ((((ph7_int64)(n)) + 0xFFF) & ~(ph7_int64)0xFFF)` |
|      - | 1041 | `#define PH7_OB_DEFSIZE    0x4000` |
|  12426 | 1042 | `static ph7_int64 VmObInitSize(VmObEntry *pEntry)` |
|      5 | 1043 | `{` |
|  12431 | 1044 | `	return pEntry->nChunk > 0 ? PH7_OB_ALIGN(pEntry->nChunk) : PH7_OB_DEFSIZE;` |
|      5 | 1045 | `}` |
| 257276 | 1046 | `static void VmObGrow(VmObEntry *pEntry,sxu32 nIncoming)` |
|      5 | 1047 | `{` |
| 257281 | 1048 | `	ph7_int64 nUsed = (ph7_int64)SyBlobLength(&pEntry->sOB);` |
| 257281 | 1049 | `	ph7_int64 nFree = pEntry->nSize > nUsed ? pEntry->nSize - nUsed : 0;` |
| 257281 | 1050 | `	if( nFree <= (ph7_int64)nIncoming ){` |
|     34 | 1051 | `		ph7_int64 nInit = VmObInitSize(pEntry);` |
|     34 | 1052 | `		ph7_int64 nGrow = PH7_OB_ALIGN((ph7_int64)nIncoming - nFree);` |
|     34 | 1053 | `		pEntry->nSize += nGrow > nInit ? nGrow : nInit;` |
|     16 | 1054 | `	}` |
| 257281 | 1055 | `}` |
|      - | 1056 | `/*` |
|      - | 1057 | ` * Describe one buffer the way ob_get_status() does.` |
|      - | 1058 | ` */` |
|    100 | 1059 | `static void VmObStatusEntry(ph7_context *pCtx,VmObEntry *pEntry,sxu32 nIdx,ph7_value *pOut)` |
|      1 | 1060 | `{` |
|    101 | 1061 | `	ph7_vm *pVm = pCtx->pVm;` |
|    101 | 1062 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    101 | 1063 | `	ph7_int64 iFlags = pEntry->iFlags;` |
|      - | 1064 | `	SyBlob sName;` |
|    101 | 1065 | `	if( pVal == 0 ){` |
|    ! 0 | 1066 | `		return;` |
|      - | 1067 | `	}` |
|    101 | 1068 | `	if( VmObInHandler(pVm) && pVm->nObActive == nIdx + 1 ){` |
|      - | 1069 | `		/* Asking from inside this buffer's own handler: the DISABLED mark it wears` |
|      - | 1070 | `		 * for the duration of the call is bookkeeping, not an answer. */` |
|      5 | 1071 | `		iFlags &= ~PH7_OB_DISABLED;` |
|      2 | 1072 | `	}` |
|    101 | 1073 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|    101 | 1074 | `	VmObHandlerName(pVm,pEntry,&sName);` |
|    101 | 1075 | `	ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|    101 | 1076 | `	ph7_array_add_strkey_elem(pOut,"name",pVal);` |
|    101 | 1077 | `	SyBlobRelease(&sName);` |
|    101 | 1078 | `	ph7_value_int(pVal,(iFlags & PH7_OB_USER) ? 1 : 0);` |
|    101 | 1079 | `	ph7_array_add_strkey_elem(pOut,"type",pVal);` |
|    101 | 1080 | `	ph7_value_int64(pVal,iFlags);` |
|    101 | 1081 | `	ph7_array_add_strkey_elem(pOut,"flags",pVal);` |
|    101 | 1082 | `	ph7_value_int64(pVal,(ph7_int64)nIdx);` |
|    101 | 1083 | `	ph7_array_add_strkey_elem(pOut,"level",pVal);` |
|    101 | 1084 | `	ph7_value_int64(pVal,pEntry->nChunk);` |
|    101 | 1085 | `	ph7_array_add_strkey_elem(pOut,"chunk_size",pVal);` |
|      - | 1086 | `	/* A buffer whose handler failed is not buffering at all, and php reports the` |
|      - | 1087 | `	 * allocation it dropped: 0. */` |
|    101 | 1088 | `	ph7_value_int64(pVal,(iFlags & PH7_OB_DISABLED) ? 0 : pEntry->nSize);` |
|    101 | 1089 | `	ph7_array_add_strkey_elem(pOut,"buffer_size",pVal);` |
|    101 | 1090 | `	ph7_value_int64(pVal,(ph7_int64)SyBlobLength(&pEntry->sOB));` |
|    101 | 1091 | `	ph7_array_add_strkey_elem(pOut,"buffer_used",pVal);` |
|    101 | 1092 | `	ph7_context_release_value(pCtx,pVal);` |
|     51 | 1093 | `}` |
|      - | 1094 | `/*` |
|      - | 1095 | ` * array ob_get_status([bool $full_status = false])` |
|      - | 1096 | ` *  Describe the active output buffers: the TOPMOST one by default (an empty array` |
|      - | 1097 | ` *  when nothing is buffering), or every one of them, outermost first, when asked` |
|      - | 1098 | ` *  for the full status.` |
|      - | 1099 | ` * Note` |
|      - | 1100 | ` *  This function did not exist here at all, so the standard way to ask what an` |
|      - | 1101 | `` *  output handler is and what it may do — `ob_get_status()['flags']` — was an`` |
|      - | 1102 | ` *  undefined-function fatal, and so was every framework probe that guards on it.` |
|      - | 1103 | ` */` |
|     94 | 1104 | `PH7_PRIVATE int vm_builtin_ob_get_status(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1105 | `{` |
|     95 | 1106 | `	ph7_vm *pVm = pCtx->pVm;` |
|     95 | 1107 | `	sxu32 nSeen = VmObVisible(pVm);` |
|     95 | 1108 | `	int bFull = nArg > 0 && ph7_value_to_bool(apArg[0]);` |
|     95 | 1109 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     95 | 1110 | `	if( pArray == 0 ){` |
|    ! 0 | 1111 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1112 | `		return PH7_OK;` |
|      - | 1113 | `	}` |
|     95 | 1114 | `	if( bFull ){` |
|      - | 1115 | `		sxu32 n;` |
|     17 | 1116 | `		for( n = 0 ; n < nSeen ; ++n ){` |
|     13 | 1117 | `			VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,n);` |
|     13 | 1118 | `			ph7_value *pOne = ph7_context_new_array(pCtx);` |
|     13 | 1119 | `			if( pEntry == 0 \|\| pOne == 0 ){` |
|    ! 0 | 1120 | `				continue;` |
|      - | 1121 | `			}` |
|     13 | 1122 | `			VmObStatusEntry(pCtx,pEntry,n,pOne);` |
|     13 | 1123 | `			ph7_array_add_elem(pArray,0,pOne);` |
|     13 | 1124 | `			ph7_context_release_value(pCtx,pOne);` |
|      7 | 1125 | `		}` |
|     93 | 1126 | `	}else if( nSeen > 0 ){` |
|     89 | 1127 | `		VmObEntry *pEntry = (VmObEntry *)SySetAt(&pVm->aOB,nSeen - 1);` |
|     89 | 1128 | `		if( pEntry ){` |
|     89 | 1129 | `			VmObStatusEntry(pCtx,pEntry,nSeen - 1,pArray);` |
|     44 | 1130 | `		}` |
|     44 | 1131 | `	}` |
|     95 | 1132 | `	ph7_result_value(pCtx,pArray);` |
|     95 | 1133 | `	return PH7_OK;` |
|     48 | 1134 | `}` |
|      - | 1135 | `/*` |
|      - | 1136 | ` * array ob_list_handlers(void)` |
|      - | 1137 | ` *  Lists all output handlers in use.` |
|      - | 1138 | ` * Parameter` |
|      - | 1139 | ` *  None` |
|      - | 1140 | ` * Return` |
|      - | 1141 | ` *  This will return an array with the output handlers in use (if any).` |
|      - | 1142 | ` */` |
|     14 | 1143 | `PH7_PRIVATE int vm_builtin_ob_list_handlers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1144 | `{` |
|     17 | 1145 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1146 | `	ph7_value *pArray;` |
|      - | 1147 | `	VmObEntry *aEntry;` |
|      - | 1148 | `	ph7_value sVal;` |
|      - | 1149 | `	sxu32 n;` |
|      - | 1150 | `	/* Create a new array. php answers an EMPTY array when nothing is buffering` |
|      - | 1151 | ``	 * (`foreach (ob_list_handlers() as ...)` over the NULL this used to answer is`` |
|      - | 1152 | `	 * a TypeError), so the array is built before the stack is looked at. */` |
|     17 | 1153 | `	pArray = ph7_context_new_array(pCtx);` |
|     17 | 1154 | `	if( pArray == 0 ){` |
|      - | 1155 | `		/* Out of memory,return NULL */` |
|    ! 0 | 1156 | `		SXUNUSED(nArg); /* cc warning */` |
|    ! 0 | 1157 | `		SXUNUSED(apArg);` |
|    ! 0 | 1158 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1159 | `		return PH7_OK;` |
|      - | 1160 | `	}` |
|     17 | 1161 | `	PH7_MemObjInit(pVm,&sVal);` |
|      - | 1162 | `	/* Point to the installed OB entries */` |
|     17 | 1163 | `	aEntry = (VmObEntry *)SySetBasePtr(&pVm->aOB);` |
|      - | 1164 | `	/* Perform the requested operation */` |
|     35 | 1165 | `	for( n = 0 ; n < VmObVisible(pVm) ; n++ ){` |
|     21 | 1166 | `		VmObEntry *pEntry = &aEntry[n];` |
|      - | 1167 | `		/* Extract handler name */` |
|     21 | 1168 | `		SyBlobReset(&sVal.sBlob);` |
|     21 | 1169 | `		VmObHandlerName(pVm,pEntry,&sVal.sBlob);` |
|     21 | 1170 | `		sVal.iFlags = MEMOBJ_STRING;` |
|      - | 1171 | `		/* Perform the insertion */` |
|     21 | 1172 | `		ph7_array_add_elem(pArray,0/* Automatic index assign */,&sVal /* Will make it's own copy */);` |
|     12 | 1173 | `	}` |
|     17 | 1174 | `	PH7_MemObjRelease(&sVal);` |
|      - | 1175 | `	/* Return the freshly created array */` |
|     17 | 1176 | `	ph7_result_value(pCtx,pArray);` |
|     17 | 1177 | `	return PH7_OK;` |
|     10 | 1178 | `}` |
|      - | 1179 |  |
