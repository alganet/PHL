# src/ph7/vm_libxml.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 240/334 lines (71.86%)

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
|   4670 |   38 | `static void LibxmlGlobalInit(void)` |
|      5 |   39 | `{` |
|      - |   40 | `	static int bInit = 0;` |
|   4675 |   41 | `	if( !bInit ){` |
|   4675 |   42 | `		xmlInitParser();` |
|   4675 |   43 | `		bInit = 1;` |
|   2335 |   44 | `	}` |
|   4675 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * Free one registered document: its orphaned subtrees first, then the tree` |
|      - |   48 | ` * itself.  Registry links and the phl_xmldoc shell live in SyMemBackend and` |
|      - |   49 | ` * are reclaimed with the VM allocator.` |
|      - |   50 | ` */` |
|    112 |   51 | `static void LibxmlFreeDoc(phl_xmldoc *pDoc)` |
|      1 |   52 | `{` |
|    113 |   53 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pDoc->aOrphans);` |
|      - |   54 | `	sxu32 n;` |
|    129 |   55 | `	for( n = 0 ; n < SySetUsed(&pDoc->aOrphans) ; ++n ){` |
|     17 |   56 | `		xmlUnlinkNode(apOrphan[n]);` |
|     17 |   57 | `		xmlFreeNode(apOrphan[n]);` |
|      9 |   58 | `	}` |
|    113 |   59 | `	SySetRelease(&pDoc->aOrphans);` |
|    113 |   60 | `	if( pDoc->pDoc ){` |
|    113 |   61 | `		xmlFreeDoc((xmlDocPtr)pDoc->pDoc);` |
|    113 |   62 | `		pDoc->pDoc = 0;` |
|     56 |   63 | `	}` |
|    113 |   64 | `}` |
|      - |   65 | `/*` |
|      - |   66 | ` * Reset the per-VM libxml state between executions: drop the accumulated` |
|      - |   67 | ` * error queue and free every document from the previous request.  Called` |
|      - |   68 | ` * from PH7_VmMakeReady() (which runs once per exec, including on VM reuse` |
|      - |   69 | ` * by the -S server) alongside the other per-exec field resets.` |
|      - |   70 | ` */` |
|   4080 |   71 | `PH7_PRIVATE void PH7_LibxmlVmReset(ph7_vm *pVm)` |
|      5 |   72 | `{` |
|      - |   73 | `	phl_xmldoc *pDoc,*pNext;` |
|   4085 |   74 | `	PH7_LibxmlClearErrors(pVm);` |
|   4085 |   75 | `	pVm->bLibxmlInternalErr = 0;` |
|   4085 |   76 | `	pDoc = (phl_xmldoc *)pVm->pXmlDocs;` |
|   4197 |   77 | `	while( pDoc ){` |
|    113 |   78 | `		pNext = pDoc->pNext;` |
|    113 |   79 | `		LibxmlFreeDoc(pDoc);` |
|    113 |   80 | `		SyMemBackendFree(&pVm->sAllocator,pDoc);` |
|    113 |   81 | `		pDoc = pNext;` |
|      1 |   82 | `	}` |
|   4085 |   83 | `	pVm->pXmlDocs = 0;` |
|      - |   84 | `	/* XMLWriter buffers live outside SyMemBackend too (see vm_xmlwriter.c) */` |
|   4085 |   85 | `	PH7_XmlWriterVmSweep(pVm);` |
|   4085 |   86 | `}` |
|      - |   87 | `/*` |
|      - |   88 | ` * Final teardown on VM release.  Must run before SyMemBackendRelease()` |
|      - |   89 | ` * wipes the allocator that holds the registry shells.` |
|      - |   90 | ` */` |
|   4072 |   91 | `PH7_PRIVATE void PH7_LibxmlVmRelease(ph7_vm *pVm)` |
|      5 |   92 | `{` |
|   4077 |   93 | `	PH7_LibxmlVmReset(pVm);` |
|   4077 |   94 | `	SySetRelease(&pVm->aLibxmlErr);` |
|   4077 |   95 | `}` |
|      - |   96 | `/*` |
|      - |   97 | ` * Release the copied message/file strings of one queue entry.` |
|      - |   98 | ` */` |
|     28 |   99 | `static void LibxmlFreeErr(ph7_vm *pVm,phl_libxml_err *pErr)` |
|      1 |  100 | `{` |
|     29 |  101 | `	if( pErr->sMsg.zString ){` |
|     29 |  102 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sMsg.zString);` |
|     14 |  103 | `	}` |
|     29 |  104 | `	if( pErr->sFile.zString ){` |
|    ! 0 |  105 | `		SyMemBackendFree(&pVm->sAllocator,(void *)pErr->sFile.zString);` |
|    ! 0 |  106 | `	}` |
|     29 |  107 | `	SyStringInitFromBuf(&pErr->sMsg,0,0);` |
|     29 |  108 | `	SyStringInitFromBuf(&pErr->sFile,0,0);` |
|     29 |  109 | `}` |
|      - |  110 | `/*` |
|      - |  111 | ` * Empty the libxml error queue, releasing the copied strings.  The` |
|      - |  112 | ` * last-error slot is kept (php parity: use_internal_errors(false) drops` |
|      - |  113 | ` * the buffer but libxml_get_last_error still reports).` |
|      - |  114 | ` */` |
|   4130 |  115 | `static void LibxmlClearQueue(ph7_vm *pVm)` |
|      5 |  116 | `{` |
|   4135 |  117 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  118 | `	sxu32 n;` |
|   4149 |  119 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     15 |  120 | `		LibxmlFreeErr(pVm,&aErr[n]);` |
|      8 |  121 | `	}` |
|   4135 |  122 | `	SySetReset(&pVm->aLibxmlErr);` |
|   4135 |  123 | `}` |
|      - |  124 | `/*` |
|      - |  125 | ` * libxml_clear_errors(): drop the queue AND the last-error slot.` |
|      - |  126 | ` */` |
|   4104 |  127 | `PH7_PRIVATE void PH7_LibxmlClearErrors(ph7_vm *pVm)` |
|      5 |  128 | `{` |
|   4109 |  129 | `	LibxmlClearQueue(pVm);` |
|   4109 |  130 | `	if( pVm->pLibxmlLastErr ){` |
|     11 |  131 | `		LibxmlFreeErr(pVm,(phl_libxml_err *)pVm->pLibxmlLastErr);` |
|     11 |  132 | `		SyMemBackendFree(&pVm->sAllocator,pVm->pLibxmlLastErr);` |
|     11 |  133 | `		pVm->pLibxmlLastErr = 0;` |
|      5 |  134 | `	}` |
|   4109 |  135 | `}` |
|      - |  136 | `/*` |
|      - |  137 | ` * Push one error onto the per-VM queue and last-error slot, copying the` |
|      - |  138 | ` * message/file strings.  The typed structured-error callback below and the` |
|      - |  139 | ` * DOM schema hooks (vm_dom.c) both funnel through this, keeping the` |
|      - |  140 | ` * queue-building logic in one place and ph7int.h free of libxml types.` |
|      - |  141 | ` */` |
|     14 |  142 | `PH7_PRIVATE void PH7_LibxmlQueueError(ph7_vm *pVm,int iLevel,int iCode,int iLine,int iColumn,` |
|      - |  143 | `	const char *zMsg,const char *zFile)` |
|      1 |  144 | `{` |
|      - |  145 | `	phl_libxml_err sEntry;` |
|      - |  146 | `	phl_libxml_err *pLast;` |
|     15 |  147 | `	if( pVm == 0 ){` |
|    ! 0 |  148 | `		return;` |
|      - |  149 | `	}` |
|     15 |  150 | `	SyZero(&sEntry,sizeof(sEntry));` |
|     15 |  151 | `	sEntry.iLevel = iLevel;` |
|     15 |  152 | `	sEntry.iCode = iCode;` |
|     15 |  153 | `	sEntry.iLine = iLine;` |
|     15 |  154 | `	sEntry.iColumn = iColumn;` |
|     15 |  155 | `	if( zMsg ){` |
|     15 |  156 | `		sxu32 nMsg = SyStrlen(zMsg);` |
|     15 |  157 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zMsg,nMsg);` |
|     15 |  158 | `		if( zDup ){` |
|      - |  159 | `			/* Trailing newline kept: php's LibXMLError->message preserves it */` |
|     15 |  160 | `			SyStringInitFromBuf(&sEntry.sMsg,zDup,nMsg);` |
|      7 |  161 | `		}` |
|      7 |  162 | `	}` |
|     15 |  163 | `	if( zFile ){` |
|    ! 0 |  164 | `		sxu32 nFile = SyStrlen(zFile);` |
|    ! 0 |  165 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,zFile,nFile);` |
|    ! 0 |  166 | `		if( zDup ){` |
|    ! 0 |  167 | `			SyStringInitFromBuf(&sEntry.sFile,zDup,nFile);` |
|    ! 0 |  168 | `		}` |
|    ! 0 |  169 | `	}` |
|     15 |  170 | `	SySetPut(&pVm->aLibxmlErr,(const void *)&sEntry);` |
|      - |  171 | `	/* Mirror into the last-error slot (independent string copies so queue` |
|      - |  172 | `	 * draining cannot invalidate it). */` |
|     15 |  173 | `	pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|     15 |  174 | `	if( pLast == 0 ){` |
|     11 |  175 | `		pLast = (phl_libxml_err *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_libxml_err));` |
|     11 |  176 | `		if( pLast == 0 ){` |
|    ! 0 |  177 | `			return;` |
|      - |  178 | `		}` |
|     11 |  179 | `		SyZero(pLast,sizeof(phl_libxml_err));` |
|     11 |  180 | `		pVm->pLibxmlLastErr = (void *)pLast;` |
|      6 |  181 | `	}else{` |
|      5 |  182 | `		LibxmlFreeErr(pVm,pLast);` |
|      - |  183 | `	}` |
|     15 |  184 | `	pLast->iLevel = iLevel;` |
|     15 |  185 | `	pLast->iCode = iCode;` |
|     15 |  186 | `	pLast->iLine = iLine;` |
|     15 |  187 | `	pLast->iColumn = iColumn;` |
|     15 |  188 | `	if( sEntry.sMsg.zString ){` |
|     15 |  189 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sMsg.zString,sEntry.sMsg.nByte);` |
|     15 |  190 | `		if( zDup ){` |
|     15 |  191 | `			SyStringInitFromBuf(&pLast->sMsg,zDup,sEntry.sMsg.nByte);` |
|      7 |  192 | `		}` |
|      7 |  193 | `	}` |
|     15 |  194 | `	if( sEntry.sFile.zString ){` |
|    ! 0 |  195 | `		char *zDup = SyMemBackendStrDup(&pVm->sAllocator,sEntry.sFile.zString,sEntry.sFile.nByte);` |
|    ! 0 |  196 | `		if( zDup ){` |
|    ! 0 |  197 | `			SyStringInitFromBuf(&pLast->sFile,zDup,sEntry.sFile.nByte);` |
|    ! 0 |  198 | `		}` |
|    ! 0 |  199 | `	}` |
|      8 |  200 | `}` |
|      - |  201 | `/*` |
|      - |  202 | ` * Structured-error callback installed while a libxml2 entry point runs` |
|      - |  203 | ` * between PH7_LibxmlCaptureBegin/End.  Forwards to PH7_LibxmlQueueError;` |
|      - |  204 | ` * the capture-end decides whether entries stay queued or drain as` |
|      - |  205 | ` * php-style warnings.` |
|      - |  206 | ` */` |
|      - |  207 | `#if LIBXML_VERSION >= 21200` |
|      5 |  208 | `static void LibxmlStructuredErr(void *pUserData,const xmlError *pErr)` |
|      - |  209 | `#else` |
|      5 |  210 | `static void LibxmlStructuredErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  211 | `#endif` |
|      1 |  212 | `{` |
|     11 |  213 | `	if( pErr == 0 ){` |
|    ! 0 |  214 | `		return;` |
|      - |  215 | `	}` |
|      - |  216 | `	/* libxml keeps the column in int2 */` |
|     16 |  217 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|     10 |  218 | `		pErr->int2,pErr->message,pErr->file);` |
|      6 |  219 | `}` |
|      - |  220 | `/*` |
|      - |  221 | ` * Bracket a libxml2 entry point.  Begin installs the structured handler` |
|      - |  222 | ` * routed at this VM and returns the current queue depth; End restores the` |
|      - |  223 | ` * default handler and, when libxml_use_internal_errors() is OFF, drains` |
|      - |  224 | ` * every entry recorded since the mark as php-style warnings:` |
|      - |  225 | ` *   funcname(): <message> in <Entity\|file>, line: <n>` |
|      - |  226 | ` * (php's exact wording for the memory-parser case).` |
|      - |  227 | ` */` |
|    156 |  228 | `PH7_PRIVATE sxu32 PH7_LibxmlCaptureBegin(ph7_vm *pVm)` |
|      1 |  229 | `{` |
|    157 |  230 | `	xmlSetStructuredErrorFunc(pVm,LibxmlStructuredErr);` |
|    157 |  231 | `	return SySetUsed(&pVm->aLibxmlErr);` |
|      1 |  232 | `}` |
|    156 |  233 | `PH7_PRIVATE void PH7_LibxmlCaptureEnd(ph7_vm *pVm,sxu32 nMark,const char *zFnName)` |
|      1 |  234 | `{` |
|    157 |  235 | `	xmlSetStructuredErrorFunc(0,0);` |
|    157 |  236 | `	if( pVm->bLibxmlInternalErr ){` |
|      - |  237 | `		/* Internal capture on: entries stay queued for libxml_get_errors() */` |
|     13 |  238 | `		return;` |
|      - |  239 | `	}` |
|    145 |  240 | `	if( SySetUsed(&pVm->aLibxmlErr) > nMark ){` |
|    ! 0 |  241 | `		phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|      - |  242 | `		sxu32 n;` |
|    ! 0 |  243 | `		for( n = nMark ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|      - |  244 | `			SyString sFunc;` |
|      - |  245 | `			SyBlob sMsg;` |
|    ! 0 |  246 | `			sxu32 nTrim = aErr[n].sMsg.nByte;` |
|      - |  247 | `			/* php trims the trailing newline off the warning copy */` |
|    ! 0 |  248 | `			while( nTrim > 0 && (aErr[n].sMsg.zString[nTrim-1] == '\n' \|\| aErr[n].sMsg.zString[nTrim-1] == '\r') ){` |
|    ! 0 |  249 | `				nTrim--;` |
|    ! 0 |  250 | `			}` |
|    ! 0 |  251 | `			SyBlobInit(&sMsg,&pVm->sAllocator);` |
|    ! 0 |  252 | `			SyBlobAppend(&sMsg,aErr[n].sMsg.zString,nTrim);` |
|      - |  253 | `			/* php appends the source location only for parser errors that` |
|      - |  254 | `			 * carry a real line; generic libxml errors print bare. */` |
|    ! 0 |  255 | `			if( aErr[n].iLine > 0 ){` |
|    ! 0 |  256 | `				if( aErr[n].sFile.nByte > 0 ){` |
|    ! 0 |  257 | `					SyBlobFormat(&sMsg," in %z, line: %d",&aErr[n].sFile,aErr[n].iLine);` |
|    ! 0 |  258 | `				}else{` |
|    ! 0 |  259 | `					SyBlobFormat(&sMsg," in Entity, line: %d",aErr[n].iLine);` |
|      - |  260 | `				}` |
|    ! 0 |  261 | `			}` |
|    ! 0 |  262 | `			SyBlobAppend(&sMsg,"\0",1);` |
|    ! 0 |  263 | `			SyStringInitFromBuf(&sFunc,zFnName,SyStrlen(zFnName));` |
|    ! 0 |  264 | `			PH7_VmThrowError(&(*pVm),zFnName ? &sFunc : 0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    ! 0 |  265 | `			SyBlobRelease(&sMsg);` |
|    ! 0 |  266 | `			LibxmlFreeErr(pVm,&aErr[n]);` |
|    ! 0 |  267 | `		}` |
|    ! 0 |  268 | `		SySetTruncate(&pVm->aLibxmlErr,nMark);` |
|    ! 0 |  269 | `	}` |
|     79 |  270 | `}` |
|      - |  271 | `/*` |
|      - |  272 | ` * Allocate and register a new document shell on the per-VM registry.` |
|      - |  273 | ` * Returns NULL on allocation failure (the caller reports OOM).` |
|      - |  274 | ` */` |
|    112 |  275 | `PH7_PRIVATE phl_xmldoc * PH7_LibxmlNewDoc(ph7_vm *pVm,void *pXmlDocPtr)` |
|      1 |  276 | `{` |
|      - |  277 | `	phl_xmldoc *pDoc;` |
|    113 |  278 | `	pDoc = (phl_xmldoc *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmldoc));` |
|    113 |  279 | `	if( pDoc == 0 ){` |
|    ! 0 |  280 | `		return 0;` |
|      - |  281 | `	}` |
|    113 |  282 | `	SyZero(pDoc,sizeof(phl_xmldoc));` |
|    113 |  283 | `	SySetInit(&pDoc->aOrphans,&pVm->sAllocator,sizeof(void *));` |
|    113 |  284 | `	pDoc->pDoc = pXmlDocPtr;` |
|    113 |  285 | `	pDoc->pVm = &(*pVm);` |
|    113 |  286 | `	pDoc->bPreserveWS = 1;  /* DOMDocument->preserveWhiteSpace default */` |
|    113 |  287 | `	pDoc->bFormatOutput = 0;` |
|    113 |  288 | `	pDoc->pNext = (phl_xmldoc *)pVm->pXmlDocs;` |
|    113 |  289 | `	pVm->pXmlDocs = (void *)pDoc;` |
|    113 |  290 | `	return pDoc;` |
|     57 |  291 | `}` |
|      - |  292 |  |
|      - |  293 | `/* ===== Constants (php ext/libxml + ext/dom node types) ===== */` |
|      - |  294 |  |
|      - |  295 | `#define LIBXML_INT_CONST(FN,VALUE) \` |
|      - |  296 | `	static void FN(ph7_value *pVal,void *pUnused){ \` |
|      - |  297 | `		SXUNUSED(pUnused); \` |
|      - |  298 | `		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \` |
|      - |  299 | `	}` |
|    ! 0 |  300 | `LIBXML_INT_CONST(LibxmlConst_VERSION,        LIBXML_VERSION)` |
|    ! 0 |  301 | `LIBXML_INT_CONST(LibxmlConst_NOENT,          XML_PARSE_NOENT)` |
|    ! 0 |  302 | `LIBXML_INT_CONST(LibxmlConst_DTDLOAD,        XML_PARSE_DTDLOAD)` |
|    ! 0 |  303 | `LIBXML_INT_CONST(LibxmlConst_DTDATTR,        XML_PARSE_DTDATTR)` |
|    ! 0 |  304 | `LIBXML_INT_CONST(LibxmlConst_DTDVALID,       XML_PARSE_DTDVALID)` |
|    ! 0 |  305 | `LIBXML_INT_CONST(LibxmlConst_NOERROR,        XML_PARSE_NOERROR)` |
|    ! 0 |  306 | `LIBXML_INT_CONST(LibxmlConst_NOWARNING,      XML_PARSE_NOWARNING)` |
|    ! 0 |  307 | `LIBXML_INT_CONST(LibxmlConst_NOBLANKS,       XML_PARSE_NOBLANKS)` |
|    ! 0 |  308 | `LIBXML_INT_CONST(LibxmlConst_XINCLUDE,       XML_PARSE_XINCLUDE)` |
|    ! 0 |  309 | `LIBXML_INT_CONST(LibxmlConst_NSCLEAN,        XML_PARSE_NSCLEAN)` |
|    ! 0 |  310 | `LIBXML_INT_CONST(LibxmlConst_NOCDATA,        XML_PARSE_NOCDATA)` |
|    ! 0 |  311 | `LIBXML_INT_CONST(LibxmlConst_NONET,          XML_PARSE_NONET)` |
|    ! 0 |  312 | `LIBXML_INT_CONST(LibxmlConst_PEDANTIC,       XML_PARSE_PEDANTIC)` |
|    ! 0 |  313 | `LIBXML_INT_CONST(LibxmlConst_COMPACT,        XML_PARSE_COMPACT)` |
|    ! 0 |  314 | `LIBXML_INT_CONST(LibxmlConst_PARSEHUGE,      XML_PARSE_HUGE)` |
|    ! 0 |  315 | `LIBXML_INT_CONST(LibxmlConst_BIGLINES,       XML_PARSE_BIG_LINES)` |
|    ! 0 |  316 | `LIBXML_INT_CONST(LibxmlConst_NOXMLDECL,      2)      /* XML_SAVE_NO_DECL */` |
|    ! 0 |  317 | `LIBXML_INT_CONST(LibxmlConst_NOEMPTYTAG,     4)      /* XML_SAVE_NO_EMPTY */` |
|    ! 0 |  318 | `LIBXML_INT_CONST(LibxmlConst_SCHEMA_CREATE,  1)      /* XML_SCHEMA_VAL_VC_I_CREATE */` |
|    ! 0 |  319 | `LIBXML_INT_CONST(LibxmlConst_ERR_NONE,       XML_ERR_NONE)` |
|    ! 0 |  320 | `LIBXML_INT_CONST(LibxmlConst_ERR_WARNING,    XML_ERR_WARNING)` |
|    ! 0 |  321 | `LIBXML_INT_CONST(LibxmlConst_ERR_ERROR,      XML_ERR_ERROR)` |
|      3 |  322 | `LIBXML_INT_CONST(LibxmlConst_ERR_FATAL,      XML_ERR_FATAL)` |
|      - |  323 | `/* ext/dom node-type constants (values fixed by the DOM spec / libxml enums) */` |
|    ! 0 |  324 | `LIBXML_INT_CONST(LibxmlConst_ELEMENT_NODE,        XML_ELEMENT_NODE)` |
|    ! 0 |  325 | `LIBXML_INT_CONST(LibxmlConst_ATTRIBUTE_NODE,      XML_ATTRIBUTE_NODE)` |
|    ! 0 |  326 | `LIBXML_INT_CONST(LibxmlConst_TEXT_NODE,           XML_TEXT_NODE)` |
|    ! 0 |  327 | `LIBXML_INT_CONST(LibxmlConst_CDATA_SECTION_NODE,  XML_CDATA_SECTION_NODE)` |
|    ! 0 |  328 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_REF_NODE,     XML_ENTITY_REF_NODE)` |
|    ! 0 |  329 | `LIBXML_INT_CONST(LibxmlConst_ENTITY_NODE,         XML_ENTITY_NODE)` |
|    ! 0 |  330 | `LIBXML_INT_CONST(LibxmlConst_PI_NODE,             XML_PI_NODE)` |
|    ! 0 |  331 | `LIBXML_INT_CONST(LibxmlConst_COMMENT_NODE,        XML_COMMENT_NODE)` |
|    ! 0 |  332 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_NODE,       XML_DOCUMENT_NODE)` |
|    ! 0 |  333 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_TYPE_NODE,  XML_DOCUMENT_TYPE_NODE)` |
|    ! 0 |  334 | `LIBXML_INT_CONST(LibxmlConst_DOCUMENT_FRAG_NODE,  XML_DOCUMENT_FRAG_NODE)` |
|    ! 0 |  335 | `LIBXML_INT_CONST(LibxmlConst_NOTATION_NODE,       XML_NOTATION_NODE)` |
|    ! 0 |  336 | `LIBXML_INT_CONST(LibxmlConst_HTML_DOCUMENT_NODE,  XML_HTML_DOCUMENT_NODE)` |
|    ! 0 |  337 | `LIBXML_INT_CONST(LibxmlConst_DTD_NODE,            XML_DTD_NODE)` |
|      - |  338 |  |
|    ! 0 |  339 | `static void LibxmlConst_DOTTED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  340 | `{` |
|    ! 0 |  341 | `	SXUNUSED(pUnused);` |
|    ! 0 |  342 | `	ph7_value_string(pVal,LIBXML_DOTTED_VERSION,-1);` |
|    ! 0 |  343 | `}` |
|    ! 0 |  344 | `static void LibxmlConst_LOADED_VERSION(ph7_value *pVal,void *pUnused)` |
|    ! 0 |  345 | `{` |
|    ! 0 |  346 | `	SXUNUSED(pUnused);` |
|      - |  347 | `	/* php exposes the runtime-loaded version string here */` |
|    ! 0 |  348 | `	ph7_value_string(pVal,(const char *)xmlParserVersion,-1);` |
|    ! 0 |  349 | `}` |
|      - |  350 |  |
|   4076 |  351 | `PH7_PRIVATE void PH7_RegisterLibxmlConstants(ph7_vm *pVm)` |
|      5 |  352 | `{` |
|      - |  353 | `	static const struct {` |
|      - |  354 | `		const char *zName;` |
|      - |  355 | `		void (*xExpand)(ph7_value *,void *);` |
|      - |  356 | `	} aConst[] = {` |
|      - |  357 | `		{ "LIBXML_VERSION",        LibxmlConst_VERSION        },` |
|      - |  358 | `		{ "LIBXML_DOTTED_VERSION", LibxmlConst_DOTTED_VERSION },` |
|      - |  359 | `		{ "LIBXML_LOADED_VERSION", LibxmlConst_LOADED_VERSION },` |
|      - |  360 | `		{ "LIBXML_NOENT",          LibxmlConst_NOENT          },` |
|      - |  361 | `		{ "LIBXML_DTDLOAD",        LibxmlConst_DTDLOAD        },` |
|      - |  362 | `		{ "LIBXML_DTDATTR",        LibxmlConst_DTDATTR        },` |
|      - |  363 | `		{ "LIBXML_DTDVALID",       LibxmlConst_DTDVALID       },` |
|      - |  364 | `		{ "LIBXML_NOERROR",        LibxmlConst_NOERROR        },` |
|      - |  365 | `		{ "LIBXML_NOWARNING",      LibxmlConst_NOWARNING      },` |
|      - |  366 | `		{ "LIBXML_NOBLANKS",       LibxmlConst_NOBLANKS       },` |
|      - |  367 | `		{ "LIBXML_XINCLUDE",       LibxmlConst_XINCLUDE       },` |
|      - |  368 | `		{ "LIBXML_NSCLEAN",        LibxmlConst_NSCLEAN        },` |
|      - |  369 | `		{ "LIBXML_NOCDATA",        LibxmlConst_NOCDATA        },` |
|      - |  370 | `		{ "LIBXML_NONET",          LibxmlConst_NONET          },` |
|      - |  371 | `		{ "LIBXML_PEDANTIC",       LibxmlConst_PEDANTIC       },` |
|      - |  372 | `		{ "LIBXML_COMPACT",        LibxmlConst_COMPACT        },` |
|      - |  373 | `		{ "LIBXML_PARSEHUGE",      LibxmlConst_PARSEHUGE      },` |
|      - |  374 | `		{ "LIBXML_BIGLINES",       LibxmlConst_BIGLINES       },` |
|      - |  375 | `		{ "LIBXML_NOXMLDECL",      LibxmlConst_NOXMLDECL      },` |
|      - |  376 | `		{ "LIBXML_NOEMPTYTAG",     LibxmlConst_NOEMPTYTAG     },` |
|      - |  377 | `		{ "LIBXML_SCHEMA_CREATE",  LibxmlConst_SCHEMA_CREATE  },` |
|      - |  378 | `		{ "LIBXML_ERR_NONE",       LibxmlConst_ERR_NONE       },` |
|      - |  379 | `		{ "LIBXML_ERR_WARNING",    LibxmlConst_ERR_WARNING    },` |
|      - |  380 | `		{ "LIBXML_ERR_ERROR",      LibxmlConst_ERR_ERROR      },` |
|      - |  381 | `		{ "LIBXML_ERR_FATAL",      LibxmlConst_ERR_FATAL      },` |
|      - |  382 | `		{ "XML_ELEMENT_NODE",       LibxmlConst_ELEMENT_NODE       },` |
|      - |  383 | `		{ "XML_ATTRIBUTE_NODE",     LibxmlConst_ATTRIBUTE_NODE     },` |
|      - |  384 | `		{ "XML_TEXT_NODE",          LibxmlConst_TEXT_NODE          },` |
|      - |  385 | `		{ "XML_CDATA_SECTION_NODE", LibxmlConst_CDATA_SECTION_NODE },` |
|      - |  386 | `		{ "XML_ENTITY_REF_NODE",    LibxmlConst_ENTITY_REF_NODE    },` |
|      - |  387 | `		{ "XML_ENTITY_NODE",        LibxmlConst_ENTITY_NODE        },` |
|      - |  388 | `		{ "XML_PI_NODE",            LibxmlConst_PI_NODE            },` |
|      - |  389 | `		{ "XML_COMMENT_NODE",       LibxmlConst_COMMENT_NODE       },` |
|      - |  390 | `		{ "XML_DOCUMENT_NODE",      LibxmlConst_DOCUMENT_NODE      },` |
|      - |  391 | `		{ "XML_DOCUMENT_TYPE_NODE", LibxmlConst_DOCUMENT_TYPE_NODE },` |
|      - |  392 | `		{ "XML_DOCUMENT_FRAG_NODE", LibxmlConst_DOCUMENT_FRAG_NODE },` |
|      - |  393 | `		{ "XML_NOTATION_NODE",      LibxmlConst_NOTATION_NODE      },` |
|      - |  394 | `		{ "XML_HTML_DOCUMENT_NODE", LibxmlConst_HTML_DOCUMENT_NODE },` |
|      - |  395 | `		{ "XML_DTD_NODE",           LibxmlConst_DTD_NODE           },` |
|      - |  396 | `	};` |
|      - |  397 | `	sxu32 n;` |
| 163045 |  398 | `	for( n = 0 ; n < sizeof(aConst)/sizeof(aConst[0]) ; n++ ){` |
| 158969 |  399 | `		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);` |
|  79487 |  400 | `	}` |
|   4081 |  401 | `}` |
|      - |  402 | `/* ===== libxml_* native thunks ===== */` |
|      - |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * bool __libxml_use_internal_errors(?bool $use_errors = null)` |
|      - |  406 | ` *  Flip (or just report, on null/omitted) the internal-capture flag and` |
|      - |  407 | ` *  return the PREVIOUS state -- php semantics.` |
|      - |  408 | ` */` |
|     44 |  409 | `static int vm_builtin_libxml_use_internal_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  410 | `{` |
|     45 |  411 | `	ph7_vm *pVm = pCtx->pVm;` |
|     45 |  412 | `	int bPrev = pVm->bLibxmlInternalErr;` |
|     45 |  413 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     39 |  414 | `		pVm->bLibxmlInternalErr = ph7_value_to_bool(apArg[0]) ? 1 : 0;` |
|     39 |  415 | `		if( !pVm->bLibxmlInternalErr ){` |
|      - |  416 | `			/* php frees the accumulated buffer when capture turns OFF */` |
|     27 |  417 | `			LibxmlClearQueue(pVm);` |
|     13 |  418 | `		}` |
|     19 |  419 | `	}` |
|     45 |  420 | `	ph7_result_bool(pCtx,bPrev);` |
|     45 |  421 | `	return PH7_OK;` |
|      1 |  422 | `}` |
|      - |  423 | `/*` |
|      - |  424 | ` * array __libxml_get_errors_raw()` |
|      - |  425 | ` *  The queued errors as raw assoc arrays, oldest first.` |
|      - |  426 | ` */` |
|      - |  427 | `/*` |
|      - |  428 | ` * Build one LibXMLError from a recorded error.` |
|      - |  429 | ` *` |
|      - |  430 | `` * The prelude did this in PHP (`__phl_libxml_err_obj()` copying six array keys`` |
|      - |  431 | `` * onto a `new LibXMLError`), over an array these accessors returned purely so`` |
|      - |  432 | ` * that PHP could reshape it. Both the array hop and the helper are gone: the` |
|      - |  433 | ` * accessors below now answer the objects php answers.` |
|      - |  434 | ` */` |
|     12 |  435 | `static ph7_class_instance * LibxmlErrObject(ph7_context *pCtx,phl_libxml_err *pErr)` |
|      1 |  436 | `{` |
|     13 |  437 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  438 | `	ph7_class *pClass = PH7_VmExtractClass(pVm,"LibXMLError",sizeof("LibXMLError")-1,0,0);` |
|      - |  439 | `	ph7_class_instance *pObj;` |
|      - |  440 | `	ph7_value sVal;` |
|      - |  441 | `	SyString sStr;` |
|     13 |  442 | `	if( pClass == 0 ){` |
|    ! 0 |  443 | `		return 0;` |
|      - |  444 | `	}` |
|     13 |  445 | `	pObj = PH7_NewClassInstance(pVm,pClass);` |
|     13 |  446 | `	if( pObj == 0 ){` |
|    ! 0 |  447 | `		return 0;` |
|      - |  448 | `	}` |
|     13 |  449 | `	PH7_MemObjInit(pVm,&sVal);` |
|     13 |  450 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLevel);` |
|     13 |  451 | `	PH7_NativeSetProp(pVm,pObj,"level",sizeof("level")-1,&sVal);` |
|     13 |  452 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iCode);` |
|     13 |  453 | `	PH7_NativeSetProp(pVm,pObj,"code",sizeof("code")-1,&sVal);` |
|     13 |  454 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iColumn);` |
|     13 |  455 | `	PH7_NativeSetProp(pVm,pObj,"column",sizeof("column")-1,&sVal);` |
|     13 |  456 | `	PH7_MemObjInitFromInt(pVm,&sVal,pErr->iLine);` |
|     13 |  457 | `	PH7_NativeSetProp(pVm,pObj,"line",sizeof("line")-1,&sVal);` |
|     13 |  458 | `	SyStringInitFromBuf(&sStr,pErr->sMsg.zString ? pErr->sMsg.zString : "",pErr->sMsg.nByte);` |
|     13 |  459 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     13 |  460 | `	PH7_NativeSetProp(pVm,pObj,"message",sizeof("message")-1,&sVal);` |
|     13 |  461 | `	SyStringInitFromBuf(&sStr,pErr->sFile.zString ? pErr->sFile.zString : "",pErr->sFile.nByte);` |
|     13 |  462 | `	PH7_MemObjInitFromString(pVm,&sVal,&sStr);` |
|     13 |  463 | `	PH7_NativeSetProp(pVm,pObj,"file",sizeof("file")-1,&sVal);` |
|     13 |  464 | `	PH7_MemObjRelease(&sVal);` |
|     13 |  465 | `	return pObj;` |
|      7 |  466 | `}` |
|      - |  467 | `/* array libxml_get_errors() */` |
|     10 |  468 | `static int vm_builtin_libxml_get_errors_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  469 | `{` |
|     11 |  470 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  471 | `	phl_libxml_err *aErr = (phl_libxml_err *)SySetBasePtr(&pVm->aLibxmlErr);` |
|     11 |  472 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|     11 |  473 | `	ph7_value *pWorker = ph7_context_new_scalar(pCtx);` |
|      - |  474 | `	sxu32 n;` |
|      5 |  475 | `	SXUNUSED(nArg);` |
|      5 |  476 | `	SXUNUSED(apArg);` |
|     11 |  477 | `	if( pList == 0 \|\| pWorker == 0 ){` |
|    ! 0 |  478 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  479 | `		ph7_result_null(pCtx);` |
|    ! 0 |  480 | `		return PH7_OK;` |
|      - |  481 | `	}` |
|     21 |  482 | `	for( n = 0 ; n < SySetUsed(&pVm->aLibxmlErr) ; ++n ){` |
|     11 |  483 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,&aErr[n]);` |
|      - |  484 | `		ph7_value sEntry;` |
|     11 |  485 | `		if( pObj == 0 ){` |
|    ! 0 |  486 | `			break;` |
|      - |  487 | `		}` |
|      - |  488 | `		/* The list takes over the instance's initial iRef=1: ph7_array_add_elem` |
|      - |  489 | `		 * copies the slot (taking its own reference), so the local one is dropped. */` |
|     11 |  490 | `		PH7_MemObjInit(pVm,&sEntry);` |
|     11 |  491 | `		sEntry.x.pOther = pObj;` |
|     11 |  492 | `		sEntry.iFlags = MEMOBJ_OBJ;` |
|     11 |  493 | `		ph7_array_add_elem(pList,0,&sEntry);` |
|     11 |  494 | `		PH7_ClassInstanceUnref(pObj);` |
|      6 |  495 | `	}` |
|     11 |  496 | `	ph7_result_value(pCtx,pList);` |
|     11 |  497 | `	return PH7_OK;` |
|      6 |  498 | `}` |
|      - |  499 | `/*` |
|      - |  500 | ` * void __libxml_clear_errors()` |
|      - |  501 | ` */` |
|     24 |  502 | `static int vm_builtin_libxml_clear_errors(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  503 | `{` |
|     12 |  504 | `	SXUNUSED(nArg);` |
|     12 |  505 | `	SXUNUSED(apArg);` |
|     25 |  506 | `	PH7_LibxmlClearErrors(pCtx->pVm);` |
|     25 |  507 | `	ph7_result_null(pCtx);` |
|     25 |  508 | `	return PH7_OK;` |
|      1 |  509 | `}` |
|      - |  510 | `/*` |
|      - |  511 | ` * array\|false __libxml_get_last_error_raw()` |
|      - |  512 | ` */` |
|      4 |  513 | `static int vm_builtin_libxml_get_last_error_raw(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  514 | `{` |
|      5 |  515 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 |  516 | `	phl_libxml_err *pLast = (phl_libxml_err *)pVm->pLibxmlLastErr;` |
|      2 |  517 | `	SXUNUSED(nArg);` |
|      2 |  518 | `	SXUNUSED(apArg);` |
|      5 |  519 | `	if( pLast == 0 ){` |
|      3 |  520 | `		ph7_result_bool(pCtx,0);` |
|      3 |  521 | `		return PH7_OK;` |
|      - |  522 | `	}` |
|      - |  523 | `	{` |
|      3 |  524 | `		ph7_class_instance *pObj = LibxmlErrObject(pCtx,pLast);` |
|      - |  525 | `		ph7_value sRes;` |
|      3 |  526 | `		if( pObj == 0 ){` |
|    ! 0 |  527 | `			ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  528 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  529 | `			return PH7_OK;` |
|      - |  530 | `		}` |
|      3 |  531 | `		PH7_MemObjInit(pVm,&sRes);` |
|      3 |  532 | `		sRes.x.pOther = pObj;` |
|      3 |  533 | `		sRes.iFlags = MEMOBJ_OBJ;` |
|      3 |  534 | `		ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|      3 |  535 | `		PH7_ClassInstanceUnref(pObj);` |
|      - |  536 | `	}` |
|      3 |  537 | `	return PH7_OK;` |
|      3 |  538 | `}` |
|      - |  539 |  |
|      - |  540 | `/*` |
|      - |  541 | ` * The libxml PHP-visible surface: the LibXMLError class plus the four` |
|      - |  542 | ` * libxml_* functions, delegating to the __libxml_* thunks above.` |
|      - |  543 | ` */` |
|      - |  544 | `/* LibXMLError is declared from C below, and the four libxml_* functions ARE the` |
|      - |  545 | ` * C routines -- they used to be PHP wrappers over __libxml_* thunks, with a PHP` |
|      - |  546 | ` * helper reshaping an array into the object. */` |
|      - |  547 |  |
|      - |  548 | `/*` |
|      - |  549 | ` * Install the shared libxml layer: one-time global init, the per-VM state` |
|      - |  550 | ` * the DOM/XMLWriter surfaces build on, the __libxml_* thunks and the` |
|      - |  551 | ` * PHP-visible libxml_* functions + LibXMLError class.  Called from` |
|      - |  552 | ` * PH7_VmInit inside the bCompilingBuiltin window.` |
|      - |  553 | ` */` |
|   4670 |  554 | `PH7_PRIVATE sxi32 PH7_VmInstallLibxml(ph7_vm *pVm)` |
|      5 |  555 | `{` |
|      - |  556 | `	/* Registered under the names php exposes. There is no thunk and no PHP` |
|      - |  557 | `	 * wrapper any more: these are the functions, so they are internal for` |
|      - |  558 | `	 * reflection and get the arity enforcement a prelude wrapper never had. */` |
|      - |  559 | `	static const struct {` |
|      - |  560 | `		const char *zName;` |
|      - |  561 | `		ProchHostFunction xFunc;` |
|      - |  562 | `	} aFunc[] = {` |
|      - |  563 | `		{ "libxml_use_internal_errors", vm_builtin_libxml_use_internal_errors },` |
|      - |  564 | `		{ "libxml_get_errors",          vm_builtin_libxml_get_errors_raw      },` |
|      - |  565 | `		{ "libxml_clear_errors",        vm_builtin_libxml_clear_errors        },` |
|      - |  566 | `		{ "libxml_get_last_error",      vm_builtin_libxml_get_last_error_raw  },` |
|      - |  567 | `	};` |
|      - |  568 | `	/* Plain data carrier; php declares no methods on it. */` |
|      - |  569 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  570 | `		{ "level",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  571 | `		{ "code",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  572 | `		{ "column",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  573 | `		{ "message", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  574 | `		{ "file",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  575 | `		{ "line",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "int" },` |
|      - |  576 | `	};` |
|      - |  577 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - |  578 | `		"LibXMLError", 0, 0, 0, 0, 0, 0, 0, aProp, SX_ARRAYSIZE(aProp), 0, 0, 0` |
|      - |  579 | `	};` |
|      - |  580 | `	sxu32 n;` |
|   4675 |  581 | `	LibxmlGlobalInit();` |
|   4675 |  582 | `	SySetInit(&pVm->aLibxmlErr,&pVm->sAllocator,sizeof(phl_libxml_err));` |
|   4675 |  583 | `	pVm->bLibxmlInternalErr = 0;` |
|   4675 |  584 | `	pVm->pLibxmlLastErr = 0;` |
|   4675 |  585 | `	pVm->pXmlDocs = 0;` |
|   4675 |  586 | `	pVm->pXmlWriters = 0;` |
|  23355 |  587 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
|  18685 |  588 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|   9345 |  589 | `	}` |
|      - |  590 | `	/* The class must exist before the accessors can build one. */` |
|   4675 |  591 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 |  592 | `}` |
|      - |  593 |  |
|      - |  594 | `#else` |
|      - |  595 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - |  596 | `typedef int vm_libxml_unused;` |
|      - |  597 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - |  598 |  |
