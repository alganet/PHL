/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
/*
 * libxml2 2.14 marks most xmlParserCtxt members deprecated, among them the
 * seven this port reads or writes exactly where php's compat.c does
 * (instate, loadsubset, validate, pedantic, linenumbers, keepBlanks,
 * replaceEntities). Each access is then a -Wdeprecated-declarations, which
 * -Werror turns into a failed build on any toolchain with a current libxml
 * (Homebrew's). php silences the same warning around the same accesses.
 * Blanking the attribute through libxml's own #ifndef guard does that for
 * every compiler at once: a GCC pragma would be an unknown pragma to MSVC
 * under /WX, where libxml already defines the macro empty.
 */
#define XML_DEPRECATED_MEMBER
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/entities.h>
#include <libxml/dict.h>

/*
 * php's ext/xml: the expat-style PUSH parser (xml_parser_create, the handler
 * setters, xml_parse) over libxml2.
 *
 * php has not linked real expat for two decades: its ext/xml is a COMPAT
 * layer (ext/xml/compat.c) that answers the expat API from libxml2's push
 * parser, plus the PHP-facing half (ext/xml/xml.c) that folds case, converts
 * encodings and builds xml_parse_into_struct()'s arrays. This unit is a port
 * of that PAIR into one file, because the split exists only to imitate
 * expat's header. Every routing rule below that looks arbitrary is php's,
 * verified against the php 8.5 oracle:
 *
 *  - the parser runs with XML_PARSE_OLDSAX|XML_PARSE_NOENT after zeroing the
 *    ctxt options (php_libxml_sanitize_parse_ctxt_options), so ATTRIBUTE
 *    values arrive entity-expanded while CONTENT entity references produce
 *    no SAX events at all -- what a reference becomes is decided entirely
 *    inside the getEntity hook (XmlSaxGetEntity below);
 *  - a tag/PI/comment with NO handler for it is rebuilt as raw text for the
 *    DEFAULT handler -- start tags by scanning the parser's input buffer
 *    BACKWARD to the nearest '<', which is also why the raw text shows the
 *    original entity spelling php's own SAX layer never sees;
 *  - each xml_set_*_handler() call INSTALLS its libxml-level route once and
 *    forever: setting a handler and then unsetting it (null) leaves the
 *    route claimed, so those events are swallowed rather than falling back
 *    to the default handler. bInstalled mirrors php's compat-level handler
 *    pointers, the value slots mirror the PHP-level ones;
 *  - error CODES are raw libxml errNo values while xml_error_string() maps
 *    through php's own expat-flavoured table, so the two disagree on what
 *    each number means -- xml_get_error_code() after a mismatched tag is 76
 *    and XML_ERROR_TAG_MISMATCH is 7. Reproduced as-is;
 *  - an external-entity-ref handler that answers false stops the parser and
 *    forces errNo to expat's XML_ERROR_EXTERNAL_ENTITY_HANDLING (21), a
 *    number that means something else in the libxml table. Also php's.
 *
 * The deprecated surface is NOT here, per the scope policy:
 * xml_parser_free() (E_DEPRECATED 8.5, a no-op since 8.0) and
 * xml_set_object() (E_DEPRECATED 8.4) stay loud undefined functions, and the
 * method-name-string handler spelling (E_DEPRECATED 8.4) is refused as a
 * TypeError -- each twin-paired under 002-integration/function/xml/.
 *
 * Lifetime: an XMLParser instance owns a phl_xmlparser through a hidden slot;
 * the C shell (parser ctxt, its myDoc, the handler VALUES) is chained on the
 * per-VM registry (pVm->pXmlParsers) and freed only at VM reset/release,
 * like the DOM/XMLWriter registries above it -- PH7 resources carry no
 * destructor hook. libxml2 memory stays on the system allocator on purpose
 * (see vm_libxml.c's note on PHL_MAX_ALLOC fault injection).
 */

#define PHL_XML_MAXLEVEL 255  /* php's XML_MAXLEVEL: into_struct records no row deeper */

#ifndef XML_MAX_DICTIONARY_LIMIT
#define XML_MAX_DICTIONARY_LIMIT 10000000
#endif

/* Handler slots, one per event php exposes a setter for. */
enum {
	PHL_XML_H_START = 0,   /* xml_set_element_handler arg #2 */
	PHL_XML_H_END,         /* xml_set_element_handler arg #3 */
	PHL_XML_H_CDATA,       /* xml_set_character_data_handler */
	PHL_XML_H_PI,          /* xml_set_processing_instruction_handler */
	PHL_XML_H_DEFAULT,     /* xml_set_default_handler */
	PHL_XML_H_UNPARSED,    /* xml_set_unparsed_entity_decl_handler */
	PHL_XML_H_NOTATION,    /* xml_set_notation_decl_handler */
	PHL_XML_H_EXTENT,      /* xml_set_external_entity_ref_handler */
	PHL_XML_H_NSSTART,     /* xml_set_start_namespace_decl_handler */
	PHL_XML_H_NSEND,       /* xml_set_end_namespace_decl_handler (php never fires it) */
	PHL_XML_H_COUNT
};

/* Target encodings: the three php supports, nothing else can be named. */
enum { PHL_XML_ENC_UTF8 = 0, PHL_XML_ENC_ISO88591, PHL_XML_ENC_ASCII };

typedef struct phl_xmlparser phl_xmlparser;
struct phl_xmlparser {
	ph7_vm *pVm;
	xmlParserCtxtPtr pCtxt;
	/* Live only while xml_parse()/xml_parse_into_struct() runs: the calling
	 * context (whose value-tracking frees per-event temporaries) and the
	 * XMLParser instance handlers receive as their first argument. Both are
	 * borrowed -- the builtin's own argument keeps the instance alive. */
	ph7_context *pCallCtx;
	ph7_class_instance *pObj;
	int bNs;                  /* created by xml_parser_create_ns() */
	xmlChar cSep;             /* first byte of the separator; '\0' joins URI and name directly */
	int bCaseFolding;         /* XML_OPTION_CASE_FOLDING, default ON */
	int bSkipWhite;           /* XML_OPTION_SKIP_WHITE (into_struct only, like php) */
	int nSkipTagstart;        /* XML_OPTION_SKIP_TAGSTART */
	int bParseHuge;           /* XML_OPTION_PARSE_HUGE */
	int iTargetEnc;           /* PHL_XML_ENC_* */
	int bParsing;             /* recursion guard ("Parser must not be called recursively") */
	sxi32 iCbRc;              /* PH7_EXCEPTION/PH7_ABORT parked by a handler: further
	                           * handler calls and into_struct recording are suppressed,
	                           * the parse runs out, and xml_parse returns this status so
	                           * the dispatcher unwinds -- the builtin-throw rail. */
	int aInstalled[PHL_XML_H_COUNT]; /* the compat-level route was claimed once (see header note) */
	ph7_value aHandler[PHL_XML_H_COUNT];
	/* xml_parse_into_struct state; pStructData NULL when a plain xml_parse runs. */
	ph7_value *pStructData;   /* the working $values array (owned by the builtin frame) */
	ph7_value *pStructIndex;  /* the working $index array, or 0 when not asked for */
	int nLevel;
	int nCurtag;              /* row counter feeding $index (php never resets it either) */
	int bLastWasOpen;
	ph7_hashmap *pCtagMap;    /* the current OPEN row; rows are handles, so the pointer
	                           * stays valid inside the data array (no user can touch the
	                           * working array until it is stored back) */
	SyString aLtag[PHL_XML_MAXLEVEL]; /* UNSTRIPPED decoded tag per recorded level, because
	                           * SKIP_TAGSTART may change between open and the cdata rows */
	phl_xmlparser *pNext;     /* per-VM registry chain */
};

/* ------------------------------------------------------------------------
 * Encoding + case folding.
 *
 * libxml hands every SAX string as UTF-8. php converts to the TARGET
 * encoding on the way out (xml_utf8_decode): ISO-8859-1 keeps code points
 * <= 0xFF as bytes, US-ASCII <= 0x7F, everything else -- including an
 * invalid UTF-8 byte, consumed one byte at a time -- becomes '?'. Case
 * folding is zend_str_toupper on the CONVERTED bytes: ASCII-only, so
 * an accented name keeps its case (oracle: '<Aé/>' folds to 'Aé').
 * ------------------------------------------------------------------------ */
static void XmlDecodeAppend(int iTargetEnc,const char *zIn,sxu32 nIn,SyBlob *pOut)
{
	sxu32 i = 0;
	if( iTargetEnc == PHL_XML_ENC_UTF8 ){
		SyBlobAppend(pOut,zIn,nIn);
		return;
	}
	while( i < nIn ){
		const unsigned char *z = (const unsigned char *)zIn;
		unsigned int c = z[i];
		char cOut;
		if( c < 0x80 ){
			i++;
		}else if( (c & 0xE0) == 0xC0 && i + 1 < nIn && (z[i+1] & 0xC0) == 0x80 ){
			c = ((c & 0x1F) << 6) | (z[i+1] & 0x3F);
			if( c < 0x80 ){ c = (unsigned int)'?'; } /* overlong */
			i += 2;
		}else if( (c & 0xF0) == 0xE0 && i + 2 < nIn
		       && (z[i+1] & 0xC0) == 0x80 && (z[i+2] & 0xC0) == 0x80 ){
			c = ((c & 0x0F) << 12) | ((z[i+1] & 0x3F) << 6) | (z[i+2] & 0x3F);
			if( c < 0x800 || (c >= 0xD800 && c <= 0xDFFF) ){ c = (unsigned int)'?'; }
			i += 3;
		}else if( (c & 0xF8) == 0xF0 && i + 3 < nIn && (z[i+1] & 0xC0) == 0x80
		       && (z[i+2] & 0xC0) == 0x80 && (z[i+3] & 0xC0) == 0x80 ){
			c = ((c & 0x07) << 18) | ((z[i+1] & 0x3F) << 12)
			  | ((z[i+2] & 0x3F) << 6) | (z[i+3] & 0x3F);
			if( c < 0x10000 || c > 0x10FFFF ){ c = (unsigned int)'?'; }
			i += 4;
		}else{
			/* Invalid lead/truncated sequence: php consumes ONE byte per '?'. */
			c = (unsigned int)'?';
			i++;
		}
		if( iTargetEnc == PHL_XML_ENC_ISO88591 ){
			cOut = (char)(c > 0xFF ? '?' : c);
		}else{
			cOut = (char)(c > 0x7F ? '?' : c);
		}
		SyBlobAppend(pOut,&cOut,1);
	}
}
/* Decode a NUL-terminated SAX string into pOut; fold when asked. */
static void XmlDecodeName(phl_xmlparser *p,const xmlChar *zName,SyBlob *pOut)
{
	sxu32 n;
	XmlDecodeAppend(p->iTargetEnc,(const char *)zName,SyStrlen((const char *)zName),pOut);
	if( p->bCaseFolding ){
		char *z = (char *)SyBlobData(pOut);
		for( n = 0 ; n < SyBlobLength(pOut) ; ++n ){
			if( z[n] >= 'a' && z[n] <= 'z' ){
				z[n] = (char)(z[n] - ('a' - 'A'));
			}
		}
	}
}
/* php's xml_stripped_tag: SKIP_TAGSTART drops the first N bytes of a
 * DECODED tag name; an offset past the end is the empty string. */
