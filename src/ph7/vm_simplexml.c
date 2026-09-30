/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/entities.h>

/*
 * ext/simplexml on libxml2: SimpleXMLElement, SimpleXMLIterator and the four
 * functions that make and unmake them.
 *
 * WHAT AN INSTANCE IS.  php's SimpleXMLElement is not a node wrapper -- it is a
 * NODE plus a QUESTION about it, and the question is what every one of its
 * surfaces answers.  The object carries
 *
 *   $__res  the {document shell, xmlNode} handle (the same phl_domnode ext/dom
 *           uses, so the two extensions share one tree and one lifetime);
 *   $__it   which of php's four `iter.type`s this object IS;
 *   $__nm   the element/attribute NAME the question names, when it names one;
 *   $__ns   a namespace filter, and $__isp whether it is a PREFIX or a URI.
 *
 * The four questions, php's SXE_ITER_* :
 *
 *   NONE      "this node".            `simplexml_load_string()`, `$set[0]`.
 *   ELEMENT   "the children of this node named $__nm".    `$x->kid`.
 *   CHILD     "the children of this node".                `$x->children()`.
 *   ATTRLIST  "the attributes of this node" (named by $__nm when an xpath
 *             `//@a` produced it).                        `$x->attributes()`.
 *
 * So `$x->kid` does not FIND anything: it hands back an object standing for a
 * SET that may be empty, which is why `$x->nothing` is an object and not null
 * while `isset($x->nothing)` is false, and why `count($x->kid)` is the number
 * of matches.  Every derived object is FRESH -- php caches no identity here,
 * and `$x->kid === $x->kid` is false under both engines.
 *
 * WHAT IT SHOWS.  var_dump/print_r/(array)/var_export/json_encode/
 * get_object_vars/__debugInfo all print php's get_properties table, which is
 * NOT the object's slots and is derived in SxeTable() below -- see the rules
 * stated there, every one of them read off php 8.5.9 rather than its source.
 *
 * WHAT IT IS NOT.  SimpleXMLElement declares no property, so every slot above
 * is PH7_MOD_HIDDEN; it declares neither ArrayAccess nor JsonSerializable, so
 * `$x['a']` is a dimension HANDLER (ph7_class::xDim) and `json_encode()` reads
 * the table; and it is Stringable, Countable and a RecursiveIterator, which it
 * satisfies with real declared methods rather than an engine vtable.
 */

/* One native method body, with the receiver's state already reachable. */
#define SXE_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)

/* The hidden slots. */
#define SXE_RES  "__res"
#define SXE_IT   "__it"
#define SXE_NM   "__nm"
#define SXE_NS   "__ns"
#define SXE_ISP  "__isp"
#define SXE_CUR  "__cur"
#define SXE_XPNS "__xpns"

/* php's iter.type. */
#define SXE_ITER_NONE     0
#define SXE_ITER_ELEMENT  1
#define SXE_ITER_CHILD    2
#define SXE_ITER_ATTRLIST 3

/* ===== The receiver's state ===== */

static phl_domnode * SxeResOf(ph7_class_instance *pObj)
{
	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,SXE_RES) : 0;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (phl_domnode *)pVal->x.pOther;
}
static xmlNodePtr SxeNodeOf(ph7_class_instance *pObj)
{
	phl_domnode *pNd = SxeResOf(pObj);
	return pNd ? (xmlNodePtr)pNd->pNode : 0;
}
static int SxeTypeOf(ph7_class_instance *pObj)
{
	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_IT) : SXE_ITER_NONE;
}
/*
 * A string slot that may be UNSET, which is not the same as empty: php's
 * `iter.name` and `iter.nsprefix` are pointers, and a NULL nsprefix means "no
 * filter" where an empty one would mean "the empty namespace".  Answers 0 for
 * an unset slot -- the length sentinel PH7_NativeAttrStr cannot give.
 */
static const char * SxeSlotStr(ph7_class_instance *pObj,const char *zSlot,int *pnLen)
{
	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,zSlot) : 0;
	if( pnLen ){
		*pnLen = 0;
	}
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0 ){
		return 0;
	}
	if( pnLen ){
		*pnLen = (int)SyBlobLength(&pVal->sBlob);
	}
	return (const char *)SyBlobData(&pVal->sBlob);
}
static const char * SxeIterName(ph7_class_instance *pObj,int *pnLen)
{
	return SxeSlotStr(pObj,SXE_NM,pnLen);
}
static const char * SxeNsFilter(ph7_class_instance *pObj,int *pnLen)
{
	return SxeSlotStr(pObj,SXE_NS,pnLen);
}
static int SxeIsPrefix(ph7_class_instance *pObj)
{
	return pObj ? (int)PH7_NativeAttrInt(pObj,SXE_ISP) : 0;
}
/* The receiver of a native method, when it really is one of ours. */
static ph7_class_instance * SxeThis(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	return pThis && PH7_NativeAttr(pThis,SXE_RES) ? pThis : 0;
}

/*
 * php's `match_ns`, the screen every walk here runs a node past.
 *
 * A NULL filter is not "match everything": it is "match what carries no
 * PREFIX", which is why `$x->children()` on a document with a default
 * namespace still yields the default-namespace children and skips the prefixed
 * ones.  A filter compares against the href, or against the prefix when the
 * caller said `$isPrefix`.  An EMPTY filter is php's "none at all" -- the
 * default `''` its two load functions carry, which must not mean "the empty
 * namespace".
 */
static int SxeMatchNs(xmlNodePtr pNode,const char *zNs,int nNs,int bPrefix)
{
	const xmlChar *zHave;
	if( pNode == 0 ){
		return 0;
	}
	if( zNs == 0 || nNs < 1 ){
		return pNode->ns == 0 || pNode->ns->prefix == 0;
	}
	if( pNode->ns == 0 ){
		return 0;
	}
	zHave = bPrefix ? pNode->ns->prefix : pNode->ns->href;
	if( zHave == 0 ){
		return 0;
	}
	return (int)xmlStrlen(zHave) == nNs && SyMemcmp(zHave,zNs,(sxu32)nNs) == 0;
}
/* ...and the receiver's own filter, applied to one node. */
static int SxeMatch(ph7_class_instance *pObj,xmlNodePtr pNode)
{
	int nNs = 0;
	const char *zNs = SxeNsFilter(pObj,&nNs);
	return SxeMatchNs(pNode,zNs,nNs,SxeIsPrefix(pObj));
}
/* Is this the name the receiver's question names? An unnamed question takes
 * every name. */
static int SxeNameMatch(ph7_class_instance *pObj,xmlNodePtr pNode)
{
	int nNm = 0;
	const char *zNm = SxeIterName(pObj,&nNm);
	if( zNm == 0 ){
		return 1;
	}
	if( pNode->name == 0 ){
		return 0;
	}
	return (int)xmlStrlen(pNode->name) == nNm
		&& SyMemcmp(pNode->name,zNm,(sxu32)nNm) == 0;
}

/*
 * php's `php_sxe_get_first_node`: the node this object's question STARTS at,
 * which for three of the four types is not the node it holds.
 *
 *   NONE      the node itself (even when it is an attribute).
 *   ELEMENT   the first child element with the question's name and namespace.
 *   CHILD     the first child ELEMENT the namespace filter takes -- a text
 *             node is never one, which is why `(string)$x->children()` on a
 *             text-only element is the empty string.
 *   ATTRLIST  the first attribute the filter (and the name, when the question
 *             carries one) takes.
 */
static xmlNodePtr SxeFirstNode(ph7_class_instance *pObj)
{
	xmlNodePtr pNode = SxeNodeOf(pObj);
	int iType = SxeTypeOf(pObj);
	xmlNodePtr pWalk;
	if( pNode == 0 || iType == SXE_ITER_NONE ){
		return pNode;
	}
	if( iType == SXE_ITER_ATTRLIST ){
		for( pWalk = (xmlNodePtr)pNode->properties ; pWalk ; pWalk = pWalk->next ){
			if( SxeMatch(pObj,pWalk) && SxeNameMatch(pObj,pWalk) ){
				return pWalk;
			}
		}
		return 0;
	}
	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type != XML_ELEMENT_NODE || !SxeMatch(pObj,pWalk) ){
			continue;
		}
		if( iType == SXE_ITER_ELEMENT && !SxeNameMatch(pObj,pWalk) ){
			continue;
		}
		return pWalk;
	}
	return 0;
}
/* The node AFTER pCur in the same walk -- the iterator's step, and the one
 * `use_iter` in the table below takes. */
static xmlNodePtr SxeNextNode(ph7_class_instance *pObj,xmlNodePtr pCur)
{
	int iType = SxeTypeOf(pObj);
	xmlNodePtr pWalk;
	if( pCur == 0 || iType == SXE_ITER_NONE ){
		return 0;
	}
	for( pWalk = pCur->next ; pWalk ; pWalk = pWalk->next ){
		if( iType == SXE_ITER_ATTRLIST ){
			if( SxeMatch(pObj,pWalk) && SxeNameMatch(pObj,pWalk) ){
				return pWalk;
			}
			continue;
		}
		if( pWalk->type != XML_ELEMENT_NODE || !SxeMatch(pObj,pWalk) ){
			continue;
		}
		if( iType == SXE_ITER_ELEMENT && !SxeNameMatch(pObj,pWalk) ){
			continue;
		}
		return pWalk;
	}
	return 0;
}

/*
 * The ITERATOR's walk, which is not php's `get_first_node` walk for the plain
 * question: `foreach ($x as ...)` over "this node" yields its element CHILDREN,
 * where `(string)$x` and `$x[0]` are about the node itself. php keeps the two
 * apart (php_sxe_reset_iterator starts at node->children for SXE_ITER_NONE,
 * php_sxe_get_first_node answers the node), and so does this: everything a
 * foreach, count() and the RecursiveIterator pair see comes through here.
 */
static xmlNodePtr SxeIterFirst(ph7_class_instance *pObj)
{
	xmlNodePtr pNode,pWalk;
	if( SxeTypeOf(pObj) != SXE_ITER_NONE ){
		return SxeFirstNode(pObj);
	}
	pNode = SxeNodeOf(pObj);
	if( pNode == 0 || pNode->type == XML_ATTRIBUTE_NODE ){
		return 0;
	}
	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type == XML_ELEMENT_NODE && SxeMatch(pObj,pWalk) ){
			return pWalk;
		}
	}
	return 0;
}
static xmlNodePtr SxeIterNext(ph7_class_instance *pObj,xmlNodePtr pCur)
{
	xmlNodePtr pWalk;
	if( pCur == 0 ){
		return 0;
	}
	if( SxeTypeOf(pObj) != SXE_ITER_NONE ){
		return SxeNextNode(pObj,pCur);
	}
	for( pWalk = pCur->next ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type == XML_ELEMENT_NODE && SxeMatch(pObj,pWalk) ){
			return pWalk;
		}
	}
	return 0;
}

/* ===== Text ===== */

/*
 * php's `xmlNodeListGetString(doc, node->children, 1)`: the text a node
 * CONTAINS, which walks its child list joining text and CDATA and stepping
 * over everything else.  This is `(string)$x` and it is what a child's value
 * is when the table below decides it is a string.
 */
static void SxeNodeText(SyBlob *pOut,xmlNodePtr pNode)
{
	xmlNodePtr pWalk;
	if( pNode == 0 ){
		return;
	}
	if( pNode->type == XML_COMMENT_NODE || pNode->type == XML_PI_NODE
	 || pNode->type == XML_TEXT_NODE || pNode->type == XML_CDATA_SECTION_NODE ){
		/* These carry their text in `content` and have no child list at all: the
		 * comment object php's table shows under `comment` stringifies to the
		 * comment's own words. */
		if( pNode->content ){
			SyBlobAppend(pOut,(const char *)pNode->content,
				(sxu32)SyStrlen((const char *)pNode->content));
		}
		return;
	}
	for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type == XML_TEXT_NODE || pWalk->type == XML_CDATA_SECTION_NODE ){
			if( pWalk->content ){
				SyBlobAppend(pOut,(const char *)pWalk->content,
					(sxu32)SyStrlen((const char *)pWalk->content));
			}
		}else if( pWalk->type == XML_ENTITY_REF_NODE ){
			/* An unexpanded entity reference contributes its replacement text,
			 * which is where libxml keeps it. */
			SxeNodeText(pOut,pWalk);
		}
	}
}
/* A blank text node in libxml's sense -- whitespace only. php's table skips
 * one where it would otherwise show the element's text. */
