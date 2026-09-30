/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <locale.h>
#include <stdlib.h>   /* getenv */
#ifdef __WINNT__
#include <Windows.h>  /* GetEnvironmentVariableA -- see GtGetenv() below */
#endif
/*
 * Section:
 *    php's gettext extension: the message catalogs a translated program reads.
 * Status:
 *    Stable.
 *
 * php's ext/gettext is a shell over GNU libintl, so its contract is really
 * libintl's -- and that is what is reproduced here, over PHL's own catalog
 * reader rather than the platform's, so a Windows build answers what a Linux
 * one does. php's own layer is thin and contributes only the argument screens
 * (§ "The php layer" below); everything else in this file is the library's
 * behaviour, measured against glibc 2.4x through php 8.5.9.
 *
 * WHICH FILE a lookup reads is the whole of it, and it is four rules:
 *
 *   1. The CATEGORY VALUE. gettext asks the C library what locale the category
 *      is set to (`setlocale(LC_MESSAGES, NULL)`), NOT what the environment
 *      says -- so a program that never calls setlocale() is in the "C" locale
 *      and every lookup answers the msgid untranslated, whatever LANG holds.
 *      When LANGUAGE is set AND the category is not the C locale, LANGUAGE
 *      wins: it is a colon-separated PRIORITY LIST, each element tried whole
 *      before the next, and an element that IS the C locale ENDS the search
 *      untranslated. What counts as the C locale is wider than its name and is
 *      spelled out at GtIsPosixLocale() below.
 *   2. The NAME EXPANSION. One locale name is `language[_territory][.codeset]
 *      [@modifier]`, and each of the three optional parts is dropped in turn to
 *      make a candidate list, most specific first, with the modifier the most
 *      significant part and the codeset the least: `pt_BR.UTF-8` tries
 *      pt_BR.UTF-8, pt_BR.utf8, pt_BR, pt.UTF-8, pt.utf8, pt. The `.utf8` forms
 *      are the NORMALISED codeset (lower-cased, non-alphanumerics dropped, and
 *      prefixed `iso` when nothing but digits is left), and they only appear
 *      when normalising changed the spelling.
 *   3. The PATH is `<bound directory>/<candidate>/<CATEGORY>/<domain>.mo`. The
 *      bound directory is what bindtextdomain() was given; a domain nobody bound
 *      reads `/usr/share/locale`, which is the directory glibc is built with.
 *      What wins is NOT the first file that opens but the first that CARRIES
 *      the msgid: every candidate is a link in a chain (glibc's
 *      `domain->successor[]`, walked in DCIGETTEXT), so a `pt_BR` catalog
 *      overrides a `pt` one and falls through to it for everything it does not
 *      say -- taking that catalog's charset and plural rule with it.
 *   4. The CATEGORY DIRECTORY is the category's own name -- `LC_MESSAGES` for
 *      gettext()/dgettext(), and whatever dcgettext() was handed for the rest.
 *      LC_ALL is refused by php before it reaches here.
 *
 * The FILE is the GNU MO format: a magic word that also gives the byte order, a
 * count, and two tables of {length, offset} pairs, one for the msgids and one
 * for the translations. The msgid table is sorted for BINARY SEARCH, and the
 * comparison is a C-string one -- which is what makes a plural entry work at
 * all: it is stored as `singular\0plural` and compares equal to its own
 * singular, so ngettext() looks up the singular and finds the pair.
 *
 * Entry 0 is the HEADER, whose msgid is empty and whose translation is the
 * RFC-822-shaped block a `.po` file opens with. Two of its fields are contract:
 * `Content-Type: text/plain; charset=X` names the encoding the file's bytes are
 * in, and `Plural-Forms: nplurals=N; plural=EXPR;` is the C expression that
 * turns a count into a form index. EXPR is evaluated per call by the little
 * recursive-descent parser at the bottom of this file -- glibc's plural.y
 * grammar, in unsigned long arithmetic, because that is what the rule
 * `plural=(n > 1)` means for a NEGATIVE count: php hands the count down as a
 * zend_long, libintl takes an `unsigned long`, and -1 is therefore huge and
 * PLURAL. A catalog with no Plural-Forms is `nplurals=2; plural=(n != 1)`,
 * which is also what a MISSING msgid answers with -- an untranslated
 * ngettext() is the Germanic rule whatever the catalog says.
 *
 * The RESULT ENCODING is the last rule. glibc converts every answer from the
 * charset the header names to an output charset, with transliteration on: the
 * one bind_textdomain_codeset() set for the domain, or -- when nothing set one
 * -- the code set of the LC_CTYPE locale. Two cuts here, both §10's:
 *
 *   - PHL models UTF-8, ISO-8859-1 and US-ASCII (the iconv/mbstring scope cut).
 *     A catalog or a bound codeset outside those three cannot be converted, and
 *     php's own answer for a conversion it cannot open is the msgid
 *     UNTRANSLATED, which is what this answers too.
 *   - a header with no charset is not converted at all, which is glibc's rule
 *     for the same case.
 *
 * Two RECORDED divergences, PLAN.md §7.4, both twin-less in the corpus because a
 * php half of either would kill the runner or measure the cache. The first is
 * arithmetic: glibc's `plural_eval` RAISES SIGFPE for a division or a modulo by
 * zero in a plural rule, deliberately (`if (rightarg == 0) raise (SIGFPE);`), so
 * php does not answer a catalog whose header says `plural=(n%0)` -- it dies with
 * a core dump on nothing worse than a typo in a translation file. This answers 0
 * for that division, and the form index lands on 0.
 *
 * The second is memory: glibc remembers a translation it has
 * already found, keyed by domain + msgid + category, and throws that memory away
 * only when its `_nl_msg_cat_cntr` moves -- which setlocale(), textdomain() and a
 * CHANGED binding do and putenv() does not. So under php a bare
 * `putenv("LANGUAGE=…")` between two lookups of ONE msgid is answered out of the
 * cache, while a msgid never asked for before sees the new list. PHL re-reads
 * whenever the category value changes, so both see it. That cache is a
 * performance artifact of one implementation with an undocumented invalidation
 * set; what it hides is the rule this file is written to, and every program that
 * changes LANGUAGE calls setlocale() beside it (isocodes, the library that put
 * this extension on the roadmap, does exactly that), so both engines agree there.
 *
 * The php layer on top is five screens and one return rule:
 *   - an EMPTY domain is a ValueError, at every door that takes one;
 *   - a domain over 1024 bytes and a message over 4096 are "too long";
 *   - bindtextdomain() reads BOTH its arguments as PATHS, so a NUL byte in
 *     either is the catchable "must not contain any null bytes";
 *   - dcgettext()/dcngettext() refuse LC_ALL by name, and pass any other
 *     integer through (a category the library does not know simply finds no
 *     directory of that name);
 *   - bindtextdomain() makes its directory ABSOLUTE with realpath() and answers
 *     FALSE when that fails, and reads an empty directory (or the string "0")
 *     as the working directory.
 * The return rule is that a MISS gives back the caller's own bytes rather than
 * the C string libintl handed back, so `gettext("a\0b")` keeps its NUL.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC

/* php's own LC_* numbering (constant.c), which is not the C library's. */
#define GT_LC_CTYPE     0
#define GT_LC_NUMERIC   1
#define GT_LC_TIME      2
#define GT_LC_COLLATE   3
#define GT_LC_MONETARY  4
#define GT_LC_MESSAGES  5
#define GT_LC_ALL       6

/* php's caps, checked before anything looks at the value. */
#define GT_MAX_DOMAIN  1024
#define GT_MAX_MSGID   4096

/* The directory glibc is built with, and therefore what a domain nobody bound
 * reads. php answers it verbatim from bindtextdomain($domain, null). */
