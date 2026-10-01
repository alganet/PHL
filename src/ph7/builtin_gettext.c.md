# src/ph7/builtin_gettext.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 811/926 lines (87.58%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#include "ph7int.h"` |
|    - |    6 | `#include <locale.h>` |
|    - |    7 | `#include <stdlib.h>   /* getenv */` |
|    - |    8 | `#ifdef __WINNT__` |
|    - |    9 | `#include <Windows.h>  /* GetEnvironmentVariableA -- see GtGetenv() below */` |
|    - |   10 | `#endif` |
|    - |   11 | `/*` |
|    - |   12 | ` * Section:` |
|    - |   13 | ` *    php's gettext extension: the message catalogs a translated program reads.` |
|    - |   14 | ` * Status:` |
|    - |   15 | ` *    Stable.` |
|    - |   16 | ` *` |
|    - |   17 | ` * php's ext/gettext is a shell over GNU libintl, so its contract is really` |
|    - |   18 | ` * libintl's -- and that is what is reproduced here, over PHL's own catalog` |
|    - |   19 | ` * reader rather than the platform's, so a Windows build answers what a Linux` |
|    - |   20 | ` * one does. php's own layer is thin and contributes only the argument screens` |
|    - |   21 | ` * (§ "The php layer" below); everything else in this file is the library's` |
|    - |   22 | ` * behaviour, measured against glibc 2.4x through php 8.5.9.` |
|    - |   23 | ` *` |
|    - |   24 | ` * WHICH FILE a lookup reads is the whole of it, and it is four rules:` |
|    - |   25 | ` *` |
|    - |   26 | ` *   1. The CATEGORY VALUE. gettext asks the C library what locale the category` |
|    - |   27 | `` *      is set to (`setlocale(LC_MESSAGES, NULL)`), NOT what the environment`` |
|    - |   28 | ` *      says -- so a program that never calls setlocale() is in the "C" locale` |
|    - |   29 | ` *      and every lookup answers the msgid untranslated, whatever LANG holds.` |
|    - |   30 | ` *      When LANGUAGE is set AND the category is not the C locale, LANGUAGE` |
|    - |   31 | ` *      wins: it is a colon-separated PRIORITY LIST, each element tried whole` |
|    - |   32 | ` *      before the next, and an element that IS the C locale ENDS the search` |
|    - |   33 | ` *      untranslated. What counts as the C locale is wider than its name and is` |
|    - |   34 | ` *      spelled out at GtIsPosixLocale() below.` |
|    - |   35 | `` *   2. The NAME EXPANSION. One locale name is `language[_territory][.codeset]`` |
|    - |   36 | `` *      [@modifier]`, and each of the three optional parts is dropped in turn to`` |
|    - |   37 | ` *      make a candidate list, most specific first, with the modifier the most` |
|    - |   38 | `` *      significant part and the codeset the least: `pt_BR.UTF-8` tries`` |
|    - |   39 | `` *      pt_BR.UTF-8, pt_BR.utf8, pt_BR, pt.UTF-8, pt.utf8, pt. The `.utf8` forms`` |
|    - |   40 | ` *      are the NORMALISED codeset (lower-cased, non-alphanumerics dropped, and` |
|    - |   41 | `` *      prefixed `iso` when nothing but digits is left), and they only appear`` |
|    - |   42 | ` *      when normalising changed the spelling.` |
|    - |   43 | `` *   3. The PATH is `<bound directory>/<candidate>/<CATEGORY>/<domain>.mo`. The`` |
|    - |   44 | ` *      bound directory is what bindtextdomain() was given; a domain nobody bound` |
|    - |   45 | `` *      reads `/usr/share/locale`, which is the directory glibc is built with.`` |
|    - |   46 | ` *      What wins is NOT the first file that opens but the first that CARRIES` |
|    - |   47 | ` *      the msgid: every candidate is a link in a chain (glibc's` |
|    - |   48 | `` *      `domain->successor[]`, walked in DCIGETTEXT), so a `pt_BR` catalog`` |
|    - |   49 | `` *      overrides a `pt` one and falls through to it for everything it does not`` |
|    - |   50 | ` *      say -- taking that catalog's charset and plural rule with it.` |
|    - |   51 | `` *   4. The CATEGORY DIRECTORY is the category's own name -- `LC_MESSAGES` for`` |
|    - |   52 | ` *      gettext()/dgettext(), and whatever dcgettext() was handed for the rest.` |
|    - |   53 | ` *      LC_ALL is refused by php before it reaches here.` |
|    - |   54 | ` *` |
|    - |   55 | ` * The FILE is the GNU MO format: a magic word that also gives the byte order, a` |
|    - |   56 | ` * count, and two tables of {length, offset} pairs, one for the msgids and one` |
|    - |   57 | ` * for the translations. The msgid table is sorted for BINARY SEARCH, and the` |
|    - |   58 | ` * comparison is a C-string one -- which is what makes a plural entry work at` |
|    - |   59 | `` * all: it is stored as `singular\0plural` and compares equal to its own`` |
|    - |   60 | ` * singular, so ngettext() looks up the singular and finds the pair.` |
|    - |   61 | ` *` |
|    - |   62 | ` * Entry 0 is the HEADER, whose msgid is empty and whose translation is the` |
|    - |   63 | `` * RFC-822-shaped block a `.po` file opens with. Two of its fields are contract:`` |
|    - |   64 | `` * `Content-Type: text/plain; charset=X` names the encoding the file's bytes are`` |
|    - |   65 | `` * in, and `Plural-Forms: nplurals=N; plural=EXPR;` is the C expression that`` |
|    - |   66 | ` * turns a count into a form index. EXPR is evaluated per call by the little` |
|    - |   67 | ` * recursive-descent parser at the bottom of this file -- glibc's plural.y` |
|    - |   68 | ` * grammar, in unsigned long arithmetic, because that is what the rule` |
|    - |   69 | `` * `plural=(n > 1)` means for a NEGATIVE count: php hands the count down as a`` |
|    - |   70 | `` * zend_long, libintl takes an `unsigned long`, and -1 is therefore huge and`` |
|    - |   71 | `` * PLURAL. A catalog with no Plural-Forms is `nplurals=2; plural=(n != 1)`,`` |
|    - |   72 | ` * which is also what a MISSING msgid answers with -- an untranslated` |
|    - |   73 | ` * ngettext() is the Germanic rule whatever the catalog says.` |
|    - |   74 | ` *` |
|    - |   75 | ` * The RESULT ENCODING is the last rule. glibc converts every answer from the` |
|    - |   76 | ` * charset the header names to an output charset, with transliteration on: the` |
|    - |   77 | ` * one bind_textdomain_codeset() set for the domain, or -- when nothing set one` |
|    - |   78 | ` * -- the code set of the LC_CTYPE locale. Two cuts here, both §10's:` |
|    - |   79 | ` *` |
|    - |   80 | ` *   - PHL models UTF-8, ISO-8859-1 and US-ASCII (the iconv/mbstring scope cut).` |
|    - |   81 | ` *     A catalog or a bound codeset outside those three cannot be converted, and` |
|    - |   82 | ` *     php's own answer for a conversion it cannot open is the msgid` |
|    - |   83 | ` *     UNTRANSLATED, which is what this answers too.` |
|    - |   84 | ` *   - a header with no charset is not converted at all, which is glibc's rule` |
|    - |   85 | ` *     for the same case.` |
|    - |   86 | ` *` |
|    - |   87 | ` * Two RECORDED divergences, PLAN.md §7.4, both twin-less in the corpus because a` |
|    - |   88 | ` * php half of either would kill the runner or measure the cache. The first is` |
|    - |   89 | `` * arithmetic: glibc's `plural_eval` RAISES SIGFPE for a division or a modulo by`` |
|    - |   90 | `` * zero in a plural rule, deliberately (`if (rightarg == 0) raise (SIGFPE);`), so`` |
|    - |   91 | `` * php does not answer a catalog whose header says `plural=(n%0)` -- it dies with`` |
|    - |   92 | ` * a core dump on nothing worse than a typo in a translation file. This answers 0` |
|    - |   93 | ` * for that division, and the form index lands on 0.` |
|    - |   94 | ` *` |
|    - |   95 | ` * The second is memory: glibc remembers a translation it has` |
|    - |   96 | ` * already found, keyed by domain + msgid + category, and throws that memory away` |
|    - |   97 | `` * only when its `_nl_msg_cat_cntr` moves -- which setlocale(), textdomain() and a`` |
|    - |   98 | ` * CHANGED binding do and putenv() does not. So under php a bare` |
|    - |   99 | `` * `putenv("LANGUAGE=…")` between two lookups of ONE msgid is answered out of the`` |
|    - |  100 | ` * cache, while a msgid never asked for before sees the new list. PHL re-reads` |
|    - |  101 | ` * whenever the category value changes, so both see it. That cache is a` |
|    - |  102 | ` * performance artifact of one implementation with an undocumented invalidation` |
|    - |  103 | ` * set; what it hides is the rule this file is written to, and every program that` |
|    - |  104 | ` * changes LANGUAGE calls setlocale() beside it (isocodes, the library that put` |
|    - |  105 | ` * this extension on the roadmap, does exactly that), so both engines agree there.` |
|    - |  106 | ` *` |
|    - |  107 | ` * The php layer on top is five screens and one return rule:` |
|    - |  108 | ` *   - an EMPTY domain is a ValueError, at every door that takes one;` |
|    - |  109 | ` *   - a domain over 1024 bytes and a message over 4096 are "too long";` |
|    - |  110 | ` *   - bindtextdomain() reads BOTH its arguments as PATHS, so a NUL byte in` |
|    - |  111 | ` *     either is the catchable "must not contain any null bytes";` |
|    - |  112 | ` *   - dcgettext()/dcngettext() refuse LC_ALL by name, and pass any other` |
|    - |  113 | ` *     integer through (a category the library does not know simply finds no` |
|    - |  114 | ` *     directory of that name);` |
|    - |  115 | ` *   - bindtextdomain() makes its directory ABSOLUTE with realpath() and answers` |
|    - |  116 | ` *     FALSE when that fails, and reads an empty directory (or the string "0")` |
|    - |  117 | ` *     as the working directory.` |
|    - |  118 | ` * The return rule is that a MISS gives back the caller's own bytes rather than` |
|    - |  119 | `` * the C string libintl handed back, so `gettext("a\0b")` keeps its NUL.`` |
|    - |  120 | ` */` |
|    - |  121 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|    - |  122 |  |
|    - |  123 | `/* php's own LC_* numbering (constant.c), which is not the C library's. */` |
|    - |  124 | `#define GT_LC_CTYPE     0` |
|    - |  125 | `#define GT_LC_NUMERIC   1` |
|    - |  126 | `#define GT_LC_TIME      2` |
|    - |  127 | `#define GT_LC_COLLATE   3` |
|    - |  128 | `#define GT_LC_MONETARY  4` |
|    - |  129 | `#define GT_LC_MESSAGES  5` |
|    - |  130 | `#define GT_LC_ALL       6` |
|    - |  131 |  |
|    - |  132 | `/* php's caps, checked before anything looks at the value. */` |
|    - |  133 | `#define GT_MAX_DOMAIN  1024` |
|    - |  134 | `#define GT_MAX_MSGID   4096` |
|    - |  135 |  |
|    - |  136 | `/* The directory glibc is built with, and therefore what a domain nobody bound` |
|    - |  137 | ` * reads. php answers it verbatim from bindtextdomain($domain, null). */` |
|    - |  138 | `#define GT_DEFAULT_DIR "/usr/share/locale"` |
|    - |  139 |  |
|    - |  140 | `/*` |
|    - |  141 | ` * How many candidate catalogs one resolution keeps. glibc builds the same chain` |
|    - |  142 | ` * and walks it whole; the cap only bounds what a pathological LANGUAGE list can` |
|    - |  143 | ` * make this open, and a real one names one or two files.` |
|    - |  144 | ` */` |
|    - |  145 | `#define GT_MAX_CATS 64` |
|    - |  146 |  |
|    - |  147 | `/* How much of a locale name (and of a LANGUAGE list) is read. glibc's own` |
|    - |  148 | ` * buffers are of this order; a name that does not fit is answered as absent,` |
|    - |  149 | ` * which leaves the lookup on the category's locale rather than half a list. */` |
|    - |  150 | `#define GT_LOCALE_MAX 512` |
|    - |  151 |  |
|    - |  152 | `/* One loaded catalog: the file's bytes plus what its header said. */` |
|    - |  153 | `typedef struct gt_cat gt_cat;` |
|    - |  154 | `struct gt_cat {` |
|    - |  155 | `	gt_cat *pNext;       /* the NEXT candidate, less specific than this one */` |
|    - |  156 | `	SyBlob sData;        /* the whole .mo file */` |
|    - |  157 | `	int bLoaded;         /* a well-formed catalog is present */` |
|    - |  158 | `	int bSwap;           /* the file's byte order is not this machine's */` |
|    - |  159 | `	sxu32 nStr;          /* how many strings it holds */` |
|    - |  160 | `	sxu32 nOrig;         /* byte offset of the msgid table */` |
|    - |  161 | `	sxu32 nTrans;        /* byte offset of the translation table */` |
|    - |  162 | `	sxu32 nPlurals;      /* nplurals from the header (2 when it says nothing) */` |
|    - |  163 | ``	SyBlob sPlural;      /* the plural EXPRESSION text ("" -> `n != 1`) */`` |
|    - |  164 | `	SyBlob sCharset;     /* the header's charset ("" -> convert nothing) */` |
|    - |  165 | `};` |
|    - |  166 | `/* One domain's binding, and the catalog last resolved for it. */` |
|    - |  167 | `typedef struct gt_dom gt_dom;` |
|    - |  168 | `struct gt_dom {` |
|    - |  169 | `	gt_dom *pNext;` |
|    - |  170 | `	SyBlob sName;        /* the domain name, as bound */` |
|    - |  171 | `	SyBlob sDir;         /* bindtextdomain()'s directory ("" -> the default) */` |
|    - |  172 | `	SyBlob sCodeset;     /* bind_textdomain_codeset() ("" -> none bound) */` |
|    - |  173 | `	SyBlob sKey;         /* the search key pCats was resolved under: the category` |
|    - |  174 | `	                      * value and the category name joined, so a setlocale()` |
|    - |  175 | `	                      * or a putenv("LANGUAGE=…") between two calls re-reads` |
|    - |  176 | `	                      * the files instead of answering the old ones */` |
|    - |  177 | `	int bResolved;       /* sKey is meaningful */` |
|    - |  178 | `	/* EVERY candidate that exists, in the order the expansion names them --` |
|    - |  179 | `	 * not just the first. A lookup walks the whole chain and takes the first` |
|    - |  180 | `	 * catalog that CARRIES the msgid, which is how a region catalog overrides a` |
|    - |  181 | `	 * language one and falls through to it for everything it does not say` |
|    - |  182 | ``	 * (glibc's `domain->successor[]` walk in DCIGETTEXT). The charset and the`` |
|    - |  183 | `	 * plural rule then come from the catalog the msgid was found in. */` |
|    - |  184 | `	gt_cat *pCats;` |
|    - |  185 | `};` |
|    - |  186 | `/* The whole extension's per-VM state. */` |
|    - |  187 | `typedef struct gt_state gt_state;` |
|    - |  188 | `struct gt_state {` |
|    - |  189 | `	gt_dom *pList;` |
|    - |  190 | `	SyBlob sDomain;      /* textdomain() ("" -> "messages") */` |
|    - |  191 | `};` |
|    - |  192 |  |
|    - |  193 | `/* --- State ------------------------------------------------------------- */` |
|    - |  194 |  |
|  146 |  195 | `static gt_state * GtState(ph7_vm *pVm)` |
|    3 |  196 | `{` |
|  149 |  197 | `	gt_state *pState = (gt_state *)pVm->pGettext;` |
|  149 |  198 | `	if( pState == 0 ){` |
|    7 |  199 | `		pState = (gt_state *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_state));` |
|    7 |  200 | `		if( pState == 0 ){` |
|  ! 0 |  201 | `			return 0;` |
|    - |  202 | `		}` |
|    7 |  203 | `		pState->pList = 0;` |
|    7 |  204 | `		SyBlobInit(&pState->sDomain,&pVm->sAllocator);` |
|    7 |  205 | `		pVm->pGettext = pState;` |
|    1 |  206 | `	}` |
|  149 |  207 | `	return pState;` |
|   12 |  208 | `}` |
|    - |  209 | `/*` |
|    - |  210 | ` * The domain record for this name, created empty if this is the first mention.` |
|    - |  211 | ` * php has no way to FORGET a binding, so the list only ever grows, and a` |
|    - |  212 | ` * program binds a handful of domains.` |
|    - |  213 | ` */` |
|   93 |  214 | `static gt_dom * GtDomain(ph7_vm *pVm,const char *zName,int nName)` |
|    3 |  215 | `{` |
|   96 |  216 | `	gt_state *pState = GtState(pVm);` |
|    - |  217 | `	gt_dom *pDom;` |
|   96 |  218 | `	if( pState == 0 ){` |
|  ! 0 |  219 | `		return 0;` |
|    - |  220 | `	}` |
|  172 |  221 | `	for( pDom = pState->pList ; pDom ; pDom = pDom->pNext ){` |
|  157 |  222 | `		if( (int)SyBlobLength(&pDom->sName) == nName` |
|   94 |  223 | `		 && (nName == 0 \|\| SyMemcmp(SyBlobData(&pDom->sName),zName,(sxu32)nName) == 0) ){` |
|   84 |  224 | `			return pDom;` |
|    - |  225 | `		}` |
|    8 |  226 | `	}` |
|   15 |  227 | `	pDom = (gt_dom *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_dom));` |
|   15 |  228 | `	if( pDom == 0 ){` |
|  ! 0 |  229 | `		return 0;` |
|    - |  230 | `	}` |
|   15 |  231 | `	SyZero(pDom,sizeof(gt_dom));` |
|   15 |  232 | `	SyBlobInit(&pDom->sName,&pVm->sAllocator);` |
|   15 |  233 | `	SyBlobInit(&pDom->sDir,&pVm->sAllocator);` |
|   15 |  234 | `	SyBlobInit(&pDom->sCodeset,&pVm->sAllocator);` |
|   15 |  235 | `	SyBlobInit(&pDom->sKey,&pVm->sAllocator);` |
|   15 |  236 | `	SyBlobAppend(&pDom->sName,zName,(sxu32)nName);` |
|   15 |  237 | `	pDom->pNext = pState->pList;` |
|   15 |  238 | `	pState->pList = pDom;` |
|   15 |  239 | `	return pDom;` |
|   12 |  240 | `}` |
|    - |  241 |  |
|    - |  242 | `/* --- The MO file ------------------------------------------------------- */` |
|    - |  243 |  |
|    - |  244 | `#define GT_MO_MAGIC     0x950412deu` |
|    - |  245 | `#define GT_MO_MAGIC_SWP 0xde120495u` |
|    - |  246 |  |
|  668 |  247 | `static sxu32 GtWord(const unsigned char *z,int bSwap)` |
|    2 |  248 | `{` |
|  670 |  249 | `	sxu32 v = (sxu32)z[0] \| ((sxu32)z[1] << 8) \| ((sxu32)z[2] << 16) \| ((sxu32)z[3] << 24);` |
|  670 |  250 | `	if( bSwap ){` |
|   15 |  251 | `		v = ((v & 0x000000ffu) << 24) \| ((v & 0x0000ff00u) << 8)` |
|   14 |  252 | `		  \| ((v & 0x00ff0000u) >> 8)  \| ((v & 0xff000000u) >> 24);` |
|  ! 0 |  253 | `	}` |
|  670 |  254 | `	return v;` |
|    2 |  255 | `}` |
|    - |  256 | `/*` |
|    - |  257 | ` * One {length, offset} pair out of a table, screened against the file's size.` |
|    - |  258 | ` * A malformed catalog is answered as a MISSING one rather than trusted: the` |
|    - |  259 | ` * bytes come from a file the script named, and every offset in them is a read` |
|    - |  260 | ` * into this buffer.` |
|    - |  261 | ` */` |
|  271 |  262 | `static int GtEntry(gt_cat *pCat,sxu32 nTable,sxu32 i,const char **pzStr,sxu32 *pnStr)` |
|    2 |  263 | `{` |
|  273 |  264 | `	const unsigned char *zBase = (const unsigned char *)SyBlobData(&pCat->sData);` |
|  273 |  265 | `	sxu32 nData = SyBlobLength(&pCat->sData);` |
|  273 |  266 | `	sxu32 nOfft = nTable + i * 8, nLen, nStr;` |
|  273 |  267 | `	if( nOfft + 8 > nData \|\| nOfft < nTable ){` |
|  ! 0 |  268 | `		return -1;` |
|    - |  269 | `	}` |
|  273 |  270 | `	nLen = GtWord(&zBase[nOfft],pCat->bSwap);` |
|  273 |  271 | `	nStr = GtWord(&zBase[nOfft + 4],pCat->bSwap);` |
|    - |  272 | `	/* The format guarantees a NUL past every string, and the reader below leans` |
|    - |  273 | `	 * on it, so the byte at nStr+nLen has to be inside the file and zero. */` |
|  273 |  274 | `	if( nStr >= nData \|\| nLen >= nData - nStr \|\| zBase[nStr + nLen] != 0 ){` |
|  ! 0 |  275 | `		return -1;` |
|    - |  276 | `	}` |
|  273 |  277 | `	*pzStr = (const char *)&zBase[nStr];` |
|  273 |  278 | `	*pnStr = nLen;` |
|  273 |  279 | `	return PH7_OK;` |
|   31 |  280 | `}` |
|    - |  281 | `/*` |
|    - |  282 | ` * Find zNeedle in the header. glibc reaches for both of the fields it reads with` |
|    - |  283 | `` * a plain `strstr` over the WHOLE null entry -- not per line and not`` |
|    - |  284 | `` * case-insensitively -- so a `charset=` anywhere in it is the one that answers.`` |
|    - |  285 | ` * Returns the offset just past the needle, or -1.` |
|    - |  286 | ` */` |
|   75 |  287 | `static int GtHeaderFind(const char *zHdr,sxu32 nHdr,const char *zNeedle)` |
|    2 |  288 | `{` |
|   77 |  289 | `	sxu32 n = (sxu32)SyStrlen(zNeedle), i;` |
|   77 |  290 | `	if( n < 1 \|\| n > nHdr ){` |
|  ! 0 |  291 | `		return -1;` |
|    - |  292 | `	}` |
| 6815 |  293 | `	for( i = 0 ; i + n <= nHdr ; ++i ){` |
| 6815 |  294 | `		if( SyMemcmp(&zHdr[i],zNeedle,n) == 0 ){` |
|   77 |  295 | `			return (int)(i + n);` |
|    - |  296 | `		}` |
|  586 |  297 | `	}` |
|  ! 0 |  298 | `	return -1;` |
|   14 |  299 | `}` |
|    - |  300 | `/*` |
|    - |  301 | `` * `Content-Type: text/plain; charset=UTF-8` -> `UTF-8`: glibc takes what follows`` |
|    - |  302 | `` * `charset=` up to the first space, tab or newline (`strcspn (charsetstr,`` |
|    - |  303 | `` * " \t\n")`), and a header with no such parameter names no charset at all.`` |
|    - |  304 | ` */` |
|   25 |  305 | `static void GtParseCharset(const char *zHdr,sxu32 nHdr,SyBlob *pOut)` |
|    2 |  306 | `{` |
|   27 |  307 | `	int i = GtHeaderFind(zHdr,nHdr,"charset=");` |
|    - |  308 | `	sxu32 k;` |
|   27 |  309 | `	if( i < 0 ){` |
|  ! 0 |  310 | `		return;` |
|    - |  311 | `	}` |
|   27 |  312 | `	k = (sxu32)i;` |
|  152 |  313 | `	while( k < nHdr && zHdr[k] != ' ' && zHdr[k] != '\t' && zHdr[k] != '\n' ){` |
|  127 |  314 | `		k++;` |
|    2 |  315 | `	}` |
|   27 |  316 | `	SyBlobAppend(pOut,&zHdr[i],k - (sxu32)i);` |
|    6 |  317 | `}` |
|    - |  318 | `/*` |
|    - |  319 | `` * `Plural-Forms: nplurals=2; plural=(n != 1);` -> the count and the expression.`` |
|    - |  320 | ` * glibc wants BOTH fields and falls back to the Germanic rule when either is` |
|    - |  321 | `` * missing, skips whitespace after `nplurals=` and then requires a digit. The`` |
|    - |  322 | ` * expression text is kept and parsed per call, which is the same answer for a` |
|    - |  323 | `` * few dozen bytes of arithmetic; its own grammar stops at the `;`, and the cut`` |
|    - |  324 | ` * below only bounds what is stored.` |
|    - |  325 | ` */` |
|   25 |  326 | `static void GtParsePlural(const char *zHdr,sxu32 nHdr,gt_cat *pCat)` |
|    2 |  327 | `{` |
|   27 |  328 | `	int iN = GtHeaderFind(zHdr,nHdr,"nplurals=");` |
|   27 |  329 | `	int iP = GtHeaderFind(zHdr,nHdr,"plural=");` |
|    - |  330 | `	sxu32 j,k;` |
|   27 |  331 | `	sxi64 iVal = 0;` |
|   27 |  332 | `	pCat->nPlurals = 2;` |
|   27 |  333 | `	if( iN < 0 \|\| iP < 0 ){` |
|  ! 0 |  334 | `		return;` |
|    - |  335 | `	}` |
|   27 |  336 | `	j = (sxu32)iN;` |
|   31 |  337 | `	while( j < nHdr && (zHdr[j] == ' ' \|\| zHdr[j] == '\t') ){` |
|  ! 0 |  338 | `		j++;` |
|  ! 0 |  339 | `	}` |
|   27 |  340 | `	if( j >= nHdr \|\| zHdr[j] < '0' \|\| zHdr[j] > '9' ){` |
|  ! 0 |  341 | `		return;` |
|    - |  342 | `	}` |
|   52 |  343 | `	while( j < nHdr && zHdr[j] >= '0' && zHdr[j] <= '9' ){` |
|   27 |  344 | `		if( iVal < 1000000 ){` |
|   27 |  345 | `			iVal = iVal * 10 + (zHdr[j] - '0');` |
|    4 |  346 | `		}` |
|   27 |  347 | `		j++;` |
|    2 |  348 | `	}` |
|   27 |  349 | `	if( iVal < 1 ){` |
|  ! 0 |  350 | `		return;   /* nplurals=0 leaves no form to pick */` |
|    - |  351 | `	}` |
|   27 |  352 | `	pCat->nPlurals = (sxu32)iVal;` |
|   27 |  353 | `	k = (sxu32)iP;` |
| 1405 |  354 | `	while( k < nHdr && zHdr[k] != ';' && zHdr[k] != '\n' ){` |
| 1380 |  355 | `		k++;` |
|    2 |  356 | `	}` |
|   27 |  357 | `	SyBlobAppend(&pCat->sPlural,&zHdr[iP],k - (sxu32)iP);` |
|    6 |  358 | `}` |
|    - |  359 | `/* Take apart a catalog whose bytes have just been read. */` |
|   26 |  360 | `static int GtParseMo(gt_cat *pCat)` |
|    2 |  361 | `{` |
|   28 |  362 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&pCat->sData);` |
|   28 |  363 | `	sxu32 nData = SyBlobLength(&pCat->sData), nMagic, nRev;` |
|    - |  364 | `	const char *zHdr;` |
|    - |  365 | `	sxu32 nHdr;` |
|   28 |  366 | `	if( nData < 28 ){` |
|  ! 0 |  367 | `		return -1;` |
|    - |  368 | `	}` |
|   28 |  369 | `	nMagic = GtWord(z,0);` |
|   28 |  370 | `	if( nMagic == GT_MO_MAGIC ){` |
|   26 |  371 | `		pCat->bSwap = 0;` |
|    7 |  372 | `	}else if( nMagic == GT_MO_MAGIC_SWP ){` |
|    2 |  373 | `		pCat->bSwap = 1;` |
|    1 |  374 | `	}else{` |
|    2 |  375 | `		return -1;` |
|    - |  376 | `	}` |
|    - |  377 | `	/* Only major revision 0 is read here. Revision 1 adds the "system dependent` |
|    - |  378 | `	 * segments" nothing in the wild uses, and glibc refuses an unknown major the` |
|    - |  379 | `	 * same way -- as a catalog that is not there. */` |
|   27 |  380 | `	nRev = GtWord(&z[4],pCat->bSwap);` |
|   27 |  381 | `	if( (nRev >> 16) != 0 ){` |
|  ! 0 |  382 | `		return -1;` |
|    - |  383 | `	}` |
|   27 |  384 | `	pCat->nStr   = GtWord(&z[8],pCat->bSwap);` |
|   27 |  385 | `	pCat->nOrig  = GtWord(&z[12],pCat->bSwap);` |
|   27 |  386 | `	pCat->nTrans = GtWord(&z[16],pCat->bSwap);` |
|   25 |  387 | `	if( pCat->nStr == 0 \|\| pCat->nStr > nData / 8` |
|   25 |  388 | `	 \|\| pCat->nOrig > nData \|\| pCat->nTrans > nData` |
|   25 |  389 | `	 \|\| pCat->nOrig + pCat->nStr * 8 > nData` |
|   27 |  390 | `	 \|\| pCat->nTrans + pCat->nStr * 8 > nData ){` |
|  ! 0 |  391 | `		return -1;` |
|    - |  392 | `	}` |
|   27 |  393 | `	pCat->bLoaded = 1;` |
|    - |  394 | `	/* Entry 0 is the header when its msgid is empty. */` |
|   27 |  395 | `	pCat->nPlurals = 2;` |
|   25 |  396 | `	if( GtEntry(pCat,pCat->nOrig,0,&zHdr,&nHdr) == PH7_OK && nHdr == 0` |
|   27 |  397 | `	 && GtEntry(pCat,pCat->nTrans,0,&zHdr,&nHdr) == PH7_OK ){` |
|   27 |  398 | `		GtParseCharset(zHdr,nHdr,&pCat->sCharset);` |
|   27 |  399 | `		GtParsePlural(zHdr,nHdr,pCat);` |
|    4 |  400 | `	}` |
|   27 |  401 | `	return PH7_OK;` |
|    6 |  402 | `}` |
|    - |  403 | `/*` |
|    - |  404 | ` * Binary search of the msgid table. The comparison is a C-string one over` |
|    - |  405 | ` * bytes the format guarantees NUL-terminated, which is what lets a plural` |
|    - |  406 | `` * entry (`singular\0plural`) be found by its singular alone.`` |
|    - |  407 | ` */` |
|  173 |  408 | `static int GtStrcmp(const char *zLeft,const char *zRight)` |
|    2 |  409 | `{` |
|  175 |  410 | `	const unsigned char *a = (const unsigned char *)zLeft;` |
|  175 |  411 | `	const unsigned char *b = (const unsigned char *)zRight;` |
|  306 |  412 | `	while( *a && *a == *b ){` |
|  133 |  413 | `		a++; b++;` |
|    2 |  414 | `	}` |
|  175 |  415 | `	return (int)*a - (int)*b;` |
|    2 |  416 | `}` |
|   63 |  417 | `static int GtFindMsg(gt_cat *pCat,const char *zMsg,const char **pzOut,sxu32 *pnOut)` |
|    2 |  418 | `{` |
|   65 |  419 | `	sxu32 iLow = 0, iHigh;` |
|   65 |  420 | `	if( !pCat->bLoaded \|\| pCat->nStr == 0 ){` |
|  ! 0 |  421 | `		return -1;` |
|    - |  422 | `	}` |
|   65 |  423 | `	iHigh = pCat->nStr - 1;` |
|  117 |  424 | `	for(;;){` |
|  175 |  425 | `		sxu32 iMid = iLow + (iHigh - iLow) / 2;` |
|    - |  426 | `		const char *zCand;` |
|    - |  427 | `		sxu32 nCand;` |
|    - |  428 | `		int c;` |
|  175 |  429 | `		if( GtEntry(pCat,pCat->nOrig,iMid,&zCand,&nCand) != PH7_OK ){` |
|  ! 0 |  430 | `			return -1;` |
|    - |  431 | `		}` |
|  175 |  432 | `		c = GtStrcmp(zMsg,zCand);` |
|  175 |  433 | `		if( c == 0 ){` |
|   50 |  434 | `			return GtEntry(pCat,pCat->nTrans,iMid,pzOut,pnOut);` |
|    - |  435 | `		}` |
|  127 |  436 | `		if( c < 0 ){` |
|   40 |  437 | `			if( iMid == iLow ){` |
|    5 |  438 | `				return -1;` |
|    - |  439 | `			}` |
|   36 |  440 | `			iHigh = iMid - 1;` |
|    1 |  441 | `		}else{` |
|   88 |  442 | `			if( iMid == iHigh ){` |
|   12 |  443 | `				return -1;` |
|    - |  444 | `			}` |
|   77 |  445 | `			iLow = iMid + 1;` |
|    - |  446 | `		}` |
|    2 |  447 | `	}` |
|    9 |  448 | `}` |
|    - |  449 |  |
|    - |  450 | `/* --- The plural expression --------------------------------------------- */` |
|    - |  451 |  |
|    - |  452 | `/*` |
|    - |  453 | `` * glibc's plural.y, as a recursive-descent parser over `unsigned long`. The`` |
|    - |  454 | `` * grammar is C's conditional expression with `n` as its only variable, and the`` |
|    - |  455 | ` * arithmetic has to be UNSIGNED because that is the type libintl evaluates it` |
|    - |  456 | `` * in -- `plural=(n > 1)` with a count of -1 is the PLURAL form, not the`` |
|    - |  457 | ` * singular. Division and modulo by zero answer 0 rather than trapping -- glibc` |
|    - |  458 | ` * raises SIGFPE there on purpose, which is the §7.4 record above: a catalog is` |
|    - |  459 | ` * DATA, and a typo in one must not take the process down.` |
|    - |  460 | ` */` |
|    - |  461 | `typedef struct gt_plural gt_plural;` |
|    - |  462 | `struct gt_plural {` |
|    - |  463 | `	const char *z;` |
|    - |  464 | `	const char *zEnd;` |
|    - |  465 | `	sxu64 n;` |
|    - |  466 | `	int bBad;` |
|    - |  467 | `};` |
|    - |  468 | `static sxu64 GtPlCond(gt_plural *p);` |
| 5041 |  469 | `static void GtPlSpace(gt_plural *p)` |
|    2 |  470 | `{` |
| 5391 |  471 | `	while( p->z < p->zEnd && (p->z[0] == ' ' \|\| p->z[0] == '\t'` |
| 4581 |  472 | `	    \|\| p->z[0] == '\r' \|\| p->z[0] == '\n') ){` |
|  241 |  473 | `		p->z++;` |
|    1 |  474 | `	}` |
| 5043 |  475 | `}` |
| 3089 |  476 | `static int GtPlEat(gt_plural *p,const char *zTok)` |
|    2 |  477 | `{` |
| 3091 |  478 | `	sxu32 n = (sxu32)SyStrlen(zTok);` |
| 3091 |  479 | `	GtPlSpace(p);` |
| 3091 |  480 | `	if( (sxu32)(p->zEnd - p->z) >= n && SyMemcmp(p->z,zTok,n) == 0 ){` |
|  333 |  481 | `		p->z += n;` |
|  333 |  482 | `		return 1;` |
|    - |  483 | `	}` |
| 2760 |  484 | `	return 0;` |
|  114 |  485 | `}` |
|  369 |  486 | `static sxu64 GtPlPrimary(gt_plural *p)` |
|    2 |  487 | `{` |
|  371 |  488 | `	GtPlSpace(p);` |
|  371 |  489 | `	if( p->z >= p->zEnd ){` |
|  ! 0 |  490 | `		p->bBad = 1;` |
|  ! 0 |  491 | `		return 0;` |
|    - |  492 | `	}` |
|  371 |  493 | `	if( p->z[0] == '!' ){` |
|  ! 0 |  494 | `		p->z++;` |
|  ! 0 |  495 | `		return GtPlPrimary(p) == 0 ? 1 : 0;` |
|    - |  496 | `	}` |
|  371 |  497 | `	if( p->z[0] == '(' ){` |
|    - |  498 | `		sxu64 v;` |
|   40 |  499 | `		p->z++;` |
|   40 |  500 | `		v = GtPlCond(p);` |
|   40 |  501 | `		if( !GtPlEat(p,")") ){` |
|  ! 0 |  502 | `			p->bBad = 1;` |
|  ! 0 |  503 | `		}` |
|   40 |  504 | `		return v;` |
|    - |  505 | `	}` |
|  333 |  506 | `	if( p->z[0] == 'n' ){` |
|  100 |  507 | `		p->z++;` |
|  100 |  508 | `		return p->n;` |
|    - |  509 | `	}` |
|  235 |  510 | `	if( p->z[0] >= '0' && p->z[0] <= '9' ){` |
|  235 |  511 | `		sxu64 v = 0;` |
|  648 |  512 | `		while( p->z < p->zEnd && p->z[0] >= '0' && p->z[0] <= '9' ){` |
|  415 |  513 | `			v = v * 10 + (sxu64)(p->z[0] - '0');` |
|  415 |  514 | `			p->z++;` |
|    2 |  515 | `		}` |
|  235 |  516 | `		return v;` |
|    - |  517 | `	}` |
|  ! 0 |  518 | `	p->bBad = 1;` |
|  ! 0 |  519 | `	return 0;` |
|   14 |  520 | `}` |
|  271 |  521 | `static sxu64 GtPlMul(gt_plural *p)` |
|    2 |  522 | `{` |
|  273 |  523 | `	sxu64 v = GtPlPrimary(p);` |
|   12 |  524 | `	for(;;){` |
|  371 |  525 | `		GtPlSpace(p);` |
|  371 |  526 | `		if( GtPlEat(p,"*") ){` |
|  ! 0 |  527 | `			v = v * GtPlPrimary(p);` |
|  371 |  528 | `		}else if( GtPlEat(p,"/") ){` |
|  ! 0 |  529 | `			sxu64 r = GtPlPrimary(p);` |
|  ! 0 |  530 | `			v = r ? v / r : 0;` |
|  371 |  531 | `		}else if( GtPlEat(p,"%") ){` |
|  100 |  532 | `			sxu64 r = GtPlPrimary(p);` |
|  100 |  533 | `			v = r ? v % r : 0;` |
|    6 |  534 | `		}else{` |
|  273 |  535 | `			return v;` |
|    - |  536 | `		}` |
|    2 |  537 | `	}` |
|    2 |  538 | `}` |
|  271 |  539 | `static sxu64 GtPlAdd(gt_plural *p)` |
|    2 |  540 | `{` |
|  273 |  541 | `	sxu64 v = GtPlMul(p);` |
|    8 |  542 | `	for(;;){` |
|  273 |  543 | `		GtPlSpace(p);` |
|  273 |  544 | `		if( GtPlEat(p,"+") ){` |
|  ! 0 |  545 | `			v = v + GtPlMul(p);` |
|  273 |  546 | `		}else if( GtPlEat(p,"-") ){` |
|  ! 0 |  547 | `			v = v - GtPlMul(p);` |
|  ! 0 |  548 | `		}else{` |
|  273 |  549 | `			return v;` |
|    - |  550 | `		}` |
|  ! 0 |  551 | `	}` |
|    2 |  552 | `}` |
|  211 |  553 | `static sxu64 GtPlRel(gt_plural *p)` |
|    2 |  554 | `{` |
|  213 |  555 | `	sxu64 v = GtPlAdd(p);` |
|    8 |  556 | `	for(;;){` |
|  273 |  557 | `		GtPlSpace(p);` |
|  273 |  558 | `		if( GtPlEat(p,"<=") ){` |
|   16 |  559 | `			v = v <= GtPlAdd(p) ? 1 : 0;` |
|  258 |  560 | `		}else if( GtPlEat(p,">=") ){` |
|   31 |  561 | `			v = v >= GtPlAdd(p) ? 1 : 0;` |
|  228 |  562 | `		}else if( p->z < p->zEnd && p->z[0] == '<' ){` |
|   16 |  563 | `			p->z++;` |
|   16 |  564 | `			v = v < GtPlAdd(p) ? 1 : 0;` |
|  213 |  565 | `		}else if( p->z < p->zEnd && p->z[0] == '>' ){` |
|  ! 0 |  566 | `			p->z++;` |
|  ! 0 |  567 | `			v = v > GtPlAdd(p) ? 1 : 0;` |
|  ! 0 |  568 | `		}else{` |
|  213 |  569 | `			return v;` |
|    - |  570 | `		}` |
|    1 |  571 | `	}` |
|    2 |  572 | `}` |
|  181 |  573 | `static sxu64 GtPlEq(gt_plural *p)` |
|    2 |  574 | `{` |
|  183 |  575 | `	sxu64 v = GtPlRel(p);` |
|    8 |  576 | `	for(;;){` |
|  213 |  577 | `		GtPlSpace(p);` |
|  213 |  578 | `		if( GtPlEat(p,"==") ){` |
|   16 |  579 | `			v = v == GtPlRel(p) ? 1 : 0;` |
|  198 |  580 | `		}else if( GtPlEat(p,"!=") ){` |
|   16 |  581 | `			v = v != GtPlRel(p) ? 1 : 0;` |
|    1 |  582 | `		}else{` |
|  183 |  583 | `			return v;` |
|    - |  584 | `		}` |
|    1 |  585 | `	}` |
|    2 |  586 | `}` |
|  136 |  587 | `static sxu64 GtPlAnd(gt_plural *p)` |
|    2 |  588 | `{` |
|  138 |  589 | `	sxu64 v = GtPlEq(p);` |
|    8 |  590 | `	for(;;){` |
|  183 |  591 | `		GtPlSpace(p);` |
|  183 |  592 | `		if( GtPlEat(p,"&&") ){` |
|   46 |  593 | `			sxu64 r = GtPlEq(p);` |
|   46 |  594 | `			v = (v && r) ? 1 : 0;` |
|    1 |  595 | `		}else{` |
|  138 |  596 | `			return v;` |
|    - |  597 | `		}` |
|    1 |  598 | `	}` |
|    2 |  599 | `}` |
|  121 |  600 | `static sxu64 GtPlOr(gt_plural *p)` |
|    2 |  601 | `{` |
|  123 |  602 | `	sxu64 v = GtPlAnd(p);` |
|    8 |  603 | `	for(;;){` |
|  138 |  604 | `		GtPlSpace(p);` |
|  138 |  605 | `		if( GtPlEat(p,"\|\|") ){` |
|   16 |  606 | `			sxu64 r = GtPlAnd(p);` |
|   16 |  607 | `			v = (v \|\| r) ? 1 : 0;` |
|    1 |  608 | `		}else{` |
|  123 |  609 | `			return v;` |
|    - |  610 | `		}` |
|    1 |  611 | `	}` |
|    2 |  612 | `}` |
|  121 |  613 | `static sxu64 GtPlCond(gt_plural *p)` |
|    2 |  614 | `{` |
|  123 |  615 | `	sxu64 v = GtPlOr(p);` |
|  123 |  616 | `	GtPlSpace(p);` |
|  123 |  617 | `	if( GtPlEat(p,"?") ){` |
|   31 |  618 | `		sxu64 a = GtPlCond(p);` |
|    - |  619 | `		sxu64 b;` |
|   31 |  620 | `		if( !GtPlEat(p,":") ){` |
|  ! 0 |  621 | `			p->bBad = 1;` |
|  ! 0 |  622 | `			return 0;` |
|    - |  623 | `		}` |
|   31 |  624 | `		b = GtPlCond(p);` |
|   31 |  625 | `		return v ? a : b;` |
|    - |  626 | `	}` |
|   93 |  627 | `	return v;` |
|   10 |  628 | `}` |
|    - |  629 | `/* The form index this catalog gives a count, or the Germanic rule with no` |
|    - |  630 | ` * catalog (and with an expression that does not parse). */` |
|   23 |  631 | `static sxu32 GtPluralIndex(gt_cat *pCat,sxu64 n)` |
|    2 |  632 | `{` |
|    - |  633 | `	gt_plural sP;` |
|    - |  634 | `	sxu64 v;` |
|   25 |  635 | `	if( pCat == 0 \|\| SyBlobLength(&pCat->sPlural) < 1 ){` |
|  ! 0 |  636 | `		return n == 1 ? 0 : 1;` |
|    - |  637 | `	}` |
|   25 |  638 | `	sP.z = (const char *)SyBlobData(&pCat->sPlural);` |
|   25 |  639 | `	sP.zEnd = &sP.z[SyBlobLength(&pCat->sPlural)];` |
|   25 |  640 | `	sP.n = n;` |
|   25 |  641 | `	sP.bBad = 0;` |
|   25 |  642 | `	v = GtPlCond(&sP);` |
|   25 |  643 | `	GtPlSpace(&sP);` |
|   25 |  644 | `	if( sP.bBad \|\| sP.z != sP.zEnd ){` |
|    - |  645 | `		/* glibc runs a bison parser over the whole field, so text it cannot` |
|    - |  646 | `		 * reduce -- including anything LEFT OVER after a complete expression --` |
|    - |  647 | `		 * is a catalog with no plural rule, and the Germanic one answers. */` |
|  ! 0 |  648 | `		return n == 1 ? 0 : 1;` |
|    - |  649 | `	}` |
|   25 |  650 | `	return v > 0xffffffffu ? 0 : (sxu32)v;` |
|    6 |  651 | `}` |
|    - |  652 |  |
|    - |  653 | `/* --- Locale resolution -------------------------------------------------- */` |
|    - |  654 |  |
|    - |  655 | `/*` |
|    - |  656 | ` * One environment variable, read the way php's own putenv() WROTE it. On` |
|    - |  657 | ` * Windows that is SetEnvironmentVariable(), which the CRT's getenv() copy does` |
|    - |  658 | ` * not see, so the process block has to be asked directly -- otherwise a script` |
|    - |  659 | ` * that does putenv("LANGUAGE=pt_BR") would be answered the language it started` |
|    - |  660 | ` * with. Answers the length written, 0 for absent or empty.` |
|    - |  661 | ` */` |
|   56 |  662 | `static int GtGetenv(const char *zName,char *zBuf,int nBuf)` |
|    3 |  663 | `{` |
|    - |  664 | `#ifdef __WINNT__` |
|    3 |  665 | `	DWORD n = GetEnvironmentVariableA(zName,zBuf,(DWORD)nBuf);` |
|    3 |  666 | `	if( n == 0 \|\| n >= (DWORD)nBuf ){` |
|    3 |  667 | `		zBuf[0] = 0;` |
|    3 |  668 | `		return 0;` |
|    - |  669 | `	}` |
|    2 |  670 | `	return (int)n;` |
|    - |  671 | `#else` |
|   56 |  672 | `	const char *z = getenv(zName);` |
|    - |  673 | `	int n;` |
|   56 |  674 | `	if( z == 0 \|\| z[0] == 0 ){` |
|  ! 0 |  675 | `		zBuf[0] = 0;` |
|  ! 0 |  676 | `		return 0;` |
|    - |  677 | `	}` |
|   56 |  678 | `	n = (int)SyStrlen(z);` |
|   56 |  679 | `	if( n >= nBuf ){` |
|  ! 0 |  680 | `		zBuf[0] = 0;` |
|  ! 0 |  681 | `		return 0;` |
|    - |  682 | `	}` |
|   56 |  683 | `	SyMemcpy(z,zBuf,(sxu32)n);` |
|   56 |  684 | `	zBuf[n] = 0;` |
|   56 |  685 | `	return n;` |
|    - |  686 | `#endif` |
|   10 |  687 | `}` |
|   73 |  688 | `static const char * GtCategoryName(int iCat)` |
|    3 |  689 | `{` |
|   76 |  690 | `	switch( iCat ){` |
|    2 |  691 | `		case GT_LC_CTYPE:    return "LC_CTYPE";` |
|  ! 0 |  692 | `		case GT_LC_NUMERIC:  return "LC_NUMERIC";` |
|  ! 0 |  693 | `		case GT_LC_TIME:     return "LC_TIME";` |
|  ! 0 |  694 | `		case GT_LC_COLLATE:  return "LC_COLLATE";` |
|    2 |  695 | `		case GT_LC_MONETARY: return "LC_MONETARY";` |
|   73 |  696 | `		case GT_LC_MESSAGES: return "LC_MESSAGES";` |
|    2 |  697 | `		default:             return 0;` |
|    - |  698 | `	}` |
|   10 |  699 | `}` |
|    - |  700 | `/*` |
|    - |  701 | ` * What the C library says this category is set to. Windows has no LC_MESSAGES` |
|    - |  702 | ` * at all, so the ENVIRONMENT answers for it there -- which is the same lookup` |
|    - |  703 | ` * (LC_ALL, then the category, then LANG) libintl's own POSIX path makes.` |
|    - |  704 | ` */` |
|  109 |  705 | `static void GtLocaleName(int iCat,char *zBuf,int nBuf)` |
|    3 |  706 | `{` |
|  112 |  707 | `	const char *z = 0;` |
|    - |  708 | `	int n;` |
|  112 |  709 | `	switch( iCat ){` |
|   41 |  710 | `		case GT_LC_CTYPE:    z = setlocale(LC_CTYPE,0);    break;` |
|  ! 0 |  711 | `		case GT_LC_NUMERIC:  z = setlocale(LC_NUMERIC,0);  break;` |
|  ! 0 |  712 | `		case GT_LC_TIME:     z = setlocale(LC_TIME,0);     break;` |
|  ! 0 |  713 | `		case GT_LC_COLLATE:  z = setlocale(LC_COLLATE,0);  break;` |
|    2 |  714 | `		case GT_LC_MONETARY: z = setlocale(LC_MONETARY,0); break;` |
|    - |  715 | `#ifdef LC_MESSAGES` |
|   70 |  716 | `		case GT_LC_MESSAGES: z = setlocale(LC_MESSAGES,0); break;` |
|    - |  717 | `#else` |
|    - |  718 | `		/* Windows has no LC_MESSAGES category at all, so libintl's own POSIX` |
|    - |  719 | `		 * path answers for it there: LC_ALL, then the category, then LANG. */` |
|    - |  720 | `		case GT_LC_MESSAGES:` |
|    - |  721 | `			if( GtGetenv("LC_ALL",zBuf,nBuf) > 0` |
|    - |  722 | `			 \|\| GtGetenv("LC_MESSAGES",zBuf,nBuf) > 0` |
|    3 |  723 | `			 \|\| GtGetenv("LANG",zBuf,nBuf) > 0 ){` |
|    2 |  724 | `				return;` |
|    - |  725 | `			}` |
|    1 |  726 | `			z = 0;` |
|    - |  727 | `			break;` |
|    - |  728 | `#endif` |
|  ! 0 |  729 | `		default: break;` |
|    - |  730 | `	}` |
|  112 |  731 | `	if( z == 0 \|\| z[0] == 0 ){` |
|    1 |  732 | `		z = "C";` |
|  ! 0 |  733 | `	}` |
|  112 |  734 | `	n = (int)SyStrlen(z);` |
|  112 |  735 | `	if( n >= nBuf ){` |
|  ! 0 |  736 | `		n = nBuf - 1;` |
|  ! 0 |  737 | `	}` |
|  112 |  738 | `	SyMemcpy(z,zBuf,(sxu32)n);` |
|  112 |  739 | `	zBuf[n] = 0;` |
|  112 |  740 | `}` |
|    - |  741 | `/*` |
|    - |  742 | ` * Is this the locale gettext reads NO catalog in? Measured rather than read,` |
|    - |  743 | ` * because the rule is wider than the "C" the documentation names: a locale` |
|    - |  744 | `` * whose language part is `C` and whose next character is a `.` counts too, so`` |
|    - |  745 | `` * `C.UTF-8`, `C.utf8` and even `C.anything` are all the C locale here --`` |
|    - |  746 | `` * which matters because `C.UTF-8` is what a modern container is usually in.`` |
|    - |  747 | `` * `POSIX` counts by name; `POSIX.UTF-8`, `Cx`, `C_XX`, `C@mod` and a lower-case`` |
|    - |  748 | `` * `c` do not. The test applies both to the category's own locale (which is what`` |
|    - |  749 | ` * decides whether LANGUAGE is consulted at all) and to each element of the` |
|    - |  750 | ` * LANGUAGE list (where it ENDS the search rather than skipping one element).` |
|    - |  751 | ` */` |
|  122 |  752 | `static int GtIsPosixLocale(const char *z,int n)` |
|    3 |  753 | `{` |
|  125 |  754 | `	if( n >= 1 && z[0] == 'C' && (n == 1 \|\| z[1] == '.') ){` |
|   25 |  755 | `		return 1;` |
|    - |  756 | `	}` |
|  101 |  757 | `	return n == 5 && SyMemcmp(z,"POSIX",5) == 0;` |
|   18 |  758 | `}` |
|    - |  759 | `/*` |
|    - |  760 | ` * The list gettext actually walks: LANGUAGE when it is set and the category is` |
|    - |  761 | ` * not "C", the category's own locale otherwise.` |
|    - |  762 | ` */` |
|   72 |  763 | `static void GtCategoryValue(int iCat,char *zBuf,int nBuf)` |
|    3 |  764 | `{` |
|    - |  765 | `	char zLoc[GT_LOCALE_MAX];` |
|   75 |  766 | `	GtLocaleName(iCat,zLoc,(int)sizeof(zLoc));` |
|   75 |  767 | `	if( !GtIsPosixLocale(zLoc,(int)SyStrlen(zLoc)) && GtGetenv("LANGUAGE",zBuf,nBuf) > 0 ){` |
|   58 |  768 | `		return;` |
|    - |  769 | `	}` |
|   18 |  770 | `	Systrcpy(zBuf,(sxu32)nBuf,zLoc,0);` |
|   10 |  771 | `}` |
|    - |  772 | `/*` |
|    - |  773 | ` * glibc's _nl_normalize_codeset: keep the alphanumerics, lower-case them, and` |
|    - |  774 | `` * prefix `iso` when nothing but digits survives. Answers 0 when the result is`` |
|    - |  775 | `` * the same as the input, which is what decides whether a `.normalised` form`` |
|    - |  776 | ` * appears in the candidate list at all.` |
|    - |  777 | ` */` |
|    5 |  778 | `static int GtNormCodeset(const char *z,int n,char *zOut,int nOut)` |
|    1 |  779 | `{` |
|    6 |  780 | `	int i,k = 0,bDigit = 1;` |
|    6 |  781 | `	if( n + 3 > nOut ){` |
|  ! 0 |  782 | `		return 0;   /* no room for the normalised spelling: do not offer one */` |
|    - |  783 | `	}` |
|   36 |  784 | `	for( i = 0 ; i < n ; ++i ){` |
|   31 |  785 | `		int c = (unsigned char)z[i];` |
|   31 |  786 | `		if( c >= '0' && c <= '9' ){` |
|   10 |  787 | `			if( k < nOut ){ zOut[k++] = (char)c; }` |
|   22 |  788 | `		}else if( c >= 'A' && c <= 'Z' ){` |
|   16 |  789 | `			bDigit = 0;` |
|   16 |  790 | `			if( k < nOut ){ zOut[k++] = (char)(c - 'A' + 'a'); }` |
|    7 |  791 | `		}else if( c >= 'a' && c <= 'z' ){` |
|  ! 0 |  792 | `			bDigit = 0;` |
|  ! 0 |  793 | `			if( k < nOut ){ zOut[k++] = (char)c; }` |
|  ! 0 |  794 | `		}` |
|    1 |  795 | `	}` |
|    6 |  796 | `	if( bDigit && k > 0 && k + 3 <= nOut ){` |
|    - |  797 | `		int j;` |
|  ! 0 |  798 | `		for( j = k - 1 ; j >= 0 ; --j ){` |
|  ! 0 |  799 | `			zOut[j + 3] = zOut[j];` |
|  ! 0 |  800 | `		}` |
|  ! 0 |  801 | `		zOut[0] = 'i'; zOut[1] = 's'; zOut[2] = 'o';` |
|  ! 0 |  802 | `		k += 3;` |
|  ! 0 |  803 | `	}` |
|    6 |  804 | `	if( k == n && k > 0 && SyMemcmp(zOut,z,(sxu32)k) == 0 ){` |
|  ! 0 |  805 | `		return 0;   /* normalising changed nothing */` |
|    - |  806 | `	}` |
|    6 |  807 | `	return k;` |
|    1 |  808 | `}` |
|    - |  809 | ``/* The four parts of `language[_territory][.codeset][@modifier]`. */`` |
|    - |  810 | `typedef struct gt_name gt_name;` |
|    - |  811 | `struct gt_name {` |
|    - |  812 | `	const char *zLang;  int nLang;` |
|    - |  813 | `	const char *zTerr;  int nTerr;` |
|    - |  814 | `	const char *zCode;  int nCode;` |
|    - |  815 | `	const char *zMod;   int nMod;` |
|    - |  816 | `	char zNorm[64];     int nNorm;` |
|    - |  817 | `};` |
|   22 |  818 | `static void GtExplode(const char *z,int n,gt_name *p)` |
|    2 |  819 | `{` |
|   24 |  820 | `	int i = 0;` |
|   24 |  821 | `	SyZero(p,sizeof(*p));` |
|   24 |  822 | `	p->zLang = z;` |
|   68 |  823 | `	while( i < n && z[i] != '_' && z[i] != '.' && z[i] != '@' ){` |
|   46 |  824 | `		i++;` |
|    2 |  825 | `	}` |
|   24 |  826 | `	p->nLang = i;` |
|   24 |  827 | `	if( i < n && z[i] == '_' ){` |
|   21 |  828 | `		int j = ++i;` |
|   59 |  829 | `		while( i < n && z[i] != '.' && z[i] != '@' ){` |
|   40 |  830 | `			i++;` |
|    2 |  831 | `		}` |
|   21 |  832 | `		p->zTerr = &z[j];` |
|   21 |  833 | `		p->nTerr = i - j;` |
|    4 |  834 | `	}` |
|   24 |  835 | `	if( i < n && z[i] == '.' ){` |
|    6 |  836 | `		int j = ++i;` |
|   36 |  837 | `		while( i < n && z[i] != '@' ){` |
|   31 |  838 | `			i++;` |
|    1 |  839 | `		}` |
|    6 |  840 | `		p->zCode = &z[j];` |
|    6 |  841 | `		p->nCode = i - j;` |
|    6 |  842 | `		p->nNorm = GtNormCodeset(p->zCode,p->nCode,p->zNorm,(int)sizeof(p->zNorm));` |
|  ! 0 |  843 | `	}` |
|   24 |  844 | `	if( i < n && z[i] == '@' ){` |
|    3 |  845 | `		p->zMod = &z[i + 1];` |
|    3 |  846 | `		p->nMod = n - i - 1;` |
|  ! 0 |  847 | `	}` |
|   24 |  848 | `}` |
|    - |  849 | `/*` |
|    - |  850 | ` * glibc's mask, and its bit VALUES -- the order the candidates come out in is` |
|    - |  851 | ` * exactly the descending order of these, so they are contract rather than` |
|    - |  852 | ` * taste: the modifier is the most significant part, the normalised codeset the` |
|    - |  853 | ` * least, and a candidate never carries both spellings of the codeset.` |
|    - |  854 | ` */` |
|    - |  855 | `#define GT_MASK_NORM   1` |
|    - |  856 | `#define GT_MASK_CODE   2` |
|    - |  857 | `#define GT_MASK_TERR   4` |
|    - |  858 | `#define GT_MASK_MOD    8` |
|    - |  859 |  |
|    - |  860 | `/* --- Reading a catalog -------------------------------------------------- */` |
|    - |  861 |  |
|    - |  862 | `#ifndef PH7_DISABLE_DISK_IO` |
|    - |  863 | `/*` |
|    - |  864 | ` * Read a whole file through the default (plain-file) device. Deliberately NOT` |
|    - |  865 | ` * PH7_VmGetStreamDevice(): libintl calls open(2), so a bound directory naming` |
|    - |  866 | `` * a userland wrapper is not one, and a `://` inside a locale directory is a`` |
|    - |  867 | ` * directory name here as it is there.` |
|    - |  868 | ` */` |
|   64 |  869 | `static int GtReadFile(ph7_vm *pVm,const char *zPath,SyBlob *pOut)` |
|    2 |  870 | `{` |
|   66 |  871 | `	const ph7_io_stream *pStream = pVm->pDefStream;` |
|    - |  872 | `	void *pHandle;` |
|   66 |  873 | `	if( pStream == 0 ){` |
|  ! 0 |  874 | `		return -1;` |
|    - |  875 | `	}` |
|   66 |  876 | `	pHandle = PH7_StreamOpenHandle(pVm,pStream,zPath,PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,"gettext");` |
|   66 |  877 | `	if( pHandle == 0 ){` |
|   40 |  878 | `		return -1;` |
|    - |  879 | `	}` |
|   28 |  880 | `	PH7_StreamReadWholeFile(pHandle,pStream,pOut);` |
|   28 |  881 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|   28 |  882 | `	return SyBlobLength(pOut) > 0 ? PH7_OK : -1;` |
|   10 |  883 | `}` |
|    - |  884 | `#else` |
|    - |  885 | `static int GtReadFile(ph7_vm *pVm,const char *zPath,SyBlob *pOut)` |
|    - |  886 | `{` |
|    - |  887 | `	SXUNUSED(pVm); SXUNUSED(zPath); SXUNUSED(pOut);` |
|    - |  888 | `	return -1;   /* a build with no disk I/O carries no catalogs */` |
|    - |  889 | `}` |
|    - |  890 | `#endif /* PH7_DISABLE_DISK_IO */` |
|    - |  891 |  |
|    - |  892 | `/* Drop every catalog a previous resolution loaded. */` |
|   27 |  893 | `static void GtDropCats(ph7_vm *pVm,gt_dom *pDom)` |
|    3 |  894 | `{` |
|   30 |  895 | `	gt_cat *pCat = pDom->pCats,*pNext;` |
|   49 |  896 | `	while( pCat ){` |
|   21 |  897 | `		pNext = pCat->pNext;` |
|   21 |  898 | `		SyBlobRelease(&pCat->sData);` |
|   21 |  899 | `		SyBlobRelease(&pCat->sPlural);` |
|   21 |  900 | `		SyBlobRelease(&pCat->sCharset);` |
|   21 |  901 | `		SyMemBackendFree(&pVm->sAllocator,pCat);` |
|   21 |  902 | `		pCat = pNext;` |
|    2 |  903 | `	}` |
|   30 |  904 | `	pDom->pCats = 0;` |
|   30 |  905 | `}` |
|    - |  906 | `/*` |
|    - |  907 | `` * Read `<dir>/<candidate>/<category>/<domain>.mo` and, if it is a catalog, add`` |
|    - |  908 | ` * it to the END of this domain's chain. Answers PH7_OK when one was added.` |
|    - |  909 | ` */` |
|   64 |  910 | `static int GtTryLoad(ph7_vm *pVm,gt_dom *pDom,const char *zDir,int nDir,` |
|    - |  911 | `	const char *zCand,int nCand,const char *zCat)` |
|    2 |  912 | `{` |
|    - |  913 | `	SyBlob sPath;` |
|    - |  914 | `	gt_cat *pCat,**ppTail;` |
|    - |  915 | `	int rc;` |
|   66 |  916 | `	pCat = (gt_cat *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(gt_cat));` |
|   66 |  917 | `	if( pCat == 0 ){` |
|  ! 0 |  918 | `		return -1;` |
|    - |  919 | `	}` |
|   66 |  920 | `	SyZero(pCat,sizeof(gt_cat));` |
|   66 |  921 | `	SyBlobInit(&pCat->sData,&pVm->sAllocator);` |
|   66 |  922 | `	SyBlobInit(&pCat->sPlural,&pVm->sAllocator);` |
|   66 |  923 | `	SyBlobInit(&pCat->sCharset,&pVm->sAllocator);` |
|   66 |  924 | `	SyBlobInit(&sPath,&pVm->sAllocator);` |
|   66 |  925 | `	SyBlobAppend(&sPath,zDir,(sxu32)nDir);` |
|   66 |  926 | `	SyBlobAppend(&sPath,"/",1);` |
|   66 |  927 | `	SyBlobAppend(&sPath,zCand,(sxu32)nCand);` |
|   66 |  928 | `	SyBlobAppend(&sPath,"/",1);` |
|   66 |  929 | `	SyBlobAppend(&sPath,zCat,(sxu32)SyStrlen(zCat));` |
|   66 |  930 | `	SyBlobAppend(&sPath,"/",1);` |
|   66 |  931 | `	SyBlobAppend(&sPath,SyBlobData(&pDom->sName),SyBlobLength(&pDom->sName));` |
|   66 |  932 | `	SyBlobAppend(&sPath,".mo",3);` |
|   66 |  933 | `	SyBlobNullAppend(&sPath);` |
|   66 |  934 | `	rc = GtReadFile(pVm,(const char *)SyBlobData(&sPath),&pCat->sData);` |
|   66 |  935 | `	SyBlobRelease(&sPath);` |
|   66 |  936 | `	if( rc == PH7_OK ){` |
|   28 |  937 | `		rc = GtParseMo(pCat);` |
|    4 |  938 | `	}` |
|   66 |  939 | `	if( rc != PH7_OK ){` |
|   41 |  940 | `		SyBlobRelease(&pCat->sData);` |
|   41 |  941 | `		SyBlobRelease(&pCat->sPlural);` |
|   41 |  942 | `		SyBlobRelease(&pCat->sCharset);` |
|   41 |  943 | `		SyMemBackendFree(&pVm->sAllocator,pCat);` |
|   41 |  944 | `		return -1;` |
|    - |  945 | `	}` |
|   37 |  946 | `	for( ppTail = &pDom->pCats ; *ppTail ; ppTail = &(*ppTail)->pNext ){}` |
|   27 |  947 | `	*ppTail = pCat;` |
|   27 |  948 | `	return PH7_OK;` |
|   10 |  949 | `}` |
|    - |  950 | `/*` |
|    - |  951 | ` * Walk one locale NAME's candidate list, most specific first, and add EVERY` |
|    - |  952 | ` * candidate that carries a catalog to the domain's chain. Answers how many.` |
|    - |  953 | ` */` |
|   22 |  954 | `static int GtLoadForName(ph7_vm *pVm,gt_dom *pDom,const char *zDir,int nDir,` |
|    - |  955 | `	const char *zName,int nName,const char *zCat)` |
|    2 |  956 | `{` |
|    - |  957 | `	gt_name sN;` |
|   24 |  958 | `	int mask = 0,cnt,nLoaded = 0;` |
|   24 |  959 | `	if( nName < 1 ){` |
|  ! 0 |  960 | `		return 0;` |
|    - |  961 | `	}` |
|   24 |  962 | `	GtExplode(zName,nName,&sN);` |
|   24 |  963 | `	if( sN.nLang < 1 ){` |
|  ! 0 |  964 | `		return 0;` |
|    - |  965 | `	}` |
|   24 |  966 | `	if( sN.nTerr > 0 ){ mask \|= GT_MASK_TERR; }` |
|   24 |  967 | `	if( sN.nCode > 0 ){ mask \|= GT_MASK_CODE; }` |
|   24 |  968 | `	if( sN.nNorm > 0 ){ mask \|= GT_MASK_NORM; }` |
|   24 |  969 | `	if( sN.nMod  > 0 ){ mask \|= GT_MASK_MOD;  }` |
|  153 |  970 | `	for( cnt = mask ; cnt >= 0 ; --cnt ){` |
|    - |  971 | `		char zBuf[GT_LOCALE_MAX];` |
|  131 |  972 | `		int k = sN.nLang;` |
|  131 |  973 | `		if( (cnt & ~mask) != 0 ){` |
|   67 |  974 | `			continue;` |
|    - |  975 | `		}` |
|   76 |  976 | `		if( (cnt & GT_MASK_CODE) && (cnt & GT_MASK_NORM) ){` |
|   11 |  977 | `			continue;   /* one spelling of the codeset at a time */` |
|    - |  978 | `		}` |
|    - |  979 | `		/* Measure first: a candidate that would not fit is SKIPPED rather than` |
|    - |  980 | `		 * truncated -- a shortened name is a different directory, and finding a` |
|    - |  981 | `		 * catalog under it would be a wrong answer rather than a missing one. */` |
|   66 |  982 | `		if( cnt & GT_MASK_TERR ){ k += 1 + sN.nTerr; }` |
|   66 |  983 | `		if( cnt & GT_MASK_CODE ){ k += 1 + sN.nCode; }` |
|   56 |  984 | `		else if( cnt & GT_MASK_NORM ){ k += 1 + sN.nNorm; }` |
|   66 |  985 | `		if( cnt & GT_MASK_MOD ){ k += 1 + sN.nMod; }` |
|   66 |  986 | `		if( k >= (int)sizeof(zBuf) ){` |
|  ! 0 |  987 | `			continue;` |
|    - |  988 | `		}` |
|   66 |  989 | `		SyMemcpy(sN.zLang,zBuf,(sxu32)sN.nLang);` |
|   66 |  990 | `		k = sN.nLang;` |
|   66 |  991 | `		if( cnt & GT_MASK_TERR ){` |
|   32 |  992 | `			zBuf[k++] = '_';` |
|   32 |  993 | `			SyMemcpy(sN.zTerr,&zBuf[k],(sxu32)sN.nTerr);` |
|   32 |  994 | `			k += sN.nTerr;` |
|    4 |  995 | `		}` |
|   66 |  996 | `		if( cnt & GT_MASK_CODE ){` |
|   11 |  997 | `			zBuf[k++] = '.';` |
|   11 |  998 | `			SyMemcpy(sN.zCode,&zBuf[k],(sxu32)sN.nCode);` |
|   11 |  999 | `			k += sN.nCode;` |
|   56 | 1000 | `		}else if( cnt & GT_MASK_NORM ){` |
|   11 | 1001 | `			zBuf[k++] = '.';` |
|   11 | 1002 | `			SyMemcpy(sN.zNorm,&zBuf[k],(sxu32)sN.nNorm);` |
|   11 | 1003 | `			k += sN.nNorm;` |
|  ! 0 | 1004 | `		}` |
|   66 | 1005 | `		if( cnt & GT_MASK_MOD ){` |
|    4 | 1006 | `			zBuf[k++] = '@';` |
|    4 | 1007 | `			SyMemcpy(sN.zMod,&zBuf[k],(sxu32)sN.nMod);` |
|    4 | 1008 | `			k += sN.nMod;` |
|  ! 0 | 1009 | `		}` |
|   66 | 1010 | `		if( GtTryLoad(pVm,pDom,zDir,nDir,zBuf,k,zCat) == PH7_OK ){` |
|   27 | 1011 | `			nLoaded++;` |
|   27 | 1012 | `			if( nLoaded >= GT_MAX_CATS ){` |
|  ! 0 | 1013 | `				break;` |
|    - | 1014 | `			}` |
|    4 | 1015 | `		}` |
|   10 | 1016 | `	}` |
|   24 | 1017 | `	return nLoaded;` |
|    6 | 1018 | `}` |
|    - | 1019 | `/*` |
|    - | 1020 | ` * Resolve (and cache) the catalog CHAIN this domain answers from right now,` |
|    - | 1021 | ` * head first. The cache key is the category value and the category name` |
|    - | 1022 | ` * together, so a setlocale() or a putenv("LANGUAGE=…") between two calls is` |
|    - | 1023 | ` * noticed.` |
|    - | 1024 | ` */` |
|   73 | 1025 | `static gt_cat * GtResolve(ph7_vm *pVm,gt_dom *pDom,int iCat)` |
|    3 | 1026 | `{` |
|   76 | 1027 | `	const char *zCat = GtCategoryName(iCat);` |
|    - | 1028 | `	const char *zDir;` |
|    - | 1029 | `	char zVal[GT_LOCALE_MAX];` |
|   76 | 1030 | `	int nVal,nDir,i,nLoaded = 0;` |
|    - | 1031 | `	SyBlob sKey;` |
|   76 | 1032 | `	if( pDom == 0 \|\| zCat == 0 ){` |
|    2 | 1033 | `		return 0;` |
|    - | 1034 | `	}` |
|   75 | 1035 | `	GtCategoryValue(iCat,zVal,(int)sizeof(zVal));` |
|   75 | 1036 | `	nVal = (int)SyStrlen(zVal);` |
|   75 | 1037 | `	SyBlobInit(&sKey,&pVm->sAllocator);` |
|   75 | 1038 | `	SyBlobAppend(&sKey,zCat,(sxu32)SyStrlen(zCat));` |
|   75 | 1039 | `	SyBlobAppend(&sKey,"\|",1);` |
|   75 | 1040 | `	SyBlobAppend(&sKey,zVal,(sxu32)nVal);` |
|   75 | 1041 | `	if( pDom->bResolved && SyBlobCmp(&sKey,&pDom->sKey) == 0 ){` |
|   48 | 1042 | `		SyBlobRelease(&sKey);` |
|   48 | 1043 | `		return pDom->pCats;` |
|    - | 1044 | `	}` |
|   30 | 1045 | `	SyBlobReset(&pDom->sKey);` |
|   30 | 1046 | `	SyBlobAppend(&pDom->sKey,SyBlobData(&sKey),SyBlobLength(&sKey));` |
|   30 | 1047 | `	SyBlobRelease(&sKey);` |
|   30 | 1048 | `	pDom->bResolved = 1;` |
|   30 | 1049 | `	GtDropCats(pVm,pDom);   /* a fresh resolution owns its own files */` |
|    - | 1050 | `	/* The "C" locale reads no catalog at all, and neither does a LANGUAGE` |
|    - | 1051 | `	 * whose first element says so. */` |
|   30 | 1052 | `	if( GtIsPosixLocale(zVal,nVal) ){` |
|    8 | 1053 | `		return 0;` |
|    - | 1054 | `	}` |
|   23 | 1055 | `	zDir = (const char *)SyBlobData(&pDom->sDir);` |
|   23 | 1056 | `	nDir = (int)SyBlobLength(&pDom->sDir);` |
|   23 | 1057 | `	if( nDir < 1 ){` |
|    2 | 1058 | `		zDir = GT_DEFAULT_DIR;` |
|    2 | 1059 | `		nDir = (int)SyStrlen(GT_DEFAULT_DIR);` |
|  ! 0 | 1060 | `	}` |
|    - | 1061 | `	/* LANGUAGE is a colon-separated PRIORITY LIST; an element spelled "C" or` |
|    - | 1062 | `	 * "POSIX" ends the search with nothing. */` |
|   23 | 1063 | `	i = 0;` |
|   45 | 1064 | `	while( i <= nVal ){` |
|   25 | 1065 | `		int j = i;` |
|  170 | 1066 | `		while( j < nVal && zVal[j] != ':' ){` |
|  147 | 1067 | `			j++;` |
|    2 | 1068 | `		}` |
|   25 | 1069 | `		if( j > i ){` |
|   25 | 1070 | `			if( GtIsPosixLocale(&zVal[i],j - i) ){` |
|    2 | 1071 | `				break;` |
|    - | 1072 | `			}` |
|   24 | 1073 | `			nLoaded += GtLoadForName(pVm,pDom,zDir,nDir,&zVal[i],j - i,zCat);` |
|   24 | 1074 | `			if( nLoaded >= GT_MAX_CATS ){` |
|  ! 0 | 1075 | `				break;` |
|    - | 1076 | `			}` |
|    4 | 1077 | `		}` |
|   24 | 1078 | `		i = j + 1;` |
|    2 | 1079 | `	}` |
|   23 | 1080 | `	return pDom->pCats;` |
|   10 | 1081 | `}` |
|    - | 1082 |  |
|    - | 1083 | `/* --- Answering ---------------------------------------------------------- */` |
|    - | 1084 |  |
|    - | 1085 | `/*` |
|    - | 1086 | ` * Hand back one answer, converted if the domain and the catalog disagree on` |
|    - | 1087 | ` * the encoding. A conversion that cannot be opened -- an unknown name, or one` |
|    - | 1088 | ` * of the code sets §10 leaves out -- gives php's own answer for the same case:` |
|    - | 1089 | ` * the msgid, untranslated.` |
|    - | 1090 | ` */` |
|   73 | 1091 | `static void GtResult(ph7_context *pCtx,gt_dom *pDom,gt_cat *pCat,` |
|    - | 1092 | `	const char *zHit,int nHit,const char *zMiss,int nMiss)` |
|    3 | 1093 | `{` |
|    - | 1094 | `	const char *zFrom,*zTo;` |
|    - | 1095 | `	int nFrom,nTo;` |
|    - | 1096 | `	SyBlob sOut;` |
|    - | 1097 | `	char zCtype[64];` |
|   76 | 1098 | `	if( zHit == 0 ){` |
|   27 | 1099 | `		ph7_result_string(pCtx,zMiss,nMiss);` |
|   27 | 1100 | `		return;` |
|    - | 1101 | `	}` |
|   50 | 1102 | `	zFrom = pCat ? (const char *)SyBlobData(&pCat->sCharset) : 0;` |
|   50 | 1103 | `	nFrom = pCat ? (int)SyBlobLength(&pCat->sCharset) : 0;` |
|   50 | 1104 | `	if( nFrom < 1 ){` |
|    - | 1105 | `		/* A catalog that names no charset is not converted -- glibc's rule. */` |
|  ! 0 | 1106 | `		ph7_result_string(pCtx,zHit,nHit);` |
|  ! 0 | 1107 | `		return;` |
|    - | 1108 | `	}` |
|   50 | 1109 | `	zTo = (const char *)SyBlobData(&pDom->sCodeset);` |
|   50 | 1110 | `	nTo = (int)SyBlobLength(&pDom->sCodeset);` |
|   50 | 1111 | `	if( nTo < 1 ){` |
|    - | 1112 | `		/* Nothing bound: the output charset is the LC_CTYPE locale's, which is` |
|    - | 1113 | ``		 * what glibc reads. `C`/`POSIX` (and a name with no `.codeset`) is`` |
|    - | 1114 | `		 * ASCII, spelled the way the C library spells it. */` |
|    - | 1115 | `		char zLoc[GT_LOCALE_MAX];` |
|    - | 1116 | `		int nLoc,k;` |
|   39 | 1117 | `		GtLocaleName(GT_LC_CTYPE,zLoc,(int)sizeof(zLoc));` |
|   39 | 1118 | `		nLoc = (int)SyStrlen(zLoc);` |
|   76 | 1119 | `		for( k = 0 ; k < nLoc && zLoc[k] != '.' ; ++k ){}` |
|   39 | 1120 | `		if( k < nLoc - 1 ){` |
|   39 | 1121 | `			int m = 0;` |
|   39 | 1122 | `			k++;` |
|  224 | 1123 | `			while( k < nLoc && zLoc[k] != '@' && m + 1 < (int)sizeof(zCtype) ){` |
|  187 | 1124 | `				zCtype[m++] = zLoc[k++];` |
|    2 | 1125 | `			}` |
|   39 | 1126 | `			zCtype[m] = 0;` |
|   39 | 1127 | `			nTo = m;` |
|    9 | 1128 | `		}else{` |
|  ! 0 | 1129 | `			Systrcpy(zCtype,(sxu32)sizeof(zCtype),"ANSI_X3.4-1968",0);` |
|  ! 0 | 1130 | `			nTo = (int)SyStrlen(zCtype);` |
|    - | 1131 | `		}` |
|   39 | 1132 | `		zTo = zCtype;` |
|    7 | 1133 | `	}` |
|   50 | 1134 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   50 | 1135 | `	if( PH7_IconvTranslate(&sOut,zHit,nHit,zFrom,nFrom,zTo,nTo) == PH7_OK ){` |
|   49 | 1136 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    9 | 1137 | `	}else{` |
|    2 | 1138 | `		ph7_result_string(pCtx,zMiss,nMiss);` |
|    - | 1139 | `	}` |
|   50 | 1140 | `	SyBlobRelease(&sOut);` |
|   10 | 1141 | `}` |
|    - | 1142 | `/*` |
|    - | 1143 | `` * The C-string view of a php string: libintl takes a `const char *`, so a NUL`` |
|    - | 1144 | ` * inside the argument ENDS it for the lookup -- and the miss answer is still` |
|    - | 1145 | ` * the caller's whole string, because php returns the zend_string it was given` |
|    - | 1146 | ` * whenever libintl handed the pointer straight back.` |
|    - | 1147 | ` */` |
|  204 | 1148 | `static int GtCStrLen(const char *z,int n)` |
|    3 | 1149 | `{` |
|    - | 1150 | `	int i;` |
| 6385 | 1151 | `	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}` |
|  207 | 1152 | `	return i;` |
|    3 | 1153 | `}` |
|    - | 1154 | `/* php's screens, in php's order. Answers 0 when the argument is fine. */` |
|   64 | 1155 | `static int GtScreenDomain(ph7_context *pCtx,ph7_value *pArg,int iPos,const char **pz,int *pn)` |
|    3 | 1156 | `{` |
|   67 | 1157 | `	int n = 0;` |
|   67 | 1158 | `	const char *z = ph7_value_to_string(pArg,&n);` |
|   67 | 1159 | `	if( n < 1 ){` |
|    6 | 1160 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1161 | `			"%s(): Argument #%d ($domain) must not be empty",ph7_function_name(pCtx),iPos);` |
|    6 | 1162 | `		return -1;` |
|    - | 1163 | `	}` |
|   62 | 1164 | `	if( n > GT_MAX_DOMAIN ){` |
|    2 | 1165 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1166 | `			"%s(): Argument #%d ($domain) is too long",ph7_function_name(pCtx),iPos);` |
|    2 | 1167 | `		return -1;` |
|    - | 1168 | `	}` |
|    - | 1169 | `	/* Both screens above read php's WHOLE string; libintl then gets a C string,` |
|    - | 1170 | `	 * so the name the binding is filed under stops at the first NUL --` |
|    - | 1171 | ``	 * `textdomain("dom\0x")` answers "dom", and a domain that is nothing BUT a`` |
|    - | 1172 | `	 * NUL is the empty one libintl reads as "no domain at all". */` |
|   61 | 1173 | `	*pz = z;` |
|   61 | 1174 | `	*pn = GtCStrLen(z,n);` |
|   61 | 1175 | `	return 0;` |
|   12 | 1176 | `}` |
|  111 | 1177 | `static int GtScreenMsg(ph7_context *pCtx,ph7_value *pArg,int iPos,const char *zName,` |
|    - | 1178 | `	const char **pz,int *pn)` |
|    3 | 1179 | `{` |
|  114 | 1180 | `	int n = 0;` |
|  114 | 1181 | `	const char *z = ph7_value_to_string(pArg,&n);` |
|  114 | 1182 | `	if( n > GT_MAX_MSGID ){` |
|    4 | 1183 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1184 | `			"%s(): Argument #%d ($%s) is too long",ph7_function_name(pCtx),iPos,zName);` |
|    4 | 1185 | `		return -1;` |
|    - | 1186 | `	}` |
|  111 | 1187 | `	*pz = z;` |
|  111 | 1188 | `	*pn = n;` |
|  111 | 1189 | `	return 0;` |
|   14 | 1190 | `}` |
|    - | 1191 | `/* dcgettext()/dcngettext() refuse LC_ALL by name and pass everything else on. */` |
|    8 | 1192 | `static int GtScreenCategory(ph7_context *pCtx,ph7_value *pArg,int iPos,int *piCat)` |
|    2 | 1193 | `{` |
|   10 | 1194 | `	sxi64 iVal = ph7_value_to_int64(pArg);` |
|   10 | 1195 | `	if( iVal == GT_LC_ALL ){` |
|    3 | 1196 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|  ! 0 | 1197 | `			"%s(): Argument #%d ($category) cannot be LC_ALL",ph7_function_name(pCtx),iPos);` |
|    3 | 1198 | `		return -1;` |
|    - | 1199 | `	}` |
|    8 | 1200 | `	*piCat = (int)iVal;` |
|    8 | 1201 | `	return 0;` |
|    2 | 1202 | `}` |
|    - | 1203 | `/* The current textdomain, which is "messages" until one is set. */` |
|   49 | 1204 | `static void GtCurrentDomain(ph7_vm *pVm,const char **pz,int *pn)` |
|    2 | 1205 | `{` |
|   51 | 1206 | `	gt_state *pState = GtState(pVm);` |
|   51 | 1207 | `	if( pState && SyBlobLength(&pState->sDomain) > 0 ){` |
|   40 | 1208 | `		*pz = (const char *)SyBlobData(&pState->sDomain);` |
|   40 | 1209 | `		*pn = (int)SyBlobLength(&pState->sDomain);` |
|   40 | 1210 | `		return;` |
|    - | 1211 | `	}` |
|   12 | 1212 | `	*pz = "messages";` |
|   12 | 1213 | `	*pn = 8;` |
|    2 | 1214 | `}` |
|    - | 1215 | `/*` |
|    - | 1216 | ` * The first catalog in the chain that CARRIES this msgid, with its translation` |
|    - | 1217 | ` * block. Answers 0 when no candidate has it -- which is not the same as "no` |
|    - | 1218 | ` * catalog": a region catalog that says nothing about a msgid falls through to` |
|    - | 1219 | ` * the language one behind it.` |
|    - | 1220 | ` */` |
|   73 | 1221 | `static gt_cat * GtFindInChain(ph7_vm *pVm,gt_cat *pChain,const char *zMsg,int nMsg,` |
|    - | 1222 | `	const char **pzHit,sxu32 *pnHit)` |
|    3 | 1223 | `{` |
|    - | 1224 | `	SyBlob sKey;` |
|    - | 1225 | `	gt_cat *pCat;` |
|   76 | 1226 | `	SyBlobInit(&sKey,&pVm->sAllocator);` |
|   76 | 1227 | `	SyBlobAppend(&sKey,zMsg,(sxu32)GtCStrLen(zMsg,nMsg));` |
|   76 | 1228 | `	SyBlobNullAppend(&sKey);` |
|   91 | 1229 | `	for( pCat = pChain ; pCat ; pCat = pCat->pNext ){` |
|   65 | 1230 | `		if( GtFindMsg(pCat,(const char *)SyBlobData(&sKey),pzHit,pnHit) == PH7_OK ){` |
|   50 | 1231 | `			SyBlobRelease(&sKey);` |
|   50 | 1232 | `			return pCat;` |
|    - | 1233 | `		}` |
|    1 | 1234 | `	}` |
|   27 | 1235 | `	SyBlobRelease(&sKey);` |
|   27 | 1236 | `	return 0;` |
|   10 | 1237 | `}` |
|    - | 1238 | `/* The whole of dcgettext(), which every singular door funnels into. */` |
|   42 | 1239 | `static void GtLookupOne(ph7_context *pCtx,const char *zDom,int nDom,` |
|    - | 1240 | `	const char *zMsg,int nMsg,int iCat)` |
|    3 | 1241 | `{` |
|   45 | 1242 | `	ph7_vm *pVm = pCtx->pVm;` |
|   45 | 1243 | `	gt_dom *pDom = GtDomain(pVm,zDom,nDom);` |
|    - | 1244 | `	gt_cat *pCat;` |
|   45 | 1245 | `	const char *zHit = 0;` |
|   45 | 1246 | `	sxu32 nHit = 0;` |
|   45 | 1247 | `	if( pDom == 0 ){` |
|  ! 0 | 1248 | `		ph7_result_string(pCtx,zMsg,nMsg);` |
|  ! 0 | 1249 | `		return;` |
|    - | 1250 | `	}` |
|   45 | 1251 | `	pCat = GtFindInChain(pVm,GtResolve(pVm,pDom,iCat),zMsg,nMsg,&zHit,&nHit);` |
|   45 | 1252 | `	if( pCat == 0 ){` |
|   19 | 1253 | `		zHit = 0;` |
|    2 | 1254 | `	}else{` |
|    - | 1255 | ``		/* php hands libintl's `const char *` to RETURN_STRING, so an answer stops`` |
|    - | 1256 | `		 * at its first NUL -- which is what a PLURAL entry reached through the` |
|    - | 1257 | `		 * singular door gives back: its first form. */` |
|   27 | 1258 | `		nHit = (sxu32)GtCStrLen(zHit,(int)nHit);` |
|    - | 1259 | `	}` |
|   45 | 1260 | `	GtResult(pCtx,pDom,pCat,zHit,(int)nHit,zMsg,nMsg);` |
|    6 | 1261 | `}` |
|    - | 1262 | `/* The whole of dcngettext(). */` |
|   31 | 1263 | `static void GtLookupPlural(ph7_context *pCtx,const char *zDom,int nDom,` |
|    - | 1264 | `	const char *zOne,int nOne,const char *zMany,int nMany,sxi64 iCount,int iCat)` |
|    3 | 1265 | `{` |
|   34 | 1266 | `	ph7_vm *pVm = pCtx->pVm;` |
|   34 | 1267 | `	gt_dom *pDom = GtDomain(pVm,zDom,nDom);` |
|    - | 1268 | `	gt_cat *pCat;` |
|   34 | 1269 | `	const char *zHit = 0;` |
|   34 | 1270 | `	sxu32 nHit = 0;` |
|   34 | 1271 | `	sxu64 n = (sxu64)iCount;` |
|   34 | 1272 | `	const char *zMiss = n == 1 ? zOne : zMany;` |
|   34 | 1273 | `	int nMiss = n == 1 ? nOne : nMany;` |
|   34 | 1274 | `	if( pDom == 0 ){` |
|  ! 0 | 1275 | `		ph7_result_string(pCtx,zMiss,nMiss);` |
|  ! 0 | 1276 | `		return;` |
|    - | 1277 | `	}` |
|    - | 1278 | ``	/* The SINGULAR is the key: a plural entry is stored `singular\0plural` and`` |
|    - | 1279 | `	 * compares equal to its own singular under the C-string comparison the` |
|    - | 1280 | `	 * format's binary search uses. */` |
|   34 | 1281 | `	pCat = GtFindInChain(pVm,GtResolve(pVm,pDom,iCat),zOne,nOne,&zHit,&nHit);` |
|   34 | 1282 | `	if( pCat == 0 ){` |
|   10 | 1283 | `		zHit = 0;` |
|    2 | 1284 | `	}else{` |
|    - | 1285 | `		/* The plural RULE is the found catalog's, not the chain head's. */` |
|   25 | 1286 | `		const char *zBase = zHit;` |
|   25 | 1287 | `		sxu32 nBase = nHit, iForm = GtPluralIndex(pCat,n), i;` |
|    - | 1288 | `		/* An index the header cannot hold is form 0, and so is a block that` |
|    - | 1289 | `		 * runs out of forms before the index -- both are glibc's own` |
|    - | 1290 | `		 * "this should never happen" answers. */` |
|   25 | 1291 | `		if( iForm >= pCat->nPlurals ){` |
|  ! 0 | 1292 | `			iForm = 0;` |
|  ! 0 | 1293 | `		}` |
|   43 | 1294 | `		for( i = 0 ; i < iForm ; ++i ){` |
|   20 | 1295 | `			sxu32 k = (sxu32)GtCStrLen(zHit,(int)nHit);` |
|   20 | 1296 | `			if( k + 1 >= nHit ){` |
|    2 | 1297 | `				zHit = zBase;` |
|    2 | 1298 | `				nHit = nBase;` |
|    2 | 1299 | `				break;` |
|    - | 1300 | `			}` |
|   19 | 1301 | `			zHit += k + 1;` |
|   19 | 1302 | `			nHit -= k + 1;` |
|    1 | 1303 | `		}` |
|   25 | 1304 | `		nHit = (sxu32)GtCStrLen(zHit,(int)nHit);` |
|    - | 1305 | `	}` |
|   34 | 1306 | `	GtResult(pCtx,pDom,pCat,zHit,(int)nHit,zMiss,nMiss);` |
|    7 | 1307 | `}` |
|    - | 1308 |  |
|    - | 1309 | `/* --- The ten functions --------------------------------------------------- */` |
|    - | 1310 |  |
|    - | 1311 | `/*` |
|    - | 1312 | ` * string textdomain(?string $domain = null)` |
|    - | 1313 | ` *  Set (or, with null, only read) the domain gettext()/ngettext() answer from.` |
|    - | 1314 | ` */` |
|    8 | 1315 | `PH7_PRIVATE int PH7_builtin_textdomain(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1316 | `{` |
|   10 | 1317 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1318 | `	gt_state *pState;` |
|    - | 1319 | `	const char *z;` |
|    - | 1320 | `	int n;` |
|   10 | 1321 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    7 | 1322 | `		if( GtScreenDomain(pCtx,apArg[0],1,&z,&n) != 0 ){` |
|    2 | 1323 | `			return PH7_OK;` |
|    - | 1324 | `		}` |
|    - | 1325 | `		/* An EMPTY C string RESETS the domain: libintl's textdomain("") puts` |
|    - | 1326 | ``		 * `messages` back, which is what a name that is nothing but a NUL asks`` |
|    - | 1327 | `		 * for (php's own screen refuses the empty php string before this). */` |
|    6 | 1328 | `		pState = GtState(pVm);` |
|    6 | 1329 | `		if( pState ){` |
|    6 | 1330 | `			SyBlobReset(&pState->sDomain);` |
|    6 | 1331 | `			SyBlobAppend(&pState->sDomain,z,(sxu32)n);` |
|  ! 0 | 1332 | `		}` |
|  ! 0 | 1333 | `	}` |
|    9 | 1334 | `	GtCurrentDomain(pVm,&z,&n);` |
|    9 | 1335 | `	ph7_result_string(pCtx,z,n);` |
|    9 | 1336 | `	return PH7_OK;` |
|    2 | 1337 | `}` |
|    - | 1338 | `/*` |
|    - | 1339 | ` * string\|false bindtextdomain(string $domain, ?string $directory = null)` |
|    - | 1340 | ` *  Where this domain's catalogs live. php makes the directory ABSOLUTE with` |
|    - | 1341 | ` *  realpath() before libintl ever sees it, so a relative name that does not` |
|    - | 1342 | ` *  resolve is FALSE rather than a binding nothing can read.` |
|    - | 1343 | ` */` |
|   13 | 1344 | `PH7_PRIVATE int PH7_builtin_bindtextdomain(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1345 | `{` |
|   16 | 1346 | `	ph7_vm *pVm = pCtx->pVm;` |
|   16 | 1347 | `	const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|    - | 1348 | `	gt_dom *pDom;` |
|    - | 1349 | `	const char *zDom,*zDir,*zAbs;` |
|   16 | 1350 | `	int nDom,nDir = 0,nAbs = 0;` |
|   16 | 1351 | `	if( nArg < 1 ){` |
|  ! 0 | 1352 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1353 | `		return PH7_OK;` |
|    - | 1354 | `	}` |
|   16 | 1355 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0 ){` |
|    2 | 1356 | `		return PH7_OK;` |
|    - | 1357 | `	}` |
|   15 | 1358 | `	pDom = GtDomain(pVm,zDom,nDom);` |
|   15 | 1359 | `	if( pDom == 0 ){` |
|  ! 0 | 1360 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1361 | `		return PH7_OK;` |
|    - | 1362 | `	}` |
|   15 | 1363 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|    - | 1364 | `		/* A query: the directory in force, or the library's own default. */` |
|    3 | 1365 | `		if( SyBlobLength(&pDom->sDir) > 0 ){` |
|    2 | 1366 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pDom->sDir),` |
|    1 | 1367 | `				(int)SyBlobLength(&pDom->sDir));` |
|    1 | 1368 | `		}else{` |
|    2 | 1369 | `			ph7_result_string(pCtx,GT_DEFAULT_DIR,-1);` |
|    - | 1370 | `		}` |
|    3 | 1371 | `		return PH7_OK;` |
|    - | 1372 | `	}` |
|   13 | 1373 | `	zDir = ph7_value_to_string(apArg[1],&nDir);` |
|    - | 1374 | `	/* The VFS writes STRAIGHT into the call's result, so seed it empty and read` |
|    - | 1375 | `	 * the answer back out -- which is also the value this call returns. */` |
|   13 | 1376 | `	ph7_result_string(pCtx,"",0);` |
|   13 | 1377 | `	if( nDir < 1 \|\| (nDir == 1 && zDir[0] == '0') ){` |
|    - | 1378 | `		/* php reads an empty directory (and the string "0") as the working one. */` |
|    3 | 1379 | `		if( pVfs == 0 \|\| pVfs->xGetcwd == 0 \|\| pVfs->xGetcwd(pCtx) != PH7_OK ){` |
|  ! 0 | 1380 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 | 1381 | `			return PH7_OK;` |
|    - | 1382 | `		}` |
|    1 | 1383 | `	}else{` |
|    - | 1384 | `		SyBlob sPath;` |
|    - | 1385 | `		int rc;` |
|   11 | 1386 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|   11 | 1387 | `		SyBlobAppend(&sPath,zDir,(sxu32)nDir);` |
|   11 | 1388 | `		SyBlobNullAppend(&sPath);` |
|   11 | 1389 | `		rc = (pVfs == 0 \|\| pVfs->xRealpath == 0)` |
|   14 | 1390 | `			? -1 : pVfs->xRealpath((const char *)SyBlobData(&sPath),pCtx);` |
|   11 | 1391 | `		SyBlobRelease(&sPath);` |
|   11 | 1392 | `		if( rc != PH7_OK ){` |
|    2 | 1393 | `			ph7_result_bool(pCtx,0);` |
|    2 | 1394 | `			return PH7_OK;` |
|    - | 1395 | `		}` |
|    - | 1396 | `	}` |
|   12 | 1397 | `	zAbs = ph7_value_to_string(pCtx->pRet,&nAbs);` |
|   12 | 1398 | `	SyBlobReset(&pDom->sDir);` |
|   12 | 1399 | `	SyBlobAppend(&pDom->sDir,zAbs,(sxu32)nAbs);` |
|   12 | 1400 | `	pDom->bResolved = 0;   /* a new directory: re-read on the next lookup */` |
|   12 | 1401 | `	return PH7_OK;` |
|    5 | 1402 | `}` |
|    - | 1403 | `/*` |
|    - | 1404 | ` * string\|false bind_textdomain_codeset(string $domain, ?string $codeset = null)` |
|    - | 1405 | ` *  The encoding this domain's answers come back in. With null (or nothing) it` |
|    - | 1406 | ` *  only READS the binding, and answers false when there is none.` |
|    - | 1407 | ` */` |
|   10 | 1408 | `PH7_PRIVATE int PH7_builtin_bind_textdomain_codeset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1409 | `{` |
|   12 | 1410 | `	ph7_vm *pVm = pCtx->pVm;` |
|    - | 1411 | `	gt_dom *pDom;` |
|    - | 1412 | `	const char *zDom,*zCs;` |
|   12 | 1413 | `	int nDom,nCs = 0;` |
|   12 | 1414 | `	if( nArg < 1 ){` |
|  ! 0 | 1415 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1416 | `		return PH7_OK;` |
|    - | 1417 | `	}` |
|   12 | 1418 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0 ){` |
|    2 | 1419 | `		return PH7_OK;` |
|    - | 1420 | `	}` |
|   11 | 1421 | `	pDom = nDom > 0 ? GtDomain(pVm,zDom,nDom) : 0;` |
|   11 | 1422 | `	if( pDom == 0 ){` |
|    - | 1423 | `		/* libintl reads the empty domain name as no domain at all. */` |
|    2 | 1424 | `		ph7_result_bool(pCtx,0);` |
|    2 | 1425 | `		return PH7_OK;` |
|    - | 1426 | `	}` |
|   10 | 1427 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|    8 | 1428 | `		zCs = ph7_value_to_string(apArg[1],&nCs);` |
|    8 | 1429 | `		SyBlobReset(&pDom->sCodeset);` |
|    8 | 1430 | `		SyBlobAppend(&pDom->sCodeset,zCs,(sxu32)GtCStrLen(zCs,nCs));` |
|  ! 0 | 1431 | `	}` |
|   10 | 1432 | `	if( SyBlobLength(&pDom->sCodeset) < 1 ){` |
|    2 | 1433 | `		ph7_result_bool(pCtx,0);` |
|    2 | 1434 | `		return PH7_OK;` |
|    - | 1435 | `	}` |
|    9 | 1436 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&pDom->sCodeset),` |
|    7 | 1437 | `		(int)SyBlobLength(&pDom->sCodeset));` |
|    9 | 1438 | `	return PH7_OK;` |
|    2 | 1439 | `}` |
|    - | 1440 | `/*` |
|    - | 1441 | ` * string gettext(string $message) / string _(string $message)` |
|    - | 1442 | ` */` |
|   25 | 1443 | `PH7_PRIVATE int PH7_builtin_gettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1444 | `{` |
|    - | 1445 | `	const char *zDom,*zMsg;` |
|    - | 1446 | `	int nDom,nMsg;` |
|   27 | 1447 | `	if( nArg < 1 ){` |
|  ! 0 | 1448 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1449 | `		return PH7_OK;` |
|    - | 1450 | `	}` |
|   27 | 1451 | `	if( GtScreenMsg(pCtx,apArg[0],1,"message",&zMsg,&nMsg) != 0 ){` |
|    2 | 1452 | `		return PH7_OK;` |
|    - | 1453 | `	}` |
|   26 | 1454 | `	GtCurrentDomain(pCtx->pVm,&zDom,&nDom);` |
|   26 | 1455 | `	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,GT_LC_MESSAGES);` |
|   26 | 1456 | `	return PH7_OK;` |
|    2 | 1457 | `}` |
|    - | 1458 | `/*` |
|    - | 1459 | ` * string dgettext(string $domain, string $message)` |
|    - | 1460 | ` */` |
|   15 | 1461 | `PH7_PRIVATE int PH7_builtin_dgettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1462 | `{` |
|    - | 1463 | `	const char *zDom,*zMsg;` |
|    - | 1464 | `	int nDom,nMsg;` |
|   18 | 1465 | `	if( nArg < 2 ){` |
|  ! 0 | 1466 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1467 | `		return PH7_OK;` |
|    - | 1468 | `	}` |
|   15 | 1469 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0` |
|   16 | 1470 | `	 \|\| GtScreenMsg(pCtx,apArg[1],2,"message",&zMsg,&nMsg) != 0 ){` |
|    3 | 1471 | `		return PH7_OK;` |
|    - | 1472 | `	}` |
|   16 | 1473 | `	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,GT_LC_MESSAGES);` |
|   16 | 1474 | `	return PH7_OK;` |
|    6 | 1475 | `}` |
|    - | 1476 | `/*` |
|    - | 1477 | ` * string dcgettext(string $domain, string $message, int $category)` |
|    - | 1478 | ` */` |
|    6 | 1479 | `PH7_PRIVATE int PH7_builtin_dcgettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1480 | `{` |
|    - | 1481 | `	const char *zDom,*zMsg;` |
|    - | 1482 | `	int nDom,nMsg,iCat;` |
|    8 | 1483 | `	if( nArg < 3 ){` |
|  ! 0 | 1484 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1485 | `		return PH7_OK;` |
|    - | 1486 | `	}` |
|    6 | 1487 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0` |
|    6 | 1488 | `	 \|\| GtScreenMsg(pCtx,apArg[1],2,"message",&zMsg,&nMsg) != 0` |
|    8 | 1489 | `	 \|\| GtScreenCategory(pCtx,apArg[2],3,&iCat) != 0 ){` |
|    2 | 1490 | `		return PH7_OK;` |
|    - | 1491 | `	}` |
|    7 | 1492 | `	GtLookupOne(pCtx,zDom,nDom,zMsg,nMsg,iCat);` |
|    7 | 1493 | `	return PH7_OK;` |
|    2 | 1494 | `}` |
|    - | 1495 | `/*` |
|    - | 1496 | ` * string ngettext(string $singular, string $plural, int $count)` |
|    - | 1497 | ` */` |
|   20 | 1498 | `PH7_PRIVATE int PH7_builtin_ngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    2 | 1499 | `{` |
|    - | 1500 | `	const char *zDom,*zOne,*zMany;` |
|    - | 1501 | `	int nDom,nOne,nMany;` |
|   22 | 1502 | `	if( nArg < 3 ){` |
|  ! 0 | 1503 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1504 | `		return PH7_OK;` |
|    - | 1505 | `	}` |
|   20 | 1506 | `	if( GtScreenMsg(pCtx,apArg[0],1,"singular",&zOne,&nOne) != 0` |
|   21 | 1507 | `	 \|\| GtScreenMsg(pCtx,apArg[1],2,"plural",&zMany,&nMany) != 0 ){` |
|    3 | 1508 | `		return PH7_OK;` |
|    - | 1509 | `	}` |
|   20 | 1510 | `	GtCurrentDomain(pCtx->pVm,&zDom,&nDom);` |
|   20 | 1511 | `	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,` |
|   18 | 1512 | `		ph7_value_to_int64(apArg[2]),GT_LC_MESSAGES);` |
|   20 | 1513 | `	return PH7_OK;` |
|    2 | 1514 | `}` |
|    - | 1515 | `/*` |
|    - | 1516 | ` * string dngettext(string $domain, string $singular, string $plural, int $count)` |
|    - | 1517 | ` */` |
|   13 | 1518 | `PH7_PRIVATE int PH7_builtin_dngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    3 | 1519 | `{` |
|    - | 1520 | `	const char *zDom,*zOne,*zMany;` |
|    - | 1521 | `	int nDom,nOne,nMany;` |
|   16 | 1522 | `	if( nArg < 4 ){` |
|  ! 0 | 1523 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1524 | `		return PH7_OK;` |
|    - | 1525 | `	}` |
|   13 | 1526 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0` |
|   12 | 1527 | `	 \|\| GtScreenMsg(pCtx,apArg[1],2,"singular",&zOne,&nOne) != 0` |
|   15 | 1528 | `	 \|\| GtScreenMsg(pCtx,apArg[2],3,"plural",&zMany,&nMany) != 0 ){` |
|    2 | 1529 | `		return PH7_OK;` |
|    - | 1530 | `	}` |
|   19 | 1531 | `	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,` |
|   12 | 1532 | `		ph7_value_to_int64(apArg[3]),GT_LC_MESSAGES);` |
|   15 | 1533 | `	return PH7_OK;` |
|    7 | 1534 | `}` |
|    - | 1535 | `/*` |
|    - | 1536 | ` * string dcngettext(string $domain, string $singular, string $plural,` |
|    - | 1537 | ` *                   int $count, int $category)` |
|    - | 1538 | ` */` |
|    2 | 1539 | `PH7_PRIVATE int PH7_builtin_dcngettext(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    1 | 1540 | `{` |
|    - | 1541 | `	const char *zDom,*zOne,*zMany;` |
|    - | 1542 | `	int nDom,nOne,nMany,iCat;` |
|    3 | 1543 | `	if( nArg < 5 ){` |
|  ! 0 | 1544 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 | 1545 | `		return PH7_OK;` |
|    - | 1546 | `	}` |
|    2 | 1547 | `	if( GtScreenDomain(pCtx,apArg[0],1,&zDom,&nDom) != 0` |
|    2 | 1548 | `	 \|\| GtScreenMsg(pCtx,apArg[1],2,"singular",&zOne,&nOne) != 0` |
|    2 | 1549 | `	 \|\| GtScreenMsg(pCtx,apArg[2],3,"plural",&zMany,&nMany) != 0` |
|    3 | 1550 | `	 \|\| GtScreenCategory(pCtx,apArg[4],5,&iCat) != 0 ){` |
|    2 | 1551 | `		return PH7_OK;` |
|    - | 1552 | `	}` |
|    2 | 1553 | `	GtLookupPlural(pCtx,zDom,nDom,zOne,nOne,zMany,nMany,` |
|    1 | 1554 | `		ph7_value_to_int64(apArg[3]),iCat);` |
|    2 | 1555 | `	return PH7_OK;` |
|    1 | 1556 | `}` |
|    - | 1557 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|    - | 1558 |  |