static int SxeBlankText(xmlNodePtr pNode)
{
	const xmlChar *z;
	if( pNode == 0 || pNode->content == 0 ){
		return 1;
	}
	for( z = pNode->content ; *z ; ++z ){
		if( *z != ' ' && *z != '\t' && *z != '\n' && *z != '\r' ){
			return 0;
		}
	}
	return 1;
}

/* ===== Making one ===== */

/*
 * A new object over pNode asking iType about it, of the RECEIVER's class -- a
 * subclass of SimpleXMLElement propagates through every navigation, which is
 * what makes `simplexml_load_string($s,'MySX')->kid` a MySX.
 *
 * The namespace filter is INHERITED unless the caller states one: `$x->
 * children('urn:a')->kid` means the `urn:a` kid, because the filter rode the
 * navigation.  BORROWED-free: the caller owns the returned reference.
 */
static ph7_class_instance * SxeNew(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,
	xmlNodePtr pNode,int iType,const char *zName,int nName,
	const char *zNs,int nNs,int bIsPrefix)
{
	ph7_class_instance *pObj;
	phl_domnode *pRes;
	ph7_value sVal;
	if( pClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(pVm,pClass);
	if( pObj == 0 ){
		return 0;
	}
	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));
	if( pRes == 0 ){
		PH7_ClassInstanceUnref(pObj);
		return 0;
	}
	pRes->pShell = pShell;
	pRes->pNode = pNode;
	PH7_MemObjInit(pVm,&sVal);
	sVal.x.pOther = pRes;
	sVal.iFlags = MEMOBJ_RES;
	PH7_NativeSetProp(pVm,pObj,SXE_RES,sizeof(SXE_RES)-1,&sVal);
	PH7_NativeSetAttrInt(pVm,pObj,SXE_IT,(sxi64)iType);
	if( zName ){
		PH7_NativeSetAttrStr(pVm,pObj,SXE_NM,zName,(sxu32)nName);
	}
	if( zNs && nNs > 0 ){
		PH7_NativeSetAttrStr(pVm,pObj,SXE_NS,zNs,(sxu32)nNs);
		PH7_NativeSetAttrInt(pVm,pObj,SXE_ISP,bIsPrefix ? 1 : 0);
	}
	return pObj;
}
/* The same, derived from an existing object: its class, its shell and (unless
 * the caller states one) its namespace filter. */
static ph7_class_instance * SxeDerive(ph7_vm *pVm,ph7_class_instance *pSrc,
	xmlNodePtr pNode,int iType,const char *zName,int nName)
{
	phl_domnode *pNd = SxeResOf(pSrc);
	int nNs = 0;
	const char *zNs = SxeNsFilter(pSrc,&nNs);
	return SxeNew(pVm,pSrc->pClass,pNd ? pNd->pShell : 0,pNode,iType,zName,nName,
		zNs,nNs,SxeIsPrefix(pSrc));
}
/* ...and hand it to PHP. */
static int SxeResultObj(ph7_context *pCtx,ph7_class_instance *pObj)
{
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/* ...and hand back one the CALLER still owns -- the cursor object, which the
 * receiver's slot keeps alive. ph7_result_value takes its own reference. */
static int SxeResultBorrowed(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_value sRes;
	PH7_MemObjInit(pCtx->pVm,&sRes);
	sRes.x.pOther = pObj;
	sRes.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx,&sRes);
	return PH7_OK;
}

/* ===== php's get_properties table ===== */

/*
 * The value ONE child element contributes, php's `_get_base_node_value`.
 *
 * A string when the node's FIRST child is a non-blank text node, and the whole
 * of the node's text then -- so `<i>a<j/>b</i>` is the string `ab`, elements
 * stepped over -- and a fresh SimpleXMLElement otherwise.  The three shapes
 * that make it an object are: no children at all, a first child that is not
 * text (an element, a comment, a CDATA section), and a first child that is
 * blank text.  `<l>  </l>` is an object, `<l> x </l>` is the string ` x `.
 */
static void SxeBaseValue(ph7_vm *pVm,ph7_class_instance *pSrc,xmlNodePtr pNode,
	ph7_value *pOut)
{
	xmlNodePtr pFirst = pNode ? pNode->children : 0;
	if( pFirst && pFirst->type == XML_TEXT_NODE && !SxeBlankText(pFirst) ){
		SyBlob sText;
		SyBlobInit(&sText,&pVm->sAllocator);
		SxeNodeText(&sText,pNode);
		PH7_MemObjRelease(pOut);
		PH7_MemObjInitFromString(pVm,pOut,0);
		PH7_MemObjStringAppend(pOut,(const char *)SyBlobData(&sText),SyBlobLength(&sText));
		SyBlobRelease(&sText);
		return;
	}
	{
		ph7_class_instance *pObj = SxeDerive(pVm,pSrc,pNode,SXE_ITER_NONE,0,0);
		PH7_MemObjRelease(pOut);
		PH7_MemObjInit(pVm,pOut);
		if( pObj ){
			pOut->x.pOther = pObj;
			pOut->iFlags = MEMOBJ_OBJ;
		}
	}
}
/*
 * php's `sxe_properties_add`: the first value under a name is stored plainly,
 * a second turns the entry into a list and both go in it, and every one after
 * appends.  This is why `<r><c>1</c></r>` shows `c => '1'` and
 * `<r><c>1</c><c>2</c></r>` shows `c => ['1','2']`.
 */
static void SxeTableAdd(ph7_vm *pVm,ph7_value *pTable,const char *zName,int nName,
	ph7_value *pVal)
{
	ph7_value *pHave = ph7_array_fetch(pTable,zName,nName);
	ph7_value sKey;
	if( pHave == 0 ){
		PH7_MemObjInitFromString(pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);
		ph7_array_add_elem(pTable,&sKey,pVal);
		PH7_MemObjRelease(&sKey);
		return;
	}
	if( (pHave->iFlags & MEMOBJ_HASHMAP) == 0 ){
		/* Promote the single value into php's two-element list. */
		ph7_value sList,sFirst;
		PH7_MemObjInit(pVm,&sFirst);
		PH7_MemObjStore(pHave,&sFirst);
		PH7_MemObjInit(pVm,&sList);
		if( PH7_MemObjToHashmap(&sList) != SXRET_OK ){
			PH7_MemObjRelease(&sFirst);
			return;
		}
		ph7_array_add_elem(&sList,0,&sFirst);
		ph7_array_add_elem(&sList,0,pVal);
		PH7_MemObjInitFromString(pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zName,(sxu32)nName);
		ph7_array_add_elem(pTable,&sKey,&sList);
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sList);
		PH7_MemObjRelease(&sFirst);
		return;
	}
	ph7_array_add_elem(pHave,0,pVal);
}
/*
 * php's `sxe_get_prop_hash` -- the table every presentation surface prints,
 * derived from php 8.5.9 across a matrix of document shapes and accessors.
 * The rules, in the order they run:
 *
 * 1. ATTRIBUTES.  Unless this is a `children()` object being asked by
 *    get_properties (a DEBUG walk asks even then, which is why var_dump of
 *    `children()` shows `@attributes` and `(array)` of the same object does
 *    not), every attribute of the START node that the filter takes goes under
 *    `@attributes`.  For an ELEMENT question the start node is the first
 *    MATCH, so `$x->kid` shows the first kid's attributes.
 *
 * 2. An ATTRIBUTE node has no children question: it shows its own value at the
 *    next index, and nothing else.  `$x['a']` is `[0 => '1']`.
 *
 * 3. Otherwise the walk starts at the start node's CHILDREN and keys by name
 *    -- EXCEPT for the one shape php walks the SET instead: a start node whose
 *    only child is a non-blank text node.  There the walk is over the matched
 *    siblings themselves and the keys are indices, which is what makes
 *    `$x->c` on two text-only `<c>` show `[0=>'one', 1=>'two']` while the same
 *    two EMPTY `<c/>` show nothing at all (their start node has no children,
 *    so the child walk begins at NULL).
 *
 * 4. In the child walk, text and CDATA nodes are stepped over, an element that
 *    fails the namespace filter is stepped over, and everything else -- an
 *    element, a comment (`comment`), a processing instruction (its target) --
 *    contributes its base value under its node name.
 */
static void SxeTable(ph7_vm *pVm,ph7_class_instance *pThis,int bDebug,ph7_value *pOut)
{
	int iType = SxeTypeOf(pThis);
	xmlNodePtr pNode = SxeNodeOf(pThis);
	xmlNodePtr pStart,pWalk;
	int bUseIter = 0;
	if( pNode == 0 ){
		return;
	}
	if( bDebug || iType != SXE_ITER_CHILD ){
		xmlNodePtr pAttrOn = iType == SXE_ITER_ELEMENT ? SxeFirstNode(pThis) : pNode;
		ph7_value sAttrs;
		int bAny = 0;
		PH7_MemObjInit(pVm,&sAttrs);
		if( pAttrOn && pAttrOn->type != XML_ENTITY_DECL
		 && PH7_MemObjToHashmap(&sAttrs) == SXRET_OK ){
			xmlAttrPtr pAttr;
			for( pAttr = pAttrOn->properties ; pAttr ; pAttr = pAttr->next ){
				SyBlob sText;
				ph7_value sKey,sVal;
				if( !SxeMatch(pThis,(xmlNodePtr)pAttr) ){
					continue;
				}
				if( iType == SXE_ITER_ATTRLIST && !SxeNameMatch(pThis,(xmlNodePtr)pAttr) ){
					continue;
				}
				SyBlobInit(&sText,&pVm->sAllocator);
				SxeNodeText(&sText,(xmlNodePtr)pAttr);
				PH7_MemObjInitFromString(pVm,&sKey,0);
				PH7_MemObjStringAppend(&sKey,(const char *)pAttr->name,
					(sxu32)xmlStrlen(pAttr->name));
				PH7_MemObjInitFromString(pVm,&sVal,0);
				PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),
					SyBlobLength(&sText));
				ph7_array_add_elem(&sAttrs,&sKey,&sVal);
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sVal);
				SyBlobRelease(&sText);
				bAny = 1;
			}
		}
		if( bAny ){
			ph7_value sKey;
			PH7_MemObjInitFromString(pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,"@attributes",sizeof("@attributes")-1);
			ph7_array_add_elem(pOut,&sKey,&sAttrs);
			PH7_MemObjRelease(&sKey);
		}
		PH7_MemObjRelease(&sAttrs);
	}
	if( iType == SXE_ITER_ATTRLIST ){
		return;
	}
	if( iType == SXE_ITER_CHILD ){
		/* `children()` walks the node's OWN child list -- php never moves it to
		 * the first match here, which is what makes the table of a children()
		 * object the same set of rows the element's own table shows. */
		pStart = SxeNodeOf(pThis);
		pWalk = pStart ? pStart->children : 0;
	}else{
		pStart = SxeFirstNode(pThis);
		if( pStart == 0 ){
			return;
		}
		if( pStart->type == XML_ATTRIBUTE_NODE ){
			SyBlob sText;
			ph7_value sVal;
			SyBlobInit(&sText,&pVm->sAllocator);
			SxeNodeText(&sText,pStart);
			PH7_MemObjInitFromString(pVm,&sVal,0);
			PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),SyBlobLength(&sText));
			ph7_array_add_elem(pOut,0,&sVal);
			PH7_MemObjRelease(&sVal);
			SyBlobRelease(&sText);
			return;
		}
		if( pStart->children && pStart->children->next == 0
		 && pStart->children->type == XML_TEXT_NODE && !SxeBlankText(pStart->children) ){
			bUseIter = 1;
			pWalk = pStart;
		}else{
			pWalk = pStart->children;
		}
	}
	while( pWalk ){
		ph7_value sVal;
		if( pWalk->type == XML_TEXT_NODE || pWalk->type == XML_CDATA_SECTION_NODE ){
			goto next_node;
		}
		if( iType == SXE_ITER_CHILD && pWalk->type != XML_ELEMENT_NODE ){
			/* `children()` walks with php's ITERATOR, which yields elements and
			 * nothing else -- so a comment shows in the table of the element and
			 * not in the table of its children(). */
			goto next_node;
		}
		/* The namespace filter screens CANDIDATE CHILDREN. It is not asked of the
		 * set walk: those nodes are the question's own subjects, already matched
		 * where the walk found them -- which is why an xpath result standing on a
		 * PREFIXED element shows that element's text, filter or no filter. */
		if( !bUseIter && pWalk->type == XML_ELEMENT_NODE && !SxeMatch(pThis,pWalk) ){
			goto next_node;
		}
		if( pWalk->name == 0 ){
			goto next_node;
		}
		PH7_MemObjInit(pVm,&sVal);
		SxeBaseValue(pVm,pThis,pWalk,&sVal);
		if( bUseIter ){
			ph7_array_add_elem(pOut,0,&sVal);
		}else{
			SxeTableAdd(pVm,pOut,(const char *)pWalk->name,(int)xmlStrlen(pWalk->name),&sVal);
		}
		PH7_MemObjRelease(&sVal);
next_node:
		pWalk = bUseIter ? SxeNextNode(pThis,pWalk) : pWalk->next;
	}
}