static void XmlStrippedTag(const char *zTag,sxu32 nTag,int nOffset,SyString *pOut)
{
	if( nOffset <= 0 ){
		SyStringInitFromBuf(pOut,zTag,nTag);
	}else if( (sxu32)nOffset >= nTag ){
		SyStringInitFromBuf(pOut,zTag,0);
	}else{
		SyStringInitFromBuf(pOut,&zTag[nOffset],nTag - (sxu32)nOffset);
	}
}

/* ------------------------------------------------------------------------
 * Handler plumbing.
 * ------------------------------------------------------------------------ */
static int XmlHandlerSet(phl_xmlparser *p,int iH)
{
	return (p->aHandler[iH].iFlags & MEMOBJ_NULL) == 0;
}
/*
 * Run one user handler. A throw (or exit) comes back as PH7_EXCEPTION/
 * PH7_ABORT; php does not STOP the parser for it -- the pending exception
 * makes every later zend_call a no-op and suppresses recording while libxml
 * runs the rest of the document -- so iCbRc mirrors exactly that, and the
 * raise itself is deferred to xml_parse's return (the builtin-throw rail:
 * an in-flight throw from a C builtin would run the enclosing catch NOW).
 */
static void XmlCallHandler(phl_xmlparser *p,int iH,int nArg,ph7_value **apArg,ph7_value *pRet)
{
	ph7_value sRes;
	sxi32 rc;
	if( p->iCbRc || !XmlHandlerSet(p,iH) ){
		return;
	}
	PH7_MemObjInit(p->pVm,&sRes);
	rc = PH7_VmCallCallbackByValue(p->pVm,&p->aHandler[iH],nArg,apArg,&sRes,0);
	if( PH7_CALLBACK_UNWOUND(rc) ){
		p->iCbRc = rc;
	}else if( pRet && rc == SXRET_OK ){
		PH7_MemObjStore(&sRes,pRet);
	}
	PH7_MemObjRelease(&sRes);
}
/* The $parser argument every handler receives first: the instance the
 * running xml_parse() was handed. The wrapper takes its OWN reference,
 * because the PH7_MemObjRelease that ends every argument's life drops one. */
static void XmlParserValue(phl_xmlparser *p,ph7_value *pOut)
{
	PH7_MemObjInit(p->pVm,pOut);
	if( p->pObj ){
		p->pObj->iRef++;
		pOut->x.pOther = p->pObj;
		MemObjSetType(pOut,MEMOBJ_OBJ);
	}
}
/* A decoded-string argument (no folding). NULL becomes bool FALSE -- php's
 * xml_xmlchar_zval, which is why a notation with no PUBLIC id hands false. */
static void XmlStringValue(phl_xmlparser *p,const char *zIn,sxu32 nIn,ph7_value *pOut)
{
	SyBlob sBlob;
	SyString sStr;
	PH7_MemObjInit(p->pVm,pOut);
	if( zIn == 0 ){
		PH7_MemObjInitFromBool(p->pVm,pOut,0);
		return;
	}
	SyBlobInit(&sBlob,&p->pVm->sAllocator);
	XmlDecodeAppend(p->iTargetEnc,zIn,nIn,&sBlob);
	SyStringInitFromBuf(&sStr,SyBlobData(&sBlob),SyBlobLength(&sBlob));
	PH7_MemObjInitFromString(p->pVm,pOut,&sStr);
	SyBlobRelease(&sBlob);
}
static void XmlStringValueZ(phl_xmlparser *p,const xmlChar *zIn,ph7_value *pOut)
{
	XmlStringValue(p,(const char *)zIn,zIn ? SyStrlen((const char *)zIn) : 0,pOut);
}

/* ------------------------------------------------------------------------
 * xml_parse_into_struct recording -- the port of php's data/info halves.
 * Rows are hashmaps INSIDE the working array; hashmaps are handles under
 * MemObjStore, so pCtagMap can keep pointing at the open row after insert.
 * ------------------------------------------------------------------------ */
static void XmlRowAddStr(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey,const char *zVal,sxu32 nVal)
{
	ph7_value sVal;
	SyString sStr;
	PH7_MemObjInit(pVm,&sVal);
	SyStringInitFromBuf(&sStr,zVal,nVal);
	PH7_MemObjInitFromString(pVm,&sVal,&sStr);
	PH7_HashmapInsertRawKey(pRow,zKey,SyStrlen(zKey),&sVal);
	PH7_MemObjRelease(&sVal);
}
static void XmlRowAddInt(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey,sxi64 iVal)
{
	ph7_value sVal;
	PH7_MemObjInit(pVm,&sVal);
	PH7_MemObjInitFromInt(pVm,&sVal,iVal);
	PH7_HashmapInsertRawKey(pRow,zKey,SyStrlen(zKey),&sVal);
	PH7_MemObjRelease(&sVal);
}
/* Fetch a row's entry, or 0. The returned value is a COPY handle: for a
 * string entry the caller may not mutate through it, but reading and
 * re-inserting under the same key is php's add_assoc overwrite. */
static ph7_hashmap_node * XmlRowFind(ph7_vm *pVm,ph7_hashmap *pRow,const char *zKey)
{
	ph7_value sKey;
	ph7_hashmap_node *pNode = 0;
	SyString sStr;
	PH7_MemObjInit(pVm,&sKey);
	SyStringInitFromBuf(&sStr,zKey,SyStrlen(zKey));
	PH7_MemObjInitFromString(pVm,&sKey,&sStr);
	if( SXRET_OK != PH7_HashmapLookup(pRow,&sKey,&pNode) ){
		pNode = 0;
	}
	PH7_MemObjRelease(&sKey);
	return pNode;
}
/* php's xml_add_to_info: $index[$tag][] = curtag++ */
static void XmlAddToInfo(phl_xmlparser *p,const SyString *pTag)
{
	ph7_vm *pVm = p->pVm;
	ph7_hashmap *pInfo;
	ph7_hashmap_node *pNode;
	ph7_value sKey,sVal;
	if( p->pStructIndex == 0 ){
		return;
	}
	pInfo = (ph7_hashmap *)p->pStructIndex->x.pOther;
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjInitFromString(pVm,&sKey,pTag);
	if( SXRET_OK != PH7_HashmapLookup(pInfo,&sKey,&pNode) ){
		ph7_value sList;
		PH7_MemObjInit(pVm,&sList);
		if( SXRET_OK != PH7_MemObjToHashmap(&sList) ){
			PH7_MemObjRelease(&sKey);
			return;
		}
		PH7_HashmapInsert(pInfo,&sKey,&sList);
		PH7_MemObjRelease(&sList);
		if( SXRET_OK != PH7_HashmapLookup(pInfo,&sKey,&pNode) ){
			PH7_MemObjRelease(&sKey);
			return;
		}
	}
	PH7_MemObjInit(pVm,&sVal);
	{
		ph7_value sEntry;
		/* A Load view still takes a reference on the list it points at, so
		 * the release below is what keeps the per-tag lists from leaking
		 * one count per recorded row. */
		PH7_HashmapExtractNodeValue(pNode,&sVal,FALSE);
		if( (sVal.iFlags & MEMOBJ_HASHMAP) != 0 ){
			PH7_MemObjInit(pVm,&sEntry);
			PH7_MemObjInitFromInt(pVm,&sEntry,p->nCurtag);
			PH7_HashmapInsert((ph7_hashmap *)sVal.x.pOther,0,&sEntry);
			PH7_MemObjRelease(&sEntry);
		}
		PH7_MemObjRelease(&sVal);
	}
	PH7_MemObjRelease(&sKey);
	p->nCurtag++;
}
/* Append a finished row to the working $values array and answer its map. */
static ph7_hashmap * XmlRowAppend(phl_xmlparser *p,ph7_value *pRow)
{
	ph7_hashmap *pMap = (ph7_hashmap *)p->pStructData->x.pOther;
	if( SXRET_OK != PH7_HashmapInsert(pMap,0,pRow) ){
		return 0;
	}
	return (ph7_hashmap *)pRow->x.pOther;
}
/* Release the recorded per-level tag names (php's xml_parser_free_ltags). */
static void XmlFreeLtags(phl_xmlparser *p)
{
	int i;
	for( i = 0 ; i < PHL_XML_MAXLEVEL ; ++i ){
		if( p->aLtag[i].zString ){
			SyMemBackendFree(&p->pVm->sAllocator,(void *)p->aLtag[i].zString);
			SyStringInitFromBuf(&p->aLtag[i],0,0);
		}
	}
}

/* ------------------------------------------------------------------------
 * SAX callbacks: the compat.c + xml.c pair folded together.
 * ------------------------------------------------------------------------ */

/*
 * The raw-text route for a start tag nothing handles: scan the parser's
 * input buffer BACKWARD from the cursor to the nearest '<' and hand the
 * slice to the default handler verbatim -- original entity spellings and
 * all. A self-closing tag's cursor sits on the '/', which php patches to
 * '>' so the event reads like an open tag (the matching end event is built
 * separately). Straight from php's start_element_emit_default().
 */
static void XmlEmitRawStartTag(phl_xmlparser *p)
{
	const xmlChar *zCur,*zEnd,*zBase;
	if( p->iCbRc || !p->pCtxt || !p->pCtxt->input || !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){
		return;
	}
	zCur = p->pCtxt->input->cur;
	zEnd = zCur;
	zBase = p->pCtxt->input->base;
	while( zCur > zBase && *zCur != '<' ){
		zCur--;
	}
	{
		ph7_value sParser,sData;
		ph7_value *apArg[2];
		XmlParserValue(p,&sParser);
		if( *zEnd == '/' ){
			SyBlob sBlob;
			SyBlobInit(&sBlob,&p->pVm->sAllocator);
			SyBlobAppend(&sBlob,zCur,(sxu32)(zEnd - zCur));
			SyBlobAppend(&sBlob,">",1);
			XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);
			SyBlobRelease(&sBlob);
		}else{
			XmlStringValue(p,(const char *)zCur,(sxu32)(zEnd - zCur) + 1,&sData);
		}
		apArg[0] = &sParser;
		apArg[1] = &sData;
		XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sData);
	}
}
/* "</name>" for the default handler when no end handler exists. */
static void XmlEmitRawEndTag(phl_xmlparser *p,const xmlChar *zPrefix,const xmlChar *zName)
{
	SyBlob sBlob;
	ph7_value sParser,sData;
	ph7_value *apArg[2];
	if( p->iCbRc || !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){
		return;
	}
	SyBlobInit(&sBlob,&p->pVm->sAllocator);
	SyBlobAppend(&sBlob,"</",2);
	if( zPrefix ){
		SyBlobAppend(&sBlob,zPrefix,SyStrlen((const char *)zPrefix));
		SyBlobAppend(&sBlob,":",1);
	}
	SyBlobAppend(&sBlob,zName,SyStrlen((const char *)zName));
	SyBlobAppend(&sBlob,">",1);
	XmlParserValue(p,&sParser);
	XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);
	SyBlobRelease(&sBlob);
	apArg[0] = &sParser;
	apArg[1] = &sData;
	XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
	PH7_MemObjRelease(&sParser);
	PH7_MemObjRelease(&sData);
}
/*
 * The shared start-element body: attributes arrive as decoded (name,value)
 * pairs already collected into pAttrArr by the SAX1/SAX2 fronts below.
 * Ports php's xml_startElementHandler: the user handler runs first, then
 * the into_struct row is recorded -- both against the same folded name,
 * both skipping the first SKIP_TAGSTART bytes at PRESENTATION time.
 */
