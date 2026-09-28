# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 507/531 lines (95.48%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/parser.h>` |
|      - |    8 | `#include <libxml/xmlerror.h>` |
|      - |    9 | `#include <libxml/tree.h>` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * Shared libxml2 plumbing for the DOM / XMLWriter / libxml_* surfaces.` |
|      - |   13 | ` *` |
|      - |   14 | ` * Memory model: libxml2 stays on its own (system) allocator on purpose.` |
|      - |   15 | ` * Routing it through SyMemBackend would subject libxml2 internals to the` |
|      - |   16 | ` * PHL_MAX_ALLOC fault-injection used by the stress tier, and libxml2 does` |
|      - |   17 | ` * not tolerate mid-parse OOM injection the way the engine's own code does.` |
|      - |   18 | ` * The cost is that allocation-stress tests do not exercise libxml2 OOM` |
|      - |   19 | ` * paths.` |
|      - |   20 | ` *` |
|      - |   21 | ` * Lifetime model: PH7 resources (MEMOBJ_RES) carry no destructor hook, so` |
|      - |   22 | ` * every xmlDoc created on behalf of PHP code is owned by a phl_xmldoc entry` |
|      - |   23 | ` * chained on the per-VM registry (pVm->pXmlDocs) and freed only when the VM` |
|      - |   24 | ` * is reset (a new request on a reused VM) or released -- never while PHP` |
|      - |   25 | ` * code could still hold a node wrapper into it.  Nodes unlinked from their` |
|      - |   26 | ` * tree (removeChild/replaceChild) are parked on the owning phl_xmldoc's` |
|      - |   27 | ` * aOrphans set and freed with the doc.  Docs therefore accumulate until VM` |
|      - |   28 | ` * teardown; acceptable for CLI/per-request VMs, revisit with __destruct-` |
|      - |   29 | ` * driven refcounting if long-lived embeddings ever need early release.` |
|      - |   30 | ` */` |
|      - |   31 |  |
|      - |   32 | `/*` |
|      - |   33 | ` * One-time process-global libxml2 initialization.  Safe as a plain static` |
|      - |   34 | ` * flag under PHL's current single-threaded-execution model (see the` |
|      - |   35 | ` * equivalent note in vm_pcre.c); xmlCleanupParser() is deliberately never` |
|      - |   36 | ` * called -- it is unsafe with threads and process exit reclaims everything.` |
|      - |   37 | ` */` |
|   5740 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   5745 |   41 | `	if( !bInit ){` |
|   5745 |   42 | `		xmlInitParser();` |
|   5745 |   43 | `		bInit = 1;` |
|   2870 |   44 | `	}` |
|   5745 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|   3252 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      5 |   52 | `{` |
|   3257 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	xmlEntityPtr *apNot;` |
|      - |   55 | `	sxu32 n;` |
|   4095 |   56 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|    841 |   57 | `		xmlUnlinkNode(apOrphan[n]);` |
|    841 |   58 | `		xmlFreeNode(apOrphan[n]);` |
|    422 |   59 | `	}` |
|   3257 |   60 | `	SySetRelease(&pDoc->aOrphans);` |
|      - |   61 | `	/* The synthesized notation nodes, freed field by field as php frees its` |
|      - |   62 | ``	 * own: xmlFreeNode would read the entity's `length`/`etype` pair as a`` |
|      - |   63 | `	 * node's property list and walk into it. */` |
|   3257 |   64 | `	apNot = (xmlEntityPtr *)SySetBasePtr(&pDoc->aNotations);` |
|   3267 |   65 | `	for( n = 0 ; n < SySetUsed(&pDoc->aNotations) ; ++n ){` |
|     12 |   66 | `		if( apNot[n]->name ){` |
|     12 |   67 | `			xmlFree((xmlChar *)apNot[n]->name);` |
|      5 |   68 | `		}` |
|     12 |   69 | `		if( apNot[n]->ExternalID ){` |
|      5 |   70 | `			xmlFree((xmlChar *)apNot[n]->ExternalID);` |
|      2 |   71 | `		}` |
|     12 |   72 | `		if( apNot[n]->SystemID ){` |
|     10 |   73 | `			xmlFree((xmlChar *)apNot[n]->SystemID);` |
|      4 |   74 | `		}` |
|     12 |   75 | `		xmlFree(apNot[n]);` |
|      7 |   76 | `	}` |
|   3257 |   77 | `	SySetRelease(&pDoc->aNotations);` |
|   3257 |   78 | `	if( pDoc->pDoc ){` |
|   3255 |   79 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|   3255 |   80 | `		pDoc->pDoc = 0;` |
|   1625 |   81 | `	}` |
|   3257 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   85 | ` * error queue and free every document from the previous request.  Called` |
|      - |   86 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   87 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   88 | ` */` |
|   4974 |   89 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   90 | `{` |
|      - |   91 | `	phl_xmldoc *pDoc,*pNext;` |
|   4979 |   92 | `	PH7_LibxmlClearErrors(pVm);` |
|      - |   93 | `	/* A held message fragment is per-request state: a reused VM must not print` |
|      - |   94 | `	 * the previous request's tail joined to this one's first diagnostic. */` |
|   4979 |   95 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|   4979 |   96 | `	pVm->bLibxmlInternalErr = 0;` |
|   4979 |   97 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|   8231 |   98 | `	while( pDoc ){` |
|   3257 |   99 | `		pNext = pDoc->pNext;` |
|   3257 |  100 | `		LibxmlFreeDoc(pDoc);` |
|   3257 |  101 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|   3257 |  102 | `		pDoc = pNext;` |
|      5 |  103 | `	}` |
|   4979 |  104 | `	pVm->pXmlDocs = 0;` |
|      - |  105 | `	/* The ownerless shell rode the chain just freed. */` |
|   4979 |  106 | `	pVm->pXmlLimbo = 0;` |
|      - |  107 | `	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */` |
|   4979 |  108 | `	PH7_XmlWriterVmSweep(pVm);` |
|      - |  109 | `	/* ext/xml push parsers: their ctxt/myDoc are libxml allocations and the` |
|      - |  110 | `	 * handler VALUES hold references that must drop before the allocator goes. */` |
|   4979 |  111 | `	PH7_XmlParserVmSweep(pVm);` |
|      - |  112 | `	/* The entity-loader/streams-context slots hold per-request VALUES (a` |
|      - |  113 | `	 * closure, a context resource): drop them so a reused VM starts default. */` |
|   4979 |  114 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|   4979 |  115 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|   4979 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |  119 | ` * wipes the allocator that holds the registry shells.` |
|      - |  120 | ` */` |
|   4958 |  121 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |  122 | `{` |
|   4963 |  123 | `	PH7_LibxmlVmReset(pVm);` |
|   4963 |  124 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   4963 |  125 | `	SyBlobRelease(&pVm->sLibxmlPend);` |
|   4963 |  126 | `}` |
|      - |  127 | `/*` |
|      - |  128 | ` * Release the copied message/file strings of one queue entry.` |
|      - |  129 | ` */` |
|    394 |  130 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      1 |  131 | `{` |
|    395 |  132 | `	if( pErr->sMsg.zString ){` |
|    395 |  133 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|    184 |  134 | `	}` |
|    395 |  135 | `	if( pErr->sFile.zString ){` |
|     63 |  136 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|     26 |  137 | `	}` |
|    395 |  138 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|    395 |  139 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|    395 |  140 | `}` |
|      - |  141 | `/*` |
|      - |  142 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  143 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  144 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  145 | ` */` |
|   5222 |  146 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  147 | `{` |
|   5227 |  148 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  149 | `	sxu32 n;` |
|   5309 |  150 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     83 |  151 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|     40 |  152 | `	}` |
|   5227 |  153 | `	SySetReset(&pVm->aLibxmlErr);` |
|   5227 |  154 | `}` |
|      - |  155 | `/*` |
|      - |  156 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  157 | ` */` |
|   5120 |  158 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  159 | `{` |
|   5125 |  160 | `	LibxmlClearQueue(pVm);` |
|   5125 |  161 | `	if( pVm->pLibxmlLastErr ){` |
|     57 |  162 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|     57 |  163 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|     57 |  164 | `		pVm->pLibxmlLastErr = 0;` |
|     28 |  165 | `	}` |
|   5125 |  166 | `}` |
|      - |  167 | `/*` |
|      - |  168 | ` * Push one error onto the per-VM queue and last-error slot, copying the` |
|      - |  169 | ` * message/file strings.  The typed structured-error callback below and the` |
|      - |  170 | ` * DOM schema hooks (vm_dom.c) both funnel through this, keeping the` |
|      - |  171 | ` * queue-building logic in one place and ph7int.h free of libxml types.` |
|      - |  172 | ` */` |
|    197 |  173 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|      - |  174 | `	const char *zMsg,const char *zFile)` |
|      1 |  175 | `{` |
|      - |  176 | `	phl_libxml_err sEntry;` |
|      - |  177 | `	phl_libxml_err *pLast;` |
|    198 |  178 | `	if( pVm == 0 ){` |
|    ! 0 |  179 | `		return;` |
|      - |  180 | `	}` |
|    198 |  181 | `	SyZero(&sEntry,sizeof(sEntry));` |
|    198 |  182 | `	sEntry.iLevel = iLevel;` |
|    198 |  183 | `	sEntry.iCode = iCode;` |
|    198 |  184 | `	sEntry.iLine = iLine;` |
|    198 |  185 | `	sEntry.iColumn = iColumn;` |
|    198 |  186 | `	if( zMsg ){` |
|    198 |  187 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|    198 |  188 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);` |
|    198 |  189 | `		if( zDup ){` |
|      - |  190 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|    198 |  191 | `			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);` |
|     92 |  192 | `		}` |
|     92 |  193 | `	}` |
|    198 |  194 | `	if( zFile ){` |
|     32 |  195 | `		sxu32 nFile = SyStrlen(zFile);` |
|     32 |  196 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);` |
|     32 |  197 | `		if( zDup ){` |
|     32 |  198 | `			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);` |
|     13 |  199 | `		}` |
|     13 |  200 | `	}` |
|    198 |  201 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  202 | `	/* Mirror into the last-error slot (independent string copies so queue` |
|      - |  203 | `	 * draining cannot invalidate it). */` |
|    198 |  204 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|    198 |  205 | `	if( pLast == 0 ){` |
|     57 |  206 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|     57 |  207 | `		if( pLast == 0 ){` |
|    ! 0 |  208 | `			return;` |
|      - |  209 | `		}` |
|     57 |  210 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|     57 |  211 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|     29 |  212 | `	}else{` |
|    142 |  213 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  214 | `	}` |
|    198 |  215 | `	pLast->iLevel = iLevel;` |
|    198 |  216 | `	pLast->iCode = iCode;` |
|    198 |  217 | `	pLast->iLine = iLine;` |
|    198 |  218 | `	pLast->iColumn = iColumn;` |
|    198 |  219 | `	if( sEntry.sMsg.zString ){` |
|    198 |  220 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|    198 |  221 | `		if( zDup ){` |
|    198 |  222 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|     92 |  223 | `		}` |
|     92 |  224 | `	}` |
|    198 |  225 | `	if( sEntry.sFile.zString ){` |
|     32 |  226 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|     32 |  227 | `		if( zDup ){` |
|     32 |  228 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|     13 |  229 | `		}` |
|     13 |  230 | `	}` |
|     93 |  231 | `}` |
|      - |  232 | `/*` |
|      - |  233 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  234 | ` * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;` |
|      - |  235 | ` * the capture-end decides whether entries stay queued or drain as` |
|      - |  236 | ` * php-style warnings.` |
|      - |  237 | ` */` |
|      - |  238 | `#if LIBXML_VERSION >= 21200` |
|     75 |  239 | `static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)` |
|      - |  240 | `#else` |
|     87 |  241 | `static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  242 | `#endif` |
|      1 |  243 | `{` |
|    163 |  244 | `	if( pErr == 0 ){` |
|    ! 0 |  245 | `		return;` |
|      - |  246 | `	}` |
|      - |  247 | `	/* libxml keeps the column in int2 */` |
|    238 |  248 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|    162 |  249 | `		pErr->int2,pErr->message,pErr->file);` |
|     76 |  250 | `}` |
|      - |  251 | `/*` |
|      - |  252 | ` * ...and libxml's OTHER error channel. A handful of diagnostics never reach` |
|      - |  253 | ` * the structured handler at all: they are printed with xmlGenericError()` |
|      - |  254 | ` * directly, whose default writes them to stderr. XPath is where a program` |
|      - |  255 | `` * meets them -- `zz:nope()` under an unbound prefix says "xmlXPathCompOpEval:`` |
|      - |  256 | ` * function nope bound to undefined prefix zz" through this channel -- and` |
|      - |  257 | ` * before this handler they went to the terminal, invisible to` |
|      - |  258 | ` * libxml_get_errors() and to a program's error handler alike, in the middle` |
|      - |  259 | ` * of whatever the script was writing. php captures them, at libxml's ERROR` |
|      - |  260 | ` * level under code 1 with no file or line, and this says the same.` |
|      - |  261 | ` *` |
|      - |  262 | ` * The signature is printf-style, so the message is formatted here.` |
|      - |  263 | ` */` |
|      5 |  264 | `static void LibxmlGenericErr(void *pUserData,const char *zFmt,...)` |
|    ! 0 |  265 | `{` |
|      5 |  266 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  267 | `	SyBlob sMsg;` |
|      - |  268 | `	va_list ap;` |
|      5 |  269 | `	if( pVm == 0 \|\| zFmt == 0 ){` |
|    ! 0 |  270 | `		return;` |
|      - |  271 | `	}` |
|      5 |  272 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      5 |  273 | `	va_start(ap,zFmt);` |
|      5 |  274 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|      5 |  275 | `	va_end(ap);` |
|      - |  276 | `	/* php's own generic handler is line-buffered and reports what it flushed` |
|      - |  277 | `	 * WITHOUT the newline, where a structured message keeps its own -- this` |
|      - |  278 | `	 * channel's messages arrive whole and newline-terminated. */` |
|      - |  279 | `	{` |
|      5 |  280 | `		char *zMsg = (char *)SyBlobData(&sMsg);` |
|      5 |  281 | `		sxu32 nMsg = SyBlobLength(&sMsg);` |
|     10 |  282 | `		while( nMsg > 0 && (zMsg[nMsg-1] == '\n' \|\| zMsg[nMsg-1] == '\r') ){` |
|      5 |  283 | `			nMsg--;` |
|    ! 0 |  284 | `		}` |
|      5 |  285 | `		sMsg.nByte = nMsg;` |
|      - |  286 | `	}` |
|      5 |  287 | `	SyBlobAppend(&sMsg,"",1);   /* the queue copies a C string */` |
|      - |  288 | `	{` |
|      - |  289 | `		/* The channel mark goes on the entry the queue just took -- confirmed` |
|      - |  290 | `		 * by the depth having GROWN, so an insertion that failed cannot leave` |
|      - |  291 | `		 * the mark on the message before it. */` |
|      5 |  292 | `		sxu32 nBefore = SySetUsed(&pVm->aLibxmlErr);` |
|      5 |  293 | `		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,1,0,0,(const char *)SyBlobData(&sMsg),"");` |
|      5 |  294 | `		if( SySetUsed(&pVm->aLibxmlErr) > nBefore ){` |
|      5 |  295 | `			phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      5 |  296 | `			aErr[SySetUsed(&pVm->aLibxmlErr)-1].bWholeLine = 1;` |
|      5 |  297 | `			if( pVm->pLibxmlLastErr ){` |
|      5 |  298 | `				((phl_libxml_err *)pVm->pLibxmlLastErr)->bWholeLine = 1;` |
|    ! 0 |  299 | `			}` |
|    ! 0 |  300 | `		}` |
|      - |  301 | `	}` |
|      5 |  302 | `	SyBlobRelease(&sMsg);` |
|    ! 0 |  303 | `}` |
|      - |  304 | `/* xmlSetGenericErrorFunc is deprecated from libxml 2.12 and the MSVC gate` |
|      - |  305 | ` * refuses a deprecated symbol under /WX. It is still the only door onto that` |
|      - |  306 | ` * channel, so the deprecation is suppressed at this one call pair. */` |
|   8826 |  307 | `static void LibxmlGenericSet(ph7_vm *pVm,int bOn)` |
|      5 |  308 | `{` |
|      - |  309 | `#if defined(_MSC_VER)` |
|      - |  310 | `#pragma warning(push)` |
|      - |  311 | `#pragma warning(disable:4996)` |
|      - |  312 | `#endif` |
|   8831 |  313 | `	xmlSetGenericErrorFunc(bOn ? (void *)pVm : 0,bOn ? LibxmlGenericErr : 0);` |
|      - |  314 | `#if defined(_MSC_VER)` |
|      - |  315 | `#pragma warning(pop)` |
|      - |  316 | `#endif` |
|   8831 |  317 | `}` |
|      - |  318 | `/*` |
|      - |  319 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  320 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  321 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  322 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  323 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  324 | ` * (php's exact wording for the memory-parser case).` |
|      - |  325 | ` */` |
|   4414 |  326 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      5 |  327 | `{` |
|   4419 |  328 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);` |
|   4419 |  329 | `	LibxmlGenericSet(pVm,1);` |
|   4419 |  330 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      5 |  331 | `}` |
|      - |  332 | `/*` |
|      - |  333 | ` * End a capture window WITHOUT reporting what it caught: for an entry point` |
|      - |  334 | ` * whose failure php reports through the return value alone (a stream write` |
|      - |  335 | ` * that could not land under XMLWriter), where libxml's own message would be a` |
|      - |  336 | ` * warning php never raises. Entries stay queued when internal capture is on --` |
|      - |  337 | `` * `libxml_get_errors()` is the one place php does show them.`` |
|      - |  338 | ` */` |
|     44 |  339 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark)` |
|      1 |  340 | `{` |
|     45 |  341 | `	xmlSetStructuredErrorFunc(0,0);` |
|     45 |  342 | `	LibxmlGenericSet(pVm,0);` |
|     45 |  343 | `	if( pVm->bLibxmlInternalErr ){` |
|    ! 0 |  344 | `		return;` |
|      - |  345 | `	}` |
|     45 |  346 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|      5 |  347 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  348 | `		sxu32 n;` |
|      9 |  349 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      5 |  350 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|      3 |  351 | `		}` |
|      5 |  352 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|      2 |  353 | `	}` |
|     23 |  354 | `}` |
|   2780 |  355 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      3 |  356 | `{` |
|   2783 |  357 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFnName,0);` |
|   2783 |  358 | `}` |
|      - |  359 | `/*` |
|      - |  360 | ` * The same drain for a parse that was given OPTIONS, two of which are about` |
|      - |  361 | `` * these very diagnostics: `LIBXML_NOERROR` silences the errors and the fatals,`` |
|      - |  362 | `` * `LIBXML_NOWARNING` the warnings. php silences them at the PRINT, not at the`` |
|      - |  363 | `` * source -- the queue `libxml_get_errors()` answers still holds them, and so`` |
|      - |  364 | `` * does the slot `libxml_get_last_error()` reads -- so this skips them here,`` |
|      - |  365 | ` * after they have been recorded, and the line buffer never sees them either` |
|      - |  366 | ` * (php's does not: libxml's message never reaches the printer at all).` |
|      - |  367 | ` */` |
|   4368 |  368 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts)` |
|      5 |  369 | `{` |
|   4373 |  370 | `	xmlSetStructuredErrorFunc(0,0);` |
|   4373 |  371 | `	LibxmlGenericSet(pVm,0);` |
|   4373 |  372 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  373 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|     91 |  374 | `		return;` |
|      - |  375 | `	}` |
|   4283 |  376 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    147 |  377 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  378 | `		sxu32 n;` |
|    258 |  379 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  380 | `			SyString sFunc;` |
|      - |  381 | `			SyBlob sMsg;` |
|      - |  382 | `			const char *zPend;` |
|      - |  383 | `			sxu32 nPend,nTrim;` |
|    239 |  384 | `			if( (aErr[n].iLevel == XML_ERR_WARNING)` |
|     83 |  385 | `			 ? (iOpts & XML_PARSE_NOWARNING) != 0` |
|     97 |  386 | `			 : (iOpts & XML_PARSE_NOERROR) != 0 ){` |
|     89 |  387 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|     94 |  388 | `				continue;` |
|      - |  389 | `			}` |
|      - |  390 | `			/* php's libxml diagnostics are LINE-buffered: each message is` |
|      - |  391 | `			 * appended to one buffer and the diagnostic is only raised when the` |
|      - |  392 | `			 * accumulated text ends in a newline. Most of libxml's messages do,` |
|      - |  393 | `			 * so most of them stand alone -- but the ones that do not (libxml's` |
|      - |  394 | `			 * "Validation failed: no DTD found !" is one) are held back and` |
|      - |  395 | `			 * printed JOINED to whatever comes next, even from a later parse of` |
|      - |  396 | `			 * a different document, and are never printed at all if nothing` |
|      - |  397 | `			 * else follows. Reproduced rather than tidied up: a program's` |
|      - |  398 | `			 * output is what it is. */` |
|     92 |  399 | `			SyBlobAppend(&pVm->sLibxmlPend,aErr[n].sMsg.zString,aErr[n].sMsg.nByte);` |
|     92 |  400 | `			zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|     92 |  401 | `			nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|     92 |  402 | `			if( !aErr[n].bWholeLine && (nPend < 1 \|\| zPend[nPend-1] != '\n') ){` |
|      - |  403 | `				/* no line yet: hold it for the next message */` |
|     11 |  404 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|     11 |  405 | `				continue;` |
|      - |  406 | `			}` |
|     82 |  407 | `			nTrim = nPend;` |
|      - |  408 | `			/* php trims the trailing newline off the warning copy */` |
|    197 |  409 | `			while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|     80 |  410 | `				nTrim--;` |
|      1 |  411 | `			}` |
|     82 |  412 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     82 |  413 | `			SyBlobAppend(&sMsg,zPend,nTrim);` |
|      - |  414 | `			/* php appends the source location only for parser errors that` |
|      - |  415 | `			 * carry a real line; generic libxml errors print bare. The location` |
|      - |  416 | `			 * and the LEVEL are the flushing message's, not the held one's. */` |
|     82 |  417 | `			if( aErr[n].iLine > 0 ){` |
|     54 |  418 | `				if( aErr[n].sFile.nByte > 0 ){` |
|      7 |  419 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|      4 |  420 | `				}else{` |
|     48 |  421 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  422 | `				}` |
|     25 |  423 | `			}` |
|     82 |  424 | `			SyBlobAppend(&sMsg,"\0",1);` |
|     82 |  425 | `			if( zFnName ){` |
|     82 |  426 | `				SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|     36 |  427 | `			}` |
|      - |  428 | `			/* php reports libxml's own SEVERITY: a warning (an unsupported XML` |
|      - |  429 | `			 * version, a DTD the content does not follow) is an E_NOTICE there` |
|      - |  430 | `			 * and only an error or a fatal is an E_WARNING. The distinction is` |
|      - |  431 | ``			 * visible to any set_error_handler() and to `error_reporting` --`` |
|      - |  432 | `			 * a handler screening on E_WARNING must not see the warnings. */` |
|     82 |  433 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,` |
|     81 |  434 | `				aErr[n].iLevel == XML_ERR_WARNING ? PH7_CTX_NOTICE : PH7_CTX_WARNING,` |
|     81 |  435 | `				(const char *)SyBlobData(&sMsg));` |
|     82 |  436 | `			SyBlobRelease(&sMsg);` |
|     82 |  437 | `			SyBlobReset(&pVm->sLibxmlPend);` |
|     82 |  438 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|     37 |  439 | `		}` |
|     79 |  440 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|     39 |  441 | `	}` |
|   2121 |  442 | `}` |
|      - |  443 | `/*` |
|      - |  444 | ` * php's OTHER libxml channel.` |
|      - |  445 | ` *` |
|      - |  446 | ` * libxml reports an I/O failure through its GENERIC error function rather than` |
|      - |  447 | ` * the structured one, with the severity spelled INTO the text ("I/O warning :` |
|      - |  448 | ` * ..."), and php prints that text at E_WARNING whatever the severity says --` |
|      - |  449 | `` * while the structured copy, which is what `libxml_get_errors()` reports, keeps`` |
|      - |  450 | ` * libxml's own level and carries no such prefix. A caller with a message from` |
|      - |  451 | ` * that channel hands it here: it goes through the same line buffer as every` |
|      - |  452 | ` * other diagnostic (so a held fragment is printed in front of it) and takes no` |
|      - |  453 | ` * source location, because that channel has none.` |
|      - |  454 | ` */` |
|      4 |  455 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg)` |
|      1 |  456 | `{` |
|      - |  457 | `	SyString sFunc;` |
|      - |  458 | `	SyBlob sOut;` |
|      - |  459 | `	const char *zPend;` |
|      - |  460 | `	sxu32 nPend,nTrim;` |
|      5 |  461 | `	SyBlobAppend(&pVm->sLibxmlPend,zMsg,SyStrlen(zMsg));` |
|      5 |  462 | `	zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|      5 |  463 | `	nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|      5 |  464 | `	if( nPend < 1 \|\| zPend[nPend-1] != '\n' ){` |
|    ! 0 |  465 | `		return;` |
|      - |  466 | `	}` |
|      5 |  467 | `	nTrim = nPend;` |
|     11 |  468 | `	while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|      5 |  469 | `		nTrim--;` |
|      1 |  470 | `	}` |
|      5 |  471 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      5 |  472 | `	SyBlobAppend(&sOut,zPend,nTrim);` |
|      5 |  473 | `	SyBlobAppend(&sOut,"\0",sizeof(char));` |
|      5 |  474 | `	SyStringInitFromBuf(&sFunc,zFnName,zFnName ? SyStrlen(zFnName) : 0);` |
|      3 |  475 | `	PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,` |
|      4 |  476 | `		(const char *)SyBlobData(&sOut));` |
|      5 |  477 | `	SyBlobRelease(&sOut);` |
|      5 |  478 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|      3 |  479 | `}` |
|      - |  480 | `/*` |
|      - |  481 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  482 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  483 | ` */` |
|   3252 |  484 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      5 |  485 | `{` |
|      - |  486 | `	phl_xmldoc *pDoc;` |
|   3257 |  487 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|   3257 |  488 | `	if( pDoc == 0 ){` |
|    ! 0 |  489 | `		return 0;` |
|      - |  490 | `	}` |
|   3257 |  491 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|   3257 |  492 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|   3257 |  493 | `	SySetInit(&pDoc->aNotations,&pVm->sAllocator,sizeof(void *));` |
|   3257 |  494 | `	pDoc->pDoc = pXmlDocPtr;` |
|   3257 |  495 | `	pDoc->pVm = &(*pVm);` |
|   3257 |  496 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|   3257 |  497 | `	pDoc->bFormatOutput = 0;` |
|   3257 |  498 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|   3257 |  499 | `	pVm->pXmlDocs = (void *)pDoc;` |
|   3257 |  500 | `	return pDoc;` |
|   1631 |  501 | `}` |
|      - |  502 |  |
|      - |  503 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  504 |  |
|      - |  505 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  506 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  507 | `		SXUNUSED(pUnused); \` |
|      - |  508 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  509 | `	}` |
|     65 |  510 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|     69 |  511 | `LIBXML_INT_CONST(LibxmlConst_RECOVER,        XML_PARSE_RECOVER)` |
|     69 |  512 | `LIBXML_INT_CONST(LibxmlConst_HTML_NOIMPLIED, 8192)   /* HTML_PARSE_NOIMPLIED */` |
|     69 |  513 | `LIBXML_INT_CONST(LibxmlConst_HTML_NODEFDTD,  4)      /* HTML_PARSE_NODEFDTD */` |
|     65 |  514 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|     65 |  515 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|     65 |  516 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|     65 |  517 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|     75 |  518 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|     71 |  519 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|     65 |  520 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|     65 |  521 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|     65 |  522 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|     65 |  523 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|     65 |  524 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|     65 |  525 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|     65 |  526 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|     65 |  527 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|     65 |  528 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|     69 |  529 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|     69 |  530 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|     67 |  531 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|     65 |  532 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|     65 |  533 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|     65 |  534 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|     67 |  535 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  536 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|     65 |  537 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|     65 |  538 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|     65 |  539 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|     65 |  540 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|     65 |  541 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|     65 |  542 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|     65 |  543 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|     65 |  544 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|     65 |  545 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|     67 |  546 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|     65 |  547 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|     65 |  548 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|     65 |  549 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|     65 |  550 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|     67 |  551 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_DECL_NODE,   XML_ELEMENT_DECL)` |
|     67 |  552 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_DECL_NODE, XML_ATTRIBUTE_DECL)` |
|     67 |  553 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_DECL_NODE,    XML_ENTITY_DECL)` |
|      - |  554 | `/* php spells libxml's XML_NAMESPACE_DECL twice, under both DOM's name for a` |
|      - |  555 | ` * namespace node and libxml's own. */` |
|     67 |  556 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_DECL_NODE, XML_NAMESPACE_DECL)` |
|     67 |  557 | `LIBXML_INT_CONST(LibxmlConst_LOCAL_NAMESPACE,     XML_NAMESPACE_DECL)` |
|      - |  558 | `/*` |
|      - |  559 | ` * The DTD attribute-TYPE enum. php's numbers are libxml's xmlAttributeType with` |
|      - |  560 | ` * one deliberate hole: php has no XML_ATTRIBUTE_ENTITIES and gives the name` |
|      - |  561 | ` * XML_ATTRIBUTE_ENTITY libxml's ENTITIES value (6), so the two disagree about` |
|      - |  562 | ` * what "entity" means by one.  php's numbering is the contract.` |
|      - |  563 | ` */` |
|     67 |  564 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_CDATA,       XML_ATTRIBUTE_CDATA)` |
|     67 |  565 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ID,          XML_ATTRIBUTE_ID)` |
|     67 |  566 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREF,       XML_ATTRIBUTE_IDREF)` |
|     67 |  567 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREFS,      XML_ATTRIBUTE_IDREFS)` |
|     67 |  568 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENTITY,      XML_ATTRIBUTE_ENTITIES)` |
|     67 |  569 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKEN,     XML_ATTRIBUTE_NMTOKEN)` |
|     67 |  570 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKENS,    XML_ATTRIBUTE_NMTOKENS)` |
|     67 |  571 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENUMERATION, XML_ATTRIBUTE_ENUMERATION)` |
|     67 |  572 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NOTATION,    XML_ATTRIBUTE_NOTATION)` |
|      - |  573 | `/*` |
|      - |  574 | ` * ext/dom's DOMException codes -- the DOM level-2 numbering an exception's` |
|      - |  575 | ` * getCode() answers, which is what a catch tests to tell one refusal from` |
|      - |  576 | ` * another (php's own zero, DOM_PHP_ERR, is the code for everything that is not` |
|      - |  577 | ` * a DOM error).` |
|      - |  578 | ` */` |
|     69 |  579 | `LIBXML_INT_CONST(LibxmlConst_PHP_ERR,                  0)` |
|     67 |  580 | `LIBXML_INT_CONST(LibxmlConst_INDEX_SIZE_ERR,           1)` |
|     67 |  581 | `LIBXML_INT_CONST(LibxmlConst_DOMSTRING_SIZE_ERR,       2)` |
|     67 |  582 | `LIBXML_INT_CONST(LibxmlConst_HIERARCHY_REQUEST_ERR,    3)` |
|     67 |  583 | `LIBXML_INT_CONST(LibxmlConst_WRONG_DOCUMENT_ERR,       4)` |
|     67 |  584 | `LIBXML_INT_CONST(LibxmlConst_INVALID_CHARACTER_ERR,    5)` |
|     67 |  585 | `LIBXML_INT_CONST(LibxmlConst_NO_DATA_ALLOWED_ERR,      6)` |
|     67 |  586 | `LIBXML_INT_CONST(LibxmlConst_NO_MODIFICATION_ALLOWED_ERR, 7)` |
|     69 |  587 | `LIBXML_INT_CONST(LibxmlConst_NOT_FOUND_ERR,            8)` |
|     67 |  588 | `LIBXML_INT_CONST(LibxmlConst_NOT_SUPPORTED_ERR,        9)` |
|     67 |  589 | `LIBXML_INT_CONST(LibxmlConst_INUSE_ATTRIBUTE_ERR,     10)` |
|     67 |  590 | `LIBXML_INT_CONST(LibxmlConst_INVALID_STATE_ERR,       11)` |
|     67 |  591 | `LIBXML_INT_CONST(LibxmlConst_SYNTAX_ERR,              12)` |
|     67 |  592 | `LIBXML_INT_CONST(LibxmlConst_INVALID_MODIFICATION_ERR,13)` |
|     67 |  593 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_ERR,           14)` |
|     67 |  594 | `LIBXML_INT_CONST(LibxmlConst_INVALID_ACCESS_ERR,      15)` |
|     67 |  595 | `LIBXML_INT_CONST(LibxmlConst_VALIDATION_ERR,          16)` |
|      - |  596 |  |
|     62 |  597 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|      3 |  598 | `{` |
|     31 |  599 | `	SXUNUSED(pUnused);` |
|     65 |  600 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|     65 |  601 | `}` |
|     62 |  602 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|      3 |  603 | `{` |
|     31 |  604 | `	SXUNUSED(pUnused);` |
|      - |  605 | `	/* php exposes the runtime-loaded version string here */` |
|     65 |  606 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|     65 |  607 | `}` |
|      - |  608 |  |
|   4962 |  609 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  610 | `{` |
|      - |  611 | `	static const struct {` |
|      - |  612 | `		const char *zName;` |
|      - |  613 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  614 | `	} aConst[] = {` |
|      - |  615 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  616 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  617 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  618 | `		{ "LIBXML_RECOVER",        LibxmlConst_RECOVER        },` |
|      - |  619 | `		{ "LIBXML_HTML_NOIMPLIED", LibxmlConst_HTML_NOIMPLIED },` |
|      - |  620 | `		{ "LIBXML_HTML_NODEFDTD",  LibxmlConst_HTML_NODEFDTD  },` |
|      - |  621 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  622 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  623 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  624 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  625 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  626 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  627 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  628 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  629 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  630 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  631 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  632 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  633 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  634 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  635 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  636 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  637 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  638 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  639 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  640 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  641 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  642 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  643 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  644 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  645 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  646 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  647 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  648 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  649 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  650 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  651 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  652 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  653 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  654 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  655 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  656 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  657 | `		{ "XML_ELEMENT_DECL_NODE",  LibxmlConst_ELEMENT_DECL_NODE  },` |
|      - |  658 | `		{ "XML_ATTRIBUTE_DECL_NODE",LibxmlConst_ATTRIBUTE_DECL_NODE},` |
|      - |  659 | `		{ "XML_ENTITY_DECL_NODE",   LibxmlConst_ENTITY_DECL_NODE   },` |
|      - |  660 | `		{ "XML_NAMESPACE_DECL_NODE",LibxmlConst_NAMESPACE_DECL_NODE},` |
|      - |  661 | `		{ "XML_LOCAL_NAMESPACE",    LibxmlConst_LOCAL_NAMESPACE    },` |
|      - |  662 | `		{ "XML_ATTRIBUTE_CDATA",       LibxmlConst_ATTRIBUTE_CDATA       },` |
|      - |  663 | `		{ "XML_ATTRIBUTE_ID",          LibxmlConst_ATTRIBUTE_ID          },` |
|      - |  664 | `		{ "XML_ATTRIBUTE_IDREF",       LibxmlConst_ATTRIBUTE_IDREF       },` |
|      - |  665 | `		{ "XML_ATTRIBUTE_IDREFS",      LibxmlConst_ATTRIBUTE_IDREFS      },` |
|      - |  666 | `		{ "XML_ATTRIBUTE_ENTITY",      LibxmlConst_ATTRIBUTE_ENTITY      },` |
|      - |  667 | `		{ "XML_ATTRIBUTE_NMTOKEN",     LibxmlConst_ATTRIBUTE_NMTOKEN     },` |
|      - |  668 | `		{ "XML_ATTRIBUTE_NMTOKENS",    LibxmlConst_ATTRIBUTE_NMTOKENS    },` |
|      - |  669 | `		{ "XML_ATTRIBUTE_ENUMERATION", LibxmlConst_ATTRIBUTE_ENUMERATION },` |
|      - |  670 | `		{ "XML_ATTRIBUTE_NOTATION",    LibxmlConst_ATTRIBUTE_NOTATION    },` |
|      - |  671 | `		{ "DOM_PHP_ERR",                    LibxmlConst_PHP_ERR                    },` |
|      - |  672 | `		{ "DOM_INDEX_SIZE_ERR",             LibxmlConst_INDEX_SIZE_ERR             },` |
|      - |  673 | `		{ "DOMSTRING_SIZE_ERR",             LibxmlConst_DOMSTRING_SIZE_ERR         },` |
|      - |  674 | `		{ "DOM_HIERARCHY_REQUEST_ERR",      LibxmlConst_HIERARCHY_REQUEST_ERR      },` |
|      - |  675 | `		{ "DOM_WRONG_DOCUMENT_ERR",         LibxmlConst_WRONG_DOCUMENT_ERR         },` |
|      - |  676 | `		{ "DOM_INVALID_CHARACTER_ERR",      LibxmlConst_INVALID_CHARACTER_ERR      },` |
|      - |  677 | `		{ "DOM_NO_DATA_ALLOWED_ERR",        LibxmlConst_NO_DATA_ALLOWED_ERR        },` |
|      - |  678 | `		{ "DOM_NO_MODIFICATION_ALLOWED_ERR",LibxmlConst_NO_MODIFICATION_ALLOWED_ERR},` |
|      - |  679 | `		{ "DOM_NOT_FOUND_ERR",              LibxmlConst_NOT_FOUND_ERR              },` |
|      - |  680 | `		{ "DOM_NOT_SUPPORTED_ERR",          LibxmlConst_NOT_SUPPORTED_ERR          },` |
|      - |  681 | `		{ "DOM_INUSE_ATTRIBUTE_ERR",        LibxmlConst_INUSE_ATTRIBUTE_ERR        },` |
|      - |  682 | `		{ "DOM_INVALID_STATE_ERR",          LibxmlConst_INVALID_STATE_ERR          },` |
|      - |  683 | `		{ "DOM_SYNTAX_ERR",                 LibxmlConst_SYNTAX_ERR                 },` |
|      - |  684 | `		{ "DOM_INVALID_MODIFICATION_ERR",   LibxmlConst_INVALID_MODIFICATION_ERR   },` |
|      - |  685 | `		{ "DOM_NAMESPACE_ERR",              LibxmlConst_NAMESPACE_ERR              },` |
|      - |  686 | `		{ "DOM_INVALID_ACCESS_ERR",         LibxmlConst_INVALID_ACCESS_ERR         },` |
|      - |  687 | `		{ "DOM_VALIDATION_ERR",             LibxmlConst_VALIDATION_ERR             },` |
|      - |  688 | `	};` |
|      - |  689 | `	sxu32 n;` |
| 367193 |  690 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 362231 |  691 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
| 181118 |  692 | `	}` |
|   4967 |  693 | `}` |
|      - |  694 | `/* ===== libxml_* native thunks ===== */` |
|      - |  695 |  |
|      - |  696 | `/*` |
|      - |  697 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  698 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  699 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  700 | ` */` |
|    180 |  701 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  702 | `{` |
|    181 |  703 | `	ph7_vm *pVm = pCtx->pVm;` |
|    181 |  704 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|    181 |  705 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    175 |  706 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|    175 |  707 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  708 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|    103 |  709 | `			LibxmlClearQueue(pVm);` |
|     51 |  710 | `		}` |
|     87 |  711 | `	}` |
|    181 |  712 | `	ph7_result_bool(pCtx,bPrev);` |
|    181 |  713 | `	return PH7_OK;` |
|      1 |  714 | `}` |
|      - |  715 | `/*` |
|      - |  716 | ` * array __libxml_get_errors_raw()` |
|      - |  717 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  718 | ` */` |
|      - |  719 | `/*` |
|      - |  720 | ` * Build one LibXMLError from a recorded error.` |
|      - |  721 | ` *` |
|      - |  722 | `` * The prelude did this in PHP (`__phl_libxml_err_obj()` copying six array keys`` |
|      - |  723 | `` * onto a `new LibXMLError`), over an array these accessors returned purely so`` |
|      - |  724 | ` * that PHP could reshape it. Both the array hop and the helper are gone: the` |
|      - |  725 | ` * accessors below now answer the objects php answers.` |
|      - |  726 | ` */` |
|     56 |  727 | `static ph7_class_instance * LibxmlErrObject(ph7_context *pCtx,phl_libxml_err *pErr)` |
|      1 |  728 | `{` |
|     57 |  729 | `	ph7_vm *pVm = pCtx->pVm;` |
|     57 |  730 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"LibXMLError",sizeof("LibXMLError")-1,0,0);` |
|      - |  731 | `	ph7_class_instance *pObj;` |
|      - |  732 | `	ph7_value sVal;` |
|      - |  733 | `	SyString sStr;` |
|     57 |  734 | `	if( pClass == 0 ){` |
|    ! 0 |  735 | `		return 0;` |
|      - |  736 | `	}` |
|     57 |  737 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     57 |  738 | `	if( pObj == 0 ){` |
|    ! 0 |  739 | `		return 0;` |
|      - |  740 | `	}` |
|     57 |  741 | `	PH7_MemObjInit(pVm,&sVal);` |
|     57 |  742 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLevel);` |
|     57 |  743 | `	PH7_NativeSetProp(pVm,pObj,"level",sizeof("level")-1,&sVal);` |
|     57 |  744 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iCode);` |
|     57 |  745 | `	PH7_NativeSetProp(pVm,pObj,"code",sizeof("code")-1,&sVal);` |
|     57 |  746 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iColumn);` |
|     57 |  747 | `	PH7_NativeSetProp(pVm,pObj,"column",sizeof("column")-1,&sVal);` |
|     57 |  748 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLine);` |
|     57 |  749 | `	PH7_NativeSetProp(pVm,pObj,"line",sizeof("line")-1,&sVal);` |
|     57 |  750 | `	SyStringInitFromBuf(&sStr,pErr->sMsg.zString ? pErr->sMsg.zString : "",pErr->sMsg.nByte);` |
|     57 |  751 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     57 |  752 | `	PH7_NativeSetProp(pVm,pObj,"message",sizeof("message")-1,&sVal);` |
|     57 |  753 | `	SyStringInitFromBuf(&sStr,pErr->sFile.zString ? pErr->sFile.zString : "",pErr->sFile.nByte);` |
|     57 |  754 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     57 |  755 | `	PH7_NativeSetProp(pVm,pObj,"file",sizeof("file")-1,&sVal);` |
|     57 |  756 | `	PH7_MemObjRelease(&sVal);` |
|     57 |  757 | `	return pObj;` |
|     29 |  758 | `}` |
|      - |  759 | `/* array libxml_get_errors() */` |
|     62 |  760 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  761 | `{` |
|     63 |  762 | `	ph7_vm *pVm = pCtx->pVm;` |
|     63 |  763 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|     63 |  764 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|     63 |  765 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  766 | `	sxu32 n;` |
|     31 |  767 | `	SXUNUSED(nArg);` |
|     31 |  768 | `	SXUNUSED(apArg);` |
|     63 |  769 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  770 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  771 | `		ph7_result_null(pCtx);` |
|    ! 0 |  772 | `		return PH7_OK;` |
|      - |  773 | `	}` |
|    113 |  774 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     51 |  775 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,&aErr[n]);` |
|      - |  776 | `		ph7_value sEntry;` |
|     51 |  777 | `		if( pObj == 0 ){` |
|    ! 0 |  778 | `			break;` |
|      - |  779 | `		}` |
|      - |  780 | `		/* The list takes over the instance's initial iRef=1: ph7_array_add_elem` |
|      - |  781 | `		 * copies the slot (taking its own reference), so the local one is dropped. */` |
|     51 |  782 | `		PH7_MemObjInit(pVm,&sEntry);` |
|     51 |  783 | `		sEntry.x.pOther = pObj;` |
|     51 |  784 | `		sEntry.iFlags = MEMOBJ_OBJ;` |
|     51 |  785 | `		ph7_array_add_elem(pList,0,&sEntry);` |
|     51 |  786 | `		PH7_ClassInstanceUnref(pObj);` |
|     26 |  787 | `	}` |
|     63 |  788 | `	ph7_result_value(pCtx,pList);` |
|     63 |  789 | `	return PH7_OK;` |
|     32 |  790 | `}` |
|      - |  791 | `/*` |
|      - |  792 | ` * void __libxml_clear_errors()` |
|      - |  793 | ` */` |
|    146 |  794 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  795 | `{` |
|     73 |  796 | `	SXUNUSED(nArg);` |
|     73 |  797 | `	SXUNUSED(apArg);` |
|    147 |  798 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|    147 |  799 | `	ph7_result_null(pCtx);` |
|    147 |  800 | `	return PH7_OK;` |
|      1 |  801 | `}` |
|      - |  802 | `/*` |
|      - |  803 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  804 | ` */` |
|      8 |  805 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  806 | `{` |
|      9 |  807 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 |  808 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      4 |  809 | `	SXUNUSED(nArg);` |
|      4 |  810 | `	SXUNUSED(apArg);` |
|      9 |  811 | `	if( pLast == 0 ){` |
|      3 |  812 | `		ph7_result_bool(pCtx,0);` |
|      3 |  813 | `		return PH7_OK;` |
|      - |  814 | `	}` |
|      - |  815 | `	{` |
|      7 |  816 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,pLast);` |
|      - |  817 | `		ph7_value sRes;` |
|      7 |  818 | `		if( pObj == 0 ){` |
|    ! 0 |  819 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  820 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  821 | `			return PH7_OK;` |
|      - |  822 | `		}` |
|      7 |  823 | `		PH7_MemObjInit(pVm,&sRes);` |
|      7 |  824 | `		sRes.x.pOther = pObj;` |
|      7 |  825 | `		sRes.iFlags = MEMOBJ_OBJ;` |
|      7 |  826 | `		ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|      7 |  827 | `		PH7_ClassInstanceUnref(pObj);` |
|      - |  828 | `	}` |
|      7 |  829 | `	return PH7_OK;` |
|      5 |  830 | `}` |
|      - |  831 |  |
|      - |  832 | `/*` |
|      - |  833 | ` * ?callable libxml_get_external_entity_loader()` |
|      - |  834 | ` *  The stored resolver VERBATIM (a callable string answers as that string),` |
|      - |  835 | ` *  or null for the default loader -- php's answer shape.` |
|      - |  836 | ` */` |
|      8 |  837 | `static int vm_builtin_libxml_get_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  838 | `{` |
|      9 |  839 | `	ph7_vm *pVm = pCtx->pVm;` |
|      4 |  840 | `	SXUNUSED(nArg);` |
|      4 |  841 | `	SXUNUSED(apArg);` |
|      9 |  842 | `	if( (pVm->sXmlEntLoader.iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  843 | `		ph7_result_value(pCtx,&pVm->sXmlEntLoader); /* makes its own copy */` |
|      3 |  844 | `	}else{` |
|      5 |  845 | `		ph7_result_null(pCtx);` |
|      - |  846 | `	}` |
|      9 |  847 | `	return PH7_OK;` |
|      1 |  848 | `}` |
|      - |  849 | `/*` |
|      - |  850 | ` * true libxml_set_external_entity_loader(?callable $resolver_function)` |
|      - |  851 | ` *  Store the resolver (null restores the default). The slot is never` |
|      - |  852 | ` *  INVOKED here -- no PHL parse path loads an external entity, the same` |
|      - |  853 | ` *  off-by-default php's sanitized parser options enforce -- so the` |
|      - |  854 | ` *  round-trip contract is the whole observable surface.` |
|      - |  855 | ` */` |
|     10 |  856 | `static int vm_builtin_libxml_set_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  857 | `{` |
|     11 |  858 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  859 | `	if( nArg < 1 ){` |
|    ! 0 |  860 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  861 | `		return PH7_OK;` |
|      - |  862 | `	}` |
|     11 |  863 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      - |  864 | `		/* php screens through the FCC machinery: a string must resolve as a` |
|      - |  865 | `		 * CALLABLE, and the refusal names the callback rule. */` |
|      9 |  866 | `		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"resolver_function",1);` |
|      9 |  867 | `		if( rc != PH7_OK ){` |
|      5 |  868 | `			return rc;` |
|      - |  869 | `		}` |
|      2 |  870 | `	}` |
|      7 |  871 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|      7 |  872 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      5 |  873 | `		PH7_MemObjStore(apArg[0],&pVm->sXmlEntLoader);` |
|      2 |  874 | `	}` |
|      7 |  875 | `	ph7_result_bool(pCtx,1);` |
|      7 |  876 | `	return PH7_OK;` |
|      6 |  877 | `}` |
|      - |  878 | `/*` |
|      - |  879 | ` * void libxml_set_streams_context($context)` |
|      - |  880 | ` *  Take a stream-context RESOURCE and keep it for the document loaders.` |
|      - |  881 | ` *  php validates lazily at the next load; PHL has no loader that would` |
|      - |  882 | ` *  ever read it (the consumer is the http:// wrapper), so` |
|      - |  883 | ` *  the check runs here, with php's own two messages.` |
|      - |  884 | ` */` |
|      6 |  885 | `static int vm_builtin_libxml_set_streams_context(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  886 | `{` |
|      7 |  887 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  888 | `	const io_private *pDev;` |
|      7 |  889 | `	if( nArg < 1 ){` |
|    ! 0 |  890 | `		return PH7_OK;` |
|      - |  891 | `	}` |
|      7 |  892 | `	if( (apArg[0]->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 |  893 | `		const char *zType = "null";` |
|      3 |  894 | `		if( apArg[0]->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }` |
|      3 |  895 | `		else if( apArg[0]->iFlags & MEMOBJ_OBJ ){ zType = "object"; }` |
|      3 |  896 | `		else if( apArg[0]->iFlags & MEMOBJ_STRING ){ zType = "string"; }` |
|      3 |  897 | `		else if( apArg[0]->iFlags & MEMOBJ_BOOL ){ zType = "bool"; }` |
|      3 |  898 | `		else if( apArg[0]->iFlags & MEMOBJ_REAL ){ zType = "float"; }` |
|      3 |  899 | `		else if( apArg[0]->iFlags & MEMOBJ_INT ){ zType = "int"; }` |
|      4 |  900 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  901 | `			"libxml_set_streams_context(): Argument #1 ($context) must be of type resource, %s given",` |
|      1 |  902 | `			zType);` |
|      - |  903 | `	}` |
|      5 |  904 | `	pDev = (const io_private *)apArg[0]->x.pOther;` |
|      5 |  905 | `	if( pDev == 0 \|\| pDev->iMagic != STREAM_CTX_MAGIC ){` |
|      3 |  906 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  907 | `			"libxml_set_streams_context(): supplied resource is not a valid Stream-Context resource");` |
|      - |  908 | `	}` |
|      3 |  909 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|      3 |  910 | `	PH7_MemObjStore(apArg[0],&pVm->sXmlStreamsCtx);` |
|      3 |  911 | `	ph7_result_null(pCtx);` |
|      3 |  912 | `	return PH7_OK;` |
|      4 |  913 | `}` |
|      - |  914 |  |
|      - |  915 | `/*` |
|      - |  916 | ` * The libxml PHP-visible surface: the LibXMLError class plus the seven` |
|      - |  917 | ` * libxml_* functions (php's eighth, libxml_disable_entity_loader(), is` |
|      - |  918 | ` * E_DEPRECATED since 8.0 and stays removed per the scope policy §10 --` |
|      - |  919 | ` * twin-paired in 002-integration/function/libxml/).` |
|      - |  920 | ` */` |
|      - |  921 | `/* LibXMLError is declared from C below, and the four libxml_* functions ARE the` |
|      - |  922 | ` * C routines -- they used to be PHP wrappers over __libxml_* thunks, with a PHP` |
|      - |  923 | ` * helper reshaping an array into the object. */` |
|      - |  924 |  |
|      - |  925 | `/*` |
|      - |  926 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  927 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  928 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  929 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  930 | ` */` |
|   5740 |  931 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  932 | `{` |
|      - |  933 | `	/* Registered under the names php exposes. There is no thunk and no PHP` |
|      - |  934 | `	 * wrapper any more: these are the functions, so they are internal for` |
|      - |  935 | `	 * reflection and get the arity enforcement a prelude wrapper never had. */` |
|      - |  936 | `	static const struct {` |
|      - |  937 | `		const char *zName;` |
|      - |  938 | `		ProchHostFunction xFunc;` |
|      - |  939 | `	} aFunc[] = {` |
|      - |  940 | `		{ "libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  941 | `		{ "libxml_get_errors",          vm_builtin_libxml_get_errors_raw      },` |
|      - |  942 | `		{ "libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  943 | `		{ "libxml_get_last_error",      vm_builtin_libxml_get_last_error_raw  },` |
|      - |  944 | `		{ "libxml_get_external_entity_loader", vm_builtin_libxml_get_external_entity_loader },` |
|      - |  945 | `		{ "libxml_set_external_entity_loader", vm_builtin_libxml_set_external_entity_loader },` |
|      - |  946 | `		{ "libxml_set_streams_context", vm_builtin_libxml_set_streams_context },` |
|      - |  947 | `	};` |
|      - |  948 | `	/* Plain data carrier; php declares no methods on it. */` |
|      - |  949 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  950 | `		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  951 | `		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  952 | `		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  953 | `		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  954 | `		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  955 | `		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  956 | `	};` |
|      - |  957 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - |  958 | `		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - |  959 | `	};` |
|      - |  960 | `	sxu32 n;` |
|   5745 |  961 | `	LibxmlGlobalInit();` |
|   5745 |  962 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   5745 |  963 | `	SyBlobInit(&pVm->sLibxmlPend,&pVm->sAllocator);` |
|   5745 |  964 | `	pVm->bLibxmlInternalErr = 0;` |
|   5745 |  965 | `	pVm->pLibxmlLastErr = 0;` |
|   5745 |  966 | `	pVm->pXmlDocs = 0;` |
|   5745 |  967 | `	pVm->pXmlWriters = 0;` |
|   5745 |  968 | `	PH7_MemObjInit(pVm,&pVm->sXmlEntLoader);` |
|   5745 |  969 | `	PH7_MemObjInit(pVm,&pVm->sXmlStreamsCtx);` |
|  45925 |  970 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  40185 |  971 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  20095 |  972 | `	}` |
|      - |  973 | `	/* The class must exist before the accessors can build one. */` |
|   5745 |  974 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 |  975 | `}` |
|      - |  976 |  |
|      - |  977 | `#else` |
|      - |  978 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  979 | `typedef int vm_libxml_unused;` |
|      - |  980 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  981 |  |
