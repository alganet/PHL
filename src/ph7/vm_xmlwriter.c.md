# src/ph7/vm_xmlwriter.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 718/802 lines (89.53%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/tree.h>` |
|      - |    8 | `#include <libxml/xmlerror.h>` |
|      - |    9 | `#include <libxml/xmlwriter.h>` |
|      - |   10 |  |
|      - |   11 | `/*` |
|      - |   12 | ` * ext/xmlwriter on libxml2: __xw_* native thunks + the XMLWriter class` |
|      - |   13 | ` * prelude.  Architecture notes live in vm_libxml.c (shared registries,` |
|      - |   14 | ` * error queue, lifetime model).` |
|      - |   15 | ` *` |
|      - |   16 | ` * An XMLWriter object holds a phl_xmlwriter resource {xmlTextWriterPtr,` |
|      - |   17 | ` * xmlBufferPtr} in $__res.  In-memory writers (openMemory) own an` |
|      - |   18 | ` * xmlBuffer; outputMemory reads it back.  A writer opened on a URI or` |
|      - |   19 | ` * attached to a stream carries an io_private instead and pushes its bytes` |
|      - |   20 | ` * through PH7_StreamWrite, so every wrapper this engine has -- php://,` |
|      - |   21 | ` * data://, a userland streamWrapper -- is a destination, as in php.` |
|      - |   22 | ` * Every writer is chained on the per-VM registry (pVm->pXmlWriters) and` |
|      - |   23 | ` * freed at VM reset/release since PH7 resources have no destructor hook.` |
|      - |   24 | ` */` |
|      - |   25 |  |
|      - |   26 | `typedef struct phl_xmlwriter phl_xmlwriter;` |
|      - |   27 | `struct phl_xmlwriter {` |
|      - |   28 | `	xmlTextWriterPtr pWriter;` |
|      - |   29 | `	xmlBufferPtr pBuf;   /* non-NULL for openMemory writers */` |
|      - |   30 | `	io_private *pDev;    /* non-NULL for openUri/toUri/toStream writers */` |
|      - |   31 | `	int bOwnDev;         /* this writer opened pDev, so its close closes it */` |
|      - |   32 | `	int bIoFailed;       /* the last verb's write to pDev failed (see XwRun) */` |
|      - |   33 | `	ph7_class_instance *pOwner; /* the object whose slot holds it: the one whose` |
|      - |   34 | `	                             * death flushes it (see XmlWriterInstanceRelease) */` |
|      - |   35 | `	phl_xmlwriter *pNext;` |
|      - |   36 | `};` |
|      - |   37 |  |
|      - |   38 | `/*` |
|      - |   39 | ` * libxml reports a write that failed through the error handler, and the paths` |
|      - |   40 | ` * that run while an object or the whole VM is going away have no caller to` |
|      - |   41 | ` * warn: php frees its writers with its request's error handling already gone,` |
|      - |   42 | ` * so nothing is printed there either. Drop whatever those paths raise.` |
|      - |   43 | ` */` |
|      - |   44 | `#if LIBXML_VERSION >= 21200` |
|    ! 0 |   45 | `static void XmlWriterSilentError(void *pUser,const xmlError *pErr)` |
|      - |   46 | `#else` |
|      3 |   47 | `static void XmlWriterSilentError(void *pUser,xmlErrorPtr pErr)` |
|      - |   48 | `#endif` |
|    ! 0 |   49 | `{` |
|    ! 0 |   50 | `	SXUNUSED(pUser);` |
|    ! 0 |   51 | `	SXUNUSED(pErr);` |
|      3 |   52 | `}` |
|      - |   53 | `/*` |
|      - |   54 | ` * Free one writer (called from the registry sweep in vm_libxml.c via` |
|      - |   55 | ` * PH7_XmlWriterVmRelease).  The order matters: the text writer must be` |
|      - |   56 | ` * freed before its backing buffer.` |
|      - |   57 | ` */` |
|    132 |   58 | `static void XmlWriterFree(phl_xmlwriter *pXw)` |
|      1 |   59 | `{` |
|    133 |   60 | `	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);` |
|    133 |   61 | `	if( pXw->pWriter ){` |
|    133 |   62 | `		xmlFreeTextWriter(pXw->pWriter);` |
|    133 |   63 | `		pXw->pWriter = 0;` |
|     66 |   64 | `	}` |
|    133 |   65 | `	if( pXw->pBuf ){` |
|    121 |   66 | `		xmlBufferFree(pXw->pBuf);` |
|    121 |   67 | `		pXw->pBuf = 0;` |
|     60 |   68 | `	}` |
|    133 |   69 | `	xmlSetStructuredErrorFunc(0,0);` |
|    133 |   70 | `}` |
|      - |   71 | `/*` |
|      - |   72 | ` * Free every registered writer.  Called from PH7_LibxmlVmReset /` |
|      - |   73 | ` * PH7_LibxmlVmRelease before the allocator that holds the shells is torn` |
|      - |   74 | ` * down.` |
|      - |   75 | ` */` |
|   4672 |   76 | `PH7_PRIVATE void PH7_XmlWriterVmSweep(ph7_vm *pVm)` |
|      5 |   77 | `{` |
|   4677 |   78 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pVm->pXmlWriters;` |
|   4809 |   79 | `	while( pXw ){` |
|    133 |   80 | `		phl_xmlwriter *pNext = pXw->pNext;` |
|    133 |   81 | `		XmlWriterFree(pXw);` |
|    133 |   82 | `		SyMemBackendFree(&pVm->sAllocator,pXw);` |
|    133 |   83 | `		pXw = pNext;` |
|      1 |   84 | `	}` |
|   4677 |   85 | `	pVm->pXmlWriters = 0;` |
|   4677 |   86 | `}` |
|      - |   87 |  |
|      - |   88 | `/*` |
|      - |   89 | ` * A writer's bytes on their way to a stream. libxml calls this from its own` |
|      - |   90 | ` * output buffer, so everything the engine's stream layer does for fwrite() --` |
|      - |   91 | ` * the filter chain, the buffered position -- happens here too.` |
|      - |   92 | ` */` |
|     28 |   93 | `static int XmlWriterIoWrite(void *pUser,const char *zBuf,int nLen)` |
|      1 |   94 | `{` |
|     29 |   95 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;` |
|      - |   96 | `	ph7_int64 nWr;` |
|     29 |   97 | `	if( pXw->pDev == 0 \|\| IO_PRIVATE_INVALID(pXw->pDev) ){` |
|      - |   98 | `		/* The script fclose()'d the handle it handed toStream() */` |
|      7 |   99 | `		pXw->bIoFailed = 1;` |
|      7 |  100 | `		return -1;` |
|      - |  101 | `	}` |
|     23 |  102 | `	nWr = PH7_StreamWrite(pXw->pDev,(const void *)zBuf,(ph7_int64)nLen);` |
|     23 |  103 | `	if( nWr < 0 ){` |
|    ! 0 |  104 | `		pXw->bIoFailed = 1;` |
|    ! 0 |  105 | `		return -1;` |
|      - |  106 | `	}` |
|     23 |  107 | `	return (int)nWr;` |
|     15 |  108 | `}` |
|      - |  109 | `/*` |
|      - |  110 | ` * The output buffer's close, reached from xmlFreeTextWriter (so: the registry` |
|      - |  111 | ` * sweep at VM reset). A handle this writer OPENED is closed here, which is` |
|      - |  112 | ` * where a file written through openUri() gets its last bytes; a borrowed one --` |
|      - |  113 | ` * toStream()'s -- is left alone, because the script still holds it.` |
|      - |  114 | ` */` |
|    118 |  115 | `static int XmlWriterIoClose(void *pUser)` |
|      1 |  116 | `{` |
|    119 |  117 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)pUser;` |
|    119 |  118 | `	if( pXw->bOwnDev && pXw->pDev && !IO_PRIVATE_INVALID(pXw->pDev) ){` |
|      9 |  119 | `		PH7_StreamFilterReleaseChains(pXw->pDev);` |
|      9 |  120 | `		PH7_StreamCloseHandle(pXw->pDev->pStream,pXw->pDev->pHandle);` |
|      9 |  121 | `		MarkIOPrivateClosed(pXw->pDev);` |
|      4 |  122 | `	}` |
|    119 |  123 | `	pXw->pDev = 0;` |
|    119 |  124 | `	return 0;` |
|      1 |  125 | `}` |
|      - |  126 | `/*` |
|      - |  127 | ` * Flush a writer and let go of its stream: what php does when the writer is` |
|      - |  128 | ` * freed, which is the point at which a file opened through openUri() is` |
|      - |  129 | ` * complete. The libxml writer itself stays alive (the registry sweep frees it),` |
|      - |  130 | ` * so a later call answers false instead of reaching released memory.` |
|      - |  131 | ` */` |
|    106 |  132 | `static void XmlWriterFlushAndDetach(phl_xmlwriter *pXw)` |
|      1 |  133 | `{` |
|    107 |  134 | `	xmlSetStructuredErrorFunc(0,XmlWriterSilentError);` |
|    107 |  135 | `	if( pXw->pWriter ){` |
|    107 |  136 | `		xmlTextWriterFlush(pXw->pWriter);` |
|     53 |  137 | `	}` |
|    107 |  138 | `	XmlWriterIoClose((void *)pXw);` |
|    107 |  139 | `	xmlSetStructuredErrorFunc(0,0);` |
|    107 |  140 | `	pXw->pOwner = 0;` |
|    107 |  141 | `}` |
|    758 |  142 | `static phl_xmlwriter * XmlWriterArg(ph7_value *pVal)` |
|      1 |  143 | `{` |
|    759 |  144 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    173 |  145 | `		return 0;` |
|      - |  146 | `	}` |
|    587 |  147 | `	return (phl_xmlwriter *)ph7_value_to_resource(pVal);` |
|    380 |  148 | `}` |
|      - |  149 |  |
|      - |  150 | `/*` |
|      - |  151 | ` * The writer behind $this->__res.` |
|      - |  152 | ` *` |
|      - |  153 | `` * These methods were global `__xw_verb($this->__res, ...)` thunks, so each body`` |
|      - |  154 | ` * used to take the resource as argument #0. As native methods they reach it the` |
|      - |  155 | ` * way php does -- through the receiver -- and their arguments start at #0.` |
|      - |  156 | ` */` |
|    436 |  157 | `static phl_xmlwriter * XmlWriterOf(ph7_context *pCtx)` |
|      1 |  158 | `{` |
|    437 |  159 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  160 | `	SyString sAttr;` |
|      - |  161 | `	ph7_value *pRes;` |
|    437 |  162 | `	if( pThis == 0 ){` |
|    ! 0 |  163 | `		return 0;` |
|      - |  164 | `	}` |
|    437 |  165 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    437 |  166 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    437 |  167 | `	return XmlWriterArg(pRes);` |
|    219 |  168 | `}` |
|      - |  169 | `/*` |
|      - |  170 | ` * One call into a writer verb.` |
|      - |  171 | ` *` |
|      - |  172 | ` * php implements the class and the procedural surface with ONE C function per` |
|      - |  173 | `` * verb: the method spelling reaches the writer through `$this`, the function`` |
|      - |  174 | ` * spelling takes it as argument #1 and shifts every other argument up by one.` |
|      - |  175 | ` * The two therefore share their diagnostics, and that is visible from PHP --` |
|      - |  176 | ` * see XmlWriterCheckName for the off-by-one it leaves in the method's own` |
|      - |  177 | ` * error text. A verb body reads its arguments through this descriptor so both` |
|      - |  178 | ` * entry points can drive it.` |
|      - |  179 | ` */` |
|      - |  180 | `typedef struct xw_call xw_call;` |
|      - |  181 | `struct xw_call {` |
|      - |  182 | `	phl_xmlwriter *pXw;    /* the resolved writer; never NULL in a verb body */` |
|      - |  183 | `	const char *zFn;       /* "XMLWriter::startElement" / "xmlwriter_start_element" */` |
|      - |  184 | `	const char *zNameArg;  /* this spelling's argument text for the NAME argument */` |
|      - |  185 | `	int iArgBase;          /* 0 for a method, 1 for the function spelling: what a` |
|      - |  186 | `	                        * ZPP-style message adds to an argument's position */` |
|      - |  187 | `	int nArg;              /* how many arguments follow the writer */` |
|      - |  188 | `	ph7_value **apArg;     /* the first argument past the writer */` |
|      - |  189 | `};` |
|      - |  190 | `/*` |
|      - |  191 | ` * php's Z_XMLWRITER_P: an XMLWriter that was never opened has no writer behind` |
|      - |  192 | ` * it, and every method and every procedural entry refuses it with this Error` |
|      - |  193 | ` * rather than answering false -- which is what a caller would otherwise store` |
|      - |  194 | ` * or print as if a document had been written.` |
|      - |  195 | ` */` |
|     14 |  196 | `static int XmlWriterMissing(ph7_context *pCtx)` |
|      1 |  197 | `{` |
|     15 |  198 | `	return PH7_VmThrowException(pCtx,"Error","Invalid or uninitialized XMLWriter object");` |
|      1 |  199 | `}` |
|      - |  200 | `/*` |
|      - |  201 | `` * The writer behind an `XMLWriter $writer` argument. The declared type has`` |
|      - |  202 | ` * already refused everything that is not one.` |
|      - |  203 | ` */` |
|     58 |  204 | `static phl_xmlwriter * XmlWriterOfValue(ph7_value *pArg)` |
|      1 |  205 | `{` |
|      - |  206 | `	ph7_class_instance *pThis;` |
|      - |  207 | `	SyString sAttr;` |
|     59 |  208 | `	if( pArg == 0 \|\| (pArg->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  209 | `		return 0;` |
|      - |  210 | `	}` |
|     59 |  211 | `	pThis = (ph7_class_instance *)pArg->x.pOther;` |
|     59 |  212 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|     59 |  213 | `	return XmlWriterArg(PH7_ClassInstanceFetchAttr(pThis,&sAttr));` |
|     30 |  214 | `}` |
|      - |  215 | `/*` |
|      - |  216 | ` * Build the call descriptor for the METHOD spelling. Returns 0 when the verb` |
|      - |  217 | ` * may run, or the throw status when the receiver holds no writer.` |
|      - |  218 | ` */` |
|    436 |  219 | `static int XwCallFromThis(ph7_context *pCtx,xw_call *pCall,const char *zFn,` |
|      - |  220 | `	const char *zNameArg,int nArg,ph7_value **apArg)` |
|      1 |  221 | `{` |
|    437 |  222 | `	phl_xmlwriter *pXw = XmlWriterOf(pCtx);` |
|    437 |  223 | `	if( pXw == 0 \|\| pXw->pWriter == 0 ){` |
|     13 |  224 | `		return XmlWriterMissing(pCtx);` |
|      - |  225 | `	}` |
|    425 |  226 | `	pCall->pXw = pXw;` |
|    425 |  227 | `	pCall->zFn = zFn;` |
|    425 |  228 | `	pCall->zNameArg = zNameArg;` |
|    425 |  229 | `	pCall->iArgBase = 0;` |
|    425 |  230 | `	pCall->nArg = nArg;` |
|    425 |  231 | `	pCall->apArg = apArg;` |
|    425 |  232 | `	return 0;` |
|    219 |  233 | `}` |
|      - |  234 | `/* The n-th argument past the writer as a string, or "" when it is absent. */` |
|    290 |  235 | `static const char * XwStr(xw_call *pCall,int iArg)` |
|      1 |  236 | `{` |
|    291 |  237 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0 ){` |
|    ! 0 |  238 | `		return "";` |
|      - |  239 | `	}` |
|    291 |  240 | `	return ph7_value_to_string(pCall->apArg[iArg],0);` |
|    146 |  241 | `}` |
|      - |  242 | `/* The n-th argument past the writer as a string, or NULL for an absent/null one. */` |
|    232 |  243 | `static const char * XwStrOrNull(xw_call *pCall,int iArg)` |
|      1 |  244 | `{` |
|    232 |  245 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0` |
|    167 |  246 | `	 \|\| ph7_value_is_null(pCall->apArg[iArg]) ){` |
|    103 |  247 | `		return 0;` |
|      - |  248 | `	}` |
|    131 |  249 | `	return ph7_value_to_string(pCall->apArg[iArg],0);` |
|    117 |  250 | `}` |
|      - |  251 | `/* The n-th argument past the writer as a bool. */` |
|    120 |  252 | `static int XwBool(xw_call *pCall,int iArg,int bDefault)` |
|      1 |  253 | `{` |
|    121 |  254 | `	if( iArg >= pCall->nArg \|\| pCall->apArg[iArg] == 0 ){` |
|     91 |  255 | `		return bDefault;` |
|      - |  256 | `	}` |
|     31 |  257 | `	return ph7_value_to_bool(pCall->apArg[iArg]) ? 1 : 0;` |
|     61 |  258 | `}` |
|      - |  259 | `/*` |
|      - |  260 | ` * php's XMLW_NAME_CHK: the name a verb is handed is validated with libxml's own` |
|      - |  261 | ` * xmlValidateName and refused with a ValueError before anything is written --` |
|      - |  262 | ` * the alternative is what this engine used to do, which is to hand libxml a name` |
|      - |  263 | `` * it will not quote and emit a document that is not XML (`<1bad`, `<a x y="v"`).`` |
|      - |  264 | ` *` |
|      - |  265 | ` * The argument NUMBER php reports is the PROCEDURAL one, hardcoded in the macro,` |
|      - |  266 | ` * so the method spelling reports its own first argument as "#2" and prints` |
|      - |  267 | `` * whichever of its OWN parameters sits at that shifted position -- `$content`,`` |
|      - |  268 | `` * `$value`, `$isParam` -- or no name at all when it has none. Reproduced as`` |
|      - |  269 | ` * written: each entry point states its spelling's text.` |
|      - |  270 | ` */` |
|    196 |  271 | `static int XmlWriterCheckName(ph7_context *pCtx,xw_call *pCall,const char *zName,` |
|      - |  272 | `	const char *zWhat)` |
|      1 |  273 | `{` |
|    197 |  274 | `	if( xmlValidateName((const xmlChar *)zName,0) == 0 ){` |
|    149 |  275 | `		return 0;` |
|      - |  276 | `	}` |
|     73 |  277 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  278 | `		"%s(): Argument %s must be a valid %s, \"%s\" given",` |
|     24 |  279 | `		pCall->zFn,pCall->zNameArg,zWhat,zName);` |
|     99 |  280 | `}` |
|      - |  281 | `/*` |
|      - |  282 | ` * Allocate a writer shell and chain it on the per-VM registry. The shell is` |
|      - |  283 | ` * reclaimed with the allocator and the libxml writer behind it by the sweep;` |
|      - |  284 | ` * a URI writer's own handle is flushed and closed earlier, when the object` |
|      - |  285 | ` * holding it dies (XmlWriterInstanceRelease).` |
|      - |  286 | ` */` |
|    132 |  287 | `static phl_xmlwriter * XmlWriterNew(ph7_vm *pVm)` |
|      1 |  288 | `{` |
|    133 |  289 | `	phl_xmlwriter *pXw = (phl_xmlwriter *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlwriter));` |
|    133 |  290 | `	if( pXw == 0 ){` |
|    ! 0 |  291 | `		return 0;` |
|      - |  292 | `	}` |
|    133 |  293 | `	SyZero(pXw,sizeof(phl_xmlwriter));` |
|    133 |  294 | `	pXw->pNext = (phl_xmlwriter *)pVm->pXmlWriters;` |
|    133 |  295 | `	pVm->pXmlWriters = (void *)pXw;` |
|    133 |  296 | `	return pXw;` |
|     67 |  297 | `}` |
|      - |  298 | `/*` |
|      - |  299 | ` * Store a writer in an object's hidden slot, flushing the one it replaces.` |
|      - |  300 | ` * The replaced SHELL is not freed: the slot is reachable from PHP, so another` |
|      - |  301 | ` * value may still name it, and the registry sweep frees every writer anyway.` |
|      - |  302 | ` */` |
|    132 |  303 | `static int XmlWriterAttach(ph7_class_instance *pThis,phl_xmlwriter *pXw)` |
|      1 |  304 | `{` |
|      - |  305 | `	SyString sAttr;` |
|      - |  306 | `	ph7_value *pRes;` |
|    133 |  307 | `	if( pThis == 0 ){` |
|    ! 0 |  308 | `		return -1;` |
|      - |  309 | `	}` |
|    133 |  310 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    133 |  311 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    133 |  312 | `	if( pRes == 0 ){` |
|    ! 0 |  313 | `		return -1;` |
|      - |  314 | `	}` |
|      - |  315 | `	{` |
|      - |  316 | `		/* A second open on the same object replaces its writer, and php frees` |
|      - |  317 | `		 * the old one there -- which is what flushes a file opened through` |
|      - |  318 | `		 * openUri(). Flush and close it here for the same reason; the shell` |
|      - |  319 | `		 * itself stays on the registry, since the slot is reachable from PHP. */` |
|    133 |  320 | `		phl_xmlwriter *pOld = XmlWriterArg(pRes);` |
|    133 |  321 | `		if( pOld && pOld != pXw && pOld->pOwner == pThis ){` |
|    ! 0 |  322 | `			XmlWriterFlushAndDetach(pOld);` |
|    ! 0 |  323 | `		}` |
|      - |  324 | `	}` |
|    133 |  325 | `	PH7_MemObjRelease(pRes);` |
|    133 |  326 | `	pRes->x.pOther = pXw;` |
|    133 |  327 | `	MemObjSetType(pRes,MEMOBJ_RES);` |
|    133 |  328 | `	pXw->pOwner = pThis;` |
|    133 |  329 | `	return 0;` |
|     67 |  330 | `}` |
|      - |  331 | `/*` |
|      - |  332 | ` * php frees the libxml writer with the OBJECT, and for a writer opened on a URI` |
|      - |  333 | `` * that is when the file gets its last bytes -- `unset($w)` and then reading the`` |
|      - |  334 | ` * file is how a document written to disk is finished. PH7 resources carry no` |
|      - |  335 | ` * destructor hook, but a native class does: xRelease runs while the instance's` |
|      - |  336 | ` * slots are still readable, and before the registry sweep.` |
|      - |  337 | ` *` |
|      - |  338 | ` * Only the object the writer was ATTACHED to flushes it. The hidden slot is` |
|      - |  339 | ` * assignable from PHP, so a second object can name the same writer; the shell` |
|      - |  340 | ` * is freed by the sweep and never here, and a write after the flush answers` |
|      - |  341 | ` * false rather than reaching released memory.` |
|      - |  342 | ` */` |
|    132 |  343 | `static void XmlWriterInstanceRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 |  344 | `{` |
|      - |  345 | `	SyString sAttr;` |
|      - |  346 | `	ph7_value *pRes;` |
|      - |  347 | `	phl_xmlwriter *pXw;` |
|     66 |  348 | `	SXUNUSED(pVm);` |
|    133 |  349 | `	SyStringInitFromBuf(&sAttr,"__res",sizeof("__res")-1);` |
|    133 |  350 | `	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);` |
|    133 |  351 | `	pXw = XmlWriterArg(pRes);` |
|    133 |  352 | `	if( pXw == 0 \|\| pXw->pOwner != pThis ){` |
|     27 |  353 | `		return;` |
|      - |  354 | `	}` |
|    107 |  355 | `	XmlWriterFlushAndDetach(pXw);` |
|     67 |  356 | `}` |
|      - |  357 | `/* An in-memory writer: its own xmlBuffer, which outputMemory() reads back. */` |
|    120 |  358 | `static phl_xmlwriter * XmlWriterOpenMemory(ph7_vm *pVm)` |
|      1 |  359 | `{` |
|    121 |  360 | `	phl_xmlwriter *pXw = XmlWriterNew(pVm);` |
|    121 |  361 | `	if( pXw == 0 ){` |
|    ! 0 |  362 | `		return 0;` |
|      - |  363 | `	}` |
|    121 |  364 | `	pXw->pBuf = xmlBufferCreate();` |
|    121 |  365 | `	if( pXw->pBuf ){` |
|    121 |  366 | `		pXw->pWriter = xmlNewTextWriterMemory(pXw->pBuf,0);` |
|     60 |  367 | `	}` |
|    121 |  368 | `	if( pXw->pWriter == 0 ){` |
|    ! 0 |  369 | `		XmlWriterFree(pXw);` |
|    ! 0 |  370 | `		return 0;` |
|      - |  371 | `	}` |
|    121 |  372 | `	return pXw;` |
|     61 |  373 | `}` |
|      - |  374 | `/*` |
|      - |  375 | `` * A writer over an engine stream handle. `bOwn` says whether closing the writer`` |
|      - |  376 | ` * closes the handle -- true for openUri()/toUri(), false for the handle` |
|      - |  377 | ` * toStream() borrows from the script.` |
|      - |  378 | ` */` |
|     12 |  379 | `static phl_xmlwriter * XmlWriterOpenDevice(ph7_vm *pVm,io_private *pDev,int bOwn)` |
|      1 |  380 | `{` |
|      - |  381 | `	xmlOutputBufferPtr pOut;` |
|     13 |  382 | `	phl_xmlwriter *pXw = XmlWriterNew(pVm);` |
|     13 |  383 | `	if( pXw == 0 ){` |
|    ! 0 |  384 | `		return 0;` |
|      - |  385 | `	}` |
|     13 |  386 | `	pXw->pDev = pDev;` |
|     13 |  387 | `	pXw->bOwnDev = bOwn;` |
|     13 |  388 | `	pOut = xmlOutputBufferCreateIO(XmlWriterIoWrite,XmlWriterIoClose,(void *)pXw,0);` |
|     13 |  389 | `	if( pOut == 0 ){` |
|      - |  390 | `		/* Nothing will call the close callback, so an owned handle is closed here` |
|      - |  391 | `		 * rather than left open until the VM goes. */` |
|    ! 0 |  392 | `		XmlWriterIoClose((void *)pXw);` |
|    ! 0 |  393 | `		return 0;` |
|      - |  394 | `	}` |
|     13 |  395 | `	pXw->pWriter = xmlNewTextWriter(pOut);` |
|     13 |  396 | `	if( pXw->pWriter == 0 ){` |
|      - |  397 | `		/* xmlNewTextWriter does not take the buffer on failure */` |
|    ! 0 |  398 | `		xmlOutputBufferClose(pOut);` |
|    ! 0 |  399 | `		pXw->pDev = 0;` |
|    ! 0 |  400 | `		return 0;` |
|      - |  401 | `	}` |
|     13 |  402 | `	return pXw;` |
|      7 |  403 | `}` |
|      - |  404 | `/*` |
|      - |  405 | ` * php refuses a NUL inside the two arguments it hands to a C interface that` |
|      - |  406 | ` * would stop at one: the URI, and startDocument()'s encoding NAME. The message` |
|      - |  407 | ` * is ZPP's, so the argument number is this SPELLING's own -- unlike the` |
|      - |  408 | ` * hand-written name check, which always reports the procedural position.` |
|      - |  409 | ` */` |
|     50 |  410 | `static int XmlWriterCheckNul(ph7_context *pCtx,const char *zFn,int iArg,const char *zParam,` |
|      - |  411 | `	ph7_value *pVal)` |
|      1 |  412 | `{` |
|     51 |  413 | `	int nLen = 0;` |
|      - |  414 | `	const char *zStr;` |
|     51 |  415 | `	if( pVal == 0 \|\| ph7_value_is_null(pVal) ){` |
|      5 |  416 | `		return 0;` |
|      - |  417 | `	}` |
|     47 |  418 | `	zStr = ph7_value_to_string(pVal,&nLen);` |
|     47 |  419 | `	if( SyByteFind(zStr,(sxu32)nLen,'\0',0) != SXRET_OK ){` |
|     41 |  420 | `		return 0;` |
|      - |  421 | `	}` |
|     10 |  422 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      3 |  423 | `		"%s(): Argument #%d ($%s) must not contain any null bytes",zFn,iArg,zParam);` |
|     26 |  424 | `}` |
|      - |  425 | `/*` |
|      - |  426 | ` * Does the FILE path a failed open was given resolve at all -- that is, does the` |
|      - |  427 | `` * directory it names exist? php's `toUri()` factory raises two different things`` |
|      - |  428 | ` * for a path it cannot write, and this is the line between them:` |
|      - |  429 | `` * `expand_filepath()` failing (no such directory) is the ValueError, and`` |
|      - |  430 | ` * everything the OPEN itself refuses -- a directory, an unwritable directory,` |
|      - |  431 | ` * a scheme no wrapper takes -- is the plain Error. A path with no separator is` |
|      - |  432 | ` * relative to a working directory that always exists, so it resolves.` |
|      - |  433 | ` */` |
|      8 |  434 | `static int XmlWriterPathResolves(ph7_vm *pVm,const char *zUri,int nUri)` |
|      1 |  435 | `{` |
|      9 |  436 | `	const ph7_vfs *pVfs = pVm->pEngine ? pVm->pEngine->pVfs : 0;` |
|      - |  437 | `	SyBlob sDir;` |
|      9 |  438 | `	int i,iCut = -1,rc;` |
|      - |  439 | `	/* php reads the path as a URI reference first, and one with a SCHEME skips` |
|      - |  440 | `	 * the directory check entirely -- so the open's own refusal, the plain` |
|      - |  441 | `	 * Error, answers for it. The exceptions are file:/// and file://localhost/,` |
|      - |  442 | `	 * which are stripped to the local path and checked like one. A Windows` |
|      - |  443 | `	 * drive path is a URI with a scheme too: "C:" parses as one. */` |
|     45 |  444 | `	for( i = 0 ; i < nUri ; ++i ){` |
|     45 |  445 | `		int c = (unsigned char)zUri[i];` |
|     45 |  446 | `		if( !(SyisAlpha(c) \|\| (i > 0 && (SyisDigit(c) \|\| c == '+' \|\| c == '-' \|\| c == '.'))) ){` |
|      5 |  447 | `			break;` |
|      - |  448 | `		}` |
|     19 |  449 | `	}` |
|      9 |  450 | `	if( i > 0 && i < nUri && zUri[i] == ':' ){` |
|      5 |  451 | `		if( nUri >= 8 && SyStrnicmp(zUri,"file:///",8) == 0 ){` |
|      3 |  452 | `			zUri += 7;` |
|      3 |  453 | `			nUri -= 7;` |
|      4 |  454 | `		}else if( nUri >= 17 && SyStrnicmp(zUri,"file://localhost/",17) == 0 ){` |
|    ! 0 |  455 | `			zUri += 16;` |
|    ! 0 |  456 | `			nUri -= 16;` |
|    ! 0 |  457 | `		}else{` |
|      3 |  458 | `			return 1;` |
|      - |  459 | `		}` |
|      3 |  460 | `		if( nUri > 2 && SyisAlpha((unsigned char)zUri[1]) && zUri[2] == ':' ){` |
|      1 |  461 | `			zUri++;   /* file:///C:/x names C:/x */` |
|      1 |  462 | `			nUri--;` |
|    ! 0 |  463 | `		}` |
|      1 |  464 | `	}` |
|      7 |  465 | `	if( pVfs == 0 \|\| pVfs->xIsdir == 0 ){` |
|    ! 0 |  466 | `		return 1;` |
|      - |  467 | `	}` |
|    221 |  468 | `	for( i = 0 ; i < nUri ; ++i ){` |
|    215 |  469 | `		if( zUri[i] == '/' \|\| zUri[i] == '\\' ){` |
|     23 |  470 | `			iCut = i;` |
|     15 |  471 | `		}` |
|    153 |  472 | `	}` |
|      7 |  473 | `	if( iCut < 0 ){` |
|    ! 0 |  474 | `		return 1;   /* a bare name: the working directory */` |
|      - |  475 | `	}` |
|      7 |  476 | `	if( iCut == 0 ){` |
|    ! 0 |  477 | `		return 1;   /* "/name": the root */` |
|      - |  478 | `	}` |
|      7 |  479 | `	SyBlobInit(&sDir,&pVm->sAllocator);` |
|      7 |  480 | `	SyBlobAppend(&sDir,zUri,(sxu32)iCut);` |
|      7 |  481 | `	SyBlobAppend(&sDir,"\0",sizeof(char));` |
|      7 |  482 | `	rc = pVfs->xIsdir((const char *)SyBlobData(&sDir)) == PH7_OK;` |
|      7 |  483 | `	SyBlobRelease(&sDir);` |
|      7 |  484 | `	return rc;` |
|      5 |  485 | `}` |
|      - |  486 | `/*` |
|      - |  487 | ` * Open a URI for writing through the engine's stream layer and answer the` |
|      - |  488 | `` * writer, or 0 with php's own diagnostic already raised. `zFn` names the`` |
|      - |  489 | ` * spelling (method or function) every message is reported under.` |
|      - |  490 | ` */` |
|     36 |  491 | `static phl_xmlwriter * XmlWriterOpenUri(ph7_context *pCtx,ph7_value *pArg,const char *zFn,` |
|      - |  492 | `	int bStatic,int *pRc)` |
|      1 |  493 | `{` |
|     37 |  494 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  495 | `	const ph7_io_stream *pStream;` |
|      - |  496 | `	io_private *pDev;` |
|      - |  497 | `	phl_xmlwriter *pXw;` |
|      - |  498 | `	const char *zUri,*zSrc;` |
|     37 |  499 | `	int nUri = 0,nSrc;` |
|     37 |  500 | `	*pRc = PH7_OK;` |
|     37 |  501 | `	zUri = pArg ? ph7_value_to_string(pArg,&nUri) : "";` |
|     37 |  502 | `	zSrc = zUri;` |
|     37 |  503 | `	nSrc = nUri;` |
|     37 |  504 | `	if( nUri < 1 ){` |
|      7 |  505 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  506 | `			"%s(): Argument #1 ($uri) must not be empty",zFn);` |
|      5 |  507 | `		return 0;` |
|      - |  508 | `	}` |
|     33 |  509 | `	*pRc = XmlWriterCheckNul(pCtx,zFn,1,"uri",pArg);` |
|     33 |  510 | `	if( *pRc != PH7_OK ){` |
|      5 |  511 | `		return 0;` |
|      - |  512 | `	}` |
|     29 |  513 | `	pStream = PH7_VmGetStreamDevice(pVm,&zUri,nUri);` |
|     29 |  514 | `	pDev = pStream ? (io_private *)ph7_context_alloc_chunk(pCtx,sizeof(io_private),TRUE,FALSE) : 0;` |
|     29 |  515 | `	if( pDev ){` |
|     25 |  516 | `		InitIOPrivate(pVm,pStream,pDev);` |
|      - |  517 | `		/* php opens the destination "wb" */` |
|     37 |  518 | `		pDev->pHandle = PH7_StreamOpenHandle(pVm,pStream,zUri,` |
|     12 |  519 | `			PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,pArg,FALSE,0,zFn);` |
|     25 |  520 | `		if( pDev->pHandle ){` |
|      9 |  521 | `			int nOrig = 0;` |
|      9 |  522 | `			const char *zOrig = ph7_value_to_string(pArg,&nOrig);` |
|      9 |  523 | `			SetIOPrivateOpenedAs(pDev,zOrig,nOrig,"wb",2);` |
|      5 |  524 | `		}else{` |
|      - |  525 | `			/* Nothing reached PHP, so the shell goes back */` |
|     17 |  526 | `			PH7_StreamReleaseUnopened(pCtx,pDev);` |
|     17 |  527 | `			pDev = 0;` |
|      - |  528 | `		}` |
|     12 |  529 | `	}` |
|     29 |  530 | `	pXw = pDev ? XmlWriterOpenDevice(pVm,pDev,TRUE) : 0;` |
|     29 |  531 | `	if( pXw == 0 ){` |
|      - |  532 | `		/* php's refusals for the same failure: the FACTORY raises -- a ValueError` |
|      - |  533 | `		 * when the path did not resolve, php's plain Error when the open itself` |
|      - |  534 | `		 * was refused -- while the opener warns and answers false. */` |
|     21 |  535 | `		if( bStatic ){` |
|     11 |  536 | `			if( pStream == 0 \|\| XmlWriterPathResolves(pVm,zSrc,nSrc) ){` |
|      7 |  537 | `				*pRc = PH7_VmThrowException(pCtx,"Error","Could not construct libxml writer");` |
|      4 |  538 | `			}else{` |
|      7 |  539 | `				*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  540 | `					"%s(): Argument #1 ($uri) must resolve to a valid file path",zFn);` |
|      - |  541 | `			}` |
|      6 |  542 | `		}else{` |
|      - |  543 | `			SyString sFn;` |
|     11 |  544 | `			SyStringInitFromBuf(&sFn,zFn,SyStrlen(zFn));` |
|     11 |  545 | `			PH7_VmThrowError(pVm,&sFn,PH7_CTX_WARNING,"Unable to resolve file path");` |
|      - |  546 | `		}` |
|     21 |  547 | `		return 0;` |
|      - |  548 | `	}` |
|      9 |  549 | `	return pXw;` |
|     19 |  550 | `}` |
|      - |  551 | `/* bool XMLWriter::openMemory() */` |
|    110 |  552 | `static int vm_builtin_xw_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  553 | `{` |
|      - |  554 | `	phl_xmlwriter *pXw;` |
|     55 |  555 | `	SXUNUSED(nArg);` |
|     55 |  556 | `	SXUNUSED(apArg);` |
|    111 |  557 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|    111 |  558 | `	if( pXw == 0 \|\| XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){` |
|    ! 0 |  559 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  560 | `		return PH7_OK;` |
|      - |  561 | `	}` |
|    111 |  562 | `	ph7_result_bool(pCtx,1);` |
|    111 |  563 | `	return PH7_OK;` |
|     56 |  564 | `}` |
|      - |  565 | `/* bool XMLWriter::openUri(string $uri) */` |
|     18 |  566 | `static int vm_builtin_xw_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  567 | `{` |
|      - |  568 | `	phl_xmlwriter *pXw;` |
|     19 |  569 | `	int rc = PH7_OK;` |
|     19 |  570 | `	pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::openUri",FALSE,&rc);` |
|     19 |  571 | `	if( pXw == 0 ){` |
|     15 |  572 | `		if( rc != PH7_OK ){` |
|      5 |  573 | `			return rc;` |
|      - |  574 | `		}` |
|     11 |  575 | `		ph7_result_bool(pCtx,0);` |
|     11 |  576 | `		return PH7_OK;` |
|      - |  577 | `	}` |
|      5 |  578 | `	if( XmlWriterAttach(PH7_ContextThis(pCtx),pXw) != 0 ){` |
|    ! 0 |  579 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  580 | `		return PH7_OK;` |
|      - |  581 | `	}` |
|      5 |  582 | `	ph7_result_bool(pCtx,1);` |
|      5 |  583 | `	return PH7_OK;` |
|     10 |  584 | `}` |
|      - |  585 | `/*` |
|      - |  586 | ` * The php 8.5 factories. Each answers a NEW writer rather than configuring the` |
|      - |  587 | ` * receiver, and each builds the LATE STATIC class, so a subclass of XMLWriter` |
|      - |  588 | ` * gets one of its own.` |
|      - |  589 | ` */` |
|     18 |  590 | `static int XmlWriterFactory(ph7_context *pCtx,phl_xmlwriter *pXw)` |
|      1 |  591 | `{` |
|     19 |  592 | `	ph7_class *pClass = PH7_ContextCalledClass(pCtx);` |
|      - |  593 | `	ph7_class_instance *pObj;` |
|     19 |  594 | `	if( pClass == 0 ){` |
|      7 |  595 | `		pClass = PH7_VmExtractClass(pCtx->pVm,"XMLWriter",sizeof("XMLWriter")-1,FALSE,0);` |
|      3 |  596 | `	}` |
|     19 |  597 | `	pObj = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     19 |  598 | `	if( pObj == 0 ){` |
|    ! 0 |  599 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  600 | `	}` |
|     19 |  601 | `	if( XmlWriterAttach(pObj,pXw) != 0 ){` |
|    ! 0 |  602 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  603 | `	}` |
|     19 |  604 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     19 |  605 | `	return PH7_OK;` |
|     10 |  606 | `}` |
|      - |  607 | `/* static XMLWriter::toMemory(): static */` |
|      6 |  608 | `static int vm_builtin_xw_to_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  609 | `{` |
|      - |  610 | `	phl_xmlwriter *pXw;` |
|      3 |  611 | `	SXUNUSED(nArg);` |
|      3 |  612 | `	SXUNUSED(apArg);` |
|      7 |  613 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|      7 |  614 | `	if( pXw == 0 ){` |
|    ! 0 |  615 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  616 | `	}` |
|      7 |  617 | `	return XmlWriterFactory(pCtx,pXw);` |
|      4 |  618 | `}` |
|      - |  619 | `/* static XMLWriter::toUri(string $uri): static */` |
|     14 |  620 | `static int vm_builtin_xw_to_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  621 | `{` |
|     15 |  622 | `	int rc = PH7_OK;` |
|     15 |  623 | `	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"XMLWriter::toUri",TRUE,&rc);` |
|     15 |  624 | `	if( pXw == 0 ){` |
|     13 |  625 | `		return rc;` |
|      - |  626 | `	}` |
|      3 |  627 | `	return XmlWriterFactory(pCtx,pXw);` |
|      8 |  628 | `}` |
|      - |  629 | `/*` |
|      - |  630 | ` * static XMLWriter::toStream(mixed $stream): static` |
|      - |  631 | ` *` |
|      - |  632 | ` * The handle stays the script's: the writer only pushes bytes at it, and` |
|      - |  633 | ` * closing the writer does not close it. A handle the script fcloses afterwards` |
|      - |  634 | ` * makes every later write fail, which is the answer php's own stream reference` |
|      - |  635 | ` * gives once the resource is gone.` |
|      - |  636 | ` */` |
|      8 |  637 | `static int vm_builtin_xw_to_stream(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  638 | `{` |
|      - |  639 | `	io_private *pDev;` |
|      - |  640 | `	phl_xmlwriter *pXw;` |
|      9 |  641 | `	if( nArg < 1 \|\| !ph7_value_is_resource(apArg[0]) ){` |
|      5 |  642 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  643 | `			"XMLWriter::toStream(): Argument #1 ($stream) must be of type resource, %s given",` |
|      2 |  644 | `			nArg > 0 ? ph7_type_name(apArg[0]) : "null");` |
|      - |  645 | `	}` |
|      7 |  646 | `	pDev = (io_private *)ph7_value_to_resource(apArg[0]);` |
|      7 |  647 | `	if( IO_PRIVATE_INVALID(pDev) ){` |
|      3 |  648 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  649 | `			"XMLWriter::toStream(): supplied resource is not a valid stream resource");` |
|      - |  650 | `	}` |
|      5 |  651 | `	pXw = XmlWriterOpenDevice(pCtx->pVm,pDev,FALSE);` |
|      5 |  652 | `	if( pXw == 0 ){` |
|    ! 0 |  653 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  654 | `	}` |
|      5 |  655 | `	return XmlWriterFactory(pCtx,pXw);` |
|      5 |  656 | `}` |
|      - |  657 | `/*` |
|      - |  658 | ` * Run one verb inside a libxml error capture window, the way php's own` |
|      - |  659 | ` * xmlwriter does: libxml reports a refusal it can explain (a DTD with a public` |
|      - |  660 | ` * identifier and no system one, say) through the error handler rather than the` |
|      - |  661 | ` * return value, and php turns each one into a warning naming the caller.` |
|      - |  662 | ` */` |
|    480 |  663 | `static int XwRun(ph7_context *pCtx,xw_call *pCall,int (*xVerb)(ph7_context *,xw_call *))` |
|      1 |  664 | `{` |
|    481 |  665 | `	sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|      - |  666 | `	int rc;` |
|    481 |  667 | `	pCall->pXw->bIoFailed = 0;` |
|    481 |  668 | `	rc = xVerb(pCtx,pCall);` |
|    481 |  669 | `	if( pCall->pXw->bIoFailed ){` |
|      - |  670 | `		/* A write that could not land is reported by the RETURN value alone:` |
|      - |  671 | `		 * php says nothing when the stream behind a writer has gone away, and` |
|      - |  672 | `		 * libxml's own "I/O error" would be a warning php never raises. */` |
|    ! 0 |  673 | `		pCall->pXw->bIoFailed = 0;` |
|    ! 0 |  674 | `		PH7_LibxmlDropErrors(pCtx->pVm,nMark);` |
|    ! 0 |  675 | `		return rc;` |
|      - |  676 | `	}` |
|    481 |  677 | `	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,pCall->zFn);` |
|    481 |  678 | `	return rc;` |
|    241 |  679 | `}` |
|      - |  680 | `/* Report a libxml writer call the way php does: its int status as a bool. */` |
|    342 |  681 | `static int XwStatus(ph7_context *pCtx,int rc)` |
|      1 |  682 | `{` |
|    343 |  683 | `	ph7_result_bool(pCtx,rc >= 0);` |
|    343 |  684 | `	return PH7_OK;` |
|      1 |  685 | `}` |
|      - |  686 | `/*` |
|      - |  687 | ` * One method entry point. The receiver has to hold a writer (php's Error), and` |
|      - |  688 | ` * the verb then reads its arguments from #0 up -- the procedural entry, added` |
|      - |  689 | ` * with the rest of that surface, hands it the same descriptor one slot along.` |
|      - |  690 | ` */` |
|      - |  691 | `#define XW_METHOD(cfn,verb,meth,namearg)                                          \` |
|      - |  692 | `static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \` |
|      - |  693 | `{                                                                                 \` |
|      - |  694 | `	xw_call sCall;                                                            \` |
|      - |  695 | `	int rc;                                                                   \` |
|      - |  696 | `	SyZero(&sCall,sizeof(sCall));                                             \` |
|      - |  697 | `	rc = XwCallFromThis(pCtx,&sCall,"XMLWriter::" meth,namearg,nArg,apArg);   \` |
|      - |  698 | `	if( rc != 0 ){                                                            \` |
|      - |  699 | `		return rc;                                                        \` |
|      - |  700 | `	}                                                                         \` |
|      - |  701 | `	return XwRun(pCtx,&sCall,verb);                                           \` |
|      - |  702 | `}` |
|      - |  703 |  |
|      - |  704 | `/*` |
|      - |  705 | ` * Build the call descriptor for the FUNCTION spelling: the writer is argument` |
|      - |  706 | ` * #1 and everything else shifts one slot, which is where php's diagnostics get` |
|      - |  707 | ` * their numbering from.` |
|      - |  708 | ` */` |
|     58 |  709 | `static int XwCallFromArg(ph7_context *pCtx,xw_call *pCall,const char *zFn,` |
|      - |  710 | `	const char *zNameArg,int nArg,ph7_value **apArg)` |
|      1 |  711 | `{` |
|     59 |  712 | `	phl_xmlwriter *pXw = nArg > 0 ? XmlWriterOfValue(apArg[0]) : 0;` |
|     59 |  713 | `	if( pXw == 0 \|\| pXw->pWriter == 0 ){` |
|      3 |  714 | `		return XmlWriterMissing(pCtx);` |
|      - |  715 | `	}` |
|     57 |  716 | `	pCall->pXw = pXw;` |
|     57 |  717 | `	pCall->zFn = zFn;` |
|     57 |  718 | `	pCall->zNameArg = zNameArg;` |
|     57 |  719 | `	pCall->iArgBase = 1;` |
|     57 |  720 | `	pCall->nArg = nArg - 1;` |
|     57 |  721 | `	pCall->apArg = apArg + 1;` |
|     57 |  722 | `	return 0;` |
|     30 |  723 | `}` |
|      - |  724 | `/* One procedural entry point: the same verb, one argument along. */` |
|      - |  725 | `#define XW_FUNCTION(cfn,verb,fname,namearg)                                       \` |
|      - |  726 | `static int cfn(ph7_context *pCtx,int nArg,ph7_value **apArg)                      \` |
|      - |  727 | `{                                                                                 \` |
|      - |  728 | `	xw_call sCall;                                                            \` |
|      - |  729 | `	int rc;                                                                   \` |
|      - |  730 | `	SyZero(&sCall,sizeof(sCall));                                             \` |
|      - |  731 | `	rc = XwCallFromArg(pCtx,&sCall,fname,namearg,nArg,apArg);                 \` |
|      - |  732 | `	if( rc != 0 ){                                                            \` |
|      - |  733 | `		return rc;                                                        \` |
|      - |  734 | `	}                                                                         \` |
|      - |  735 | `	return XwRun(pCtx,&sCall,verb);                                           \` |
|      - |  736 | `}` |
|      - |  737 |  |
|      - |  738 | `/* bool XMLWriter::setIndent(bool $enable) */` |
|     10 |  739 | `static int XwSetIndent(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  740 | `{` |
|      - |  741 | `	/* No indent STRING is set here: libxml's own default is the single space` |
|      - |  742 | `	 * php answers with, and writing it back made setIndentString() before` |
|      - |  743 | `	 * setIndent() -- the documented order, and the one php's own examples use --` |
|      - |  744 | `	 * silently lose the string it had just been given. */` |
|     11 |  745 | `	return XwStatus(pCtx,xmlTextWriterSetIndent(pCall->pXw->pWriter,XwBool(pCall,0,0)));` |
|      1 |  746 | `}` |
|     11 |  747 | `XW_METHOD(vm_builtin_xw_set_indent,XwSetIndent,"setIndent",0)` |
|      - |  748 |  |
|      - |  749 | `/* bool XMLWriter::setIndentString(string $indentation) */` |
|      2 |  750 | `static int XwSetIndentString(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  751 | `{` |
|      4 |  752 | `	return XwStatus(pCtx,xmlTextWriterSetIndentString(pCall->pXw->pWriter,` |
|      2 |  753 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 |  754 | `}` |
|      3 |  755 | `XW_METHOD(vm_builtin_xw_set_indent_string,XwSetIndentString,"setIndentString",0)` |
|      - |  756 |  |
|      - |  757 | `/*` |
|      - |  758 | ` * bool XMLWriter::startDocument(?string $version, ?string $encoding, ?string $standalone)` |
|      - |  759 | ` *` |
|      - |  760 | ` * The ENCODING is the one argument here php refuses a NUL in: it names a` |
|      - |  761 | ` * character set for libxml to look up, and that lookup stops at the first NUL.` |
|      - |  762 | ` * The version and the standalone flag are written out verbatim and take one.` |
|      - |  763 | ` */` |
|     18 |  764 | `static int XwStartDocument(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  765 | `{` |
|     28 |  766 | `	int rc = XmlWriterCheckNul(pCtx,pCall->zFn,pCall->iArgBase + 2,"encoding",` |
|     18 |  767 | `		pCall->nArg > 1 ? pCall->apArg[1] : 0);` |
|     19 |  768 | `	if( rc != PH7_OK ){` |
|      3 |  769 | `		return rc;` |
|      - |  770 | `	}` |
|     25 |  771 | `	return XwStatus(pCtx,xmlTextWriterStartDocument(pCall->pXw->pWriter,` |
|      8 |  772 | `		XwStrOrNull(pCall,0),XwStrOrNull(pCall,1),XwStrOrNull(pCall,2)));` |
|     10 |  773 | `}` |
|     17 |  774 | `XW_METHOD(vm_builtin_xw_start_document,XwStartDocument,"startDocument",0)` |
|      - |  775 |  |
|      - |  776 | `/* bool XMLWriter::endDocument() */` |
|     12 |  777 | `static int XwEndDocument(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  778 | `{` |
|     13 |  779 | `	return XwStatus(pCtx,xmlTextWriterEndDocument(pCall->pXw->pWriter));` |
|      1 |  780 | `}` |
|     11 |  781 | `XW_METHOD(vm_builtin_xw_end_document,XwEndDocument,"endDocument",0)` |
|      - |  782 |  |
|      - |  783 | `/* bool XMLWriter::startComment() */` |
|      6 |  784 | `static int XwStartComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  785 | `{` |
|      7 |  786 | `	return XwStatus(pCtx,xmlTextWriterStartComment(pCall->pXw->pWriter));` |
|      1 |  787 | `}` |
|      7 |  788 | `XW_METHOD(vm_builtin_xw_start_comment,XwStartComment,"startComment",0)` |
|      - |  789 |  |
|      - |  790 | `/* bool XMLWriter::endComment() */` |
|      6 |  791 | `static int XwEndComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  792 | `{` |
|      7 |  793 | `	return XwStatus(pCtx,xmlTextWriterEndComment(pCall->pXw->pWriter));` |
|      1 |  794 | `}` |
|      7 |  795 | `XW_METHOD(vm_builtin_xw_end_comment,XwEndComment,"endComment",0)` |
|      - |  796 |  |
|      - |  797 | `/* bool XMLWriter::startAttribute(string $name) */` |
|      8 |  798 | `static int XwStartAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  799 | `{` |
|      9 |  800 | `	const char *zName = XwStr(pCall,0);` |
|      9 |  801 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      9 |  802 | `	if( rc != 0 ){` |
|      3 |  803 | `		return rc;` |
|      - |  804 | `	}` |
|      7 |  805 | `	return XwStatus(pCtx,xmlTextWriterStartAttribute(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      5 |  806 | `}` |
|      7 |  807 | `XW_METHOD(vm_builtin_xw_start_attribute,XwStartAttribute,"startAttribute","#2")` |
|      - |  808 |  |
|      - |  809 | `/* bool XMLWriter::endAttribute() */` |
|      8 |  810 | `static int XwEndAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  811 | `{` |
|      9 |  812 | `	return XwStatus(pCtx,xmlTextWriterEndAttribute(pCall->pXw->pWriter));` |
|      1 |  813 | `}` |
|      7 |  814 | `XW_METHOD(vm_builtin_xw_end_attribute,XwEndAttribute,"endAttribute",0)` |
|      - |  815 |  |
|      - |  816 | `/*` |
|      - |  817 | ` * bool XMLWriter::startAttributeNs(?string $prefix, string $name, ?string $namespace)` |
|      - |  818 | ` *` |
|      - |  819 | ` * Only the local NAME is validated -- php checks neither the prefix nor the` |
|      - |  820 | `` * namespace, so a prefix with a space in it reaches the document (`<x y:e`),`` |
|      - |  821 | ` * which is libxml's answer and therefore php's.` |
|      - |  822 | ` */` |
|      4 |  823 | `static int XwStartAttributeNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  824 | `{` |
|      5 |  825 | `	const char *zName = XwStr(pCall,1);` |
|      5 |  826 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      5 |  827 | `	if( rc != 0 ){` |
|      3 |  828 | `		return rc;` |
|      - |  829 | `	}` |
|      4 |  830 | `	return XwStatus(pCtx,xmlTextWriterStartAttributeNS(pCall->pXw->pWriter,` |
|      2 |  831 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  832 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|      3 |  833 | `}` |
|      5 |  834 | `XW_METHOD(vm_builtin_xw_start_attribute_ns,XwStartAttributeNs,"startAttributeNs","#3 ($namespace)")` |
|      - |  835 |  |
|      - |  836 | `/* bool XMLWriter::writeAttributeNs(?string $prefix, string $name, ?string $namespace, string $value) */` |
|      4 |  837 | `static int XwWriteAttributeNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  838 | `{` |
|      5 |  839 | `	const char *zName = XwStr(pCall,1);` |
|      5 |  840 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      5 |  841 | `	if( rc != 0 ){` |
|      3 |  842 | `		return rc;` |
|      - |  843 | `	}` |
|      4 |  844 | `	return XwStatus(pCtx,xmlTextWriterWriteAttributeNS(pCall->pXw->pWriter,` |
|      2 |  845 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  846 | `		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStr(pCall,3)));` |
|      3 |  847 | `}` |
|      5 |  848 | `XW_METHOD(vm_builtin_xw_write_attribute_ns,XwWriteAttributeNs,"writeAttributeNs","#3 ($namespace)")` |
|      - |  849 |  |
|      - |  850 | `/* bool XMLWriter::startElement(string $name) */` |
|     46 |  851 | `static int XwStartElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  852 | `{` |
|     47 |  853 | `	const char *zName = XwStr(pCall,0);` |
|     47 |  854 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     47 |  855 | `	if( rc != 0 ){` |
|      9 |  856 | `		return rc;` |
|      - |  857 | `	}` |
|     39 |  858 | `	return XwStatus(pCtx,xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|     24 |  859 | `}` |
|     45 |  860 | `XW_METHOD(vm_builtin_xw_start_element,XwStartElement,"startElement","#2")` |
|      - |  861 |  |
|      - |  862 | `/* bool XMLWriter::endElement() */` |
|     46 |  863 | `static int XwEndElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  864 | `{` |
|     47 |  865 | `	return XwStatus(pCtx,xmlTextWriterEndElement(pCall->pXw->pWriter));` |
|      1 |  866 | `}` |
|     41 |  867 | `XW_METHOD(vm_builtin_xw_end_element,XwEndElement,"endElement",0)` |
|      - |  868 |  |
|      - |  869 | `/* bool XMLWriter::fullEndElement() */` |
|    ! 0 |  870 | `static int XwFullEndElement(ph7_context *pCtx,xw_call *pCall)` |
|    ! 0 |  871 | `{` |
|    ! 0 |  872 | `	return XwStatus(pCtx,xmlTextWriterFullEndElement(pCall->pXw->pWriter));` |
|    ! 0 |  873 | `}` |
|    ! 0 |  874 | `XW_METHOD(vm_builtin_xw_full_end_element,XwFullEndElement,"fullEndElement",0)` |
|      - |  875 |  |
|      - |  876 | `/* bool XMLWriter::writeAttribute(string $name, string $value) */` |
|     16 |  877 | `static int XwWriteAttribute(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  878 | `{` |
|     17 |  879 | `	const char *zName = XwStr(pCall,0);` |
|     17 |  880 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|     17 |  881 | `	if( rc != 0 ){` |
|      3 |  882 | `		return rc;` |
|      - |  883 | `	}` |
|     22 |  884 | `	return XwStatus(pCtx,xmlTextWriterWriteAttribute(pCall->pXw->pWriter,` |
|     14 |  885 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      9 |  886 | `}` |
|     15 |  887 | `XW_METHOD(vm_builtin_xw_write_attribute,XwWriteAttribute,"writeAttribute","#2 ($value)")` |
|      - |  888 |  |
|      - |  889 | `/* bool XMLWriter::writeElement(string $name, ?string $content = null) */` |
|     28 |  890 | `static int XwWriteElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  891 | `{` |
|     29 |  892 | `	const char *zName = XwStr(pCall,0);` |
|     29 |  893 | `	const char *zContent = XwStrOrNull(pCall,1);` |
|     29 |  894 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     29 |  895 | `	if( rc != 0 ){` |
|      5 |  896 | `		return rc;` |
|      - |  897 | `	}` |
|     25 |  898 | `	if( zContent ){` |
|     37 |  899 | `		rc = xmlTextWriterWriteElement(pCall->pXw->pWriter,(const xmlChar *)zName,` |
|     12 |  900 | `			(const xmlChar *)zContent);` |
|     13 |  901 | `	}else{` |
|      - |  902 | `		/* Empty element: start + end so it serializes as <name/> */` |
|    ! 0 |  903 | `		rc = xmlTextWriterStartElement(pCall->pXw->pWriter,(const xmlChar *)zName);` |
|    ! 0 |  904 | `		if( rc >= 0 ){` |
|    ! 0 |  905 | `			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);` |
|    ! 0 |  906 | `		}` |
|      - |  907 | `	}` |
|     25 |  908 | `	return XwStatus(pCtx,rc);` |
|     15 |  909 | `}` |
|     25 |  910 | `XW_METHOD(vm_builtin_xw_write_element,XwWriteElement,"writeElement","#2 ($content)")` |
|      - |  911 |  |
|      - |  912 | `/*` |
|      - |  913 | ` * bool XMLWriter::startElementNs(?string $prefix, string $name, ?string $namespace)` |
|      - |  914 | ` *` |
|      - |  915 | ` * libxml declares the namespace on the element it opens, so a null $namespace` |
|      - |  916 | ` * writes the prefixed name alone -- which is how a document declares a prefix` |
|      - |  917 | ` * once at the root and uses it below.` |
|      - |  918 | ` */` |
|     20 |  919 | `static int XwStartElementNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  920 | `{` |
|     21 |  921 | `	const char *zName = XwStr(pCall,1);` |
|     21 |  922 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     21 |  923 | `	if( rc != 0 ){` |
|      5 |  924 | `		return rc;` |
|      - |  925 | `	}` |
|     25 |  926 | `	return XwStatus(pCtx,xmlTextWriterStartElementNS(pCall->pXw->pWriter,` |
|     16 |  927 | `		(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|     16 |  928 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|     11 |  929 | `}` |
|     17 |  930 | `XW_METHOD(vm_builtin_xw_start_element_ns,XwStartElementNs,"startElementNs","#3 ($namespace)")` |
|      - |  931 |  |
|      - |  932 | `/* bool XMLWriter::writeElementNs(?string $prefix, string $name, ?string $namespace, ?string $content = null) */` |
|      8 |  933 | `static int XwWriteElementNs(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  934 | `{` |
|      9 |  935 | `	const char *zName = XwStr(pCall,1);` |
|      9 |  936 | `	const char *zContent = XwStrOrNull(pCall,3);` |
|      9 |  937 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      9 |  938 | `	if( rc != 0 ){` |
|      3 |  939 | `		return rc;` |
|      - |  940 | `	}` |
|      7 |  941 | `	if( zContent ){` |
|      7 |  942 | `		rc = xmlTextWriterWriteElementNS(pCall->pXw->pWriter,` |
|      4 |  943 | `			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      4 |  944 | `			(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)zContent);` |
|      3 |  945 | `	}else{` |
|      - |  946 | ``		/* No content: the empty element php writes, `<p:e xmlns:p="urn"/>` */`` |
|      4 |  947 | `		rc = xmlTextWriterStartElementNS(pCall->pXw->pWriter,` |
|      2 |  948 | `			(const xmlChar *)XwStrOrNull(pCall,0),(const xmlChar *)zName,` |
|      2 |  949 | `			(const xmlChar *)XwStrOrNull(pCall,2));` |
|      3 |  950 | `		if( rc >= 0 ){` |
|      3 |  951 | `			rc = xmlTextWriterEndElement(pCall->pXw->pWriter);` |
|      1 |  952 | `		}` |
|      - |  953 | `	}` |
|      7 |  954 | `	return XwStatus(pCtx,rc);` |
|      5 |  955 | `}` |
|      9 |  956 | `XW_METHOD(vm_builtin_xw_write_element_ns,XwWriteElementNs,"writeElementNs","#3 ($namespace)")` |
|      - |  957 |  |
|      - |  958 | `/*` |
|      - |  959 | ` * bool XMLWriter::startPi(string $target)` |
|      - |  960 | ` *` |
|      - |  961 | ` * The target is checked with the same xmlValidateName the element and attribute` |
|      - |  962 | ` * names go through -- php names it a "PI target" and nothing else changes, so` |
|      - |  963 | `` * `<?php ... ?>` is spellable and `<?x y ... ?>` is a ValueError.`` |
|      - |  964 | ` */` |
|      8 |  965 | `static int XwStartPi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  966 | `{` |
|      9 |  967 | `	const char *zTarget = XwStr(pCall,0);` |
|      9 |  968 | `	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");` |
|      9 |  969 | `	if( rc != 0 ){` |
|      5 |  970 | `		return rc;` |
|      - |  971 | `	}` |
|      5 |  972 | `	return XwStatus(pCtx,xmlTextWriterStartPI(pCall->pXw->pWriter,(const xmlChar *)zTarget));` |
|      5 |  973 | `}` |
|      7 |  974 | `XW_METHOD(vm_builtin_xw_start_pi,XwStartPi,"startPi","#2")` |
|      - |  975 |  |
|      - |  976 | `/* bool XMLWriter::endPi() */` |
|      4 |  977 | `static int XwEndPi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  978 | `{` |
|      5 |  979 | `	return XwStatus(pCtx,xmlTextWriterEndPI(pCall->pXw->pWriter));` |
|      1 |  980 | `}` |
|      5 |  981 | `XW_METHOD(vm_builtin_xw_end_pi,XwEndPi,"endPi",0)` |
|      - |  982 |  |
|      - |  983 | `/* bool XMLWriter::writePi(string $target, string $content) */` |
|      8 |  984 | `static int XwWritePi(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  985 | `{` |
|      9 |  986 | `	const char *zTarget = XwStr(pCall,0);` |
|      9 |  987 | `	int rc = XmlWriterCheckName(pCtx,pCall,zTarget,"PI target");` |
|      9 |  988 | `	if( rc != 0 ){` |
|      3 |  989 | `		return rc;` |
|      - |  990 | `	}` |
|     10 |  991 | `	return XwStatus(pCtx,xmlTextWriterWritePI(pCall->pXw->pWriter,` |
|      6 |  992 | `		(const xmlChar *)zTarget,(const xmlChar *)XwStr(pCall,1)));` |
|      5 |  993 | `}` |
|      7 |  994 | `XW_METHOD(vm_builtin_xw_write_pi,XwWritePi,"writePi","#2 ($content)")` |
|      - |  995 |  |
|      - |  996 | `/* bool XMLWriter::startCdata() */` |
|      6 |  997 | `static int XwStartCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 |  998 | `{` |
|      7 |  999 | `	return XwStatus(pCtx,xmlTextWriterStartCDATA(pCall->pXw->pWriter));` |
|      1 | 1000 | `}` |
|      7 | 1001 | `XW_METHOD(vm_builtin_xw_start_cdata,XwStartCdata,"startCdata",0)` |
|      - | 1002 |  |
|      - | 1003 | `/* bool XMLWriter::endCdata() */` |
|      6 | 1004 | `static int XwEndCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1005 | `{` |
|      7 | 1006 | `	return XwStatus(pCtx,xmlTextWriterEndCDATA(pCall->pXw->pWriter));` |
|      1 | 1007 | `}` |
|      7 | 1008 | `XW_METHOD(vm_builtin_xw_end_cdata,XwEndCdata,"endCdata",0)` |
|      - | 1009 |  |
|      - | 1010 | `/* bool XMLWriter::text(string $content) */` |
|     30 | 1011 | `static int XwText(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1012 | `{` |
|     46 | 1013 | `	return XwStatus(pCtx,xmlTextWriterWriteString(pCall->pXw->pWriter,` |
|     30 | 1014 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1015 | `}` |
|     31 | 1016 | `XW_METHOD(vm_builtin_xw_text,XwText,"text",0)` |
|      - | 1017 |  |
|      - | 1018 | `/* bool XMLWriter::writeRaw(string $content) */` |
|    ! 0 | 1019 | `static int XwWriteRaw(ph7_context *pCtx,xw_call *pCall)` |
|    ! 0 | 1020 | `{` |
|    ! 0 | 1021 | `	return XwStatus(pCtx,xmlTextWriterWriteRaw(pCall->pXw->pWriter,` |
|    ! 0 | 1022 | `		(const xmlChar *)XwStr(pCall,0)));` |
|    ! 0 | 1023 | `}` |
|    ! 0 | 1024 | `XW_METHOD(vm_builtin_xw_write_raw,XwWriteRaw,"writeRaw",0)` |
|      - | 1025 |  |
|      - | 1026 | `/* bool XMLWriter::writeCdata(string $content) */` |
|      6 | 1027 | `static int XwWriteCdata(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1028 | `{` |
|     10 | 1029 | `	return XwStatus(pCtx,xmlTextWriterWriteCDATA(pCall->pXw->pWriter,` |
|      6 | 1030 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1031 | `}` |
|      5 | 1032 | `XW_METHOD(vm_builtin_xw_write_cdata,XwWriteCdata,"writeCdata",0)` |
|      - | 1033 |  |
|      - | 1034 | `/* bool XMLWriter::writeComment(string $content) */` |
|      4 | 1035 | `static int XwWriteComment(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1036 | `{` |
|      7 | 1037 | `	return XwStatus(pCtx,xmlTextWriterWriteComment(pCall->pXw->pWriter,` |
|      4 | 1038 | `		(const xmlChar *)XwStr(pCall,0)));` |
|      1 | 1039 | `}` |
|      5 | 1040 | `XW_METHOD(vm_builtin_xw_write_comment,XwWriteComment,"writeComment",0)` |
|      - | 1041 |  |
|      - | 1042 | `/*` |
|      - | 1043 | ` * The DTD twelve.` |
|      - | 1044 | ` *` |
|      - | 1045 | ` * libxml decides the shape of every one of these, and two of its decisions are` |
|      - | 1046 | ` * only reported through the error handler: a DOCTYPE with a public identifier` |
|      - | 1047 | ` * and no system one ("system identifier needed!"), and a DTD opened once the` |
|      - | 1048 | ` * root element has been written ("DTD allowed only in prolog!"). Both come out` |
|      - | 1049 | ` * as php's warning through the capture window in XwRun.` |
|      - | 1050 | ` *` |
|      - | 1051 | ` * php validates the name of the internal-subset declarations and NOT the` |
|      - | 1052 | ``  * DOCTYPE's own qualified name, so `startDtd('x y')` writes `<!DOCTYPE x y` `` |
|      - | 1053 | `` * while `startDtdElement('x y')` is a ValueError -- and the ValueError for`` |
|      - | 1054 | ` * startDtdEntity says "attribute name" where its neighbours say "element name",` |
|      - | 1055 | ` * because php reaches for a different macro there.` |
|      - | 1056 | ` */` |
|      - | 1057 | `/* bool XMLWriter::startDtd(string $qualifiedName, ?string $publicId, ?string $systemId) */` |
|     12 | 1058 | `static int XwStartDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1059 | `{` |
|     19 | 1060 | `	return XwStatus(pCtx,xmlTextWriterStartDTD(pCall->pXw->pWriter,` |
|     12 | 1061 | `		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),` |
|     12 | 1062 | `		(const xmlChar *)XwStrOrNull(pCall,2)));` |
|      1 | 1063 | `}` |
|     11 | 1064 | `XW_METHOD(vm_builtin_xw_start_dtd,XwStartDtd,"startDtd",0)` |
|      - | 1065 |  |
|      - | 1066 | `/* bool XMLWriter::endDtd() */` |
|      6 | 1067 | `static int XwEndDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1068 | `{` |
|      7 | 1069 | `	return XwStatus(pCtx,xmlTextWriterEndDTD(pCall->pXw->pWriter));` |
|      1 | 1070 | `}` |
|      5 | 1071 | `XW_METHOD(vm_builtin_xw_end_dtd,XwEndDtd,"endDtd",0)` |
|      - | 1072 |  |
|      - | 1073 | `/* bool XMLWriter::writeDtd(string $name, ?string $publicId, ?string $systemId, ?string $content) */` |
|      2 | 1074 | `static int XwWriteDtd(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1075 | `{` |
|      4 | 1076 | `	return XwStatus(pCtx,xmlTextWriterWriteDTD(pCall->pXw->pWriter,` |
|      2 | 1077 | `		(const xmlChar *)XwStr(pCall,0),(const xmlChar *)XwStrOrNull(pCall,1),` |
|      2 | 1078 | `		(const xmlChar *)XwStrOrNull(pCall,2),(const xmlChar *)XwStrOrNull(pCall,3)));` |
|      1 | 1079 | `}` |
|      3 | 1080 | `XW_METHOD(vm_builtin_xw_write_dtd,XwWriteDtd,"writeDtd",0)` |
|      - | 1081 |  |
|      - | 1082 | `/* bool XMLWriter::startDtdElement(string $qualifiedName) */` |
|      6 | 1083 | `static int XwStartDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1084 | `{` |
|      7 | 1085 | `	const char *zName = XwStr(pCall,0);` |
|      7 | 1086 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      7 | 1087 | `	if( rc != 0 ){` |
|      5 | 1088 | `		return rc;` |
|      - | 1089 | `	}` |
|      3 | 1090 | `	return XwStatus(pCtx,xmlTextWriterStartDTDElement(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      4 | 1091 | `}` |
|      5 | 1092 | `XW_METHOD(vm_builtin_xw_start_dtd_element,XwStartDtdElement,"startDtdElement","#2")` |
|      - | 1093 |  |
|      - | 1094 | `/* bool XMLWriter::endDtdElement() */` |
|      4 | 1095 | `static int XwEndDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1096 | `{` |
|      5 | 1097 | `	return XwStatus(pCtx,xmlTextWriterEndDTDElement(pCall->pXw->pWriter));` |
|      1 | 1098 | `}` |
|      5 | 1099 | `XW_METHOD(vm_builtin_xw_end_dtd_element,XwEndDtdElement,"endDtdElement",0)` |
|      - | 1100 |  |
|      - | 1101 | `/* bool XMLWriter::writeDtdElement(string $name, string $content) */` |
|      4 | 1102 | `static int XwWriteDtdElement(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1103 | `{` |
|      5 | 1104 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1105 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1106 | `	if( rc != 0 ){` |
|      3 | 1107 | `		return rc;` |
|      - | 1108 | `	}` |
|      4 | 1109 | `	return XwStatus(pCtx,xmlTextWriterWriteDTDElement(pCall->pXw->pWriter,` |
|      2 | 1110 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      3 | 1111 | `}` |
|      5 | 1112 | `XW_METHOD(vm_builtin_xw_write_dtd_element,XwWriteDtdElement,"writeDtdElement","#2 ($content)")` |
|      - | 1113 |  |
|      - | 1114 | `/* bool XMLWriter::startDtdAttlist(string $name) */` |
|      4 | 1115 | `static int XwStartDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1116 | `{` |
|      5 | 1117 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1118 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1119 | `	if( rc != 0 ){` |
|      3 | 1120 | `		return rc;` |
|      - | 1121 | `	}` |
|      3 | 1122 | `	return XwStatus(pCtx,xmlTextWriterStartDTDAttlist(pCall->pXw->pWriter,(const xmlChar *)zName));` |
|      3 | 1123 | `}` |
|      5 | 1124 | `XW_METHOD(vm_builtin_xw_start_dtd_attlist,XwStartDtdAttlist,"startDtdAttlist","#2")` |
|      - | 1125 |  |
|      - | 1126 | `/* bool XMLWriter::endDtdAttlist() */` |
|      4 | 1127 | `static int XwEndDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1128 | `{` |
|      5 | 1129 | `	return XwStatus(pCtx,xmlTextWriterEndDTDAttlist(pCall->pXw->pWriter));` |
|      1 | 1130 | `}` |
|      5 | 1131 | `XW_METHOD(vm_builtin_xw_end_dtd_attlist,XwEndDtdAttlist,"endDtdAttlist",0)` |
|      - | 1132 |  |
|      - | 1133 | `/* bool XMLWriter::writeDtdAttlist(string $name, string $content) */` |
|      4 | 1134 | `static int XwWriteDtdAttlist(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1135 | `{` |
|      5 | 1136 | `	const char *zName = XwStr(pCall,0);` |
|      5 | 1137 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|      5 | 1138 | `	if( rc != 0 ){` |
|      3 | 1139 | `		return rc;` |
|      - | 1140 | `	}` |
|      4 | 1141 | `	return XwStatus(pCtx,xmlTextWriterWriteDTDAttlist(pCall->pXw->pWriter,` |
|      2 | 1142 | `		(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1)));` |
|      3 | 1143 | `}` |
|      5 | 1144 | `XW_METHOD(vm_builtin_xw_write_dtd_attlist,XwWriteDtdAttlist,"writeDtdAttlist","#2 ($content)")` |
|      - | 1145 |  |
|      - | 1146 | `/* bool XMLWriter::startDtdEntity(string $name, bool $isParam) */` |
|      6 | 1147 | `static int XwStartDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1148 | `{` |
|      7 | 1149 | `	const char *zName = XwStr(pCall,0);` |
|      7 | 1150 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"attribute name");` |
|      7 | 1151 | `	if( rc != 0 ){` |
|      5 | 1152 | `		return rc;` |
|      - | 1153 | `	}` |
|      4 | 1154 | `	return XwStatus(pCtx,xmlTextWriterStartDTDEntity(pCall->pXw->pWriter,` |
|      1 | 1155 | `		XwBool(pCall,1,0),(const xmlChar *)zName));` |
|      4 | 1156 | `}` |
|      5 | 1157 | `XW_METHOD(vm_builtin_xw_start_dtd_entity,XwStartDtdEntity,"startDtdEntity","#2 ($isParam)")` |
|      - | 1158 |  |
|      - | 1159 | `/* bool XMLWriter::endDtdEntity() */` |
|      4 | 1160 | `static int XwEndDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1161 | `{` |
|      5 | 1162 | `	return XwStatus(pCtx,xmlTextWriterEndDTDEntity(pCall->pXw->pWriter));` |
|      1 | 1163 | `}` |
|      5 | 1164 | `XW_METHOD(vm_builtin_xw_end_dtd_entity,XwEndDtdEntity,"endDtdEntity",0)` |
|      - | 1165 |  |
|      - | 1166 | `/*` |
|      - | 1167 | ` * bool XMLWriter::writeDtdEntity(string $name, string $content, bool $isParam = false,` |
|      - | 1168 | ` *                                ?string $publicId = null, ?string $systemId = null,` |
|      - | 1169 | ` *                                ?string $notationData = null)` |
|      - | 1170 | ` *` |
|      - | 1171 | ` * php has two calls behind this one name, and picks by whether a public or` |
|      - | 1172 | ` * system identifier is there: with neither it writes the INTERNAL entity (the` |
|      - | 1173 | ` * $content), and with either it writes the EXTERNAL declaration -- where` |
|      - | 1174 | ` * $content is not written at all. $notationData does NOT decide, so a call that` |
|      - | 1175 | ` * names only a notation is still the internal entity and the notation is` |
|      - | 1176 | ` * dropped, which is php's answer and not an oversight of this port.` |
|      - | 1177 | ` */` |
|     22 | 1178 | `static int XwWriteDtdEntity(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1179 | `{` |
|     23 | 1180 | `	const char *zName = XwStr(pCall,0);` |
|     23 | 1181 | `	const char *zPub = XwStrOrNull(pCall,3);` |
|     23 | 1182 | `	const char *zSys = XwStrOrNull(pCall,4);` |
|     23 | 1183 | `	const char *zNdata = XwStrOrNull(pCall,5);` |
|     23 | 1184 | `	int rc = XmlWriterCheckName(pCtx,pCall,zName,"element name");` |
|     23 | 1185 | `	if( rc != 0 ){` |
|      3 | 1186 | `		return rc;` |
|      - | 1187 | `	}` |
|     21 | 1188 | `	if( zPub == 0 && zSys == 0 ){` |
|     19 | 1189 | `		rc = xmlTextWriterWriteDTDInternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),` |
|     12 | 1190 | `			(const xmlChar *)zName,(const xmlChar *)XwStr(pCall,1));` |
|      7 | 1191 | `	}else{` |
|     13 | 1192 | `		rc = xmlTextWriterWriteDTDExternalEntity(pCall->pXw->pWriter,XwBool(pCall,2,0),` |
|      4 | 1193 | `			(const xmlChar *)zName,(const xmlChar *)zPub,(const xmlChar *)zSys,` |
|      4 | 1194 | `			(const xmlChar *)zNdata);` |
|      - | 1195 | `	}` |
|     21 | 1196 | `	return XwStatus(pCtx,rc);` |
|     12 | 1197 | `}` |
|     21 | 1198 | `XW_METHOD(vm_builtin_xw_write_dtd_entity,XwWriteDtdEntity,"writeDtdEntity","#2 ($content)")` |
|      - | 1199 |  |
|      - | 1200 | `/* string XMLWriter::outputMemory(bool $flush = true) -- read the buffer back */` |
|     78 | 1201 | `static int XwOutputMemory(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1202 | `{` |
|     79 | 1203 | `	phl_xmlwriter *pXw = pCall->pXw;` |
|     79 | 1204 | `	int bFlush = XwBool(pCall,0,1);` |
|     79 | 1205 | `	if( pXw->pBuf == 0 ){` |
|      - | 1206 | `		/* Not an in-memory writer: php answers the empty string */` |
|      3 | 1207 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1208 | `		return PH7_OK;` |
|      - | 1209 | `	}` |
|      - | 1210 | `	/* Flush the writer into the buffer before reading (php does this) */` |
|     77 | 1211 | `	xmlTextWriterFlush(pXw->pWriter);` |
|     77 | 1212 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|     77 | 1213 | `	if( bFlush ){` |
|     75 | 1214 | `		xmlBufferEmpty(pXw->pBuf);` |
|     37 | 1215 | `	}` |
|     77 | 1216 | `	return PH7_OK;` |
|     40 | 1217 | `}` |
|     77 | 1218 | `XW_METHOD(vm_builtin_xw_output_memory,XwOutputMemory,"outputMemory",0)` |
|      - | 1219 |  |
|      - | 1220 | `/* string\|int XMLWriter::flush(bool $empty = true) */` |
|     10 | 1221 | `static int XwFlush(ph7_context *pCtx,xw_call *pCall)` |
|      1 | 1222 | `{` |
|     11 | 1223 | `	phl_xmlwriter *pXw = pCall->pXw;` |
|     11 | 1224 | `	int bEmpty = XwBool(pCall,0,1);` |
|     11 | 1225 | `	int nOut = xmlTextWriterFlush(pXw->pWriter);` |
|     11 | 1226 | `	if( pXw->pBuf ){` |
|      - | 1227 | `		/* Memory writer: php returns the buffer as a string from flush() */` |
|    ! 0 | 1228 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pXw->pBuf),(int)xmlBufferLength(pXw->pBuf));` |
|    ! 0 | 1229 | `		if( bEmpty ){` |
|    ! 0 | 1230 | `			xmlBufferEmpty(pXw->pBuf);` |
|    ! 0 | 1231 | `		}` |
|    ! 0 | 1232 | `	}else{` |
|     11 | 1233 | `		ph7_result_int(pCtx,nOut);` |
|      - | 1234 | `	}` |
|     11 | 1235 | `	return PH7_OK;` |
|      1 | 1236 | `}` |
|     11 | 1237 | `XW_METHOD(vm_builtin_xw_flush,XwFlush,"flush",0)` |
|      - | 1238 |  |
|      - | 1239 | `/*` |
|      - | 1240 | ` * The procedural surface. php's ext/xmlwriter presents every verb twice, and` |
|      - | 1241 | ` * the function spelling is the ORIGINAL one -- the class arrived in 5.1.2 --` |
|      - | 1242 | ` * so a program written against it is not using an alias for the method but the` |
|      - | 1243 | ` * name the extension was documented under. Each entry drives the same verb one` |
|      - | 1244 | ` * argument along, and states its OWN name-argument text: procedural numbering` |
|      - | 1245 | ` * is what php's macro reports, so here it names the real parameter.` |
|      - | 1246 | ` */` |
|      3 | 1247 | `XW_FUNCTION(vm_builtin_xmlwriter_set_indent,XwSetIndent,"xmlwriter_set_indent",0)` |
|    ! 0 | 1248 | `XW_FUNCTION(vm_builtin_xmlwriter_set_indent_string,XwSetIndentString,"xmlwriter_set_indent_string",0)` |
|    ! 0 | 1249 | `XW_FUNCTION(vm_builtin_xmlwriter_start_comment,XwStartComment,"xmlwriter_start_comment",0)` |
|    ! 0 | 1250 | `XW_FUNCTION(vm_builtin_xmlwriter_end_comment,XwEndComment,"xmlwriter_end_comment",0)` |
|      3 | 1251 | `XW_FUNCTION(vm_builtin_xmlwriter_start_attribute,XwStartAttribute,"xmlwriter_start_attribute","#2 ($name)")` |
|      3 | 1252 | `XW_FUNCTION(vm_builtin_xmlwriter_end_attribute,XwEndAttribute,"xmlwriter_end_attribute",0)` |
|      3 | 1253 | `XW_FUNCTION(vm_builtin_xmlwriter_write_attribute,XwWriteAttribute,"xmlwriter_write_attribute","#2 ($name)")` |
|    ! 0 | 1254 | `XW_FUNCTION(vm_builtin_xmlwriter_start_attribute_ns,XwStartAttributeNs,"xmlwriter_start_attribute_ns","#3 ($name)")` |
|    ! 0 | 1255 | `XW_FUNCTION(vm_builtin_xmlwriter_write_attribute_ns,XwWriteAttributeNs,"xmlwriter_write_attribute_ns","#3 ($name)")` |
|      7 | 1256 | `XW_FUNCTION(vm_builtin_xmlwriter_start_element,XwStartElement,"xmlwriter_start_element","#2 ($name)")` |
|      7 | 1257 | `XW_FUNCTION(vm_builtin_xmlwriter_end_element,XwEndElement,"xmlwriter_end_element",0)` |
|    ! 0 | 1258 | `XW_FUNCTION(vm_builtin_xmlwriter_full_end_element,XwFullEndElement,"xmlwriter_full_end_element",0)` |
|      5 | 1259 | `XW_FUNCTION(vm_builtin_xmlwriter_start_element_ns,XwStartElementNs,"xmlwriter_start_element_ns","#3 ($name)")` |
|      7 | 1260 | `XW_FUNCTION(vm_builtin_xmlwriter_write_element,XwWriteElement,"xmlwriter_write_element","#2 ($name)")` |
|    ! 0 | 1261 | `XW_FUNCTION(vm_builtin_xmlwriter_write_element_ns,XwWriteElementNs,"xmlwriter_write_element_ns","#3 ($name)")` |
|      3 | 1262 | `XW_FUNCTION(vm_builtin_xmlwriter_start_pi,XwStartPi,"xmlwriter_start_pi","#2 ($target)")` |
|    ! 0 | 1263 | `XW_FUNCTION(vm_builtin_xmlwriter_end_pi,XwEndPi,"xmlwriter_end_pi",0)` |
|      3 | 1264 | `XW_FUNCTION(vm_builtin_xmlwriter_write_pi,XwWritePi,"xmlwriter_write_pi","#2 ($target)")` |
|    ! 0 | 1265 | `XW_FUNCTION(vm_builtin_xmlwriter_start_cdata,XwStartCdata,"xmlwriter_start_cdata",0)` |
|    ! 0 | 1266 | `XW_FUNCTION(vm_builtin_xmlwriter_end_cdata,XwEndCdata,"xmlwriter_end_cdata",0)` |
|      3 | 1267 | `XW_FUNCTION(vm_builtin_xmlwriter_write_cdata,XwWriteCdata,"xmlwriter_write_cdata",0)` |
|      3 | 1268 | `XW_FUNCTION(vm_builtin_xmlwriter_text,XwText,"xmlwriter_text",0)` |
|    ! 0 | 1269 | `XW_FUNCTION(vm_builtin_xmlwriter_write_raw,XwWriteRaw,"xmlwriter_write_raw",0)` |
|      3 | 1270 | `XW_FUNCTION(vm_builtin_xmlwriter_start_document,XwStartDocument,"xmlwriter_start_document",0)` |
|      3 | 1271 | `XW_FUNCTION(vm_builtin_xmlwriter_end_document,XwEndDocument,"xmlwriter_end_document",0)` |
|    ! 0 | 1272 | `XW_FUNCTION(vm_builtin_xmlwriter_write_comment,XwWriteComment,"xmlwriter_write_comment",0)` |
|      3 | 1273 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd,XwStartDtd,"xmlwriter_start_dtd",0)` |
|      3 | 1274 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd,XwEndDtd,"xmlwriter_end_dtd",0)` |
|    ! 0 | 1275 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd,XwWriteDtd,"xmlwriter_write_dtd",0)` |
|      3 | 1276 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_element,XwStartDtdElement,"xmlwriter_start_dtd_element","#2 ($qualifiedName)")` |
|    ! 0 | 1277 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_element,XwEndDtdElement,"xmlwriter_end_dtd_element",0)` |
|    ! 0 | 1278 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_element,XwWriteDtdElement,"xmlwriter_write_dtd_element","#2 ($name)")` |
|    ! 0 | 1279 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_attlist,XwStartDtdAttlist,"xmlwriter_start_dtd_attlist","#2 ($name)")` |
|    ! 0 | 1280 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_attlist,XwEndDtdAttlist,"xmlwriter_end_dtd_attlist",0)` |
|    ! 0 | 1281 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_attlist,XwWriteDtdAttlist,"xmlwriter_write_dtd_attlist","#2 ($name)")` |
|      3 | 1282 | `XW_FUNCTION(vm_builtin_xmlwriter_start_dtd_entity,XwStartDtdEntity,"xmlwriter_start_dtd_entity","#2 ($name)")` |
|    ! 0 | 1283 | `XW_FUNCTION(vm_builtin_xmlwriter_end_dtd_entity,XwEndDtdEntity,"xmlwriter_end_dtd_entity",0)` |
|      3 | 1284 | `XW_FUNCTION(vm_builtin_xmlwriter_write_dtd_entity,XwWriteDtdEntity,"xmlwriter_write_dtd_entity","#2 ($name)")` |
|      5 | 1285 | `XW_FUNCTION(vm_builtin_xmlwriter_output_memory,XwOutputMemory,"xmlwriter_output_memory",0)` |
|      3 | 1286 | `XW_FUNCTION(vm_builtin_xmlwriter_flush,XwFlush,"xmlwriter_flush",0)` |
|      - | 1287 |  |
|      - | 1288 | `/* XMLWriter\|false xmlwriter_open_memory() */` |
|      4 | 1289 | `static int vm_builtin_xmlwriter_open_memory(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1290 | `{` |
|      - | 1291 | `	phl_xmlwriter *pXw;` |
|      2 | 1292 | `	SXUNUSED(nArg);` |
|      2 | 1293 | `	SXUNUSED(apArg);` |
|      5 | 1294 | `	pXw = XmlWriterOpenMemory(pCtx->pVm);` |
|      5 | 1295 | `	if( pXw == 0 ){` |
|    ! 0 | 1296 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1297 | `		return PH7_OK;` |
|      - | 1298 | `	}` |
|      5 | 1299 | `	return XmlWriterFactory(pCtx,pXw);` |
|      3 | 1300 | `}` |
|      - | 1301 | `/* XMLWriter\|false xmlwriter_open_uri(string $uri) */` |
|      4 | 1302 | `static int vm_builtin_xmlwriter_open_uri(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1303 | `{` |
|      5 | 1304 | `	int rc = PH7_OK;` |
|      5 | 1305 | `	phl_xmlwriter *pXw = XmlWriterOpenUri(pCtx,nArg > 0 ? apArg[0] : 0,"xmlwriter_open_uri",FALSE,&rc);` |
|      5 | 1306 | `	if( pXw == 0 ){` |
|      3 | 1307 | `		if( rc != PH7_OK ){` |
|      3 | 1308 | `			return rc;` |
|      - | 1309 | `		}` |
|    ! 0 | 1310 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1311 | `		return PH7_OK;` |
|      - | 1312 | `	}` |
|      3 | 1313 | `	return XmlWriterFactory(pCtx,pXw);` |
|      3 | 1314 | `}` |
|      - | 1315 |  |
|      - | 1316 | `/* XMLWriter is declared entirely from C by PH7_VmInstallXmlWriter below. It was` |
|      - | 1317 | ` * an embedded PHP class whose every method forwarded to a global __xw_ thunk. */` |
|      - | 1318 |  |
|      - | 1319 | `/*` |
|      - | 1320 | ` * Install the XMLWriter library.  Called from PH7_VmInit inside the` |
|      - | 1321 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml.` |
|      - | 1322 | ` */` |
|   5254 | 1323 | `PH7_PRIVATE sxi32 PH7_VmInstallXmlWriter(ph7_vm *pVm)` |
|      5 | 1324 | `{` |
|      - | 1325 | `	/* php's own signatures. Declaring them is what gives these methods argument` |
|      - | 1326 | `	 * coercion and a too-few/too-many ArgumentCountError; the prelude hand-cast` |
|      - | 1327 | `	 * every argument ((string)$name, (bool)$enable) and enforced no arity at all. */` |
|      - | 1328 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 1329 | `		/* Declared in php's own stub order: get_class_methods() and Reflection` |
|      - | 1330 | `		 * both answer declaration order, so the two engines list one surface. */` |
|      - | 1331 | `		{ "openUri",         PH7_MOD_PUBLIC, "string $uri", "@bool", vm_builtin_xw_open_uri },` |
|      - | 1332 | `		{ "toUri",           PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $uri", "static",` |
|      - | 1333 | `		  vm_builtin_xw_to_uri },` |
|      - | 1334 | `		{ "openMemory",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_open_memory },` |
|      - | 1335 | `		{ "toMemory",        PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "", "static", vm_builtin_xw_to_memory },` |
|      - | 1336 | `		{ "toStream",        PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $stream", "static",` |
|      - | 1337 | `		  vm_builtin_xw_to_stream },` |
|      - | 1338 | `		{ "setIndent",       PH7_MOD_PUBLIC, "bool $enable", "@bool", vm_builtin_xw_set_indent },` |
|      - | 1339 | `		{ "setIndentString", PH7_MOD_PUBLIC, "string $indentation", "@bool", vm_builtin_xw_set_indent_string },` |
|      - | 1340 | `		{ "startComment",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_comment },` |
|      - | 1341 | `		{ "endComment",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_comment },` |
|      - | 1342 | `		{ "startAttribute",  PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_attribute },` |
|      - | 1343 | `		{ "endAttribute",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_attribute },` |
|      - | 1344 | `		{ "writeAttribute",  PH7_MOD_PUBLIC, "string $name, string $value", "@bool", vm_builtin_xw_write_attribute },` |
|      - | 1345 | `		{ "startAttributeNs", PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",` |
|      - | 1346 | `		  "@bool", vm_builtin_xw_start_attribute_ns },` |
|      - | 1347 | `		{ "writeAttributeNs", PH7_MOD_PUBLIC,` |
|      - | 1348 | `		  "?string $prefix, string $name, ?string $namespace, string $value",` |
|      - | 1349 | `		  "@bool", vm_builtin_xw_write_attribute_ns },` |
|      - | 1350 | `		{ "startElement",    PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_element },` |
|      - | 1351 | `		{ "endElement",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_element },` |
|      - | 1352 | `		{ "fullEndElement",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_full_end_element },` |
|      - | 1353 | `		{ "startElementNs",  PH7_MOD_PUBLIC, "?string $prefix, string $name, ?string $namespace",` |
|      - | 1354 | `		  "@bool", vm_builtin_xw_start_element_ns },` |
|      - | 1355 | `		{ "writeElement",    PH7_MOD_PUBLIC, "string $name, ?string $content = null", "@bool", vm_builtin_xw_write_element },` |
|      - | 1356 | `		{ "writeElementNs",  PH7_MOD_PUBLIC,` |
|      - | 1357 | `		  "?string $prefix, string $name, ?string $namespace, ?string $content = null",` |
|      - | 1358 | `		  "@bool", vm_builtin_xw_write_element_ns },` |
|      - | 1359 | `		{ "startPi",         PH7_MOD_PUBLIC, "string $target", "@bool", vm_builtin_xw_start_pi },` |
|      - | 1360 | `		{ "endPi",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_pi },` |
|      - | 1361 | `		{ "writePi",         PH7_MOD_PUBLIC, "string $target, string $content", "@bool", vm_builtin_xw_write_pi },` |
|      - | 1362 | `		{ "startCdata",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_start_cdata },` |
|      - | 1363 | `		{ "endCdata",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_cdata },` |
|      - | 1364 | `		{ "writeCdata",      PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_cdata },` |
|      - | 1365 | `		{ "text",            PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_text },` |
|      - | 1366 | `		{ "writeRaw",        PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_raw },` |
|      - | 1367 | `		{ "startDocument",   PH7_MOD_PUBLIC,` |
|      - | 1368 | `		  "?string $version = null, ?string $encoding = null, ?string $standalone = null",` |
|      - | 1369 | `		  "@bool", vm_builtin_xw_start_document },` |
|      - | 1370 | `		{ "endDocument",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_document },` |
|      - | 1371 | `		{ "writeComment",    PH7_MOD_PUBLIC, "string $content", "@bool", vm_builtin_xw_write_comment },` |
|      - | 1372 | `		{ "startDtd",        PH7_MOD_PUBLIC,` |
|      - | 1373 | `		  "string $qualifiedName, ?string $publicId = null, ?string $systemId = null",` |
|      - | 1374 | `		  "@bool", vm_builtin_xw_start_dtd },` |
|      - | 1375 | `		{ "endDtd",          PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd },` |
|      - | 1376 | `		{ "writeDtd",        PH7_MOD_PUBLIC,` |
|      - | 1377 | `		  "string $name, ?string $publicId = null, ?string $systemId = null, ?string $content = null",` |
|      - | 1378 | `		  "@bool", vm_builtin_xw_write_dtd },` |
|      - | 1379 | `		{ "startDtdElement", PH7_MOD_PUBLIC, "string $qualifiedName", "@bool", vm_builtin_xw_start_dtd_element },` |
|      - | 1380 | `		{ "endDtdElement",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_element },` |
|      - | 1381 | `		{ "writeDtdElement", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",` |
|      - | 1382 | `		  vm_builtin_xw_write_dtd_element },` |
|      - | 1383 | `		{ "startDtdAttlist", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_xw_start_dtd_attlist },` |
|      - | 1384 | `		{ "endDtdAttlist",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_attlist },` |
|      - | 1385 | `		{ "writeDtdAttlist", PH7_MOD_PUBLIC, "string $name, string $content", "@bool",` |
|      - | 1386 | `		  vm_builtin_xw_write_dtd_attlist },` |
|      - | 1387 | `		{ "startDtdEntity",  PH7_MOD_PUBLIC, "string $name, bool $isParam", "@bool",` |
|      - | 1388 | `		  vm_builtin_xw_start_dtd_entity },` |
|      - | 1389 | `		{ "endDtdEntity",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_xw_end_dtd_entity },` |
|      - | 1390 | `		{ "writeDtdEntity",  PH7_MOD_PUBLIC,` |
|      - | 1391 | `		  "string $name, string $content, bool $isParam = false, ?string $publicId = null, "` |
|      - | 1392 | `		  "?string $systemId = null, ?string $notationData = null",` |
|      - | 1393 | `		  "@bool", vm_builtin_xw_write_dtd_entity },` |
|      - | 1394 | `		{ "outputMemory",    PH7_MOD_PUBLIC, "bool $flush = true", "@string", vm_builtin_xw_output_memory },` |
|      - | 1395 | `		{ "flush",           PH7_MOD_PUBLIC, "bool $empty = true", "@string\|int", vm_builtin_xw_flush },` |
|      - | 1396 | `	};` |
|      - | 1397 | `	/* The libxml writer handle: storage the class owns, kept public because the` |
|      - | 1398 | `	 * prelude declared it so. */` |
|      - | 1399 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 1400 | `		{ "__res", PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1401 | `	};` |
|      - | 1402 | ``	/* php's XMLWriter has no clone handler, so `clone $w` is the engine's own`` |
|      - | 1403 | `	 * refusal there -- and it has to be one here too: the copy would carry the` |
|      - | 1404 | `	 * SAME libxml writer in its hidden slot, so the two objects would interleave` |
|      - | 1405 | `	 * their output into one document and the copy's own buffer would answer "". */` |
|      - | 1406 | `	/* php's ext/xmlwriter presents every verb under a function name too; each` |
|      - | 1407 | `	 * one drives the very same body through XwCallFromArg. */` |
|      - | 1408 | `	static const struct {` |
|      - | 1409 | `		const char *zName;` |
|      - | 1410 | `		ProchHostFunction xFunc;` |
|      - | 1411 | `	} aFunc[] = {` |
|      - | 1412 | `		{ "xmlwriter_open_uri",    vm_builtin_xmlwriter_open_uri },` |
|      - | 1413 | `		{ "xmlwriter_open_memory", vm_builtin_xmlwriter_open_memory },` |
|      - | 1414 | `		{ "xmlwriter_set_indent", vm_builtin_xmlwriter_set_indent },` |
|      - | 1415 | `		{ "xmlwriter_set_indent_string", vm_builtin_xmlwriter_set_indent_string },` |
|      - | 1416 | `		{ "xmlwriter_start_comment", vm_builtin_xmlwriter_start_comment },` |
|      - | 1417 | `		{ "xmlwriter_end_comment", vm_builtin_xmlwriter_end_comment },` |
|      - | 1418 | `		{ "xmlwriter_start_attribute", vm_builtin_xmlwriter_start_attribute },` |
|      - | 1419 | `		{ "xmlwriter_end_attribute", vm_builtin_xmlwriter_end_attribute },` |
|      - | 1420 | `		{ "xmlwriter_write_attribute", vm_builtin_xmlwriter_write_attribute },` |
|      - | 1421 | `		{ "xmlwriter_start_attribute_ns", vm_builtin_xmlwriter_start_attribute_ns },` |
|      - | 1422 | `		{ "xmlwriter_write_attribute_ns", vm_builtin_xmlwriter_write_attribute_ns },` |
|      - | 1423 | `		{ "xmlwriter_start_element", vm_builtin_xmlwriter_start_element },` |
|      - | 1424 | `		{ "xmlwriter_end_element", vm_builtin_xmlwriter_end_element },` |
|      - | 1425 | `		{ "xmlwriter_full_end_element", vm_builtin_xmlwriter_full_end_element },` |
|      - | 1426 | `		{ "xmlwriter_start_element_ns", vm_builtin_xmlwriter_start_element_ns },` |
|      - | 1427 | `		{ "xmlwriter_write_element", vm_builtin_xmlwriter_write_element },` |
|      - | 1428 | `		{ "xmlwriter_write_element_ns", vm_builtin_xmlwriter_write_element_ns },` |
|      - | 1429 | `		{ "xmlwriter_start_pi", vm_builtin_xmlwriter_start_pi },` |
|      - | 1430 | `		{ "xmlwriter_end_pi", vm_builtin_xmlwriter_end_pi },` |
|      - | 1431 | `		{ "xmlwriter_write_pi", vm_builtin_xmlwriter_write_pi },` |
|      - | 1432 | `		{ "xmlwriter_start_cdata", vm_builtin_xmlwriter_start_cdata },` |
|      - | 1433 | `		{ "xmlwriter_end_cdata", vm_builtin_xmlwriter_end_cdata },` |
|      - | 1434 | `		{ "xmlwriter_write_cdata", vm_builtin_xmlwriter_write_cdata },` |
|      - | 1435 | `		{ "xmlwriter_text", vm_builtin_xmlwriter_text },` |
|      - | 1436 | `		{ "xmlwriter_write_raw", vm_builtin_xmlwriter_write_raw },` |
|      - | 1437 | `		{ "xmlwriter_start_document", vm_builtin_xmlwriter_start_document },` |
|      - | 1438 | `		{ "xmlwriter_end_document", vm_builtin_xmlwriter_end_document },` |
|      - | 1439 | `		{ "xmlwriter_write_comment", vm_builtin_xmlwriter_write_comment },` |
|      - | 1440 | `		{ "xmlwriter_start_dtd", vm_builtin_xmlwriter_start_dtd },` |
|      - | 1441 | `		{ "xmlwriter_end_dtd", vm_builtin_xmlwriter_end_dtd },` |
|      - | 1442 | `		{ "xmlwriter_write_dtd", vm_builtin_xmlwriter_write_dtd },` |
|      - | 1443 | `		{ "xmlwriter_start_dtd_element", vm_builtin_xmlwriter_start_dtd_element },` |
|      - | 1444 | `		{ "xmlwriter_end_dtd_element", vm_builtin_xmlwriter_end_dtd_element },` |
|      - | 1445 | `		{ "xmlwriter_write_dtd_element", vm_builtin_xmlwriter_write_dtd_element },` |
|      - | 1446 | `		{ "xmlwriter_start_dtd_attlist", vm_builtin_xmlwriter_start_dtd_attlist },` |
|      - | 1447 | `		{ "xmlwriter_end_dtd_attlist", vm_builtin_xmlwriter_end_dtd_attlist },` |
|      - | 1448 | `		{ "xmlwriter_write_dtd_attlist", vm_builtin_xmlwriter_write_dtd_attlist },` |
|      - | 1449 | `		{ "xmlwriter_start_dtd_entity", vm_builtin_xmlwriter_start_dtd_entity },` |
|      - | 1450 | `		{ "xmlwriter_end_dtd_entity", vm_builtin_xmlwriter_end_dtd_entity },` |
|      - | 1451 | `		{ "xmlwriter_write_dtd_entity", vm_builtin_xmlwriter_write_dtd_entity },` |
|      - | 1452 | `		{ "xmlwriter_output_memory", vm_builtin_xmlwriter_output_memory },` |
|      - | 1453 | `		{ "xmlwriter_flush", vm_builtin_xmlwriter_flush },` |
|      - | 1454 | `	};` |
|      - | 1455 | `	sxu32 n;` |
|      - | 1456 | `	static const PH7_NativeClassSpec sSpec = {` |
|      - | 1457 | `		"XMLWriter", 0, 0, PH7_CLASS_NOCLONE,` |
|      - | 1458 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|      - | 1459 | `		0, 0,` |
|      - | 1460 | `		aProp, SX_ARRAYSIZE(aProp),` |
|      - | 1461 | `		XmlWriterInstanceRelease, 0, 0` |
|      - | 1462 | `	};` |
| 225927 | 1463 | `	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){` |
| 220673 | 1464 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
| 110339 | 1465 | `	}` |
|   5259 | 1466 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|      5 | 1467 | `}` |
|      - | 1468 |  |
|      - | 1469 | `#else` |
|      - | 1470 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1471 | `typedef int vm_xmlwriter_unused;` |
|      - | 1472 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1473 |  |
