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

/* Insertion modes.  Everything past IN_BODY is decided by the open stack. */
#define HTML5_M_INITIAL      0
#define HTML5_M_BEFORE_HTML  1
#define HTML5_M_BEFORE_HEAD  2
#define HTML5_M_IN_HEAD      3
#define HTML5_M_AFTER_HEAD   4
#define HTML5_M_IN_BODY      5
#define HTML5_M_AFTER_BODY   6

/* Text-only element flavours the tree constructor arms the tokenizer with. */
#define HTML5_TEXT_NONE    0
#define HTML5_TEXT_RCDATA  1
#define HTML5_TEXT_RAW     2

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
/* These take neither: everything to the matching end tag is one text node. */
static const char * const azHtml5Raw[] = {
	"iframe","noembed","noframes","noscript","script","style","xmp"
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
/* The elements the head collects, wherever in the source they are spelled. */
static const char * const azHtml5Head[] = {
	"base","basefont","bgsound","link","meta","noframes","script","style",
	"template","title"
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
			{
				char zc = (char)Html5Lower(Html5Read(p));
				if( zc != 0 ){
					SyBlobAppend(&p->sAttrBuf,&zc,1);
				}
			}
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
						char zc = (char)Html5Read(p);
						if( zc != 0 ){
							SyBlobAppend(&p->sAttrBuf,&zc,1);
						}
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
						char zc = (char)Html5Read(p);
						if( zc != 0 ){
							SyBlobAppend(&p->sAttrBuf,&zc,1);
						}
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
		{
			char zc = (char)Html5Read(p);
			SyBlobAppend(&p->sBuf,&zc,1);
		}
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
		{
			char zc = (char)Html5Read(p);
			SyBlobAppend(&p->sBuf,&zc,1);
		}
	}
	p->iTok = HTML5_TOK_COMMENT;
}
/*
 * The text-only tokenizer the tree constructor arms by name: one text token
 * running to the matching end tag, which is then the next token.
 */