#define GT_DEFAULT_DIR "/usr/share/locale"

/*
 * How many candidate catalogs one resolution keeps. glibc builds the same chain
 * and walks it whole; the cap only bounds what a pathological LANGUAGE list can
 * make this open, and a real one names one or two files.
 */
#define GT_MAX_CATS 64

/* How much of a locale name (and of a LANGUAGE list) is read. glibc's own
 * buffers are of this order; a name that does not fit is answered as absent,
 * which leaves the lookup on the category's locale rather than half a list. */
#define GT_LOCALE_MAX 512

/* One loaded catalog: the file's bytes plus what its header said. */
typedef struct gt_cat gt_cat;
struct gt_cat {
	gt_cat *pNext;       /* the NEXT candidate, less specific than this one */
	SyBlob sData;        /* the whole .mo file */
	int bLoaded;         /* a well-formed catalog is present */
	int bSwap;           /* the file's byte order is not this machine's */
	sxu32 nStr;          /* how many strings it holds */
	sxu32 nOrig;         /* byte offset of the msgid table */
	sxu32 nTrans;        /* byte offset of the translation table */
	sxu32 nPlurals;      /* nplurals from the header (2 when it says nothing) */
	SyBlob sPlural;      /* the plural EXPRESSION text ("" -> `n != 1`) */
	SyBlob sCharset;     /* the header's charset ("" -> convert nothing) */
};
/* One domain's binding, and the catalog last resolved for it. */
typedef struct gt_dom gt_dom;
struct gt_dom {
	gt_dom *pNext;
	SyBlob sName;        /* the domain name, as bound */
	SyBlob sDir;         /* bindtextdomain()'s directory ("" -> the default) */
	SyBlob sCodeset;     /* bind_textdomain_codeset() ("" -> none bound) */
	SyBlob sKey;         /* the search key pCats was resolved under: the category
	                      * value and the category name joined, so a setlocale()
	                      * or a putenv("LANGUAGE=…") between two calls re-reads
	                      * the files instead of answering the old ones */
	int bResolved;       /* sKey is meaningful */
	/* EVERY candidate that exists, in the order the expansion names them --
	 * not just the first. A lookup walks the whole chain and takes the first
	 * catalog that CARRIES the msgid, which is how a region catalog overrides a
	 * language one and falls through to it for everything it does not say
	 * (glibc's `domain->successor[]` walk in DCIGETTEXT). The charset and the
	 * plural rule then come from the catalog the msgid was found in. */
	gt_cat *pCats;
};
/* The whole extension's per-VM state. */
typedef struct gt_state gt_state;
struct gt_state {
	gt_dom *pList;
	SyBlob sDomain;      /* textdomain() ("" -> "messages") */
};

/* --- State ------------------------------------------------------------- */

static gt_state * GtState(ph7_vm *pVm)
{
	gt_state *pState = (gt_state *)pVm->pGettext;
	if( pState == 0 ){
		pState = (gt_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_state));
		if( pState == 0 ){
			return 0;
		}
		pState->pList = 0;
		SyBlobInit(&pState->sDomain,&pVm->sAllocator);
		pVm->pGettext = pState;
	}
	return pState;
}
/*
 * The domain record for this name, created empty if this is the first mention.
 * php has no way to FORGET a binding, so the list only ever grows, and a
 * program binds a handful of domains.
 */
static gt_dom * GtDomain(ph7_vm *pVm,const char *zName,int nName)
{
	gt_state *pState = GtState(pVm);
	gt_dom *pDom;
	if( pState == 0 ){
		return 0;
	}
	for( pDom = pState->pList ; pDom ; pDom = pDom->pNext ){
		if( (int)SyBlobLength(&pDom->sName) == nName
		 && (nName == 0 || SyMemcmp(SyBlobData(&pDom->sName),zName,(sxu32)nName) == 0) ){
			return pDom;
		}
	}
	pDom = (gt_dom *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_dom));
	if( pDom == 0 ){
		return 0;
	}
	SyZero(pDom,sizeof(gt_dom));
	SyBlobInit(&pDom->sName,&pVm->sAllocator);
	SyBlobInit(&pDom->sDir,&pVm->sAllocator);
	SyBlobInit(&pDom->sCodeset,&pVm->sAllocator);
	SyBlobInit(&pDom->sKey,&pVm->sAllocator);
	SyBlobAppend(&pDom->sName,zName,(sxu32)nName);
	pDom->pNext = pState->pList;
	pState->pList = pDom;
	return pDom;
}

/* --- The MO file ------------------------------------------------------- */

#define GT_MO_MAGIC     0x950412deu
#define GT_MO_MAGIC_SWP 0xde120495u

static sxu32 GtWord(const unsigned char *z,int bSwap)
{
	sxu32 v = (sxu32)z[0] | ((sxu32)z[1] << 8) | ((sxu32)z[2] << 16) | ((sxu32)z[3] << 24);
	if( bSwap ){
		v = ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8)
		  | ((v & 0x00ff0000u) >> 8)  | ((v & 0xff000000u) >> 24);
	}
	return v;
}
/*
 * One {length, offset} pair out of a table, screened against the file's size.
 * A malformed catalog is answered as a MISSING one rather than trusted: the
 * bytes come from a file the script named, and every offset in them is a read
 * into this buffer.
 */
static int GtEntry(gt_cat *pCat,sxu32 nTable,sxu32 i,const char **pzStr,sxu32 *pnStr)
{
	const unsigned char *zBase = (const unsigned char *)SyBlobData(&pCat->sData);
	sxu32 nData = SyBlobLength(&pCat->sData);
	sxu32 nOfft = nTable + i * 8, nLen, nStr;
	if( nOfft + 8 > nData || nOfft < nTable ){
		return -1;
	}
	nLen = GtWord(&zBase[nOfft],pCat->bSwap);
	nStr = GtWord(&zBase[nOfft + 4],pCat->bSwap);
	/* The format guarantees a NUL past every string, and the reader below leans
	 * on it, so the byte at nStr+nLen has to be inside the file and zero. */
	if( nStr >= nData || nLen >= nData - nStr || zBase[nStr + nLen] != 0 ){
		return -1;
	}
	*pzStr = (const char *)&zBase[nStr];
	*pnStr = nLen;
	return PH7_OK;
}
/*
 * Find zNeedle in the header. glibc reaches for both of the fields it reads with
 * a plain `strstr` over the WHOLE null entry -- not per line and not
 * case-insensitively -- so a `charset=` anywhere in it is the one that answers.
 * Returns the offset just past the needle, or -1.
 */
static int GtHeaderFind(const char *zHdr,sxu32 nHdr,const char *zNeedle)
{
	sxu32 n = (sxu32)SyStrlen(zNeedle), i;
	if( n < 1 || n > nHdr ){
		return -1;
	}
	for( i = 0 ; i + n <= nHdr ; ++i ){
		if( SyMemcmp(&zHdr[i],zNeedle,n) == 0 ){
			return (int)(i + n);
		}
	}
	return -1;
}
/*
 * `Content-Type: text/plain; charset=UTF-8` -> `UTF-8`: glibc takes what follows
 * `charset=` up to the first space, tab or newline (`strcspn (charsetstr,
 * " \t\n")`), and a header with no such parameter names no charset at all.
 */
static void GtParseCharset(const char *zHdr,sxu32 nHdr,SyBlob *pOut)
{
	int i = GtHeaderFind(zHdr,nHdr,"charset=");
	sxu32 k;
	if( i < 0 ){
		return;
	}
	k = (sxu32)i;
	while( k < nHdr && zHdr[k] != ' ' && zHdr[k] != '\t' && zHdr[k] != '\n' ){
		k++;
	}
	SyBlobAppend(pOut,&zHdr[i],k - (sxu32)i);
}
/*
 * `Plural-Forms: nplurals=2; plural=(n != 1);` -> the count and the expression.
 * glibc wants BOTH fields and falls back to the Germanic rule when either is
 * missing, skips whitespace after `nplurals=` and then requires a digit. The
 * expression text is kept and parsed per call, which is the same answer for a
 * few dozen bytes of arithmetic; its own grammar stops at the `;`, and the cut
 * below only bounds what is stored.
 */