static void XmlStartElementCommon(phl_xmlparser *p,const char *zTag,sxu32 nTag,ph7_value *pAttrArr,int nAttr)
{
	ph7_vm *pVm = p->pVm;
	SyString sStripped;
	p->nLevel++;
	XmlStrippedTag(zTag,nTag,p->nSkipTagstart,&sStripped);
	if( XmlHandlerSet(p,PHL_XML_H_START) && !p->iCbRc ){
		ph7_value sParser,sName;
		ph7_value *apArg[3];
		XmlParserValue(p,&sParser);
		PH7_MemObjInit(pVm,&sName);
		PH7_MemObjInitFromString(pVm,&sName,&sStripped);
		apArg[0] = &sParser;
		apArg[1] = &sName;
		apArg[2] = pAttrArr;
		XmlCallHandler(p,PHL_XML_H_START,3,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sName);
	}
	if( p->pStructData && !p->iCbRc ){
		if( p->nLevel <= PHL_XML_MAXLEVEL ){
			ph7_value sRow;
			ph7_hashmap *pRow;
			char *zDup;
			PH7_MemObjInit(pVm,&sRow);
			if( SXRET_OK != PH7_MemObjToHashmap(&sRow) ){
				return;
			}
			pRow = (ph7_hashmap *)sRow.x.pOther;
			XmlAddToInfo(p,&sStripped);
			XmlRowAddStr(pVm,pRow,"tag",sStripped.zString,sStripped.nByte);
			XmlRowAddStr(pVm,pRow,"type","open",sizeof("open")-1);
			XmlRowAddInt(pVm,pRow,"level",p->nLevel);
			/* The UNSTRIPPED name is what later cdata rows strip live,
			 * because SKIP_TAGSTART may change mid-parse (php's comment). */
			zDup = SyMemBackendStrDup(&pVm->sAllocator,zTag,nTag);
			if( p->aLtag[p->nLevel - 1].zString ){
				SyMemBackendFree(&pVm->sAllocator,(void *)p->aLtag[p->nLevel - 1].zString);
			}
			SyStringInitFromBuf(&p->aLtag[p->nLevel - 1],zDup,zDup ? nTag : 0);
			p->bLastWasOpen = 1;
			if( nAttr > 0 ){
				ph7_value sKey;
				SyString sStr;
				PH7_MemObjInit(pVm,&sKey);
				SyStringInitFromBuf(&sStr,"attributes",sizeof("attributes")-1);
				PH7_MemObjInitFromString(pVm,&sKey,&sStr);
				PH7_HashmapInsert(pRow,&sKey,pAttrArr);
				PH7_MemObjRelease(&sKey);
			}
			p->pCtagMap = XmlRowAppend(p,&sRow);
			PH7_MemObjRelease(&sRow);
		}else if( p->nLevel == PHL_XML_MAXLEVEL + 1 ){
			ph7_context_throw_error(p->pCallCtx,PH7_CTX_WARNING,
				"Maximum depth exceeded - Results truncated");
		}
	}
}
/* The shared end-element body (php's xml_endElementHandler). */
static void XmlEndElementCommon(phl_xmlparser *p,const char *zTag,sxu32 nTag)
{
	ph7_vm *pVm = p->pVm;
	SyString sStripped;
	XmlStrippedTag(zTag,nTag,p->nSkipTagstart,&sStripped);
	if( XmlHandlerSet(p,PHL_XML_H_END) && !p->iCbRc ){
		ph7_value sParser,sName;
		ph7_value *apArg[2];
		XmlParserValue(p,&sParser);
		PH7_MemObjInit(pVm,&sName);
		PH7_MemObjInitFromString(pVm,&sName,&sStripped);
		apArg[0] = &sParser;
		apArg[1] = &sName;
		XmlCallHandler(p,PHL_XML_H_END,2,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sName);
	}
	if( p->pStructData && !p->iCbRc ){
		if( p->bLastWasOpen && p->pCtagMap ){
			/* Open with nothing recorded since: the row BECOMES the close. */
			XmlRowAddStr(pVm,p->pCtagMap,"type","complete",sizeof("complete")-1);
		}else{
			ph7_value sRow;
			PH7_MemObjInit(pVm,&sRow);
			if( SXRET_OK == PH7_MemObjToHashmap(&sRow) ){
				ph7_hashmap *pRow = (ph7_hashmap *)sRow.x.pOther;
				XmlAddToInfo(p,&sStripped);
				XmlRowAddStr(pVm,pRow,"tag",sStripped.zString,sStripped.nByte);
				XmlRowAddStr(pVm,pRow,"type","close",sizeof("close")-1);
				XmlRowAddInt(pVm,pRow,"level",p->nLevel);
				XmlRowAppend(p,&sRow);
				PH7_MemObjRelease(&sRow);
			}
		}
		p->bLastWasOpen = 0;
	}
	if( p->nLevel > 0 && p->nLevel <= PHL_XML_MAXLEVEL ){
		if( p->aLtag[p->nLevel - 1].zString ){
			SyMemBackendFree(&pVm->sAllocator,(void *)p->aLtag[p->nLevel - 1].zString);
			SyStringInitFromBuf(&p->aLtag[p->nLevel - 1],0,0);
		}
	}
	p->nLevel--;
}
/* SAX1 start (non-namespace parser): raw names, xmlns attrs included. */
static void XmlSaxStartElement(void *pUser,const xmlChar *zName,const xmlChar **azAttr)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	SyBlob sTag;
	ph7_value *pAttrArr;
	int nAttr = 0;
	if( p == 0 ){
		return;
	}
	if( !XmlHandlerSet(p,PHL_XML_H_START) && !p->aInstalled[PHL_XML_H_START] && !p->pStructData ){
		XmlEmitRawStartTag(p);
		return;
	}
	SyBlobInit(&sTag,&p->pVm->sAllocator);
	XmlDecodeName(p,zName,&sTag);
	pAttrArr = ph7_context_new_array(p->pCallCtx);
	if( pAttrArr ){
		while( azAttr && azAttr[0] ){
			SyBlob sAtt;
			ph7_value sVal;
			SyBlobInit(&sAtt,&p->pVm->sAllocator);
			XmlDecodeName(p,azAttr[0],&sAtt);
			XmlStringValueZ(p,azAttr[1],&sVal);
			PH7_HashmapInsertRawKey((ph7_hashmap *)pAttrArr->x.pOther,
				(const char *)SyBlobData(&sAtt),SyBlobLength(&sAtt),&sVal);
			PH7_MemObjRelease(&sVal);
			SyBlobRelease(&sAtt);
			nAttr++;
			azAttr += 2;
		}
		XmlStartElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag),pAttrArr,nAttr);
		ph7_context_release_value(p->pCallCtx,pAttrArr);
	}
	SyBlobRelease(&sTag);
}
static void XmlSaxEndElement(void *pUser,const xmlChar *zName)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	SyBlob sTag;
	if( p == 0 ){
		return;
	}
	if( !XmlHandlerSet(p,PHL_XML_H_END) && !p->aInstalled[PHL_XML_H_END] && !p->pStructData ){
		XmlEmitRawEndTag(p,0,zName);
		return;
	}
	SyBlobInit(&sTag,&p->pVm->sAllocator);
	XmlDecodeName(p,zName,&sTag);
	XmlEndElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag));
	SyBlobRelease(&sTag);
}
/* URI + separator + local name, php's qualify_namespace: the separator is
 * ONE byte and an empty one joins the parts directly (an artifact of php's
 * xmlStrncat over a "" separator that the oracle confirms). */
static void XmlQualifyName(phl_xmlparser *p,const xmlChar *zLocal,const xmlChar *zUri,SyBlob *pOut)
{
	if( zUri ){
		SyBlobAppend(pOut,zUri,SyStrlen((const char *)zUri));
		if( p->cSep ){
			SyBlobAppend(pOut,&p->cSep,1);
		}
	}
	SyBlobAppend(pOut,zLocal,SyStrlen((const char *)zLocal));
}
/* SAX2 start (namespace parser). */
static void XmlSaxStartElementNs(void *pUser,const xmlChar *zLocal,const xmlChar *zPrefix,
	const xmlChar *zUri,int nNs,const xmlChar **azNs,int nAttrIn,int nDefaulted,
	const xmlChar **azAttr)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	int i;
	SXUNUSED(nDefaulted);
	SXUNUSED(zPrefix);
	if( p == 0 ){
		return;
	}
	/* Namespace-declaration events fire BEFORE the element, php's order. */
	if( nNs > 0 && (XmlHandlerSet(p,PHL_XML_H_NSSTART) || p->aInstalled[PHL_XML_H_NSSTART]) ){
		for( i = 0 ; i < nNs ; ++i ){
			ph7_value sParser,sPrefix,sUri;
			ph7_value *apArg[3];
			XmlParserValue(p,&sParser);
			XmlStringValueZ(p,azNs[i*2],&sPrefix);       /* NULL prefix -> false */
			XmlStringValueZ(p,azNs[i*2+1],&sUri);
			apArg[0] = &sParser;
			apArg[1] = &sPrefix;
			apArg[2] = &sUri;
			XmlCallHandler(p,PHL_XML_H_NSSTART,3,apArg,0);
			PH7_MemObjRelease(&sParser);
			PH7_MemObjRelease(&sPrefix);
			PH7_MemObjRelease(&sUri);
		}
	}
	if( !XmlHandlerSet(p,PHL_XML_H_START) && !p->aInstalled[PHL_XML_H_START] && !p->pStructData ){
		XmlEmitRawStartTag(p);
		return;
	}
	{
		SyBlob sRaw,sTag;
		ph7_value *pAttrArr;
		int nAttr = 0;
		SyBlobInit(&sRaw,&p->pVm->sAllocator);
		SyBlobInit(&sTag,&p->pVm->sAllocator);
		XmlQualifyName(p,zLocal,zUri,&sRaw);
		SyBlobAppend(&sRaw,"",1);
		XmlDecodeName(p,(const xmlChar *)SyBlobData(&sRaw),&sTag);
		pAttrArr = ph7_context_new_array(p->pCallCtx);
		if( pAttrArr ){
			for( i = 0 ; i < nAttrIn ; ++i ){
				/* attributes: localname, prefix, uri, value, valueend */
				const xmlChar *zAl = azAttr[i*5];
				const xmlChar *zAp = azAttr[i*5+1];
				const xmlChar *zAu = azAttr[i*5+2];
				const xmlChar *zVs = azAttr[i*5+3];
				const xmlChar *zVe = azAttr[i*5+4];
				SyBlob sAr,sAn;
				ph7_value sVal;
				SyBlobInit(&sAr,&p->pVm->sAllocator);
				SyBlobInit(&sAn,&p->pVm->sAllocator);
				if( zAp ){
					XmlQualifyName(p,zAl,zAu,&sAr);
				}else{
					SyBlobAppend(&sAr,zAl,SyStrlen((const char *)zAl));
				}
				SyBlobAppend(&sAr,"",1);
				XmlDecodeName(p,(const xmlChar *)SyBlobData(&sAr),&sAn);
				XmlStringValue(p,(const char *)zVs,(sxu32)(zVe - zVs),&sVal);
				PH7_HashmapInsertRawKey((ph7_hashmap *)pAttrArr->x.pOther,
					(const char *)SyBlobData(&sAn),SyBlobLength(&sAn),&sVal);
				PH7_MemObjRelease(&sVal);
				SyBlobRelease(&sAn);
				SyBlobRelease(&sAr);
				nAttr++;
			}
			XmlStartElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag),pAttrArr,nAttr);
			ph7_context_release_value(p->pCallCtx,pAttrArr);
		}
		SyBlobRelease(&sTag);
		SyBlobRelease(&sRaw);
	}
}
static void XmlSaxEndElementNs(void *pUser,const xmlChar *zLocal,const xmlChar *zPrefix,
	const xmlChar *zUri)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	SyBlob sRaw,sTag;
	if( p == 0 ){
		return;
	}
	if( !XmlHandlerSet(p,PHL_XML_H_END) && !p->aInstalled[PHL_XML_H_END] && !p->pStructData ){
		XmlEmitRawEndTag(p,zPrefix,zLocal);
		return;
	}
	SyBlobInit(&sRaw,&p->pVm->sAllocator);
	SyBlobInit(&sTag,&p->pVm->sAllocator);
	XmlQualifyName(p,zLocal,zUri,&sRaw);
	SyBlobAppend(&sRaw,"",1);
	XmlDecodeName(p,(const xmlChar *)SyBlobData(&sRaw),&sTag);
	XmlEndElementCommon(p,(const char *)SyBlobData(&sTag),SyBlobLength(&sTag));
	SyBlobRelease(&sTag);
	SyBlobRelease(&sRaw);
}
/*
 * Character data (also CDATA blocks: php wires cdataBlock to the same
 * routine). Routes to the cdata handler; with none INSTALLED it falls
 * through to the default handler. The into_struct half appends to the
 * current open row's "value", or to a trailing cdata row, or starts one --
 * with SKIP_WHITE dropping only whitespace-only NEW rows, exactly php.
 */
