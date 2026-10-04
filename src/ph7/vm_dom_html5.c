/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/tree.h>

/*
 * The named character references, as php's own `get_html_translation_table()`
 * spells them for the HTML 4.01 document type: 253 names, sorted so a lookup
 * is a binary search.  libxml has this table too, behind `htmlEntityLookup`,
 * but that name is deprecated in 2.13 and the /WX build refuses it -- and a
 * table that moves with the linked libxml version is the wrong thing for a
 * parser whose answers are supposed to be php's.
 */
static const struct { const char *zName; sxu32 cp; } aHtml5Ent[] = {
	{"#039",0x0027},{"AElig",0x00C6},{"Aacute",0x00C1},{"Acirc",0x00C2},
	{"Agrave",0x00C0},{"Alpha",0x0391},{"Aring",0x00C5},{"Atilde",0x00C3},
	{"Auml",0x00C4},{"Beta",0x0392},{"Ccedil",0x00C7},{"Chi",0x03A7},
	{"Dagger",0x2021},{"Delta",0x0394},{"ETH",0x00D0},{"Eacute",0x00C9},
	{"Ecirc",0x00CA},{"Egrave",0x00C8},{"Epsilon",0x0395},{"Eta",0x0397},
	{"Euml",0x00CB},{"Gamma",0x0393},{"Iacute",0x00CD},{"Icirc",0x00CE},
	{"Igrave",0x00CC},{"Iota",0x0399},{"Iuml",0x00CF},{"Kappa",0x039A},
	{"Lambda",0x039B},{"Mu",0x039C},{"Ntilde",0x00D1},{"Nu",0x039D},
	{"OElig",0x0152},{"Oacute",0x00D3},{"Ocirc",0x00D4},{"Ograve",0x00D2},
	{"Omega",0x03A9},{"Omicron",0x039F},{"Oslash",0x00D8},{"Otilde",0x00D5},
	{"Ouml",0x00D6},{"Phi",0x03A6},{"Pi",0x03A0},{"Prime",0x2033},
	{"Psi",0x03A8},{"Rho",0x03A1},{"Scaron",0x0160},{"Sigma",0x03A3},
	{"THORN",0x00DE},{"Tau",0x03A4},{"Theta",0x0398},{"Uacute",0x00DA},
	{"Ucirc",0x00DB},{"Ugrave",0x00D9},{"Upsilon",0x03A5},{"Uuml",0x00DC},
	{"Xi",0x039E},{"Yacute",0x00DD},{"Yuml",0x0178},{"Zeta",0x0396},
	{"aacute",0x00E1},{"acirc",0x00E2},{"acute",0x00B4},{"aelig",0x00E6},
	{"agrave",0x00E0},{"alefsym",0x2135},{"alpha",0x03B1},{"amp",0x0026},
	{"and",0x2227},{"ang",0x2220},{"aring",0x00E5},{"asymp",0x2248},
	{"atilde",0x00E3},{"auml",0x00E4},{"bdquo",0x201E},{"beta",0x03B2},
	{"brvbar",0x00A6},{"bull",0x2022},{"cap",0x2229},{"ccedil",0x00E7},
	{"cedil",0x00B8},{"cent",0x00A2},{"chi",0x03C7},{"circ",0x02C6},
	{"clubs",0x2663},{"cong",0x2245},{"copy",0x00A9},{"crarr",0x21B5},
	{"cup",0x222A},{"curren",0x00A4},{"dArr",0x21D3},{"dagger",0x2020},
	{"darr",0x2193},{"deg",0x00B0},{"delta",0x03B4},{"diams",0x2666},
	{"divide",0x00F7},{"eacute",0x00E9},{"ecirc",0x00EA},{"egrave",0x00E8},
	{"empty",0x2205},{"emsp",0x2003},{"ensp",0x2002},{"epsilon",0x03B5},
	{"equiv",0x2261},{"eta",0x03B7},{"eth",0x00F0},{"euml",0x00EB},
	{"euro",0x20AC},{"exist",0x2203},{"fnof",0x0192},{"forall",0x2200},
	{"frac12",0x00BD},{"frac14",0x00BC},{"frac34",0x00BE},{"frasl",0x2044},
	{"gamma",0x03B3},{"ge",0x2265},{"gt",0x003E},{"hArr",0x21D4},
	{"harr",0x2194},{"hearts",0x2665},{"hellip",0x2026},{"iacute",0x00ED},
	{"icirc",0x00EE},{"iexcl",0x00A1},{"igrave",0x00EC},{"image",0x2111},
	{"infin",0x221E},{"int",0x222B},{"iota",0x03B9},{"iquest",0x00BF},
	{"isin",0x2208},{"iuml",0x00EF},{"kappa",0x03BA},{"lArr",0x21D0},
	{"lambda",0x03BB},{"lang",0x2329},{"laquo",0x00AB},{"larr",0x2190},
	{"lceil",0x2308},{"ldquo",0x201C},{"le",0x2264},{"lfloor",0x230A},
	{"lowast",0x2217},{"loz",0x25CA},{"lrm",0x200E},{"lsaquo",0x2039},
	{"lsquo",0x2018},{"lt",0x003C},{"macr",0x00AF},{"mdash",0x2014},
	{"micro",0x00B5},{"middot",0x00B7},{"minus",0x2212},{"mu",0x03BC},
	{"nabla",0x2207},{"nbsp",0x00A0},{"ndash",0x2013},{"ne",0x2260},
	{"ni",0x220B},{"not",0x00AC},{"notin",0x2209},{"nsub",0x2284},
	{"ntilde",0x00F1},{"nu",0x03BD},{"oacute",0x00F3},{"ocirc",0x00F4},
	{"oelig",0x0153},{"ograve",0x00F2},{"oline",0x203E},{"omega",0x03C9},
	{"omicron",0x03BF},{"oplus",0x2295},{"or",0x2228},{"ordf",0x00AA},
	{"ordm",0x00BA},{"oslash",0x00F8},{"otilde",0x00F5},{"otimes",0x2297},
	{"ouml",0x00F6},{"para",0x00B6},{"part",0x2202},{"permil",0x2030},
	{"perp",0x22A5},{"phi",0x03C6},{"pi",0x03C0},{"piv",0x03D6},
	{"plusmn",0x00B1},{"pound",0x00A3},{"prime",0x2032},{"prod",0x220F},
	{"prop",0x221D},{"psi",0x03C8},{"quot",0x0022},{"rArr",0x21D2},
	{"radic",0x221A},{"rang",0x232A},{"raquo",0x00BB},{"rarr",0x2192},
	{"rceil",0x2309},{"rdquo",0x201D},{"real",0x211C},{"reg",0x00AE},
	{"rfloor",0x230B},{"rho",0x03C1},{"rlm",0x200F},{"rsaquo",0x203A},
	{"rsquo",0x2019},{"sbquo",0x201A},{"scaron",0x0161},{"sdot",0x22C5},
	{"sect",0x00A7},{"shy",0x00AD},{"sigma",0x03C3},{"sigmaf",0x03C2},
	{"sim",0x223C},{"spades",0x2660},{"sub",0x2282},{"sube",0x2286},
	{"sum",0x2211},{"sup",0x2283},{"sup1",0x00B9},{"sup2",0x00B2},
	{"sup3",0x00B3},{"supe",0x2287},{"szlig",0x00DF},{"tau",0x03C4},
	{"there4",0x2234},{"theta",0x03B8},{"thetasym",0x03D1},{"thinsp",0x2009},
	{"thorn",0x00FE},{"tilde",0x02DC},{"times",0x00D7},{"trade",0x2122},
	{"uArr",0x21D1},{"uacute",0x00FA},{"uarr",0x2191},{"ucirc",0x00FB},
	{"ugrave",0x00F9},{"uml",0x00A8},{"upsih",0x03D2},{"upsilon",0x03C5},
	{"uuml",0x00FC},{"weierp",0x2118},{"xi",0x03BE},{"yacute",0x00FD},
	{"yen",0x00A5},{"yuml",0x00FF},{"zeta",0x03B6},{"zwj",0x200D},
	{"zwnj",0x200C},
};
/* An ordering comparator over two NUL-terminated names; sx has none, and
 * SyStrncmp needs a length that would have to come from somewhere. */
static int Html5StrCmp(const char *zLeft,const char *zRight)
{
	while( *zLeft != 0 && *zLeft == *zRight ){
		zLeft++;
		zRight++;
	}
	return (int)(unsigned char)*zLeft - (int)(unsigned char)*zRight;
}
static sxu32 Html5EntLookup(const char *zName)
{
	int iLo = 0,iHi = (int)SX_ARRAYSIZE(aHtml5Ent) - 1;
	while( iLo <= iHi ){
		int iMid = (iLo + iHi) / 2;
		int iCmp = Html5StrCmp(aHtml5Ent[iMid].zName,zName);
		if( iCmp == 0 ){
			return aHtml5Ent[iMid].cp;
		}
		if( iCmp < 0 ){
			iLo = iMid + 1;
		}else{
			iHi = iMid - 1;
		}
	}
	return 0;
}

/*
 * The HTML5 tokenizer and tree constructor behind `Dom\HTMLDocument`'s two
 * parsing producers.
 *
 * php 8.4 parses HTML for the namespaced tree through lexbor, and the shape it
 * answers is NOT libxml's `htmlReadMemory`: libxml mints no `<head>` for a
 * document that states none and no `<tbody>` around a bare `<tr>`, where the
 * HTML5 tree construction algorithm mints both.  Nothing in libxml can be asked
 * for that shape, so the algorithm is written here, over libxml's tree as the
 * output type -- the same tree every other door of this family already reads.
 *
 * What the file owes the rest of the engine is one entry point, PH7_Html5Parse:
 * source bytes in, an xmlDocPtr out, and every diagnostic handed back through a
 * callback so the two doors can name themselves in it.  It never touches a
 * ph7_context, which is what keeps the algorithm testable against the one thing
 * that decides it -- the tree a document produces.
 *
 * The tokenizer is the spec's states reachable from an HTML (never foreign)
 * insertion point: data, the tag and attribute states, comments, doctype, and
 * the two text-only variants (RCDATA for `<title>`/`<textarea>`, RAWTEXT for
 * `<style>`, `<script>` and their kin) that the tree constructor arms by name.
 * Character references resolve through the name table at the top of this file,
 * plus the numeric forms and their C1 replacement table.
 *
 * The tree constructor runs the insertion modes down to `in body` and then
 * reads the OPEN ELEMENT STACK rather than carrying separate table modes: the
 * only thing `in table` and its three children decide is which implied element
 * a `<tr>` or a `<td>` needs above it, and the stack already says.
 */

#define HTML5_NS "http://www.w3.org/1999/xhtml"
/*
 * The two namespaces a `<svg>` or a `<math>` start tag switches the tree into,
 * and the two an adjusted attribute name is moved into.  A foreign element is
 * namespaced whatever the document's options say: `HTML_NO_DEFAULT_NS` speaks
 * about the HTML declaration alone, because an SVG subtree that lost its
 * namespace would stop being SVG rather than stop being decorated.
 */
#define HTML5_SVG_NS   "http://www.w3.org/2000/svg"
#define HTML5_MATH_NS  "http://www.w3.org/1998/Math/MathML"
#define HTML5_XLINK_NS "http://www.w3.org/1999/xlink"
#define HTML5_XML_NS   "http://www.w3.org/XML/1998/namespace"
/* ...as the three answers to "which rules does this element parse under". */
#define HTML5_NSK_HTML 0
#define HTML5_NSK_SVG  1
#define HTML5_NSK_MATH 2

/* Parse flags; the door maps php's option bits onto these. */
#define HTML5_NOIMPLIED    0x01   /* LIBXML_HTML_NOIMPLIED */
#define HTML5_NO_DEF_NS    0x02   /* Dom\HTML_NO_DEFAULT_NS */

/* Token kinds */
#define HTML5_TOK_EOF      0
#define HTML5_TOK_TEXT     1
#define HTML5_TOK_START    2
#define HTML5_TOK_END      3
#define HTML5_TOK_COMMENT  4
#define HTML5_TOK_DOCTYPE  5

/*
 * Insertion modes.  The table modes past IN_BODY are decided by the open
 * stack, but the three frameset ones cannot be: a frameset document has no
 * body at all, and what it accepts is a closed list rather than a question
 * about what is open.
 */
#define HTML5_M_INITIAL      0
#define HTML5_M_BEFORE_HTML  1
#define HTML5_M_BEFORE_HEAD  2
#define HTML5_M_IN_HEAD      3
#define HTML5_M_AFTER_HEAD   4
#define HTML5_M_IN_BODY      5
#define HTML5_M_AFTER_BODY   6
#define HTML5_M_IN_FRAMESET  7
#define HTML5_M_AFTER_FRAMESET 8
#define HTML5_M_AFTER_AFTER_FRAMESET 9
#define HTML5_M_IN_TEMPLATE  10
/*
 * `in head noscript` cannot be answered from the open stack either: what it
 * accepts is a closed list, and everything else CLOSES the noscript and is
 * re-run one mode out.
 */
#define HTML5_M_IN_HEAD_NOSCRIPT 11

/* Text-only element flavours the tree constructor arms the tokenizer with. */
#define HTML5_TEXT_NONE    0
#define HTML5_TEXT_RCDATA  1
#define HTML5_TEXT_RAW     2
/*
 * `<plaintext>` has no end tag at all: the state it switches the tokenizer
 * into is never left, so every remaining byte of the document is its text.
 */
#define HTML5_TEXT_PLAIN   3

typedef struct html5_attr html5_attr;
struct html5_attr {
	sxu32 nNameOfs;  /* offset into sAttrBuf of the NUL-terminated name  */
	sxu32 nValOfs;   /* offset into sAttrBuf of the NUL-terminated value */
};

typedef struct html5_parser html5_parser;
struct html5_parser {
	const unsigned char *zIn;      /* normalized source (CRLF and CR folded) */
	sxu32 nIn;
	sxu32 iPos;
	sxu32 iLine;                   /* 1-based, of the NEXT byte */
	sxu32 iCol;                    /* 1-based, of the NEXT byte */
	SyMemBackend *pAlloc;
	xmlDocPtr pDoc;
	xmlNsPtr pNs;                  /* the XHTML declaration of the current root */
	int iFlags;
	int iMode;
	int iText;                     /* armed text flavour for the next token   */
	SySet sOpen;                   /* xmlNodePtr, the open element stack      */
	SySet sFmt;                    /* xmlNodePtr, the active formatting list;
	                                * a 0 entry is the spec's MARKER          */
	SySet sTmpl;                   /* int, the insertion mode each open
	                                * `<template>` will be handed back        */
	xmlNodePtr pHead;
	xmlNodePtr pHtml;
	/* the current token */
	int iTok;
	SyBlob sName;                  /* tag or doctype name, folded, NUL-ended  */
	SyBlob sBuf;                   /* text or comment data                    */
	SyBlob sAttrBuf;
	SySet sAttr;
	int bSelfClose;
	int bTextTok;                  /* the token is an armed element's TEXT     */
	int bFoster;                   /* this token's content leaves the table    */
	int bFramesetOk;               /* may a `<frameset>` still replace the body */
	int bHtmlRules;  /* this token skips the namespace question exactly once */
	sxu32 iTokLine,iTokCol,iTokCol2;  /* the NAME's span, which is what php prints */
	void (*xErr)(void *,const char *,const char *,sxu32,sxu32,sxu32);
	void *pErrUser;
};