static void GtParsePlural(const char *zHdr,sxu32 nHdr,gt_cat *pCat)
{
	int iN = GtHeaderFind(zHdr,nHdr,"nplurals=");
	int iP = GtHeaderFind(zHdr,nHdr,"plural=");
	sxu32 j,k;
	sxi64 iVal = 0;
	pCat->nPlurals = 2;
	if( iN < 0 || iP < 0 ){
		return;
	}
	j = (sxu32)iN;
	while( j < nHdr && (zHdr[j] == ' ' || zHdr[j] == '\t') ){
		j++;
	}
	if( j >= nHdr || zHdr[j] < '0' || zHdr[j] > '9' ){
		return;
	}
	while( j < nHdr && zHdr[j] >= '0' && zHdr[j] <= '9' ){
		if( iVal < 1000000 ){
			iVal = iVal * 10 + (zHdr[j] - '0');
		}
		j++;
	}
	if( iVal < 1 ){
		return;   /* nplurals=0 leaves no form to pick */
	}
	pCat->nPlurals = (sxu32)iVal;
	k = (sxu32)iP;
	while( k < nHdr && zHdr[k] != ';' && zHdr[k] != '\n' ){
		k++;
	}
	SyBlobAppend(&pCat->sPlural,&zHdr[iP],k - (sxu32)iP);
}
/* Take apart a catalog whose bytes have just been read. */
static int GtParseMo(gt_cat *pCat)
{
	const unsigned char *z = (const unsigned char *)SyBlobData(&pCat->sData);
	sxu32 nData = SyBlobLength(&pCat->sData), nMagic, nRev;
	const char *zHdr;
	sxu32 nHdr;
	if( nData < 28 ){
		return -1;
	}
	nMagic = GtWord(z,0);
	if( nMagic == GT_MO_MAGIC ){
		pCat->bSwap = 0;
	}else if( nMagic == GT_MO_MAGIC_SWP ){
		pCat->bSwap = 1;
	}else{
		return -1;
	}
	/* Only major revision 0 is read here. Revision 1 adds the "system dependent
	 * segments" nothing in the wild uses, and glibc refuses an unknown major the
	 * same way -- as a catalog that is not there. */
	nRev = GtWord(&z[4],pCat->bSwap);
	if( (nRev >> 16) != 0 ){
		return -1;
	}
	pCat->nStr   = GtWord(&z[8],pCat->bSwap);
	pCat->nOrig  = GtWord(&z[12],pCat->bSwap);
	pCat->nTrans = GtWord(&z[16],pCat->bSwap);
	if( pCat->nStr == 0 || pCat->nStr > nData / 8
	 || pCat->nOrig > nData || pCat->nTrans > nData
	 || pCat->nOrig + pCat->nStr * 8 > nData
	 || pCat->nTrans + pCat->nStr * 8 > nData ){
		return -1;
	}
	pCat->bLoaded = 1;
	/* Entry 0 is the header when its msgid is empty. */
	pCat->nPlurals = 2;
	if( GtEntry(pCat,pCat->nOrig,0,&zHdr,&nHdr) == PH7_OK && nHdr == 0
	 && GtEntry(pCat,pCat->nTrans,0,&zHdr,&nHdr) == PH7_OK ){
		GtParseCharset(zHdr,nHdr,&pCat->sCharset);
		GtParsePlural(zHdr,nHdr,pCat);
	}
	return PH7_OK;
}
/*
 * Binary search of the msgid table. The comparison is a C-string one over
 * bytes the format guarantees NUL-terminated, which is what lets a plural
 * entry (`singular\0plural`) be found by its singular alone.
 */
static int GtStrcmp(const char *zLeft,const char *zRight)
{
	const unsigned char *a = (const unsigned char *)zLeft;
	const unsigned char *b = (const unsigned char *)zRight;
	while( *a && *a == *b ){
		a++; b++;
	}
	return (int)*a - (int)*b;
}
static int GtFindMsg(gt_cat *pCat,const char *zMsg,const char **pzOut,sxu32 *pnOut)
{
	sxu32 iLow = 0, iHigh;
	if( !pCat->bLoaded || pCat->nStr == 0 ){
		return -1;
	}
	iHigh = pCat->nStr - 1;
	for(;;){
		sxu32 iMid = iLow + (iHigh - iLow) / 2;
		const char *zCand;
		sxu32 nCand;
		int c;
		if( GtEntry(pCat,pCat->nOrig,iMid,&zCand,&nCand) != PH7_OK ){
			return -1;
		}
		c = GtStrcmp(zMsg,zCand);
		if( c == 0 ){
			return GtEntry(pCat,pCat->nTrans,iMid,pzOut,pnOut);
		}
		if( c < 0 ){
			if( iMid == iLow ){
				return -1;
			}
			iHigh = iMid - 1;
		}else{
			if( iMid == iHigh ){
				return -1;
			}
			iLow = iMid + 1;
		}
	}
}

/* --- The plural expression --------------------------------------------- */

/*
 * glibc's plural.y, as a recursive-descent parser over `unsigned long`. The
 * grammar is C's conditional expression with `n` as its only variable, and the
 * arithmetic has to be UNSIGNED because that is the type libintl evaluates it
 * in -- `plural=(n > 1)` with a count of -1 is the PLURAL form, not the
 * singular. Division and modulo by zero answer 0 rather than trapping -- glibc
 * raises SIGFPE there on purpose, which is the §7.4 record above: a catalog is
 * DATA, and a typo in one must not take the process down.
 */