static void XmlSaxCharacters(void *pUser,const xmlChar *zCh,int nLen)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	ph7_vm *pVm;
	if( p == 0 || nLen < 0 ){
		return;
	}
	pVm = p->pVm;
	if( XmlHandlerSet(p,PHL_XML_H_CDATA) && !p->iCbRc ){
		ph7_value sParser,sData;
		ph7_value *apArg[2];
		XmlParserValue(p,&sParser);
		XmlStringValue(p,(const char *)zCh,(sxu32)nLen,&sData);
		apArg[0] = &sParser;
		apArg[1] = &sData;
		XmlCallHandler(p,PHL_XML_H_CDATA,2,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sData);
	}else if( !p->aInstalled[PHL_XML_H_CDATA] && !p->pStructData && !p->iCbRc ){
		/* No cdata route claimed: the bytes reach the default handler raw. */
		ph7_value sParser,sData;
		ph7_value *apArg[2];
		if( XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){
			XmlParserValue(p,&sParser);
			XmlStringValue(p,(const char *)zCh,(sxu32)nLen,&sData);
			apArg[0] = &sParser;
			apArg[1] = &sData;
			XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
			PH7_MemObjRelease(&sParser);
			PH7_MemObjRelease(&sData);
		}
	}
	if( p->pStructData == 0 || p->iCbRc ){
		return;
	}
	{
		SyBlob sDec;
		int bPrint = 0;
		sxu32 n;
		SyBlobInit(&sDec,&pVm->sAllocator);
		XmlDecodeAppend(p->iTargetEnc,(const char *)zCh,(sxu32)nLen,&sDec);
		if( p->bSkipWhite ){
			const char *z = (const char *)SyBlobData(&sDec);
			for( n = 0 ; n < SyBlobLength(&sDec) ; ++n ){
				if( z[n] != ' ' && z[n] != '\t' && z[n] != '\n' ){
					bPrint = 1;
					break;
				}
			}
		}
		if( p->bLastWasOpen && p->pCtagMap ){
			ph7_hashmap_node *pNode = XmlRowFind(pVm,p->pCtagMap,"value");
			if( pNode ){
				/* Append to the existing value (php realloc+strncpy). */
				ph7_value sOld;
				PH7_MemObjInit(pVm,&sOld);
				PH7_HashmapExtractNodeValue(pNode,&sOld,FALSE);
				if( (sOld.iFlags & MEMOBJ_STRING) != 0 ){
					SyBlob sNew;
					SyBlobInit(&sNew,&pVm->sAllocator);
					SyBlobAppend(&sNew,SyBlobData(&sOld.sBlob),SyBlobLength(&sOld.sBlob));
					SyBlobAppend(&sNew,SyBlobData(&sDec),SyBlobLength(&sDec));
					XmlRowAddStr(pVm,p->pCtagMap,"value",
						(const char *)SyBlobData(&sNew),SyBlobLength(&sNew));
					SyBlobRelease(&sNew);
				}
				PH7_MemObjRelease(&sOld);
			}else if( bPrint || !p->bSkipWhite ){
				XmlRowAddStr(pVm,p->pCtagMap,"value",
					(const char *)SyBlobData(&sDec),SyBlobLength(&sDec));
			}
		}else{
			/* Not directly after an open: append to a trailing cdata row of the
			 * SAME run, else start a new row tagged with the enclosing element. */
			ph7_hashmap *pData = (ph7_hashmap *)p->pStructData->x.pOther;
			ph7_hashmap_node *pLast = pData ? pData->pLast : 0;
			int bAppended = 0;
			if( pLast ){
				ph7_value sLastRow;
				PH7_MemObjInit(pVm,&sLastRow);
				PH7_HashmapExtractNodeValue(pLast,&sLastRow,FALSE);
				if( (sLastRow.iFlags & MEMOBJ_HASHMAP) != 0 ){
					ph7_hashmap *pRowMap = (ph7_hashmap *)sLastRow.x.pOther;
					ph7_hashmap_node *pType = XmlRowFind(pVm,pRowMap,"type");
					if( pType ){
						ph7_value sType;
						PH7_MemObjInit(pVm,&sType);
						PH7_HashmapExtractNodeValue(pType,&sType,FALSE);
						if( (sType.iFlags & MEMOBJ_STRING) != 0
						 && SyBlobLength(&sType.sBlob) == sizeof("cdata")-1
						 && SyMemcmp(SyBlobData(&sType.sBlob),"cdata",sizeof("cdata")-1) == 0 ){
							ph7_hashmap_node *pVal = XmlRowFind(pVm,pRowMap,"value");
							if( pVal ){
								ph7_value sOld;
								PH7_MemObjInit(pVm,&sOld);
								PH7_HashmapExtractNodeValue(pVal,&sOld,FALSE);
								if( (sOld.iFlags & MEMOBJ_STRING) != 0 ){
									SyBlob sNew;
									SyBlobInit(&sNew,&pVm->sAllocator);
									SyBlobAppend(&sNew,SyBlobData(&sOld.sBlob),SyBlobLength(&sOld.sBlob));
									SyBlobAppend(&sNew,SyBlobData(&sDec),SyBlobLength(&sDec));
									XmlRowAddStr(pVm,pRowMap,"value",
										(const char *)SyBlobData(&sNew),SyBlobLength(&sNew));
									SyBlobRelease(&sNew);
									bAppended = 1;
								}
								PH7_MemObjRelease(&sOld);
							}
						}
						PH7_MemObjRelease(&sType);
					}
				}
				PH7_MemObjRelease(&sLastRow);
			}
			if( !bAppended ){
				if( p->nLevel > 0 && p->nLevel <= PHL_XML_MAXLEVEL
				 && (bPrint || !p->bSkipWhite) ){
					ph7_value sRow;
					PH7_MemObjInit(pVm,&sRow);
					if( SXRET_OK == PH7_MemObjToHashmap(&sRow) ){
						ph7_hashmap *pRow = (ph7_hashmap *)sRow.x.pOther;
						SyString sStripped;
						XmlStrippedTag(p->aLtag[p->nLevel - 1].zString ? p->aLtag[p->nLevel - 1].zString : "",
							p->aLtag[p->nLevel - 1].nByte,p->nSkipTagstart,&sStripped);
						XmlAddToInfo(p,&sStripped);
						XmlRowAddStr(pVm,pRow,"tag",sStripped.zString ? sStripped.zString : "",sStripped.nByte);
						XmlRowAddStr(pVm,pRow,"value",
							(const char *)SyBlobData(&sDec),SyBlobLength(&sDec));
						XmlRowAddStr(pVm,pRow,"type","cdata",sizeof("cdata")-1);
						XmlRowAddInt(pVm,pRow,"level",p->nLevel);
						XmlRowAppend(p,&sRow);
						PH7_MemObjRelease(&sRow);
					}
				}else if( p->nLevel == PHL_XML_MAXLEVEL + 1 ){
					ph7_context_throw_error(p->pCallCtx,PH7_CTX_WARNING,
						"Maximum depth exceeded - Results truncated");
				}
			}
		}
		SyBlobRelease(&sDec);
	}
}
/* Processing instruction: PI handler, else "<?target data?>" to default. */
static void XmlSaxPi(void *pUser,const xmlChar *zTarget,const xmlChar *zData)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	if( p == 0 ){
		return;
	}
	if( XmlHandlerSet(p,PHL_XML_H_PI) || p->aInstalled[PHL_XML_H_PI] ){
		ph7_value sParser,sTarget,sData;
		ph7_value *apArg[3];
		XmlParserValue(p,&sParser);
		XmlStringValueZ(p,zTarget,&sTarget);
		XmlStringValueZ(p,zData,&sData);
		apArg[0] = &sParser;
		apArg[1] = &sTarget;
		apArg[2] = &sData;
		XmlCallHandler(p,PHL_XML_H_PI,3,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sTarget);
		PH7_MemObjRelease(&sData);
	}else if( XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){
		SyBlob sBlob;
		ph7_value sParser,sData;
		ph7_value *apArg[2];
		SyBlobInit(&sBlob,&p->pVm->sAllocator);
		SyBlobFormat(&sBlob,"<?%s %s?>",zTarget ? (const char *)zTarget : "",
			zData ? (const char *)zData : "");
		XmlParserValue(p,&sParser);
		XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);
		SyBlobRelease(&sBlob);
		apArg[0] = &sParser;
		apArg[1] = &sData;
		XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sData);
	}
}
/* Comment: "<!--data-->" to the default handler; there is no comment setter. */
static void XmlSaxComment(void *pUser,const xmlChar *zVal)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	SyBlob sBlob;
	ph7_value sParser,sData;
	ph7_value *apArg[2];
	if( p == 0 || !XmlHandlerSet(p,PHL_XML_H_DEFAULT) ){
		return;
	}
	SyBlobInit(&sBlob,&p->pVm->sAllocator);
	SyBlobFormat(&sBlob,"<!--%s-->",zVal ? (const char *)zVal : "");
	XmlParserValue(p,&sParser);
	XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);
	SyBlobRelease(&sBlob);
	apArg[0] = &sParser;
	apArg[1] = &sData;
	XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
	PH7_MemObjRelease(&sParser);
	PH7_MemObjRelease(&sData);
}
/* Notation declaration: (parser, name, base=false, systemId, publicId). */
static void XmlSaxNotationDecl(void *pUser,const xmlChar *zName,const xmlChar *zPubId,
	const xmlChar *zSysId)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	ph7_value sParser,sName,sBase,sSys,sPub;
	ph7_value *apArg[5];
	if( p == 0 || !XmlHandlerSet(p,PHL_XML_H_NOTATION) ){
		return;
	}
	XmlParserValue(p,&sParser);
	XmlStringValueZ(p,zName,&sName);
	XmlStringValueZ(p,0,&sBase);       /* php hands NULL -> false */
	XmlStringValueZ(p,zSysId,&sSys);
	XmlStringValueZ(p,zPubId,&sPub);
	apArg[0] = &sParser;
	apArg[1] = &sName;
	apArg[2] = &sBase;
	apArg[3] = &sSys;
	apArg[4] = &sPub;
	XmlCallHandler(p,PHL_XML_H_NOTATION,5,apArg,0);
	PH7_MemObjRelease(&sParser);
	PH7_MemObjRelease(&sName);
	PH7_MemObjRelease(&sBase);
	PH7_MemObjRelease(&sSys);
	PH7_MemObjRelease(&sPub);
}
/* Unparsed (NDATA) entity: (parser, name, base=false, sysId, pubId, notation). */
static void XmlSaxUnparsedEntityDecl(void *pUser,const xmlChar *zName,const xmlChar *zPubId,
	const xmlChar *zSysId,const xmlChar *zNotation)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	ph7_value sParser,sName,sBase,sSys,sPub,sNot;
	ph7_value *apArg[6];
	if( p == 0 || !XmlHandlerSet(p,PHL_XML_H_UNPARSED) ){
		return;
	}
	XmlParserValue(p,&sParser);
	XmlStringValueZ(p,zName,&sName);
	XmlStringValueZ(p,0,&sBase);
	XmlStringValueZ(p,zSysId,&sSys);
	XmlStringValueZ(p,zPubId,&sPub);
	XmlStringValueZ(p,zNotation,&sNot);
	apArg[0] = &sParser;
	apArg[1] = &sName;
	apArg[2] = &sBase;
	apArg[3] = &sSys;
	apArg[4] = &sPub;
	apArg[5] = &sNot;
	XmlCallHandler(p,PHL_XML_H_UNPARSED,6,apArg,0);
	PH7_MemObjRelease(&sParser);
	PH7_MemObjRelease(&sName);
	PH7_MemObjRelease(&sBase);
	PH7_MemObjRelease(&sSys);
	PH7_MemObjRelease(&sPub);
	PH7_MemObjRelease(&sNot);
}
/*
 * External general entity reference. Fired from the getEntity hook (php's
 * compat routes it there, not through resolveEntity). The handler answers
 * an int: zero stops the parser and forces expat's error 21, php verbatim
 * -- including when the route was claimed and the handler later unset.
 */