/* php's get_properties / get_debug_info / get_object_vars for a SimpleXML
 * object: all three are this table, unlike every other native class here (a
 * DateTime shows nothing to get_object_vars, a DOM node nothing to `(array)`).
 * That is why the hook answers every purpose. */
static sxi32 SxePresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	if( pThis == 0 || pOut == 0 || (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return SXERR_NOTFOUND;
	}
	SxeTable(pVm,pThis,bDebug,pOut);
	return SXRET_OK;
}
/*
 * php's cast_object for _IS_BOOL, which is the one cast a SimpleXML object does
 * not answer with `true`.
 *
 * A node the question actually FINDS is truthy; with none, the answer is
 * whether the TABLE has anything in it.  So `(bool)simplexml_load_string('<r/>')`
 * is false and `(bool)simplexml_load_string('<r a="1"/>')` is true -- an object
 * whose question is "this node" never finds a first node, because php's
 * `php_sxe_get_first_node` answers NULL for SXE_ITER_NONE when the caller
 * passes no node, and the cast is the one caller that does.
 */
static int SxeBool(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value sTable;
	int bTruthy;
	if( SxeTypeOf(pThis) != SXE_ITER_NONE && SxeFirstNode(pThis) != 0 ){
		return 1;
	}
	PH7_MemObjInit(pVm,&sTable);
	if( PH7_MemObjToHashmap(&sTable) != SXRET_OK ){
		return 1;
	}
	/* The emptiness question ALWAYS counts attributes -- php's own
	 * `sxe_prop_is_empty` has no `children()` exception where its get_properties
	 * does, so `(bool)$x->children()` on an element with attributes and no
	 * element children is TRUE while `(array)` of the same object is empty. */
	SxeTable(pVm,pThis,1,&sTable);
	bTruthy = ph7_array_count(&sTable) > 0;
	PH7_MemObjRelease(&sTable);
	return bTruthy;
}

/* ===== The three answers a plain read gives ===== */

/* `(string)$x`: the text of the first node the question finds, and an
 * attribute's own value when that is what it found. */
static void SxeToText(ph7_vm *pVm,ph7_class_instance *pThis,SyBlob *pOut)
{
	xmlNodePtr pNode = SxeTypeOf(pThis) == SXE_ITER_NONE
		? SxeNodeOf(pThis) : SxeFirstNode(pThis);
	(void)pVm;
	SxeNodeText(pOut,pNode);
}
/* `count($x)`: how many nodes the question stands for. For "this node" that is
 * its child ELEMENTS -- php counts what a foreach would yield, and a foreach
 * over an element yields its children. */
static sxi64 SxeCount(ph7_class_instance *pThis)
{
	xmlNodePtr pNode = SxeIterFirst(pThis);
	sxi64 n = 0;
	while( pNode ){
		n++;
		pNode = SxeIterNext(pThis,pNode);
	}
	return n;
}

/* ===== Property access: php's read_property / write_property ===== */

/*
 * `$x->name` -- the ELEMENT question, always answered and never a miss: php
 * hands back an object standing for the (possibly empty) set, so a name that
 * is not in the document is an object with count 0 and an empty table.  An
 * `isset()` is a different question and answers by whether the set has a first
 * node.
 */
static void SxePropRead(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	const SyString *pName = pCtx->pName;
	xmlNodePtr pNode = SxeNodeOf(pThis);
	ph7_class_instance *pSub;
	if( pNode == 0 ){
		return;
	}
	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){
		/* php's handler reaches an attribute list and finds no element to look
		 * under: the read answers null and a write is dropped on the floor. */
		pCtx->bAnswered = 1;
		if( pCtx->iMode != PH7_NATIVE_PROP_READ ){
			PH7_MemObjRelease(pCtx->pResult);
			ph7_value_bool(pCtx->pResult,0);
		}
		return;
	}
	if( SxeTypeOf(pThis) == SXE_ITER_ELEMENT ){
		/* Only a NAMED set collapses to its first match before asking for a
		 * child: `$x->a->b` means the b of the first a. "This node" and
		 * `children()` keep their own node, which is what makes
		 * `$x->children('urn:a')->kid` the urn:a KID OF THIS NODE and not a
		 * grandchild. */
		pNode = SxeFirstNode(pThis);
		if( pNode == 0 ){
			pCtx->bAnswered = 1;
			if( pCtx->iMode != PH7_NATIVE_PROP_READ ){
				PH7_MemObjRelease(pCtx->pResult);
				ph7_value_bool(pCtx->pResult,0);
			}
			return;
		}
	}
	pSub = SxeDerive(pVm,pThis,pNode,SXE_ITER_ELEMENT,
		SyStringData(pName),(int)SyStringLength(pName));
	if( pSub == 0 ){
		return;
	}
	pCtx->bAnswered = 1;
	if( pCtx->iMode == PH7_NATIVE_PROP_READ ){
		PH7_MemObjRelease(pCtx->pResult);
		PH7_MemObjInit(pVm,pCtx->pResult);
		pCtx->pResult->x.pOther = pSub;
		pCtx->pResult->iFlags = MEMOBJ_OBJ;
		return;
	}
	/* isset() / empty() / property_exists(): php asks the SET, not the object.
	 * `isset()` is "is there a node", and the two emptiness questions are the
	 * object's own truth -- which for a found node with empty text is still
	 * true, because the table carries it. */
	{
		int bAns;
		if( pCtx->iMode == PH7_NATIVE_PROP_ISSET || pCtx->iMode == PH7_NATIVE_PROP_EXISTS ){
			bAns = SxeFirstNode(pSub) != 0;
		}else{
			bAns = SxeBool(pVm,pSub);
		}
		PH7_MemObjRelease(pCtx->pResult);
		ph7_value_bool(pCtx->pResult,bAns);
		PH7_ClassInstanceUnref(pSub);
	}
}

/* ===== Writing ===== */

/*
 * A NUL-terminated copy of a slice, for the libxml calls that take one. The
 * copy lives in the caller's blob until it releases it -- a fixed buffer would
 * have to REFUSE a name longer than itself, and refusing silently is the one
 * answer php never gives.
 */
static const char * SxeCopyZ(ph7_vm *pVm,SyBlob *pBuf,const char *z,int n)
{
	SyBlobInit(pBuf,&pVm->sAllocator);
	if( z && n > 0 ){
		SyBlobAppend(pBuf,z,(sxu32)n);
	}
	SyBlobNullAppend(pBuf);
	return (const char *)SyBlobData(pBuf);
}

/*
 * The text a value becomes when it lands in the tree.
 *
 * php takes scalars and NULL through the ordinary string conversion, takes a
 * SimpleXMLElement as its own text (so `$a->x = $b->y` copies the TEXT, not
 * the node), and refuses everything else -- an array, and any other object,
 * including one that HAS a __toString().  The refusal names the destination:
 * `properties` for a property write, `attributes` for an attribute one.
 * Answers 0 when it refused, with the TypeError already worded into pCtx.
 */
static int SxeValueText(ph7_vm *pVm,ph7_value *pVal,int bAttr,SyBlob *pOut,
	const char **pzClass,char *zMsg,sxu32 nMsg)
{
	SyBlobInit(pOut,&pVm->sAllocator);
	if( pVal == 0 ){
		SyBlobNullAppend(pOut);
		return 1;
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		*pzClass = "TypeError";
		SyBufferFormat(zMsg,nMsg,
			"It's not possible to assign a complex type to %s, array given",
			bAttr ? "attributes" : "properties");
		return 0;
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
		if( pObj && PH7_NativeAttr(pObj,SXE_RES) ){
			SxeToText(pVm,pObj,pOut);
			SyBlobNullAppend(pOut);
			return 1;
		}
		*pzClass = "TypeError";
		SyBufferFormat(zMsg,nMsg,
			"It's not possible to assign a complex type to %s, %.*s given",
			bAttr ? "attributes" : "properties",
			pObj ? (int)pObj->pClass->sName.nByte : 6,
			pObj ? pObj->pClass->sName.zString : "object");
		return 0;
	}
	{
		int nLen = 0;
		const char *zStr = ph7_value_to_string(pVal,&nLen);
		if( nLen > 0 ){
			SyBlobAppend(pOut,zStr,(sxu32)nLen);
		}
	}
	SyBlobNullAppend(pOut);
	return 1;
}
/*
 * Put text into an element or an attribute the way php's `change_node_zval`
 * does: the string is ENCODED first and then set as content, so a `&` in a
 * value is stored as data and comes back out escaped -- unlike addChild(),
 * whose value libxml PARSES.  An empty string leaves the node childless, which
 * is what makes `$x->d = null` print `<d/>` and not `<d></d>`.
 */
static void SxeSetText(xmlNodePtr pNode,const char *zText)
{
	xmlChar *pEnc;
	if( pNode == 0 ){
		return;
	}
	if( zText == 0 || zText[0] == 0 ){
		xmlNodeSetContent(pNode,0);
		return;
	}
	pEnc = xmlEncodeEntitiesReentrant(pNode->doc,(const xmlChar *)zText);
	xmlNodeSetContent(pNode,pEnc ? pEnc : (const xmlChar *)zText);
	if( pEnc ){
		xmlFree(pEnc);
	}
}
/* A new element under pParent carrying pParent's namespace -- php's
 * `xmlNewTextChild(node, node->ns, name, value)`, which is why a child created
 * under a default-namespace root is in that namespace too. */
static xmlNodePtr SxeNewChild(ph7_vm *pVm,xmlNodePtr pParent,const char *zName,int nName,
	const char *zText)
{
	SyBlob sName;
	const char *zZ;
	xmlNodePtr pNew;
	if( pParent == 0 || zName == 0 || nName < 1 ){
		return 0;
	}
	zZ = SxeCopyZ(pVm,&sName,zName,nName);
	/* An xmlDoc is not an xmlNode past its first eight fields -- reading `ns`
	 * off one lands on an int. A document parent carries no namespace anyway.
	 * The text goes in RAW (escaped at serialize time), and an EMPTY one still
	 * makes a text child -- which is why creating `$x->c = null` prints
	 * `<c></c>` where setting an existing `<c>` to null prints `<c/>`. */
	pNew = xmlNewTextChild(pParent,
		pParent->type == XML_ELEMENT_NODE ? pParent->ns : 0,(const xmlChar *)zZ,
		(const xmlChar *)zText);
	SyBlobRelease(&sName);
	return pNew;
}
/* The attribute of pNode with this name that the filter takes, or NULL. */
static xmlAttrPtr SxeFindAttr(ph7_class_instance *pThis,xmlNodePtr pNode,
	const char *zName,int nName)
{
	xmlAttrPtr pAttr;
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){
		if( !SxeMatch(pThis,(xmlNodePtr)pAttr) ){
			continue;
		}
		if( pAttr->name && (int)xmlStrlen(pAttr->name) == nName
		 && SyMemcmp(pAttr->name,zName,(sxu32)nName) == 0 ){
			return pAttr;
		}
	}
	return 0;
}
/*
 * The node a WRITE lands on or beside: the object's own node for "this node",
 * and the first match otherwise -- MATERIALIZED when there is none, so
 * `$x->a->b = 'v'` on `<r/>` creates `<a>` before it creates `<b>`.  Answers 0
 * for a question that has nothing to write through (an attribute list, an
 * object with no node).
 */
static xmlNodePtr SxeWriteBase(ph7_vm *pVm,ph7_class_instance *pThis,int bCreate)
{
	int iType = SxeTypeOf(pThis);
	xmlNodePtr pNode = SxeNodeOf(pThis);
	xmlNodePtr pFirst;
	if( pNode == 0 || iType == SXE_ITER_ATTRLIST ){
		return iType == SXE_ITER_ATTRLIST ? pNode : 0;
	}
	if( iType == SXE_ITER_NONE || iType == SXE_ITER_CHILD ){
		return pNode;
	}
	pFirst = SxeFirstNode(pThis);
	if( pFirst == 0 && bCreate ){
		int nNm = 0;
		const char *zNm = SxeIterName(pThis,&nNm);
		pFirst = SxeNewChild(pVm,pNode,zNm,nNm,0);
	}
	return pFirst;
}