/*
 * The elements that never take children, so a start tag is the whole element
 * and an end tag for one is ignored.
 */
static const char * const azHtml5Void[] = {
	"area","base","basefont","bgsound","br","col","embed","frame","hr","img",
	"input","keygen","link","meta","param","source","track","wbr"
};
/* `<title>` and `<textarea>` take character references but no markup. */
static const char * const azHtml5Rcdata[] = { "textarea","title" };
/*
 * These take neither: everything to the matching end tag is one text node.
 * `<noscript>` is NOT here.  It is RAWTEXT only where the scripting flag is
 * set, and this parser -- like php's -- parses with scripting DISABLED, so a
 * `<noscript>` holds markup: in the body an ordinary element, in the head the
 * insertion mode of its own below.
 */
static const char * const azHtml5Raw[] = {
	"iframe","noembed","noframes","script","style","xmp"
};
/*
 * The head elements `in head noscript` keeps INSIDE the noscript.  It is not
 * azHtml5Head: a `<base>`, `<script>`, `<template>` or `<title>` written there
 * is the mode's anything-else branch, so it closes the noscript and lands in
 * the head beside it.
 */
static const char * const azHtml5HeadNoscript[] = {
	"basefont","bgsound","link","meta","noframes","style"
};
/*
 * A start tag that closes an open `<p>`.  This is the spec's list verbatim: it
 * is the one piece of `in body` that cannot be derived from the stack, because
 * it is a property of the tag being opened rather than of what is open.
 */
static const char * const azHtml5ClosesP[] = {
	"address","article","aside","blockquote","center","details","dialog","dir",
	"div","dl","fieldset","figcaption","figure","footer","form","h1","h2","h3",
	"h4","h5","h6","header","hgroup","hr","li","main","menu","nav","ol","p",
	"plaintext","pre","search","section","summary","table","ul","xmp"
};
/*
 * The start tags that make a later `<frameset>` unwritable -- the spec's
 * frameset-ok flag, swept out of php rather than read off the list, because
 * the two disagree in one place: php clears the flag for `<input type=HIDDEN>`
 * where the spec asks for an ASCII case-INSENSITIVE match, so the comparison
 * below is the byte one php makes.  `<body>` is here too, but only its
 * explicit spelling clears the flag, so it is handled where a body is opened
 * rather than by name.
 */
static const char * const azHtml5NoFrameset[] = {
	"applet","area","br","button","dd","dt","embed","hr","iframe","image",
	"img","keygen","li","listing","marquee","object","plaintext","pre",
	"select","table","textarea","wbr","xmp"
};
/* The elements the head collects, wherever in the source they are spelled. */
static const char * const azHtml5Head[] = {
	"base","basefont","bgsound","link","meta","noframes","script","style",
	"template","title"
};
/*
 * The formatting elements: the ones an end tag does not simply close, because
 * a document that leaves one open across a close has it REOPENED after it.
 */
static const char * const azHtml5Fmt[] = {
	"a","b","big","code","em","font","i","nobr","s","small","strike","strong",
	"tt","u"
};
/*
 * The `special` category.  Only two questions ask it: which open element is
 * the `furthest block` an adoption moves out of, and which start tags skip the
 * reconstruction (everything here but the handful listed in Html5Reconstructs).
 */
static const char * const azHtml5Special[] = {
	"address","applet","area","article","aside","base","basefont","bgsound",
	"blockquote","body","br","button","caption","center","col","colgroup","dd",
	"details","dir","div","dl","dt","embed","fieldset","figcaption","figure",
	"footer","form","frame","frameset","h1","h2","h3","h4","h5","h6","head",
	"header","hgroup","hr","html","iframe","img","input","keygen","li","link",
	"listing","main","marquee","menu","meta","nav","noembed","noframes",
	"noscript","object","ol","p","param","plaintext","pre","script","search",
	"section","select","source","style","summary","table","tbody","td",
	"template","textarea","tfoot","th","thead","title","tr","track","ul","wbr",
	"xmp"
};
/* The elements a scope question stops at. */
static const char * const azHtml5Scope[] = {
	"applet","caption","html","marquee","object","table","td","template","th"
};
/*
 * The elements whose contents are their own formatting world: opening one
 * parks a MARKER on the list, and popping it clears back past that marker, so
 * a `<b>` left open outside is not reopened inside -- nor the other way.
 */
static const char * const azHtml5FmtMark[] = {
	"applet","caption","marquee","object","td","th"
};
/*
 * The special elements whose start tag reconstructs anyway.  Everything NOT
 * special reconstructs; these are the exceptions on the other side, and the
 * rest of the special list (the block containers, the head elements and the
 * table internals) does not.
 */
static const char * const azHtml5ReconSpecial[] = {
	"applet","area","br","button","embed","img","input","keygen","marquee",
	"object","select","wbr","xmp"
};
/*
 * The start tags a table's own insertion modes handle.  Everything else
 * spelled while a table, a row group or a row is the current node is content
 * the table cannot hold, and is FOSTER PARENTED -- moved out to just before
 * the table rather than left inside it.  `<form>` and a hidden `<input>` are
 * on this list too but are asked for by name: `<input>` only qualifies when
 * its `type` says `hidden`.
 */
static const char * const azHtml5TableOwn[] = {
	"caption","col","colgroup","script","style","table","tbody","td","template",
	"tfoot","th","thead","tr"
};
/*
 * The table furniture, written where no table is open.  Each of these names
 * belongs to a table's own insertion mode, and `in body` -- which is where one
 * arrives when the source forgot the `<table>`, or put the tag inside a
 * `<select>` or a `<p>` -- DROPS it and carries on, so the text after it lands
 * in whatever was already open rather than inside an element nothing can nest.
 *
 * `head` and `frame` are the same refusal and are answered on their own, because
 * they are dropped whether or not a table is open.
 */
static const char * const azHtml5NoTable[] = {
	"caption","col","colgroup","tbody","td","tfoot","th","thead","tr"
};

/* sx has no NUL-terminated comparator; every name here is one. */
/*
 * SyBlobNullAppend terminates a blob without counting the NUL, which is what
 * every reader of one wants -- but the attribute buffer packs name after value
 * after name and reads each back by offset, so its separators have to be real
 * bytes or the next append lands on top of them.
 */
static void Html5EndStr(SyBlob *pBlob)
{
	char zNul = 0;
	SyBlobAppend(pBlob,&zNul,1);
}
static int Html5Eq(const char *zLeft,const char *zRight)
{
	sxu32 n = SyStrlen(zLeft);
	return n == SyStrlen(zRight) && SyMemcmp(zLeft,zRight,n) == 0;
}
static int Html5NameIn(const char * const *apList,int nList,const char *zName)
{
	int i;
	for( i = 0 ; i < nList ; ++i ){
		if( Html5Eq(apList[i],zName) ){
			return 1;
		}
	}
	return 0;
}
#define HTML5_IN(L,z) Html5NameIn((L),(int)SX_ARRAYSIZE(L),(z))

/* ------------------------------------------------------------------ *
 * The tokenizer
 * ------------------------------------------------------------------ */

/* The tokenizer asks the tree exactly one question -- see the CDATA branch. */
static xmlNodePtr Html5Top(html5_parser *p);
static int Html5NsKind(xmlNodePtr pEl);

static int Html5Peek(html5_parser *p,sxu32 nAhead)
{
	return (p->iPos + nAhead < p->nIn) ? p->zIn[p->iPos + nAhead] : -1;
}
static int Html5Read(html5_parser *p)
{
	int c;
	if( p->iPos >= p->nIn ){
		return -1;
	}
	c = p->zIn[p->iPos++];
	if( c == '\n' ){
		p->iLine++;
		p->iCol = 1;
	}else{
		p->iCol++;
	}
	return c;
}
static int Html5IsSpace(int c)
{
	return c == ' ' || c == '\n' || c == '\t' || c == '\f' || c == '\r';
}
static int Html5Lower(int c)
{
	return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}
/* Does the source at the cursor spell `zWord`, case-insensitively? */
static int Html5LookWord(html5_parser *p,const char *zWord)
{
	sxu32 i;
	for( i = 0 ; zWord[i] != 0 ; ++i ){
		int c = Html5Peek(p,i);
		if( c < 0 || Html5Lower(c) != Html5Lower((unsigned char)zWord[i]) ){
			return 0;
		}
	}
	return 1;
}
static void Html5Skip(html5_parser *p,sxu32 n)
{
	while( n-- > 0 ){
		Html5Read(p);
	}
}
static void Html5Err(html5_parser *p,const char *zKind,const char *zName,
	sxu32 iLine,sxu32 iCol,sxu32 iCol2)
{
	if( p->xErr ){
		p->xErr(p->pErrUser,zKind,zName,iLine,iCol,iCol2);
	}
}
/* Append one code point as UTF-8. */
static void Html5PutUtf8(SyBlob *pOut,unsigned int c)
{
	unsigned char zBuf[4];
	int n = 0;
	if( c < 0x80 ){
		zBuf[n++] = (unsigned char)c;
	}else if( c < 0x800 ){
		zBuf[n++] = (unsigned char)(0xC0 | (c >> 6));
		zBuf[n++] = (unsigned char)(0x80 | (c & 0x3F));
	}else if( c < 0x10000 ){
		zBuf[n++] = (unsigned char)(0xE0 | (c >> 12));
		zBuf[n++] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
		zBuf[n++] = (unsigned char)(0x80 | (c & 0x3F));
	}else{
		zBuf[n++] = (unsigned char)(0xF0 | (c >> 18));
		zBuf[n++] = (unsigned char)(0x80 | ((c >> 12) & 0x3F));
		zBuf[n++] = (unsigned char)(0x80 | ((c >> 6) & 0x3F));
		zBuf[n++] = (unsigned char)(0x80 | (c & 0x3F));
	}
	SyBlobAppend(pOut,(const void *)zBuf,(sxu32)n);
}
/*
 * One source byte into a name, an attribute, a comment or a text-state run.
 * Every state but the data one REPLACES a NUL rather than passing it on: the
 * byte terminates a C string, so a tag name or a comment that kept one would
 * be truncated at it rather than carry it, and php's answer everywhere here
 * is the replacement character.
 */
static void Html5PutByte(SyBlob *pOut,int c)
{
	char zc = (char)c;
	if( zc != 0 ){
		SyBlobAppend(pOut,&zc,1);
	}else{
		Html5PutUtf8(pOut,0xFFFD);
	}
}
/*
 * The numeric reference's replacement table: the C1 block is not what the
 * bytes say it is, and every other refused value is the replacement character.
 */
static unsigned int Html5NumRepl(unsigned int c)
{
	static const unsigned int aC1[32] = {
		0x20AC,0x0081,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,
		0x02C6,0x2030,0x0160,0x2039,0x0152,0x008D,0x017D,0x008F,
		0x0090,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
		0x02DC,0x2122,0x0161,0x203A,0x0153,0x009D,0x017E,0x0178
	};
	if( c >= 0x80 && c <= 0x9F ){
		return aC1[c - 0x80];
	}
	if( c == 0 || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF) ){
		return 0xFFFD;
	}
	return c;
}
/*
 * A character reference at the cursor, which has NOT yet consumed the `&`.
 * Anything that does not resolve is the literal text it was spelled with,
 * which is why the whole thing is written as "copy on failure".
 */
static void Html5CharRef(html5_parser *p,SyBlob *pOut,int bAttr)
{
	sxu32 iSave = p->iPos;
	sxu32 iLine = p->iLine,iCol = p->iCol;
	int c;
	Html5Read(p);  /* the '&' */
	c = Html5Peek(p,0);
	if( c == '#' ){
		unsigned int iVal = 0;
		int nDig = 0,bHex = 0;
		Html5Read(p);
		c = Html5Peek(p,0);
		if( c == 'x' || c == 'X' ){
			bHex = 1;
			Html5Read(p);
		}
		for(;;){
			c = Html5Peek(p,0);
			if( c < 0 ){
				break;
			}
			if( c >= '0' && c <= '9' ){
				iVal = iVal * (bHex ? 16 : 10) + (unsigned int)(c - '0');
			}else if( bHex && Html5Lower(c) >= 'a' && Html5Lower(c) <= 'f' ){
				iVal = iVal * 16 + (unsigned int)(Html5Lower(c) - 'a' + 10);
			}else{
				break;
			}
			if( iVal > 0x10FFFF ){
				iVal = 0x110000;   /* clamped; the table answers FFFD for it */
			}
			nDig++;
			Html5Read(p);
		}
		if( nDig < 1 ){
			p->iPos = iSave;
			p->iLine = iLine;
			p->iCol = iCol;
			Html5Read(p);
			SyBlobAppend(pOut,"&",1);
			return;
		}
		if( Html5Peek(p,0) == ';' ){
			Html5Read(p);
		}
		Html5PutUtf8(pOut,Html5NumRepl(iVal));
		return;
	}
	/*
	 * A named reference.  libxml's table is the HTML4 name set and is keyed by
	 * the bare name, so the longest run of name characters is looked up whole:
	 * the spec's longest-match walk over a 2231-name table is not reachable
	 * from here, and a name this table does not hold stays literal text.
	 */
	{
		char zName[32];
		int n = 0;
		sxu32 cEnt;
		for(;;){
			c = Html5Peek(p,(sxu32)n);
			if( c < 0 || n >= (int)sizeof(zName) - 1 ){
				break;
			}
			if( !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			   || (c >= '0' && c <= '9')) ){
				break;
			}
			zName[n++] = (char)c;
		}
		zName[n] = 0;
		c = Html5Peek(p,(sxu32)n);
		/*
		 * In an attribute a reference that states no `;` and is followed by a
		 * name character or `=` is NOT one: that rule is what keeps a query
		 * string's `?a=1&copy=2` from becoming a copyright sign.
		 */
		if( n > 0 && (c == ';' || (!bAttr && c != '=')) ){
			cEnt = Html5EntLookup(zName);
			if( cEnt == 0 && c == ';' ){
				Html5Err(p,"tokenizer","unknown-named-character-reference",
					iLine,iCol + (sxu32)n + 1,iCol + (sxu32)n + 1);
			}
			if( cEnt ){
				Html5Skip(p,(sxu32)n);
				if( Html5Peek(p,0) == ';' ){
					Html5Read(p);
				}
				Html5PutUtf8(pOut,(unsigned int)cEnt);
				return;
			}
		}
	}
	SyBlobAppend(pOut,"&",1);
}
/* The attribute states, entered with the cursor on the first name character. */
static void Html5ReadAttrs(html5_parser *p)
{
	for(;;){
		html5_attr sAttr;
		int c;
		while( Html5IsSpace(Html5Peek(p,0)) ){
			Html5Read(p);
		}
		c = Html5Peek(p,0);
		if( c < 0 || c == '>' ){
			break;
		}
		if( c == '/' ){
			Html5Read(p);
			if( Html5Peek(p,0) == '>' ){
				p->bSelfClose = 1;
			}
			continue;
		}
		sAttr.nNameOfs = SyBlobLength(&p->sAttrBuf);
		for(;;){
			c = Html5Peek(p,0);
			if( c < 0 || Html5IsSpace(c) || c == '>' || c == '=' || c == '/' ){
				break;
			}
			Html5PutByte(&p->sAttrBuf,Html5Lower(Html5Read(p)));
		}
		Html5EndStr(&p->sAttrBuf);
		while( Html5IsSpace(Html5Peek(p,0)) ){
			Html5Read(p);
		}
		sAttr.nValOfs = SyBlobLength(&p->sAttrBuf);
		if( Html5Peek(p,0) == '=' ){
			int qc;
			Html5Read(p);
			while( Html5IsSpace(Html5Peek(p,0)) ){
				Html5Read(p);
			}
			qc = Html5Peek(p,0);
			if( qc == '"' || qc == '\'' ){
				Html5Read(p);
				for(;;){
					c = Html5Peek(p,0);
					if( c < 0 || c == qc ){
						if( c == qc ){
							Html5Read(p);
						}
						break;
					}
					if( c == '&' ){
						Html5CharRef(p,&p->sAttrBuf,TRUE);
					}else{
						Html5PutByte(&p->sAttrBuf,Html5Read(p));
					}
				}
			}else{
				for(;;){
					c = Html5Peek(p,0);
					if( c < 0 || Html5IsSpace(c) || c == '>' ){
						break;
					}
					if( c == '&' ){
						Html5CharRef(p,&p->sAttrBuf,TRUE);
					}else{
						Html5PutByte(&p->sAttrBuf,Html5Read(p));
					}
				}
			}
		}
		Html5EndStr(&p->sAttrBuf);
		SySetPut(&p->sAttr,(const void *)&sAttr);
	}
	if( Html5Peek(p,0) == '>' ){
		Html5Read(p);
	}
}
/* The comment states: everything to `-->`, or to the end of the source. */
static void Html5ReadComment(html5_parser *p)
{
	for(;;){
		int c = Html5Peek(p,0);
		if( c < 0 ){
			break;
		}
		if( c == '-' && Html5Peek(p,1) == '-' && Html5Peek(p,2) == '>' ){
			Html5Skip(p,3);
			break;
		}
		Html5PutByte(&p->sBuf,Html5Read(p));
	}
	p->iTok = HTML5_TOK_COMMENT;
}
/* A bogus comment: everything to the next `>` is the comment's data. */
static void Html5BogusComment(html5_parser *p)
{
	for(;;){
		int c = Html5Peek(p,0);
		if( c < 0 ){
			break;
		}
		if( c == '>' ){
			Html5Read(p);
			break;
		}
		Html5PutByte(&p->sBuf,Html5Read(p));
	}
	p->iTok = HTML5_TOK_COMMENT;
}
/*
 * The text-only tokenizer the tree constructor arms by name: one text token
 * running to the matching end tag, which is then the next token -- or, when
 * zEnd is 0, to end of file, because `<plaintext>` has no end tag.
 *
 * Unlike the data state, these states do not DROP a NUL: the spec replaces it
 * with U+FFFD here, and the byte is observable in the text node that results.
 */