typedef struct gt_plural gt_plural;
struct gt_plural {
	const char *z;
	const char *zEnd;
	sxu64 n;
	int bBad;
};
static sxu64 GtPlCond(gt_plural *p);
static void GtPlSpace(gt_plural *p)
{
	while( p->z < p->zEnd && (p->z[0] == ' ' || p->z[0] == '\t'
	    || p->z[0] == '\r' || p->z[0] == '\n') ){
		p->z++;
	}
}
static int GtPlEat(gt_plural *p,const char *zTok)
{
	sxu32 n = (sxu32)SyStrlen(zTok);
	GtPlSpace(p);
	if( (sxu32)(p->zEnd - p->z) >= n && SyMemcmp(p->z,zTok,n) == 0 ){
		p->z += n;
		return 1;
	}
	return 0;
}
static sxu64 GtPlPrimary(gt_plural *p)
{
	GtPlSpace(p);
	if( p->z >= p->zEnd ){
		p->bBad = 1;
		return 0;
	}
	if( p->z[0] == '!' ){
		p->z++;
		return GtPlPrimary(p) == 0 ? 1 : 0;
	}
	if( p->z[0] == '(' ){
		sxu64 v;
		p->z++;
		v = GtPlCond(p);
		if( !GtPlEat(p,")") ){
			p->bBad = 1;
		}
		return v;
	}
	if( p->z[0] == 'n' ){
		p->z++;
		return p->n;
	}
	if( p->z[0] >= '0' && p->z[0] <= '9' ){
		sxu64 v = 0;
		while( p->z < p->zEnd && p->z[0] >= '0' && p->z[0] <= '9' ){
			v = v * 10 + (sxu64)(p->z[0] - '0');
			p->z++;
		}
		return v;
	}
	p->bBad = 1;
	return 0;
}
static sxu64 GtPlMul(gt_plural *p)
{
	sxu64 v = GtPlPrimary(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"*") ){
			v = v * GtPlPrimary(p);
		}else if( GtPlEat(p,"/") ){
			sxu64 r = GtPlPrimary(p);
			v = r ? v / r : 0;
		}else if( GtPlEat(p,"%") ){
			sxu64 r = GtPlPrimary(p);
			v = r ? v % r : 0;
		}else{
			return v;
		}
	}
}
static sxu64 GtPlAdd(gt_plural *p)
{
	sxu64 v = GtPlMul(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"+") ){
			v = v + GtPlMul(p);
		}else if( GtPlEat(p,"-") ){
			v = v - GtPlMul(p);
		}else{
			return v;
		}
	}
}
static sxu64 GtPlRel(gt_plural *p)
{
	sxu64 v = GtPlAdd(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"<=") ){
			v = v <= GtPlAdd(p) ? 1 : 0;
		}else if( GtPlEat(p,">=") ){
			v = v >= GtPlAdd(p) ? 1 : 0;
		}else if( p->z < p->zEnd && p->z[0] == '<' ){
			p->z++;
			v = v < GtPlAdd(p) ? 1 : 0;
		}else if( p->z < p->zEnd && p->z[0] == '>' ){
			p->z++;
			v = v > GtPlAdd(p) ? 1 : 0;
		}else{
			return v;
		}
	}
}
static sxu64 GtPlEq(gt_plural *p)
{
	sxu64 v = GtPlRel(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"==") ){
			v = v == GtPlRel(p) ? 1 : 0;
		}else if( GtPlEat(p,"!=") ){
			v = v != GtPlRel(p) ? 1 : 0;
		}else{
			return v;
		}
	}
}
static sxu64 GtPlAnd(gt_plural *p)
{
	sxu64 v = GtPlEq(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"&&") ){
			sxu64 r = GtPlEq(p);
			v = (v && r) ? 1 : 0;
		}else{
			return v;
		}
	}
}
static sxu64 GtPlOr(gt_plural *p)
{
	sxu64 v = GtPlAnd(p);
	for(;;){
		GtPlSpace(p);
		if( GtPlEat(p,"||") ){
			sxu64 r = GtPlAnd(p);
			v = (v || r) ? 1 : 0;
		}else{
			return v;
		}
	}
}
static sxu64 GtPlCond(gt_plural *p)
{
	sxu64 v = GtPlOr(p);
	GtPlSpace(p);
	if( GtPlEat(p,"?") ){
		sxu64 a = GtPlCond(p);
		sxu64 b;
		if( !GtPlEat(p,":") ){
			p->bBad = 1;
			return 0;
		}
		b = GtPlCond(p);
		return v ? a : b;
	}
	return v;
}
/* The form index this catalog gives a count, or the Germanic rule with no
 * catalog (and with an expression that does not parse). */
static sxu32 GtPluralIndex(gt_cat *pCat,sxu64 n)
{
	gt_plural sP;
	sxu64 v;
	if( pCat == 0 || SyBlobLength(&pCat->sPlural) < 1 ){
		return n == 1 ? 0 : 1;
	}
	sP.z = (const char *)SyBlobData(&pCat->sPlural);
	sP.zEnd = &sP.z[SyBlobLength(&pCat->sPlural)];
	sP.n = n;
	sP.bBad = 0;
	v = GtPlCond(&sP);
	GtPlSpace(&sP);
	if( sP.bBad || sP.z != sP.zEnd ){
		/* glibc runs a bison parser over the whole field, so text it cannot
		 * reduce -- including anything LEFT OVER after a complete expression --
		 * is a catalog with no plural rule, and the Germanic one answers. */
		return n == 1 ? 0 : 1;
	}
	return v > 0xffffffffu ? 0 : (sxu32)v;
}

/* --- Locale resolution -------------------------------------------------- */

/*
 * One environment variable, read the way php's own putenv() WROTE it. On
 * Windows that is SetEnvironmentVariable(), which the CRT's getenv() copy does
 * not see, so the process block has to be asked directly -- otherwise a script
 * that does putenv("LANGUAGE=pt_BR") would be answered the language it started
 * with. Answers the length written, 0 for absent or empty.
 */
static int GtGetenv(const char *zName,char *zBuf,int nBuf)
{
#ifdef __WINNT__
	DWORD n = GetEnvironmentVariableA(zName,zBuf,(DWORD)nBuf);
	if( n == 0 || n >= (DWORD)nBuf ){
		zBuf[0] = 0;
		return 0;
	}
	return (int)n;
#else
	const char *z = getenv(zName);
	int n;
	if( z == 0 || z[0] == 0 ){
		zBuf[0] = 0;
		return 0;
	}
	n = (int)SyStrlen(z);
	if( n >= nBuf ){
		zBuf[0] = 0;
		return 0;
	}
	SyMemcpy(z,zBuf,(sxu32)n);
	zBuf[n] = 0;
	return n;
#endif
}
static const char * GtCategoryName(int iCat)
{
	switch( iCat ){
		case GT_LC_CTYPE:    return "LC_CTYPE";
		case GT_LC_NUMERIC:  return "LC_NUMERIC";
		case GT_LC_TIME:     return "LC_TIME";
		case GT_LC_COLLATE:  return "LC_COLLATE";
		case GT_LC_MONETARY: return "LC_MONETARY";
		case GT_LC_MESSAGES: return "LC_MESSAGES";
		default:             return 0;
	}
}
/*
 * What the C library says this category is set to. Windows has no LC_MESSAGES
 * at all, so the ENVIRONMENT answers for it there -- which is the same lookup
 * (LC_ALL, then the category, then LANG) libintl's own POSIX path makes.
 */
static void GtLocaleName(int iCat,char *zBuf,int nBuf)
{
	const char *z = 0;
	int n;
	switch( iCat ){
		case GT_LC_CTYPE:    z = setlocale(LC_CTYPE,0);    break;
		case GT_LC_NUMERIC:  z = setlocale(LC_NUMERIC,0);  break;
		case GT_LC_TIME:     z = setlocale(LC_TIME,0);     break;
		case GT_LC_COLLATE:  z = setlocale(LC_COLLATE,0);  break;
		case GT_LC_MONETARY: z = setlocale(LC_MONETARY,0); break;
#ifdef LC_MESSAGES
		case GT_LC_MESSAGES: z = setlocale(LC_MESSAGES,0); break;
#else
		/* Windows has no LC_MESSAGES category at all, so libintl's own POSIX
		 * path answers for it there: LC_ALL, then the category, then LANG. */
		case GT_LC_MESSAGES:
			if( GtGetenv("LC_ALL",zBuf,nBuf) > 0
			 || GtGetenv("LC_MESSAGES",zBuf,nBuf) > 0
			 || GtGetenv("LANG",zBuf,nBuf) > 0 ){
				return;
			}
			z = 0;
			break;
#endif
		default: break;
	}
	if( z == 0 || z[0] == 0 ){
		z = "C";
	}
	n = (int)SyStrlen(z);
	if( n >= nBuf ){
		n = nBuf - 1;
	}
	SyMemcpy(z,zBuf,(sxu32)n);
	zBuf[n] = 0;
}
/*
 * Is this the locale gettext reads NO catalog in? Measured rather than read,
 * because the rule is wider than the "C" the documentation names: a locale
 * whose language part is `C` and whose next character is a `.` counts too, so
 * `C.UTF-8`, `C.utf8` and even `C.anything` are all the C locale here --
 * which matters because `C.UTF-8` is what a modern container is usually in.
 * `POSIX` counts by name; `POSIX.UTF-8`, `Cx`, `C_XX`, `C@mod` and a lower-case
 * `c` do not. The test applies both to the category's own locale (which is what
 * decides whether LANGUAGE is consulted at all) and to each element of the
 * LANGUAGE list (where it ENDS the search rather than skipping one element).
 */