static void XmlExternalEntityRef(phl_xmlparser *p,const xmlChar *zName,const xmlChar *zSysId,
	const xmlChar *zPubId)
{
	int bOk = 0;
	if( !p->aInstalled[PHL_XML_H_EXTENT] ){
		return;
	}
	if( XmlHandlerSet(p,PHL_XML_H_EXTENT) && !p->iCbRc ){
		ph7_value sParser,sNames,sBase,sSys,sPub,sRet;
		ph7_value *apArg[5];
		XmlParserValue(p,&sParser);
		XmlStringValueZ(p,zName,&sNames);
		XmlStringValue(p,"",0,&sBase);     /* php hands the empty base string */
		XmlStringValueZ(p,zSysId,&sSys);
		XmlStringValueZ(p,zPubId,&sPub);
		PH7_MemObjInit(p->pVm,&sRet);
		apArg[0] = &sParser;
		apArg[1] = &sNames;
		apArg[2] = &sBase;
		apArg[3] = &sSys;
		apArg[4] = &sPub;
		XmlCallHandler(p,PHL_XML_H_EXTENT,5,apArg,&sRet);
		if( p->iCbRc == 0 ){
			bOk = ph7_value_to_int64(&sRet) != 0;
		}
		PH7_MemObjRelease(&sParser);
		PH7_MemObjRelease(&sNames);
		PH7_MemObjRelease(&sBase);
		PH7_MemObjRelease(&sSys);
		PH7_MemObjRelease(&sPub);
		PH7_MemObjRelease(&sRet);
	}
	if( !bOk && p->iCbRc == 0 ){
		xmlStopParser(p->pCtxt);
		p->pCtxt->errNo = 21; /* expat's XML_ERROR_EXTERNAL_ENTITY_HANDLING */
	}
}
/*
 * The entity hook, php's get_entity(): what a reference BECOMES is decided
 * here. Predefined entities expand through the cdata route (libxml inlines
 * them) unless only a default handler listens, in which case the RAW
 * "&name;" goes there; internal entities are never expanded -- raw to the
 * default handler, or their replacement text to the cdata handler when no
 * default one listens; an unknown name also defaults raw and then fails the
 * parse (libxml raises 26 on the NULL return); an external entity routes
 * to the external-entity-ref handler.
 */
static xmlEntityPtr XmlSaxGetEntity(void *pUser,const xmlChar *zName)
{
	phl_xmlparser *p = (phl_xmlparser *)pUser;
	xmlEntityPtr pEnt = 0;
	if( p == 0 || p->pCtxt == 0 ){
		return 0;
	}
	if( p->pCtxt->inSubset == 0 ){
		pEnt = xmlGetPredefinedEntity(zName);
		if( pEnt == 0 ){
			pEnt = xmlGetDocEntity(p->pCtxt->myDoc,zName);
		}
		if( pEnt == 0 || p->pCtxt->instate == XML_PARSER_CONTENT ){
			if( pEnt == 0
			 || pEnt->etype == XML_INTERNAL_GENERAL_ENTITY
			 || pEnt->etype == XML_INTERNAL_PARAMETER_ENTITY
			 || pEnt->etype == XML_INTERNAL_PREDEFINED_ENTITY ){
				int bDefault = XmlHandlerSet(p,PHL_XML_H_DEFAULT) || p->aInstalled[PHL_XML_H_DEFAULT];
				int bCdata = XmlHandlerSet(p,PHL_XML_H_CDATA) || p->aInstalled[PHL_XML_H_CDATA];
				if( bDefault
				 && !(pEnt && pEnt->etype == XML_INTERNAL_PREDEFINED_ENTITY && bCdata) ){
					SyBlob sBlob;
					ph7_value sParser,sData;
					ph7_value *apArg[2];
					SyBlobInit(&sBlob,&p->pVm->sAllocator);
					SyBlobFormat(&sBlob,"&%s;",(const char *)zName);
					XmlParserValue(p,&sParser);
					XmlStringValue(p,(const char *)SyBlobData(&sBlob),SyBlobLength(&sBlob),&sData);
					SyBlobRelease(&sBlob);
					apArg[0] = &sParser;
					apArg[1] = &sData;
					XmlCallHandler(p,PHL_XML_H_DEFAULT,2,apArg,0);
					PH7_MemObjRelease(&sParser);
					PH7_MemObjRelease(&sData);
				}else if( bCdata && pEnt && pEnt->content ){
					/* No default route: the entity's replacement text reaches
					 * the cdata ROUTE from here -- under OLDSAX libxml inlines
					 * nothing itself, predefined entities included (php's
					 * comment: expat expands and hands it over). Through the
					 * full character-data front so an into_struct value picks
					 * the text up too, exactly as php's h_cdata pointer does. */
					XmlSaxCharacters(pUser,pEnt->content,
						(int)SyStrlen((const char *)pEnt->content));
				}
			}else if( pEnt->etype == XML_EXTERNAL_GENERAL_PARSED_ENTITY ){
				XmlExternalEntityRef(p,pEnt->name,pEnt->SystemID,pEnt->ExternalID);
			}
		}
	}
	return pEnt;
}

/* ------------------------------------------------------------------------
 * Parser lifecycle.
 * ------------------------------------------------------------------------ */
static void XmlInitSaxHandler(xmlSAXHandler *pSax)
{
	SyZero(pSax,sizeof(xmlSAXHandler));
	pSax->getEntity = XmlSaxGetEntity;
	pSax->notationDecl = XmlSaxNotationDecl;
	pSax->unparsedEntityDecl = XmlSaxUnparsedEntityDecl;
	pSax->startElement = XmlSaxStartElement;
	pSax->endElement = XmlSaxEndElement;
	pSax->characters = XmlSaxCharacters;
	pSax->processingInstruction = XmlSaxPi;
	pSax->comment = XmlSaxComment;
	pSax->cdataBlock = XmlSaxCharacters;
	pSax->initialized = XML_SAX2_MAGIC;
	pSax->startElementNs = XmlSaxStartElementNs;
	pSax->endElementNs = XmlSaxEndElementNs;
}
/* Free one parser's libxml half (php's XML_ParserFree order: doc, ctxt). */
static void XmlParserFreeC(phl_xmlparser *p)
{
	if( p->pCtxt ){
		if( p->pCtxt->myDoc ){
			xmlFreeDoc(p->pCtxt->myDoc);
			p->pCtxt->myDoc = 0;
		}
		xmlFreeParserCtxt(p->pCtxt);
		p->pCtxt = 0;
	}
}
/* Registry sweep, called from PH7_LibxmlVmReset (VM reset AND release). */
PH7_PRIVATE void PH7_XmlParserVmSweep(ph7_vm *pVm)
{
	phl_xmlparser *p = (phl_xmlparser *)pVm->pXmlParsers;
	while( p ){
		phl_xmlparser *pNext = p->pNext;
		int i;
		for( i = 0 ; i < PHL_XML_H_COUNT ; ++i ){
			PH7_MemObjRelease(&p->aHandler[i]);
		}
		XmlFreeLtags(p);
		XmlParserFreeC(p);
		SyMemBackendFree(&pVm->sAllocator,p);
		p = pNext;
	}
	pVm->pXmlParsers = 0;
}
/* The struct behind an XMLParser argument's hidden slot. The declared
 * `XMLParser $parser` type has already refused everything else. */