static void Html5ReadText(html5_parser *p,const char *zEnd,int bRcdata)
{
	for(;;){
		int c = Html5Peek(p,0);
		if( c < 0 ){
			break;
		}
		if( zEnd && c == '<' && Html5Peek(p,1) == '/' ){
			sxu32 n = (sxu32)SyStrlen(zEnd);
			sxu32 i;
			int bMatch = 1;
			for( i = 0 ; i < n ; ++i ){
				int d = Html5Peek(p,2 + i);
				if( d < 0 || Html5Lower(d) != (int)(unsigned char)zEnd[i] ){
					bMatch = 0;
					break;
				}
			}
			if( bMatch ){
				int d = Html5Peek(p,2 + n);
				if( d < 0 || d == '>' || d == '/' || Html5IsSpace(d) ){
					break;
				}
			}
		}
		if( bRcdata && c == '&' ){
			Html5CharRef(p,&p->sBuf,FALSE);
			continue;
		}
		Html5PutByte(&p->sBuf,Html5Read(p));
	}
	p->iTok = HTML5_TOK_TEXT;
}
/*
 * One token.  The tree constructor drives this and may arm p->iText first,
 * which takes the text-only path above instead of the data state.
 */
static void Html5NextToken(html5_parser *p)
{
	int c;
	p->iTok = HTML5_TOK_EOF;
	p->bSelfClose = 0;
	p->bTextTok = 0;
	SyBlobReset(&p->sBuf);
	SyBlobReset(&p->sAttrBuf);
	SySetReset(&p->sAttr);
	if( p->iText != HTML5_TEXT_NONE ){
		char zEnd[32];
		int nEnd = (int)SyBlobLength(&p->sName);
		int bRc = p->iText == HTML5_TEXT_RCDATA;
		int bPlain = p->iText == HTML5_TEXT_PLAIN;
		if( nEnd > (int)sizeof(zEnd) - 1 ){
			nEnd = (int)sizeof(zEnd) - 1;
		}
		SyMemcpy(SyBlobData(&p->sName),zEnd,(sxu32)nEnd);
		zEnd[nEnd] = 0;
		p->iText = HTML5_TEXT_NONE;
		if( bPlain || SyBlobLength(&p->sName) > 0 ){
			Html5ReadText(p,bPlain ? 0 : zEnd,bRc);
			if( SyBlobLength(&p->sBuf) > 0 ){
				p->bTextTok = 1;
				return;
			}
		}
	}
	SyBlobReset(&p->sName);
	c = Html5Peek(p,0);
	if( c < 0 ){
		return;
	}
	if( c != '<' ){
		/* The data state: text to the next `<`, references resolved. */
		for(;;){
			c = Html5Peek(p,0);
			if( c < 0 || c == '<' ){
				break;
			}
			if( c == '&' ){
				Html5CharRef(p,&p->sBuf,FALSE);
				continue;
			}
			{
				sxu32 iLine = p->iLine,iCol = p->iCol;
				char zc = (char)Html5Read(p);
				/* A NUL is a parse error and never reaches the tree. */
				if( zc != 0 ){
					SyBlobAppend(&p->sBuf,&zc,1);
				}else{
					Html5Err(p,"tokenizer","unexpected-null-character",
						iLine,iCol,iCol);
				}
			}
		}
		p->iTok = HTML5_TOK_TEXT;
		return;
	}
	c = Html5Peek(p,1);
	if( c == '!' ){
		Html5Skip(p,2);
		if( Html5Peek(p,0) == '-' && Html5Peek(p,1) == '-' ){
			Html5Skip(p,2);
			Html5ReadComment(p);
			return;
		}
		/*
		 * `<![CDATA[` is a bogus COMMENT in HTML and a run of character data
		 * in foreign content -- the one question the tokenizer has to ask the
		 * tree, because the same bytes mean two different things depending on
		 * what is open.  The run is raw: no character reference is resolved
		 * inside it.
		 */
		if( Html5NsKind(Html5Top(p)) != HTML5_NSK_HTML
		 && Html5Peek(p,0) == '[' && Html5Peek(p,1) == 'C'
		 && Html5Peek(p,2) == 'D' && Html5Peek(p,3) == 'A'
		 && Html5Peek(p,4) == 'T' && Html5Peek(p,5) == 'A'
		 && Html5Peek(p,6) == '[' ){
			Html5Skip(p,7);
			for(;;){
				int d = Html5Peek(p,0);
				char zc;
				if( d < 0 ){
					break;
				}
				if( d == ']' && Html5Peek(p,1) == ']' && Html5Peek(p,2) == '>' ){
					Html5Skip(p,3);
					break;
				}
				zc = (char)Html5Read(p);
				SyBlobAppend(&p->sBuf,&zc,1);
			}
			p->iTok = HTML5_TOK_TEXT;
			return;
		}
		if( Html5LookWord(p,"DOCTYPE") ){
			Html5Skip(p,7);
			while( Html5IsSpace(Html5Peek(p,0)) ){
				Html5Read(p);
			}
			for(;;){
				int d = Html5Peek(p,0);
				if( d < 0 || Html5IsSpace(d) || d == '>' ){
					break;
				}
				Html5PutByte(&p->sName,Html5Lower(Html5Read(p)));
			}
			SyBlobNullAppend(&p->sName);
			/* The public and system identifiers are read but not kept: php
			 * prints neither for an HTML document, and the doctype node it
			 * builds carries only the name. */
			for(;;){
				int d = Html5Peek(p,0);
				if( d < 0 || d == '>' ){
					if( d == '>' ){
						Html5Read(p);
					}
					break;
				}
				Html5Read(p);
			}
			p->iTok = HTML5_TOK_DOCTYPE;
			return;
		}
		Html5Err(p,"tokenizer","incorrectly-opened-comment",
			p->iLine,p->iCol,p->iCol);
		Html5BogusComment(p);
		return;
	}
	if( c == '?' ){
		Html5Read(p);
		Html5Err(p,"tokenizer","unexpected-question-mark-instead-of-tag-name",
			p->iLine,p->iCol,p->iCol);
		Html5BogusComment(p);
		return;
	}
	if( c == '/' ){
		int d = Html5Peek(p,2);
		if( d < 0 ){
			/* `</` at the end of the source is literal text. */
			SyBlobAppend(&p->sBuf,"</",2);
			Html5Skip(p,2);
			p->iTok = HTML5_TOK_TEXT;
			return;
		}
		if( !((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z')) ){
			Html5Skip(p,2);
			Html5BogusComment(p);
			return;
		}
		Html5Skip(p,2);
		p->iTok = HTML5_TOK_END;
	}else if( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ){
		Html5Read(p);
		p->iTok = HTML5_TOK_START;
	}else{
		/* `<` that opens nothing is the character it is. */
		Html5Read(p);
		SyBlobAppend(&p->sBuf,"<",1);
		p->iTok = HTML5_TOK_TEXT;
		return;
	}
	p->iTokLine = p->iLine;
	p->iTokCol = p->iCol;
	for(;;){
		int d = Html5Peek(p,0);
		if( d < 0 || Html5IsSpace(d) || d == '>' || d == '/' ){
			break;
		}
		Html5PutByte(&p->sName,Html5Lower(Html5Read(p)));
	}
	p->iTokCol2 = p->iCol > p->iTokCol ? p->iCol - 1 : p->iTokCol;
	SyBlobNullAppend(&p->sName);
	Html5ReadAttrs(p);
}

/* ------------------------------------------------------------------ *
 * The tree constructor
 * ------------------------------------------------------------------ */

static const char * Html5TokName(html5_parser *p)
{
	return SyBlobLength(&p->sName) > 0 ? (const char *)SyBlobData(&p->sName) : "";
}
static xmlNodePtr Html5Top(html5_parser *p)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	sxu32 n = SySetUsed(&p->sOpen);
	return n > 0 ? apStack[n - 1] : 0;
}
/* Where a node goes: the innermost open element, or the document itself. */
static xmlNodePtr Html5Target(html5_parser *p)
{
	xmlNodePtr pTop = Html5Top(p);
	return pTop ? pTop : (xmlNodePtr)p->pDoc;
}
static void Html5Push(html5_parser *p,xmlNodePtr pNode)
{
	SySetPut(&p->sOpen,(const void *)&pNode);
}
static void Html5FmtClearToMarker(html5_parser *p);
static void Html5Pop(html5_parser *p)
{
	xmlNodePtr pTop = Html5Top(p);
	SySetPop(&p->sOpen);
	if( pTop && HTML5_IN(azHtml5FmtMark,(const char *)pTop->name) ){
		Html5FmtClearToMarker(p);
	}
}
/* Where `pEl` sits in the open stack, counted from the root, or -1. */
static int Html5StackIndex(html5_parser *p,xmlNodePtr pEl)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int i,n = (int)SySetUsed(&p->sOpen);
	for( i = 0 ; i < n ; ++i ){
		if( apStack[i] == pEl ){
			return i;
		}
	}
	return -1;
}
/*
 * The adoption moves entries about in the middle of both lists, which is the
 * one thing SySet has no verb for; its slots are contiguous, so a shift is the
 * whole of it.
 */
static void Html5SetRemoveAt(SySet *pSet,int i)
{
	xmlNodePtr *ap = (xmlNodePtr *)SySetBasePtr(pSet);
	int n = (int)SySetUsed(pSet);
	if( i < 0 || i >= n ){
		return;
	}
	for( ; i + 1 < n ; ++i ){
		ap[i] = ap[i + 1];
	}
	pSet->nUsed = (sxu32)(n - 1);
}
static void Html5SetInsertAt(SySet *pSet,int i,xmlNodePtr pEl)
{
	xmlNodePtr *ap;
	int j,n;
	SySetPut(pSet,(const void *)&pEl);       /* grow by one, then shift */
	ap = (xmlNodePtr *)SySetBasePtr(pSet);
	n = (int)SySetUsed(pSet);
	if( i < 0 ){
		i = 0;
	}
	if( i > n - 1 ){
		return;
	}
	for( j = n - 1 ; j > i ; --j ){
		ap[j] = ap[j - 1];
	}
	ap[i] = pEl;
}
/* Is `zName` open?  Answers the depth from the top, or -1. */
static int Html5OpenDepth(html5_parser *p,const char *zName)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	sxu32 n = SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( Html5Eq((const char *)apStack[n]->name,zName) ){
			return (int)(SySetUsed(&p->sOpen) - 1 - n);
		}
	}
	return -1;
}
/* Pop everything down to and including the innermost `zName`, if it is open. */
static void Html5PopTo(html5_parser *p,const char *zName)
{
	int iDepth = Html5OpenDepth(p,zName);
	if( iDepth < 0 ){
		return;
	}
	while( iDepth-- >= 0 ){
		Html5Pop(p);
	}
}
/*
 * The declaration for `zHref` that `pEl` should carry: the innermost one
 * already in scope above it, or a fresh one minted on the element itself.  A
 * foreign subtree therefore states its namespace once, on the `<svg>` or
 * `<math>` that opened it, and every descendant points at that same one.
 */
static xmlNsPtr Html5NsGet(html5_parser *p,xmlNodePtr pEl,const char *zHref,
	const char *zPrefix)
{
	xmlNsPtr pNs = xmlSearchNsByHref(p->pDoc,pEl,(const xmlChar *)zHref);
	if( pNs == 0 ){
		pNs = xmlNewNs(pEl,(const xmlChar *)zHref,(const xmlChar *)zPrefix);
	}
	return pNs;
}
/*
 * Is one of the table's own insertion modes running?  The question is about
 * the CURRENT node rather than about a table being open anywhere: content
 * inside a `<td>` -- or inside an element already fostered out -- nests
 * normally, and only what lands directly in a table, a row group or a row is
 * misplaced.
 */
static int Html5InTableCtx(html5_parser *p)
{
	xmlNodePtr pTop = Html5Top(p);
	const char *z;
	if( pTop == 0 ){
		return 0;
	}
	z = (const char *)pTop->name;
	return Html5Eq(z,"table") || Html5Eq(z,"tbody") || Html5Eq(z,"thead")
		|| Html5Eq(z,"tfoot") || Html5Eq(z,"tr");
}
/*
 * Is a table open ANYWHERE on the stack?  Html5InTableCtx asks only about the
 * top, which is the right question for fostering and the wrong one here: inside
 * a `<td>` the top is the cell, and a stray `<td>` written there opens the next
 * cell rather than being dropped.
 */