static int GtIsPosixLocale(const char *z,int n)
{
	if( n >= 1 && z[0] == 'C' && (n == 1 || z[1] == '.') ){
		return 1;
	}
	return n == 5 && SyMemcmp(z,"POSIX",5) == 0;
}
/*
 * The list gettext actually walks: LANGUAGE when it is set and the category is
 * not "C", the category's own locale otherwise.
 */
static void GtCategoryValue(int iCat,char *zBuf,int nBuf)
{
	char zLoc[GT_LOCALE_MAX];
	GtLocaleName(iCat,zLoc,(int)sizeof(zLoc));
	if( !GtIsPosixLocale(zLoc,(int)SyStrlen(zLoc)) && GtGetenv("LANGUAGE",zBuf,nBuf) > 0 ){
		return;
	}
	Systrcpy(zBuf,(sxu32)nBuf,zLoc,0);
}
/*
 * glibc's _nl_normalize_codeset: keep the alphanumerics, lower-case them, and
 * prefix `iso` when nothing but digits survives. Answers 0 when the result is
 * the same as the input, which is what decides whether a `.normalised` form
 * appears in the candidate list at all.
 */
static int GtNormCodeset(const char *z,int n,char *zOut,int nOut)
{
	int i,k = 0,bDigit = 1;
	if( n + 3 > nOut ){
		return 0;   /* no room for the normalised spelling: do not offer one */
	}
	for( i = 0 ; i < n ; ++i ){
		int c = (unsigned char)z[i];
		if( c >= '0' && c <= '9' ){
			if( k < nOut ){ zOut[k++] = (char)c; }
		}else if( c >= 'A' && c <= 'Z' ){
			bDigit = 0;
			if( k < nOut ){ zOut[k++] = (char)(c - 'A' + 'a'); }
		}else if( c >= 'a' && c <= 'z' ){
			bDigit = 0;
			if( k < nOut ){ zOut[k++] = (char)c; }
		}
	}
	if( bDigit && k > 0 && k + 3 <= nOut ){
		int j;
		for( j = k - 1 ; j >= 0 ; --j ){
			zOut[j + 3] = zOut[j];
		}
		zOut[0] = 'i'; zOut[1] = 's'; zOut[2] = 'o';
		k += 3;
	}
	if( k == n && k > 0 && SyMemcmp(zOut,z,(sxu32)k) == 0 ){
		return 0;   /* normalising changed nothing */
	}
	return k;
}
/* The four parts of `language[_territory][.codeset][@modifier]`. */
typedef struct gt_name gt_name;
struct gt_name {
	const char *zLang;  int nLang;
	const char *zTerr;  int nTerr;
	const char *zCode;  int nCode;
	const char *zMod;   int nMod;
	char zNorm[64];     int nNorm;
};
static void GtExplode(const char *z,int n,gt_name *p)
{
	int i = 0;
	SyZero(p,sizeof(*p));
	p->zLang = z;
	while( i < n && z[i] != '_' && z[i] != '.' && z[i] != '@' ){
		i++;
	}
	p->nLang = i;
	if( i < n && z[i] == '_' ){
		int j = ++i;
		while( i < n && z[i] != '.' && z[i] != '@' ){
			i++;
		}
		p->zTerr = &z[j];
		p->nTerr = i - j;
	}
	if( i < n && z[i] == '.' ){
		int j = ++i;
		while( i < n && z[i] != '@' ){
			i++;
		}
		p->zCode = &z[j];
		p->nCode = i - j;
		p->nNorm = GtNormCodeset(p->zCode,p->nCode,p->zNorm,(int)sizeof(p->zNorm));
	}
	if( i < n && z[i] == '@' ){
		p->zMod = &z[i + 1];
		p->nMod = n - i - 1;
	}
}
/*
 * glibc's mask, and its bit VALUES -- the order the candidates come out in is
 * exactly the descending order of these, so they are contract rather than
 * taste: the modifier is the most significant part, the normalised codeset the
 * least, and a candidate never carries both spellings of the codeset.
 */
#define GT_MASK_NORM   1
#define GT_MASK_CODE   2
#define GT_MASK_TERR   4
#define GT_MASK_MOD    8

/* --- Reading a catalog -------------------------------------------------- */

#ifndef PH7_DISABLE_DISK_IO
/*
 * Read a whole file through the default (plain-file) device. Deliberately NOT
 * PH7_VmGetStreamDevice(): libintl calls open(2), so a bound directory naming
 * a userland wrapper is not one, and a `://` inside a locale directory is a
 * directory name here as it is there.
 */
static int GtReadFile(ph7_vm *pVm,const char *zPath,SyBlob *pOut)
{
	const ph7_io_stream *pStream = pVm->pDefStream;
	void *pHandle;
	if( pStream == 0 ){
		return -1;
	}
	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,"gettext");
	if( pHandle == 0 ){
		return -1;
	}
	PH7_StreamReadWholeFile(pHandle,pStream,pOut);
	PH7_StreamCloseHandle(pStream,pHandle);
	return SyBlobLength(pOut) > 0 ? PH7_OK : -1;
}
#else
static int GtReadFile(ph7_vm *pVm,const char *zPath,SyBlob *pOut)
{
	SXUNUSED(pVm); SXUNUSED(zPath); SXUNUSED(pOut);
	return -1;   /* a build with no disk I/O carries no catalogs */
}
#endif /* PH7_DISABLE_DISK_IO */

/* Drop every catalog a previous resolution loaded. */
static void GtDropCats(ph7_vm *pVm,gt_dom *pDom)
{
	gt_cat *pCat = pDom->pCats,*pNext;
	while( pCat ){
		pNext = pCat->pNext;
		SyBlobRelease(&pCat->sData);
		SyBlobRelease(&pCat->sPlural);
		SyBlobRelease(&pCat->sCharset);
		SyMemBackendFree(&pVm->sAllocator,pCat);
		pCat = pNext;
	}
	pDom->pCats = 0;
}
/*
 * Read `<dir>/<candidate>/<category>/<domain>.mo` and, if it is a catalog, add
 * it to the END of this domain's chain. Answers PH7_OK when one was added.
 */
static int GtTryLoad(ph7_vm *pVm,gt_dom *pDom,const char *zDir,int nDir,
	const char *zCand,int nCand,const char *zCat)
{
	SyBlob sPath;
	gt_cat *pCat,**ppTail;
	int rc;
	pCat = (gt_cat *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_cat));
	if( pCat == 0 ){
		return -1;
	}
	SyZero(pCat,sizeof(gt_cat));
	SyBlobInit(&pCat->sData,&pVm->sAllocator);
	SyBlobInit(&pCat->sPlural,&pVm->sAllocator);
	SyBlobInit(&pCat->sCharset,&pVm->sAllocator);
	SyBlobInit(&sPath,&pVm->sAllocator);
	SyBlobAppend(&sPath,zDir,(sxu32)nDir);
	SyBlobAppend(&sPath,"/",1);
	SyBlobAppend(&sPath,zCand,(sxu32)nCand);
	SyBlobAppend(&sPath,"/",1);
	SyBlobAppend(&sPath,zCat,(sxu32)SyStrlen(zCat));
	SyBlobAppend(&sPath,"/",1);
	SyBlobAppend(&sPath,SyBlobData(&pDom->sName),SyBlobLength(&pDom->sName));
	SyBlobAppend(&sPath,".mo",3);
	SyBlobNullAppend(&sPath);
	rc = GtReadFile(pVm,(const char *)SyBlobData(&sPath),&pCat->sData);
	SyBlobRelease(&sPath);
	if( rc == PH7_OK ){
		rc = GtParseMo(pCat);
	}
	if( rc != PH7_OK ){
		SyBlobRelease(&pCat->sData);
		SyBlobRelease(&pCat->sPlural);
		SyBlobRelease(&pCat->sCharset);
		SyMemBackendFree(&pVm->sAllocator,pCat);
		return -1;
	}
	for( ppTail = &pDom->pCats ; *ppTail ; ppTail = &(*ppTail)->pNext ){}
	*ppTail = pCat;
	return PH7_OK;
}
/*
 * Walk one locale NAME's candidate list, most specific first, and add EVERY
 * candidate that carries a catalog to the domain's chain. Answers how many.
 */