/*
 * A warning php raises from a WRITE, which is not a call: there is no accessor
 * name to print in front of it, so php attributes it to the caller's scope --
 * `main(): Cannot assign to an array of nodes`, `f(): ...` inside a function.
 * Same shape ext/dom's property writes use.
 */
static void SxeCallerWarn(ph7_vm *pVm,const char *zFormat,...)
{
	SyBlob sFn,sMsg;
	SyString sName;
	va_list ap;
	SyBlobInit(&sFn,&pVm->sAllocator);
	SyBlobInit(&sMsg,&pVm->sAllocator);
	PH7_VmActiveFuncName(pVm,&sFn);
	va_start(ap,zFormat);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	va_end(ap);
	SyBlobNullAppend(&sMsg);
	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));
	PH7_VmThrowError(pVm,&sName,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
	SyBlobRelease(&sFn);
}
/*
 * `$x->name = value` -- php's write_property.
 *
 * The write lands on the ONE child element of that name, creates it when there
 * is none, and REFUSES when there is more than one: php cannot tell which of a
 * set the program meant, and says so in a warning rather than picking.  An
 * attribute list has no children to write, and php's handler drops the write
 * without a word.
 */
static void SxePropStore(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	const SyString *pName = pCtx->pName;
	ph7_value *pValue = pCtx->pResult;
	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,TRUE);
	xmlNodePtr pWalk,pHit = 0;
	int nHit = 0;
	SyBlob sText;
	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST || pBase == 0
	 || pBase->type != XML_ELEMENT_NODE ){
		return;
	}
	for( pWalk = pBase->children ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type != XML_ELEMENT_NODE || !SxeMatch(pThis,pWalk) ){
			continue;
		}
		if( pWalk->name == 0
		 || (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)
		 || SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){
			continue;
		}
		if( pHit == 0 ){
			pHit = pWalk;
		}
		nHit++;
	}
	if( nHit > 1 ){
		SxeCallerWarn(pVm,
			"Cannot assign to an array of nodes (duplicate subnodes or attr detected)");
		return;
	}
	if( !SxeValueText(pVm,pValue,0,&sText,&pCtx->zThrowClass,
		pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg)) ){
		return;
	}
	if( pHit ){
		SxeSetText(pHit,(const char *)SyBlobData(&sText));
	}else{
		SxeNewChild(pVm,pBase,SyStringData(pName),(int)SyStringLength(pName),
			(const char *)SyBlobData(&sText));
	}
	SyBlobRelease(&sText);
}
/* `unset($x->name)` -- php removes EVERY matching child, not the first. */
static void SxePropUnset(ph7_vm *pVm,ph7_class_instance *pThis,const SyString *pName)
{
	xmlNodePtr pBase = SxeWriteBase(pVm,pThis,FALSE);
	xmlNodePtr pWalk,pNext;
	if( SxeTypeOf(pThis) == SXE_ITER_ATTRLIST || pBase == 0
	 || pBase->type != XML_ELEMENT_NODE ){
		return;
	}
	for( pWalk = pBase->children ; pWalk ; pWalk = pNext ){
		pNext = pWalk->next;
		if( pWalk->type != XML_ELEMENT_NODE || !SxeMatch(pThis,pWalk) ){
			continue;
		}
		if( pWalk->name == 0
		 || (sxu32)xmlStrlen(pWalk->name) != SyStringLength(pName)
		 || SyMemcmp(pWalk->name,SyStringData(pName),SyStringLength(pName)) != 0 ){
			continue;
		}
		xmlUnlinkNode(pWalk);
		xmlFreeNode(pWalk);
	}
}
/*
 * ph7_class::xProp -- php's read_property / has_property / write_property /
 * unset_property for a class that keeps no property slot at all.
 *
 * Every name is the handler's: SimpleXMLElement declares nothing, so the
 * ordinary path would create a dynamic property where php asks the document.
 * A subclass's OWN declared property is a slot and never reaches here (the
 * engine consults the hook only where the instance has none), which is the
 * one place php and this differ -- php's handler owns those too.
 */
static void SxePropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){
		/* Every name is: the class declares no property, so php's write_property
		 * stands where a dynamic property would be created. */
		pCtx->bAnswered = 1;
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_PROP_WRITE ){
		/* Asked before the value exists, and this handler really STORES -- so it
		 * declines here and takes the write at STORE below, which is the door
		 * every overloaded write shape ends at. Answering here would SWALLOW the
		 * store: the opcode would consume the access and the value would land
		 * nowhere. */
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){
		SxePropStore(pVm,pThis,pCtx);
		pCtx->bAnswered = 1;
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_PROP_UNSET ){
		SxePropUnset(pVm,pThis,pCtx->pName);
		pCtx->bAnswered = 1;
		return;
	}
	SxePropRead(pVm,pThis,pCtx);
}

/* ===== Dimensions: php's read_dimension / write_dimension ===== */

/*
 * php's dimension rules turn on the OFFSET'S TYPE, not on its text: a string
 * offset is an ATTRIBUTE name and an integer one is a position in the set.
 * That is why `$x['0']` makes an attribute called `0` and `$x[0]` reaches the
 * element -- the two spellings are different questions.
 */
static int SxeDimIsInt(ph7_value *pOffset)
{
	return pOffset != 0 && (pOffset->iFlags & (MEMOBJ_INT|MEMOBJ_REAL|MEMOBJ_BOOL)) != 0
		&& (pOffset->iFlags & MEMOBJ_STRING) == 0;
}
/* The node at a positional offset in this object's walk, or NULL. A negative
 * offset is php's first node: its loop counts UP to the offset and a negative
 * one never advances it. */
static xmlNodePtr SxeNodeAtOffset(ph7_class_instance *pThis,sxi64 iOfs,sxi64 *pnCount)
{
	xmlNodePtr pNode = SxeFirstNode(pThis);
	xmlNodePtr pHit = 0;
	sxi64 n = 0;
	if( SxeTypeOf(pThis) == SXE_ITER_NONE ){
		if( pnCount ){
			*pnCount = pNode ? 1 : 0;
		}
		return iOfs <= 0 ? pNode : 0;
	}
	while( pNode ){
		if( n == iOfs || (iOfs < 0 && n == 0) ){
			pHit = pNode;
			if( pnCount == 0 ){
				return pHit;
			}
		}
		n++;
		pNode = SxeNextNode(pThis,pNode);
	}
	if( pnCount ){
		*pnCount = n;
	}
	return pHit;
}
/* The element an ATTRIBUTE offset is asked of. */
static xmlNodePtr SxeAttrBase(ph7_class_instance *pThis)
{
	int iType = SxeTypeOf(pThis);
	if( iType == SXE_ITER_NONE || iType == SXE_ITER_CHILD || iType == SXE_ITER_ATTRLIST ){
		return SxeNodeOf(pThis);
	}
	return SxeFirstNode(pThis);
}
static void SxeDimRead(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	ph7_class_instance *pSub = 0;
	int bIsset = pCtx->iMode == PH7_NATIVE_DIM_ISSET;
	int bEmpty = pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY;
	xmlNodePtr pHit = 0;
	if( SxeDimIsInt(pCtx->pOffset) ){
		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pCtx->pOffset),0);
	}else if( pCtx->pOffset ){
		int nName = 0;
		const char *zName = ph7_value_to_string(pCtx->pOffset,&nName);
		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);
	}
	if( pHit == 0 ){
		if( bIsset || bEmpty ){
			PH7_MemObjRelease(pCtx->pResult);
			ph7_value_bool(pCtx->pResult,0);
		}
		return;
	}
	if( bIsset ){
		PH7_MemObjRelease(pCtx->pResult);
		ph7_value_bool(pCtx->pResult,1);
		return;
	}
	if( bEmpty ){
		/* php's `check_empty` reads the offset's TEXT and judges THAT, so an
		 * attribute holding "0" is empty() while the object a read of it hands
		 * back is truthy. */
		SyBlob sText;
		ph7_value sVal;
		SyBlobInit(&sText,&pVm->sAllocator);
		SxeNodeText(&sText,pHit);
		PH7_MemObjInitFromString(pVm,&sVal,0);
		PH7_MemObjStringAppend(&sVal,(const char *)SyBlobData(&sText),SyBlobLength(&sText));
		PH7_MemObjRelease(pCtx->pResult);
		ph7_value_bool(pCtx->pResult,ph7_value_to_bool(&sVal));
		PH7_MemObjRelease(&sVal);
		SyBlobRelease(&sText);
		return;
	}
	pSub = SxeDerive(pVm,pThis,pHit,SXE_ITER_NONE,0,0);
	if( pSub == 0 ){
		return;
	}
	PH7_MemObjRelease(pCtx->pResult);
	PH7_MemObjInit(pVm,pCtx->pResult);
	pCtx->pResult->x.pOther = pSub;
	pCtx->pResult->iFlags = MEMOBJ_OBJ;
}
/*
 * `$x[$k] = $v`, `$x[] = $v` and `unset($x[$k])`.
 *
 * A string offset is an attribute: set where it exists, created where it does
 * not, and on a MISSING element the element is created first, so
 * `$x->kid['a'] = '1'` on `<r/>` yields `<r><kid a="1"/></r>`.
 *
 * An integer offset is a position, and the write goes where php's does:
 *   * "this node" writes the node's own text, warning when the offset is past
 *     the one node it stands for;
 *   * a NAMED set writes the offset-th match, and past the end warns and then
 *     appends a fresh sibling of the first match;
 *   * `children()` -- the one question with no name -- creates a SIBLING OF ITS
 *     OWN NODE carrying that node's name, which for a root element means a
 *     second root. php's own answer, degenerate and reproduced.
 */