static int Html5TableOpen(html5_parser *p)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	sxu32 n = SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		/*
		 * A template is a table context of its own: the spec answers a `<tr>`
		 * or a `<td>` written inside one by pushing `in table body` or `in
		 * row` whether or not a real table is anywhere, so the row survives
		 * and is not dropped as a table-only tag written outside a table.
		 */
		if( Html5Eq((const char *)apStack[n]->name,"template") ){
			return 1;
		}
		if( Html5Eq((const char *)apStack[n]->name,"table") ){
			return 1;
		}
	}
	return 0;
}
/*
 * The node the fostered content goes immediately before: the innermost open
 * table.  A table with no parent has nowhere earlier to put anything, so the
 * caller falls back to an ordinary insertion -- the spec reaches for the
 * element under the table on the stack, which under NOIMPLIED may not exist.
 */
static xmlNodePtr Html5FosterBefore(html5_parser *p)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	sxu32 n = SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( Html5Eq((const char *)apStack[n]->name,"template") ){
			return 0;
		}
		if( Html5Eq((const char *)apStack[n]->name,"table") ){
			return apStack[n]->parent ? apStack[n] : 0;
		}
	}
	return 0;
}
/*
 * Attach `pNode` where this token's content belongs.  Fostering needs BOTH
 * halves: the token has to be one the table modes hand to `in body`, and the
 * place it would land has to still be the table itself -- a second formatting
 * clone in one reconstruction nests inside the first, which is no longer a
 * table.
 *
 * Both insertion paths owe the same invariant -- no two adjacent text nodes --
 * and only the ordinary one gets it for free.  xmlAddChild merges a text node
 * into the parent's last child; xmlAddPrevSibling merges only when the node it
 * inserts BEFORE is itself text, because it compares `cur->name` against
 * `cur->prev->name`.  The foster target is always the open TABLE element, so
 * that test never fires here and every run fostered out past a table landed as
 * its own node: `<table>a<td>b</td>c</table>` left `a` and `c` side by side
 * where the spec -- which merges on the insertion POSITION, appending to the
 * node immediately before it when that node is text -- leaves `ac`.  The text
 * already sitting before the table is the same position, so `<div>d<table>a`
 * merges into `d` rather than opening a second node beside it.
 */
static void Html5Attach(html5_parser *p,xmlNodePtr pNode)
{
	xmlNodePtr pBefore = 0;
	if( pNode == 0 ){
		return;
	}
	if( p->bFoster && Html5InTableCtx(p) ){
		pBefore = Html5FosterBefore(p);
	}
	if( pBefore ){
		if( pNode->type == XML_TEXT_NODE && pBefore->prev
		 && pBefore->prev->type == XML_TEXT_NODE ){
			/* libxml's own merge, which is what xmlAddPrevSibling runs when it
			 * does decide to merge -- so the dict-allocated content case is
			 * handled the way every other text append in this tree handles it. */
			xmlNodeAddContent(pBefore->prev,pNode->content);
			xmlFreeNode(pNode);
			return;
		}
		xmlAddPrevSibling(pBefore,pNode);
	}else{
		xmlAddChild(Html5Target(p),pNode);
	}
}
static xmlNodePtr Html5NewElemNs(html5_parser *p,const char *zName,const char *zHref)
{
	xmlNodePtr pParent = Html5Target(p);
	xmlNodePtr pEl = xmlNewDocNode(p->pDoc,0,(const xmlChar *)zName,0);
	if( pEl == 0 ){
		return 0;
	}
	if( zHref ){
		/* Attached first: the declaration in scope is only findable from a
		 * node that is already under the element that states it. */
		Html5Attach(p,pEl);
		xmlSetNs(pEl,Html5NsGet(p,pEl,zHref,0));
		return pEl;
	}
	if( (p->iFlags & HTML5_NO_DEF_NS) == 0 ){
		/*
		 * The declaration belongs to the root of the tree being built, so a
		 * document with one root states it once.  Under NOIMPLIED there can be
		 * several roots and each states its own -- there is no node above them
		 * that could hold one for all.
		 */
		if( pParent == (xmlNodePtr)p->pDoc || p->pNs == 0 ){
			p->pNs = xmlNewNs(pEl,(const xmlChar *)HTML5_NS,0);
		}
		xmlSetNs(pEl,p->pNs);
	}
	Html5Attach(p,pEl);
	return pEl;
}
static xmlNodePtr Html5NewElem(html5_parser *p,const char *zName)
{
	return Html5NewElemNs(p,zName,0);
}
static void Html5AddAttrs(html5_parser *p,xmlNodePtr pEl)
{
	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);
	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);
	sxu32 i,n = SySetUsed(&p->sAttr);
	for( i = 0 ; i < n ; ++i ){
		const char *zName = zBuf + aAttr[i].nNameOfs;
		const char *zVal = zBuf + aAttr[i].nValOfs;
		if( zName[0] == 0 ){
			continue;
		}
		/* A repeated attribute is the FIRST one; the rest are dropped. */
		if( xmlHasProp(pEl,(const xmlChar *)zName) ){
			continue;
		}
		xmlNewProp(pEl,(const xmlChar *)zName,(const xmlChar *)zVal);
	}
}
/*
 * A detached copy of a formatting element: same name, same attributes, no
 * children.  Both the reconstruction and the adoption mint one, because the
 * spec builds them from the TOKEN the original was created for and the token
 * is long gone by then -- the element it built is the only record of it.
 */
static xmlNodePtr Html5CloneFmt(html5_parser *p,xmlNodePtr pSrc)
{
	xmlNodePtr pEl = xmlNewDocNode(p->pDoc,0,pSrc->name,0);
	xmlAttrPtr pAttr;
	if( pEl == 0 ){
		return 0;
	}
	if( (p->iFlags & HTML5_NO_DEF_NS) == 0 && p->pNs ){
		xmlSetNs(pEl,p->pNs);
	}
	for( pAttr = pSrc->properties ; pAttr ; pAttr = pAttr->next ){
		xmlChar *zVal = xmlNodeListGetString(p->pDoc,pAttr->children,1);
		xmlNewProp(pEl,pAttr->name,zVal ? zVal : (const xmlChar *)"");
		if( zVal ){
			xmlFree(zVal);
		}
	}
	return pEl;
}
/* ------------------------------------------------------------------ *
 * The list of active formatting elements
 * ------------------------------------------------------------------ */

static int Html5FmtIndex(html5_parser *p,xmlNodePtr pEl)
{
	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);
	int i,n = (int)SySetUsed(&p->sFmt);
	for( i = 0 ; i < n ; ++i ){
		if( apFmt[i] == pEl ){
			return i;
		}
	}
	return -1;
}
static void Html5FmtRemove(html5_parser *p,xmlNodePtr pEl)
{
	Html5SetRemoveAt(&p->sFmt,Html5FmtIndex(p,pEl));
}
static void Html5FmtMarker(html5_parser *p)
{
	xmlNodePtr pNull = 0;
	SySetPut(&p->sFmt,(const void *)&pNull);
}
static void Html5FmtClearToMarker(html5_parser *p)
{
	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);
	int n = (int)SySetUsed(&p->sFmt);
	while( n-- > 0 ){
		if( apFmt[n] == 0 ){
			break;
		}
	}
	if( n >= 0 ){
		p->sFmt.nUsed = (sxu32)n;      /* the marker goes with them */
	}
}
/* The innermost entry named `zName` after the last marker, or 0. */
static xmlNodePtr Html5FmtLast(html5_parser *p,const char *zName)
{
	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);
	int n = (int)SySetUsed(&p->sFmt);
	while( n-- > 0 ){
		if( apFmt[n] == 0 ){
			break;
		}
		if( Html5Eq((const char *)apFmt[n]->name,zName) ){
			return apFmt[n];
		}
	}
	return 0;
}
/* Same name and same attributes -- the Noah's Ark clause asks both. */
static int Html5SameFmt(xmlNodePtr pLeft,xmlNodePtr pRight)
{
	xmlAttrPtr pA,pB;
	int nLeft = 0,nRight = 0;
	if( !Html5Eq((const char *)pLeft->name,(const char *)pRight->name) ){
		return 0;
	}
	for( pA = pLeft->properties ; pA ; pA = pA->next ){
		nLeft++;
	}
	for( pB = pRight->properties ; pB ; pB = pB->next ){
		nRight++;
	}
	if( nLeft != nRight ){
		return 0;
	}
	for( pA = pLeft->properties ; pA ; pA = pA->next ){
		xmlChar *zL,*zR;
		int bSame;
		if( xmlHasProp(pRight,pA->name) == 0 ){
			return 0;
		}
		zL = xmlNodeListGetString(pLeft->doc,pA->children,1);
		zR = xmlGetProp(pRight,pA->name);
		bSame = zL && zR && Html5Eq((const char *)zL,(const char *)zR);
		if( zL ){
			xmlFree(zL);
		}
		if( zR ){
			xmlFree(zR);
		}
		if( !bSame ){
			return 0;
		}
	}
	return 1;
}
/*
 * Push a formatting element, under the Noah's Ark clause: three entries that
 * agree on name and attributes are the most the list may hold after its last
 * marker, and a fourth evicts the outermost of them.
 */
static void Html5FmtPush(html5_parser *p,xmlNodePtr pEl)
{
	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);
	int i,n = (int)SySetUsed(&p->sFmt);
	int nSame = 0,iFirst = -1;
	for( i = n - 1 ; i >= 0 ; --i ){
		if( apFmt[i] == 0 ){
			break;
		}
		if( Html5SameFmt(apFmt[i],pEl) ){
			nSame++;
			iFirst = i;
		}
	}
	if( nSame >= 3 && iFirst >= 0 ){
		Html5SetRemoveAt(&p->sFmt,iFirst);
	}
	SySetPut(&p->sFmt,(const void *)&pEl);
}
/*
 * Reconstruct: every entry after the last one that is still open is reopened,
 * innermost last, as a fresh element in the current position.  This is what
 * puts the `<i>` back around the text after `<b><i>x</b>y`.
 */
/*
 * An open `<select>` is the spec's own insertion mode, and that mode takes
 * neither a reconstruction nor an adoption: everything it does not recognise
 * is ignored where it stands.  This parser reads the stack instead of carrying
 * the mode, so both doors ask here.
 */
static int Html5InSelect(html5_parser *p)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int n = (int)SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( Html5Eq((const char *)apStack[n]->name,"select") ){
			return 1;
		}
	}
	return 0;
}
static void Html5Reconstruct(html5_parser *p)
{
	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);
	int i,n = (int)SySetUsed(&p->sFmt);
	if( n < 1 || Html5InSelect(p) ){
		return;
	}
	i = n - 1;
	if( apFmt[i] == 0 || Html5StackIndex(p,apFmt[i]) >= 0 ){
		return;
	}
	while( i > 0 ){
		i--;
		if( apFmt[i] == 0 || Html5StackIndex(p,apFmt[i]) >= 0 ){
			i++;
			break;
		}
	}
	for( ; i < n ; ++i ){
		xmlNodePtr pNew = Html5CloneFmt(p,apFmt[i]);
		if( pNew == 0 ){
			break;
		}
		Html5Attach(p,pNew);
		Html5Push(p,pNew);
		apFmt[i] = pNew;
	}
}
/* Is `pEl` open with no scope marker between it and the current node? */
static int Html5InScope(html5_parser *p,xmlNodePtr pEl)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int n = (int)SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( apStack[n] == pEl ){
			return 1;
		}
		if( HTML5_IN(azHtml5Scope,(const char *)apStack[n]->name) ){
			return 0;
		}
	}
	return 0;
}
/*
 * The adoption agency algorithm.  Answers 0 when the name is not on the list
 * at all, which is the caller's cue to treat the end tag as any other.
 *
 * The outer loop is not a formality: one pass moves ONE block out of the
 * formatting element, and a `<b><div><i>x</b>` needs two -- the first to lift
 * the `<div>` out and the second to retire the copy the first left behind.
 */
static int Html5Adoption(html5_parser *p,const char *zName)
{
	xmlNodePtr pTop = Html5Top(p);
	int iOuter;
	if( Html5InSelect(p) ){
		return 1;
	}
	if( pTop && Html5Eq((const char *)pTop->name,zName)
	 && Html5FmtIndex(p,pTop) < 0 ){
		Html5Pop(p);
		return 1;
	}
	for( iOuter = 0 ; iOuter < 8 ; ++iOuter ){
		xmlNodePtr *apStack;
		xmlNodePtr pFmt,pFurthest = 0,pAncestor,pNode,pLast,pNew,pChild;
		int iFmtStack,iBookmark,iInner,iNode,i,n;
		pFmt = Html5FmtLast(p,zName);
		if( pFmt == 0 ){
			return 0;
		}
		iFmtStack = Html5StackIndex(p,pFmt);
		if( iFmtStack < 0 ){
			Html5FmtRemove(p,pFmt);
			return 1;
		}
		if( !Html5InScope(p,pFmt) ){
			return 1;
		}
		apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
		n = (int)SySetUsed(&p->sOpen);
		for( i = iFmtStack + 1 ; i < n ; ++i ){
			if( HTML5_IN(azHtml5Special,(const char *)apStack[i]->name) ){
				pFurthest = apStack[i];
				break;
			}
		}
		if( pFurthest == 0 ){
			/* Nothing block-level is caught inside it: just close it. */
			while( (int)SySetUsed(&p->sOpen) > iFmtStack ){
				Html5Pop(p);
			}
			Html5FmtRemove(p,pFmt);
			return 1;
		}
		pAncestor = iFmtStack > 0 ? apStack[iFmtStack - 1] : 0;
		iBookmark = Html5FmtIndex(p,pFmt);
		pLast = pFurthest;
		iNode = Html5StackIndex(p,pFurthest);
		for( iInner = 1 ; ; ++iInner ){
			if( --iNode < 0 ){
				break;
			}
			pNode = ((xmlNodePtr *)SySetBasePtr(&p->sOpen))[iNode];
			if( pNode == pFmt ){
				break;
			}
			if( iInner > 3 ){
				Html5FmtRemove(p,pNode);
			}
			if( Html5FmtIndex(p,pNode) < 0 ){
				Html5SetRemoveAt(&p->sOpen,iNode);
				continue;
			}
			pNew = Html5CloneFmt(p,pNode);
			if( pNew == 0 ){
				break;
			}
			((xmlNodePtr *)SySetBasePtr(&p->sFmt))[Html5FmtIndex(p,pNode)] = pNew;
			((xmlNodePtr *)SySetBasePtr(&p->sOpen))[iNode] = pNew;
			if( pLast == pFurthest ){
				iBookmark = Html5FmtIndex(p,pNew) + 1;
			}
			xmlUnlinkNode(pLast);
			xmlAddChild(pNew,pLast);
			pLast = pNew;
		}
		xmlUnlinkNode(pLast);
		xmlAddChild(pAncestor ? pAncestor : (xmlNodePtr)p->pDoc,pLast);
		pNew = Html5CloneFmt(p,pFmt);
		if( pNew == 0 ){
			return 1;
		}
		pChild = pFurthest->children;
		while( pChild ){
			xmlNodePtr pNext = pChild->next;
			xmlUnlinkNode(pChild);
			xmlAddChild(pNew,pChild);
			pChild = pNext;
		}
		xmlAddChild(pFurthest,pNew);
		i = Html5FmtIndex(p,pFmt);
		if( i >= 0 ){
			Html5SetRemoveAt(&p->sFmt,i);
			if( iBookmark > i ){
				iBookmark--;
			}
		}
		Html5SetInsertAt(&p->sFmt,iBookmark,pNew);
		i = Html5StackIndex(p,pFmt);
		if( i >= 0 ){
			Html5SetRemoveAt(&p->sOpen,i);
		}
		Html5SetInsertAt(&p->sOpen,Html5StackIndex(p,pFurthest) + 1,pNew);
	}
	return 1;
}
/* Does a start tag of this name reconstruct before it is inserted? */
static int Html5Reconstructs(const char *zName)
{
	return !HTML5_IN(azHtml5Special,zName)
		|| HTML5_IN(azHtml5ReconSpecial,zName);
}
/*
 * The spec's `any other end tag`, which is where a formatting name lands when
 * the list does not hold it: walk out from the current node, close the first
 * element of that name, and STOP at the first special one -- a `</b>` spelled
 * inside a `<td>` whose `<b>` is outside it closes nothing at all.
 */