static int GtLoadForName(ph7_vm *pVm,gt_dom *pDom,const char *zDir,int nDir,
	const char *zName,int nName,const char *zCat)
{
	gt_name sN;
	int mask = 0,cnt,nLoaded = 0;
	if( nName < 1 ){
		return 0;
	}
	GtExplode(zName,nName,&sN);
	if( sN.nLang < 1 ){
		return 0;
	}
	if( sN.nTerr > 0 ){ mask |= GT_MASK_TERR; }
	if( sN.nCode > 0 ){ mask |= GT_MASK_CODE; }
	if( sN.nNorm > 0 ){ mask |= GT_MASK_NORM; }
	if( sN.nMod  > 0 ){ mask |= GT_MASK_MOD;  }
	for( cnt = mask ; cnt >= 0 ; --cnt ){
		char zBuf[GT_LOCALE_MAX];
		int k = sN.nLang;
		if( (cnt & ~mask) != 0 ){
			continue;
		}
		if( (cnt & GT_MASK_CODE) && (cnt & GT_MASK_NORM) ){
			continue;   /* one spelling of the codeset at a time */
		}
		/* Measure first: a candidate that would not fit is SKIPPED rather than
		 * truncated -- a shortened name is a different directory, and finding a
		 * catalog under it would be a wrong answer rather than a missing one. */
		if( cnt & GT_MASK_TERR ){ k += 1 + sN.nTerr; }
		if( cnt & GT_MASK_CODE ){ k += 1 + sN.nCode; }
		else if( cnt & GT_MASK_NORM ){ k += 1 + sN.nNorm; }
		if( cnt & GT_MASK_MOD ){ k += 1 + sN.nMod; }
		if( k >= (int)sizeof(zBuf) ){
			continue;
		}
		SyMemcpy(sN.zLang,zBuf,(sxu32)sN.nLang);
		k = sN.nLang;
		if( cnt & GT_MASK_TERR ){
			zBuf[k++] = '_';
			SyMemcpy(sN.zTerr,&zBuf[k],(sxu32)sN.nTerr);
			k += sN.nTerr;
		}
		if( cnt & GT_MASK_CODE ){
			zBuf[k++] = '.';
			SyMemcpy(sN.zCode,&zBuf[k],(sxu32)sN.nCode);
			k += sN.nCode;
		}else if( cnt & GT_MASK_NORM ){
			zBuf[k++] = '.';
			SyMemcpy(sN.zNorm,&zBuf[k],(sxu32)sN.nNorm);
			k += sN.nNorm;
		}
		if( cnt & GT_MASK_MOD ){
			zBuf[k++] = '@';
			SyMemcpy(sN.zMod,&zBuf[k],(sxu32)sN.nMod);
			k += sN.nMod;
		}
		if( GtTryLoad(pVm,pDom,zDir,nDir,zBuf,k,zCat) == PH7_OK ){
			nLoaded++;
			if( nLoaded >= GT_MAX_CATS ){
				break;
			}
		}
	}
	return nLoaded;
}
/*
 * Resolve (and cache) the catalog CHAIN this domain answers from right now,
 * head first. The cache key is the category value and the category name
 * together, so a setlocale() or a putenv("LANGUAGE=…") between two calls is
 * noticed.
 */
static gt_cat * GtResolve(ph7_vm *pVm,gt_dom *pDom,int iCat)
{
	const char *zCat = GtCategoryName(iCat);
	const char *zDir;
	char zVal[GT_LOCALE_MAX];
	int nVal,nDir,i,nLoaded = 0;
	SyBlob sKey;
	if( pDom == 0 || zCat == 0 ){
		return 0;
	}
	GtCategoryValue(iCat,zVal,(int)sizeof(zVal));
	nVal = (int)SyStrlen(zVal);
	SyBlobInit(&sKey,&pVm->sAllocator);
	SyBlobAppend(&sKey,zCat,(sxu32)SyStrlen(zCat));
	SyBlobAppend(&sKey,"|",1);
	SyBlobAppend(&sKey,zVal,(sxu32)nVal);
	if( pDom->bResolved && SyBlobCmp(&sKey,&pDom->sKey) == 0 ){
		SyBlobRelease(&sKey);
		return pDom->pCats;
	}
	SyBlobReset(&pDom->sKey);
	SyBlobAppend(&pDom->sKey,SyBlobData(&sKey),SyBlobLength(&sKey));
	SyBlobRelease(&sKey);
	pDom->bResolved = 1;
	GtDropCats(pVm,pDom);   /* a fresh resolution owns its own files */
	/* The "C" locale reads no catalog at all, and neither does a LANGUAGE
	 * whose first element says so. */
	if( GtIsPosixLocale(zVal,nVal) ){
		return 0;
	}
	zDir = (const char *)SyBlobData(&pDom->sDir);
	nDir = (int)SyBlobLength(&pDom->sDir);
	if( nDir < 1 ){
		zDir = GT_DEFAULT_DIR;
		nDir = (int)SyStrlen(GT_DEFAULT_DIR);
	}
	/* LANGUAGE is a colon-separated PRIORITY LIST; an element spelled "C" or
	 * "POSIX" ends the search with nothing. */
	i = 0;
	while( i <= nVal ){
		int j = i;
		while( j < nVal && zVal[j] != ':' ){
			j++;
		}
		if( j > i ){
			if( GtIsPosixLocale(&zVal[i],j - i) ){
				break;
			}
			nLoaded += GtLoadForName(pVm,pDom,zDir,nDir,&zVal[i],j - i,zCat);
			if( nLoaded >= GT_MAX_CATS ){
				break;
			}
		}
		i = j + 1;
	}
	return pDom->pCats;
}

/* --- Answering ---------------------------------------------------------- */

/*
 * Hand back one answer, converted if the domain and the catalog disagree on
 * the encoding. A conversion that cannot be opened -- an unknown name, or one
 * of the code sets §10 leaves out -- gives php's own answer for the same case:
 * the msgid, untranslated.
 */