static phl_xmlparser * XmlParserOf(ph7_value *pArg,ph7_class_instance **ppObj)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	ph7_value *pRes;
	if( pArg == 0 || (pArg->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pArg->x.pOther;
	SyStringInitFromBuf(&sAttr,"__p",sizeof("__p")-1);
	pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
	if( pRes == 0 || (pRes->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	if( ppObj ){
		*ppObj = pThis;
	}
	return (phl_xmlparser *)pRes->x.pOther;
}
/*
 * xml_parser_create() / xml_parser_create_ns() -- shared body.
 * The $encoding names one of the three php supports (or nothing, which is
 * UTF-8); it decides the TARGET encoding, the input side is auto-detected
 * by libxml exactly as in php.
 */
static int XmlParserCreateImpl(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNs)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_xmlparser *p;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	xmlSAXHandler sSax;
	int iTargetEnc = PHL_XML_ENC_UTF8;
	xmlChar cSep = ':';
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		int nEnc = 0;
		const char *zEnc = ph7_value_to_string(apArg[0],&nEnc);
		if( nEnc == 0 ){
			iTargetEnc = PHL_XML_ENC_UTF8;
		}else if( nEnc == (int)sizeof("ISO-8859-1")-1 && SyStrnicmp(zEnc,"ISO-8859-1",(sxu32)nEnc) == 0 ){
			iTargetEnc = PHL_XML_ENC_ISO88591;
		}else if( nEnc == (int)sizeof("UTF-8")-1 && SyStrnicmp(zEnc,"UTF-8",(sxu32)nEnc) == 0 ){
			iTargetEnc = PHL_XML_ENC_UTF8;
		}else if( nEnc == (int)sizeof("US-ASCII")-1 && SyStrnicmp(zEnc,"US-ASCII",(sxu32)nEnc) == 0 ){
			iTargetEnc = PHL_XML_ENC_ASCII;
		}else{
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #1 ($encoding) is not a supported source encoding",
				ph7_function_name(pCtx));
		}
	}
	if( bNs && nArg > 1 ){
		int nSep = 0;
		const char *zSep = ph7_value_to_string(apArg[1],&nSep);
		/* php reads ONE byte: xml_parser_create_ns($e,"##") separates with
		 * '#', and the empty string joins URI and local name directly. */
		cSep = nSep > 0 ? (xmlChar)zSep[0] : (xmlChar)'\0';
	}
	p = (phl_xmlparser *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_xmlparser));
	if( p == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	SyZero(p,sizeof(phl_xmlparser));
	p->pVm = pVm;
	p->bNs = bNs;
	p->cSep = bNs ? cSep : (xmlChar)0;
	p->bCaseFolding = 1;
	p->iTargetEnc = iTargetEnc;
	{
		int i;
		for( i = 0 ; i < PHL_XML_H_COUNT ; ++i ){
			PH7_MemObjInit(pVm,&p->aHandler[i]);
		}
	}
	XmlInitSaxHandler(&sSax);
	p->pCtxt = xmlCreatePushParserCtxt(&sSax,(void *)p,0,0,0);
	if( p->pCtxt == 0 ){
		SyMemBackendFree(&pVm->sAllocator,p);
		return PH7_ContextMemoryError(pCtx);
	}
	/* php's setup: sanitize the inherited ctxt state, then run with
	 * OLDSAX+NOENT -- attributes expand entities, content references do
	 * not, and the getEntity hook decides everything else. Fields are set
	 * directly rather than through xmlCtxtUseOptions() so newer libxml's
	 * deprecation attribute on that function cannot break a /WX build. */
	p->pCtxt->loadsubset = 0;
	p->pCtxt->validate = 0;
	p->pCtxt->pedantic = 0;
	p->pCtxt->linenumbers = 0;
	p->pCtxt->keepBlanks = 1;
	p->pCtxt->options = XML_PARSE_OLDSAX | XML_PARSE_NOENT;
	p->pCtxt->replaceEntities = 1;
	p->pCtxt->wellFormed = 0;
	if( !bNs ){
		/* SAX1 dispatch: clearing the magic makes libxml use the raw
		 * startElement/endElement pair, which is where document-order
		 * attributes (xmlns included) come from. php's exact move. */
		p->pCtxt->sax->initialized = 1;
	}
	pClass = PH7_VmExtractClass(pVm,"XMLParser",sizeof("XMLParser")-1,FALSE,0);
	pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	if( pThis == 0 ){
		XmlParserFreeC(p);
		SyMemBackendFree(&pVm->sAllocator,p);
		return PH7_ContextMemoryError(pCtx);
	}
	{
		SyString sAttr;
		ph7_value *pRes;
		SyStringInitFromBuf(&sAttr,"__p",sizeof("__p")-1);
		pRes = PH7_ClassInstanceFetchAttr(pThis,&sAttr);
		if( pRes == 0 ){
			XmlParserFreeC(p);
			SyMemBackendFree(&pVm->sAllocator,p);
			PH7_ClassInstanceUnref(pThis);
			return PH7_ContextMemoryError(pCtx);
		}
		PH7_MemObjRelease(pRes);
		pRes->x.pOther = p;
		MemObjSetType(pRes,MEMOBJ_RES);
	}
	p->pNext = (phl_xmlparser *)pVm->pXmlParsers;
	pVm->pXmlParsers = (void *)p;
	PH7_NativeResultObject(pCtx,pThis);
	return PH7_OK;
}
/* XMLParser xml_parser_create(?string $encoding = null) */
static int vm_builtin_xml_parser_create(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlParserCreateImpl(pCtx,nArg,apArg,0);
}
/* XMLParser xml_parser_create_ns(?string $encoding = null, string $separator = ":") */
static int vm_builtin_xml_parser_create_ns(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlParserCreateImpl(pCtx,nArg,apArg,1);
}

/* ------------------------------------------------------------------------
 * Handler setters.
 * ------------------------------------------------------------------------ */
/*
 * Screen ONE handler argument and store it. php 8.4 deprecates the
 * method-name-string spelling (looked up on an xml_set_object() receiver);
 * PHL removes xml_set_object() and refuses a non-callable string with
 * php's callback TypeError instead (scope policy `10, twin-paired). A
 * callable STRING like "strlen" is an ordinary callable and passes.
 */
static sxi32 XmlStoreHandlerArg(ph7_context *pCtx,phl_xmlparser *p,int iH,
	ph7_value *pArg,int iArgPos,const char *zParam)
{
	if( !ph7_value_is_null(pArg) ){
		sxi32 rc = PH7_CheckCallbackArg(pCtx,pArg,iArgPos,zParam,1);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	PH7_MemObjRelease(&p->aHandler[iH]);
	if( !ph7_value_is_null(pArg) ){
		PH7_MemObjStore(pArg,&p->aHandler[iH]);
	}
	p->aInstalled[iH] = 1;
	return PH7_OK;
}
/* true xml_set_element_handler(XMLParser $parser, callable|string|null $start_handler,
 *                              callable|string|null $end_handler) */
static int vm_builtin_xml_set_element_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	sxi32 rc;
	if( p == 0 || nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = XmlStoreHandlerArg(pCtx,p,PHL_XML_H_START,apArg[1],2,"start_handler");
	if( rc != PH7_OK ){
		return rc;
	}
	rc = XmlStoreHandlerArg(pCtx,p,PHL_XML_H_END,apArg[2],3,"end_handler");
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int XmlSetOneHandler(ph7_context *pCtx,int nArg,ph7_value **apArg,int iH)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	sxi32 rc;
	if( p == 0 || nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	rc = XmlStoreHandlerArg(pCtx,p,iH,apArg[1],2,"handler");
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
static int vm_builtin_xml_set_character_data_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_CDATA);
}
static int vm_builtin_xml_set_processing_instruction_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_PI);
}
static int vm_builtin_xml_set_default_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_DEFAULT);
}
static int vm_builtin_xml_set_unparsed_entity_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_UNPARSED);
}
static int vm_builtin_xml_set_notation_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NOTATION);
}
static int vm_builtin_xml_set_external_entity_ref_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_EXTENT);
}
static int vm_builtin_xml_set_start_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NSSTART);
}
static int vm_builtin_xml_set_end_namespace_decl_handler(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/* Accepted and stored; php's libxml layer never fires this event. */
	return XmlSetOneHandler(pCtx,nArg,apArg,PHL_XML_H_NSEND);
}

/* ------------------------------------------------------------------------
 * Parsing.
 * ------------------------------------------------------------------------ */
/*
 * php's XML_Parse + xml_parse_helper: apply the PARSE_HUGE decision, feed
 * the chunk, then answer 1 only when neither the chunk call nor the ctxt's
 * recorded last error says otherwise. A handler that threw makes the whole
 * builtin return the parked status so the throw resumes at the CALLER of
 * xml_parse, php's timing.
 */