static void Html5EndTagOther(html5_parser *p,const char *zName)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int n = (int)SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( Html5Eq((const char *)apStack[n]->name,zName) ){
			while( (int)SySetUsed(&p->sOpen) > n ){
				Html5Pop(p);
			}
			return;
		}
		if( HTML5_IN(azHtml5Special,(const char *)apStack[n]->name) ){
			return;
		}
	}
}
/* Is an element of this name open, with no scope marker above it? */
static int Html5NameInScope(html5_parser *p,const char *zName)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int n = (int)SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		if( Html5Eq((const char *)apStack[n]->name,zName) ){
			return 1;
		}
		if( HTML5_IN(azHtml5Scope,(const char *)apStack[n]->name) ){
			return 0;
		}
	}
	return 0;
}
/* The current start tag's value for `zName`, or a NULL when it has none. */
static const char * Html5TokAttr(html5_parser *p,const char *zName)
{
	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);
	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);
	sxu32 i,n = SySetUsed(&p->sAttr);
	for( i = 0 ; i < n ; ++i ){
		if( Html5Eq(zBuf + aAttr[i].nNameOfs,zName) ){
			return zBuf + aAttr[i].nValOfs;
		}
	}
	return 0;
}
/*
 * Does a table's own insertion mode handle this start tag, so that it stays
 * inside the table?  `<input>` is the one answered by an attribute: a hidden
 * one is table furniture, any other is content and leaves.
 */
static int Html5TableOwns(html5_parser *p,const char *zName)
{
	if( Html5Eq(zName,"input") ){
		const char *zType = Html5TokAttr(p,"type");
		return zType && Html5Eq(zType,"hidden");
	}
	return Html5Eq(zName,"form") || HTML5_IN(azHtml5TableOwn,zName);
}
/* Insert the current start tag, pushing it unless it takes no children. */
static xmlNodePtr Html5InsertStart(html5_parser *p)
{
	const char *zName = Html5TokName(p);
	xmlNodePtr pEl = Html5NewElem(p,zName);
	if( pEl == 0 ){
		return 0;
	}
	Html5AddAttrs(p,pEl);
	if( HTML5_IN(azHtml5Void,zName) ){
		return pEl;
	}
	Html5Push(p,pEl);
	if( HTML5_IN(azHtml5Rcdata,zName) ){
		p->iText = HTML5_TEXT_RCDATA;
	}else if( HTML5_IN(azHtml5Raw,zName) ){
		p->iText = HTML5_TEXT_RAW;
	}else if( Html5Eq(zName,"plaintext") ){
		p->iText = HTML5_TEXT_PLAIN;
	}
	return pEl;
}
/*
 * A `<template>` is the one element whose content is parsed in a mode of its
 * own.  Every mode that can meet the start tag hands it here: the element is
 * inserted where the current mode would put it, the mode that was running is
 * PARKED, and the parser switches to `in template` until the matching end tag
 * hands the parked mode back.  Templates nest, so the parked modes are a
 * stack rather than one slot -- without it a `<template>` written in the head
 * fell through `in head`'s "anything else", which pops and re-runs the token
 * in `after head`, minting a second `<body>` INSIDE the head to hold content
 * that belongs in the template.
 */
static void Html5TemplateStart(html5_parser *p)
{
	int iSave = p->iMode;
	Html5InsertStart(p);
	Html5FmtMarker(p);
	p->bFramesetOk = 0;
	SySetPut(&p->sTmpl,(const void *)&iSave);
	p->iMode = HTML5_M_IN_TEMPLATE;
}
/*
 * The matching end tag, and the only way out short of the end of the document.
 * An end tag with no template open is dropped where a bare `Html5EndTag` would
 * have popped whatever else was open -- `</template>` written in a head with no
 * template closed the HEAD and sent the rest of it into the body.
 */
static void Html5TemplateEnd(html5_parser *p)
{
	int *aMode;
	sxu32 n;
	if( Html5OpenDepth(p,"template") < 0 ){
		return;
	}
	Html5PopTo(p,"template");
	Html5FmtClearToMarker(p);
	n = SySetUsed(&p->sTmpl);
	aMode = (int *)SySetBasePtr(&p->sTmpl);
	if( n > 0 ){
		p->iMode = aMode[n - 1];
		SySetPop(&p->sTmpl);
	}else{
		p->iMode = HTML5_M_IN_BODY;
	}
}
static void Html5InsertText(html5_parser *p)
{
	sxu32 n = SyBlobLength(&p->sBuf);
	if( n < 1 ){
		return;
	}
	/*
	 * Html5Attach merges a text node into a preceding text sibling on BOTH of
	 * its paths, which is exactly the spec's rule, so the tree never carries
	 * two adjacent ones.
	 */
	Html5Attach(p,xmlNewDocTextLen(p->pDoc,
		(const xmlChar *)SyBlobData(&p->sBuf),(int)n));
}
/*
 * The frameset modes keep the whitespace of a character run and drop the rest
 * of it, so a run that is a mixture is inserted with its other bytes removed
 * rather than kept whole or dropped whole.
 */
static void Html5InsertSpaceOnly(html5_parser *p)
{
	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);
	sxu32 i,n = SyBlobLength(&p->sBuf),nKeep = 0;
	unsigned char *zKeep = (unsigned char *)SyBlobData(&p->sBuf);
	for( i = 0 ; i < n ; ++i ){
		if( Html5IsSpace(z[i]) ){
			zKeep[nKeep++] = z[i];
		}
	}
	p->sBuf.nByte = nKeep;
	Html5InsertText(p);
}
static int Html5TextIsSpace(html5_parser *p)
{
	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);
	sxu32 i,n = SyBlobLength(&p->sBuf);
	for( i = 0 ; i < n ; ++i ){
		if( !Html5IsSpace(z[i]) ){
			return 0;
		}
	}
	return 1;
}
/* Drop the leading whitespace of a text token, keeping the rest. */
static void Html5TrimLeadingSpace(html5_parser *p)
{
	unsigned char *z = (unsigned char *)SyBlobData(&p->sBuf);
	sxu32 i = 0,n = SyBlobLength(&p->sBuf);
	if( n < 1 ){
		return;
	}
	while( i < n && Html5IsSpace(z[i]) ){
		i++;
	}
	if( i > 0 ){
		sxu32 j;
		for( j = 0 ; j < n - i ; ++j ){
			z[j] = z[j + i];
		}
		p->sBuf.nByte = n - i;
	}
}
/*
 * The head modes KEEP that whitespace rather than dropping it: the leading run
 * is inserted where a run of nothing but whitespace would have gone, and only
 * the rest is handed to the mode this token is about to walk into.  `before
 * html` and `before head` ignore it instead, so they still trim without
 * inserting -- which is why this is a second helper and not a flag on the
 * first.
 */
static void Html5SplitLeadingSpace(html5_parser *p)
{
	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);
	sxu32 i = 0,n = SyBlobLength(&p->sBuf);
	while( i < n && Html5IsSpace(z[i]) ){
		i++;
	}
	if( i > 0 ){
		p->sBuf.nByte = i;
		Html5InsertText(p);
		p->sBuf.nByte = n;
	}
	Html5TrimLeadingSpace(p);
}
static void Html5InsertComment(html5_parser *p,xmlNodePtr pParent)
{
	xmlNodePtr pC;
	SyBlobNullAppend(&p->sBuf);
	pC = xmlNewDocComment(p->pDoc,(const xmlChar *)SyBlobData(&p->sBuf));
	if( pC ){
		xmlAddChild(pParent,pC);
	}
}
/* ------------------------------------------------------------------ *
 * Foreign content: the SVG and MathML subtrees
 * ------------------------------------------------------------------ */

/*
 * Inside a `<svg>` or a `<math>` the tokenizer still lowercases every name it
 * reads, because it has no idea what tree it is feeding.  These two tables are
 * the spec's repair: the SVG names whose real spelling is mixed-case, and the
 * SVG attributes likewise.  A name that is in neither keeps the lowercased
 * spelling -- an unknown `<unknownThing>` is an `unknownthing` there too.
 */
typedef struct html5_fix html5_fix;
struct html5_fix { const char *zLow; const char *zFix; };
static const html5_fix aHtml5SvgTag[] = {
	{"altglyph","altGlyph"},{"altglyphdef","altGlyphDef"},
	{"altglyphitem","altGlyphItem"},{"animatecolor","animateColor"},
	{"animatemotion","animateMotion"},{"animatetransform","animateTransform"},
	{"clippath","clipPath"},{"feblend","feBlend"},
	{"fecolormatrix","feColorMatrix"},
	{"fecomponenttransfer","feComponentTransfer"},
	{"fecomposite","feComposite"},{"feconvolvematrix","feConvolveMatrix"},
	{"fediffuselighting","feDiffuseLighting"},
	{"fedisplacementmap","feDisplacementMap"},
	{"fedistantlight","feDistantLight"},{"fedropshadow","feDropShadow"},
	{"feflood","feFlood"},{"fefunca","feFuncA"},{"fefuncb","feFuncB"},
	{"fefuncg","feFuncG"},{"fefuncr","feFuncR"},
	{"fegaussianblur","feGaussianBlur"},{"feimage","feImage"},
	{"femerge","feMerge"},{"femergenode","feMergeNode"},
	{"femorphology","feMorphology"},{"feoffset","feOffset"},
	{"fepointlight","fePointLight"},
	{"fespecularlighting","feSpecularLighting"},
	{"fespotlight","feSpotLight"},{"fetile","feTile"},
	{"feturbulence","feTurbulence"},{"foreignobject","foreignObject"},
	{"glyphref","glyphRef"},{"lineargradient","linearGradient"},
	{"radialgradient","radialGradient"},{"textpath","textPath"}
};
static const html5_fix aHtml5SvgAttr[] = {
	{"attributename","attributeName"},{"attributetype","attributeType"},
	{"basefrequency","baseFrequency"},{"baseprofile","baseProfile"},
	{"calcmode","calcMode"},{"clippathunits","clipPathUnits"},
	{"diffuseconstant","diffuseConstant"},{"edgemode","edgeMode"},
	{"filterunits","filterUnits"},{"glyphref","glyphRef"},
	{"gradienttransform","gradientTransform"},{"gradientunits","gradientUnits"},
	{"kernelmatrix","kernelMatrix"},{"kernelunitlength","kernelUnitLength"},
	{"keypoints","keyPoints"},{"keysplines","keySplines"},
	{"keytimes","keyTimes"},{"lengthadjust","lengthAdjust"},
	{"limitingconeangle","limitingConeAngle"},{"markerheight","markerHeight"},
	{"markerunits","markerUnits"},{"markerwidth","markerWidth"},
	{"maskcontentunits","maskContentUnits"},{"maskunits","maskUnits"},
	{"numoctaves","numOctaves"},{"pathlength","pathLength"},
	{"patterncontentunits","patternContentUnits"},
	{"patterntransform","patternTransform"},{"patternunits","patternUnits"},
	{"pointsatx","pointsAtX"},{"pointsaty","pointsAtY"},
	{"pointsatz","pointsAtZ"},{"preservealpha","preserveAlpha"},
	{"preserveaspectratio","preserveAspectRatio"},
	{"primitiveunits","primitiveUnits"},{"refx","refX"},{"refy","refY"},
	{"repeatcount","repeatCount"},{"repeatdur","repeatDur"},
	{"requiredextensions","requiredExtensions"},
	{"requiredfeatures","requiredFeatures"},
	{"specularconstant","specularConstant"},
	{"specularexponent","specularExponent"},{"spreadmethod","spreadMethod"},
	{"startoffset","startOffset"},{"stddeviation","stdDeviation"},
	{"stitchtiles","stitchTiles"},{"surfacescale","surfaceScale"},
	{"systemlanguage","systemLanguage"},{"tablevalues","tableValues"},
	{"targetx","targetX"},{"targety","targetY"},{"textlength","textLength"},
	{"viewbox","viewBox"},{"viewtarget","viewTarget"},
	{"xchannelselector","xChannelSelector"},
	{"ychannelselector","yChannelSelector"},{"zoomandpan","zoomAndPan"}
};
/*
 * The HTML start tags that give up on the foreign subtree entirely: they are
 * so much more likely to be a page that forgot to close its `<svg>` than SVG
 * content that the spec pops back out to HTML and reprocesses the tag there.
 * `<font>` joins them only when it carries one of the three attributes that
 * make it the HTML one.
 */
static const char * const azHtml5Breakout[] = {
	"b","big","blockquote","body","br","center","code","dd","div","dl","dt",
	"em","embed","h1","h2","h3","h4","h5","h6","head","hr","i","img","li",
	"listing","menu","meta","nobr","ol","p","pre","ruby","s","small","span",
	"strong","strike","sub","sup","table","tt","u","ul","var"
};
/* The local names of the attributes that carry a namespace of their own. */
static const char * const azHtml5Xlink[] = {
	"actuate","arcrole","href","role","show","title","type"
};
static const char * const azHtml5Xml[] = { "lang","space" };

static int Html5LowerEq(const char *zLeft,const char *zRight)
{
	while( *zLeft != 0 && Html5Lower(*zLeft) == Html5Lower(*zRight) ){
		zLeft++;
		zRight++;
	}
	return *zLeft == 0 && *zRight == 0;
}
/* The namespace an open element is in.  Everything that is not one of the two
 * foreign ones -- the HTML namespace, no namespace at all -- answers HTML, and
 * that is what every rule below tests for. */
static int Html5NsKind(xmlNodePtr pEl)
{
	const char *zHref;
	if( pEl == 0 || pEl->ns == 0 || pEl->ns->href == 0 ){
		return HTML5_NSK_HTML;
	}
	zHref = (const char *)pEl->ns->href;
	if( Html5Eq(zHref,HTML5_SVG_NS) ){
		return HTML5_NSK_SVG;
	}
	if( Html5Eq(zHref,HTML5_MATH_NS) ){
		return HTML5_NSK_MATH;
	}
	return HTML5_NSK_HTML;
}
static const char * Html5NsHrefOf(int iKind)
{
	if( iKind == HTML5_NSK_SVG ){
		return HTML5_SVG_NS;
	}
	return iKind == HTML5_NSK_MATH ? HTML5_MATH_NS : 0;
}
/* A MathML `<annotation-xml>` whose `encoding` names an HTML flavour, and the
 * three SVG elements that hold HTML outright: inside one of these the HTML
 * rules run again, so a `<b>` under a `<foreignObject>` is an HTML `<b>`. */