static void GtResult(ph7_context *pCtx,gt_dom *pDom,gt_cat *pCat,
	const char *zHit,int nHit,const char *zMiss,int nMiss)
{
	const char *zFrom,*zTo;
	int nFrom,nTo;
	SyBlob sOut;
	char zCtype[64];
	if( zHit == 0 ){
		ph7_result_string(pCtx,zMiss,nMiss);
		return;
	}
	zFrom = pCat ? (const char *)SyBlobData(&pCat->sCharset) : 0;
	nFrom = pCat ? (int)SyBlobLength(&pCat->sCharset) : 0;
	if( nFrom < 1 ){
		/* A catalog that names no charset is not converted -- glibc's rule. */
		ph7_result_string(pCtx,zHit,nHit);
		return;
	}
	zTo = (const char *)SyBlobData(&pDom->sCodeset);
	nTo = (int)SyBlobLength(&pDom->sCodeset);
	if( nTo < 1 ){
		/* Nothing bound: the output charset is the LC_CTYPE locale's, which is
		 * what glibc reads. `C`/`POSIX` (and a name with no `.codeset`) is
		 * ASCII, spelled the way the C library spells it. */
		char zLoc[GT_LOCALE_MAX];
		int nLoc,k;
		GtLocaleName(GT_LC_CTYPE,zLoc,(int)sizeof(zLoc));
		nLoc = (int)SyStrlen(zLoc);
		for( k = 0 ; k < nLoc && zLoc[k] != '.' ; ++k ){}
		if( k < nLoc - 1 ){
			int m = 0;
			k++;
			while( k < nLoc && zLoc[k] != '@' && m + 1 < (int)sizeof(zCtype) ){
				zCtype[m++] = zLoc[k++];
			}
			zCtype[m] = 0;
			nTo = m;
		}else{
			Systrcpy(zCtype,(sxu32)sizeof(zCtype),"ANSI_X3.4-1968",0);
			nTo = (int)SyStrlen(zCtype);
		}
		zTo = zCtype;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	if( PH7_IconvTranslate(&sOut,zHit,nHit,zFrom,nFrom,zTo,nTo) == PH7_OK ){
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	}else{
		ph7_result_string(pCtx,zMiss,nMiss);
	}
	SyBlobRelease(&sOut);
}
/*
 * The C-string view of a php string: libintl takes a `const char *`, so a NUL
 * inside the argument ENDS it for the lookup -- and the miss answer is still
 * the caller's whole string, because php returns the zend_string it was given
 * whenever libintl handed the pointer straight back.
 */
static int GtCStrLen(const char *z,int n)
{
	int i;
	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}
	return i;
}
/* php's screens, in php's order. Answers 0 when the argument is fine. */
static int GtScreenDomain(ph7_context *pCtx,ph7_value *pArg,int iPos,const char **pz,int *pn)
{
	int n = 0;
	const char *z = ph7_value_to_string(pArg,&n);
	if( n < 1 ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($domain) must not be empty",ph7_function_name(pCtx),iPos);
		return -1;
	}
	if( n > GT_MAX_DOMAIN ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($domain) is too long",ph7_function_name(pCtx),iPos);
		return -1;
	}
	/* Both screens above read php's WHOLE string; libintl then gets a C string,
	 * so the name the binding is filed under stops at the first NUL --
	 * `textdomain("dom\0x")` answers "dom", and a domain that is nothing BUT a
	 * NUL is the empty one libintl reads as "no domain at all". */
	*pz = z;
	*pn = GtCStrLen(z,n);
	return 0;
}
static int GtScreenMsg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,
	const char **pz,int *pn)
{
	int n = 0;
	const char *z = ph7_value_to_string(pArg,&n);
	if( n > GT_MAX_MSGID ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($%s) is too long",ph7_function_name(pCtx),iPos,zName);
		return -1;
	}
	*pz = z;
	*pn = n;
	return 0;
}
/* dcgettext()/dcngettext() refuse LC_ALL by name and pass everything else on. */
static int GtScreenCategory(ph7_context *pCtx,ph7_value *pArg,int iPos,int *piCat)
{
	sxi64 iVal = ph7_value_to_int64(pArg);
	if( iVal == GT_LC_ALL ){
		PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #%d ($category) cannot be LC_ALL",ph7_function_name(pCtx),iPos);
		return -1;
	}
	*piCat = (int)iVal;
	return 0;
}
/* The current textdomain, which is "messages" until one is set. */
static void GtCurrentDomain(ph7_vm *pVm,const char **pz,int *pn)
{
	gt_state *pState = GtState(pVm);
	if( pState && SyBlobLength(&pState->sDomain) > 0 ){
		*pz = (const char *)SyBlobData(&pState->sDomain);
		*pn = (int)SyBlobLength(&pState->sDomain);
		return;
	}
	*pz = "messages";
	*pn = 8;
}
/*
 * The first catalog in the chain that CARRIES this msgid, with its translation
 * block. Answers 0 when no candidate has it -- which is not the same as "no
 * catalog": a region catalog that says nothing about a msgid falls through to
 * the language one behind it.
 */
static gt_cat * GtFindInChain(ph7_vm *pVm,gt_cat *pChain,const char *zMsg,int nMsg,
	const char **pzHit,sxu32 *pnHit)
{
	SyBlob sKey;
	gt_cat *pCat;
	SyBlobInit(&sKey,&pVm->sAllocator);
	SyBlobAppend(&sKey,zMsg,(sxu32)GtCStrLen(zMsg,nMsg));
	SyBlobNullAppend(&sKey);
	for( pCat = pChain ; pCat ; pCat = pCat->pNext ){
		if( GtFindMsg(pCat,(const char *)SyBlobData(&sKey),pzHit,pnHit) == PH7_OK ){
			SyBlobRelease(&sKey);
			return pCat;
		}
	}
	SyBlobRelease(&sKey);
	return 0;
}
/* The whole of dcgettext(), which every singular door funnels into. */
static void GtLookupOne(ph7_context *pCtx,const char *zDom,int nDom,
	const char *zMsg,int nMsg,int iCat)
{
	ph7_vm *pVm = pCtx->pVm;
	gt_dom *pDom = GtDomain(pVm,zDom,nDom);
	gt_cat *pCat;
	const char *zHit = 0;
	sxu32 nHit = 0;
	if( pDom == 0 ){
		ph7_result_string(pCtx,zMsg,nMsg);
		return;
	}
	pCat = GtFindInChain(pVm,GtResolve(pVm,pDom,iCat),zMsg,nMsg,&zHit,&nHit);
	if( pCat == 0 ){
		zHit = 0;
	}else{
		/* php hands libintl's `const char *` to RETURN_STRING, so an answer stops
		 * at its first NUL -- which is what a PLURAL entry reached through the
		 * singular door gives back: its first form. */
		nHit = (sxu32)GtCStrLen(zHit,(int)nHit);
	}
	GtResult(pCtx,pDom,pCat,zHit,(int)nHit,zMsg,nMsg);
}
/* The whole of dcngettext(). */
static void GtLookupPlural(ph7_context *pCtx,const char *zDom,int nDom,
	const char *zOne,int nOne,const char *zMany,int nMany,sxi64 iCount,int iCat)
{
	ph7_vm *pVm = pCtx->pVm;
	gt_dom *pDom = GtDomain(pVm,zDom,nDom);
	gt_cat *pCat;
	const char *zHit = 0;
	sxu32 nHit = 0;
	sxu64 n = (sxu64)iCount;
	const char *zMiss = n == 1 ? zOne : zMany;
	int nMiss = n == 1 ? nOne : nMany;
	if( pDom == 0 ){
		ph7_result_string(pCtx,zMiss,nMiss);
		return;
	}
	/* The SINGULAR is the key: a plural entry is stored `singular\0plural` and
	 * compares equal to its own singular under the C-string comparison the
	 * format's binary search uses. */
	pCat = GtFindInChain(pVm,GtResolve(pVm,pDom,iCat),zOne,nOne,&zHit,&nHit);
	if( pCat == 0 ){
		zHit = 0;
	}else{
		/* The plural RULE is the found catalog's, not the chain head's. */
		const char *zBase = zHit;
		sxu32 nBase = nHit, iForm = GtPluralIndex(pCat,n), i;
		/* An index the header cannot hold is form 0, and so is a block that
		 * runs out of forms before the index -- both are glibc's own
		 * "this should never happen" answers. */
		if( iForm >= pCat->nPlurals ){
			iForm = 0;
		}
		for( i = 0 ; i < iForm ; ++i ){
			sxu32 k = (sxu32)GtCStrLen(zHit,(int)nHit);
			if( k + 1 >= nHit ){
				zHit = zBase;
				nHit = nBase;
				break;
			}
			zHit += k + 1;
			nHit -= k + 1;
		}
		nHit = (sxu32)GtCStrLen(zHit,(int)nHit);
	}
	GtResult(pCtx,pDom,pCat,zHit,(int)nHit,zMiss,nMiss);
}

/* --- The ten functions --------------------------------------------------- */

