# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 505/524 lines (96.37%)

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
|   7925 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   7930 |   41 | `	if( !bInit ){` |
|   7930 |   42 | `		xmlInitParser();` |
|   7930 |   43 | `		bInit = 1;` |
|   3957 |   44 | `	}` |
|   7930 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|   3450 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      5 |   52 | `{` |
|   3455 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	xmlEntityPtr *apNot;` |
|      - |   55 | `	sxu32 n;` |
|   3841 |   56 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|    388 |   57 | `		xmlUnlinkNode(apOrphan[n]);` |
|    388 |   58 | `		xmlFreeNode(apOrphan[n]);` |
|    195 |   59 | `	}` |
|   3455 |   60 | `	SySetRelease(&pDoc->aOrphans);` |
|      - |   61 | `	/* The synthesized notation nodes, freed field by field as php frees its` |
|      - |   62 | ``	 * own: xmlFreeNode would read the entity's `length`/`etype` pair as a`` |
|      - |   63 | `	 * node's property list and walk into it. */` |
|   3455 |   64 | `	apNot = (xmlEntityPtr *)SySetBasePtr(&pDoc->aNotations);` |
|   3465 |   65 | `	for( n = 0 ; n < SySetUsed(&pDoc->aNotations) ; ++n ){` |
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
|   3455 |   77 | `	SySetRelease(&pDoc->aNotations);` |
|   3455 |   78 | `	if( pDoc->pDoc ){` |
|   3453 |   79 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|   3453 |   80 | `		pDoc->pDoc = 0;` |
|   1724 |   81 | `	}` |
|   3455 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   85 | ` * error queue and free every document from the previous request.  Called` |
|      - |   86 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   87 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   88 | ` */` |
|   6717 |   89 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   90 | `{` |
|      - |   91 | `	phl_xmldoc *pDoc,*pNext;` |
|   6722 |   92 | `	PH7_LibxmlClearErrors(pVm);` |
|      - |   93 | `	/* A held message fragment is per-request state: a reused VM must not print` |
|      - |   94 | `	 * the previous request's tail joined to this one's first diagnostic. */` |
|   6722 |   95 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|   6722 |   96 | `	pVm->bLibxmlInternalErr = 0;` |
|   6722 |   97 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|  10172 |   98 | `	while( pDoc ){` |
|   3455 |   99 | `		pNext = pDoc->pNext;` |
|   3455 |  100 | `		LibxmlFreeDoc(pDoc);` |
|   3455 |  101 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|   3455 |  102 | `		pDoc = pNext;` |
|      5 |  103 | `	}` |
|   6722 |  104 | `	pVm->pXmlDocs = 0;` |
|      - |  105 | `	/* The ownerless shell rode the chain just freed. */` |
|   6722 |  106 | `	pVm->pXmlLimbo = 0;` |
|      - |  107 | `	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */` |
|   6722 |  108 | `	PH7_XmlWriterVmSweep(pVm);` |
|      - |  109 | `	/* ext/xml push parsers: their ctxt/myDoc are libxml allocations and the` |
|      - |  110 | `	 * handler VALUES hold references that must drop before the allocator goes. */` |
|   6722 |  111 | `	PH7_XmlParserVmSweep(pVm);` |
|      - |  112 | `	/* The entity-loader/streams-context slots hold per-request VALUES (a` |
|      - |  113 | `	 * closure, a context resource): drop them so a reused VM starts default. */` |
|   6722 |  114 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|   6722 |  115 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|   6722 |  116 | `}` |
|      - |  117 | `/*` |
|      - |  118 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |  119 | ` * wipes the allocator that holds the registry shells.` |
|      - |  120 | ` */` |
|   6701 |  121 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |  122 | `{` |
|   6706 |  123 | `	PH7_LibxmlVmReset(pVm);` |
|   6706 |  124 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   6706 |  125 | `	SyBlobRelease(&pVm->sLibxmlPend);` |
|   6706 |  126 | `}` |
|      - |  127 | `/*` |
|      - |  128 | ` * Release the copied message/file strings of one queue entry.` |
|      - |  129 | ` */` |
|    500 |  130 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      2 |  131 | `{` |
|    502 |  132 | `	if( pErr->sMsg.zString ){` |
|    502 |  133 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|    236 |  134 | `	}` |
|    502 |  135 | `	if( pErr->sFile.zString ){` |
|    160 |  136 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|     74 |  137 | `	}` |
|    502 |  138 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|    502 |  139 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|    502 |  140 | `}` |
|      - |  141 | `/*` |
|      - |  142 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  143 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  144 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  145 | ` */` |
|   6969 |  146 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  147 | `{` |
|   6974 |  148 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  149 | `	sxu32 n;` |
|   7059 |  150 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     87 |  151 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|     42 |  152 | `	}` |
|   6974 |  153 | `	SySetReset(&pVm->aLibxmlErr);` |
|   6974 |  154 | `}` |
|      - |  155 | `/*` |
|      - |  156 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  157 | ` */` |
|   6865 |  158 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  159 | `{` |
|   6870 |  160 | `	LibxmlClearQueue(pVm);` |
|   6870 |  161 | `	if( pVm->pLibxmlLastErr ){` |
|     60 |  162 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|     60 |  163 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|     60 |  164 | `		pVm->pLibxmlLastErr = 0;` |
|     29 |  165 | `	}` |
|   6870 |  166 | `}` |
|      - |  167 | `/*` |
|      - |  168 | ` * Push one error onto the per-VM queue and last-error slot, copying the` |
|      - |  169 | ` * message/file strings.  The typed structured-error callback below and the` |
|      - |  170 | ` * DOM schema hooks (vm_dom.c) both funnel through this, keeping the` |
|      - |  171 | ` * queue-building logic in one place and ph7int.h free of libxml types.` |
|      - |  172 | ` */` |
|    250 |  173 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|      - |  174 | `	const char *zMsg,const char *zFile)` |
|      2 |  175 | `{` |
|      - |  176 | `	phl_libxml_err sEntry;` |
|      - |  177 | `	phl_libxml_err *pLast;` |
|    252 |  178 | `	if( pVm == 0 ){` |
|    ! 0 |  179 | `		return;` |
|      - |  180 | `	}` |
|    252 |  181 | `	SyZero(&sEntry,sizeof(sEntry));` |
|    252 |  182 | `	sEntry.iLevel = iLevel;` |
|    252 |  183 | `	sEntry.iCode = iCode;` |
|    252 |  184 | `	sEntry.iLine = iLine;` |
|    252 |  185 | `	sEntry.iColumn = iColumn;` |
|    252 |  186 | `	if( zMsg ){` |
|    252 |  187 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|    252 |  188 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);` |
|    252 |  189 | `		if( zDup ){` |
|      - |  190 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|    252 |  191 | `			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);` |
|    118 |  192 | `		}` |
|    118 |  193 | `	}` |
|    252 |  194 | `	if( zFile ){` |
|     81 |  195 | `		sxu32 nFile = SyStrlen(zFile);` |
|     81 |  196 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);` |
|     81 |  197 | `		if( zDup ){` |
|     81 |  198 | `			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);` |
|     37 |  199 | `		}` |
|     37 |  200 | `	}` |
|    252 |  201 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  202 | `	/* Mirror into the last-error slot (independent string copies so queue` |
|      - |  203 | `	 * draining cannot invalidate it). */` |
|    252 |  204 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|    252 |  205 | `	if( pLast == 0 ){` |
|     60 |  206 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|     60 |  207 | `		if( pLast == 0 ){` |
|    ! 0 |  208 | `			return;` |
|      - |  209 | `		}` |
|     60 |  210 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|     60 |  211 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|     31 |  212 | `	}else{` |
|    194 |  213 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  214 | `	}` |
|    252 |  215 | `	pLast->iLevel = iLevel;` |
|    252 |  216 | `	pLast->iCode = iCode;` |
|    252 |  217 | `	pLast->iLine = iLine;` |
|    252 |  218 | `	pLast->iColumn = iColumn;` |
|    252 |  219 | `	if( sEntry.sMsg.zString ){` |
|    252 |  220 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|    252 |  221 | `		if( zDup ){` |
|    252 |  222 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|    118 |  223 | `		}` |
|    118 |  224 | `	}` |
|    252 |  225 | `	if( sEntry.sFile.zString ){` |
|     81 |  226 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|     81 |  227 | `		if( zDup ){` |
|     81 |  228 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|     37 |  229 | `		}` |
|     37 |  230 | `	}` |
|    120 |  231 | `}` |
|      - |  232 | `/*` |
|      - |  233 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  234 | ` * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;` |
|      - |  235 | ` * the capture-end decides whether entries stay queued or drain as` |
|      - |  236 | ` * php-style warnings.` |
|      - |  237 | ` */` |
|      - |  238 | `#if LIBXML_VERSION >= 21200` |
|     77 |  239 | `static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)` |
|      - |  240 | `#else` |
|     90 |  241 | `static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  242 | `#endif` |
|      2 |  243 | `{` |
|    169 |  244 | `	if( pErr == 0 ){` |
|    ! 0 |  245 | `		return;` |
|      - |  246 | `	}` |
|      - |  247 | `	/* libxml keeps the column in int2 */` |
|    246 |  248 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|    167 |  249 | `		pErr->int2,pErr->message,pErr->file);` |
|     79 |  250 | `}` |
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
|     53 |  264 | `static void LibxmlGenericErr(void *pUserData,const char *zFmt,...)` |
|      1 |  265 | `{` |
|     54 |  266 | `	ph7_vm *pVm = (ph7_vm *)pUserData;` |
|      - |  267 | `	SyBlob sMsg;` |
|      - |  268 | `	va_list ap;` |
|     54 |  269 | `	if( pVm == 0 \|\| zFmt == 0 ){` |
|    ! 0 |  270 | `		return;` |
|      - |  271 | `	}` |
|     54 |  272 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     54 |  273 | `	va_start(ap,zFmt);` |
|     54 |  274 | `	SyBlobFormatAp(&sMsg,zFmt,ap);` |
|     54 |  275 | `	va_end(ap);` |
|      - |  276 | `	/* The message keeps its newline: php's own generic handler is LINE-buffered` |
|      - |  277 | `	 * and this channel does not deliver whole lines. libxml's default` |
|      - |  278 | ``	 * parser-error routine calls it FOUR times for one error -- `Entity: line`` |
|      - |  279 | ``	 * 1: `, `parser `, `error : `, then the message and a newline -- and php`` |
|      - |  280 | `	 * prints the four as the one line they are. Trimming each fragment here` |
|      - |  281 | `	 * instead printed four warnings for one diagnostic; the drain does the` |
|      - |  282 | `	 * flushing, exactly as it does for a structured message. */` |
|     54 |  283 | `	SyBlobAppend(&sMsg,"",1);   /* the queue copies a C string */` |
|     54 |  284 | `	PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,1,0,0,(const char *)SyBlobData(&sMsg),"");` |
|     54 |  285 | `	SyBlobRelease(&sMsg);` |
|     25 |  286 | `}` |
|      - |  287 | `/* xmlSetGenericErrorFunc is deprecated from libxml 2.12 and the MSVC gate` |
|      - |  288 | ` * refuses a deprecated symbol under /WX. It is still the only door onto that` |
|      - |  289 | ` * channel, so the deprecation is suppressed at this one call pair. */` |
|   9298 |  290 | `static void LibxmlGenericSet(ph7_vm *pVm,int bOn)` |
|      5 |  291 | `{` |
|      - |  292 | `#if defined(_MSC_VER)` |
|      - |  293 | `#pragma warning(push)` |
|      - |  294 | `#pragma warning(disable:4996)` |
|      - |  295 | `#endif` |
|   9303 |  296 | `	xmlSetGenericErrorFunc(bOn ? (void *)pVm : 0,bOn ? LibxmlGenericErr : 0);` |
|      - |  297 | `#if defined(_MSC_VER)` |
|      - |  298 | `#pragma warning(pop)` |
|      - |  299 | `#endif` |
|   9303 |  300 | `}` |
|      - |  301 | `/*` |
|      - |  302 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  303 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  304 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  305 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  306 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  307 | ` * (php's exact wording for the memory-parser case).` |
|      - |  308 | ` */` |
|   4484 |  309 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      5 |  310 | `{` |
|   4489 |  311 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);` |
|   4489 |  312 | `	LibxmlGenericSet(pVm,1);` |
|   4489 |  313 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      5 |  314 | `}` |
|      - |  315 | `/*` |
|      - |  316 | ` * The same window for an entry point that leaves libxml's OWN formatting in` |
|      - |  317 | ` * place -- ext/simplexml's parse, which php reports through the generic channel` |
|      - |  318 | ` * and not through the structured one.` |
|      - |  319 | ` *` |
|      - |  320 | ` * The difference is visible in every parse diagnostic the two extensions raise:` |
|      - |  321 | `` * ext/dom's is php's one-line `StartTag: invalid element name in Entity,`` |
|      - |  322 | `` * line: 1`, while ext/simplexml's is libxml's own three lines --`` |
|      - |  323 | `` * `Entity: line 1: parser error : StartTag: invalid element name`, the offending`` |
|      - |  324 | ` * source line, and a caret under it. That is what libxml's default parser-error` |
|      - |  325 | ` * routine prints, and it only runs when no structured handler is installed.` |
|      - |  326 | `` * With `libxml_use_internal_errors()` ON php's structured handler takes`` |
|      - |  327 | `` * precedence there too, and the queue `libxml_get_errors()` answers carries the`` |
|      - |  328 | ` * error CODES -- so the structured handler is still installed for that case.` |
|      - |  329 | ` */` |
|    166 |  330 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBeginRaw(ph7_vm *pVm)` |
|      4 |  331 | `{` |
|    170 |  332 | `	xmlSetStructuredErrorFunc(pVm->bLibxmlInternalErr ? pVm : 0,` |
|    166 |  333 | `		pVm->bLibxmlInternalErr ? LibxmlStructuredErr : 0);` |
|    170 |  334 | `	LibxmlGenericSet(pVm,1);` |
|    170 |  335 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      4 |  336 | `}` |
|      - |  337 | `/*` |
|      - |  338 | ` * End a capture window WITHOUT reporting what it caught: for an entry point` |
|      - |  339 | ` * whose failure php reports through the return value alone (a stream write` |
|      - |  340 | ` * that could not land under XMLWriter), where libxml's own message would be a` |
|      - |  341 | ` * warning php never raises. Entries stay queued when internal capture is on --` |
|      - |  342 | `` * `libxml_get_errors()` is the one place php does show them.`` |
|      - |  343 | ` */` |
|     70 |  344 | `PH7_PRIVATE void PH7_LibxmlDropErrors(ph7_vm *pVm,sxu32 nMark)` |
|      2 |  345 | `{` |
|     72 |  346 | `	xmlSetStructuredErrorFunc(0,0);` |
|     72 |  347 | `	LibxmlGenericSet(pVm,0);` |
|     72 |  348 | `	if( pVm->bLibxmlInternalErr ){` |
|    ! 0 |  349 | `		return;` |
|      - |  350 | `	}` |
|     72 |  351 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|      5 |  352 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  353 | `		sxu32 n;` |
|      9 |  354 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      5 |  355 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|      3 |  356 | `		}` |
|      5 |  357 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|      2 |  358 | `	}` |
|     37 |  359 | `}` |
|   2808 |  360 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      3 |  361 | `{` |
|   2811 |  362 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFnName,0);` |
|   2811 |  363 | `}` |
|      - |  364 | `/*` |
|      - |  365 | ` * The same drain for a parse that was given OPTIONS, two of which are about` |
|      - |  366 | `` * these very diagnostics: `LIBXML_NOERROR` silences the errors and the fatals,`` |
|      - |  367 | `` * `LIBXML_NOWARNING` the warnings. php silences them at the PRINT, not at the`` |
|      - |  368 | `` * source -- the queue `libxml_get_errors()` answers still holds them, and so`` |
|      - |  369 | `` * does the slot `libxml_get_last_error()` reads -- so this skips them here,`` |
|      - |  370 | ` * after they have been recorded, and the line buffer never sees them either` |
|      - |  371 | ` * (php's does not: libxml's message never reaches the printer at all).` |
|      - |  372 | ` */` |
|   4578 |  373 | `PH7_PRIVATE void PH7_LibxmlCaptureEndOpts(ph7_vm *pVm,sxu32 nMark,const char *zFnName,int iOpts)` |
|      5 |  374 | `{` |
|   4583 |  375 | `	xmlSetStructuredErrorFunc(0,0);` |
|   4583 |  376 | `	LibxmlGenericSet(pVm,0);` |
|   4583 |  377 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  378 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|     94 |  379 | `		return;` |
|      - |  380 | `	}` |
|   4491 |  381 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    206 |  382 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  383 | `		sxu32 n;` |
|    367 |  384 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  385 | `			SyString sFunc;` |
|      - |  386 | `			SyBlob sMsg;` |
|      - |  387 | `			const char *zPend;` |
|      - |  388 | `			sxu32 nPend,nTrim;` |
|    364 |  389 | `			if( (aErr[n].iLevel == XML_ERR_WARNING)` |
|    134 |  390 | `			 ? (iOpts & XML_PARSE_NOWARNING) != 0` |
|    147 |  391 | `			 : (iOpts & XML_PARSE_NOERROR) != 0 ){` |
|    139 |  392 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|    156 |  393 | `				continue;` |
|      - |  394 | `			}` |
|      - |  395 | `			/* php's libxml diagnostics are LINE-buffered: each message is` |
|      - |  396 | `			 * appended to one buffer and the diagnostic is only raised when the` |
|      - |  397 | `			 * accumulated text ends in a newline. Most of libxml's messages do,` |
|      - |  398 | `			 * so most of them stand alone -- but the ones that do not (libxml's` |
|      - |  399 | `			 * "Validation failed: no DTD found !" is one) are held back and` |
|      - |  400 | `			 * printed JOINED to whatever comes next, even from a later parse of` |
|      - |  401 | `			 * a different document, and are never printed at all if nothing` |
|      - |  402 | `			 * else follows. Reproduced rather than tidied up: a program's` |
|      - |  403 | `			 * output is what it is. */` |
|    143 |  404 | `			SyBlobAppend(&pVm->sLibxmlPend,aErr[n].sMsg.zString,aErr[n].sMsg.nByte);` |
|    143 |  405 | `			zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|    143 |  406 | `			nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|    143 |  407 | `			if( nPend < 1 \|\| zPend[nPend-1] != '\n' ){` |
|      - |  408 | `				/* no line yet: hold it for the next message */` |
|     36 |  409 | `				LibxmlFreeErr(pVm,&aErr[n]);` |
|     36 |  410 | `				continue;` |
|      - |  411 | `			}` |
|    109 |  412 | `			nTrim = nPend;` |
|      - |  413 | `			/* php trims the trailing newline off the warning copy */` |
|    265 |  414 | `			while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|    109 |  415 | `				nTrim--;` |
|      2 |  416 | `			}` |
|    109 |  417 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    109 |  418 | `			SyBlobAppend(&sMsg,zPend,nTrim);` |
|      - |  419 | `			/* php appends the source location only for parser errors that` |
|      - |  420 | `			 * carry a real line; generic libxml errors print bare. The location` |
|      - |  421 | `			 * and the LEVEL are the flushing message's, not the held one's. */` |
|    109 |  422 | `			if( aErr[n].iLine > 0 ){` |
|     54 |  423 | `				if( aErr[n].sFile.nByte > 0 ){` |
|      7 |  424 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|      4 |  425 | `				}else{` |
|     48 |  426 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  427 | `				}` |
|     25 |  428 | `			}` |
|    109 |  429 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    109 |  430 | `			if( zFnName ){` |
|    109 |  431 | `				SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|     49 |  432 | `			}` |
|      - |  433 | `			/* php reports libxml's own SEVERITY: a warning (an unsupported XML` |
|      - |  434 | `			 * version, a DTD the content does not follow) is an E_NOTICE there` |
|      - |  435 | `			 * and only an error or a fatal is an E_WARNING. The distinction is` |
|      - |  436 | ``			 * visible to any set_error_handler() and to `error_reporting` --`` |
|      - |  437 | `			 * a handler screening on E_WARNING must not see the warnings. */` |
|    109 |  438 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,` |
|    107 |  439 | `				aErr[n].iLevel == XML_ERR_WARNING ? PH7_CTX_NOTICE : PH7_CTX_WARNING,` |
|    107 |  440 | `				(const char *)SyBlobData(&sMsg));` |
|    109 |  441 | `			SyBlobRelease(&sMsg);` |
|    109 |  442 | `			SyBlobReset(&pVm->sLibxmlPend);` |
|    109 |  443 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|     51 |  444 | `		}` |
|     88 |  445 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|     44 |  446 | `	}` |
|   2176 |  447 | `}` |
|      - |  448 | `/*` |
|      - |  449 | ` * php's OTHER libxml channel.` |
|      - |  450 | ` *` |
|      - |  451 | ` * libxml reports an I/O failure through its GENERIC error function rather than` |
|      - |  452 | ` * the structured one, with the severity spelled INTO the text ("I/O warning :` |
|      - |  453 | ` * ..."), and php prints that text at E_WARNING whatever the severity says --` |
|      - |  454 | `` * while the structured copy, which is what `libxml_get_errors()` reports, keeps`` |
|      - |  455 | ` * libxml's own level and carries no such prefix. A caller with a message from` |
|      - |  456 | ` * that channel hands it here: it goes through the same line buffer as every` |
|      - |  457 | ` * other diagnostic (so a held fragment is printed in front of it) and takes no` |
|      - |  458 | ` * source location, because that channel has none.` |
|      - |  459 | ` */` |
|      4 |  460 | `PH7_PRIVATE void PH7_LibxmlRaiseGeneric(ph7_vm *pVm,const char *zFnName,const char *zMsg)` |
|      1 |  461 | `{` |
|      - |  462 | `	SyString sFunc;` |
|      - |  463 | `	SyBlob sOut;` |
|      - |  464 | `	const char *zPend;` |
|      - |  465 | `	sxu32 nPend,nTrim;` |
|      5 |  466 | `	SyBlobAppend(&pVm->sLibxmlPend,zMsg,SyStrlen(zMsg));` |
|      5 |  467 | `	zPend = (const char *)SyBlobData(&pVm->sLibxmlPend);` |
|      5 |  468 | `	nPend = SyBlobLength(&pVm->sLibxmlPend);` |
|      5 |  469 | `	if( nPend < 1 \|\| zPend[nPend-1] != '\n' ){` |
|    ! 0 |  470 | `		return;` |
|      - |  471 | `	}` |
|      5 |  472 | `	nTrim = nPend;` |
|     11 |  473 | `	while( nTrim > 0 && (zPend[nTrim-1] == '\n' \|\| zPend[nTrim-1] == '\r') ){` |
|      5 |  474 | `		nTrim--;` |
|      1 |  475 | `	}` |
|      5 |  476 | `	SyBlobInit(&sOut,&pVm->sAllocator);` |
|      5 |  477 | `	SyBlobAppend(&sOut,zPend,nTrim);` |
|      5 |  478 | `	SyBlobAppend(&sOut,"\0",sizeof(char));` |
|      5 |  479 | `	SyStringInitFromBuf(&sFunc,zFnName,zFnName ? SyStrlen(zFnName) : 0);` |
|      3 |  480 | `	PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,` |
|      4 |  481 | `		(const char *)SyBlobData(&sOut));` |
|      5 |  482 | `	SyBlobRelease(&sOut);` |
|      5 |  483 | `	SyBlobReset(&pVm->sLibxmlPend);` |
|      3 |  484 | `}` |
|      - |  485 | `/*` |
|      - |  486 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  487 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  488 | ` */` |
|   3450 |  489 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      5 |  490 | `{` |
|      - |  491 | `	phl_xmldoc *pDoc;` |
|   3455 |  492 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|   3455 |  493 | `	if( pDoc == 0 ){` |
|    ! 0 |  494 | `		return 0;` |
|      - |  495 | `	}` |
|   3455 |  496 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|   3455 |  497 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|   3455 |  498 | `	SySetInit(&pDoc->aNotations,&pVm->sAllocator,sizeof(void *));` |
|   3455 |  499 | `	pDoc->pDoc = pXmlDocPtr;` |
|   3455 |  500 | `	pDoc->pVm = &(*pVm);` |
|   3455 |  501 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|   3455 |  502 | `	pDoc->bFormatOutput = 0;` |
|   3455 |  503 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|   3455 |  504 | `	pVm->pXmlDocs = (void *)pDoc;` |
|   3455 |  505 | `	return pDoc;` |
|   1730 |  506 | `}` |
|      - |  507 |  |
|      - |  508 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  509 |  |
|      - |  510 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  511 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  512 | `		SXUNUSED(pUnused); \` |
|      - |  513 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  514 | `	}` |
|     73 |  515 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|     77 |  516 | `LIBXML_INT_CONST(LibxmlConst_RECOVER,        XML_PARSE_RECOVER)` |
|     77 |  517 | `LIBXML_INT_CONST(LibxmlConst_HTML_NOIMPLIED, 8192)   /* HTML_PARSE_NOIMPLIED */` |
|     77 |  518 | `LIBXML_INT_CONST(LibxmlConst_HTML_NODEFDTD,  4)      /* HTML_PARSE_NODEFDTD */` |
|     73 |  519 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|     73 |  520 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|     73 |  521 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|     73 |  522 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|     83 |  523 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|     79 |  524 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|     73 |  525 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|     73 |  526 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|     73 |  527 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|     76 |  528 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|     73 |  529 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|     73 |  530 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|     73 |  531 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|     73 |  532 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|     73 |  533 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|     77 |  534 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|     77 |  535 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|     75 |  536 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|     73 |  537 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|     73 |  538 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|     73 |  539 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|     75 |  540 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  541 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|     73 |  542 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|     73 |  543 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|     73 |  544 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|     73 |  545 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|     73 |  546 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|     73 |  547 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|     73 |  548 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|     73 |  549 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|     73 |  550 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|     75 |  551 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|     73 |  552 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|     73 |  553 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|     73 |  554 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|     73 |  555 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|     75 |  556 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_DECL_NODE,   XML_ELEMENT_DECL)` |
|     75 |  557 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_DECL_NODE, XML_ATTRIBUTE_DECL)` |
|     75 |  558 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_DECL_NODE,    XML_ENTITY_DECL)` |
|      - |  559 | `/* php spells libxml's XML_NAMESPACE_DECL twice, under both DOM's name for a` |
|      - |  560 | ` * namespace node and libxml's own. */` |
|     75 |  561 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_DECL_NODE, XML_NAMESPACE_DECL)` |
|     75 |  562 | `LIBXML_INT_CONST(LibxmlConst_LOCAL_NAMESPACE,     XML_NAMESPACE_DECL)` |
|      - |  563 | `/*` |
|      - |  564 | ` * The DTD attribute-TYPE enum. php's numbers are libxml's xmlAttributeType with` |
|      - |  565 | ` * one deliberate hole: php has no XML_ATTRIBUTE_ENTITIES and gives the name` |
|      - |  566 | ` * XML_ATTRIBUTE_ENTITY libxml's ENTITIES value (6), so the two disagree about` |
|      - |  567 | ` * what "entity" means by one.  php's numbering is the contract.` |
|      - |  568 | ` */` |
|     75 |  569 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_CDATA,       XML_ATTRIBUTE_CDATA)` |
|     75 |  570 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ID,          XML_ATTRIBUTE_ID)` |
|     75 |  571 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREF,       XML_ATTRIBUTE_IDREF)` |
|     75 |  572 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_IDREFS,      XML_ATTRIBUTE_IDREFS)` |
|     75 |  573 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENTITY,      XML_ATTRIBUTE_ENTITIES)` |
|     75 |  574 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKEN,     XML_ATTRIBUTE_NMTOKEN)` |
|     75 |  575 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NMTOKENS,    XML_ATTRIBUTE_NMTOKENS)` |
|     75 |  576 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_ENUMERATION, XML_ATTRIBUTE_ENUMERATION)` |
|     75 |  577 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NOTATION,    XML_ATTRIBUTE_NOTATION)` |
|      - |  578 | `/*` |
|      - |  579 | ` * ext/dom's DOMException codes -- the DOM level-2 numbering an exception's` |
|      - |  580 | ` * getCode() answers, which is what a catch tests to tell one refusal from` |
|      - |  581 | ` * another (php's own zero, DOM_PHP_ERR, is the code for everything that is not` |
|      - |  582 | ` * a DOM error).` |
|      - |  583 | ` */` |
|     81 |  584 | `LIBXML_INT_CONST(LibxmlConst_PHP_ERR,                  0)` |
|     75 |  585 | `LIBXML_INT_CONST(LibxmlConst_INDEX_SIZE_ERR,           1)` |
|     75 |  586 | `LIBXML_INT_CONST(LibxmlConst_DOMSTRING_SIZE_ERR,       2)` |
|     75 |  587 | `LIBXML_INT_CONST(LibxmlConst_HIERARCHY_REQUEST_ERR,    3)` |
|     75 |  588 | `LIBXML_INT_CONST(LibxmlConst_WRONG_DOCUMENT_ERR,       4)` |
|     75 |  589 | `LIBXML_INT_CONST(LibxmlConst_INVALID_CHARACTER_ERR,    5)` |
|     75 |  590 | `LIBXML_INT_CONST(LibxmlConst_NO_DATA_ALLOWED_ERR,      6)` |
|     75 |  591 | `LIBXML_INT_CONST(LibxmlConst_NO_MODIFICATION_ALLOWED_ERR, 7)` |
|     77 |  592 | `LIBXML_INT_CONST(LibxmlConst_NOT_FOUND_ERR,            8)` |
|     75 |  593 | `LIBXML_INT_CONST(LibxmlConst_NOT_SUPPORTED_ERR,        9)` |
|     75 |  594 | `LIBXML_INT_CONST(LibxmlConst_INUSE_ATTRIBUTE_ERR,     10)` |
|     75 |  595 | `LIBXML_INT_CONST(LibxmlConst_INVALID_STATE_ERR,       11)` |
|     75 |  596 | `LIBXML_INT_CONST(LibxmlConst_SYNTAX_ERR,              12)` |
|     75 |  597 | `LIBXML_INT_CONST(LibxmlConst_INVALID_MODIFICATION_ERR,13)` |
|     75 |  598 | `LIBXML_INT_CONST(LibxmlConst_NAMESPACE_ERR,           14)` |
|     75 |  599 | `LIBXML_INT_CONST(LibxmlConst_INVALID_ACCESS_ERR,      15)` |
|     75 |  600 | `LIBXML_INT_CONST(LibxmlConst_VALIDATION_ERR,          16)` |
|      - |  601 |  |
|     70 |  602 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|      3 |  603 | `{` |
|     35 |  604 | `	SXUNUSED(pUnused);` |
|     73 |  605 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|     73 |  606 | `}` |
|     70 |  607 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|      3 |  608 | `{` |
|     35 |  609 | `	SXUNUSED(pUnused);` |
|      - |  610 | `	/* php exposes the runtime-loaded version string here */` |
|     73 |  611 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|     73 |  612 | `}` |
|      - |  613 |  |
|   6691 |  614 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  615 | `{` |
|      - |  616 | `	static const struct {` |
|      - |  617 | `		const char *zName;` |
|      - |  618 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  619 | `	} aConst[] = {` |
|      - |  620 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  621 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  622 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  623 | `		{ "LIBXML_RECOVER",        LibxmlConst_RECOVER        },` |
|      - |  624 | `		{ "LIBXML_HTML_NOIMPLIED", LibxmlConst_HTML_NOIMPLIED },` |
|      - |  625 | `		{ "LIBXML_HTML_NODEFDTD",  LibxmlConst_HTML_NODEFDTD  },` |
|      - |  626 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  627 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  628 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  629 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  630 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  631 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  632 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  633 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  634 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  635 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  636 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  637 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  638 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  639 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  640 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  641 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  642 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  643 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  644 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  645 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  646 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  647 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  648 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  649 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  650 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  651 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  652 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  653 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  654 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  655 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  656 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  657 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  658 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  659 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  660 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  661 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  662 | `		{ "XML_ELEMENT_DECL_NODE",  LibxmlConst_ELEMENT_DECL_NODE  },` |
|      - |  663 | `		{ "XML_ATTRIBUTE_DECL_NODE",LibxmlConst_ATTRIBUTE_DECL_NODE},` |
|      - |  664 | `		{ "XML_ENTITY_DECL_NODE",   LibxmlConst_ENTITY_DECL_NODE   },` |
|      - |  665 | `		{ "XML_NAMESPACE_DECL_NODE",LibxmlConst_NAMESPACE_DECL_NODE},` |
|      - |  666 | `		{ "XML_LOCAL_NAMESPACE",    LibxmlConst_LOCAL_NAMESPACE    },` |
|      - |  667 | `		{ "XML_ATTRIBUTE_CDATA",       LibxmlConst_ATTRIBUTE_CDATA       },` |
|      - |  668 | `		{ "XML_ATTRIBUTE_ID",          LibxmlConst_ATTRIBUTE_ID          },` |
|      - |  669 | `		{ "XML_ATTRIBUTE_IDREF",       LibxmlConst_ATTRIBUTE_IDREF       },` |
|      - |  670 | `		{ "XML_ATTRIBUTE_IDREFS",      LibxmlConst_ATTRIBUTE_IDREFS      },` |
|      - |  671 | `		{ "XML_ATTRIBUTE_ENTITY",      LibxmlConst_ATTRIBUTE_ENTITY      },` |
|      - |  672 | `		{ "XML_ATTRIBUTE_NMTOKEN",     LibxmlConst_ATTRIBUTE_NMTOKEN     },` |
|      - |  673 | `		{ "XML_ATTRIBUTE_NMTOKENS",    LibxmlConst_ATTRIBUTE_NMTOKENS    },` |
|      - |  674 | `		{ "XML_ATTRIBUTE_ENUMERATION", LibxmlConst_ATTRIBUTE_ENUMERATION },` |
|      - |  675 | `		{ "XML_ATTRIBUTE_NOTATION",    LibxmlConst_ATTRIBUTE_NOTATION    },` |
|      - |  676 | `		{ "DOM_PHP_ERR",                    LibxmlConst_PHP_ERR                    },` |
|      - |  677 | `		{ "DOM_INDEX_SIZE_ERR",             LibxmlConst_INDEX_SIZE_ERR             },` |
|      - |  678 | `		{ "DOMSTRING_SIZE_ERR",             LibxmlConst_DOMSTRING_SIZE_ERR         },` |
|      - |  679 | `		{ "DOM_HIERARCHY_REQUEST_ERR",      LibxmlConst_HIERARCHY_REQUEST_ERR      },` |
|      - |  680 | `		{ "DOM_WRONG_DOCUMENT_ERR",         LibxmlConst_WRONG_DOCUMENT_ERR         },` |
|      - |  681 | `		{ "DOM_INVALID_CHARACTER_ERR",      LibxmlConst_INVALID_CHARACTER_ERR      },` |
|      - |  682 | `		{ "DOM_NO_DATA_ALLOWED_ERR",        LibxmlConst_NO_DATA_ALLOWED_ERR        },` |
|      - |  683 | `		{ "DOM_NO_MODIFICATION_ALLOWED_ERR",LibxmlConst_NO_MODIFICATION_ALLOWED_ERR},` |
|      - |  684 | `		{ "DOM_NOT_FOUND_ERR",              LibxmlConst_NOT_FOUND_ERR              },` |
|      - |  685 | `		{ "DOM_NOT_SUPPORTED_ERR",          LibxmlConst_NOT_SUPPORTED_ERR          },` |
|      - |  686 | `		{ "DOM_INUSE_ATTRIBUTE_ERR",        LibxmlConst_INUSE_ATTRIBUTE_ERR        },` |
|      - |  687 | `		{ "DOM_INVALID_STATE_ERR",          LibxmlConst_INVALID_STATE_ERR          },` |
|      - |  688 | `		{ "DOM_SYNTAX_ERR",                 LibxmlConst_SYNTAX_ERR                 },` |
|      - |  689 | `		{ "DOM_INVALID_MODIFICATION_ERR",   LibxmlConst_INVALID_MODIFICATION_ERR   },` |
|      - |  690 | `		{ "DOM_NAMESPACE_ERR",              LibxmlConst_NAMESPACE_ERR              },` |
|      - |  691 | `		{ "DOM_INVALID_ACCESS_ERR",         LibxmlConst_INVALID_ACCESS_ERR         },` |
|      - |  692 | `		{ "DOM_VALIDATION_ERR",             LibxmlConst_VALIDATION_ERR             },` |
|      - |  693 | `	};` |
|      - |  694 | `	sxu32 n;` |
| 495139 |  695 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 488448 |  696 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
| 243825 |  697 | `	}` |
|   6696 |  698 | `}` |
|      - |  699 | `/* ===== libxml_* native thunks ===== */` |
|      - |  700 |  |
|      - |  701 | `/*` |
|      - |  702 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  703 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  704 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  705 | ` */` |
|    184 |  706 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  707 | `{` |
|    186 |  708 | `	ph7_vm *pVm = pCtx->pVm;` |
|    186 |  709 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|    186 |  710 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    180 |  711 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|    180 |  712 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  713 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|    106 |  714 | `			LibxmlClearQueue(pVm);` |
|     52 |  715 | `		}` |
|     89 |  716 | `	}` |
|    186 |  717 | `	ph7_result_bool(pCtx,bPrev);` |
|    186 |  718 | `	return PH7_OK;` |
|      2 |  719 | `}` |
|      - |  720 | `/*` |
|      - |  721 | ` * array __libxml_get_errors_raw()` |
|      - |  722 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  723 | ` */` |
|      - |  724 | `/*` |
|      - |  725 | ` * Build one LibXMLError from a recorded error.` |
|      - |  726 | ` *` |
|      - |  727 | `` * The prelude did this in PHP (`__phl_libxml_err_obj()` copying six array keys`` |
|      - |  728 | `` * onto a `new LibXMLError`), over an array these accessors returned purely so`` |
|      - |  729 | ` * that PHP could reshape it. Both the array hop and the helper are gone: the` |
|      - |  730 | ` * accessors below now answer the objects php answers.` |
|      - |  731 | ` */` |
|     61 |  732 | `static ph7_class_instance * LibxmlErrObject(ph7_context *pCtx,phl_libxml_err *pErr)` |
|      2 |  733 | `{` |
|     63 |  734 | `	ph7_vm *pVm = pCtx->pVm;` |
|     63 |  735 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"LibXMLError",sizeof("LibXMLError")-1,0,0);` |
|      - |  736 | `	ph7_class_instance *pObj;` |
|      - |  737 | `	ph7_value sVal;` |
|      - |  738 | `	SyString sStr;` |
|     63 |  739 | `	if( pClass == 0 ){` |
|    ! 0 |  740 | `		return 0;` |
|      - |  741 | `	}` |
|     63 |  742 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     63 |  743 | `	if( pObj == 0 ){` |
|    ! 0 |  744 | `		return 0;` |
|      - |  745 | `	}` |
|     63 |  746 | `	PH7_MemObjInit(pVm,&sVal);` |
|     63 |  747 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLevel);` |
|     63 |  748 | `	PH7_NativeSetProp(pVm,pObj,"level",sizeof("level")-1,&sVal);` |
|     63 |  749 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iCode);` |
|     63 |  750 | `	PH7_NativeSetProp(pVm,pObj,"code",sizeof("code")-1,&sVal);` |
|     63 |  751 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iColumn);` |
|     63 |  752 | `	PH7_NativeSetProp(pVm,pObj,"column",sizeof("column")-1,&sVal);` |
|     63 |  753 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLine);` |
|     63 |  754 | `	PH7_NativeSetProp(pVm,pObj,"line",sizeof("line")-1,&sVal);` |
|     63 |  755 | `	SyStringInitFromBuf(&sStr,pErr->sMsg.zString ? pErr->sMsg.zString : "",pErr->sMsg.nByte);` |
|     63 |  756 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     63 |  757 | `	PH7_NativeSetProp(pVm,pObj,"message",sizeof("message")-1,&sVal);` |
|     63 |  758 | `	SyStringInitFromBuf(&sStr,pErr->sFile.zString ? pErr->sFile.zString : "",pErr->sFile.nByte);` |
|     63 |  759 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     63 |  760 | `	PH7_NativeSetProp(pVm,pObj,"file",sizeof("file")-1,&sVal);` |
|     63 |  761 | `	PH7_MemObjRelease(&sVal);` |
|     63 |  762 | `	return pObj;` |
|     32 |  763 | `}` |
|      - |  764 | `/* array libxml_get_errors() */` |
|     64 |  765 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  766 | `{` |
|     66 |  767 | `	ph7_vm *pVm = pCtx->pVm;` |
|     66 |  768 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|     66 |  769 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|     66 |  770 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  771 | `	sxu32 n;` |
|     32 |  772 | `	SXUNUSED(nArg);` |
|     32 |  773 | `	SXUNUSED(apArg);` |
|     66 |  774 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  775 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  776 | `		ph7_result_null(pCtx);` |
|    ! 0 |  777 | `		return PH7_OK;` |
|      - |  778 | `	}` |
|    119 |  779 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     55 |  780 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,&aErr[n]);` |
|      - |  781 | `		ph7_value sEntry;` |
|     55 |  782 | `		if( pObj == 0 ){` |
|    ! 0 |  783 | `			break;` |
|      - |  784 | `		}` |
|      - |  785 | `		/* The list takes over the instance's initial iRef=1: ph7_array_add_elem` |
|      - |  786 | `		 * copies the slot (taking its own reference), so the local one is dropped. */` |
|     55 |  787 | `		PH7_MemObjInit(pVm,&sEntry);` |
|     55 |  788 | `		sEntry.x.pOther = pObj;` |
|     55 |  789 | `		sEntry.iFlags = MEMOBJ_OBJ;` |
|     55 |  790 | `		ph7_array_add_elem(pList,0,&sEntry);` |
|     55 |  791 | `		PH7_ClassInstanceUnref(pObj);` |
|     28 |  792 | `	}` |
|     66 |  793 | `	ph7_result_value(pCtx,pList);` |
|     66 |  794 | `	return PH7_OK;` |
|     34 |  795 | `}` |
|      - |  796 | `/*` |
|      - |  797 | ` * void __libxml_clear_errors()` |
|      - |  798 | ` */` |
|    148 |  799 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  800 | `{` |
|     74 |  801 | `	SXUNUSED(nArg);` |
|     74 |  802 | `	SXUNUSED(apArg);` |
|    150 |  803 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|    150 |  804 | `	ph7_result_null(pCtx);` |
|    150 |  805 | `	return PH7_OK;` |
|      2 |  806 | `}` |
|      - |  807 | `/*` |
|      - |  808 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  809 | ` */` |
|     12 |  810 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  811 | `{` |
|     14 |  812 | `	ph7_vm *pVm = pCtx->pVm;` |
|     14 |  813 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      6 |  814 | `	SXUNUSED(nArg);` |
|      6 |  815 | `	SXUNUSED(apArg);` |
|     14 |  816 | `	if( pLast == 0 ){` |
|      6 |  817 | `		ph7_result_bool(pCtx,0);` |
|      6 |  818 | `		return PH7_OK;` |
|      - |  819 | `	}` |
|      - |  820 | `	{` |
|     10 |  821 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,pLast);` |
|      - |  822 | `		ph7_value sRes;` |
|     10 |  823 | `		if( pObj == 0 ){` |
|    ! 0 |  824 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  825 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  826 | `			return PH7_OK;` |
|      - |  827 | `		}` |
|     10 |  828 | `		PH7_MemObjInit(pVm,&sRes);` |
|     10 |  829 | `		sRes.x.pOther = pObj;` |
|     10 |  830 | `		sRes.iFlags = MEMOBJ_OBJ;` |
|     10 |  831 | `		ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|     10 |  832 | `		PH7_ClassInstanceUnref(pObj);` |
|      - |  833 | `	}` |
|     10 |  834 | `	return PH7_OK;` |
|      8 |  835 | `}` |
|      - |  836 |  |
|      - |  837 | `/*` |
|      - |  838 | ` * ?callable libxml_get_external_entity_loader()` |
|      - |  839 | ` *  The stored resolver VERBATIM (a callable string answers as that string),` |
|      - |  840 | ` *  or null for the default loader -- php's answer shape.` |
|      - |  841 | ` */` |
|      8 |  842 | `static int vm_builtin_libxml_get_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  843 | `{` |
|      9 |  844 | `	ph7_vm *pVm = pCtx->pVm;` |
|      4 |  845 | `	SXUNUSED(nArg);` |
|      4 |  846 | `	SXUNUSED(apArg);` |
|      9 |  847 | `	if( (pVm->sXmlEntLoader.iFlags & MEMOBJ_NULL) == 0 ){` |
|      5 |  848 | `		ph7_result_value(pCtx,&pVm->sXmlEntLoader); /* makes its own copy */` |
|      3 |  849 | `	}else{` |
|      5 |  850 | `		ph7_result_null(pCtx);` |
|      - |  851 | `	}` |
|      9 |  852 | `	return PH7_OK;` |
|      1 |  853 | `}` |
|      - |  854 | `/*` |
|      - |  855 | ` * true libxml_set_external_entity_loader(?callable $resolver_function)` |
|      - |  856 | ` *  Store the resolver (null restores the default). The slot is never` |
|      - |  857 | ` *  INVOKED here -- no PHL parse path loads an external entity, the same` |
|      - |  858 | ` *  off-by-default php's sanitized parser options enforce -- so the` |
|      - |  859 | ` *  round-trip contract is the whole observable surface.` |
|      - |  860 | ` */` |
|     10 |  861 | `static int vm_builtin_libxml_set_external_entity_loader(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  862 | `{` |
|     11 |  863 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  864 | `	if( nArg < 1 ){` |
|    ! 0 |  865 | `		ph7_result_bool(pCtx,1);` |
|    ! 0 |  866 | `		return PH7_OK;` |
|      - |  867 | `	}` |
|     11 |  868 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      - |  869 | `		/* php screens through the FCC machinery: a string must resolve as a` |
|      - |  870 | `		 * CALLABLE, and the refusal names the callback rule. */` |
|      9 |  871 | `		sxi32 rc = PH7_CheckCallbackArg(pCtx,apArg[0],1,"resolver_function",1);` |
|      9 |  872 | `		if( rc != PH7_OK ){` |
|      5 |  873 | `			return rc;` |
|      - |  874 | `		}` |
|      2 |  875 | `	}` |
|      7 |  876 | `	PH7_MemObjRelease(&pVm->sXmlEntLoader);` |
|      7 |  877 | `	if( !ph7_value_is_null(apArg[0]) ){` |
|      5 |  878 | `		PH7_MemObjStore(apArg[0],&pVm->sXmlEntLoader);` |
|      2 |  879 | `	}` |
|      7 |  880 | `	ph7_result_bool(pCtx,1);` |
|      7 |  881 | `	return PH7_OK;` |
|      6 |  882 | `}` |
|      - |  883 | `/*` |
|      - |  884 | ` * void libxml_set_streams_context($context)` |
|      - |  885 | ` *  Take a stream-context RESOURCE and keep it for the document loaders.` |
|      - |  886 | ` *  php validates lazily at the next load; PHL has no loader that would` |
|      - |  887 | ` *  ever read it (the consumer is the http:// wrapper), so` |
|      - |  888 | ` *  the check runs here, with php's own two messages.` |
|      - |  889 | ` */` |
|      8 |  890 | `static int vm_builtin_libxml_set_streams_context(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  891 | `{` |
|      9 |  892 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  893 | `	const io_private *pDev;` |
|      9 |  894 | `	if( nArg < 1 ){` |
|    ! 0 |  895 | `		return PH7_OK;` |
|      - |  896 | `	}` |
|      9 |  897 | `	if( (apArg[0]->iFlags & MEMOBJ_RES) == 0 ){` |
|      3 |  898 | `		const char *zType = "null";` |
|      3 |  899 | `		if( apArg[0]->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }` |
|      3 |  900 | `		else if( apArg[0]->iFlags & MEMOBJ_OBJ ){ zType = "object"; }` |
|      3 |  901 | `		else if( apArg[0]->iFlags & MEMOBJ_STRING ){ zType = "string"; }` |
|      3 |  902 | `		else if( apArg[0]->iFlags & MEMOBJ_BOOL ){ zType = "bool"; }` |
|      3 |  903 | `		else if( apArg[0]->iFlags & MEMOBJ_REAL ){ zType = "float"; }` |
|      3 |  904 | `		else if( apArg[0]->iFlags & MEMOBJ_INT ){ zType = "int"; }` |
|      4 |  905 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  906 | `			"libxml_set_streams_context(): Argument #1 ($context) must be of type resource, %s given",` |
|      1 |  907 | `			zType);` |
|      - |  908 | `	}` |
|      7 |  909 | `	pDev = (const io_private *)apArg[0]->x.pOther;` |
|      7 |  910 | `	if( pDev == 0 \|\| pDev->iMagic != STREAM_CTX_MAGIC ){` |
|      3 |  911 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  912 | `			"libxml_set_streams_context(): supplied resource is not a valid Stream-Context resource");` |
|      - |  913 | `	}` |
|      5 |  914 | `	PH7_MemObjRelease(&pVm->sXmlStreamsCtx);` |
|      5 |  915 | `	PH7_MemObjStore(apArg[0],&pVm->sXmlStreamsCtx);` |
|      5 |  916 | `	ph7_result_null(pCtx);` |
|      5 |  917 | `	return PH7_OK;` |
|      5 |  918 | `}` |
|      - |  919 |  |
|      - |  920 | `/*` |
|      - |  921 | ` * The libxml PHP-visible surface: the LibXMLError class plus the seven` |
|      - |  922 | ` * libxml_* functions (php's eighth, libxml_disable_entity_loader(), is` |
|      - |  923 | ` * E_DEPRECATED since 8.0 and stays removed under the scope policy --` |
|      - |  924 | ` * twin-paired in 002-integration/function/libxml/).` |
|      - |  925 | ` */` |
|      - |  926 | `/* LibXMLError is declared from C below, and the four libxml_* functions ARE the` |
|      - |  927 | ` * C routines -- they used to be PHP wrappers over __libxml_* thunks, with a PHP` |
|      - |  928 | ` * helper reshaping an array into the object. */` |
|      - |  929 |  |
|      - |  930 | `/*` |
|      - |  931 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  932 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  933 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  934 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  935 | ` */` |
|   7925 |  936 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  937 | `{` |
|      - |  938 | `	/* Registered under the names php exposes. There is no thunk and no PHP` |
|      - |  939 | `	 * wrapper any more: these are the functions, so they are internal for` |
|      - |  940 | `	 * reflection and get the arity enforcement a prelude wrapper never had. */` |
|      - |  941 | `	static const struct {` |
|      - |  942 | `		const char *zName;` |
|      - |  943 | `		ProchHostFunction xFunc;` |
|      - |  944 | `	} aFunc[] = {` |
|      - |  945 | `		{ "libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  946 | `		{ "libxml_get_errors",          vm_builtin_libxml_get_errors_raw      },` |
|      - |  947 | `		{ "libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  948 | `		{ "libxml_get_last_error",      vm_builtin_libxml_get_last_error_raw  },` |
|      - |  949 | `		{ "libxml_get_external_entity_loader", vm_builtin_libxml_get_external_entity_loader },` |
|      - |  950 | `		{ "libxml_set_external_entity_loader", vm_builtin_libxml_set_external_entity_loader },` |
|      - |  951 | `		{ "libxml_set_streams_context", vm_builtin_libxml_set_streams_context },` |
|      - |  952 | `	};` |
|      - |  953 | `	/* Plain data carrier; php declares no methods on it. */` |
|      - |  954 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  955 | `		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  956 | `		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  957 | `		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  958 | `		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  959 | `		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  960 | `		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  961 | `	};` |
|      - |  962 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - |  963 | `		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - |  964 | `	};` |
|      - |  965 | `	sxu32 n;` |
|   7930 |  966 | `	LibxmlGlobalInit();` |
|   7930 |  967 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   7930 |  968 | `	SyBlobInit(&pVm->sLibxmlPend,&pVm->sAllocator);` |
|   7930 |  969 | `	pVm->bLibxmlInternalErr = 0;` |
|   7930 |  970 | `	pVm->pLibxmlLastErr = 0;` |
|   7930 |  971 | `	pVm->pXmlDocs = 0;` |
|   7930 |  972 | `	pVm->pXmlWriters = 0;` |
|   7930 |  973 | `	PH7_MemObjInit(pVm,&pVm->sXmlEntLoader);` |
|   7930 |  974 | `	PH7_MemObjInit(pVm,&pVm->sXmlStreamsCtx);` |
|  63405 |  975 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  55480 |  976 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  27704 |  977 | `	}` |
|      - |  978 | `	/* The class must exist before the accessors can build one. */` |
|   7930 |  979 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 |  980 | `}` |
|      - |  981 |  |
|      - |  982 | `#else` |
|      - |  983 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  984 | `typedef int vm_libxml_unused;` |
|      - |  985 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  986 |  |