static sxi32 XmlParseChunkImpl(ph7_context *pCtx,phl_xmlparser *p,ph7_class_instance *pThis,
	const char *zData,int nData,int bFinal,int *pAnswer)
{
	int iErr;
	if( p->bParseHuge ){
		p->pCtxt->options |= XML_PARSE_HUGE;
		xmlDictSetLimit(p->pCtxt->dict,0);
	}else{
		p->pCtxt->options &= ~XML_PARSE_HUGE;
		xmlDictSetLimit(p->pCtxt->dict,XML_MAX_DICTIONARY_LIMIT);
	}
	p->bParsing = 1;
	p->pCallCtx = pCtx;
	p->pObj = pThis;
	p->iCbRc = 0;
	iErr = xmlParseChunk(p->pCtxt,zData,nData,bFinal);
	p->bParsing = 0;
	p->pCallCtx = 0;
	p->pObj = 0;
	if( p->iCbRc ){
		return p->iCbRc;
	}
	if( iErr ){
		*pAnswer = 0;
	}else{
		const xmlError *pLast = xmlCtxtGetLastError(p->pCtxt);
		*pAnswer = (pLast == 0 || pLast->level <= XML_ERR_WARNING) ? 1 : 0;
	}
	return PH7_OK;
}
/* int xml_parse(XMLParser $parser, string $data, bool $is_final = false) */
static int vm_builtin_xml_parse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pThis = 0;
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],&pThis) : 0;
	const char *zData;
	int nData = 0;
	int bFinal;
	int iAnswer = 0;
	sxi32 rc;
	if( p == 0 || nArg < 2 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( p->bParsing ){
		return PH7_VmThrowException(pCtx,"Error","Parser must not be called recursively");
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	bFinal = nArg > 2 ? ph7_value_to_bool(apArg[2]) : 0;
	rc = XmlParseChunkImpl(pCtx,p,pThis,zData,nData,bFinal,&iAnswer);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_int(pCtx,iAnswer);
	return PH7_OK;
}
/* int|false xml_parse_into_struct(XMLParser $parser, string $data, &$values, &$index = null) */
static int vm_builtin_xml_parse_into_struct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = 0;
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],&pThis) : 0;
	const char *zData;
	int nData = 0;
	int iAnswer = 0;
	sxi32 rc;
	ph7_value *pValues,*pIndex = 0;
	if( p == 0 || nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( p->bParsing ){
		/* php demotes THIS spelling of the recursion refusal to a warning. */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Parser must not be called recursively");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zData = ph7_value_to_string(apArg[1],&nData);
	pValues = ph7_context_new_array(pCtx);
	if( nArg > 3 ){
		pIndex = ph7_context_new_array(pCtx);
	}
	if( pValues == 0 || (nArg > 3 && pIndex == 0) ){
		return PH7_ContextMemoryError(pCtx);
	}
	p->pStructData = pValues;
	p->pStructIndex = pIndex;
	p->nLevel = 0;
	p->bLastWasOpen = 0;
	p->pCtagMap = 0;
	XmlFreeLtags(p);
	/* php's into_struct claims the element and cdata routes for good. */
	p->aInstalled[PHL_XML_H_START] = 1;
	p->aInstalled[PHL_XML_H_END] = 1;
	p->aInstalled[PHL_XML_H_CDATA] = 1;
	rc = XmlParseChunkImpl(pCtx,p,pThis,zData,nData,1,&iAnswer);
	p->pStructData = 0;
	p->pStructIndex = 0;
	p->pCtagMap = 0;
	/* The by-ref answers are written even when a handler threw: php builds
	 * them live in the caller's variable, so a caught throw still leaves
	 * the rows recorded so far. */
	PH7_VmStoreArgByRef(pVm,apArg[2],pValues);
	if( pIndex ){
		PH7_VmStoreArgByRef(pVm,apArg[3],pIndex);
	}
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_int(pCtx,iAnswer);
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Errors, positions, options.
 * ------------------------------------------------------------------------ */
/* int xml_get_error_code(XMLParser $parser) -- the RAW libxml errNo. */
static int vm_builtin_xml_get_error_code(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	ph7_result_int(pCtx,p && p->pCtxt ? p->pCtxt->errNo : 0);
	return PH7_OK;
}
/*
 * php's expat-flavoured error table, indexed by LIBXML error code (see the
 * header note: the XML_ERROR_* constants and this table disagree past the
 * first few entries, and that mismatch is php's shipped behaviour). Entries
 * whose libxml condition php never translated keep the raw enum spelling.
 */
static const char * const azXmlErrorMap[] = {
	"No error", "No memory", "Invalid document start", "Empty document",
	"Not well-formed (invalid token)", "Invalid document end",
	"Invalid hexadecimal character reference", "Invalid decimal character reference",
	"Invalid character reference", "Invalid character",
	"XML_ERR_CHARREF_AT_EOF", "XML_ERR_CHARREF_IN_PROLOG", "XML_ERR_CHARREF_IN_EPILOG",
	"XML_ERR_CHARREF_IN_DTD", "XML_ERR_ENTITYREF_AT_EOF", "XML_ERR_ENTITYREF_IN_PROLOG",
	"XML_ERR_ENTITYREF_IN_EPILOG", "XML_ERR_ENTITYREF_IN_DTD",
	"PEReference at end of document", "PEReference in prolog", "PEReference in epilog",
	"PEReference: forbidden within markup decl in internal subset",
	"XML_ERR_ENTITYREF_NO_NAME", "EntityRef: expecting ';'", "PEReference: no name",
	"PEReference: expecting ';'", "Undeclared entity error", "Undeclared entity warning",
	"Unparsed Entity", "XML_ERR_ENTITY_IS_EXTERNAL", "XML_ERR_ENTITY_IS_PARAMETER",
	"Unknown encoding", "Unsupported encoding", "String not started expecting ' or \"",
	"String not closed expecting \" or '", "Namespace declaration error",
	"EntityValue: \" or ' expected", "EntityValue: \" or ' expected", "< in attribute",
	"Attribute not started", "Attribute not finished", "Attribute without value",
	"Attribute redefined", "SystemLiteral \" or ' expected", "SystemLiteral \" or ' expected",
	"Comment not finished", "Processing Instruction not started",
	"Processing Instruction not finished", "NOTATION: Name expected here",
	"'>' required to close NOTATION declaration",
	"'(' required to start ATTLIST enumeration", "'(' required to start ATTLIST enumeration",
	"MixedContentDecl : '|' or ')*' expected", "XML_ERR_MIXED_NOT_FINISHED",
	"ELEMENT in DTD not started", "ELEMENT in DTD not finished",
	"XML declaration not started", "XML declaration not finished",
	"XML_ERR_CONDSEC_NOT_STARTED", "XML conditional section not closed",
	"Content error in the external subset", "DOCTYPE not finished",
	"Sequence ']]>' not allowed in content", "CDATA not finished", "Reserved XML Name",
	"Space required", "XML_ERR_SEPARATOR_REQUIRED", "NmToken expected in ATTLIST enumeration",
	"XML_ERR_NAME_REQUIRED", "MixedContentDecl : '#PCDATA' expected",
	"SYSTEM or PUBLIC, the URI is missing", "PUBLIC, the Public Identifier is missing",
	"< required", "> required", "</ required", "= required", "Mismatched tag",
	"Tag not finished", "standalone accepts only 'yes' or 'no'",
	"Invalid XML encoding name", "Comment must not contain '--' (double-hyphen)",
	"Invalid encoding", "external parsed entities cannot be standalone",
	"XML conditional section '[' expected", "Entity value required",
	"chunk is not well balanced", "extra content at the end of well balanced chunk",
	"XML_ERR_ENTITY_CHAR_ERROR", "PEReferences forbidden in internal subset",
	"Detected an entity reference loop", "XML_ERR_ENTITY_BOUNDARY", "Invalid URI",
	"Fragment not allowed", "XML_WAR_CATALOG_PI", "XML_ERR_NO_DTD",
	"conditional section INCLUDE or IGNORE keyword expected",
	"Version in XML Declaration missing", "XML_WAR_UNKNOWN_VERSION", "XML_WAR_LANG_VALUE",
	"XML_WAR_NS_URI", "XML_WAR_NS_URI_RELATIVE", "Missing encoding in text declaration"
};
/* ?string xml_error_string(int $error_code) */
static int vm_builtin_xml_error_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi64 iCode = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	if( iCode < 0 || iCode >= (sxi64)SX_ARRAYSIZE(azXmlErrorMap) ){
		ph7_result_string(pCtx,"Unknown",(int)sizeof("Unknown")-1);
		return PH7_OK;
	}
	ph7_result_string(pCtx,azXmlErrorMap[iCode],-1);
	return PH7_OK;
}
/* int xml_get_current_line_number(XMLParser $parser) */
static int vm_builtin_xml_get_current_line_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	ph7_result_int(pCtx,(p && p->pCtxt && p->pCtxt->input) ? p->pCtxt->input->line : 0);
	return PH7_OK;
}
/* int xml_get_current_column_number(XMLParser $parser) */
static int vm_builtin_xml_get_current_column_number(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	ph7_result_int(pCtx,(p && p->pCtxt && p->pCtxt->input) ? p->pCtxt->input->col : 0);
	return PH7_OK;
}
/* int xml_get_current_byte_index(XMLParser $parser) */
static int vm_builtin_xml_get_current_byte_index(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	sxi64 iByte = 0;
	if( p && p->pCtxt && p->pCtxt->input ){
		iByte = (sxi64)p->pCtxt->input->consumed
			+ (sxi64)(p->pCtxt->input->cur - p->pCtxt->input->base);
	}
	ph7_result_int64(pCtx,iByte);
	return PH7_OK;
}
/* The XML_OPTION_* numbers (php's, not libxml's). */
#define PHL_XML_OPTION_CASE_FOLDING    1
#define PHL_XML_OPTION_TARGET_ENCODING 2
#define PHL_XML_OPTION_SKIP_TAGSTART   3
#define PHL_XML_OPTION_SKIP_WHITE      4
#define PHL_XML_OPTION_PARSE_HUGE      5
static const char * XmlTargetEncName(int iEnc)
{
	switch( iEnc ){
		case PHL_XML_ENC_ISO88591: return "ISO-8859-1";
		case PHL_XML_ENC_ASCII:    return "US-ASCII";
		default:                   return "UTF-8";
	}
}
/* bool xml_parser_set_option(XMLParser $parser, int $option, $value) */
static int vm_builtin_xml_parser_set_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	sxi64 iOpt;
	ph7_value *pValue;
	if( p == 0 || nArg < 3 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iOpt = ph7_value_to_int64(apArg[1]);
	pValue = apArg[2];
	/* php only WARNS about a $value outside string|int|bool and reads it
	 * anyway; float slides through the warning too. */
	if( (pValue->iFlags & (MEMOBJ_STRING|MEMOBJ_INT|MEMOBJ_BOOL)) == 0 ){
		const char *zType = "null";
		if( pValue->iFlags & MEMOBJ_REAL ){ zType = "float"; }
		else if( pValue->iFlags & MEMOBJ_HASHMAP ){ zType = "array"; }
		else if( pValue->iFlags & MEMOBJ_OBJ ){ zType = "object"; }
		else if( pValue->iFlags & MEMOBJ_RES ){ zType = "resource"; }
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Argument #3 ($value) must be of type string|int|bool, %s given",zType);
	}
	switch( (int)iOpt ){
		case PHL_XML_OPTION_CASE_FOLDING:
			p->bCaseFolding = ph7_value_to_bool(pValue) ? 1 : 0;
			break;
		case PHL_XML_OPTION_SKIP_WHITE:
			p->bSkipWhite = ph7_value_to_bool(pValue) ? 1 : 0;
			break;
		case PHL_XML_OPTION_PARSE_HUGE:
			if( p->bParsing ){
				return PH7_VmThrowException(pCtx,"Error",
					"Cannot change option XML_OPTION_PARSE_HUGE while parsing");
			}
			p->bParseHuge = ph7_value_to_bool(pValue) ? 1 : 0;
			break;
		case PHL_XML_OPTION_SKIP_TAGSTART: {
			sxi64 iVal = ph7_value_to_int64(pValue);
			if( iVal < 0 || iVal > 2147483647 ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Argument #3 ($value) must be between 0 and 2147483647 for option XML_OPTION_SKIP_TAGSTART");
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			p->nSkipTagstart = (int)iVal;
			break;
		}
		case PHL_XML_OPTION_TARGET_ENCODING: {
			int nEnc = 0;
			const char *zEnc = ph7_value_to_string(pValue,&nEnc);
			if( nEnc == (int)sizeof("ISO-8859-1")-1 && SyStrnicmp(zEnc,"ISO-8859-1",(sxu32)nEnc) == 0 ){
				p->iTargetEnc = PHL_XML_ENC_ISO88591;
			}else if( nEnc == (int)sizeof("UTF-8")-1 && SyStrnicmp(zEnc,"UTF-8",(sxu32)nEnc) == 0 ){
				p->iTargetEnc = PHL_XML_ENC_UTF8;
			}else if( nEnc == (int)sizeof("US-ASCII")-1 && SyStrnicmp(zEnc,"US-ASCII",(sxu32)nEnc) == 0 ){
				p->iTargetEnc = PHL_XML_ENC_ASCII;
			}else{
				return PH7_VmThrowException(pCtx,"ValueError",
					"%s(): Argument #3 ($value) is not a supported target encoding",
					ph7_function_name(pCtx));
			}
			break;
		}
		default:
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($option) must be a XML_OPTION_* constant",
				ph7_function_name(pCtx));
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* string|int|bool xml_parser_get_option(XMLParser $parser, int $option) */
static int vm_builtin_xml_parser_get_option(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	phl_xmlparser *p = nArg > 0 ? XmlParserOf(apArg[0],0) : 0;
	sxi64 iOpt;
	if( p == 0 || nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iOpt = ph7_value_to_int64(apArg[1]);
	switch( (int)iOpt ){
		case PHL_XML_OPTION_CASE_FOLDING:    ph7_result_bool(pCtx,p->bCaseFolding); break;
		case PHL_XML_OPTION_SKIP_TAGSTART:   ph7_result_int(pCtx,p->nSkipTagstart); break;
		case PHL_XML_OPTION_SKIP_WHITE:      ph7_result_bool(pCtx,p->bSkipWhite); break;
		case PHL_XML_OPTION_PARSE_HUGE:      ph7_result_bool(pCtx,p->bParseHuge); break;
		case PHL_XML_OPTION_TARGET_ENCODING:
			ph7_result_string(pCtx,XmlTargetEncName(p->iTargetEnc),-1);
			break;
		default:
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #2 ($option) must be a XML_OPTION_* constant",
				ph7_function_name(pCtx));
	}
	return PH7_OK;
}

/* ------------------------------------------------------------------------
 * Installation.
 * ------------------------------------------------------------------------ */
/* XMLParser has no reachable constructor; the class object refuses `new`
 * with php's own sentence (PH7_CLASS_NOINSTANTIATE + zNewRefusal below). */
#define XML_INT_CONST(FN,VALUE) \
	static void FN(ph7_value *pVal,void *pUnused){ \
		SXUNUSED(pUnused); \
		ph7_value_int64(pVal,(ph7_int64)(VALUE)); \
	}
XML_INT_CONST(XmlConst_ERROR_NONE,                          0)
XML_INT_CONST(XmlConst_ERROR_NO_MEMORY,                     1)
XML_INT_CONST(XmlConst_ERROR_SYNTAX,                        2)
XML_INT_CONST(XmlConst_ERROR_NO_ELEMENTS,                   3)
XML_INT_CONST(XmlConst_ERROR_INVALID_TOKEN,                 4)
XML_INT_CONST(XmlConst_ERROR_UNCLOSED_TOKEN,                5)
XML_INT_CONST(XmlConst_ERROR_PARTIAL_CHAR,                  6)
XML_INT_CONST(XmlConst_ERROR_TAG_MISMATCH,                  7)
XML_INT_CONST(XmlConst_ERROR_DUPLICATE_ATTRIBUTE,           8)
XML_INT_CONST(XmlConst_ERROR_JUNK_AFTER_DOC_ELEMENT,        9)
XML_INT_CONST(XmlConst_ERROR_PARAM_ENTITY_REF,             10)
XML_INT_CONST(XmlConst_ERROR_UNDEFINED_ENTITY,             11)
XML_INT_CONST(XmlConst_ERROR_RECURSIVE_ENTITY_REF,         12)
XML_INT_CONST(XmlConst_ERROR_ASYNC_ENTITY,                 13)
XML_INT_CONST(XmlConst_ERROR_BAD_CHAR_REF,                 14)
XML_INT_CONST(XmlConst_ERROR_BINARY_ENTITY_REF,            15)
XML_INT_CONST(XmlConst_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,16)
XML_INT_CONST(XmlConst_ERROR_MISPLACED_XML_PI,             17)
XML_INT_CONST(XmlConst_ERROR_UNKNOWN_ENCODING,             18)
XML_INT_CONST(XmlConst_ERROR_INCORRECT_ENCODING,           19)
XML_INT_CONST(XmlConst_ERROR_UNCLOSED_CDATA_SECTION,       20)
XML_INT_CONST(XmlConst_ERROR_EXTERNAL_ENTITY_HANDLING,     21)
XML_INT_CONST(XmlConst_OPTION_CASE_FOLDING,    PHL_XML_OPTION_CASE_FOLDING)
XML_INT_CONST(XmlConst_OPTION_TARGET_ENCODING, PHL_XML_OPTION_TARGET_ENCODING)
XML_INT_CONST(XmlConst_OPTION_SKIP_TAGSTART,   PHL_XML_OPTION_SKIP_TAGSTART)
XML_INT_CONST(XmlConst_OPTION_SKIP_WHITE,      PHL_XML_OPTION_SKIP_WHITE)
XML_INT_CONST(XmlConst_OPTION_PARSE_HUGE,      PHL_XML_OPTION_PARSE_HUGE)
static void XmlConst_SAX_IMPL(ph7_value *pVal,void *pUnused)
{
	SXUNUSED(pUnused);
	ph7_value_string(pVal,"libxml",(int)sizeof("libxml")-1);
}
/*
 * Install php's ext/xml surface: the XMLParser class, the 19 functions of
 * its NON-deprecated half, and the XML_* constants. Called from PH7_VmInit
 * inside the bCompilingBuiltin window, after PH7_VmInstallLibxml.
 */
PH7_PRIVATE sxi32 PH7_VmInstallXml(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "xml_parser_create",       vm_builtin_xml_parser_create },
		{ "xml_parser_create_ns",    vm_builtin_xml_parser_create_ns },
		{ "xml_parse",               vm_builtin_xml_parse },
		{ "xml_parse_into_struct",   vm_builtin_xml_parse_into_struct },
		{ "xml_parser_get_option",   vm_builtin_xml_parser_get_option },
		{ "xml_parser_set_option",   vm_builtin_xml_parser_set_option },
		{ "xml_error_string",        vm_builtin_xml_error_string },
		{ "xml_get_error_code",      vm_builtin_xml_get_error_code },
		{ "xml_get_current_line_number",   vm_builtin_xml_get_current_line_number },
		{ "xml_get_current_column_number", vm_builtin_xml_get_current_column_number },
		{ "xml_get_current_byte_index",    vm_builtin_xml_get_current_byte_index },
		{ "xml_set_element_handler",       vm_builtin_xml_set_element_handler },
		{ "xml_set_character_data_handler",vm_builtin_xml_set_character_data_handler },
		{ "xml_set_processing_instruction_handler", vm_builtin_xml_set_processing_instruction_handler },
		{ "xml_set_default_handler",       vm_builtin_xml_set_default_handler },
		{ "xml_set_unparsed_entity_decl_handler", vm_builtin_xml_set_unparsed_entity_decl_handler },
		{ "xml_set_notation_decl_handler", vm_builtin_xml_set_notation_decl_handler },
		{ "xml_set_external_entity_ref_handler", vm_builtin_xml_set_external_entity_ref_handler },
		{ "xml_set_start_namespace_decl_handler", vm_builtin_xml_set_start_namespace_decl_handler },
		{ "xml_set_end_namespace_decl_handler",   vm_builtin_xml_set_end_namespace_decl_handler },
	};
	static const struct {
		const char *zName;
		void (*xExpand)(ph7_value *,void *);
	} aConst[] = {
		{ "XML_ERROR_NONE",                XmlConst_ERROR_NONE },
		{ "XML_ERROR_NO_MEMORY",           XmlConst_ERROR_NO_MEMORY },
		{ "XML_ERROR_SYNTAX",              XmlConst_ERROR_SYNTAX },
		{ "XML_ERROR_NO_ELEMENTS",         XmlConst_ERROR_NO_ELEMENTS },
		{ "XML_ERROR_INVALID_TOKEN",       XmlConst_ERROR_INVALID_TOKEN },
		{ "XML_ERROR_UNCLOSED_TOKEN",      XmlConst_ERROR_UNCLOSED_TOKEN },
		{ "XML_ERROR_PARTIAL_CHAR",        XmlConst_ERROR_PARTIAL_CHAR },
		{ "XML_ERROR_TAG_MISMATCH",        XmlConst_ERROR_TAG_MISMATCH },
		{ "XML_ERROR_DUPLICATE_ATTRIBUTE", XmlConst_ERROR_DUPLICATE_ATTRIBUTE },
		{ "XML_ERROR_JUNK_AFTER_DOC_ELEMENT", XmlConst_ERROR_JUNK_AFTER_DOC_ELEMENT },
		{ "XML_ERROR_PARAM_ENTITY_REF",    XmlConst_ERROR_PARAM_ENTITY_REF },
		{ "XML_ERROR_UNDEFINED_ENTITY",    XmlConst_ERROR_UNDEFINED_ENTITY },
		{ "XML_ERROR_RECURSIVE_ENTITY_REF",XmlConst_ERROR_RECURSIVE_ENTITY_REF },
		{ "XML_ERROR_ASYNC_ENTITY",        XmlConst_ERROR_ASYNC_ENTITY },
		{ "XML_ERROR_BAD_CHAR_REF",        XmlConst_ERROR_BAD_CHAR_REF },
		{ "XML_ERROR_BINARY_ENTITY_REF",   XmlConst_ERROR_BINARY_ENTITY_REF },
		{ "XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF", XmlConst_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF },
		{ "XML_ERROR_MISPLACED_XML_PI",    XmlConst_ERROR_MISPLACED_XML_PI },
		{ "XML_ERROR_UNKNOWN_ENCODING",    XmlConst_ERROR_UNKNOWN_ENCODING },
		{ "XML_ERROR_INCORRECT_ENCODING",  XmlConst_ERROR_INCORRECT_ENCODING },
		{ "XML_ERROR_UNCLOSED_CDATA_SECTION", XmlConst_ERROR_UNCLOSED_CDATA_SECTION },
		{ "XML_ERROR_EXTERNAL_ENTITY_HANDLING", XmlConst_ERROR_EXTERNAL_ENTITY_HANDLING },
		{ "XML_OPTION_CASE_FOLDING",       XmlConst_OPTION_CASE_FOLDING },
		{ "XML_OPTION_TARGET_ENCODING",    XmlConst_OPTION_TARGET_ENCODING },
		{ "XML_OPTION_SKIP_TAGSTART",      XmlConst_OPTION_SKIP_TAGSTART },
		{ "XML_OPTION_SKIP_WHITE",         XmlConst_OPTION_SKIP_WHITE },
		{ "XML_OPTION_PARSE_HUGE",         XmlConst_OPTION_PARSE_HUGE },
		{ "XML_SAX_IMPL",                  XmlConst_SAX_IMPL },
	};
	/* The libxml parser handle: storage the class owns and never presents. */
	static const PH7_NativePropDef aProp[] = {
		{ "__p", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec sSpec = {
		"XMLParser", 0, 0,
		PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		0, 0, 0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0, 0
	};
	sxu32 n;
	sxi32 rc;
	pVm->pXmlParsers = 0;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aConst) ; n++ ){
		ph7_create_constant(&(*pVm),aConst[n].zName,aConst[n].xExpand,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
	if( rc == SXRET_OK ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"XMLParser",sizeof("XMLParser")-1,FALSE,0);
		if( pClass ){
			pClass->zNewRefusal =
				"Cannot directly construct XMLParser, use xml_parser_create() or xml_parser_create_ns() instead";
		}
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_xml_unused;
#endif /* PH7_ENABLE_LIBXML */