/*
 * string textdomain(?string $domain = null)
 *  Set (or, with null, only read) the domain gettext()/ngettext() answer from.
 */
PH7_PRIVATE int PH7_builtin_textdomain(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	gt_state *pState;
	const char *z;
	int n;
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		if( GtScreenDomain(pCtx,apArg[0],1,&z,&n) != 0 ){
			return PH7_OK;
		}
		/* An EMPTY C string RESETS the domain: libintl's textdomain("") puts
		 * `messages` back, which is what a name that is nothing but a NUL asks
		 * for (php's own screen refuses the empty php string before this). */
		pState = GtState(pVm);
		if( pState ){
			SyBlobReset(&pState->sDomain);
			SyBlobAppend(&pState->sDomain,z,(sxu32)n);
		}
	}
	GtCurrentDomain(pVm,&z,&n);
	ph7_result_string(pCtx,z,n);
	return PH7_OK;
}
/*
 * string|false bindtextdomain(string $domain, ?string $directory = null)
 *  Where this domain's catalogs live. php makes the directory ABSOLUTE with
 *  realpath() before libintl ever sees it, so a relative name that does not
 *  resolve is FALSE rather than a binding nothing can read.
 */
PH7_PRIVATE int PH7_builtin_bindtextdomain(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_vfs *pVfs = pVm->pEngine->pVfs;
	gt_dom *pDom;
	const char *zDom,*zDir,*zAbs;
	int nDom,nDir = 0,nAbs = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0 ){
		return PH7_OK;
	}
	pDom = GtDomain(pVm,zDom,nDom);
	if( pDom == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg < 2 || ph7_value_is_null(apArg[1]) ){
		/* A query: the directory in force, or the library's own default. */
		if( SyBlobLength(&pDom->sDir) > 0 ){
			ph7_result_string(pCtx,(const char *)SyBlobData(&pDom->sDir),
				(int)SyBlobLength(&pDom->sDir));
		}else{
			ph7_result_string(pCtx,GT_DEFAULT_DIR,-1);
		}
		return PH7_OK;
	}
	zDir = ph7_value_to_string(apArg[1],&nDir);
	/* The VFS writes STRAIGHT into the call's result, so seed it empty and read
	 * the answer back out -- which is also the value this call returns. */
	ph7_result_string(pCtx,"",0);
	if( nDir < 1 || (nDir == 1 && zDir[0] == '0') ){
		/* php reads an empty directory (and the string "0") as the working one. */
		if( pVfs == 0 || pVfs->xGetcwd == 0 || pVfs->xGetcwd(pCtx) != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}else{
		SyBlob sPath;
		int rc;
		SyBlobInit(&sPath,&pVm->sAllocator);
		SyBlobAppend(&sPath,zDir,(sxu32)nDir);
		SyBlobNullAppend(&sPath);
		rc = (pVfs == 0 || pVfs->xRealpath == 0)
			? -1 : pVfs->xRealpath((const char *)SyBlobData(&sPath),pCtx);
		SyBlobRelease(&sPath);
		if( rc != PH7_OK ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	zAbs = ph7_value_to_string(pCtx->pRet,&nAbs);
	SyBlobReset(&pDom->sDir);
	SyBlobAppend(&pDom->sDir,zAbs,(sxu32)nAbs);
	pDom->bResolved = 0;   /* a new directory: re-read on the next lookup */
	return PH7_OK;
}
/*
 * string|false bind_textdomain_codeset(string $domain, ?string $codeset = null)
 *  The encoding this domain's answers come back in. With null (or nothing) it
 *  only READS the binding, and answers false when there is none.
 */
PH7_PRIVATE int PH7_builtin_bind_textdomain_codeset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	gt_dom *pDom;
	const char *zDom,*zCs;
	int nDom,nCs = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0 ){
		return PH7_OK;
	}
	pDom = nDom > 0 ? GtDomain(pVm,zDom,nDom) : 0;
	if( pDom == 0 ){
		/* libintl reads the empty domain name as no domain at all. */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
		zCs = ph7_value_to_string(apArg[1],&nCs);
		SyBlobReset(&pDom->sCodeset);
		SyBlobAppend(&pDom->sCodeset,zCs,(sxu32)GtCStrLen(zCs,nCs));
	}
	if( SyBlobLength(&pDom->sCodeset) < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&pDom->sCodeset),
		(int)SyBlobLength(&pDom->sCodeset));
	return PH7_OK;
}
/*
 * string gettext(string $message) / string _(string $message)
 */
PH7_PRIVATE int PH7_builtin_gettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zMsg;
	int nDom,nMsg;
	if( nArg < 1 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenMsg(pCtx,apArg[0],1,"message",&zMsg,&nMsg) != 0 ){
		return PH7_OK;
	}
	GtCurrentDomain(pCtx->pVm,&zDom,&nDom);
	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,GT_LC_MESSAGES);
	return PH7_OK;
}
/*
 * string dgettext(string $domain, string $message)
 */
PH7_PRIVATE int PH7_builtin_dgettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zMsg;
	int nDom,nMsg;
	if( nArg < 2 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0
	 || GtScreenMsg(pCtx,apArg[1],2,"message",&zMsg,&nMsg) != 0 ){
		return PH7_OK;
	}
	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,GT_LC_MESSAGES);
	return PH7_OK;
}
/*
 * string dcgettext(string $domain, string $message, int $category)
 */
PH7_PRIVATE int PH7_builtin_dcgettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zMsg;
	int nDom,nMsg,iCat;
	if( nArg < 3 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0
	 || GtScreenMsg(pCtx,apArg[1],2,"message",&zMsg,&nMsg) != 0
	 || GtScreenCategory(pCtx,apArg[2],3,&iCat) != 0 ){
		return PH7_OK;
	}
	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,iCat);
	return PH7_OK;
}
/*
 * string ngettext(string $singular, string $plural, int $count)
 */
PH7_PRIVATE int PH7_builtin_ngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zOne,*zMany;
	int nDom,nOne,nMany;
	if( nArg < 3 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenMsg(pCtx,apArg[0],1,"singular",&zOne,&nOne) != 0
	 || GtScreenMsg(pCtx,apArg[1],2,"plural",&zMany,&nMany) != 0 ){
		return PH7_OK;
	}
	GtCurrentDomain(pCtx->pVm,&zDom,&nDom);
	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,
		ph7_value_to_int64(apArg[2]),GT_LC_MESSAGES);
	return PH7_OK;
}
/*
 * string dngettext(string $domain, string $singular, string $plural, int $count)
 */
PH7_PRIVATE int PH7_builtin_dngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zOne,*zMany;
	int nDom,nOne,nMany;
	if( nArg < 4 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0
	 || GtScreenMsg(pCtx,apArg[1],2,"singular",&zOne,&nOne) != 0
	 || GtScreenMsg(pCtx,apArg[2],3,"plural",&zMany,&nMany) != 0 ){
		return PH7_OK;
	}
	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,
		ph7_value_to_int64(apArg[3]),GT_LC_MESSAGES);
	return PH7_OK;
}
/*
 * string dcngettext(string $domain, string $singular, string $plural,
 *                   int $count, int $category)
 */
PH7_PRIVATE int PH7_builtin_dcngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDom,*zOne,*zMany;
	int nDom,nOne,nMany,iCat;
	if( nArg < 5 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0
	 || GtScreenMsg(pCtx,apArg[1],2,"singular",&zOne,&nOne) != 0
	 || GtScreenMsg(pCtx,apArg[2],3,"plural",&zMany,&nMany) != 0
	 || GtScreenCategory(pCtx,apArg[4],5,&iCat) != 0 ){
		return PH7_OK;
	}
	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,
		ph7_value_to_int64(apArg[3]),iCat);
	return PH7_OK;
}
#endif /* PH7_DISABLE_BUILTIN_FUNC */