static int Html5IsHtmlIp(xmlNodePtr pEl)
{
	int iKind = Html5NsKind(pEl);
	const char *zName;
	if( iKind == HTML5_NSK_HTML ){
		return 0;
	}
	zName = (const char *)pEl->name;
	if( iKind == HTML5_NSK_SVG ){
		return Html5Eq(zName,"foreignObject") || Html5Eq(zName,"desc")
			|| Html5Eq(zName,"title");
	}
	if( Html5Eq(zName,"annotation-xml") ){
		xmlChar *zEnc = xmlGetNoNsProp(pEl,(const xmlChar *)"encoding");
		int bHit = zEnc && (Html5LowerEq((const char *)zEnc,"text/html")
			|| Html5LowerEq((const char *)zEnc,"application/xhtml+xml"));
		if( zEnc ){
			xmlFree(zEnc);
		}
		return bHit;
	}
	return 0;
}
/* The five MathML token elements: HTML rules for everything but the two tags
 * that are genuinely MathML's own. */
static int Html5IsMathIp(xmlNodePtr pEl)
{
	const char *zName = pEl ? (const char *)pEl->name : "";
	return Html5NsKind(pEl) == HTML5_NSK_MATH
		&& (Html5Eq(zName,"mi") || Html5Eq(zName,"mo") || Html5Eq(zName,"mn")
		 || Html5Eq(zName,"ms") || Html5Eq(zName,"mtext"));
}
static const char * Html5TableFix(const char *zName,const html5_fix *aTab,int nTab)
{
	int i;
	for( i = 0 ; i < nTab ; ++i ){
		if( Html5Eq(aTab[i].zLow,zName) ){
			return aTab[i].zFix;
		}
	}
	return zName;
}
/*
 * The attributes of a foreign start tag.  Three things happen to a name that
 * do not happen in HTML: the SVG table restores its case, `definitionurl`
 * becomes MathML's `definitionURL`, and the handful of `xlink:`/`xml:` names
 * move into a real namespace instead of staying a name with a colon in it.
 * Anything else -- `foo:bar` included -- is stored verbatim.
 */
static void Html5AddAttrsForeign(html5_parser *p,xmlNodePtr pEl,int iKind)
{
	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);
	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);
	sxu32 i,n = SySetUsed(&p->sAttr);
	for( i = 0 ; i < n ; ++i ){
		const char *zName = zBuf + aAttr[i].nNameOfs;
		const char *zVal = zBuf + aAttr[i].nValOfs;
		const char *zLocal = 0;
		const char *zHref = 0;
		const char *zPfx = 0;
		if( zName[0] == 0 ){
			continue;
		}
		if( SyStrncmp(zName,"xlink:",sizeof("xlink:")-1) == 0
		 && HTML5_IN(azHtml5Xlink,zName + sizeof("xlink:") - 1) ){
			zLocal = zName + sizeof("xlink:") - 1;
			zHref = HTML5_XLINK_NS;
			zPfx = "xlink";
		}else if( SyStrncmp(zName,"xml:",sizeof("xml:")-1) == 0
		 && HTML5_IN(azHtml5Xml,zName + sizeof("xml:") - 1) ){
			zLocal = zName + sizeof("xml:") - 1;
			zHref = HTML5_XML_NS;
			zPfx = "xml";
		}else if( iKind == HTML5_NSK_SVG ){
			zLocal = Html5TableFix(zName,aHtml5SvgAttr,
				(int)SX_ARRAYSIZE(aHtml5SvgAttr));
		}else if( Html5Eq(zName,"definitionurl") ){
			zLocal = "definitionURL";
		}else{
			zLocal = zName;
		}
		if( zHref ){
			xmlNsPtr pNs = Html5NsGet(p,pEl,zHref,zPfx);
			if( xmlHasNsProp(pEl,(const xmlChar *)zLocal,(const xmlChar *)zHref) ){
				continue;
			}
			xmlNewNsProp(pEl,pNs,(const xmlChar *)zLocal,(const xmlChar *)zVal);
			continue;
		}
		if( xmlHasProp(pEl,(const xmlChar *)zLocal) ){
			continue;
		}
		xmlNewProp(pEl,(const xmlChar *)zLocal,(const xmlChar *)zVal);
	}
}
/*
 * A foreign start tag.  There is no void-element list here and no armed text
 * flavour: the ONLY thing that closes an element without an end tag is the
 * `/>` the source wrote, which HTML ignores and foreign content honours.
 */
static xmlNodePtr Html5InsertForeign(html5_parser *p,int iKind)
{
	const char *zName = Html5TokName(p);
	xmlNodePtr pEl;
	if( iKind == HTML5_NSK_SVG ){
		zName = Html5TableFix(zName,aHtml5SvgTag,(int)SX_ARRAYSIZE(aHtml5SvgTag));
	}
	pEl = Html5NewElemNs(p,zName,Html5NsHrefOf(iKind));
	if( pEl == 0 ){
		return 0;
	}
	Html5AddAttrsForeign(p,pEl,iKind);
	if( !p->bSelfClose ){
		Html5Push(p,pEl);
	}
	return pEl;
}
/* Does THIS token go through the foreign rules?  The question is asked of the
 * current node for every token, which is what lets one `<b>` inside a
 * `<foreignObject>` be HTML while its `<circle>` sibling is not. */
static int Html5UseForeign(html5_parser *p)
{
	xmlNodePtr pCur = Html5Top(p);
	const char *zTok = Html5TokName(p);
	if( pCur == 0 || p->iTok == HTML5_TOK_EOF
	 || Html5NsKind(pCur) == HTML5_NSK_HTML ){
		return 0;
	}
	if( Html5IsMathIp(pCur) ){
		if( p->iTok == HTML5_TOK_TEXT ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && !Html5Eq(zTok,"mglyph")
		 && !Html5Eq(zTok,"malignmark") ){
			return 0;
		}
	}
	if( Html5NsKind(pCur) == HTML5_NSK_MATH
	 && Html5Eq((const char *)pCur->name,"annotation-xml")
	 && p->iTok == HTML5_TOK_START && Html5Eq(zTok,"svg") ){
		return 0;
	}
	if( Html5IsHtmlIp(pCur)
	 && (p->iTok == HTML5_TOK_START || p->iTok == HTML5_TOK_TEXT) ){
		return 0;
	}
	return 1;
}
/* Is this the HTML tag that gives up on the subtree? */
static int Html5IsBreakout(html5_parser *p,const char *zName)
{
	if( HTML5_IN(azHtml5Breakout,zName) ){
		return 1;
	}
	if( Html5Eq(zName,"font") ){
		html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);
		const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);
		sxu32 i,n = SySetUsed(&p->sAttr);
		for( i = 0 ; i < n ; ++i ){
			const char *zAt = zBuf + aAttr[i].nNameOfs;
			if( Html5Eq(zAt,"color") || Html5Eq(zAt,"face")
			 || Html5Eq(zAt,"size") ){
				return 1;
			}
		}
	}
	return 0;
}
/*
 * The foreign tree constructor.  Answers 1 when the token has to be handed
 * back to the HTML rules -- which is how both ways out of a foreign subtree
 * work: a breakout tag pops to the nearest HTML element and reprocesses, and
 * an end tag that matches nothing foreign walks down to an HTML element and
 * lets that element's own rules answer it.
 */
static int Html5ForeignDispatch(html5_parser *p)
{
	const char *zName = Html5TokName(p);
	switch( p->iTok ){
	case HTML5_TOK_TEXT:
		Html5InsertText(p);
		return 0;
	case HTML5_TOK_COMMENT:
		Html5InsertComment(p,Html5Target(p));
		return 0;
	case HTML5_TOK_START:
		if( Html5IsBreakout(p,zName) ){
			while( Html5Top(p) && Html5NsKind(Html5Top(p)) != HTML5_NSK_HTML
			 && !Html5IsHtmlIp(Html5Top(p)) && !Html5IsMathIp(Html5Top(p)) ){
				Html5Pop(p);
			}
			return 1;
		}
		Html5InsertForeign(p,Html5NsKind(Html5Top(p)));
		return 0;
	case HTML5_TOK_END: {
		xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
		int i = (int)SySetUsed(&p->sOpen) - 1;
		/* The comparison is case-INSENSITIVE, because the stack holds the
		 * repaired spelling and the token holds the lowercased one: `</G>`
		 * and `</clippath>` both close what they name. */
		for( ; i >= 0 ; --i ){
			if( Html5LowerEq((const char *)apStack[i]->name,zName) ){
				while( (int)SySetUsed(&p->sOpen) > i ){
					Html5Pop(p);
				}
				return 0;
			}
			if( Html5NsKind(apStack[i]) == HTML5_NSK_HTML ){
				p->bHtmlRules = 1;
				return 1;
			}
		}
		return 0;
	}
	default:
		break;
	}
	return 0;
}
/*
 * A list item's own start tag closes the one it is a sibling of -- which, with
 * a formatting element left open inside that one, is no longer the innermost
 * open element.  The walk is the spec's: out from the current node, closing
 * the first `zOne`/`zTwo` it reaches, and stopping at a special element that
 * is not one of the three a list item is allowed to sit inside.
 */
static void Html5CloseListItem(html5_parser *p,const char *zOne,const char *zTwo)
{
	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
	int n = (int)SySetUsed(&p->sOpen);
	while( n-- > 0 ){
		const char *zTop = (const char *)apStack[n]->name;
		if( Html5Eq(zTop,zOne) || Html5Eq(zTop,zTwo) ){
			while( (int)SySetUsed(&p->sOpen) > n ){
				Html5Pop(p);
			}
			return;
		}
		if( HTML5_IN(azHtml5Special,zTop) && !Html5Eq(zTop,"address")
		 && !Html5Eq(zTop,"div") && !Html5Eq(zTop,"p") ){
			return;
		}
	}
}
/*
 * The rules an open `<select>` adds to a start tag.  A second `<select>` and
 * an `<input>` both close it -- the `<select>` is then dropped where the
 * `<input>` goes on to be inserted beside it -- and an `<hr>` or an
 * `<optgroup>` closes the `<option>`, and then the `<optgroup>`, the source
 * left open, so both land on the select rather than inside the option.
 * Answers 1 when the token is spent and nothing is to be inserted for it.
 */
static int Html5SelectImplied(html5_parser *p,const char *zName)
{
	int bSelect = Html5Eq(zName,"select");
	if( !Html5InSelect(p) ){
		return 0;
	}
	if( bSelect || Html5Eq(zName,"input") ){
		Html5PopTo(p,"select");
		return bSelect;
	}
	if( Html5Eq(zName,"hr") || Html5Eq(zName,"optgroup") ){
		xmlNodePtr pTop = Html5Top(p);
		if( pTop && Html5Eq((const char *)pTop->name,"option") ){
			Html5Pop(p);
			pTop = Html5Top(p);
		}
		if( pTop && Html5Eq((const char *)pTop->name,"optgroup") ){
			Html5Pop(p);
		}
	}
	return 0;
}
/* The `in body` start-tag rules that are about what is ALREADY open. */
static void Html5BodyImplied(html5_parser *p,const char *zName)
{
	/*
	 * An open `<select>` answers for everything inside it, so none of the
	 * implied closes below may reach past it: a `<p>` or an `<li>` the source
	 * opened OUTSIDE the select stays open, and the tag that would have
	 * closed it is inserted where it stands instead.  Only the `<option>`
	 * rule, which never looks further than the current node, still applies.
	 */
	int bSelect = Html5InSelect(p);
	/*
	 * The `<p>` being closed need not be the innermost open element: a
	 * formatting element left open inside it sits above it now, and the spec
	 * closes the p under it rather than giving up.
	 */
	if( !bSelect && HTML5_IN(azHtml5ClosesP,zName) && Html5NameInScope(p,"p") ){
		Html5PopTo(p,"p");
	}
	if( !bSelect && Html5Eq(zName,"li") ){
		Html5CloseListItem(p,"li","li");
	}
	if( !bSelect && (Html5Eq(zName,"dd") || Html5Eq(zName,"dt")) ){
		Html5CloseListItem(p,"dd","dt");
	}
	if( Html5Eq(zName,"option") && Html5OpenDepth(p,"option") == 0 ){
		Html5Pop(p);
	}
	if( !bSelect && zName[0] == 'h' && zName[1] >= '1' && zName[1] <= '6'
	 && zName[2] == 0 ){
		xmlNodePtr pTop = Html5Top(p);
		if( pTop && pTop->name[0] == 'h' && pTop->name[1] >= '1'
		 && pTop->name[1] <= '6' && pTop->name[2] == 0 ){
			Html5Pop(p);
		}
	}
}
/*
 * The table implications, which is all the four table insertion modes decide.
 * A `<tr>` needs a row-group above it and a `<td>` needs a row; the stack says
 * which of them the source already spelled.
 */
static void Html5TableImplied(html5_parser *p,const char *zName)
{
	int bCell = Html5Eq(zName,"td") || Html5Eq(zName,"th");
	int bRow = Html5Eq(zName,"tr");
	int bGroup = Html5Eq(zName,"tbody") || Html5Eq(zName,"thead")
		|| Html5Eq(zName,"tfoot");
	{
		/*
		 * The stack is cleared back to a `table`, a `template` or the root, so
		 * a template written between the tag and the table under it IS the
		 * context: the row belongs to the template, no row group is implied
		 * for it, and the table below never sees it.
		 */
		xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
		sxu32 n = SySetUsed(&p->sOpen);
		int bTable = 0;
		while( n-- > 0 ){
			if( Html5Eq((const char *)apStack[n]->name,"template") ){
				return;
			}
			if( Html5Eq((const char *)apStack[n]->name,"table") ){
				bTable = 1;
				break;
			}
		}
		if( !bTable ){
			return;
		}
	}
	if( Html5Eq(zName,"col") ){
		/* A `<col>` needs a column group the way a `<td>` needs a row. */
		if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,"colgroup") ){
			return;
		}
		while( Html5Top(p) && !Html5Eq((const char *)Html5Top(p)->name,"table") ){
			Html5Pop(p);
		}
		if( Html5Top(p) ){
			xmlNodePtr pGrp = Html5NewElem(p,"colgroup");
			if( pGrp ){
				Html5Push(p,pGrp);
			}
		}
		return;
	}
	if( bGroup || Html5Eq(zName,"caption") || Html5Eq(zName,"colgroup") ){
		while( Html5Top(p) && !Html5Eq((const char *)Html5Top(p)->name,"table") ){
			Html5Pop(p);
		}
		return;
	}
	if( bRow || bCell ){
		/*
		 * `Clear the stack back to a table context`, which is also what
		 * closes a cell beside an open one.  Whatever was fostered out of the
		 * table is still stacked ABOVE it, and a row or a cell belongs to the
		 * table under all of it rather than to the content that left.
		 */
		while( Html5Top(p) ){
			const char *zTop = (const char *)Html5Top(p)->name;
			if( Html5Eq(zTop,"table") || Html5Eq(zTop,"tbody")
			 || Html5Eq(zTop,"thead") || Html5Eq(zTop,"tfoot")
			 || Html5Eq(zTop,"html") ){
				break;
			}
			if( bCell && Html5Eq(zTop,"tr") ){
				break;
			}
			Html5Pop(p);
		}
		if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,"table") ){
			xmlNodePtr pGrp = Html5NewElem(p,"tbody");
			if( pGrp ){
				Html5Push(p,pGrp);
			}
		}
		if( bCell && Html5Top(p)
		 && !Html5Eq((const char *)Html5Top(p)->name,"tr") ){
			xmlNodePtr pRow = Html5NewElem(p,"tr");
			if( pRow ){
				Html5Push(p,pRow);
			}
		}
	}
}
/*
 * A column group holds nothing but `<col>`, so anything else written while one
 * is open closes it first and is then reconsidered against the table under it
 * -- usually to be fostered straight back out.  Whitespace stays, the way it
 * does in the table itself.
 */