static void SxeDimWrite(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)
{
	int iType = SxeTypeOf(pThis);
	SyBlob sText;
	if( pDim->pOffset && !SxeDimIsInt(pDim->pOffset) ){
		int nName = 0;
		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);
		xmlNodePtr pBase = iType == SXE_ITER_ELEMENT
			? SxeWriteBase(pVm,pThis,TRUE) : SxeAttrBase(pThis);
		xmlAttrPtr pAttr;
		if( pBase == 0 || pBase->type != XML_ELEMENT_NODE || nName < 1 ){
			return;
		}
		if( !SxeValueText(pVm,pDim->pResult,1,&sText,&pDim->zThrowClass,
			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){
			return;
		}
		pAttr = SxeFindAttr(pThis,pBase,zName,nName);
		if( pAttr == 0 && iType != SXE_ITER_ATTRLIST ){
			/* An attribute list WRITES the ones it lists and creates none: php's
			 * `$x->attributes()['zz'] = 'v'` is a silent no-op where
			 * `$x['zz'] = 'v'` makes the attribute. */
			SyBlob sNm;
			pAttr = xmlNewProp(pBase,
				(const xmlChar *)SxeCopyZ(pVm,&sNm,zName,nName),(const xmlChar *)"");
			SyBlobRelease(&sNm);
		}
		if( pAttr ){
			SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sText));
		}
		SyBlobRelease(&sText);
		return;
	}
	if( iType == SXE_ITER_ATTRLIST ){
		if( pDim->pOffset == 0 ){
			pDim->zThrowClass = "ValueError";
			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),
				"Cannot append to an attribute list");
			return;
		}
		{
			xmlNodePtr pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);
			if( pHit && SxeValueText(pVm,pDim->pResult,1,&sText,&pDim->zThrowClass,
				pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){
				SxeSetText(pHit,(const char *)SyBlobData(&sText));
				SyBlobRelease(&sText);
			}
		}
		return;
	}
	if( iType == SXE_ITER_NONE ){
		xmlNodePtr pNode = SxeNodeOf(pThis);
		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : 0;
		if( pDim->pOffset == 0 || pNode == 0 ){
			pDim->zThrowClass = "ValueError";
			SyBufferFormat(pDim->zThrowMsg,sizeof(pDim->zThrowMsg),
				"Cannot append to an attribute list");
			return;
		}
		if( iOfs > 0 ){
			SxeCallerWarn(pVm,
				"Cannot add element %s number %qd when only 0 such elements exist",
				pNode->name ? (const char *)pNode->name : "",iOfs);
		}
		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,
			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){
			SxeSetText(pNode,(const char *)SyBlobData(&sText));
			SyBlobRelease(&sText);
		}
		return;
	}
	if( iType == SXE_ITER_ELEMENT ){
		sxi64 nCount = 0;
		sxi64 iOfs = pDim->pOffset ? ph7_value_to_int64(pDim->pOffset) : -1;
		xmlNodePtr pHit = pDim->pOffset ? SxeNodeAtOffset(pThis,iOfs,&nCount) : 0;
		int nNm = 0;
		const char *zNm = SxeIterName(pThis,&nNm);
		if( pHit == 0 && pDim->pOffset && iOfs > nCount ){
			SxeCallerWarn(pVm,
				"Cannot add element %.*s number %qd when only %qd such elements exist",
				nNm,zNm ? zNm : "",iOfs,nCount);
		}
		if( !SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,
			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){
			return;
		}
		if( pHit ){
			SxeSetText(pHit,(const char *)SyBlobData(&sText));
		}else{
			xmlNodePtr pFirst = SxeFirstNode(pThis);
			if( pFirst && pFirst->parent ){
				SxeNewChild(pVm,pFirst->parent,(const char *)pFirst->name,
					(int)xmlStrlen(pFirst->name),(const char *)SyBlobData(&sText));
			}else{
				SxeNewChild(pVm,SxeNodeOf(pThis),zNm,nNm,(const char *)SyBlobData(&sText));
			}
		}
		SyBlobRelease(&sText);
		return;
	}
	/* SXE_ITER_CHILD: php's own degenerate answer, above. */
	{
		xmlNodePtr pNode = SxeNodeOf(pThis);
		if( pNode == 0 || pNode->parent == 0 ){
			return;
		}
		if( SxeValueText(pVm,pDim->pResult,0,&sText,&pDim->zThrowClass,
			pDim->zThrowMsg,sizeof(pDim->zThrowMsg)) ){
			SxeNewChild(pVm,pNode->parent,(const char *)pNode->name,
				(int)xmlStrlen(pNode->name),(const char *)SyBlobData(&sText));
			SyBlobRelease(&sText);
		}
	}
}
static void SxeDimUnset(ph7_class_instance *pThis,PH7_NativeDimCtx *pDim)
{
	xmlNodePtr pHit = 0;
	if( SxeDimIsInt(pDim->pOffset) ){
		pHit = SxeNodeAtOffset(pThis,ph7_value_to_int64(pDim->pOffset),0);
	}else if( pDim->pOffset ){
		int nName = 0;
		const char *zName = ph7_value_to_string(pDim->pOffset,&nName);
		pHit = (xmlNodePtr)SxeFindAttr(pThis,SxeAttrBase(pThis),zName,nName);
	}
	if( pHit == 0 ){
		return;
	}
	if( pHit->type == XML_ATTRIBUTE_NODE ){
		xmlRemoveProp((xmlAttrPtr)pHit);
		return;
	}
	xmlUnlinkNode(pHit);
	/* php leaves the node in the document's own ownership rather than freeing
	 * it: an object still standing on it must not read freed memory. The shell
	 * frees it with the tree. */
	SySetPut(&SxeResOf(pThis)->pShell->aOrphans,(const void *)&pHit);
}
/* ph7_class::xDim -- one callback for all five modes. */
static void SxeDimHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	if( PH7_NativeAttr(pThis,SXE_RES) == 0 ){
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_DIM_READ || pCtx->iMode == PH7_NATIVE_DIM_ISSET
	 || pCtx->iMode == PH7_NATIVE_DIM_NOTEMPTY ){
		SxeDimRead(pVm,pThis,pCtx);
		return;
	}
	if( pCtx->pResult == 0 && pCtx->iMode != PH7_NATIVE_DIM_UNSET ){
		/* The refusal-WORDING probe (PH7_ClassNativeDimRefusal), which carries
		 * neither an offset nor a value. This class has no sentence of its own:
		 * every write it cannot take is php's generic one, so leaving the probe
		 * unanswered is the right answer. */
		return;
	}
	if( pCtx->iMode == PH7_NATIVE_DIM_UNSET ){
		SxeDimUnset(pThis,pCtx);
		pCtx->bStored = 1;
		return;
	}
	SxeDimWrite(pVm,pThis,pCtx);
	pCtx->bStored = 1;
}

/* ===== The methods ===== */

/* The node a method that WORKS ON ONE node uses: "this node" for the plain
 * question and the first match otherwise. */
static xmlNodePtr SxeMethodNode(ph7_class_instance *pThis)
{
	return SxeTypeOf(pThis) == SXE_ITER_NONE ? SxeNodeOf(pThis) : SxeFirstNode(pThis);
}
/* SimpleXMLElement::getName(): the first node's name, and the empty string for
 * a question that finds none -- php answers "" rather than false. */
SXE_METHOD(vm_builtin_SimpleXMLElement_getName)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;
	(void)nArg; (void)apArg;
	if( pNode == 0 || pNode->name == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));
	return PH7_OK;
}
/* SimpleXMLElement::__toString(). */
SXE_METHOD(vm_builtin_SimpleXMLElement_toString)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	SyBlob sText;
	(void)nArg; (void)apArg;
	if( pThis == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	SyBlobInit(&sText,&pCtx->pVm->sAllocator);
	SxeToText(pCtx->pVm,pThis,&sText);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sText),(int)SyBlobLength(&sText));
	SyBlobRelease(&sText);
	return PH7_OK;
}
/* SimpleXMLElement::count(). */
SXE_METHOD(vm_builtin_SimpleXMLElement_count)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	(void)nArg; (void)apArg;
	ph7_result_int64(pCtx,pThis ? SxeCount(pThis) : 0);
	return PH7_OK;
}
/* SimpleXMLElement::__debugInfo(): php's get_debug_info handler, reachable as a
 * method because php declares it as one. */
SXE_METHOD(vm_builtin_SimpleXMLElement_debugInfo)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	(void)nArg; (void)apArg;
	if( pOut == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pThis ){
		SxeTable(pCtx->pVm,pThis,1,pOut);
	}
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}
/*
 * children() and attributes(), which are the same call with a different
 * question: both anchor at the node the receiver's question finds and both
 * take the namespace filter from their ARGUMENTS -- a filter the receiver
 * carried is not inherited through them, only through navigation.
 */
static int SxeSubQuestion(ph7_context *pCtx,int nArg,ph7_value **apArg,int iType)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;
	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;
	const char *zNs = 0;
	int nNs = 0,bPrefix = 0;
	if( pThis == 0 || pNode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		zNs = ph7_value_to_string(apArg[0],&nNs);
	}
	if( nArg > 1 ){
		bPrefix = ph7_value_to_bool(apArg[1]);
	}
	return SxeResultObj(pCtx,SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,
		pNode,iType,0,0,zNs,nNs,bPrefix));
}
SXE_METHOD(vm_builtin_SimpleXMLElement_children)
{
	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_CHILD);
}
SXE_METHOD(vm_builtin_SimpleXMLElement_attributes)
{
	return SxeSubQuestion(pCtx,nArg,apArg,SXE_ITER_ATTRLIST);
}

/*
 * getNamespaces($recursive) -- the namespaces the node (and, recursively, its
 * subtree) USES: its own, and each of its attributes'.  Keyed by prefix, with
 * a default namespace under the empty key, and the FIRST binding of a prefix
 * wins.
 */
static void SxeAddNs(ph7_vm *pVm,ph7_value *pOut,xmlNsPtr pNs)
{
	const char *zPfx = pNs && pNs->prefix ? (const char *)pNs->prefix : "";
	int nPfx = (int)SyStrlen(zPfx);
	ph7_value sKey,sVal;
	if( pNs == 0 || pNs->href == 0 || ph7_array_fetch(pOut,zPfx,nPfx) != 0 ){
		return;
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);
	PH7_MemObjInitFromString(pVm,&sVal,0);
	PH7_MemObjStringAppend(&sVal,(const char *)pNs->href,(sxu32)xmlStrlen(pNs->href));
	ph7_array_add_elem(pOut,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
}
static void SxeUsedNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)
{
	xmlAttrPtr pAttr;
	if( pNode == 0 ){
		return;
	}
	if( pNode->ns ){
		SxeAddNs(pVm,pOut,pNode->ns);
	}
	for( pAttr = pNode->properties ; pAttr ; pAttr = pAttr->next ){
		if( pAttr->ns ){
			SxeAddNs(pVm,pOut,pAttr->ns);
		}
	}
	if( bRecursive ){
		xmlNodePtr pWalk;
		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){
			if( pWalk->type == XML_ELEMENT_NODE ){
				SxeUsedNs(pVm,pWalk,1,pOut);
			}
		}
	}
}
SXE_METHOD(vm_builtin_SimpleXMLElement_getNamespaces)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);
	if( pOut == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pThis ){
		SxeUsedNs(pCtx->pVm,SxeMethodNode(pThis),bRec,pOut);
	}
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}
/*
 * getDocNamespaces($recursive, $fromRoot) -- the namespaces DECLARED, which is
 * a different list and asked of a different node: this one reads the object's
 * OWN node rather than the node its question finds, so `$x->kid->
 * getDocNamespaces(false,false)` answers the declarations on `$x`'s node --
 * the parent -- because that is what an element-set object holds.
 */