static void Html5ReadText(html5_parser *p,const char *zEnd,int bRcdata)
{
	for(;;){
		int c = Html5Peek(p,0);
		if( c < 0 ){
			break;
		}
		if( c == '<' && Html5Peek(p,1) == '/' ){
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
		{
			char zc = (char)Html5Read(p);
			if( zc != 0 ){
				SyBlobAppend(&p->sBuf,&zc,1);
			}
		}
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
		if( nEnd > (int)sizeof(zEnd) - 1 ){
			nEnd = (int)sizeof(zEnd) - 1;
		}
		SyMemcpy(SyBlobData(&p->sName),zEnd,(sxu32)nEnd);
		zEnd[nEnd] = 0;
		p->iText = HTML5_TEXT_NONE;
		if( SyBlobLength(&p->sName) > 0 ){
			Html5ReadText(p,zEnd,bRc);
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
				{
					char zc = (char)Html5Lower(Html5Read(p));
					SyBlobAppend(&p->sName,&zc,1);
				}
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
		{
			char zc = (char)Html5Lower(Html5Read(p));
			SyBlobAppend(&p->sName,&zc,1);
		}
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
static void Html5Pop(html5_parser *p)
{
	SySetPop(&p->sOpen);
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
static xmlNodePtr Html5NewElem(html5_parser *p,const char *zName)
{
	xmlNodePtr pParent = Html5Target(p);
	xmlNodePtr pEl = xmlNewDocNode(p->pDoc,0,(const xmlChar *)zName,0);
	if( pEl == 0 ){
		return 0;
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
	xmlAddChild(pParent,pEl);
	return pEl;
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
	}
	return pEl;
}
static void Html5InsertText(html5_parser *p)
{
	xmlNodePtr pParent = Html5Target(p);
	sxu32 n = SyBlobLength(&p->sBuf);
	if( n < 1 ){
		return;
	}
	/*
	 * xmlAddChild merges a text node into a preceding text sibling, which is
	 * exactly the spec's rule, so the tree never carries two adjacent ones.
	 */
	xmlAddChild(pParent,xmlNewDocTextLen(p->pDoc,
		(const xmlChar *)SyBlobData(&p->sBuf),(int)n));
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
static void Html5InsertComment(html5_parser *p,xmlNodePtr pParent)
{
	xmlNodePtr pC;
	SyBlobNullAppend(&p->sBuf);
	pC = xmlNewDocComment(p->pDoc,(const xmlChar *)SyBlobData(&p->sBuf));
	if( pC ){
		xmlAddChild(pParent,pC);
	}
}
/* The `in body` start-tag rules that are about what is ALREADY open. */
static void Html5BodyImplied(html5_parser *p,const char *zName)
{
	if( HTML5_IN(azHtml5ClosesP,zName) && Html5OpenDepth(p,"p") == 0 ){
		Html5Pop(p);
	}
	if( Html5Eq(zName,"li") && Html5OpenDepth(p,"li") == 0 ){
		Html5Pop(p);
	}
	if( (Html5Eq(zName,"dd") || Html5Eq(zName,"dt"))
	 && (Html5OpenDepth(p,"dd") == 0 || Html5OpenDepth(p,"dt") == 0) ){
		Html5Pop(p);
	}
	if( Html5Eq(zName,"option") && Html5OpenDepth(p,"option") == 0 ){
		Html5Pop(p);
	}
	if( zName[0] == 'h' && zName[1] >= '1' && zName[1] <= '6' && zName[2] == 0 ){
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
	if( Html5OpenDepth(p,"table") < 0 ){
		return;
	}
	if( bGroup || Html5Eq(zName,"caption")
	 || Html5Eq(zName,"colgroup") || Html5Eq(zName,"col") ){
		while( Html5Top(p) && !Html5Eq((const char *)Html5Top(p)->name,"table") ){
			Html5Pop(p);
		}
		return;
	}
	if( bRow || bCell ){
		if( bCell ){
			/* A cell beside an open one closes it. */
			while( Html5Top(p)
			 && (Html5Eq((const char *)Html5Top(p)->name,"td")
			  || Html5Eq((const char *)Html5Top(p)->name,"th")) ){
				Html5Pop(p);
			}
		}
		if( bRow ){
			while( Html5Top(p)
			 && !Html5Eq((const char *)Html5Top(p)->name,"table")
			 && !Html5Eq((const char *)Html5Top(p)->name,"tbody")
			 && !Html5Eq((const char *)Html5Top(p)->name,"thead")
			 && !Html5Eq((const char *)Html5Top(p)->name,"tfoot") ){
				Html5Pop(p);
			}
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
		Html5AddAttrs(p,pBody);
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
			Html5TrimLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){
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
			Html5TrimLeadingSpace(p);
		}
		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"body") ){
			Html5OpenBody(p,TRUE);
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
	default:
		break;
	}
	/* `in body`, and every table mode with it. */
	switch( p->iTok ){
	case HTML5_TOK_TEXT:
		Html5InsertText(p);
		break;
	case HTML5_TOK_COMMENT:
		Html5InsertComment(p,Html5Target(p));
		break;
	case HTML5_TOK_DOCTYPE:
		break;
	case HTML5_TOK_START:
		if( Html5Eq(zName,"html") || Html5Eq(zName,"body") ){
			break;
		}
		if( Html5Eq(zName,"head") ){
			break;
		}
		Html5TableImplied(p,zName);
		Html5BodyImplied(p,zName);
		Html5InsertStart(p);
		break;
	case HTML5_TOK_END:
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
	sParser.xErr = xErr;
	sParser.pErrUser = pErrUser;
	SySetInit(&sParser.sOpen,pAlloc,sizeof(xmlNodePtr));
	SySetInit(&sParser.sAttr,pAlloc,sizeof(html5_attr));
	SyBlobInit(&sParser.sName,pAlloc);
	SyBlobInit(&sParser.sBuf,pAlloc);
	SyBlobInit(&sParser.sAttrBuf,pAlloc);
	for(;;){
		int nGuard = 0;
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
		if( sParser.pHtml && Html5ChildNamed(sParser.pHtml,"body") == 0 ){
			while( Html5Top(&sParser) && Html5Top(&sParser) != sParser.pHtml ){
				Html5Pop(&sParser);
			}
			Html5OpenBody(&sParser,FALSE);
		}
	}
	SyBlobRelease(&sIn);
	SyBlobRelease(&sParser.sName);
	SyBlobRelease(&sParser.sBuf);
	SyBlobRelease(&sParser.sAttrBuf);
	SySetRelease(&sParser.sOpen);
	SySetRelease(&sParser.sAttr);
	return pDoc;
}
#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_html5_unused;
#endif /* PH7_ENABLE_LIBXML */