static void Html5ColgroupImplied(html5_parser *p,const char *zName)
{
	xmlNodePtr pTop = Html5Top(p);
	if( pTop == 0 || !Html5Eq((const char *)pTop->name,"colgroup") ){
		return;
	}
	if( zName && (Html5Eq(zName,"col") || Html5Eq(zName,"template")) ){
		return;
	}
	Html5Pop(p);
}
/* The generic end tag: pop to it, or ignore it if it is not open. */
static void Html5EndTag(html5_parser *p,const char *zName)
{
	if( HTML5_IN(azHtml5Void,zName) ){
		return;
	}
	if( Html5OpenDepth(p,zName) < 0 ){
		return;
	}
	Html5PopTo(p,zName);
}
/* Does `pParent` already hold an element of this name? */
static xmlNodePtr Html5ChildNamed(xmlNodePtr pParent,const char *zName)
{
	xmlNodePtr pCur;
	for( pCur = pParent ? pParent->children : 0 ; pCur ; pCur = pCur->next ){
		if( pCur->type == XML_ELEMENT_NODE
		 && Html5Eq((const char *)pCur->name,zName) ){
			return pCur;
		}
	}
	return 0;
}
static void Html5OpenHtml(html5_parser *p,int bWithAttrs)
{
	xmlNodePtr pHtml;
	if( p->iFlags & HTML5_NOIMPLIED ){
		p->iMode = HTML5_M_IN_BODY;
		return;
	}
	pHtml = Html5NewElem(p,"html");
	if( pHtml == 0 ){
		return;
	}
	if( bWithAttrs ){
		Html5AddAttrs(p,pHtml);
	}
	xmlDocSetRootElement(p->pDoc,pHtml);
	p->pHtml = pHtml;
	Html5Push(p,pHtml);
	p->iMode = HTML5_M_BEFORE_HEAD;
}
static void Html5OpenHead(html5_parser *p,int bWithAttrs)
{
	xmlNodePtr pHead;
	if( p->iFlags & HTML5_NOIMPLIED ){
		p->iMode = HTML5_M_IN_BODY;
		return;
	}
	pHead = Html5NewElem(p,"head");
	if( pHead == 0 ){
		return;
	}
	if( bWithAttrs ){
		Html5AddAttrs(p,pHead);
	}
	p->pHead = pHead;
	Html5Push(p,pHead);
	p->iMode = HTML5_M_IN_HEAD;
}
static void Html5OpenBody(html5_parser *p,int bWithAttrs)
{
	xmlNodePtr pBody;
	if( p->iFlags & HTML5_NOIMPLIED ){
		p->iMode = HTML5_M_IN_BODY;
		return;
	}
	pBody = Html5NewElem(p,"body");
	if( pBody == 0 ){
		return;
	}
	if( bWithAttrs ){
		/*
		 * A body the SOURCE spelled clears the frameset-ok flag; one the
		 * parser minted because content needed somewhere to go does not, and
		 * that is the whole difference between a `<frameset>` after `<body>`
		 * being dropped and one after `<p>` replacing the tree.
		 */
		Html5AddAttrs(p,pBody);
		p->bFramesetOk = 0;
	}
	Html5Push(p,pBody);
	p->iMode = HTML5_M_IN_BODY;
}
/*
 * One token through the insertion modes.  A mode that cannot take the token
 * mints what the spec says is missing and asks to be handed it again, which is
 * the `bAgain` answer.
 */
static int Html5Dispatch(html5_parser *p)
{
	const char *zName = Html5TokName(p);
	/*
	 * The spec's `text` insertion mode: a `<title>` or a `<script>` armed the
	 * tokenizer, so what came back is that element's content and goes into it
	 * whatever mode the tree is otherwise in.  Without this the text walks the
	 * modes, pops the element that asked for it and lands in the body.
	 */
	if( p->bTextTok ){
		Html5InsertText(p);
		return 0;
	}
	/*
	 * Foreign content is decided before the insertion mode, not inside it: the
	 * mode is still `in body` all the way through an `<svg>` subtree, and it is
	 * the current node's NAMESPACE that says which set of rules this token
	 * meets.
	 */
	/*
	 * ...unless the foreign end-tag walk has just said so.  Reaching an HTML
	 * element on the way out of a foreign subtree hands THIS token to the
	 * HTML rules without popping anything, so the namespace question has to
	 * be skipped for one dispatch -- asking it again would send the token
	 * straight back to the walk it just came out of, which is what used to
	 * spin the reprocess guard and drop the token.
	 */
	if( p->bHtmlRules ){
		p->bHtmlRules = 0;
	}else if( Html5UseForeign(p) ){
		return Html5ForeignDispatch(p);
	}
	switch( p->iMode ){
	case HTML5_M_INITIAL:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			xmlCreateIntSubset(p->pDoc,(const xmlChar *)
				(zName[0] ? zName : "html"),0,0);
			p->iMode = HTML5_M_BEFORE_HTML;
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,(xmlNodePtr)p->pDoc);
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT && Html5TextIsSpace(p) ){
			return 0;
		}
		/*
		 * A document that states no doctype is a parse error at the first
		 * token that is not one -- but only a TAG is reported: character data
		 * reaching this mode is quietly the anything-else branch.
		 */
		if( (p->iTok == HTML5_TOK_START || p->iTok == HTML5_TOK_END)
		 && (p->iFlags & HTML5_NOIMPLIED) == 0 ){
			Html5Err(p,"tree","unexpected-token-in-initial-mode",
				p->iTokLine,p->iTokCol,p->iTokCol2);
		}
		p->iMode = HTML5_M_BEFORE_HTML;
		return 1;
	case HTML5_M_BEFORE_HTML:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,(xmlNodePtr)p->pDoc);
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT ){
			if( Html5TextIsSpace(p) ){
				return 0;
			}
			Html5TrimLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
			Html5OpenHtml(p,TRUE);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"head")
		 && !Html5Eq(zName,"body") && !Html5Eq(zName,"html")
		 && !Html5Eq(zName,"br") && (p->iFlags & HTML5_NOIMPLIED) == 0 ){
			Html5Err(p,"tree","unexpected-closed-token-in-before-html-mode",
				p->iTokLine,p->iTokCol,p->iTokCol2);
			return 0;
		}
		Html5OpenHtml(p,FALSE);
		return 1;
	case HTML5_M_BEFORE_HEAD:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT ){
			if( Html5TextIsSpace(p) ){
				return 0;
			}
			Html5TrimLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"head") ){
			Html5OpenHead(p,TRUE);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
			return 0;
		}
		Html5OpenHead(p,FALSE);
		return 1;
	case HTML5_M_IN_HEAD:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT ){
			if( Html5TextIsSpace(p) ){
				Html5InsertText(p);
				return 0;
			}
			Html5SplitLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){
			Html5TemplateStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){
			Html5TemplateEnd(p);
			return 0;
		}
		/*
		 * With scripting disabled a head `<noscript>` holds MARKUP, so it
		 * gets a mode of its own rather than the RAWTEXT arming: the head
		 * elements it accepts stay inside it and everything else closes it.
		 */
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noscript") ){
			Html5InsertStart(p);
			p->iMode = HTML5_M_IN_HEAD_NOSCRIPT;
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName) ){
			Html5InsertStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"head") ){
			Html5Pop(p);
			p->iMode = HTML5_M_AFTER_HEAD;
			return 0;
		}
		/* `</title>` closes the title, not the head that holds it. */
		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"html")
		 && Html5OpenDepth(p,zName) >= 0
		 && (p->pHead == 0 || Html5Top(p) != p->pHead) ){
			Html5EndTag(p,zName);
			return 0;
		}
		Html5Pop(p);
		p->iMode = HTML5_M_AFTER_HEAD;
		return 1;
	case HTML5_M_IN_HEAD_NOSCRIPT:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noscript") ){
			Html5Pop(p);
			p->iMode = HTML5_M_IN_HEAD;
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT ){
			if( Html5TextIsSpace(p) ){
				Html5InsertText(p);
				return 0;
			}
			Html5SplitLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
			/* `in body` rules, which for an `<html>` already open is the
			 * attributes it did not have yet. */
			if( p->pHtml ){
				Html5AddAttrs(p,p->pHtml);
			}
			return 0;
		}
		if( p->iTok == HTML5_TOK_START
		 && HTML5_IN(azHtml5HeadNoscript,zName) ){
			Html5InsertStart(p);
			return 0;
		}
		/*
		 * `</style>` closes the style this mode opened, not the noscript
		 * holding it -- the same reading `in head` gives its own end tags,
		 * and without it the close walked out of the head and minted a
		 * second `<body>` inside it.
		 */
		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"html")
		 && Html5OpenDepth(p,zName) >= 0 && Html5Top(p)
		 && !Html5Eq((const char *)Html5Top(p)->name,"noscript") ){
			Html5EndTag(p,zName);
			return 0;
		}
		/*
		 * A `<head>` and a nested `<noscript>` are the two start tags this
		 * mode DROPS; everything else -- a `<title>`, a `</br>`, any content
		 * at all -- closes the noscript and is re-run one mode out.
		 */
		if( p->iTok == HTML5_TOK_START
		 && (Html5Eq(zName,"head") || Html5Eq(zName,"noscript")) ){
			return 0;
		}
		Html5Pop(p);
		p->iMode = HTML5_M_IN_HEAD;
		return 1;
	case HTML5_M_AFTER_HEAD:
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT ){
			if( Html5TextIsSpace(p) ){
				Html5InsertText(p);
				return 0;
			}
			Html5SplitLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"body") ){
			Html5OpenBody(p,TRUE);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"frameset") ){
			/* No body is minted at all: the frameset IS the document's. */
			Html5InsertStart(p);
			p->iMode = HTML5_M_IN_FRAMESET;
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
			return 0;
		}
		/*
		 * A head element spelled after the head was closed goes back INTO the
		 * head, which is the one place the tree's shape is not the source's
		 * order.
		 */
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){
			Html5TemplateStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){
			Html5TemplateEnd(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName)
		 && p->pHead ){
			Html5Push(p,p->pHead);
			Html5InsertStart(p);
			if( Html5Top(p) != p->pHead ){
				Html5Pop(p);
			}
			Html5Pop(p);
			return 0;
		}
		Html5OpenBody(p,FALSE);
		return 1;
	case HTML5_M_IN_TEMPLATE:
		/*
		 * The content of a template is everything the body would take, so the
		 * spec's rule for anything that is not a head element is to hand the
		 * mode over to `in body` and reprocess there; `</template>` is then
		 * reached from `in body` rather than from here.  The head elements
		 * keep `in head`'s treatment, which is what arms `<title>` and
		 * `<script>` for their raw text.
		 */
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){
			Html5TemplateStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){
			Html5TemplateEnd(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName) ){
			Html5InsertStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		/*
		 * An end tag is where the handover stops: everything but
		 * `</template>` is DROPPED here rather than re-run `in body`.  Only
		 * an end tag written before the template holds any content can reach
		 * this mode at all -- a start tag latches the mode to `in body` and
		 * that is where the closes of what it opened are read -- and for
		 * those the two readings differ by exactly one tag: `</br>`, which
		 * `in body` answers with an ELEMENT.
		 */
		if( p->iTok == HTML5_TOK_END ){
			return 0;
		}
		p->iMode = HTML5_M_IN_BODY;
		return 1;
	case HTML5_M_AFTER_BODY:
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,p->pHtml ? p->pHtml : (xmlNodePtr)p->pDoc);
			return 0;
		}
		if( p->iTok == HTML5_TOK_TEXT && Html5TextIsSpace(p) ){
			return 0;
		}
		if( p->iTok == HTML5_TOK_DOCTYPE ){
			return 0;
		}
		p->iMode = HTML5_M_IN_BODY;
		return 1;
	case HTML5_M_IN_FRAMESET:
		if( p->iTok == HTML5_TOK_TEXT ){
			Html5InsertSpaceOnly(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_START ){
			if( Html5Eq(zName,"frameset") || Html5Eq(zName,"frame")
			 || Html5Eq(zName,"noframes") ){
				/*
				 * `<frame>` is void, so `Html5InsertStart` inserts it without
				 * pushing it, and `<noframes>` is raw text the same way it is
				 * in the head.
				 */
				Html5InsertStart(p);
				return 0;
			}
			if( Html5Eq(zName,"html") && p->pHtml ){
				Html5AddAttrs(p,p->pHtml);
				return 0;
			}
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes") ){
			/* The one element besides a frameset these modes leave open. */
			if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,
					"noframes") ){
				Html5Pop(p);
			}
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"frameset") ){
			/*
			 * The root `<html>` is never popped by an end tag, so a stray
			 * `</frameset>` past the outermost one is ignored; closing the
			 * outermost is what leaves the mode.
			 */
			if( Html5Top(p) == p->pHtml ){
				return 0;
			}
			Html5Pop(p);
			if( Html5Top(p) == 0
			 || !Html5Eq((const char *)Html5Top(p)->name,"frameset") ){
				p->iMode = HTML5_M_AFTER_FRAMESET;
			}
			return 0;
		}
		return 0;
	case HTML5_M_AFTER_FRAMESET:
		if( p->iTok == HTML5_TOK_TEXT ){
			Html5InsertSpaceOnly(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,Html5Target(p));
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noframes") ){
			Html5InsertStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") && p->pHtml ){
			Html5AddAttrs(p,p->pHtml);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes")
		 && Html5Top(p)
		 && Html5Eq((const char *)Html5Top(p)->name,"noframes") ){
			Html5Pop(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"html") ){
			p->iMode = HTML5_M_AFTER_AFTER_FRAMESET;
			return 0;
		}
		return 0;
	case HTML5_M_AFTER_AFTER_FRAMESET:
		/* Past `</html>`: a comment is the document's, beside the root. */
		if( p->iTok == HTML5_TOK_COMMENT ){
			Html5InsertComment(p,(xmlNodePtr)p->pDoc);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noframes") ){
			Html5InsertStart(p);
			return 0;
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") && p->pHtml ){
			Html5AddAttrs(p,p->pHtml);
			return 0;
		}
		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes")
		 && Html5Top(p)
		 && Html5Eq((const char *)Html5Top(p)->name,"noframes") ){
			Html5Pop(p);
			return 0;
		}
		return 0;
	default:
		break;
	}
	/*
	 * `in body`, and every table mode with it.  Fostering is armed per token
	 * by the two cases that can be misplaced and is off for everything else,
	 * including the implied row groups and rows the table modes mint.
	 */
	p->bFoster = 0;
	switch( p->iTok ){
	case HTML5_TOK_TEXT:
		/*
		 * A run that is nothing but whitespace stays inside the table; one
		 * with any other byte in it leaves WHOLE, spaces and all.  The
		 * reconstruction runs under the same arming, so a formatting element
		 * reopened to hold the run is fostered with it.
		 */
		p->bFoster = !Html5TextIsSpace(p);
		if( p->bFoster ){
			/* Any byte that is not whitespace is content a frameset
			 * would have to throw away, so it may no longer replace it. */
			p->bFramesetOk = 0;
			Html5ColgroupImplied(p,0);
		}
		Html5Reconstruct(p);
		Html5InsertText(p);
		break;
	case HTML5_TOK_COMMENT:
		/* A comment is the one thing a table keeps wherever it is written. */
		Html5InsertComment(p,Html5Target(p));
		break;
	case HTML5_TOK_DOCTYPE:
		break;
	case HTML5_TOK_START: {
		xmlNodePtr pEl;
		int bTableForm;
		if( Html5Eq(zName,"template") ){
			Html5TemplateStart(p);
			break;
		}
		if( Html5Eq(zName,"body") && SySetUsed(&p->sTmpl) > 0 ){
			/* A `<body>` start tag inside a template has no body to merge
			 * its attributes into and does not open one; it is dropped. */
			break;
		}
		if( Html5Eq(zName,"frameset") ){
			/*
			 * A frameset written where content already is REPLACES the body
			 * rather than joining it -- but only while nothing has gone into
			 * the document that a frameset cannot hold.  The body is the
			 * second element on the stack; anything the source opened inside
			 * it goes away with it.
			 */
			xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);
			xmlNodePtr pBody;
			if( SySetUsed(&p->sOpen) < 2 || p->bFramesetOk == 0
			 || !Html5Eq((const char *)apStack[1]->name,"body") ){
				break;
			}
			pBody = apStack[1];
			/*
			 * Pop first, then free: the formatting list may still name an
			 * element inside the body, and popping past its marker is what
			 * clears it before the subtree goes.
			 */
			while( SySetUsed(&p->sOpen) > 1 ){
				Html5Pop(p);
			}
			SySetTruncate(&p->sFmt,0);
			xmlUnlinkNode(pBody);
			xmlFreeNode(pBody);
			Html5InsertStart(p);
			p->iMode = HTML5_M_IN_FRAMESET;
			break;
		}
		if( Html5Eq(zName,"html") || Html5Eq(zName,"body") ){
			/* A second `<body>` merges its attributes and shuts the door. */
			if( Html5Eq(zName,"body") ){
				p->bFramesetOk = 0;
			}
			break;
		}
		/*
		 * A `<frame>` belongs to a frameset and nowhere else: written in the
		 * body -- which is where one written after an IGNORED `<frameset>`
		 * arrives -- it is dropped rather than inserted.
		 */
		if( Html5Eq(zName,"frame") ){
			break;
		}
		/*
		 * ...and the rest of the table furniture, which is dropped only when
		 * there is no table anywhere for it to belong to.
		 */
		if( HTML5_IN(azHtml5NoTable,zName) && !Html5TableOpen(p) ){
			break;
		}
		if( HTML5_IN(azHtml5NoFrameset,zName)
		 || (Html5Eq(zName,"input") && !Html5TableOwns(p,zName)) ){
			p->bFramesetOk = 0;
		}
		if( Html5Eq(zName,"head") ){
			break;
		}
		if( Html5SelectImplied(p,zName) ){
			break;
		}
		/*
		 * Two formatting elements refuse to nest in themselves: a second `<a>`
		 * closes the first wherever it is on the list, and a second `<nobr>`
		 * closes the one in scope.  Both do it through the adoption, so what
		 * was caught inside the first is carried out of it.
		 */
		if( Html5Eq(zName,"a") ){
			xmlNodePtr pOpenA = Html5FmtLast(p,"a");
			if( pOpenA ){
				Html5Adoption(p,"a");
				Html5FmtRemove(p,pOpenA);
				Html5SetRemoveAt(&p->sOpen,Html5StackIndex(p,pOpenA));
			}
		}else if( Html5Eq(zName,"nobr") && Html5NameInScope(p,"nobr") ){
			Html5Adoption(p,"nobr");
		}
		/*
		 * The two tags that switch namespace.  They reconstruct the formatting
		 * elements like any other body content and then stop being HTML.
		 */
		if( Html5Eq(zName,"svg") || Html5Eq(zName,"math") ){
			Html5ColgroupImplied(p,zName);
			p->bFoster = 1;
			Html5Reconstruct(p);
			Html5InsertForeign(p,Html5Eq(zName,"svg")
				? HTML5_NSK_SVG : HTML5_NSK_MATH);
			break;
		}
		/*
		 * A `<table>` written inside one is the source forgetting the close:
		 * the open table is closed and the new one opens beside it rather
		 * than within it.  The question is asked in SCOPE, so a table inside
		 * a `<td>` is a real nested table and this does not fire.
		 */
		if( Html5Eq(zName,"table") && Html5NameInScope(p,"table") ){
			Html5PopTo(p,"table");
		}
		Html5ColgroupImplied(p,zName);
		/*
		 * `<image>` names no element.  The spec renames the TOKEN to `img`,
		 * so what lands is a VOID `<img>` carrying the token's attributes --
		 * misnaming it also left it OPEN, swallowing the rest of the body.
		 * The rename is an HTML rule alone: an `<image>` inside `<svg>` is an
		 * SVG image, and foreign content is dispatched before this switch.
		 * And php reaches the rename by RE-RUNNING the renamed token, which
		 * a table's foster path has no re-run to hand it, so an `<image>`
		 * written straight into a table is DROPPED there where the `<img>`
		 * it would have become is fostered out -- the implied `</colgroup>`
		 * above is why the question is asked here and not at the top.
		 */
		if( Html5Eq(zName,"image") ){
			if( Html5InTableCtx(p) ){
				break;
			}
			SyBlobReset(&p->sName);
			SyBlobAppend(&p->sName,"img",sizeof("img") - 1);
			Html5EndStr(&p->sName);
			zName = Html5TokName(p);
		}
		p->bFoster = !Html5TableOwns(p,zName);
		bTableForm = Html5Eq(zName,"form") && Html5InTableCtx(p);
		Html5TableImplied(p,zName);
		Html5BodyImplied(p,zName);
		if( Html5Reconstructs(zName) ){
			Html5Reconstruct(p);
		}
		pEl = Html5InsertStart(p);
		if( pEl && bTableForm ){
			/*
			 * A `<form>` written straight into a table is inserted and popped
			 * at once, so the table keeps the empty element and everything
			 * after it is fostered out rather than filling the form.
			 */
			Html5Pop(p);
		}
		if( pEl ){
			if( HTML5_IN(azHtml5Fmt,zName) ){
				Html5FmtPush(p,pEl);
			}else if( HTML5_IN(azHtml5FmtMark,zName) ){
				Html5FmtMarker(p);
			}
		}
		break;
	}
	case HTML5_TOK_END:
		if( Html5Eq(zName,"template") ){
			Html5TemplateEnd(p);
			break;
		}
		if( Html5Eq(zName,"body") ){
			if( Html5OpenDepth(p,"body") >= 0 ){
				Html5PopTo(p,"body");
			}
			p->iMode = HTML5_M_AFTER_BODY;
			break;
		}
		if( Html5Eq(zName,"html") ){
			if( Html5OpenDepth(p,"body") >= 0 ){
				Html5PopTo(p,"body");
			}
			p->iMode = HTML5_M_AFTER_BODY;
			break;
		}
		/*
		 * `</br>` is the one end tag that OPENS an element.  It is re-run as a
		 * `<br>` START tag carrying none of the attributes it was written
		 * with, so everything a `<br>` start tag owes -- the reconstruction,
		 * the frameset flag, and being FOSTERED out of a table -- is owed by
		 * this too, and asking the start tag rather than copying it is the
		 * only way that stays true.
		 */
		if( Html5Eq(zName,"br") ){
			p->iTok = HTML5_TOK_START;
			SyBlobReset(&p->sAttrBuf);
			SySetReset(&p->sAttr);
			return 1;
		}
		if( !Html5Eq(zName,"colgroup") ){
			Html5ColgroupImplied(p,zName);
		}
		if( HTML5_IN(azHtml5Fmt,zName) ){
			if( Html5Adoption(p,zName) == 0 ){
				Html5EndTagOther(p,zName);
			}
			break;
		}
		Html5EndTag(p,zName);
		break;
	default:
		break;
	}
	return 0;
}