static void SxeDeclaredNs(ph7_vm *pVm,xmlNodePtr pNode,int bRecursive,ph7_value *pOut)
{
	xmlNsPtr pNs;
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		return;
	}
	for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){
		SxeAddNs(pVm,pOut,pNs);
	}
	if( bRecursive ){
		xmlNodePtr pWalk;
		for( pWalk = pNode->children ; pWalk ; pWalk = pWalk->next ){
			SxeDeclaredNs(pVm,pWalk,1,pOut);
		}
	}
}
SXE_METHOD(vm_builtin_SimpleXMLElement_getDocNamespaces)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	phl_domnode *pNd = pThis ? SxeResOf(pThis) : 0;
	int bRec = nArg > 0 && ph7_value_to_bool(apArg[0]);
	int bRoot = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;
	xmlNodePtr pNode = pThis ? SxeNodeOf(pThis) : 0;
	ph7_value *pOut;
	if( pNode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( bRoot ){
		xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : pNode->doc;
		pNode = pDoc ? xmlDocGetRootElement(pDoc) : 0;
	}
	pOut = ph7_context_new_array(pCtx);
	if( pOut == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SxeDeclaredNs(pCtx->pVm,pNode,bRec,pOut);
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}

/*
 * asXML() / saveXML().
 *
 * php dumps the whole DOCUMENT -- declaration, trailing newline and all -- when
 * the node it found is the document's own child, and the NODE alone otherwise.
 * That is one test on the parent, and it is why `$x->asXML()` prints a header
 * and `$x->kid->asXML()` does not; it is also why the same call on a root that
 * something UNLINKED prints the bare element.
 */
static int SxeDumpXml(ph7_class_instance *pThis,SyBlob *pOut)
{
	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;
	if( pNode == 0 ){
		return 0;
	}
	if( pNode->parent && (pNode->parent->type == XML_DOCUMENT_NODE
	 || pNode->parent->type == XML_HTML_DOCUMENT_NODE) ){
		xmlChar *zBuf = 0;
		int nBuf = 0;
		xmlDocDumpMemory(pNode->doc,&zBuf,&nBuf);
		if( zBuf == 0 ){
			return 0;
		}
		SyBlobAppend(pOut,(const char *)zBuf,(sxu32)nBuf);
		xmlFree(zBuf);
		return 1;
	}
	{
		/* One node, through the same output buffer ext/dom dumps a DTD's children
		 * with -- xmlNodeDump itself is deprecated from libxml 2.12 and the MSVC
		 * gate refuses a deprecated symbol under /WX. */
		xmlBufferPtr pBuf = xmlBufferCreate();
		xmlOutputBufferPtr pDump = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;
		if( pDump == 0 ){
			if( pBuf ){
				xmlBufferFree(pBuf);
			}
			return 0;
		}
		xmlNodeDumpOutput(pDump,pNode->doc,pNode,0,0,0);
		xmlOutputBufferFlush(pDump);
		if( xmlBufferContent(pBuf) ){
			SyBlobAppend(pOut,(const char *)xmlBufferContent(pBuf),
				(sxu32)xmlBufferLength(pBuf));
		}
		xmlOutputBufferClose(pDump);
		xmlBufferFree(pBuf);
		return 1;
	}
}
SXE_METHOD(vm_builtin_SimpleXMLElement_asXML)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	SyBlob sOut;
	int bOk;
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	bOk = SxeDumpXml(pThis,&sOut);
	if( !bOk ){
		SyBlobRelease(&sOut);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0 ){
		/* php writes through its OWN stream layer, so a wrapper and a userland
		 * stream are valid destinations here exactly as they are for
		 * DOMDocument::save(). */
		int nFile = 0;
		const char *zFile = ph7_value_to_string(apArg[0],&nFile);
		const ph7_io_stream *pStream = PH7_VmGetStreamDevice(pCtx->pVm,&zFile,nFile);
		void *pHandle = (pStream && pStream->xWrite)
			? PH7_StreamOpenHandle(pCtx->pVm,pStream,zFile,
				PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,
				ph7_function_name(pCtx)) : 0;
		int bWrote = 0;
		if( pHandle ){
			bWrote = SyBlobLength(&sOut) == 0
				|| pStream->xWrite(pHandle,(const void *)SyBlobData(&sOut),
					(ph7_int64)SyBlobLength(&sOut)) >= 0;
			PH7_StreamCloseHandle(pStream,pHandle);
		}
		SyBlobRelease(&sOut);
		ph7_result_bool(pCtx,bWrote);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/*
 * xpath(): php anchors the expression at the node the question finds, evaluates
 * it with the prefixes registerXPathNamespace() left on THIS object, and takes
 * only a node set.
 *
 * Anything else -- a `count()`, a `string()`, a comparison -- is a warning and
 * `false`, named by the type libxml answered with. An attribute list has no
 * node to anchor at and answers null, and so does a set that found none.
 */
static int SxeRegisterOne(ph7_value *pKey,ph7_value *pVal,void *pUser)
{
	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUser;
	int nPfx = 0,nUri = 0;
	const char *zPfx = ph7_value_to_string(pKey,&nPfx);
	const char *zUri = ph7_value_to_string(pVal,&nUri);
	SyBlob sP,sU;
	if( nPfx < 1 || pKey->pVm == 0 ){
		return PH7_OK;
	}
	SxeCopyZ(pKey->pVm,&sP,zPfx,nPfx);
	SxeCopyZ(pKey->pVm,&sU,zUri,nUri);
	xmlXPathRegisterNs(pXCtx,(const xmlChar *)SyBlobData(&sP),
		(const xmlChar *)SyBlobData(&sU));
	SyBlobRelease(&sP);
	SyBlobRelease(&sU);
	return PH7_OK;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_xpath)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;
	xmlXPathContextPtr pXCtx;
	xmlXPathObjectPtr pObj;
	ph7_value *pReg,*pOut;
	xmlNsPtr *aNs = 0;
	int nExpr = 0;
	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],&nExpr) : "";
	SyBlob sExpr;
	sxu32 nMark;
	int i;
	if( pThis == 0 || pNode == 0 || SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pXCtx = xmlXPathNewContext(pNode->doc);
	if( pXCtx == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pXCtx->node = pNode;
	pReg = PH7_NativeAttr(pThis,SXE_XPNS);
	if( pReg && (pReg->iFlags & MEMOBJ_HASHMAP) ){
		ph7_array_walk(pReg,SxeRegisterOne,pXCtx);
	}
	/* php also puts the CONTEXT NODE's in-scope declarations in front of the
	 * registered table -- libxml consults `namespaces` before the registry -- so
	 * a prefix the DOCUMENT declares resolves with no registerXPathNamespace()
	 * at all, and beats one registered under the same prefix. */
	aNs = xmlGetNsList(pNode->doc,pNode);
	if( aNs ){
		int nNs = 0;
		while( aNs[nNs] ){
			nNs++;
		}
		pXCtx->namespaces = aNs;
		pXCtx->nsNr = nNs;
	}
	SyBlobInit(&sExpr,&pCtx->pVm->sAllocator);
	if( nExpr > 0 ){
		SyBlobAppend(&sExpr,zExpr,(sxu32)nExpr);
	}
	SyBlobNullAppend(&sExpr);
	/* An expression that does not evaluate is LIBXML's diagnostic, not one php
	 * writes: `Invalid expression`, `Undefined namespace prefix`. php prints it
	 * under the method's name, with no source location (an XPath error carries
	 * no line), which is what draining the capture window does here. */
	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);
	pObj = xmlXPathEvalExpression((const xmlChar *)SyBlobData(&sExpr),pXCtx);
	SyBlobRelease(&sExpr);
	if( pObj == 0 ){
		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"SimpleXMLElement::xpath");
		xmlXPathFreeContext(pXCtx);
		if( aNs ){
			xmlFree(aNs);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_LibxmlDropErrors(pCtx->pVm,nMark);
	if( pObj->type != XPATH_NODESET ){
		const char *zKind = pObj->type == XPATH_NUMBER ? "number"
			: pObj->type == XPATH_STRING ? "string"
			: pObj->type == XPATH_BOOLEAN ? "bool" : "unknown";
		xmlXPathFreeObject(pObj);
		xmlXPathFreeContext(pXCtx);
		if( aNs ){
			xmlFree(aNs);
		}
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"XPath expression must return a node set, %s returned",zKind);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pOut = ph7_context_new_array(pCtx);
	if( pOut == 0 ){
		xmlXPathFreeObject(pObj);
		xmlXPathFreeContext(pXCtx);
		if( aNs ){
			xmlFree(aNs);
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( i = 0 ; pObj->nodesetval && i < pObj->nodesetval->nodeNr ; ++i ){
		xmlNodePtr pHit = pObj->nodesetval->nodeTab[i];
		ph7_class_instance *pSub;
		ph7_value *pSlot;
		if( pHit == 0 ){
			continue;
		}
		if( pHit->type == XML_ATTRIBUTE_NODE ){
			/* php answers an attribute as the ATTRIBUTE LIST of its owner
			 * narrowed to that name, which is why its table shows an
			 * `@attributes` row where `$x['a']`'s shows a plain value. The filter
			 * is the ATTRIBUTE'S OWN namespace, not the receiver's. */
			const char *zHref = pHit->ns && pHit->ns->href
				? (const char *)pHit->ns->href : 0;
			phl_domnode *pNd = SxeResOf(pThis);
			pSub = SxeNew(pCtx->pVm,pThis->pClass,pNd ? pNd->pShell : 0,pHit->parent,
				SXE_ITER_ATTRLIST,(const char *)pHit->name,(int)xmlStrlen(pHit->name),
				zHref,zHref ? (int)SyStrlen(zHref) : 0,0);
		}else if( pHit->type == XML_TEXT_NODE || pHit->type == XML_CDATA_SECTION_NODE ){
			/* A text node is answered as the element that HOLDS it. */
			pSub = SxeDerive(pCtx->pVm,pThis,pHit->parent,SXE_ITER_NONE,0,0);
		}else if( pHit->type == XML_DOCUMENT_NODE || pHit->type == XML_HTML_DOCUMENT_NODE
		       || pHit->type == XML_NAMESPACE_DECL || pHit->type == XML_DTD_NODE ){
			/* The two a node set can hold that SimpleXML has no object for: the
			 * document itself (what `/` and a root's `..` select) and a namespace
			 * declaration. php drops both and answers the shorter array. */
			continue;
		}else{
			/* Everything else -- an element, and the comment and processing
			 * instruction php keeps here even though neither is one in its table
			 * of an element's children. */
			pSub = SxeDerive(pCtx->pVm,pThis,pHit,SXE_ITER_NONE,0,0);
		}
		if( pSub == 0 ){
			break;
		}
		/* pSlot CARRIES the constructor's reference (no bump): the array's
		 * insert takes its own, and the context releases pSlot at method end. */
		pSlot = ph7_context_new_scalar(pCtx);
		if( pSlot == 0 ){
			PH7_ClassInstanceUnref(pSub);
			break;
		}
		pSlot->x.pOther = pSub;
		pSlot->iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pOut,0,pSlot);
	}
	xmlXPathFreeObject(pObj);
	xmlXPathFreeContext(pXCtx);
	if( aNs ){
		xmlFree(aNs);   /* the xmlNs entries belong to the tree; only the array is ours */
	}
	ph7_result_value(pCtx,pOut);
	return PH7_OK;
}
/* registerXPathNamespace(): the prefixes THIS object's xpath() knows. They do
 * not travel: a child fetched off this object starts with none, which is php's
 * answer too. */
SXE_METHOD(vm_builtin_SimpleXMLElement_registerXPathNamespace)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_value *pReg;
	int nPfx = 0,nUri = 0;
	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";
	const char *zUri = nArg > 1 ? ph7_value_to_string(apArg[1],&nUri) : "";
	ph7_value sKey,sVal;
	if( pThis == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pReg = PH7_NativeAttr(pThis,SXE_XPNS);
	if( pReg == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( (pReg->iFlags & MEMOBJ_HASHMAP) == 0 && PH7_MemObjToHashmap(pReg) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInitFromString(pCtx->pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);
	PH7_MemObjInitFromString(pCtx->pVm,&sVal,0);
	PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);
	ph7_array_add_elem(pReg,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ===== addChild / addAttribute ===== */

/* Split `prefix:local`, php's qualified-name form. */
static void SxeSplitQName(const char *zName,int nName,const char **pzPfx,int *pnPfx,
	const char **pzLocal,int *pnLocal)
{
	int i;
	*pzPfx = 0;
	*pnPfx = 0;
	*pzLocal = zName;
	*pnLocal = nName;
	for( i = 0 ; i < nName ; ++i ){
		if( zName[i] == ':' ){
			*pzPfx = zName;
			*pnPfx = i;
			*pzLocal = zName + i + 1;
			*pnLocal = nName - i - 1;
			return;
		}
	}
}
/*
 * The namespace a new child or attribute is put in: the one the caller named
 * (declared on the node when the document has no binding for it yet), or the
 * one the qualified name's prefix is already bound to, or none -- in which case
 * libxml gives a child the parent's default binding.
 */
static xmlNsPtr SxeResolveNs(ph7_vm *pVm,xmlNodePtr pNode,const char *zPfx,int nPfx,
	const char *zHref,int nHref,int bHave)
{
	SyBlob sP,sH;
	xmlNsPtr pNs = 0;
	if( bHave ){
		SxeCopyZ(pVm,&sH,zHref,nHref);
		SxeCopyZ(pVm,&sP,zPfx,nPfx);
		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sH));
		if( pNs == 0 ){
			pNs = xmlNewNs(pNode,(const xmlChar *)SyBlobData(&sH),
				nPfx > 0 ? (const xmlChar *)SyBlobData(&sP) : 0);
		}
		SyBlobRelease(&sH);
		SyBlobRelease(&sP);
		return pNs;
	}
	if( zPfx && nPfx > 0 ){
		SxeCopyZ(pVm,&sP,zPfx,nPfx);
		pNs = xmlSearchNs(pNode->doc,pNode,(const xmlChar *)SyBlobData(&sP));
		SyBlobRelease(&sP);
	}
	return pNs;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_addChild)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = SxeThis(pCtx);
	xmlNodePtr pNode = pThis ? SxeMethodNode(pThis) : 0;
	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;
	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";
	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;
	int nPfx,nLocal;
	SyBlob sLocal;
	const char *zL;
	xmlNsPtr pNs;
	xmlNodePtr pNew;
	SyBlob sVal;
	sxu32 nMark;
	if( nQ < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"SimpleXMLElement::addChild(): Argument #1 ($qualifiedName) must not be empty");
	}
	if( pThis && SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot add element to attributes");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,
			"Cannot add child. Parent is not a permanent member of the XML tree");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 && (apArg[1]->iFlags & MEMOBJ_NULL) == 0 ){
		zVal = ph7_value_to_string(apArg[1],&nVal);
	}
	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){
		zHref = ph7_value_to_string(apArg[2],&nHref);
		bHaveNs = 1;
	}
	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);
	zL = SxeCopyZ(pVm,&sLocal,zLocal,nLocal);
	SyBlobInit(&sVal,&pVm->sAllocator);
	if( nVal > 0 ){
		SyBlobAppend(&sVal,zVal,(sxu32)nVal);
	}
	SyBlobNullAppend(&sVal);
	/* php's addChild does NOT escape: libxml PARSES the value for entity
	 * references, so `'a&b'` is the LIBRARY's answer and not php's -- a
	 * diagnostic and an empty child under libxml 2.9, the two letters under
	 * 2.13. Both engines answer whatever the libxml they were built against
	 * does, which is why no test pins it. Routed through the per-VM queue like
	 * every other libxml message here, or it prints itself past
	 * error_reporting(). */
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pNew = xmlNewChild(pNode,0,(const xmlChar *)zL,
		zVal ? (const xmlChar *)SyBlobData(&sVal) : 0);
	PH7_LibxmlCaptureEnd(pVm,nMark,"SimpleXMLElement::addChild");
	SyBlobRelease(&sVal);
	SyBlobRelease(&sLocal);
	/* The namespace is resolved against the NEW node and declared THERE when the
	 * document has no binding for it -- `addChild('q:n','v','urn:q')` prints
	 * `<q:n xmlns:q="urn:q">`, not a declaration hoisted onto the parent. */
	if( pNew ){
		pNs = SxeResolveNs(pVm,pNew,zPfx,nPfx,zHref,nHref,bHaveNs);
		if( pNs ){
			xmlSetNs(pNew,pNs);
		}
	}
	if( pNew == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return SxeResultObj(pCtx,SxeDerive(pVm,pThis,pNew,SXE_ITER_NONE,0,0));
}
SXE_METHOD(vm_builtin_SimpleXMLElement_addAttribute)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	/* An attribute list writes to the element it lists, not to an attribute:
	 * php reaches its own node here rather than the first match. */
	xmlNodePtr pNode = pThis
		? (SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ? SxeNodeOf(pThis) : SxeMethodNode(pThis))
		: 0;
	int nQ = 0,nVal = 0,nHref = 0,bHaveNs = 0;
	const char *zQ = nArg > 0 ? ph7_value_to_string(apArg[0],&nQ) : "";
	const char *zVal = 0,*zHref = 0,*zPfx,*zLocal;
	int nPfx,nLocal;
	SyBlob sLocal;
	const char *zL;
	xmlNsPtr pNs;
	xmlAttrPtr pAttr;
	SyBlob sVal;
	if( nQ < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"SimpleXMLElement::addAttribute(): Argument #1 ($qualifiedName) must not be empty");
	}
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Unable to locate parent Element");
		return PH7_OK;
	}
	if( nArg > 1 ){
		zVal = ph7_value_to_string(apArg[1],&nVal);
	}
	if( nArg > 2 && (apArg[2]->iFlags & MEMOBJ_NULL) == 0 ){
		zHref = ph7_value_to_string(apArg[2],&nHref);
		bHaveNs = 1;
	}
	SxeSplitQName(zQ,nQ,&zPfx,&nPfx,&zLocal,&nLocal);
	zL = SxeCopyZ(pCtx->pVm,&sLocal,zLocal,nLocal);
	pNs = SxeResolveNs(pCtx->pVm,pNode,zPfx,nPfx,zHref,nHref,bHaveNs);
	if( xmlHasNsProp(pNode,(const xmlChar *)zL,pNs ? pNs->href : 0) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Attribute already exists");
		SyBlobRelease(&sLocal);
		return PH7_OK;
	}
	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);
	if( nVal > 0 ){
		SyBlobAppend(&sVal,zVal,(sxu32)nVal);
	}
	SyBlobNullAppend(&sVal);
	/* Unlike addChild, the VALUE is data: php escapes it, so `'a&b'` lands as
	 * three characters and comes back out as `a&amp;b`. */
	pAttr = pNs ? xmlNewNsProp(pNode,pNs,(const xmlChar *)zL,(const xmlChar *)"")
		: xmlNewProp(pNode,(const xmlChar *)zL,(const xmlChar *)"");
	SxeSetText((xmlNodePtr)pAttr,(const char *)SyBlobData(&sVal));
	SyBlobRelease(&sVal);
	SyBlobRelease(&sLocal);
	return PH7_OK;
}

/* ===== The iterator: php's Iterator and RecursiveIterator, as methods ===== */

/* The object the cursor stands on, or NULL before rewind() and past the end. */
static ph7_class_instance * SxeCurrent(ph7_class_instance *pThis)
{
	return PH7_NativeAttrObj(pThis,SXE_CUR);
}
static void SxeSetCurrent(ph7_vm *pVm,ph7_class_instance *pThis,xmlNodePtr pNode)
{
	if( pNode == 0 ){
		ph7_value sNull;
		PH7_MemObjInit(pVm,&sNull);
		PH7_NativeSetProp(pVm,pThis,SXE_CUR,sizeof(SXE_CUR)-1,&sNull);
		PH7_MemObjRelease(&sNull);
		return;
	}
	{
		ph7_class_instance *pObj = SxeDerive(pVm,pThis,pNode,SXE_ITER_NONE,0,0);
		if( pObj ){
			PH7_NativeSetAttrObj(pVm,pThis,SXE_CUR,pObj);
			PH7_ClassInstanceUnref(pObj);
		}
	}
}
SXE_METHOD(vm_builtin_SimpleXMLElement_rewind)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	(void)nArg; (void)apArg;
	if( pThis ){
		SxeSetCurrent(pCtx->pVm,pThis,SxeIterFirst(pThis));
	}
	return PH7_OK;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_valid)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	(void)nArg; (void)apArg;
	ph7_result_bool(pCtx,pThis && SxeCurrent(pThis) != 0);
	return PH7_OK;
}
/* php refuses to answer current()/key() with no cursor -- before rewind() and
 * after the walk ran out -- and the refusal is an Error, not a null. */
static ph7_class_instance * SxeCursorOrThrow(ph7_context *pCtx,ph7_class_instance *pThis,
	sxi32 *pRc)
{
	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;
	*pRc = PH7_OK;
	if( pCur == 0 ){
		*pRc = PH7_VmThrowException(pCtx,"Error",
			"Iterator not initialized or already consumed");
	}
	return pCur;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_current)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	sxi32 rc;
	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);
	(void)nArg; (void)apArg;
	if( pCur == 0 ){
		return rc;
	}
	return SxeResultBorrowed(pCtx,pCur);
}
SXE_METHOD(vm_builtin_SimpleXMLElement_key)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	sxi32 rc;
	ph7_class_instance *pCur = SxeCursorOrThrow(pCtx,pThis,&rc);
	xmlNodePtr pNode;
	(void)nArg; (void)apArg;
	if( pCur == 0 ){
		return rc;
	}
	pNode = SxeNodeOf(pCur);
	if( pNode == 0 || pNode->name == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)pNode->name,(int)xmlStrlen(pNode->name));
	return PH7_OK;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_next)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;
	(void)nArg; (void)apArg;
	if( pThis == 0 ){
		return PH7_OK;
	}
	SxeSetCurrent(pCtx->pVm,pThis,pCur ? SxeIterNext(pThis,SxeNodeOf(pCur)) : 0);
	return PH7_OK;
}
/* RecursiveIterator: does the node the cursor stands on have element children,
 * and the node itself as the thing to descend into. An attribute list never
 * has either. */
SXE_METHOD(vm_builtin_SimpleXMLElement_hasChildren)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;
	xmlNodePtr pWalk;
	(void)nArg; (void)apArg;
	if( pCur == 0 || SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pWalk = SxeNodeOf(pCur);
	for( pWalk = pWalk ? pWalk->children : 0 ; pWalk ; pWalk = pWalk->next ){
		if( pWalk->type == XML_ELEMENT_NODE ){
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
SXE_METHOD(vm_builtin_SimpleXMLElement_getChildren)
{
	ph7_class_instance *pThis = SxeThis(pCtx);
	ph7_class_instance *pCur = pThis ? SxeCurrent(pThis) : 0;
	(void)nArg; (void)apArg;
	if( pCur == 0 || SxeTypeOf(pThis) == SXE_ITER_ATTRLIST ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return SxeResultBorrowed(pCtx,pCur);
}

/* ===== Loading ===== */

/*
 * Parse bytes into a document, with php's error routing: libxml's diagnostics
 * go through the per-VM queue -- which is what `libxml_use_internal_errors()`
 * turns off and what `@` and error_reporting() screen -- rather than onto
 * stderr.  Answers the registered shell, or 0.
 */
static phl_xmldoc * SxeParse(ph7_context *pCtx,const char *zSrc,int nLen,
	const char *zUrl,int iOpts,const char *zFn)
{
	ph7_vm *pVm = pCtx->pVm;
	xmlDocPtr pDoc;
	phl_xmldoc *pShell;
	sxu32 nMark = PH7_LibxmlCaptureBeginRaw(pVm);
	pDoc = xmlReadMemory(zSrc,nLen,zUrl,0,iOpts);
	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);
	if( pDoc == 0 ){
		return 0;
	}
	if( xmlDocGetRootElement(pDoc) == 0 ){
		/* php answers false for a document with no element to stand on. */
		xmlFreeDoc(pDoc);
		return 0;
	}
	pShell = PH7_LibxmlNewDoc(pVm,pDoc);
	if( pShell == 0 ){
		xmlFreeDoc(pDoc);
	}
	return pShell;
}
/* The class the two loaders build, screened the way php screens it: null is
 * SimpleXMLElement, and anything that is not a SUBCLASS of it is a TypeError
 * naming the argument. */
static ph7_class * SxeArgClass(ph7_context *pCtx,ph7_value *pVal,const char *zFn,sxi32 *pRc)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pBase = PH7_VmExtractClass(pVm,"SimpleXMLElement",
		sizeof("SimpleXMLElement")-1,FALSE,0);
	ph7_class *pClass;
	int nName = 0;
	const char *zName;
	*pRc = PH7_OK;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		return pBase;
	}
	zName = ph7_value_to_string(pVal,&nName);
	pClass = PH7_VmExtractClass(pVm,zName,(sxu32)nName,TRUE,0);
	if( pClass == 0 || pBase == 0 || !PH7_VmInstanceOf(pClass,pBase) ){
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #2 ($class_name) must be a class name derived from "
			"SimpleXMLElement or null, %.*s given",zFn,nName,zName);
		return 0;
	}
	return pClass;
}
/* The root object of a freshly parsed document. */
static ph7_class_instance * SxeRootObject(ph7_vm *pVm,ph7_class *pClass,phl_xmldoc *pShell,
	const char *zNs,int nNs,int bPrefix)
{
	xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);
	return SxeNew(pVm,pClass,pShell,pRoot,SXE_ITER_NONE,0,0,zNs,nNs,bPrefix);
}
/*
 * The shared body of simplexml_load_string() / simplexml_load_file() and of
 * SimpleXMLElement::__construct(): the same five arguments, and the only
 * difference is what a failure IS -- false from the functions, and the
 * constructor's `Exception: String could not be parsed as XML`.
 */
static int SxeLoad(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,
	const char *zFn,ph7_class *pClass,ph7_class_instance *pInto)
{
	ph7_vm *pVm = pCtx->pVm;
	int nSrc = 0,nNs = 0,bPrefix = 0,iOpts = 0;
	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";
	const char *zNs = 0;
	phl_xmldoc *pShell;
	SyBlob sBody,sPath;
	int iNsArg = pInto ? 3 : 3;
	if( nArg > (pInto ? 1 : 2) ){
		iOpts = ph7_value_to_int(apArg[pInto ? 1 : 2]);
	}
	if( nArg > iNsArg ){
		int n = 0;
		const char *z = ph7_value_to_string(apArg[iNsArg],&n);
		if( n > 0 ){
			zNs = z;
			nNs = n;
		}
	}
	if( nArg > iNsArg + 1 ){
		bPrefix = ph7_value_to_bool(apArg[iNsArg + 1]);
	}
	if( bFile ){
		if( !PH7_DomReadFile(pCtx,zSrc,nSrc,zFn,&sBody,&sPath) ){
			return 0;
		}
		pShell = SxeParse(pCtx,(const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),
			(const char *)SyBlobData(&sPath),iOpts,zFn);
		SyBlobRelease(&sBody);
		SyBlobRelease(&sPath);
	}else{
		pShell = SxeParse(pCtx,zSrc,nSrc,0,iOpts,zFn);
	}
	if( pShell == 0 ){
		return 0;
	}
	if( pInto ){
		/* The constructor REPOINTS the object it was called on rather than
		 * making a second one, so a subclass's own constructor may have run
		 * first and its state survives. */
		xmlNodePtr pRoot = xmlDocGetRootElement((xmlDocPtr)pShell->pDoc);
		phl_domnode *pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,
			sizeof(phl_domnode));
		ph7_value sVal;
		if( pRes == 0 ){
			return 0;
		}
		pRes->pShell = pShell;
		pRes->pNode = pRoot;
		PH7_MemObjInit(pVm,&sVal);
		sVal.x.pOther = pRes;
		sVal.iFlags = MEMOBJ_RES;
		PH7_NativeSetProp(pVm,pInto,SXE_RES,sizeof(SXE_RES)-1,&sVal);
		PH7_NativeSetAttrInt(pVm,pInto,SXE_IT,SXE_ITER_NONE);
		if( zNs ){
			PH7_NativeSetAttrStr(pVm,pInto,SXE_NS,zNs,(sxu32)nNs);
			PH7_NativeSetAttrInt(pVm,pInto,SXE_ISP,bPrefix ? 1 : 0);
		}
		return 1;
	}
	{
		ph7_class_instance *pObj = SxeRootObject(pVm,pClass,pShell,zNs,nNs,bPrefix);
		if( pObj == 0 ){
			return 0;
		}
		SxeResultObj(pCtx,pObj);
		return 1;
	}
}
SXE_METHOD(vm_builtin_SimpleXMLElement_construct)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int bUrl = nArg > 2 && ph7_value_to_bool(apArg[2]);
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( SxeLoad(pCtx,nArg,apArg,bUrl,"SimpleXMLElement::__construct",0,pThis) ){
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"Exception","String could not be parsed as XML");
}
static int SxeLoadFunc(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,const char *zFn)
{
	sxi32 rc;
	ph7_class *pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,zFn,&rc);
	if( pClass == 0 ){
		return rc;
	}
	if( !SxeLoad(pCtx,nArg,apArg,bFile,zFn,pClass,0) ){
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
static int vm_builtin_simplexml_load_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SxeLoadFunc(pCtx,nArg,apArg,0,"simplexml_load_string");
}
static int vm_builtin_simplexml_load_file(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return SxeLoadFunc(pCtx,nArg,apArg,1,"simplexml_load_file");
}