/* ------------------------------------------------------------------ *
 * The entry point
 * ------------------------------------------------------------------ */

/*
 * `HTML_NO_DEFAULT_NS` asks for a tree whose ELEMENTS carry no namespace, and
 * that has to happen after the parse rather than during it: the foreign rules
 * are chosen by the current node's namespace, so a `<svg>` that never got one
 * would fold its names, miss its breakout tags and never leave the subtree.
 * So the tree is built namespaced and stripped here.  An ATTRIBUTE keeps its
 * namespace -- php answers the xlink URI for an `xlink:href` either way -- and
 * so does the declaration behind it.
 */
static void Html5StripForeignNs(xmlNodePtr pNode)
{
	xmlNodePtr pCur;
	for( pCur = pNode ; pCur ; pCur = pCur->next ){
		xmlNsPtr *ppNs;
		if( pCur->type != XML_ELEMENT_NODE ){
			continue;
		}
		/* Children first: a declaration is only free to go once nothing
		 * underneath it still points at it. */
		Html5StripForeignNs(pCur->children);
		if( Html5NsKind(pCur) != HTML5_NSK_HTML ){
			pCur->ns = 0;
		}
		ppNs = &pCur->nsDef;
		while( *ppNs ){
			xmlNsPtr pNs = *ppNs;
			if( pNs->href
			 && (Html5Eq((const char *)pNs->href,HTML5_SVG_NS)
			  || Html5Eq((const char *)pNs->href,HTML5_MATH_NS)) ){
				*ppNs = pNs->next;
				pNs->next = 0;
				xmlFreeNs(pNs);
			}else{
				ppNs = &pNs->next;
			}
		}
	}
}

/*
 * Normalize the source the way the spec's input stream does: a CRLF pair and a
 * lone CR are both one LF, so nothing downstream -- including the line and
 * column a diagnostic prints -- has to know which spelling a file used.
 */
static int Html5Normalize(SyMemBackend *pAlloc,const char *zSrc,int nSrc,SyBlob *pOut)
{
	int i;
	SyBlobInit(pOut,pAlloc);
	for( i = 0 ; i < nSrc ; ++i ){
		if( zSrc[i] == '\r' ){
			SyBlobAppend(pOut,"\n",1);
			if( i + 1 < nSrc && zSrc[i + 1] == '\n' ){
				i++;
			}
			continue;
		}
		SyBlobAppend(pOut,&zSrc[i],1);
	}
	return PH7_OK;
}
PH7_PRIVATE xmlDocPtr PH7_Html5Parse(
	SyMemBackend *pAlloc,
	const char *zSrc,int nSrc,
	int iFlags,
	const char *zEnc,
	void (*xErr)(void *,const char *,const char *,sxu32,sxu32,sxu32),
	void *pErrUser
	)
{
	html5_parser sParser;
	SyBlob sIn;
	xmlDocPtr pDoc;
	pDoc = xmlNewDoc((const xmlChar *)"1.0");
	if( pDoc == 0 ){
		return 0;
	}
	pDoc->encoding = xmlStrdup((const xmlChar *)(zEnc && zEnc[0] ? zEnc : "UTF-8"));
	pDoc->standalone = 1;
	Html5Normalize(pAlloc,zSrc,nSrc,&sIn);
	SyZero(&sParser,sizeof(sParser));
	sParser.zIn = (const unsigned char *)SyBlobData(&sIn);
	sParser.nIn = SyBlobLength(&sIn);
	sParser.iLine = 1;
	sParser.iCol = 1;
	sParser.pAlloc = pAlloc;
	sParser.pDoc = pDoc;
	sParser.iFlags = iFlags;
	sParser.iMode = HTML5_M_INITIAL;
	sParser.bFramesetOk = 1;
	sParser.xErr = xErr;
	sParser.pErrUser = pErrUser;
	SySetInit(&sParser.sOpen,pAlloc,sizeof(xmlNodePtr));
	SySetInit(&sParser.sFmt,pAlloc,sizeof(xmlNodePtr));
	SySetInit(&sParser.sTmpl,pAlloc,sizeof(int));
	SySetInit(&sParser.sAttr,pAlloc,sizeof(html5_attr));
	SyBlobInit(&sParser.sName,pAlloc);
	SyBlobInit(&sParser.sBuf,pAlloc);
	SyBlobInit(&sParser.sAttrBuf,pAlloc);
	for(;;){
		int nGuard = 0;
		sParser.bHtmlRules = 0;
		Html5NextToken(&sParser);
		if( sParser.iTok == HTML5_TOK_EOF ){
			break;
		}
		/*
		 * A mode may hand the token back once per mode it walks through, and
		 * there are seven of them; the guard is what turns a rule written
		 * wrong into a refusal rather than a hang.
		 */
		while( Html5Dispatch(&sParser) && ++nGuard < 16 ){
			;
		}
	}
	/*
	 * A document that reached the end without ever leaving the initial modes
	 * still owes its skeleton: php answers `<html><head></head><body></body>`
	 * for an empty string.
	 */
	if( (iFlags & HTML5_NOIMPLIED) == 0 ){
		if( sParser.pHtml == 0 ){
			Html5OpenHtml(&sParser,FALSE);
		}
		if( sParser.pHead == 0 ){
			while( Html5Top(&sParser) && Html5Top(&sParser) != sParser.pHtml ){
				Html5Pop(&sParser);
			}
			Html5OpenHead(&sParser,FALSE);
		}
		/*
		 * A frameset document owes no body -- it is the one shape where the
		 * root holds `head` and a sibling that is not `body`.
		 */
		if( sParser.pHtml && Html5ChildNamed(sParser.pHtml,"body") == 0
		 && Html5ChildNamed(sParser.pHtml,"frameset") == 0 ){
			while( Html5Top(&sParser) && Html5Top(&sParser) != sParser.pHtml ){
				Html5Pop(&sParser);
			}
			Html5OpenBody(&sParser,FALSE);
		}
	}
	if( iFlags & HTML5_NO_DEF_NS ){
		Html5StripForeignNs(pDoc->children);
	}
	SyBlobRelease(&sIn);
	SyBlobRelease(&sParser.sName);
	SyBlobRelease(&sParser.sBuf);
	SyBlobRelease(&sParser.sAttrBuf);
	SySetRelease(&sParser.sOpen);
	SySetRelease(&sParser.sFmt);
	SySetRelease(&sParser.sTmpl);
	SySetRelease(&sParser.sAttr);
	return pDoc;
}
#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_html5_unused;
#endif /* PH7_ENABLE_LIBXML */