/* ===== The two doors between ext/simplexml and ext/dom ===== */

/*
 * simplexml_import_dom(object $node, ?string $class_name = 'SimpleXMLElement')
 *
 * The two extensions share one libxml tree, so an import is a second VIEW of it
 * and not a copy: a write through either shows in the other. php takes a
 * document (standing on its root element), an element and an attribute, and
 * warns for anything else.  It caches nothing -- two imports of one document
 * are two objects, unlike dom_import_simplexml's one.
 */
static int vm_builtin_simplexml_import_dom(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pArg;
	ph7_value *pRes;
	phl_domnode *pNd;
	xmlNodePtr pNode;
	ph7_class *pClass;
	sxi32 rc;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");
	}
	pArg = (ph7_class_instance *)apArg[0]->x.pOther;
	pRes = pArg ? PH7_NativeAttr(pArg,"__res") : 0;
	if( pRes == 0 || (pRes->iFlags & MEMOBJ_RES) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"simplexml_import_dom(): Argument #1 ($node) must be a valid XML node");
	}
	pClass = SxeArgClass(pCtx,nArg > 1 ? apArg[1] : 0,"simplexml_import_dom",&rc);
	if( pClass == 0 ){
		return rc;
	}
	pNd = (phl_domnode *)pRes->x.pOther;
	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pNode && (pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE) ){
		pNode = xmlDocGetRootElement((xmlDocPtr)pNode);
	}
	if( pNode == 0
	 || (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Invalid Nodetype to import");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return SxeResultObj(pCtx,SxeNew(pVm,pClass,pNd->pShell,pNode,SXE_ITER_NONE,0,0,0,0,0));
}
/*
 * dom_import_simplexml(object $node): DOMAttr|DOMElement -- ext/dom's half of
 * the same door, declared by that extension and bodied here because it is a
 * SimpleXML object it takes apart.
 *
 * Unlike its opposite this one has an IDENTITY: two imports of the same node
 * are the same DOM object, because ext/dom caches its wrappers per document.
 */
static int vm_builtin_dom_import_simplexml(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class_instance *pArg;
	phl_domnode *pNd;
	xmlNodePtr pNode;
	ph7_class_instance *pObj;
	ph7_value sRes;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"dom_import_simplexml(): Argument #1 ($node) is not a valid node type");
	}
	pArg = (ph7_class_instance *)apArg[0]->x.pOther;
	pNd = pArg ? SxeResOf(pArg) : 0;
	pNode = pArg ? SxeMethodNode(pArg) : 0;
	if( pNd == 0 || pNode == 0
	 || (pNode->type != XML_ELEMENT_NODE && pNode->type != XML_ATTRIBUTE_NODE) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"dom_import_simplexml(): Argument #1 ($node) is not a valid node type");
	}
	pObj = PH7_DomWrapForeign(pCtx->pVm,pNd->pShell,pNode);
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sRes);
	sRes.x.pOther = pObj;
	sRes.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx,&sRes);   /* the cache owns pObj; borrowed */
	return PH7_OK;
}

/* ===== Install ===== */

/*
 * `clone $x` on a SimpleXML object copies the DOCUMENT, exactly as ext/dom's
 * document clone does: php's SimpleXML clone_obj duplicates the tree so a write
 * through the copy does not reach the original.
 */
static void SxeClone(ph7_vm *pVm,ph7_class_instance *pCopy,ph7_class_instance *pSrc)
{
	phl_domnode *pNd = SxeResOf(pSrc);
	xmlDocPtr pDoc = pNd && pNd->pShell ? (xmlDocPtr)pNd->pShell->pDoc : 0;
	xmlDocPtr pNew = pDoc ? xmlCopyDoc(pDoc,1) : 0;
	phl_xmldoc *pShell = pNew ? PH7_LibxmlNewDoc(pVm,pNew) : 0;
	phl_domnode *pRes;
	ph7_value sVal;
	if( pShell == 0 ){
		if( pNew ){
			xmlFreeDoc(pNew);
		}
		return;
	}
	pRes = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));
	if( pRes == 0 ){
		return;
	}
	pRes->pShell = pShell;
	/* Only the ROOT survives a document copy identifiably; php's clone of a
	 * deeper object stands on the copy's root too. */
	pRes->pNode = xmlDocGetRootElement(pNew);
	PH7_MemObjInit(pVm,&sVal);
	sVal.x.pOther = pRes;
	sVal.iFlags = MEMOBJ_RES;
	PH7_NativeSetProp(pVm,pCopy,SXE_RES,sizeof(SXE_RES)-1,&sVal);
}
/*
 * php's compare handler for the class -- and it is NODE IDENTITY, not the
 * property table the rest of its surface shows.
 *
 * So `$x->kid == $x->kid` is true (both stand on the same element and ask the
 * same question of it) while two documents parsed from the SAME BYTES are never
 * equal, and neither are two objects over different nodes however alike their
 * tables. An object with no node equals another with no node and nothing else.
 */
static void SxeCmp(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeCmpCtx *pCtx)
{
	ph7_class_instance *pOther;
	xmlNodePtr pA,pB;
	(void)pVm;
	pOther = pCtx->pOther;
	if( pOther == 0 || PH7_NativeAttr(pOther,SXE_RES) == 0 ){
		return;
	}
	pA = SxeNodeOf(pThis);
	pB = SxeNodeOf(pOther);
	pCtx->bAnswered = 1;
	pCtx->iResult = (pA == 0 && pB == 0) ? 0 : (pA == pB ? 0 : 1);
}
PH7_PRIVATE sxi32 PH7_VmInstallSimpleXml(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "simplexml_load_file",   vm_builtin_simplexml_load_file   },
		{ "simplexml_load_string", vm_builtin_simplexml_load_string },
		{ "simplexml_import_dom",  vm_builtin_simplexml_import_dom  },
		/* ext/dom's own name for the other direction. */
		{ "dom_import_simplexml",  vm_builtin_dom_import_simplexml  },
	};
	/*
	 * The engine slots. php's SimpleXMLElement declares NO property -- Reflection
	 * lists none and `property_exists()` is false for every name -- so each of
	 * these is PH7_MOD_HIDDEN and the whole object shows the document instead.
	 */
	static const PH7_NativePropDef aProp[] = {
		{ SXE_RES,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SXE_IT,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },
		{ SXE_NM,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SXE_NS,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SXE_ISP,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },
		{ SXE_CUR,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ SXE_XPNS, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/*
	 * php's own list, in php's own order -- which is what Reflection reports and
	 * what `get_class_methods()` answers. Every return type is TENTATIVE (`@`):
	 * php's stubs mark the whole class that way, so a subclass may still declare
	 * `children()` returning something else without a fatal.
	 */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "xpath", PH7_MOD_PUBLIC, "string $expression", "@array|false|null",
		  vm_builtin_SimpleXMLElement_xpath },
		{ "registerXPathNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace",
		  "@bool", vm_builtin_SimpleXMLElement_registerXPathNamespace },
		{ "asXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string|bool",
		  vm_builtin_SimpleXMLElement_asXML },
		{ "saveXML", PH7_MOD_PUBLIC, "?string $filename = null", "@string|bool",
		  vm_builtin_SimpleXMLElement_asXML },
		{ "getNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false", "@array",
		  vm_builtin_SimpleXMLElement_getNamespaces },
		{ "getDocNamespaces", PH7_MOD_PUBLIC, "bool $recursive = false, bool $fromRoot = true",
		  "@array|false", vm_builtin_SimpleXMLElement_getDocNamespaces },
		{ "children", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",
		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_children },
		{ "attributes", PH7_MOD_PUBLIC, "?string $namespaceOrPrefix = null, bool $isPrefix = false",
		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_attributes },
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $data, int $options = 0, bool $dataIsURL = false, "
		  "string $namespaceOrPrefix = '', bool $isPrefix = false", "",
		  vm_builtin_SimpleXMLElement_construct },
		{ "addChild", PH7_MOD_PUBLIC,
		  "string $qualifiedName, ?string $value = null, ?string $namespace = null",
		  "@?SimpleXMLElement", vm_builtin_SimpleXMLElement_addChild },
		{ "addAttribute", PH7_MOD_PUBLIC,
		  "string $qualifiedName, string $value, ?string $namespace = null", "@void",
		  vm_builtin_SimpleXMLElement_addAttribute },
		{ "getName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_getName },
		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_SimpleXMLElement_toString },
		{ "__debugInfo", PH7_MOD_PUBLIC, "", "?array", vm_builtin_SimpleXMLElement_debugInfo },
		{ "count", PH7_MOD_PUBLIC, "", "@int", vm_builtin_SimpleXMLElement_count },
		{ "rewind", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_rewind },
		{ "valid", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_SimpleXMLElement_valid },
		{ "current", PH7_MOD_PUBLIC, "", "@SimpleXMLElement",
		  vm_builtin_SimpleXMLElement_current },
		{ "key", PH7_MOD_PUBLIC, "", "@string", vm_builtin_SimpleXMLElement_key },
		{ "next", PH7_MOD_PUBLIC, "", "@void", vm_builtin_SimpleXMLElement_next },
		{ "hasChildren", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_SimpleXMLElement_hasChildren },
		{ "getChildren", PH7_MOD_PUBLIC, "", "@?SimpleXMLElement",
		  vm_builtin_SimpleXMLElement_getChildren },
	};
	/*
	 * php's class list. SimpleXMLElement declares Stringable, Countable and
	 * RecursiveIterator -- and NOT ArrayAccess, which is why `$x['a']` is a
	 * dimension handler and `$x instanceof ArrayAccess` is false; and not
	 * JsonSerializable, because json_encode() reads its property table like any
	 * other object's.
	 *
	 * PH7_CLASS_NUM_AS_STRING is php's cast_object answering IS_LONG/IS_DOUBLE
	 * from the node's text, and PH7_CLASS_VARS_PRESENT is its get_properties
	 * answering the get_object_vars purpose too -- the two places SimpleXML does
	 * not behave like every other native class here.
	 *
	 * SimpleXMLIterator adds nothing: php declares it as an empty subclass, kept
	 * because RecursiveIteratorIterator over one is how the class is used.
	 */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "SimpleXMLElement", 0, "Stringable,Countable,RecursiveIterator",
		  PH7_CLASS_NOSERIALIZE|PH7_CLASS_NUM_AS_STRING|PH7_CLASS_VARS_PRESENT,
		  aMethod, SX_ARRAYSIZE(aMethod), 0, 0, aProp, SX_ARRAYSIZE(aProp),
		  0, 0, SxePresent },
		{ "SimpleXMLIterator", "SimpleXMLElement", 0,
		  PH7_CLASS_NOSERIALIZE|PH7_CLASS_NUM_AS_STRING|PH7_CLASS_VARS_PRESENT,
		  0, 0, 0, 0, 0, 0, 0, 0, SxePresent },
	};
	sxi32 rc;
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aFunc) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		/* The four handlers php gives the class, assigned on the mounted class
		 * because PH7_NativeClassSpec carries no field for any of them. Stated on
		 * the ROOT only: the engine walks the base chain, which is php's own
		 * handler inheritance, so SimpleXMLIterator and a userland subclass reach
		 * these. */
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"SimpleXMLElement",
			sizeof("SimpleXMLElement")-1,FALSE,0);
		if( pClass ){
			pClass->xDim = SxeDimHook;
			pClass->xClone = SxeClone;
			pClass->xBool = SxeBool;
			pClass->xCmp = SxeCmp;
		}
		PH7_NativeClassInstallPropHook(&(*pVm),"SimpleXMLElement",SxePropHook);
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_simplexml_unused;
#endif /* PH7_ENABLE_LIBXML */
