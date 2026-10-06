# src/ph7/vm_dom_html5.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1954/2148 lines (90.97%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    4 | ` */` |
|       - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <libxml/tree.h>` |
|       - |    8 |  |
|       - |    9 | `/* Defined beside the DOM's own namespace bookkeeping: a binding the ENGINE` |
|       - |   10 | ` * minted to name something, which no attribute map lists. */` |
|       - |   11 | `PH7_PRIVATE void PH7_DomNsMarkUnspelt(xmlNsPtr pNs);` |
|       - |   12 |  |
|       - |   13 | `/*` |
|       - |   14 | ``  * The named character references, as php's own `get_html_translation_table()` `` |
|       - |   15 | ` * spells them for the HTML 4.01 document type: 253 names, sorted so a lookup` |
|       - |   16 | `` * is a binary search.  libxml has this table too, behind `htmlEntityLookup`,`` |
|       - |   17 | ` * but that name is deprecated in 2.13 and the /WX build refuses it -- and a` |
|       - |   18 | ` * table that moves with the linked libxml version is the wrong thing for a` |
|       - |   19 | ` * parser whose answers are supposed to be php's.` |
|       - |   20 | ` */` |
|       - |   21 | `static const struct { const char *zName; sxu32 cp; } aHtml5Ent[] = {` |
|       - |   22 | `	{"#039",0x0027},{"AElig",0x00C6},{"Aacute",0x00C1},{"Acirc",0x00C2},` |
|       - |   23 | `	{"Agrave",0x00C0},{"Alpha",0x0391},{"Aring",0x00C5},{"Atilde",0x00C3},` |
|       - |   24 | `	{"Auml",0x00C4},{"Beta",0x0392},{"Ccedil",0x00C7},{"Chi",0x03A7},` |
|       - |   25 | `	{"Dagger",0x2021},{"Delta",0x0394},{"ETH",0x00D0},{"Eacute",0x00C9},` |
|       - |   26 | `	{"Ecirc",0x00CA},{"Egrave",0x00C8},{"Epsilon",0x0395},{"Eta",0x0397},` |
|       - |   27 | `	{"Euml",0x00CB},{"Gamma",0x0393},{"Iacute",0x00CD},{"Icirc",0x00CE},` |
|       - |   28 | `	{"Igrave",0x00CC},{"Iota",0x0399},{"Iuml",0x00CF},{"Kappa",0x039A},` |
|       - |   29 | `	{"Lambda",0x039B},{"Mu",0x039C},{"Ntilde",0x00D1},{"Nu",0x039D},` |
|       - |   30 | `	{"OElig",0x0152},{"Oacute",0x00D3},{"Ocirc",0x00D4},{"Ograve",0x00D2},` |
|       - |   31 | `	{"Omega",0x03A9},{"Omicron",0x039F},{"Oslash",0x00D8},{"Otilde",0x00D5},` |
|       - |   32 | `	{"Ouml",0x00D6},{"Phi",0x03A6},{"Pi",0x03A0},{"Prime",0x2033},` |
|       - |   33 | `	{"Psi",0x03A8},{"Rho",0x03A1},{"Scaron",0x0160},{"Sigma",0x03A3},` |
|       - |   34 | `	{"THORN",0x00DE},{"Tau",0x03A4},{"Theta",0x0398},{"Uacute",0x00DA},` |
|       - |   35 | `	{"Ucirc",0x00DB},{"Ugrave",0x00D9},{"Upsilon",0x03A5},{"Uuml",0x00DC},` |
|       - |   36 | `	{"Xi",0x039E},{"Yacute",0x00DD},{"Yuml",0x0178},{"Zeta",0x0396},` |
|       - |   37 | `	{"aacute",0x00E1},{"acirc",0x00E2},{"acute",0x00B4},{"aelig",0x00E6},` |
|       - |   38 | `	{"agrave",0x00E0},{"alefsym",0x2135},{"alpha",0x03B1},{"amp",0x0026},` |
|       - |   39 | `	{"and",0x2227},{"ang",0x2220},{"aring",0x00E5},{"asymp",0x2248},` |
|       - |   40 | `	{"atilde",0x00E3},{"auml",0x00E4},{"bdquo",0x201E},{"beta",0x03B2},` |
|       - |   41 | `	{"brvbar",0x00A6},{"bull",0x2022},{"cap",0x2229},{"ccedil",0x00E7},` |
|       - |   42 | `	{"cedil",0x00B8},{"cent",0x00A2},{"chi",0x03C7},{"circ",0x02C6},` |
|       - |   43 | `	{"clubs",0x2663},{"cong",0x2245},{"copy",0x00A9},{"crarr",0x21B5},` |
|       - |   44 | `	{"cup",0x222A},{"curren",0x00A4},{"dArr",0x21D3},{"dagger",0x2020},` |
|       - |   45 | `	{"darr",0x2193},{"deg",0x00B0},{"delta",0x03B4},{"diams",0x2666},` |
|       - |   46 | `	{"divide",0x00F7},{"eacute",0x00E9},{"ecirc",0x00EA},{"egrave",0x00E8},` |
|       - |   47 | `	{"empty",0x2205},{"emsp",0x2003},{"ensp",0x2002},{"epsilon",0x03B5},` |
|       - |   48 | `	{"equiv",0x2261},{"eta",0x03B7},{"eth",0x00F0},{"euml",0x00EB},` |
|       - |   49 | `	{"euro",0x20AC},{"exist",0x2203},{"fnof",0x0192},{"forall",0x2200},` |
|       - |   50 | `	{"frac12",0x00BD},{"frac14",0x00BC},{"frac34",0x00BE},{"frasl",0x2044},` |
|       - |   51 | `	{"gamma",0x03B3},{"ge",0x2265},{"gt",0x003E},{"hArr",0x21D4},` |
|       - |   52 | `	{"harr",0x2194},{"hearts",0x2665},{"hellip",0x2026},{"iacute",0x00ED},` |
|       - |   53 | `	{"icirc",0x00EE},{"iexcl",0x00A1},{"igrave",0x00EC},{"image",0x2111},` |
|       - |   54 | `	{"infin",0x221E},{"int",0x222B},{"iota",0x03B9},{"iquest",0x00BF},` |
|       - |   55 | `	{"isin",0x2208},{"iuml",0x00EF},{"kappa",0x03BA},{"lArr",0x21D0},` |
|       - |   56 | `	{"lambda",0x03BB},{"lang",0x2329},{"laquo",0x00AB},{"larr",0x2190},` |
|       - |   57 | `	{"lceil",0x2308},{"ldquo",0x201C},{"le",0x2264},{"lfloor",0x230A},` |
|       - |   58 | `	{"lowast",0x2217},{"loz",0x25CA},{"lrm",0x200E},{"lsaquo",0x2039},` |
|       - |   59 | `	{"lsquo",0x2018},{"lt",0x003C},{"macr",0x00AF},{"mdash",0x2014},` |
|       - |   60 | `	{"micro",0x00B5},{"middot",0x00B7},{"minus",0x2212},{"mu",0x03BC},` |
|       - |   61 | `	{"nabla",0x2207},{"nbsp",0x00A0},{"ndash",0x2013},{"ne",0x2260},` |
|       - |   62 | `	{"ni",0x220B},{"not",0x00AC},{"notin",0x2209},{"nsub",0x2284},` |
|       - |   63 | `	{"ntilde",0x00F1},{"nu",0x03BD},{"oacute",0x00F3},{"ocirc",0x00F4},` |
|       - |   64 | `	{"oelig",0x0153},{"ograve",0x00F2},{"oline",0x203E},{"omega",0x03C9},` |
|       - |   65 | `	{"omicron",0x03BF},{"oplus",0x2295},{"or",0x2228},{"ordf",0x00AA},` |
|       - |   66 | `	{"ordm",0x00BA},{"oslash",0x00F8},{"otilde",0x00F5},{"otimes",0x2297},` |
|       - |   67 | `	{"ouml",0x00F6},{"para",0x00B6},{"part",0x2202},{"permil",0x2030},` |
|       - |   68 | `	{"perp",0x22A5},{"phi",0x03C6},{"pi",0x03C0},{"piv",0x03D6},` |
|       - |   69 | `	{"plusmn",0x00B1},{"pound",0x00A3},{"prime",0x2032},{"prod",0x220F},` |
|       - |   70 | `	{"prop",0x221D},{"psi",0x03C8},{"quot",0x0022},{"rArr",0x21D2},` |
|       - |   71 | `	{"radic",0x221A},{"rang",0x232A},{"raquo",0x00BB},{"rarr",0x2192},` |
|       - |   72 | `	{"rceil",0x2309},{"rdquo",0x201D},{"real",0x211C},{"reg",0x00AE},` |
|       - |   73 | `	{"rfloor",0x230B},{"rho",0x03C1},{"rlm",0x200F},{"rsaquo",0x203A},` |
|       - |   74 | `	{"rsquo",0x2019},{"sbquo",0x201A},{"scaron",0x0161},{"sdot",0x22C5},` |
|       - |   75 | `	{"sect",0x00A7},{"shy",0x00AD},{"sigma",0x03C3},{"sigmaf",0x03C2},` |
|       - |   76 | `	{"sim",0x223C},{"spades",0x2660},{"sub",0x2282},{"sube",0x2286},` |
|       - |   77 | `	{"sum",0x2211},{"sup",0x2283},{"sup1",0x00B9},{"sup2",0x00B2},` |
|       - |   78 | `	{"sup3",0x00B3},{"supe",0x2287},{"szlig",0x00DF},{"tau",0x03C4},` |
|       - |   79 | `	{"there4",0x2234},{"theta",0x03B8},{"thetasym",0x03D1},{"thinsp",0x2009},` |
|       - |   80 | `	{"thorn",0x00FE},{"tilde",0x02DC},{"times",0x00D7},{"trade",0x2122},` |
|       - |   81 | `	{"uArr",0x21D1},{"uacute",0x00FA},{"uarr",0x2191},{"ucirc",0x00FB},` |
|       - |   82 | `	{"ugrave",0x00F9},{"uml",0x00A8},{"upsih",0x03D2},{"upsilon",0x03C5},` |
|       - |   83 | `	{"uuml",0x00FC},{"weierp",0x2118},{"xi",0x03BE},{"yacute",0x00FD},` |
|       - |   84 | `	{"yen",0x00A5},{"yuml",0x00FF},{"zeta",0x03B6},{"zwj",0x200D},` |
|       - |   85 | `	{"zwnj",0x200C},` |
|       - |   86 | `};` |
|       - |   87 | `/* An ordering comparator over two NUL-terminated names; sx has none, and` |
|       - |   88 | ` * SyStrncmp needs a length that would have to come from somewhere. */` |
|      90 |   89 | `static int Html5StrCmp(const char *zLeft,const char *zRight)` |
|       1 |   90 | `{` |
|     167 |   91 | `	while( *zLeft != 0 && *zLeft == *zRight ){` |
|      77 |   92 | `		zLeft++;` |
|      77 |   93 | `		zRight++;` |
|       1 |   94 | `	}` |
|      91 |   95 | `	return (int)(unsigned char)*zLeft - (int)(unsigned char)*zRight;` |
|       1 |   96 | `}` |
|      12 |   97 | `static sxu32 Html5EntLookup(const char *zName)` |
|       1 |   98 | `{` |
|      13 |   99 | `	int iLo = 0,iHi = (int)SX_ARRAYSIZE(aHtml5Ent) - 1;` |
|      93 |  100 | `	while( iLo <= iHi ){` |
|      91 |  101 | `		int iMid = (iLo + iHi) / 2;` |
|      91 |  102 | `		int iCmp = Html5StrCmp(aHtml5Ent[iMid].zName,zName);` |
|      91 |  103 | `		if( iCmp == 0 ){` |
|      11 |  104 | `			return aHtml5Ent[iMid].cp;` |
|       - |  105 | `		}` |
|      81 |  106 | `		if( iCmp < 0 ){` |
|      31 |  107 | `			iLo = iMid + 1;` |
|      16 |  108 | `		}else{` |
|      51 |  109 | `			iHi = iMid - 1;` |
|       - |  110 | `		}` |
|       1 |  111 | `	}` |
|       3 |  112 | `	return 0;` |
|       7 |  113 | `}` |
|       - |  114 |  |
|       - |  115 | `/*` |
|       - |  116 | `` * The HTML5 tokenizer and tree constructor behind `Dom\HTMLDocument`'s two`` |
|       - |  117 | ` * parsing producers.` |
|       - |  118 | ` *` |
|       - |  119 | ` * php 8.4 parses HTML for the namespaced tree through lexbor, and the shape it` |
|       - |  120 | `` * answers is NOT libxml's `htmlReadMemory`: libxml mints no `<head>` for a`` |
|       - |  121 | `` * document that states none and no `<tbody>` around a bare `<tr>`, where the`` |
|       - |  122 | ` * HTML5 tree construction algorithm mints both.  Nothing in libxml can be asked` |
|       - |  123 | ` * for that shape, so the algorithm is written here, over libxml's tree as the` |
|       - |  124 | ` * output type -- the same tree every other door of this family already reads.` |
|       - |  125 | ` *` |
|       - |  126 | ` * What the file owes the rest of the engine is one entry point, PH7_Html5Parse:` |
|       - |  127 | ` * source bytes in, an xmlDocPtr out, and every diagnostic handed back through a` |
|       - |  128 | ` * callback so the two doors can name themselves in it.  It never touches a` |
|       - |  129 | ` * ph7_context, which is what keeps the algorithm testable against the one thing` |
|       - |  130 | ` * that decides it -- the tree a document produces.` |
|       - |  131 | ` *` |
|       - |  132 | ` * The tokenizer is the spec's states reachable from an HTML (never foreign)` |
|       - |  133 | ` * insertion point: data, the tag and attribute states, comments, doctype, and` |
|       - |  134 | `` * the two text-only variants (RCDATA for `<title>`/`<textarea>`, RAWTEXT for`` |
|       - |  135 | `` * `<style>`, `<script>` and their kin) that the tree constructor arms by name.`` |
|       - |  136 | ` * Character references resolve through the name table at the top of this file,` |
|       - |  137 | ` * plus the numeric forms and their C1 replacement table.` |
|       - |  138 | ` *` |
|       - |  139 | `` * The tree constructor runs the insertion modes down to `in body` and then`` |
|       - |  140 | ` * reads the OPEN ELEMENT STACK rather than carrying separate table modes: the` |
|       - |  141 | `` * only thing `in table` and its three children decide is which implied element`` |
|       - |  142 | `` * a `<tr>` or a `<td>` needs above it, and the stack already says.`` |
|       - |  143 | ` */` |
|       - |  144 |  |
|       - |  145 | `#define HTML5_NS "http://www.w3.org/1999/xhtml"` |
|       - |  146 | `/*` |
|       - |  147 | `` * The two namespaces a `<svg>` or a `<math>` start tag switches the tree into,`` |
|       - |  148 | ` * and the two an adjusted attribute name is moved into.  A foreign element is` |
|       - |  149 | `` * namespaced whatever the document's options say: `HTML_NO_DEFAULT_NS` speaks`` |
|       - |  150 | ` * about the HTML declaration alone, because an SVG subtree that lost its` |
|       - |  151 | ` * namespace would stop being SVG rather than stop being decorated.` |
|       - |  152 | ` */` |
|       - |  153 | `#define HTML5_SVG_NS   "http://www.w3.org/2000/svg"` |
|       - |  154 | `#define HTML5_MATH_NS  "http://www.w3.org/1998/Math/MathML"` |
|       - |  155 | `#define HTML5_XLINK_NS "http://www.w3.org/1999/xlink"` |
|       - |  156 | `#define HTML5_XML_NS   "http://www.w3.org/XML/1998/namespace"` |
|       - |  157 | `/* ...as the three answers to "which rules does this element parse under". */` |
|       - |  158 | `#define HTML5_NSK_HTML 0` |
|       - |  159 | `#define HTML5_NSK_SVG  1` |
|       - |  160 | `#define HTML5_NSK_MATH 2` |
|       - |  161 |  |
|       - |  162 | `/* Parse flags; the door maps php's option bits onto these. */` |
|       - |  163 | `#define HTML5_NOIMPLIED    0x01   /* LIBXML_HTML_NOIMPLIED */` |
|       - |  164 | `#define HTML5_NO_DEF_NS    0x02   /* Dom\HTML_NO_DEFAULT_NS */` |
|       - |  165 |  |
|       - |  166 | `/* Token kinds */` |
|       - |  167 | `#define HTML5_TOK_EOF      0` |
|       - |  168 | `#define HTML5_TOK_TEXT     1` |
|       - |  169 | `#define HTML5_TOK_START    2` |
|       - |  170 | `#define HTML5_TOK_END      3` |
|       - |  171 | `#define HTML5_TOK_COMMENT  4` |
|       - |  172 | `#define HTML5_TOK_DOCTYPE  5` |
|       - |  173 |  |
|       - |  174 | `/*` |
|       - |  175 | ` * Insertion modes.  The table modes past IN_BODY are decided by the open` |
|       - |  176 | ` * stack, but the three frameset ones cannot be: a frameset document has no` |
|       - |  177 | ` * body at all, and what it accepts is a closed list rather than a question` |
|       - |  178 | ` * about what is open.` |
|       - |  179 | ` */` |
|       - |  180 | `#define HTML5_M_INITIAL      0` |
|       - |  181 | `#define HTML5_M_BEFORE_HTML  1` |
|       - |  182 | `#define HTML5_M_BEFORE_HEAD  2` |
|       - |  183 | `#define HTML5_M_IN_HEAD      3` |
|       - |  184 | `#define HTML5_M_AFTER_HEAD   4` |
|       - |  185 | `#define HTML5_M_IN_BODY      5` |
|       - |  186 | `#define HTML5_M_AFTER_BODY   6` |
|       - |  187 | `#define HTML5_M_IN_FRAMESET  7` |
|       - |  188 | `#define HTML5_M_AFTER_FRAMESET 8` |
|       - |  189 | `#define HTML5_M_AFTER_AFTER_FRAMESET 9` |
|       - |  190 | `#define HTML5_M_AFTER_AFTER_BODY 12` |
|       - |  191 | `#define HTML5_M_IN_TEMPLATE  10` |
|       - |  192 | `/*` |
|       - |  193 | `` * `in head noscript` cannot be answered from the open stack either: what it`` |
|       - |  194 | ` * accepts is a closed list, and everything else CLOSES the noscript and is` |
|       - |  195 | ` * re-run one mode out.` |
|       - |  196 | ` */` |
|       - |  197 | `#define HTML5_M_IN_HEAD_NOSCRIPT 11` |
|       - |  198 |  |
|       - |  199 | `/* Text-only element flavours the tree constructor arms the tokenizer with. */` |
|       - |  200 | `#define HTML5_TEXT_NONE    0` |
|       - |  201 | `#define HTML5_TEXT_RCDATA  1` |
|       - |  202 | `#define HTML5_TEXT_RAW     2` |
|       - |  203 | `/*` |
|       - |  204 | `` * `<plaintext>` has no end tag at all: the state it switches the tokenizer`` |
|       - |  205 | ` * into is never left, so every remaining byte of the document is its text.` |
|       - |  206 | ` */` |
|       - |  207 | `#define HTML5_TEXT_PLAIN   3` |
|       - |  208 |  |
|       - |  209 | `typedef struct html5_attr html5_attr;` |
|       - |  210 | `struct html5_attr {` |
|       - |  211 | `	sxu32 nNameOfs;  /* offset into sAttrBuf of the NUL-terminated name  */` |
|       - |  212 | `	sxu32 nValOfs;   /* offset into sAttrBuf of the NUL-terminated value */` |
|       - |  213 | `};` |
|       - |  214 |  |
|       - |  215 | `typedef struct html5_parser html5_parser;` |
|       - |  216 | `struct html5_parser {` |
|       - |  217 | `	const unsigned char *zIn;      /* normalized source (CRLF and CR folded) */` |
|       - |  218 | `	sxu32 nIn;` |
|       - |  219 | `	sxu32 iPos;` |
|       - |  220 | `	sxu32 iLine;                   /* 1-based, of the NEXT byte */` |
|       - |  221 | `	sxu32 iCol;                    /* 1-based, of the NEXT byte */` |
|       - |  222 | `	SyMemBackend *pAlloc;` |
|       - |  223 | `	xmlDocPtr pDoc;` |
|       - |  224 | `	xmlNsPtr pNs;                  /* the XHTML declaration of the current root */` |
|       - |  225 | `	int iFlags;` |
|       - |  226 | `	int iMode;` |
|       - |  227 | `	int iText;                     /* armed text flavour for the next token   */` |
|       - |  228 | `	SySet sOpen;                   /* xmlNodePtr, the open element stack      */` |
|       - |  229 | `	SySet sFmt;                    /* xmlNodePtr, the active formatting list;` |
|       - |  230 | `	                                * a 0 entry is the spec's MARKER          */` |
|       - |  231 | `	SySet sTmpl;                   /* int, the insertion mode each open` |
|       - |  232 | ``	                                * `<template>` will be handed back        */`` |
|       - |  233 | `	xmlNodePtr pHead;` |
|       - |  234 | `	xmlNodePtr pHtml;` |
|       - |  235 | `	/* the current token */` |
|       - |  236 | `	int iTok;` |
|       - |  237 | `	SyBlob sName;                  /* tag or doctype name, folded, NUL-ended  */` |
|       - |  238 | `	SyBlob sBuf;                   /* text or comment data                    */` |
|       - |  239 | `	SyBlob sPubId;                 /* the doctype's public identifier         */` |
|       - |  240 | `	SyBlob sSysId;                 /* ... and its system identifier           */` |
|       - |  241 | `	int bDocSys;                   /* was a system identifier spelled at all? */` |
|       - |  242 | `	int bForceQuirks;              /* THIS doctype's force-quirks flag        */` |
|       - |  243 | `	int bQuirks;                   /* the DOCUMENT is in quirks mode          */` |
|       - |  244 | `	SyBlob sAttrBuf;` |
|       - |  245 | `	SySet sAttr;` |
|       - |  246 | `	int bSelfClose;` |
|       - |  247 | `	int bTextTok;                  /* the token is an armed element's TEXT     */` |
|       - |  248 | `	int bFoster;                   /* this token's content leaves the table    */` |
|       - |  249 | ``	int bFramesetOk;               /* may a `<frameset>` still replace the body */`` |
|       - |  250 | `	int bHtmlRules;  /* this token skips the namespace question exactly once */` |
|       - |  251 | ``	int bCloseP;     /* a `</p>` minted a `<p>`; close it once the token is done */`` |
|       - |  252 | `	sxu32 iTokLine,iTokCol,iTokCol2;  /* the NAME's span, which is what php prints */` |
|       - |  253 | `	void (*xErr)(void *,const char *,const char *,sxu32,sxu32,sxu32);` |
|       - |  254 | `	void *pErrUser;` |
|       - |  255 | `};` |
|       - |  256 |  |
|       - |  257 | `/*` |
|       - |  258 | ` * The elements that never take children, so a start tag is the whole element` |
|       - |  259 | ` * and an end tag for one is ignored.` |
|       - |  260 | ` */` |
|       - |  261 | `static const char * const azHtml5Void[] = {` |
|       - |  262 | `	"area","base","basefont","bgsound","br","col","embed","frame","hr","img",` |
|       - |  263 | `	"input","keygen","link","meta","param","source","track","wbr"` |
|       - |  264 | `};` |
|       - |  265 | ``/* `<title>` and `<textarea>` take character references but no markup. */`` |
|       - |  266 | `static const char * const azHtml5Rcdata[] = { "textarea","title" };` |
|       - |  267 | `/*` |
|       - |  268 | ` * These take neither: everything to the matching end tag is one text node.` |
|       - |  269 | `` * `<noscript>` is NOT here.  It is RAWTEXT only where the scripting flag is`` |
|       - |  270 | ` * set, and this parser -- like php's -- parses with scripting DISABLED, so a` |
|       - |  271 | `` * `<noscript>` holds markup: in the body an ordinary element, in the head the`` |
|       - |  272 | ` * insertion mode of its own below.` |
|       - |  273 | ` */` |
|       - |  274 | `static const char * const azHtml5Raw[] = {` |
|       - |  275 | `	"iframe","noembed","noframes","script","style","xmp"` |
|       - |  276 | `};` |
|       - |  277 | `/*` |
|       - |  278 | `` * The head elements `in head noscript` keeps INSIDE the noscript.  It is not`` |
|       - |  279 | `` * azHtml5Head: a `<base>`, `<script>`, `<template>` or `<title>` written there`` |
|       - |  280 | ` * is the mode's anything-else branch, so it closes the noscript and lands in` |
|       - |  281 | ` * the head beside it.` |
|       - |  282 | ` */` |
|       - |  283 | `static const char * const azHtml5HeadNoscript[] = {` |
|       - |  284 | `	"basefont","bgsound","link","meta","noframes","style"` |
|       - |  285 | `};` |
|       - |  286 | `/*` |
|       - |  287 | `` * A start tag that closes an open `<p>`.  This is the spec's list verbatim: it`` |
|       - |  288 | `` * is the one piece of `in body` that cannot be derived from the stack, because`` |
|       - |  289 | ` * it is a property of the tag being opened rather than of what is open.` |
|       - |  290 | ` *` |
|       - |  291 | `` * `table` is the one row with a condition on it -- it closes a `<p>` only when`` |
|       - |  292 | ` * the document is NOT in quirks mode -- and it is the only thing quirks mode` |
|       - |  293 | `` * decides in this parser.  Swept: of the whole list, `table` under`` |
|       - |  294 | `` * `<body><p>a<NAME>` is the only name whose answer moves with the doctype.`` |
|       - |  295 | ` */` |
|       - |  296 | `static const char * const azHtml5ClosesP[] = {` |
|       - |  297 | `	"address","article","aside","blockquote","center","details","dialog","dir",` |
|       - |  298 | `	"div","dl","fieldset","figcaption","figure","footer","form","h1","h2","h3",` |
|       - |  299 | `	"h4","h5","h6","header","hgroup","hr","li","main","menu","nav","ol","p",` |
|       - |  300 | `	"plaintext","pre","search","section","summary","table","ul","xmp"` |
|       - |  301 | `};` |
|       - |  302 | `/*` |
|       - |  303 | ` * A doctype that puts the document in QUIRKS mode.  The three lists below are` |
|       - |  304 | ` * matched against the identifiers LOWERCASED, because every comparison the` |
|       - |  305 | `` * question makes is ASCII case-insensitive -- `-//w3c//dtd html 4.01`` |
|       - |  306 | `` * transitional//en` is the same answer as the spelling with capitals, and the`` |
|       - |  307 | `` * IBM system identifier answers under `HTTP://WWW.IBM.COM/...` too.`` |
|       - |  308 | ` *` |
|       - |  309 | `` * `azHtml5QuirksPubPfx` is matched as a PREFIX, so a public identifier that`` |
|       - |  310 | `` * carries a trailing `//EN` still answers; the other two are matched whole.`` |
|       - |  311 | ` * Swept out of php rather than read off the spec: all 55 prefixes were asked` |
|       - |  312 | ` * with and without a system identifier, in both cases, and the near misses` |
|       - |  313 | `` * (`-//IETF//DTD HTML 4.0//EN`, `-//W3C//DTD HTML 3.3//EN`, a leading space)`` |
|       - |  314 | ` * were asked too, to prove the list is not merely a substring test.` |
|       - |  315 | ` */` |
|       - |  316 | `static const char * const azHtml5QuirksPubPfx[] = {` |
|       - |  317 | `	"+//silmaril//dtd html pro v0r11 19970101//",` |
|       - |  318 | `	"-//as//dtd html 3.0 aswedit + extensions//",` |
|       - |  319 | `	"-//advasoft ltd//dtd html 3.0 aswedit + extensions//",` |
|       - |  320 | `	"-//ietf//dtd html 2.0 level 1//",` |
|       - |  321 | `	"-//ietf//dtd html 2.0 level 2//",` |
|       - |  322 | `	"-//ietf//dtd html 2.0 strict level 1//",` |
|       - |  323 | `	"-//ietf//dtd html 2.0 strict level 2//",` |
|       - |  324 | `	"-//ietf//dtd html 2.0 strict//",` |
|       - |  325 | `	"-//ietf//dtd html 2.0//",` |
|       - |  326 | `	"-//ietf//dtd html 2.1e//",` |
|       - |  327 | `	"-//ietf//dtd html 3.0//",` |
|       - |  328 | `	"-//ietf//dtd html 3.2 final//",` |
|       - |  329 | `	"-//ietf//dtd html 3.2//",` |
|       - |  330 | `	"-//ietf//dtd html 3//",` |
|       - |  331 | `	"-//ietf//dtd html level 0//",` |
|       - |  332 | `	"-//ietf//dtd html level 1//",` |
|       - |  333 | `	"-//ietf//dtd html level 2//",` |
|       - |  334 | `	"-//ietf//dtd html level 3//",` |
|       - |  335 | `	"-//ietf//dtd html strict level 0//",` |
|       - |  336 | `	"-//ietf//dtd html strict level 1//",` |
|       - |  337 | `	"-//ietf//dtd html strict level 2//",` |
|       - |  338 | `	"-//ietf//dtd html strict level 3//",` |
|       - |  339 | `	"-//ietf//dtd html strict//",` |
|       - |  340 | `	"-//ietf//dtd html//",` |
|       - |  341 | `	"-//metrius//dtd metrius presentational//",` |
|       - |  342 | `	"-//microsoft//dtd internet explorer 2.0 html strict//",` |
|       - |  343 | `	"-//microsoft//dtd internet explorer 2.0 html//",` |
|       - |  344 | `	"-//microsoft//dtd internet explorer 2.0 tables//",` |
|       - |  345 | `	"-//microsoft//dtd internet explorer 3.0 html strict//",` |
|       - |  346 | `	"-//microsoft//dtd internet explorer 3.0 html//",` |
|       - |  347 | `	"-//microsoft//dtd internet explorer 3.0 tables//",` |
|       - |  348 | `	"-//netscape comm. corp.//dtd html//",` |
|       - |  349 | `	"-//netscape comm. corp.//dtd strict html//",` |
|       - |  350 | `	"-//o'reilly and associates//dtd html 2.0//",` |
|       - |  351 | `	"-//o'reilly and associates//dtd html extended 1.0//",` |
|       - |  352 | `	"-//o'reilly and associates//dtd html extended relaxed 1.0//",` |
|       - |  353 | `	"-//sq//dtd html 2.0 hotmetal + extensions//",` |
|       - |  354 | `	"-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//",` |
|       - |  355 | `	"-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//",` |
|       - |  356 | `	"-//spyglass//dtd html 2.0 extended//",` |
|       - |  357 | `	"-//sun microsystems corp.//dtd hotjava html//",` |
|       - |  358 | `	"-//sun microsystems corp.//dtd hotjava strict html//",` |
|       - |  359 | `	"-//w3c//dtd html 3 1995-03-24//",` |
|       - |  360 | `	"-//w3c//dtd html 3.2 draft//",` |
|       - |  361 | `	"-//w3c//dtd html 3.2 final//",` |
|       - |  362 | `	"-//w3c//dtd html 3.2//",` |
|       - |  363 | `	"-//w3c//dtd html 3.2s draft//",` |
|       - |  364 | `	"-//w3c//dtd html 4.0 frameset//",` |
|       - |  365 | `	"-//w3c//dtd html 4.0 transitional//",` |
|       - |  366 | `	"-//w3c//dtd html experimental 19960712//",` |
|       - |  367 | `	"-//w3c//dtd html experimental 970421//",` |
|       - |  368 | `	"-//w3c//dtd w3 html//",` |
|       - |  369 | `	"-//w3o//dtd w3 html 3.0//",` |
|       - |  370 | `	"-//webtechs//dtd mozilla html 2.0//",` |
|       - |  371 | `	"-//webtechs//dtd mozilla html//"` |
|       - |  372 | `};` |
|       - |  373 | `/* The public identifiers that answer only when spelled WHOLE. */` |
|       - |  374 | `static const char * const azHtml5QuirksPubEq[] = {` |
|       - |  375 | `	"-//w3o//dtd w3 html strict 3.0//en//",` |
|       - |  376 | `	"-/w3c/dtd html 4.0 transitional/en",` |
|       - |  377 | `	"html"` |
|       - |  378 | `};` |
|       - |  379 | `/*` |
|       - |  380 | ` * The two prefixes that answer only when NO system identifier was spelled.` |
|       - |  381 | ` * Spelling one turns them into the spec's LIMITED quirks, which is not quirks` |
|       - |  382 | ` * for the one question this parser asks -- so nothing here has to name it.` |
|       - |  383 | ` */` |
|       - |  384 | `static const char * const azHtml5QuirksPubNoSys[] = {` |
|       - |  385 | `	"-//w3c//dtd html 4.01 frameset//",` |
|       - |  386 | `	"-//w3c//dtd html 4.01 transitional//"` |
|       - |  387 | `};` |
|       - |  388 | `/* The one system identifier that answers by itself. */` |
|       - |  389 | `#define HTML5_QUIRKS_SYS \` |
|       - |  390 | `	"http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"` |
|       - |  391 |  |
|       - |  392 | `/*` |
|       - |  393 | `` * The start tags that make a later `<frameset>` unwritable -- the spec's`` |
|       - |  394 | ` * frameset-ok flag, swept out of php rather than read off the list, because` |
|       - |  395 | ``  * the two disagree in one place: php clears the flag for `<input type=HIDDEN>` `` |
|       - |  396 | ` * where the spec asks for an ASCII case-INSENSITIVE match, so the comparison` |
|       - |  397 | `` * below is the byte one php makes.  `<body>` is here too, but only its`` |
|       - |  398 | ` * explicit spelling clears the flag, so it is handled where a body is opened` |
|       - |  399 | ` * rather than by name.` |
|       - |  400 | ` */` |
|       - |  401 | `static const char * const azHtml5NoFrameset[] = {` |
|       - |  402 | `	"applet","area","br","button","dd","dt","embed","hr","iframe","image",` |
|       - |  403 | `	"img","keygen","li","listing","marquee","object","plaintext","pre",` |
|       - |  404 | `	"select","table","textarea","wbr","xmp"` |
|       - |  405 | `};` |
|       - |  406 | `/* The elements the head collects, wherever in the source they are spelled. */` |
|       - |  407 | `static const char * const azHtml5Head[] = {` |
|       - |  408 | `	"base","basefont","bgsound","link","meta","noframes","script","style",` |
|       - |  409 | `	"template","title"` |
|       - |  410 | `};` |
|       - |  411 | `/*` |
|       - |  412 | ` * The formatting elements: the ones an end tag does not simply close, because` |
|       - |  413 | ` * a document that leaves one open across a close has it REOPENED after it.` |
|       - |  414 | ` */` |
|       - |  415 | `static const char * const azHtml5Fmt[] = {` |
|       - |  416 | `	"a","b","big","code","em","font","i","nobr","s","small","strike","strong",` |
|       - |  417 | `	"tt","u"` |
|       - |  418 | `};` |
|       - |  419 | `/*` |
|       - |  420 | `` * The `special` category.  Only two questions ask it: which open element is`` |
|       - |  421 | `` * the `furthest block` an adoption moves out of, and which start tags skip the`` |
|       - |  422 | ` * reconstruction (everything here but the handful listed in Html5Reconstructs).` |
|       - |  423 | ` */` |
|       - |  424 | `static const char * const azHtml5Special[] = {` |
|       - |  425 | `	"address","applet","area","article","aside","base","basefont","bgsound",` |
|       - |  426 | `	"blockquote","body","br","button","caption","center","col","colgroup","dd",` |
|       - |  427 | `	"details","dir","div","dl","dt","embed","fieldset","figcaption","figure",` |
|       - |  428 | `	"footer","form","frame","frameset","h1","h2","h3","h4","h5","h6","head",` |
|       - |  429 | `	"header","hgroup","hr","html","iframe","img","input","keygen","li","link",` |
|       - |  430 | `	"listing","main","marquee","menu","meta","nav","noembed","noframes",` |
|       - |  431 | `	"noscript","object","ol","p","param","plaintext","pre","script","search",` |
|       - |  432 | `	"section","select","source","style","summary","table","tbody","td",` |
|       - |  433 | `	"template","textarea","tfoot","th","thead","title","tr","track","ul","wbr",` |
|       - |  434 | `	"xmp"` |
|       - |  435 | `};` |
|       - |  436 | `/* The elements a scope question stops at. */` |
|       - |  437 | `static const char * const azHtml5Scope[] = {` |
|       - |  438 | `	"applet","caption","html","marquee","object","table","td","template","th"` |
|       - |  439 | `};` |
|       - |  440 | `/*` |
|       - |  441 | ` * The elements whose contents are their own formatting world: opening one` |
|       - |  442 | ` * parks a MARKER on the list, and popping it clears back past that marker, so` |
|       - |  443 | `` * a `<b>` left open outside is not reopened inside -- nor the other way.`` |
|       - |  444 | ` */` |
|       - |  445 | `static const char * const azHtml5FmtMark[] = {` |
|       - |  446 | `	"applet","caption","marquee","object","td","th"` |
|       - |  447 | `};` |
|       - |  448 | `/*` |
|       - |  449 | ` * The special elements whose start tag reconstructs anyway.  Everything NOT` |
|       - |  450 | ` * special reconstructs; these are the exceptions on the other side, and the` |
|       - |  451 | ` * rest of the special list (the block containers, the head elements and the` |
|       - |  452 | ` * table internals) does not.` |
|       - |  453 | ` */` |
|       - |  454 | `static const char * const azHtml5ReconSpecial[] = {` |
|       - |  455 | `	"applet","area","br","button","embed","img","input","keygen","marquee",` |
|       - |  456 | `	"object","select","wbr","xmp"` |
|       - |  457 | `};` |
|       - |  458 | `/*` |
|       - |  459 | ` * The start tags a table's own insertion modes handle.  Everything else` |
|       - |  460 | ` * spelled while a table, a row group or a row is the current node is content` |
|       - |  461 | ` * the table cannot hold, and is FOSTER PARENTED -- moved out to just before` |
|       - |  462 | `` * the table rather than left inside it.  `<form>` and a hidden `<input>` are`` |
|       - |  463 | `` * on this list too but are asked for by name: `<input>` only qualifies when`` |
|       - |  464 | `` * its `type` says `hidden`.`` |
|       - |  465 | ` */` |
|       - |  466 | `static const char * const azHtml5TableOwn[] = {` |
|       - |  467 | `	"caption","col","colgroup","script","style","table","tbody","td","template",` |
|       - |  468 | `	"tfoot","th","thead","tr"` |
|       - |  469 | `};` |
|       - |  470 | `/*` |
|       - |  471 | ` * The table furniture, written where no table is open.  Each of these names` |
|       - |  472 | `` * belongs to a table's own insertion mode, and `in body` -- which is where one`` |
|       - |  473 | `` * arrives when the source forgot the `<table>`, or put the tag inside a`` |
|       - |  474 | `` * `<select>` or a `<p>` -- DROPS it and carries on, so the text after it lands`` |
|       - |  475 | ` * in whatever was already open rather than inside an element nothing can nest.` |
|       - |  476 | ` *` |
|       - |  477 | `` * `head` and `frame` are the same refusal and are answered on their own, because`` |
|       - |  478 | ` * they are dropped whether or not a table is open.` |
|       - |  479 | ` */` |
|       - |  480 | `static const char * const azHtml5NoTable[] = {` |
|       - |  481 | `	"caption","col","colgroup","tbody","td","tfoot","th","thead","tr"` |
|       - |  482 | `};` |
|       - |  483 |  |
|       - |  484 | `/* sx has no NUL-terminated comparator; every name here is one. */` |
|       - |  485 | `/*` |
|       - |  486 | ` * SyBlobNullAppend terminates a blob without counting the NUL, which is what` |
|       - |  487 | ` * every reader of one wants -- but the attribute buffer packs name after value` |
|       - |  488 | ` * after name and reads each back by offset, so its separators have to be real` |
|       - |  489 | ` * bytes or the next append lands on top of them.` |
|       - |  490 | ` */` |
|     742 |  491 | `static void Html5EndStr(SyBlob *pBlob)` |
|       1 |  492 | `{` |
|     743 |  493 | `	char zNul = 0;` |
|     743 |  494 | `	SyBlobAppend(pBlob,&zNul,1);` |
|     743 |  495 | `}` |
| 4744118 |  496 | `static int Html5Eq(const char *zLeft,const char *zRight)` |
|       1 |  497 | `{` |
| 4744119 |  498 | `	sxu32 n = SyStrlen(zLeft);` |
| 4744119 |  499 | `	return n == SyStrlen(zRight) && SyMemcmp(zLeft,zRight,n) == 0;` |
|       1 |  500 | `}` |
|  250024 |  501 | `static int Html5NameIn(const char * const *apList,int nList,const char *zName)` |
|       1 |  502 | `{` |
|       - |  503 | `	int i;` |
| 3780899 |  504 | `	for( i = 0 ; i < nList ; ++i ){` |
| 3572139 |  505 | `		if( Html5Eq(apList[i],zName) ){` |
|   41265 |  506 | `			return 1;` |
|       - |  507 | `		}` |
| 1765438 |  508 | `	}` |
|  208761 |  509 | `	return 0;` |
|  125013 |  510 | `}` |
|       - |  511 | `#define HTML5_IN(L,z) Html5NameIn((L),(int)SX_ARRAYSIZE(L),(z))` |
|       - |  512 |  |
|       - |  513 | `/* ------------------------------------------------------------------ *` |
|       - |  514 | ` * The tokenizer` |
|       - |  515 | ` * ------------------------------------------------------------------ */` |
|       - |  516 |  |
|       - |  517 | `/* The tokenizer asks the tree exactly one question -- see the CDATA branch. */` |
|       - |  518 | `static xmlNodePtr Html5Top(html5_parser *p);` |
|       - |  519 | `static int Html5NsKind(xmlNodePtr pEl);` |
|       - |  520 |  |
| 1786230 |  521 | `static int Html5Peek(html5_parser *p,sxu32 nAhead)` |
|       1 |  522 | `{` |
| 1786231 |  523 | `	return (p->iPos + nAhead < p->nIn) ? p->zIn[p->iPos + nAhead] : -1;` |
|       1 |  524 | `}` |
| 1624070 |  525 | `static int Html5Read(html5_parser *p)` |
|       1 |  526 | `{` |
|       - |  527 | `	int c;` |
| 1624071 |  528 | `	if( p->iPos >= p->nIn ){` |
|     ! 0 |  529 | `		return -1;` |
|       - |  530 | `	}` |
| 1624071 |  531 | `	c = p->zIn[p->iPos++];` |
| 1624071 |  532 | `	if( c == '\n' ){` |
|    1059 |  533 | `		p->iLine++;` |
|    1059 |  534 | `		p->iCol = 1;` |
|     530 |  535 | `	}else{` |
| 1623013 |  536 | `		p->iCol++;` |
|       - |  537 | `	}` |
| 1624071 |  538 | `	return c;` |
|  812036 |  539 | `}` |
|  134208 |  540 | `static int Html5IsSpace(int c)` |
|       1 |  541 | `{` |
|  134209 |  542 | `	return c == ' ' \|\| c == '\n' \|\| c == '\t' \|\| c == '\f' \|\| c == '\r';` |
|       1 |  543 | `}` |
|       - |  544 | `/*` |
|       - |  545 | ` * The tree constructor's whitespace is not the tokenizer's.  A run reaching a` |
|       - |  546 | ` * mode that IGNORES whitespace -- or that keeps only the leading whitespace of` |
|       - |  547 | ` * a run and hands the rest one mode out -- stops at a form feed, which php` |
|       - |  548 | `` * reads as content there: `<head>\f<title>` closes the head where`` |
|       - |  549 | `` * `<head> <title>` does not, and a `\f` before `<html>` is body text rather`` |
|       - |  550 | ` * than nothing.  It is whitespace again wherever a run is KEPT rather than` |
|       - |  551 | ` * dropped, which is why the frameset modes and the table's foster question` |
|       - |  552 | `` * still ask `Html5IsSpace`.`` |
|       - |  553 | ` */` |
|    5830 |  554 | `static int Html5IsIgnSpace(int c)` |
|       1 |  555 | `{` |
|    5831 |  556 | `	return c != '\f' && Html5IsSpace(c);` |
|       1 |  557 | `}` |
|   66444 |  558 | `static int Html5Lower(int c)` |
|       1 |  559 | `{` |
|   66445 |  560 | `	return (c >= 'A' && c <= 'Z') ? c + 32 : c;` |
|       1 |  561 | `}` |
|       - |  562 | ``/* Does the source at the cursor spell `zWord`, case-insensitively? */`` |
|     612 |  563 | `static int Html5LookWord(html5_parser *p,const char *zWord)` |
|       1 |  564 | `{` |
|       - |  565 | `	sxu32 i;` |
|    4743 |  566 | `	for( i = 0 ; zWord[i] != 0 ; ++i ){` |
|    4149 |  567 | `		int c = Html5Peek(p,i);` |
|    4149 |  568 | `		if( c < 0 \|\| Html5Lower(c) != Html5Lower((unsigned char)zWord[i]) ){` |
|      19 |  569 | `			return 0;` |
|       - |  570 | `		}` |
|    2066 |  571 | `	}` |
|     595 |  572 | `	return 1;` |
|     307 |  573 | `}` |
|   14748 |  574 | `static void Html5Skip(html5_parser *p,sxu32 n)` |
|       1 |  575 | `{` |
|   47503 |  576 | `	while( n-- > 0 ){` |
|   32755 |  577 | `		Html5Read(p);` |
|       1 |  578 | `	}` |
|   14749 |  579 | `}` |
|   12410 |  580 | `static void Html5Err(html5_parser *p,const char *zKind,const char *zName,` |
|       - |  581 | `	sxu32 iLine,sxu32 iCol,sxu32 iCol2)` |
|       1 |  582 | `{` |
|   12411 |  583 | `	if( p->xErr ){` |
|   12411 |  584 | `		p->xErr(p->pErrUser,zKind,zName,iLine,iCol,iCol2);` |
|    6205 |  585 | `	}` |
|   12411 |  586 | `}` |
|       - |  587 | `/* Append one code point as UTF-8. */` |
|      52 |  588 | `static void Html5PutUtf8(SyBlob *pOut,unsigned int c)` |
|       1 |  589 | `{` |
|       - |  590 | `	unsigned char zBuf[4];` |
|      53 |  591 | `	int n = 0;` |
|      53 |  592 | `	if( c < 0x80 ){` |
|      11 |  593 | `		zBuf[n++] = (unsigned char)c;` |
|      48 |  594 | `	}else if( c < 0x800 ){` |
|       5 |  595 | `		zBuf[n++] = (unsigned char)(0xC0 \| (c >> 6));` |
|       5 |  596 | `		zBuf[n++] = (unsigned char)(0x80 \| (c & 0x3F));` |
|      41 |  597 | `	}else if( c < 0x10000 ){` |
|      39 |  598 | `		zBuf[n++] = (unsigned char)(0xE0 \| (c >> 12));` |
|      39 |  599 | `		zBuf[n++] = (unsigned char)(0x80 \| ((c >> 6) & 0x3F));` |
|      39 |  600 | `		zBuf[n++] = (unsigned char)(0x80 \| (c & 0x3F));` |
|      20 |  601 | `	}else{` |
|     ! 0 |  602 | `		zBuf[n++] = (unsigned char)(0xF0 \| (c >> 18));` |
|     ! 0 |  603 | `		zBuf[n++] = (unsigned char)(0x80 \| ((c >> 12) & 0x3F));` |
|     ! 0 |  604 | `		zBuf[n++] = (unsigned char)(0x80 \| ((c >> 6) & 0x3F));` |
|     ! 0 |  605 | `		zBuf[n++] = (unsigned char)(0x80 \| (c & 0x3F));` |
|       - |  606 | `	}` |
|      53 |  607 | `	SyBlobAppend(pOut,(const void *)zBuf,(sxu32)n);` |
|      53 |  608 | `}` |
|       - |  609 | `/*` |
|       - |  610 | ` * One source byte into a name, an attribute, a comment or a text-state run.` |
|       - |  611 | ` * Every state but the data one REPLACES a NUL rather than passing it on: the` |
|       - |  612 | ` * byte terminates a C string, so a tag name or a comment that kept one would` |
|       - |  613 | ` * be truncated at it rather than carry it, and php's answer everywhere here` |
|       - |  614 | ` * is the replacement character.` |
|       - |  615 | ` */` |
|   56784 |  616 | `static void Html5PutByte(SyBlob *pOut,int c)` |
|       1 |  617 | `{` |
|   56785 |  618 | `	char zc = (char)c;` |
|   56785 |  619 | `	if( zc != 0 ){` |
|   56747 |  620 | `		SyBlobAppend(pOut,&zc,1);` |
|   28374 |  621 | `	}else{` |
|      39 |  622 | `		Html5PutUtf8(pOut,0xFFFD);` |
|       - |  623 | `	}` |
|   56785 |  624 | `}` |
|       - |  625 | `/*` |
|       - |  626 | ` * The numeric reference's replacement table: the C1 block is not what the` |
|       - |  627 | ` * bytes say it is, and every other refused value is the replacement character.` |
|       - |  628 | ` */` |
|       4 |  629 | `static unsigned int Html5NumRepl(unsigned int c)` |
|       1 |  630 | `{` |
|       - |  631 | `	static const unsigned int aC1[32] = {` |
|       - |  632 | `		0x20AC,0x0081,0x201A,0x0192,0x201E,0x2026,0x2020,0x2021,` |
|       - |  633 | `		0x02C6,0x2030,0x0160,0x2039,0x0152,0x008D,0x017D,0x008F,` |
|       - |  634 | `		0x0090,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,` |
|       - |  635 | `		0x02DC,0x2122,0x0161,0x203A,0x0153,0x009D,0x017E,0x0178` |
|       - |  636 | `	};` |
|       5 |  637 | `	if( c >= 0x80 && c <= 0x9F ){` |
|     ! 0 |  638 | `		return aC1[c - 0x80];` |
|       - |  639 | `	}` |
|       5 |  640 | `	if( c == 0 \|\| c > 0x10FFFF \|\| (c >= 0xD800 && c <= 0xDFFF) ){` |
|     ! 0 |  641 | `		return 0xFFFD;` |
|       - |  642 | `	}` |
|       5 |  643 | `	return c;` |
|       3 |  644 | `}` |
|       - |  645 | `/*` |
|       - |  646 | `` * A character reference at the cursor, which has NOT yet consumed the `&`.`` |
|       - |  647 | ` * Anything that does not resolve is the literal text it was spelled with,` |
|       - |  648 | ` * which is why the whole thing is written as "copy on failure".` |
|       - |  649 | ` */` |
|    1094 |  650 | `static void Html5CharRef(html5_parser *p,SyBlob *pOut,int bAttr)` |
|       1 |  651 | `{` |
|    1095 |  652 | `	sxu32 iSave = p->iPos;` |
|    1095 |  653 | `	sxu32 iLine = p->iLine,iCol = p->iCol;` |
|       - |  654 | `	int c;` |
|    1095 |  655 | `	Html5Read(p);  /* the '&' */` |
|    1095 |  656 | `	c = Html5Peek(p,0);` |
|    1095 |  657 | `	if( c == '#' ){` |
|       5 |  658 | `		unsigned int iVal = 0;` |
|       5 |  659 | `		int nDig = 0,bHex = 0;` |
|       5 |  660 | `		Html5Read(p);` |
|       5 |  661 | `		c = Html5Peek(p,0);` |
|       5 |  662 | `		if( c == 'x' \|\| c == 'X' ){` |
|       3 |  663 | `			bHex = 1;` |
|       3 |  664 | `			Html5Read(p);` |
|       1 |  665 | `		}` |
|       7 |  666 | `		for(;;){` |
|      15 |  667 | `			c = Html5Peek(p,0);` |
|      15 |  668 | `			if( c < 0 ){` |
|     ! 0 |  669 | `				break;` |
|       - |  670 | `			}` |
|      15 |  671 | `			if( c >= '0' && c <= '9' ){` |
|      11 |  672 | `				iVal = iVal * (bHex ? 16 : 10) + (unsigned int)(c - '0');` |
|      10 |  673 | `			}else if( bHex && Html5Lower(c) >= 'a' && Html5Lower(c) <= 'f' ){` |
|     ! 0 |  674 | `				iVal = iVal * 16 + (unsigned int)(Html5Lower(c) - 'a' + 10);` |
|     ! 0 |  675 | `			}else{` |
|       3 |  676 | `				break;` |
|       - |  677 | `			}` |
|      11 |  678 | `			if( iVal > 0x10FFFF ){` |
|     ! 0 |  679 | `				iVal = 0x110000;   /* clamped; the table answers FFFD for it */` |
|     ! 0 |  680 | `			}` |
|      11 |  681 | `			nDig++;` |
|      11 |  682 | `			Html5Read(p);` |
|       1 |  683 | `		}` |
|       5 |  684 | `		if( nDig < 1 ){` |
|     ! 0 |  685 | `			p->iPos = iSave;` |
|     ! 0 |  686 | `			p->iLine = iLine;` |
|     ! 0 |  687 | `			p->iCol = iCol;` |
|     ! 0 |  688 | `			Html5Read(p);` |
|     ! 0 |  689 | `			SyBlobAppend(pOut,"&",1);` |
|     ! 0 |  690 | `			return;` |
|       - |  691 | `		}` |
|       5 |  692 | `		if( Html5Peek(p,0) == ';' ){` |
|       5 |  693 | `			Html5Read(p);` |
|       2 |  694 | `		}` |
|       5 |  695 | `		Html5PutUtf8(pOut,Html5NumRepl(iVal));` |
|       5 |  696 | `		return;` |
|       - |  697 | `	}` |
|       - |  698 | `	/*` |
|       - |  699 | `	 * A named reference.  libxml's table is the HTML4 name set and is keyed by` |
|       - |  700 | `	 * the bare name, so the longest run of name characters is looked up whole:` |
|       - |  701 | `	 * the spec's longest-match walk over a 2231-name table is not reachable` |
|       - |  702 | `	 * from here, and a name this table does not hold stays literal text.` |
|       - |  703 | `	 */` |
|       - |  704 | `	{` |
|       - |  705 | `		char zName[32];` |
|    1091 |  706 | `		int n = 0;` |
|       - |  707 | `		sxu32 cEnt;` |
|    1845 |  708 | `		for(;;){` |
|    1137 |  709 | `			c = Html5Peek(p,(sxu32)n);` |
|    1137 |  710 | `			if( c < 0 \|\| n >= (int)sizeof(zName) - 1 ){` |
|    1116 |  711 | `				break;` |
|       - |  712 | `			}` |
|    1403 |  713 | `			if( !((c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z')` |
|    1054 |  714 | `			   \|\| (c >= '0' && c <= '9')) ){` |
|     267 |  715 | `				break;` |
|       - |  716 | `			}` |
|    1045 |  717 | `			zName[n++] = (char)c;` |
|       1 |  718 | `		}` |
|     533 |  719 | `		zName[n] = 0;` |
|     533 |  720 | `		c = Html5Peek(p,(sxu32)n);` |
|       - |  721 | `		/*` |
|       - |  722 | ``		 * In an attribute a reference that states no `;` and is followed by a`` |
|       - |  723 | ``		 * name character or `=` is NOT one: that rule is what keeps a query`` |
|       - |  724 | ``		 * string's `?a=1&copy=2` from becoming a copyright sign.`` |
|       - |  725 | `		 */` |
|     533 |  726 | `		if( n > 0 && (c == ';' \|\| (!bAttr && c != '=')) ){` |
|      13 |  727 | `			cEnt = Html5EntLookup(zName);` |
|      13 |  728 | `			if( cEnt == 0 && c == ';' ){` |
|       4 |  729 | `				Html5Err(p,"tokenizer","unknown-named-character-reference",` |
|       2 |  730 | `					iLine,iCol + (sxu32)n + 1,iCol + (sxu32)n + 1);` |
|       1 |  731 | `			}` |
|      13 |  732 | `			if( cEnt ){` |
|      11 |  733 | `				Html5Skip(p,(sxu32)n);` |
|      11 |  734 | `				if( Html5Peek(p,0) == ';' ){` |
|      11 |  735 | `					Html5Read(p);` |
|       5 |  736 | `				}` |
|      11 |  737 | `				Html5PutUtf8(pOut,(unsigned int)cEnt);` |
|      11 |  738 | `				return;` |
|       - |  739 | `			}` |
|       1 |  740 | `		}` |
|       - |  741 | `	}` |
|     523 |  742 | `	SyBlobAppend(pOut,"&",1);` |
|     269 |  743 | `}` |
|       - |  744 | `/* The attribute states, entered with the cursor on the first name character. */` |
|   28562 |  745 | `static void Html5ReadAttrs(html5_parser *p)` |
|       1 |  746 | `{` |
|   14657 |  747 | `	for(;;){` |
|       - |  748 | `		html5_attr sAttr;` |
|       - |  749 | `		int c;` |
|   29359 |  750 | `		while( Html5IsSpace(Html5Peek(p,0)) ){` |
|     407 |  751 | `			Html5Read(p);` |
|       1 |  752 | `		}` |
|   28953 |  753 | `		c = Html5Peek(p,0);` |
|   28953 |  754 | `		if( c < 0 \|\| c == '>' ){` |
|   14282 |  755 | `			break;` |
|       - |  756 | `		}` |
|     391 |  757 | `		if( c == '/' ){` |
|      29 |  758 | `			Html5Read(p);` |
|      29 |  759 | `			if( Html5Peek(p,0) == '>' ){` |
|      27 |  760 | `				p->bSelfClose = 1;` |
|      13 |  761 | `			}` |
|      29 |  762 | `			continue;` |
|       - |  763 | `		}` |
|     363 |  764 | `		sAttr.nNameOfs = SyBlobLength(&p->sAttrBuf);` |
|    1201 |  765 | `		for(;;){` |
|    2403 |  766 | `			c = Html5Peek(p,0);` |
|    2403 |  767 | `			if( c < 0 \|\| Html5IsSpace(c) \|\| c == '>' \|\| c == '=' \|\| c == '/' ){` |
|     182 |  768 | `				break;` |
|       - |  769 | `			}` |
|    2041 |  770 | `			Html5PutByte(&p->sAttrBuf,Html5Lower(Html5Read(p)));` |
|       1 |  771 | `		}` |
|     363 |  772 | `		Html5EndStr(&p->sAttrBuf);` |
|     367 |  773 | `		while( Html5IsSpace(Html5Peek(p,0)) ){` |
|       5 |  774 | `			Html5Read(p);` |
|       1 |  775 | `		}` |
|     363 |  776 | `		sAttr.nValOfs = SyBlobLength(&p->sAttrBuf);` |
|     363 |  777 | `		if( Html5Peek(p,0) == '=' ){` |
|       - |  778 | `			int qc;` |
|     359 |  779 | `			Html5Read(p);` |
|     361 |  780 | `			while( Html5IsSpace(Html5Peek(p,0)) ){` |
|       3 |  781 | `				Html5Read(p);` |
|       1 |  782 | `			}` |
|     359 |  783 | `			qc = Html5Peek(p,0);` |
|     359 |  784 | `			if( qc == '"' \|\| qc == '\'' ){` |
|      95 |  785 | `				Html5Read(p);` |
|     473 |  786 | `				for(;;){` |
|     947 |  787 | `					c = Html5Peek(p,0);` |
|     947 |  788 | `					if( c < 0 \|\| c == qc ){` |
|      95 |  789 | `						if( c == qc ){` |
|      95 |  790 | `							Html5Read(p);` |
|      47 |  791 | `						}` |
|      95 |  792 | `						break;` |
|       - |  793 | `					}` |
|     853 |  794 | `					if( c == '&' ){` |
|     ! 0 |  795 | `						Html5CharRef(p,&p->sAttrBuf,TRUE);` |
|     ! 0 |  796 | `					}else{` |
|     853 |  797 | `						Html5PutByte(&p->sAttrBuf,Html5Read(p));` |
|       - |  798 | `					}` |
|       1 |  799 | `				}` |
|      48 |  800 | `			}else{` |
|     673 |  801 | `				for(;;){` |
|    1347 |  802 | `					c = Html5Peek(p,0);` |
|    1347 |  803 | `					if( c < 0 \|\| Html5IsSpace(c) \|\| c == '>' ){` |
|     133 |  804 | `						break;` |
|       - |  805 | `					}` |
|    1083 |  806 | `					if( c == '&' ){` |
|     ! 0 |  807 | `						Html5CharRef(p,&p->sAttrBuf,TRUE);` |
|     ! 0 |  808 | `					}else{` |
|    1083 |  809 | `						Html5PutByte(&p->sAttrBuf,Html5Read(p));` |
|       - |  810 | `					}` |
|       1 |  811 | `				}` |
|       - |  812 | `			}` |
|     179 |  813 | `		}` |
|     363 |  814 | `		Html5EndStr(&p->sAttrBuf);` |
|     363 |  815 | `		SySetPut(&p->sAttr,(const void *)&sAttr);` |
|       1 |  816 | `	}` |
|   28563 |  817 | `	if( Html5Peek(p,0) == '>' ){` |
|   28559 |  818 | `		Html5Read(p);` |
|   14279 |  819 | `	}` |
|   28563 |  820 | `}` |
|       - |  821 | ``/* The comment states: everything to `-->`, or to the end of the source. */`` |
|       - |  822 | `/*` |
|       - |  823 | ` * One quoted doctype identifier.  Answers 0 when the source ENDED it rather` |
|       - |  824 | `` * than closed it -- either at end of input or at a `>`, both of which the spec`` |
|       - |  825 | ` * calls abrupt and both of which set the force-quirks flag.  What was read` |
|       - |  826 | ``  * before the abrupt end is KEPT: php answers `<!DOCTYPE html PUBLIC "abc>def` `` |
|       - |  827 | `` * with the public identifier `abc` and the rest of the line as content, so`` |
|       - |  828 | `` * neither the bytes nor the `>` may be given back.`` |
|       - |  829 | ` */` |
|      34 |  830 | `static int Html5ReadDoctypeId(html5_parser *p,SyBlob *pOut,int cQuote)` |
|       1 |  831 | `{` |
|     681 |  832 | `	for(;;){` |
|     699 |  833 | `		int c = Html5Peek(p,0);` |
|     699 |  834 | `		if( c < 0 ){` |
|     ! 0 |  835 | `			return 0;` |
|       - |  836 | `		}` |
|     699 |  837 | `		if( c == '>' ){` |
|       3 |  838 | `			Html5Read(p);` |
|       3 |  839 | `			return 0;` |
|       - |  840 | `		}` |
|     697 |  841 | `		if( c == cQuote ){` |
|      33 |  842 | `			Html5Read(p);` |
|      33 |  843 | `			return 1;` |
|       - |  844 | `		}` |
|     665 |  845 | `		Html5PutByte(pOut,Html5Read(p));` |
|       1 |  846 | `	}` |
|      18 |  847 | `}` |
|       - |  848 | ``/* Everything to the doctype's `>`, kept by nobody. */`` |
|       6 |  849 | `static void Html5BogusDoctype(html5_parser *p)` |
|       1 |  850 | `{` |
|      27 |  851 | `	for(;;){` |
|      31 |  852 | `		int c = Html5Peek(p,0);` |
|      31 |  853 | `		if( c < 0 ){` |
|     ! 0 |  854 | `			return;` |
|       - |  855 | `		}` |
|      31 |  856 | `		Html5Read(p);` |
|      31 |  857 | `		if( c == '>' ){` |
|       7 |  858 | `			return;` |
|       - |  859 | `		}` |
|       1 |  860 | `	}` |
|       4 |  861 | `}` |
|       - |  862 | `/*` |
|       - |  863 | ` * Past the system identifier.  Junk here is a parse error and nothing more:` |
|       - |  864 | `` * php answers `<!DOCTYPE HTML SYSTEM "b" extra>` with the system identifier`` |
|       - |  865 | ` * intact and the document NOT in quirks mode.` |
|       - |  866 | ` */` |
|      16 |  867 | `static void Html5AfterDoctypeSysId(html5_parser *p)` |
|       1 |  868 | `{` |
|      10 |  869 | `	for(;;){` |
|      19 |  870 | `		int c = Html5Peek(p,0);` |
|      19 |  871 | `		if( c < 0 ){` |
|     ! 0 |  872 | `			p->bForceQuirks = 1;` |
|     ! 0 |  873 | `			return;` |
|       - |  874 | `		}` |
|      19 |  875 | `		if( c == '>' ){` |
|      15 |  876 | `			Html5Read(p);` |
|      15 |  877 | `			return;` |
|       - |  878 | `		}` |
|       5 |  879 | `		if( !Html5IsSpace(c) ){` |
|       3 |  880 | `			Html5BogusDoctype(p);` |
|       3 |  881 | `			return;` |
|       - |  882 | `		}` |
|       3 |  883 | `		Html5Read(p);` |
|       1 |  884 | `	}` |
|       9 |  885 | `}` |
|       - |  886 | `/*` |
|       - |  887 | ` * The doctype's public and system identifiers, read from just past its name.` |
|       - |  888 | `` * php keeps both on the node -- `$doctype->publicId` and `->systemId` answer`` |
|       - |  889 | ` * them -- and asks the pair one further question of its own: whether the` |
|       - |  890 | ` * document is in QUIRKS mode, which is the one thing that decides whether a` |
|       - |  891 | `` * `<table>` closes an open `<p>`.  Neither may be dropped.`` |
|       - |  892 | ` *` |
|       - |  893 | ` * The force-quirks flag is set exactly where the spec sets it, and the one` |
|       - |  894 | ` * place that is easy to get wrong is the bogus tail: reached from after the` |
|       - |  895 | ` * SYSTEM identifier it sets nothing, reached from anywhere earlier it sets the` |
|       - |  896 | `` * flag.  php agrees -- `PUBLIC "a" junk` is quirks and `SYSTEM "b" extra` is`` |
|       - |  897 | ` * not -- so the two exits below are deliberately not shared.` |
|       - |  898 | ` */` |
|     566 |  899 | `static void Html5ReadDoctypeIds(html5_parser *p)` |
|       1 |  900 | `{` |
|       - |  901 | `	int c,bPublic;` |
|     298 |  902 | `	for(;;){` |
|     597 |  903 | `		c = Html5Peek(p,0);` |
|     597 |  904 | `		if( c < 0 ){` |
|     ! 0 |  905 | `			p->bForceQuirks = 1;` |
|     ! 0 |  906 | `			return;` |
|       - |  907 | `		}` |
|     597 |  908 | `		if( c == '>' ){` |
|     537 |  909 | `			Html5Read(p);` |
|     537 |  910 | `			return;` |
|       - |  911 | `		}` |
|      61 |  912 | `		if( !Html5IsSpace(c) ){` |
|      31 |  913 | `			break;` |
|       - |  914 | `		}` |
|      31 |  915 | `		Html5Read(p);` |
|       1 |  916 | `	}` |
|      31 |  917 | `	if( Html5LookWord(p,"PUBLIC") ){` |
|      21 |  918 | `		bPublic = 1;` |
|      21 |  919 | `		Html5Skip(p,6);` |
|      21 |  920 | `	}else if( Html5LookWord(p,"SYSTEM") ){` |
|       9 |  921 | `		bPublic = 0;` |
|       9 |  922 | `		Html5Skip(p,6);` |
|       5 |  923 | `	}else{` |
|       - |  924 | `		/* A word that is neither keyword: the rest is bogus and quirks. */` |
|       3 |  925 | `		p->bForceQuirks = 1;` |
|       3 |  926 | `		Html5BogusDoctype(p);` |
|       3 |  927 | `		return;` |
|       - |  928 | `	}` |
|       - |  929 | `	/*` |
|       - |  930 | `	 * The keyword and its identifier need no whitespace between them: php` |
|       - |  931 | ``	 * reads `PUBLIC"a""b"` as the pair, a parse error that changes nothing`` |
|       - |  932 | `	 * else -- in particular it does NOT force quirks.` |
|       - |  933 | `	 */` |
|      55 |  934 | `	while( Html5IsSpace(Html5Peek(p,0)) ){` |
|      27 |  935 | `		Html5Read(p);` |
|       1 |  936 | `	}` |
|      29 |  937 | `	c = Html5Peek(p,0);` |
|      29 |  938 | `	if( c != '"' && c != '\'' ){` |
|       - |  939 | `		/* No identifier where one was promised. */` |
|       3 |  940 | `		p->bForceQuirks = 1;` |
|       3 |  941 | `		if( c == '>' ){` |
|       3 |  942 | `			Html5Read(p);` |
|       1 |  943 | `		}else if( c >= 0 ){` |
|     ! 0 |  944 | `			Html5BogusDoctype(p);` |
|     ! 0 |  945 | `		}` |
|       3 |  946 | `		return;` |
|       - |  947 | `	}` |
|      27 |  948 | `	Html5Read(p);` |
|      27 |  949 | `	if( !Html5ReadDoctypeId(p,bPublic ? &p->sPubId : &p->sSysId,c) ){` |
|       3 |  950 | `		p->bForceQuirks = 1;` |
|       3 |  951 | `		p->bDocSys = !bPublic;` |
|       3 |  952 | `		return;` |
|       - |  953 | `	}` |
|      25 |  954 | `	if( !bPublic ){` |
|       9 |  955 | `		p->bDocSys = 1;` |
|       9 |  956 | `		Html5AfterDoctypeSysId(p);` |
|       9 |  957 | `		return;` |
|       - |  958 | `	}` |
|       - |  959 | `	/*` |
|       - |  960 | `	 * Between the two identifiers.  A quote reached with no whitespace before` |
|       - |  961 | `	 * it is again only a parse error; anything else that is not the close is` |
|       - |  962 | `	 * bogus AND quirks, which is where this path differs from the one after a` |
|       - |  963 | `	 * system identifier.` |
|       - |  964 | `	 */` |
|      13 |  965 | `	for(;;){` |
|      27 |  966 | `		c = Html5Peek(p,0);` |
|      27 |  967 | `		if( c < 0 ){` |
|     ! 0 |  968 | `			p->bForceQuirks = 1;` |
|     ! 0 |  969 | `			return;` |
|       - |  970 | `		}` |
|      27 |  971 | `		if( c == '>' ){` |
|       7 |  972 | `			Html5Read(p);` |
|       7 |  973 | `			return;` |
|       - |  974 | `		}` |
|      21 |  975 | `		if( !Html5IsSpace(c) ){` |
|      11 |  976 | `			break;` |
|       - |  977 | `		}` |
|      11 |  978 | `		Html5Read(p);` |
|       1 |  979 | `	}` |
|      11 |  980 | `	if( c != '"' && c != '\'' ){` |
|       3 |  981 | `		p->bForceQuirks = 1;` |
|       3 |  982 | `		Html5BogusDoctype(p);` |
|       3 |  983 | `		return;` |
|       - |  984 | `	}` |
|       9 |  985 | `	Html5Read(p);` |
|       9 |  986 | `	if( !Html5ReadDoctypeId(p,&p->sSysId,c) ){` |
|     ! 0 |  987 | `		p->bForceQuirks = 1;` |
|     ! 0 |  988 | `		p->bDocSys = 1;` |
|     ! 0 |  989 | `		return;` |
|       - |  990 | `	}` |
|       9 |  991 | `	p->bDocSys = 1;` |
|       9 |  992 | `	Html5AfterDoctypeSysId(p);` |
|     284 |  993 | `}` |
|     294 |  994 | `static void Html5ReadComment(html5_parser *p)` |
|       1 |  995 | `{` |
|     577 |  996 | `	for(;;){` |
|     725 |  997 | `		int c = Html5Peek(p,0);` |
|     725 |  998 | `		if( c < 0 ){` |
|       5 |  999 | `			break;` |
|       - | 1000 | `		}` |
|     721 | 1001 | `		if( c == '-' && Html5Peek(p,1) == '-' && Html5Peek(p,2) == '>' ){` |
|     291 | 1002 | `			Html5Skip(p,3);` |
|     291 | 1003 | `			break;` |
|       - | 1004 | `		}` |
|     431 | 1005 | `		Html5PutByte(&p->sBuf,Html5Read(p));` |
|       1 | 1006 | `	}` |
|     295 | 1007 | `	p->iTok = HTML5_TOK_COMMENT;` |
|     295 | 1008 | `}` |
|       - | 1009 | ``/* A bogus comment: everything to the next `>` is the comment's data. */`` |
|      12 | 1010 | `static void Html5BogusComment(html5_parser *p)` |
|       1 | 1011 | `{` |
|     108 | 1012 | `	for(;;){` |
|     115 | 1013 | `		int c = Html5Peek(p,0);` |
|     115 | 1014 | `		if( c < 0 ){` |
|     ! 0 | 1015 | `			break;` |
|       - | 1016 | `		}` |
|     115 | 1017 | `		if( c == '>' ){` |
|      13 | 1018 | `			Html5Read(p);` |
|      13 | 1019 | `			break;` |
|       - | 1020 | `		}` |
|     103 | 1021 | `		Html5PutByte(&p->sBuf,Html5Read(p));` |
|       1 | 1022 | `	}` |
|      13 | 1023 | `	p->iTok = HTML5_TOK_COMMENT;` |
|      13 | 1024 | `}` |
|       - | 1025 | `/*` |
|       - | 1026 | ` * The text-only tokenizer the tree constructor arms by name: one text token` |
|       - | 1027 | ` * running to the matching end tag, which is then the next token -- or, when` |
|       - | 1028 | `` * zEnd is 0, to end of file, because `<plaintext>` has no end tag.`` |
|       - | 1029 | ` *` |
|       - | 1030 | ` * Unlike the data state, these states do not DROP a NUL: the spec replaces it` |
|       - | 1031 | ` * with U+FFFD here, and the byte is observable in the text node that results.` |
|       - | 1032 | ` */` |
|     202 | 1033 | `static void Html5ReadText(html5_parser *p,const char *zEnd,int bRcdata)` |
|       1 | 1034 | `{` |
|    1066 | 1035 | `	for(;;){` |
|    1171 | 1036 | `		int c = Html5Peek(p,0);` |
|    1171 | 1037 | `		if( c < 0 ){` |
|      31 | 1038 | `			break;` |
|       - | 1039 | `		}` |
|    1141 | 1040 | `		if( zEnd && c == '<' && Html5Peek(p,1) == '/' ){` |
|     175 | 1041 | `			sxu32 n = (sxu32)SyStrlen(zEnd);` |
|       - | 1042 | `			sxu32 i;` |
|     175 | 1043 | `			int bMatch = 1;` |
|    1085 | 1044 | `			for( i = 0 ; i < n ; ++i ){` |
|     913 | 1045 | `				int d = Html5Peek(p,2 + i);` |
|     913 | 1046 | `				if( d < 0 \|\| Html5Lower(d) != (int)(unsigned char)zEnd[i] ){` |
|       3 | 1047 | `					bMatch = 0;` |
|       3 | 1048 | `					break;` |
|       - | 1049 | `				}` |
|     456 | 1050 | `			}` |
|     175 | 1051 | `			if( bMatch ){` |
|     173 | 1052 | `				int d = Html5Peek(p,2 + n);` |
|     173 | 1053 | `				if( d < 0 \|\| d == '>' \|\| d == '/' \|\| Html5IsSpace(d) ){` |
|      87 | 1054 | `					break;` |
|       - | 1055 | `				}` |
|     ! 0 | 1056 | `			}` |
|       1 | 1057 | `		}` |
|     969 | 1058 | `		if( bRcdata && c == '&' ){` |
|       7 | 1059 | `			Html5CharRef(p,&p->sBuf,FALSE);` |
|       7 | 1060 | `			continue;` |
|       - | 1061 | `		}` |
|     963 | 1062 | `		Html5PutByte(&p->sBuf,Html5Read(p));` |
|       1 | 1063 | `	}` |
|     203 | 1064 | `	p->iTok = HTML5_TOK_TEXT;` |
|     203 | 1065 | `}` |
|       - | 1066 | `/*` |
|       - | 1067 | ` * One token.  The tree constructor drives this and may arm p->iText first,` |
|       - | 1068 | ` * which takes the text-only path above instead of the data state.` |
|       - | 1069 | ` */` |
|   56720 | 1070 | `static void Html5NextToken(html5_parser *p)` |
|       1 | 1071 | `{` |
|       - | 1072 | `	int c;` |
|   56721 | 1073 | `	p->iTok = HTML5_TOK_EOF;` |
|   56721 | 1074 | `	p->bSelfClose = 0;` |
|   56721 | 1075 | `	p->bTextTok = 0;` |
|   56721 | 1076 | `	SyBlobReset(&p->sBuf);` |
|   56721 | 1077 | `	SyBlobReset(&p->sAttrBuf);` |
|   56721 | 1078 | `	SySetReset(&p->sAttr);` |
|   56721 | 1079 | `	if( p->iText != HTML5_TEXT_NONE ){` |
|       - | 1080 | `		char zEnd[32];` |
|     203 | 1081 | `		int nEnd = (int)SyBlobLength(&p->sName);` |
|     203 | 1082 | `		int bRc = p->iText == HTML5_TEXT_RCDATA;` |
|     203 | 1083 | `		int bPlain = p->iText == HTML5_TEXT_PLAIN;` |
|     203 | 1084 | `		if( nEnd > (int)sizeof(zEnd) - 1 ){` |
|     ! 0 | 1085 | `			nEnd = (int)sizeof(zEnd) - 1;` |
|     ! 0 | 1086 | `		}` |
|     203 | 1087 | `		SyMemcpy(SyBlobData(&p->sName),zEnd,(sxu32)nEnd);` |
|     203 | 1088 | `		zEnd[nEnd] = 0;` |
|     203 | 1089 | `		p->iText = HTML5_TEXT_NONE;` |
|     203 | 1090 | `		if( bPlain \|\| SyBlobLength(&p->sName) > 0 ){` |
|     203 | 1091 | `			Html5ReadText(p,bPlain ? 0 : zEnd,bRc);` |
|     203 | 1092 | `			if( SyBlobLength(&p->sBuf) > 0 ){` |
|     197 | 1093 | `				p->bTextTok = 1;` |
|     197 | 1094 | `				return;` |
|       - | 1095 | `			}` |
|       3 | 1096 | `		}` |
|       3 | 1097 | `	}` |
|   56525 | 1098 | `	SyBlobReset(&p->sName);` |
|   56525 | 1099 | `	c = Html5Peek(p,0);` |
|   56525 | 1100 | `	if( c < 0 ){` |
|   13029 | 1101 | `		return;` |
|       - | 1102 | `	}` |
|   43497 | 1103 | `	if( c != '<' ){` |
|       - | 1104 | ``		/* The data state: text to the next `<`, references resolved. */`` |
|  750127 | 1105 | `		for(;;){` |
| 1500785 | 1106 | `			c = Html5Peek(p,0);` |
| 1500785 | 1107 | `			if( c < 0 \|\| c == '<' ){` |
|    6770 | 1108 | `				break;` |
|       - | 1109 | `			}` |
| 1487247 | 1110 | `			if( c == '&' ){` |
|     531 | 1111 | `				Html5CharRef(p,&p->sBuf,FALSE);` |
|     531 | 1112 | `				continue;` |
|       - | 1113 | `			}` |
|       - | 1114 | `			{` |
| 1486717 | 1115 | `				sxu32 iLine = p->iLine,iCol = p->iCol;` |
| 1486717 | 1116 | `				char zc = (char)Html5Read(p);` |
|       - | 1117 | `				/* A NUL is a parse error and never reaches the tree. */` |
| 1486717 | 1118 | `				if( zc != 0 ){` |
| 1486711 | 1119 | `					SyBlobAppend(&p->sBuf,&zc,1);` |
|  743356 | 1120 | `				}else{` |
|      10 | 1121 | `					Html5Err(p,"tokenizer","unexpected-null-character",` |
|       3 | 1122 | `						iLine,iCol,iCol);` |
|       - | 1123 | `				}` |
|       - | 1124 | `			}` |
|       1 | 1125 | `		}` |
|   13539 | 1126 | `		p->iTok = HTML5_TOK_TEXT;` |
|   13539 | 1127 | `		return;` |
|       - | 1128 | `	}` |
|   29959 | 1129 | `	c = Html5Peek(p,1);` |
|   29959 | 1130 | `	if( c == '!' ){` |
|     869 | 1131 | `		Html5Skip(p,2);` |
|     869 | 1132 | `		if( Html5Peek(p,0) == '-' && Html5Peek(p,1) == '-' ){` |
|     295 | 1133 | `			Html5Skip(p,2);` |
|     295 | 1134 | `			Html5ReadComment(p);` |
|     295 | 1135 | `			return;` |
|       - | 1136 | `		}` |
|       - | 1137 | `		/*` |
|       - | 1138 | ``		 * `<![CDATA[` is a bogus COMMENT in HTML and a run of character data`` |
|       - | 1139 | `		 * in foreign content -- the one question the tokenizer has to ask the` |
|       - | 1140 | `		 * tree, because the same bytes mean two different things depending on` |
|       - | 1141 | `		 * what is open.  The run is raw: no character reference is resolved` |
|       - | 1142 | `		 * inside it.` |
|       - | 1143 | `		 */` |
|     574 | 1144 | `		if( Html5NsKind(Html5Top(p)) != HTML5_NSK_HTML` |
|     288 | 1145 | `		 && Html5Peek(p,0) == '[' && Html5Peek(p,1) == 'C'` |
|       2 | 1146 | `		 && Html5Peek(p,2) == 'D' && Html5Peek(p,3) == 'A'` |
|       2 | 1147 | `		 && Html5Peek(p,4) == 'T' && Html5Peek(p,5) == 'A'` |
|       3 | 1148 | `		 && Html5Peek(p,6) == '[' ){` |
|       3 | 1149 | `			Html5Skip(p,7);` |
|       7 | 1150 | `			for(;;){` |
|       9 | 1151 | `				int d = Html5Peek(p,0);` |
|       - | 1152 | `				char zc;` |
|       9 | 1153 | `				if( d < 0 ){` |
|     ! 0 | 1154 | `					break;` |
|       - | 1155 | `				}` |
|       9 | 1156 | `				if( d == ']' && Html5Peek(p,1) == ']' && Html5Peek(p,2) == '>' ){` |
|       3 | 1157 | `					Html5Skip(p,3);` |
|       3 | 1158 | `					break;` |
|       - | 1159 | `				}` |
|       7 | 1160 | `				zc = (char)Html5Read(p);` |
|       7 | 1161 | `				SyBlobAppend(&p->sBuf,&zc,1);` |
|       1 | 1162 | `			}` |
|       3 | 1163 | `			p->iTok = HTML5_TOK_TEXT;` |
|       3 | 1164 | `			return;` |
|       - | 1165 | `		}` |
|     573 | 1166 | `		if( Html5LookWord(p,"DOCTYPE") ){` |
|     567 | 1167 | `			Html5Skip(p,7);` |
|     567 | 1168 | `			SyBlobReset(&p->sPubId);` |
|     567 | 1169 | `			SyBlobReset(&p->sSysId);` |
|     567 | 1170 | `			p->bDocSys = 0;` |
|     567 | 1171 | `			p->bForceQuirks = 0;` |
|    1129 | 1172 | `			while( Html5IsSpace(Html5Peek(p,0)) ){` |
|     563 | 1173 | `				Html5Read(p);` |
|       1 | 1174 | `			}` |
|    2533 | 1175 | `			for(;;){` |
|    2817 | 1176 | `				int d = Html5Peek(p,0);` |
|    2817 | 1177 | `				if( d < 0 \|\| Html5IsSpace(d) \|\| d == '>' ){` |
|     284 | 1178 | `					break;` |
|       - | 1179 | `				}` |
|    2251 | 1180 | `				Html5PutByte(&p->sName,Html5Lower(Html5Read(p)));` |
|       1 | 1181 | `			}` |
|     567 | 1182 | `			SyBlobNullAppend(&p->sName);` |
|     567 | 1183 | `			Html5ReadDoctypeIds(p);` |
|     567 | 1184 | `			SyBlobNullAppend(&p->sPubId);` |
|     567 | 1185 | `			SyBlobNullAppend(&p->sSysId);` |
|     567 | 1186 | `			p->iTok = HTML5_TOK_DOCTYPE;` |
|     567 | 1187 | `			return;` |
|       - | 1188 | `		}` |
|      10 | 1189 | `		Html5Err(p,"tokenizer","incorrectly-opened-comment",` |
|       3 | 1190 | `			p->iLine,p->iCol,p->iCol);` |
|       7 | 1191 | `		Html5BogusComment(p);` |
|       7 | 1192 | `		return;` |
|       - | 1193 | `	}` |
|   29091 | 1194 | `	if( c == '?' ){` |
|       7 | 1195 | `		Html5Read(p);` |
|      10 | 1196 | `		Html5Err(p,"tokenizer","unexpected-question-mark-instead-of-tag-name",` |
|       3 | 1197 | `			p->iLine,p->iCol,p->iCol);` |
|       7 | 1198 | `		Html5BogusComment(p);` |
|       7 | 1199 | `		return;` |
|       - | 1200 | `	}` |
|   29085 | 1201 | `	if( c == '/' ){` |
|   12689 | 1202 | `		int d = Html5Peek(p,2);` |
|   12689 | 1203 | `		if( d < 0 ){` |
|       - | 1204 | ``			/* `</` at the end of the source is literal text. */`` |
|     ! 0 | 1205 | `			SyBlobAppend(&p->sBuf,"</",2);` |
|     ! 0 | 1206 | `			Html5Skip(p,2);` |
|     ! 0 | 1207 | `			p->iTok = HTML5_TOK_TEXT;` |
|     ! 0 | 1208 | `			return;` |
|       - | 1209 | `		}` |
|   12689 | 1210 | `		if( !((d >= 'a' && d <= 'z') \|\| (d >= 'A' && d <= 'Z')) ){` |
|     ! 0 | 1211 | `			Html5Skip(p,2);` |
|     ! 0 | 1212 | `			Html5BogusComment(p);` |
|     ! 0 | 1213 | `			return;` |
|       - | 1214 | `		}` |
|   12689 | 1215 | `		Html5Skip(p,2);` |
|   12689 | 1216 | `		p->iTok = HTML5_TOK_END;` |
|   22741 | 1217 | `	}else if( (c >= 'a' && c <= 'z') \|\| (c >= 'A' && c <= 'Z') ){` |
|   15875 | 1218 | `		Html5Read(p);` |
|   15875 | 1219 | `		p->iTok = HTML5_TOK_START;` |
|    7938 | 1220 | `	}else{` |
|       - | 1221 | ``		/* `<` that opens nothing is the character it is. */`` |
|     523 | 1222 | `		Html5Read(p);` |
|     523 | 1223 | `		SyBlobAppend(&p->sBuf,"<",1);` |
|     523 | 1224 | `		p->iTok = HTML5_TOK_TEXT;` |
|     523 | 1225 | `		return;` |
|       - | 1226 | `	}` |
|   28563 | 1227 | `	p->iTokLine = p->iLine;` |
|   28563 | 1228 | `	p->iTokCol = p->iCol;` |
|   62683 | 1229 | `	for(;;){` |
|   76965 | 1230 | `		int d = Html5Peek(p,0);` |
|   76965 | 1231 | `		if( d < 0 \|\| Html5IsSpace(d) \|\| d == '>' \|\| d == '/' ){` |
|   14282 | 1232 | `			break;` |
|       - | 1233 | `		}` |
|   48403 | 1234 | `		Html5PutByte(&p->sName,Html5Lower(Html5Read(p)));` |
|       1 | 1235 | `	}` |
|   28563 | 1236 | `	p->iTokCol2 = p->iCol > p->iTokCol ? p->iCol - 1 : p->iTokCol;` |
|   28563 | 1237 | `	SyBlobNullAppend(&p->sName);` |
|   28563 | 1238 | `	Html5ReadAttrs(p);` |
|   28361 | 1239 | `}` |
|       - | 1240 |  |
|       - | 1241 | `/* ------------------------------------------------------------------ *` |
|       - | 1242 | ` * The tree constructor` |
|       - | 1243 | ` * ------------------------------------------------------------------ */` |
|       - | 1244 |  |
|  227234 | 1245 | `static const char * Html5TokName(html5_parser *p)` |
|       1 | 1246 | `{` |
|  227235 | 1247 | `	return SyBlobLength(&p->sName) > 0 ? (const char *)SyBlobData(&p->sName) : "";` |
|       1 | 1248 | `}` |
|  323782 | 1249 | `static xmlNodePtr Html5Top(html5_parser *p)` |
|       1 | 1250 | `{` |
|  323783 | 1251 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|  323783 | 1252 | `	sxu32 n = SySetUsed(&p->sOpen);` |
|  323783 | 1253 | `	return n > 0 ? apStack[n - 1] : 0;` |
|       1 | 1254 | `}` |
|       - | 1255 | `/* Where a node goes: the innermost open element, or the document itself. */` |
|  121118 | 1256 | `static xmlNodePtr Html5Target(html5_parser *p)` |
|       1 | 1257 | `{` |
|  121119 | 1258 | `	xmlNodePtr pTop = Html5Top(p);` |
|  121119 | 1259 | `	return pTop ? pTop : (xmlNodePtr)p->pDoc;` |
|       1 | 1260 | `}` |
|   53068 | 1261 | `static void Html5Push(html5_parser *p,xmlNodePtr pNode)` |
|       1 | 1262 | `{` |
|   53069 | 1263 | `	SySetPut(&p->sOpen,(const void *)&pNode);` |
|   53069 | 1264 | `}` |
|       - | 1265 | `static void Html5FmtClearToMarker(html5_parser *p);` |
|   25796 | 1266 | `static void Html5Pop(html5_parser *p)` |
|       1 | 1267 | `{` |
|   25797 | 1268 | `	xmlNodePtr pTop = Html5Top(p);` |
|   25797 | 1269 | `	SySetPop(&p->sOpen);` |
|   25797 | 1270 | `	if( pTop && HTML5_IN(azHtml5FmtMark,(const char *)pTop->name) ){` |
|      81 | 1271 | `		Html5FmtClearToMarker(p);` |
|      40 | 1272 | `	}` |
|   25797 | 1273 | `}` |
|       - | 1274 | ``/* Where `pEl` sits in the open stack, counted from the root, or -1. */`` |
|     290 | 1275 | `static int Html5StackIndex(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1276 | `{` |
|     291 | 1277 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|     291 | 1278 | `	int i,n = (int)SySetUsed(&p->sOpen);` |
|    1099 | 1279 | `	for( i = 0 ; i < n ; ++i ){` |
|    1073 | 1280 | `		if( apStack[i] == pEl ){` |
|     265 | 1281 | `			return i;` |
|       - | 1282 | `		}` |
|     405 | 1283 | `	}` |
|      27 | 1284 | `	return -1;` |
|     146 | 1285 | `}` |
|       - | 1286 | `/*` |
|       - | 1287 | ` * The adoption moves entries about in the middle of both lists, which is the` |
|       - | 1288 | ` * one thing SySet has no verb for; its slots are contiguous, so a shift is the` |
|       - | 1289 | ` * whole of it.` |
|       - | 1290 | ` */` |
|     114 | 1291 | `static void Html5SetRemoveAt(SySet *pSet,int i)` |
|       1 | 1292 | `{` |
|     115 | 1293 | `	xmlNodePtr *ap = (xmlNodePtr *)SySetBasePtr(pSet);` |
|     115 | 1294 | `	int n = (int)SySetUsed(pSet);` |
|     115 | 1295 | `	if( i < 0 \|\| i >= n ){` |
|       9 | 1296 | `		return;` |
|       - | 1297 | `	}` |
|     141 | 1298 | `	for( ; i + 1 < n ; ++i ){` |
|      35 | 1299 | `		ap[i] = ap[i + 1];` |
|      18 | 1300 | `	}` |
|     107 | 1301 | `	pSet->nUsed = (sxu32)(n - 1);` |
|      58 | 1302 | `}` |
|      24 | 1303 | `static void Html5SetInsertAt(SySet *pSet,int i,xmlNodePtr pEl)` |
|       1 | 1304 | `{` |
|       - | 1305 | `	xmlNodePtr *ap;` |
|       - | 1306 | `	int j,n;` |
|      25 | 1307 | `	SySetPut(pSet,(const void *)&pEl);       /* grow by one, then shift */` |
|      25 | 1308 | `	ap = (xmlNodePtr *)SySetBasePtr(pSet);` |
|      25 | 1309 | `	n = (int)SySetUsed(pSet);` |
|      25 | 1310 | `	if( i < 0 ){` |
|     ! 0 | 1311 | `		i = 0;` |
|     ! 0 | 1312 | `	}` |
|      25 | 1313 | `	if( i > n - 1 ){` |
|     ! 0 | 1314 | `		return;` |
|       - | 1315 | `	}` |
|      31 | 1316 | `	for( j = n - 1 ; j > i ; --j ){` |
|       7 | 1317 | `		ap[j] = ap[j - 1];` |
|       4 | 1318 | `	}` |
|      25 | 1319 | `	ap[i] = pEl;` |
|      13 | 1320 | `}` |
|       - | 1321 | ``/* Is `zName` open?  Answers the depth from the top, or -1. */`` |
|   24596 | 1322 | `static int Html5OpenDepth(html5_parser *p,const char *zName)` |
|       1 | 1323 | `{` |
|   24597 | 1324 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|   24597 | 1325 | `	sxu32 n = SySetUsed(&p->sOpen);` |
|   26391 | 1326 | `	while( n-- > 0 ){` |
|   25935 | 1327 | `		if( Html5Eq((const char *)apStack[n]->name,zName) ){` |
|   24141 | 1328 | `			return (int)(SySetUsed(&p->sOpen) - 1 - n);` |
|       - | 1329 | `		}` |
|       1 | 1330 | `	}` |
|     457 | 1331 | `	return -1;` |
|   12299 | 1332 | `}` |
|       - | 1333 | ``/* Pop everything down to and including the innermost `zName`, if it is open. */`` |
|   12034 | 1334 | `static void Html5PopTo(html5_parser *p,const char *zName)` |
|       1 | 1335 | `{` |
|   12035 | 1336 | `	int iDepth = Html5OpenDepth(p,zName);` |
|   12035 | 1337 | `	if( iDepth < 0 ){` |
|     ! 0 | 1338 | `		return;` |
|       - | 1339 | `	}` |
|   24249 | 1340 | `	while( iDepth-- >= 0 ){` |
|   12215 | 1341 | `		Html5Pop(p);` |
|       1 | 1342 | `	}` |
|    6018 | 1343 | `}` |
|       - | 1344 | `/*` |
|       - | 1345 | `` * The declaration for `zHref` that `pEl` should carry: the innermost one`` |
|       - | 1346 | ` * already in scope above it, or a fresh one minted on the element itself.  A` |
|       - | 1347 | `` * foreign subtree therefore states its namespace once, on the `<svg>` or`` |
|       - | 1348 | `` * `<math>` that opened it, and every descendant points at that same one.`` |
|       - | 1349 | ` */` |
|     126 | 1350 | `static xmlNsPtr Html5NsGet(html5_parser *p,xmlNodePtr pEl,const char *zHref,` |
|       - | 1351 | `	const char *zPrefix)` |
|       1 | 1352 | `{` |
|     127 | 1353 | `	xmlNsPtr pNs = xmlSearchNsByHref(p->pDoc,pEl,(const xmlChar *)zHref);` |
|     127 | 1354 | `	if( pNs == 0 ){` |
|      65 | 1355 | `		pNs = xmlNewNs(pEl,(const xmlChar *)zHref,(const xmlChar *)zPrefix);` |
|       - | 1356 | `		/* The source did not write this one -- the tree construction rules` |
|       - | 1357 | `		 * did, because an element or an attribute has to be in SOME namespace.` |
|       - | 1358 | `		 * php spells such a binding nowhere: it is on no attribute map, and a` |
|       - | 1359 | `		 * serializer that needs a declaration mints its own. */` |
|      65 | 1360 | `		PH7_DomNsMarkUnspelt(pNs);` |
|      32 | 1361 | `	}` |
|     127 | 1362 | `	return pNs;` |
|       1 | 1363 | `}` |
|       - | 1364 | `/*` |
|       - | 1365 | ` * Is one of the table's own insertion modes running?  The question is about` |
|       - | 1366 | ` * the CURRENT node rather than about a table being open anywhere: content` |
|       - | 1367 | `` * inside a `<td>` -- or inside an element already fostered out -- nests`` |
|       - | 1368 | ` * normally, and only what lands directly in a table, a row group or a row is` |
|       - | 1369 | ` * misplaced.` |
|       - | 1370 | ` */` |
|   27254 | 1371 | `static int Html5InTableCtx(html5_parser *p)` |
|       1 | 1372 | `{` |
|   27255 | 1373 | `	xmlNodePtr pTop = Html5Top(p);` |
|       - | 1374 | `	const char *z;` |
|   27255 | 1375 | `	if( pTop == 0 ){` |
|       3 | 1376 | `		return 0;` |
|       - | 1377 | `	}` |
|   27253 | 1378 | `	z = (const char *)pTop->name;` |
|   54371 | 1379 | `	return Html5Eq(z,"table") \|\| Html5Eq(z,"tbody") \|\| Html5Eq(z,"thead")` |
|   40816 | 1380 | `		\|\| Html5Eq(z,"tfoot") \|\| Html5Eq(z,"tr");` |
|   13628 | 1381 | `}` |
|       - | 1382 | `/*` |
|       - | 1383 | ` * Is a table open ANYWHERE on the stack?  Html5InTableCtx asks only about the` |
|       - | 1384 | ` * top, which is the right question for fostering and the wrong one here: inside` |
|       - | 1385 | `` * a `<td>` the top is the cell, and a stray `<td>` written there opens the next`` |
|       - | 1386 | ` * cell rather than being dropped.` |
|       - | 1387 | ` */` |
|     330 | 1388 | `static int Html5TableOpen(html5_parser *p)` |
|       1 | 1389 | `{` |
|     331 | 1390 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|     331 | 1391 | `	sxu32 n = SySetUsed(&p->sOpen);` |
|     777 | 1392 | `	while( n-- > 0 ){` |
|       - | 1393 | `		/*` |
|       - | 1394 | `` 		 * A template is a table context of its own: the spec answers a `<tr>` `` |
|       - | 1395 | ``		 * or a `<td>` written inside one by pushing `in table body` or `in`` |
|       - | 1396 | ``		 * row` whether or not a real table is anywhere, so the row survives`` |
|       - | 1397 | `		 * and is not dropped as a table-only tag written outside a table.` |
|       - | 1398 | `		 */` |
|     703 | 1399 | `		if( Html5Eq((const char *)apStack[n]->name,"template") ){` |
|       7 | 1400 | `			return 1;` |
|       - | 1401 | `		}` |
|     697 | 1402 | `		if( Html5Eq((const char *)apStack[n]->name,"table") ){` |
|     251 | 1403 | `			return 1;` |
|       - | 1404 | `		}` |
|       1 | 1405 | `	}` |
|      75 | 1406 | `	return 0;` |
|     166 | 1407 | `}` |
|       - | 1408 | `/*` |
|       - | 1409 | ` * The node the fostered content goes immediately before: the innermost open` |
|       - | 1410 | ` * table.  A table with no parent has nowhere earlier to put anything, so the` |
|       - | 1411 | ` * caller falls back to an ordinary insertion -- the spec reaches for the` |
|       - | 1412 | ` * element under the table on the stack, which under NOIMPLIED may not exist.` |
|       - | 1413 | ` */` |
|     166 | 1414 | `static xmlNodePtr Html5FosterBefore(html5_parser *p)` |
|       1 | 1415 | `{` |
|     167 | 1416 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|     167 | 1417 | `	sxu32 n = SySetUsed(&p->sOpen);` |
|     235 | 1418 | `	while( n-- > 0 ){` |
|     235 | 1419 | `		if( Html5Eq((const char *)apStack[n]->name,"template") ){` |
|     ! 0 | 1420 | `			return 0;` |
|       - | 1421 | `		}` |
|     235 | 1422 | `		if( Html5Eq((const char *)apStack[n]->name,"table") ){` |
|     167 | 1423 | `			return apStack[n]->parent ? apStack[n] : 0;` |
|       - | 1424 | `		}` |
|       1 | 1425 | `	}` |
|     ! 0 | 1426 | `	return 0;` |
|      84 | 1427 | `}` |
|       - | 1428 | `/*` |
|       - | 1429 | `` * Attach `pNode` where this token's content belongs.  Fostering needs BOTH`` |
|       - | 1430 | `` * halves: the token has to be one the table modes hand to `in body`, and the`` |
|       - | 1431 | ` * place it would land has to still be the table itself -- a second formatting` |
|       - | 1432 | ` * clone in one reconstruction nests inside the first, which is no longer a` |
|       - | 1433 | ` * table.` |
|       - | 1434 | ` *` |
|       - | 1435 | ` * Both insertion paths owe the same invariant -- no two adjacent text nodes --` |
|       - | 1436 | ` * and only the ordinary one gets it for free.  xmlAddChild merges a text node` |
|       - | 1437 | ` * into the parent's last child; xmlAddPrevSibling merges only when the node it` |
|       - | 1438 | `` * inserts BEFORE is itself text, because it compares `cur->name` against`` |
|       - | 1439 | `` * `cur->prev->name`.  The foster target is always the open TABLE element, so`` |
|       - | 1440 | ` * that test never fires here and every run fostered out past a table landed as` |
|       - | 1441 | `` * its own node: `<table>a<td>b</td>c</table>` left `a` and `c` side by side`` |
|       - | 1442 | ` * where the spec -- which merges on the insertion POSITION, appending to the` |
|       - | 1443 | `` * node immediately before it when that node is text -- leaves `ac`.  The text`` |
|       - | 1444 | ``  * already sitting before the table is the same position, so `<div>d<table>a` `` |
|       - | 1445 | `` * merges into `d` rather than opening a second node beside it.`` |
|       - | 1446 | ` */` |
|   67776 | 1447 | `static void Html5Attach(html5_parser *p,xmlNodePtr pNode)` |
|       1 | 1448 | `{` |
|   67777 | 1449 | `	xmlNodePtr pBefore = 0;` |
|   67777 | 1450 | `	if( pNode == 0 ){` |
|     ! 0 | 1451 | `		return;` |
|       - | 1452 | `	}` |
|   67777 | 1453 | `	if( p->bFoster && Html5InTableCtx(p) ){` |
|     167 | 1454 | `		pBefore = Html5FosterBefore(p);` |
|      83 | 1455 | `	}` |
|   67777 | 1456 | `	if( pBefore ){` |
|     166 | 1457 | `		if( pNode->type == XML_TEXT_NODE && pBefore->prev` |
|      70 | 1458 | `		 && pBefore->prev->type == XML_TEXT_NODE ){` |
|       - | 1459 | `			/* libxml's own merge, which is what xmlAddPrevSibling runs when it` |
|       - | 1460 | `			 * does decide to merge -- so the dict-allocated content case is` |
|       - | 1461 | `			 * handled the way every other text append in this tree handles it. */` |
|      31 | 1462 | `			xmlNodeAddContent(pBefore->prev,pNode->content);` |
|      31 | 1463 | `			xmlFreeNode(pNode);` |
|      31 | 1464 | `			return;` |
|       - | 1465 | `		}` |
|     137 | 1466 | `		xmlAddPrevSibling(pBefore,pNode);` |
|      69 | 1467 | `	}else{` |
|   67611 | 1468 | `		xmlAddChild(Html5Target(p),pNode);` |
|       - | 1469 | `	}` |
|   33889 | 1470 | `}` |
|   53490 | 1471 | `static xmlNodePtr Html5NewElemNs(html5_parser *p,const char *zName,const char *zHref)` |
|       1 | 1472 | `{` |
|   53491 | 1473 | `	xmlNodePtr pParent = Html5Target(p);` |
|   53491 | 1474 | `	xmlNodePtr pEl = xmlNewDocNode(p->pDoc,0,(const xmlChar *)zName,0);` |
|   53491 | 1475 | `	if( pEl == 0 ){` |
|     ! 0 | 1476 | `		return 0;` |
|       - | 1477 | `	}` |
|   53491 | 1478 | `	if( zHref ){` |
|       - | 1479 | `		/* Attached first: the declaration in scope is only findable from a` |
|       - | 1480 | `		 * node that is already under the element that states it. */` |
|     121 | 1481 | `		Html5Attach(p,pEl);` |
|     121 | 1482 | `		xmlSetNs(pEl,Html5NsGet(p,pEl,zHref,0));` |
|     121 | 1483 | `		return pEl;` |
|       - | 1484 | `	}` |
|   53371 | 1485 | `	if( (p->iFlags & HTML5_NO_DEF_NS) == 0 ){` |
|       - | 1486 | `		/*` |
|       - | 1487 | `		 * The declaration belongs to the root of the tree being built, so a` |
|       - | 1488 | `		 * document with one root states it once.  Under NOIMPLIED there can be` |
|       - | 1489 | `		 * several roots and each states its own -- there is no node above them` |
|       - | 1490 | `		 * that could hold one for all.` |
|       - | 1491 | `		 */` |
|   53355 | 1492 | `		if( pParent == (xmlNodePtr)p->pDoc \|\| p->pNs == 0 ){` |
|   13023 | 1493 | `			p->pNs = xmlNewNs(pEl,(const xmlChar *)HTML5_NS,0);` |
|   13023 | 1494 | `			PH7_DomNsMarkUnspelt(p->pNs);` |
|    6511 | 1495 | `		}` |
|   53355 | 1496 | `		xmlSetNs(pEl,p->pNs);` |
|   26677 | 1497 | `	}` |
|   53371 | 1498 | `	Html5Attach(p,pEl);` |
|   53371 | 1499 | `	return pEl;` |
|   26746 | 1500 | `}` |
|   53370 | 1501 | `static xmlNodePtr Html5NewElem(html5_parser *p,const char *zName)` |
|       1 | 1502 | `{` |
|   53371 | 1503 | `	return Html5NewElemNs(p,zName,0);` |
|       1 | 1504 | `}` |
|   15486 | 1505 | `static void Html5AddAttrs(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1506 | `{` |
|   15487 | 1507 | `	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);` |
|   15487 | 1508 | `	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);` |
|   15487 | 1509 | `	sxu32 i,n = SySetUsed(&p->sAttr);` |
|   15817 | 1510 | `	for( i = 0 ; i < n ; ++i ){` |
|     331 | 1511 | `		const char *zName = zBuf + aAttr[i].nNameOfs;` |
|     331 | 1512 | `		const char *zVal = zBuf + aAttr[i].nValOfs;` |
|     331 | 1513 | `		if( zName[0] == 0 ){` |
|     ! 0 | 1514 | `			continue;` |
|       - | 1515 | `		}` |
|       - | 1516 | `		/* A repeated attribute is the FIRST one; the rest are dropped. */` |
|     331 | 1517 | `		if( xmlHasProp(pEl,(const xmlChar *)zName) ){` |
|       7 | 1518 | `			continue;` |
|       - | 1519 | `		}` |
|     325 | 1520 | `		xmlNewProp(pEl,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|     163 | 1521 | `	}` |
|   15487 | 1522 | `}` |
|       - | 1523 | `/*` |
|       - | 1524 | ` * A detached copy of a formatting element: same name, same attributes, no` |
|       - | 1525 | ` * children.  Both the reconstruction and the adoption mint one, because the` |
|       - | 1526 | ` * spec builds them from the TOKEN the original was created for and the token` |
|       - | 1527 | ` * is long gone by then -- the element it built is the only record of it.` |
|       - | 1528 | ` */` |
|      32 | 1529 | `static xmlNodePtr Html5CloneFmt(html5_parser *p,xmlNodePtr pSrc)` |
|       1 | 1530 | `{` |
|      33 | 1531 | `	xmlNodePtr pEl = xmlNewDocNode(p->pDoc,0,pSrc->name,0);` |
|       - | 1532 | `	xmlAttrPtr pAttr;` |
|      33 | 1533 | `	if( pEl == 0 ){` |
|     ! 0 | 1534 | `		return 0;` |
|       - | 1535 | `	}` |
|      33 | 1536 | `	if( (p->iFlags & HTML5_NO_DEF_NS) == 0 && p->pNs ){` |
|      33 | 1537 | `		xmlSetNs(pEl,p->pNs);` |
|      16 | 1538 | `	}` |
|      35 | 1539 | `	for( pAttr = pSrc->properties ; pAttr ; pAttr = pAttr->next ){` |
|       3 | 1540 | `		xmlChar *zVal = xmlNodeListGetString(p->pDoc,pAttr->children,1);` |
|       3 | 1541 | `		xmlNewProp(pEl,pAttr->name,zVal ? zVal : (const xmlChar *)"");` |
|       3 | 1542 | `		if( zVal ){` |
|       3 | 1543 | `			xmlFree(zVal);` |
|       1 | 1544 | `		}` |
|       2 | 1545 | `	}` |
|      33 | 1546 | `	return pEl;` |
|      17 | 1547 | `}` |
|       - | 1548 | `/* ------------------------------------------------------------------ *` |
|       - | 1549 | ` * The list of active formatting elements` |
|       - | 1550 | ` * ------------------------------------------------------------------ */` |
|       - | 1551 |  |
|     168 | 1552 | `static int Html5FmtIndex(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1553 | `{` |
|     169 | 1554 | `	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);` |
|     169 | 1555 | `	int i,n = (int)SySetUsed(&p->sFmt);` |
|     227 | 1556 | `	for( i = 0 ; i < n ; ++i ){` |
|     223 | 1557 | `		if( apFmt[i] == pEl ){` |
|     165 | 1558 | `			return i;` |
|       - | 1559 | `		}` |
|      30 | 1560 | `	}` |
|       5 | 1561 | `	return -1;` |
|      85 | 1562 | `}` |
|      86 | 1563 | `static void Html5FmtRemove(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1564 | `{` |
|      87 | 1565 | `	Html5SetRemoveAt(&p->sFmt,Html5FmtIndex(p,pEl));` |
|      87 | 1566 | `}` |
|     168 | 1567 | `static void Html5FmtMarker(html5_parser *p)` |
|       1 | 1568 | `{` |
|     169 | 1569 | `	xmlNodePtr pNull = 0;` |
|     169 | 1570 | `	SySetPut(&p->sFmt,(const void *)&pNull);` |
|     169 | 1571 | `}` |
|     118 | 1572 | `static void Html5FmtClearToMarker(html5_parser *p)` |
|       1 | 1573 | `{` |
|     119 | 1574 | `	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);` |
|     119 | 1575 | `	int n = (int)SySetUsed(&p->sFmt);` |
|     119 | 1576 | `	while( n-- > 0 ){` |
|     119 | 1577 | `		if( apFmt[n] == 0 ){` |
|     119 | 1578 | `			break;` |
|       - | 1579 | `		}` |
|     ! 0 | 1580 | `	}` |
|     119 | 1581 | `	if( n >= 0 ){` |
|     119 | 1582 | `		p->sFmt.nUsed = (sxu32)n;      /* the marker goes with them */` |
|      59 | 1583 | `	}` |
|     119 | 1584 | `}` |
|       - | 1585 | ``/* The innermost entry named `zName` after the last marker, or 0. */`` |
|     114 | 1586 | `static xmlNodePtr Html5FmtLast(html5_parser *p,const char *zName)` |
|       1 | 1587 | `{` |
|     115 | 1588 | `	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);` |
|     115 | 1589 | `	int n = (int)SySetUsed(&p->sFmt);` |
|     135 | 1590 | `	while( n-- > 0 ){` |
|     125 | 1591 | `		if( apFmt[n] == 0 ){` |
|       5 | 1592 | `			break;` |
|       - | 1593 | `		}` |
|     121 | 1594 | `		if( Html5Eq((const char *)apFmt[n]->name,zName) ){` |
|     101 | 1595 | `			return apFmt[n];` |
|       - | 1596 | `		}` |
|       1 | 1597 | `	}` |
|      15 | 1598 | `	return 0;` |
|      58 | 1599 | `}` |
|       - | 1600 | `/* Same name and same attributes -- the Noah's Ark clause asks both. */` |
|      32 | 1601 | `static int Html5SameFmt(xmlNodePtr pLeft,xmlNodePtr pRight)` |
|       1 | 1602 | `{` |
|       - | 1603 | `	xmlAttrPtr pA,pB;` |
|      33 | 1604 | `	int nLeft = 0,nRight = 0;` |
|      33 | 1605 | `	if( !Html5Eq((const char *)pLeft->name,(const char *)pRight->name) ){` |
|      27 | 1606 | `		return 0;` |
|       - | 1607 | `	}` |
|       7 | 1608 | `	for( pA = pLeft->properties ; pA ; pA = pA->next ){` |
|     ! 0 | 1609 | `		nLeft++;` |
|     ! 0 | 1610 | `	}` |
|       7 | 1611 | `	for( pB = pRight->properties ; pB ; pB = pB->next ){` |
|     ! 0 | 1612 | `		nRight++;` |
|     ! 0 | 1613 | `	}` |
|       7 | 1614 | `	if( nLeft != nRight ){` |
|     ! 0 | 1615 | `		return 0;` |
|       - | 1616 | `	}` |
|       7 | 1617 | `	for( pA = pLeft->properties ; pA ; pA = pA->next ){` |
|       - | 1618 | `		xmlChar *zL,*zR;` |
|       - | 1619 | `		int bSame;` |
|     ! 0 | 1620 | `		if( xmlHasProp(pRight,pA->name) == 0 ){` |
|     ! 0 | 1621 | `			return 0;` |
|       - | 1622 | `		}` |
|     ! 0 | 1623 | `		zL = xmlNodeListGetString(pLeft->doc,pA->children,1);` |
|     ! 0 | 1624 | `		zR = xmlGetProp(pRight,pA->name);` |
|     ! 0 | 1625 | `		bSame = zL && zR && Html5Eq((const char *)zL,(const char *)zR);` |
|     ! 0 | 1626 | `		if( zL ){` |
|     ! 0 | 1627 | `			xmlFree(zL);` |
|     ! 0 | 1628 | `		}` |
|     ! 0 | 1629 | `		if( zR ){` |
|     ! 0 | 1630 | `			xmlFree(zR);` |
|     ! 0 | 1631 | `		}` |
|     ! 0 | 1632 | `		if( !bSame ){` |
|     ! 0 | 1633 | `			return 0;` |
|       - | 1634 | `		}` |
|     ! 0 | 1635 | `	}` |
|       7 | 1636 | `	return 1;` |
|      17 | 1637 | `}` |
|       - | 1638 | `/*` |
|       - | 1639 | ` * Push a formatting element, under the Noah's Ark clause: three entries that` |
|       - | 1640 | ` * agree on name and attributes are the most the list may hold after its last` |
|       - | 1641 | ` * marker, and a fourth evicts the outermost of them.` |
|       - | 1642 | ` */` |
|     132 | 1643 | `static void Html5FmtPush(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1644 | `{` |
|     133 | 1645 | `	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);` |
|     133 | 1646 | `	int i,n = (int)SySetUsed(&p->sFmt);` |
|     133 | 1647 | `	int nSame = 0,iFirst = -1;` |
|     165 | 1648 | `	for( i = n - 1 ; i >= 0 ; --i ){` |
|      51 | 1649 | `		if( apFmt[i] == 0 ){` |
|      19 | 1650 | `			break;` |
|       - | 1651 | `		}` |
|      33 | 1652 | `		if( Html5SameFmt(apFmt[i],pEl) ){` |
|       7 | 1653 | `			nSame++;` |
|       7 | 1654 | `			iFirst = i;` |
|       3 | 1655 | `		}` |
|      17 | 1656 | `	}` |
|     133 | 1657 | `	if( nSame >= 3 && iFirst >= 0 ){` |
|     ! 0 | 1658 | `		Html5SetRemoveAt(&p->sFmt,iFirst);` |
|     ! 0 | 1659 | `	}` |
|     133 | 1660 | `	SySetPut(&p->sFmt,(const void *)&pEl);` |
|     133 | 1661 | `}` |
|       - | 1662 | `/*` |
|       - | 1663 | ` * Reconstruct: every entry after the last one that is still open is reopened,` |
|       - | 1664 | ` * innermost last, as a fresh element in the current position.  This is what` |
|       - | 1665 | `` * puts the `<i>` back around the text after `<b><i>x</b>y`.`` |
|       - | 1666 | ` */` |
|       - | 1667 | `/*` |
|       - | 1668 | `` * An open `<select>` is the spec's own insertion mode, and that mode takes`` |
|       - | 1669 | ` * neither a reconstruction nor an adoption: everything it does not recognise` |
|       - | 1670 | ` * is ignored where it stands.  This parser reads the stack instead of carrying` |
|       - | 1671 | ` * the mode, so both doors ask here.` |
|       - | 1672 | ` */` |
|   27918 | 1673 | `static int Html5InSelect(html5_parser *p)` |
|       1 | 1674 | `{` |
|   27919 | 1675 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|   27919 | 1676 | `	int n = (int)SySetUsed(&p->sOpen);` |
|   85069 | 1677 | `	while( n-- > 0 ){` |
|   58267 | 1678 | `		if( Html5Eq((const char *)apStack[n]->name,"select") ){` |
|    1117 | 1679 | `			return 1;` |
|       - | 1680 | `		}` |
|       1 | 1681 | `	}` |
|   26803 | 1682 | `	return 0;` |
|   13960 | 1683 | `}` |
|   15088 | 1684 | `static void Html5Reconstruct(html5_parser *p)` |
|       1 | 1685 | `{` |
|   15089 | 1686 | `	xmlNodePtr *apFmt = (xmlNodePtr *)SySetBasePtr(&p->sFmt);` |
|   15089 | 1687 | `	int i,n = (int)SySetUsed(&p->sFmt);` |
|   15089 | 1688 | `	if( n < 1 \|\| Html5InSelect(p) ){` |
|   14767 | 1689 | `		return;` |
|       - | 1690 | `	}` |
|     323 | 1691 | `	i = n - 1;` |
|     323 | 1692 | `	if( apFmt[i] == 0 \|\| Html5StackIndex(p,apFmt[i]) >= 0 ){` |
|     305 | 1693 | `		return;` |
|       - | 1694 | `	}` |
|      21 | 1695 | `	while( i > 0 ){` |
|       3 | 1696 | `		i--;` |
|       3 | 1697 | `		if( apFmt[i] == 0 \|\| Html5StackIndex(p,apFmt[i]) >= 0 ){` |
|     ! 0 | 1698 | `			i++;` |
|     ! 0 | 1699 | `			break;` |
|       - | 1700 | `		}` |
|       1 | 1701 | `	}` |
|      39 | 1702 | `	for( ; i < n ; ++i ){` |
|      21 | 1703 | `		xmlNodePtr pNew = Html5CloneFmt(p,apFmt[i]);` |
|      21 | 1704 | `		if( pNew == 0 ){` |
|     ! 0 | 1705 | `			break;` |
|       - | 1706 | `		}` |
|      21 | 1707 | `		Html5Attach(p,pNew);` |
|      21 | 1708 | `		Html5Push(p,pNew);` |
|      21 | 1709 | `		apFmt[i] = pNew;` |
|      11 | 1710 | `	}` |
|    7545 | 1711 | `}` |
|       - | 1712 | ``/* Is `pEl` open with no scope marker between it and the current node? */`` |
|      94 | 1713 | `static int Html5InScope(html5_parser *p,xmlNodePtr pEl)` |
|       1 | 1714 | `{` |
|      95 | 1715 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|      95 | 1716 | `	int n = (int)SySetUsed(&p->sOpen);` |
|     127 | 1717 | `	while( n-- > 0 ){` |
|     127 | 1718 | `		if( apStack[n] == pEl ){` |
|      93 | 1719 | `			return 1;` |
|       - | 1720 | `		}` |
|      35 | 1721 | `		if( HTML5_IN(azHtml5Scope,(const char *)apStack[n]->name) ){` |
|       3 | 1722 | `			return 0;` |
|       - | 1723 | `		}` |
|       1 | 1724 | `	}` |
|     ! 0 | 1725 | `	return 0;` |
|      48 | 1726 | `}` |
|       - | 1727 | `/*` |
|       - | 1728 | ` * The adoption agency algorithm.  Answers 0 when the name is not on the list` |
|       - | 1729 | ` * at all, which is the caller's cue to treat the end tag as any other.` |
|       - | 1730 | ` *` |
|       - | 1731 | ` * The outer loop is not a formality: one pass moves ONE block out of the` |
|       - | 1732 | `` * formatting element, and a `<b><div><i>x</b>` needs two -- the first to lift`` |
|       - | 1733 | `` * the `<div>` out and the second to retire the copy the first left behind.`` |
|       - | 1734 | ` */` |
|      92 | 1735 | `static int Html5Adoption(html5_parser *p,const char *zName)` |
|       1 | 1736 | `{` |
|      93 | 1737 | `	xmlNodePtr pTop = Html5Top(p);` |
|       - | 1738 | `	int iOuter;` |
|      93 | 1739 | `	if( Html5InSelect(p) ){` |
|       3 | 1740 | `		return 1;` |
|       - | 1741 | `	}` |
|      90 | 1742 | `	if( pTop && Html5Eq((const char *)pTop->name,zName)` |
|      75 | 1743 | `	 && Html5FmtIndex(p,pTop) < 0 ){` |
|     ! 0 | 1744 | `		Html5Pop(p);` |
|     ! 0 | 1745 | `		return 1;` |
|       - | 1746 | `	}` |
|     103 | 1747 | `	for( iOuter = 0 ; iOuter < 8 ; ++iOuter ){` |
|       - | 1748 | `		xmlNodePtr *apStack;` |
|     103 | 1749 | `		xmlNodePtr pFmt,pFurthest = 0,pAncestor,pNode,pLast,pNew,pChild;` |
|       - | 1750 | `		int iFmtStack,iBookmark,iInner,iNode,i,n;` |
|     103 | 1751 | `		pFmt = Html5FmtLast(p,zName);` |
|     103 | 1752 | `		if( pFmt == 0 ){` |
|       7 | 1753 | `			return 0;` |
|       - | 1754 | `		}` |
|      97 | 1755 | `		iFmtStack = Html5StackIndex(p,pFmt);` |
|      97 | 1756 | `		if( iFmtStack < 0 ){` |
|       3 | 1757 | `			Html5FmtRemove(p,pFmt);` |
|       3 | 1758 | `			return 1;` |
|       - | 1759 | `		}` |
|      95 | 1760 | `		if( !Html5InScope(p,pFmt) ){` |
|       3 | 1761 | `			return 1;` |
|       - | 1762 | `		}` |
|      93 | 1763 | `		apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|      93 | 1764 | `		n = (int)SySetUsed(&p->sOpen);` |
|     109 | 1765 | `		for( i = iFmtStack + 1 ; i < n ; ++i ){` |
|      29 | 1766 | `			if( HTML5_IN(azHtml5Special,(const char *)apStack[i]->name) ){` |
|      13 | 1767 | `				pFurthest = apStack[i];` |
|      13 | 1768 | `				break;` |
|       - | 1769 | `			}` |
|       9 | 1770 | `		}` |
|      93 | 1771 | `		if( pFurthest == 0 ){` |
|       - | 1772 | `			/* Nothing block-level is caught inside it: just close it. */` |
|     177 | 1773 | `			while( (int)SySetUsed(&p->sOpen) > iFmtStack ){` |
|      97 | 1774 | `				Html5Pop(p);` |
|       1 | 1775 | `			}` |
|      81 | 1776 | `			Html5FmtRemove(p,pFmt);` |
|      81 | 1777 | `			return 1;` |
|       - | 1778 | `		}` |
|      13 | 1779 | `		pAncestor = iFmtStack > 0 ? apStack[iFmtStack - 1] : 0;` |
|      13 | 1780 | `		iBookmark = Html5FmtIndex(p,pFmt);` |
|      13 | 1781 | `		pLast = pFurthest;` |
|      13 | 1782 | `		iNode = Html5StackIndex(p,pFurthest);` |
|       7 | 1783 | `		for( iInner = 1 ; ; ++iInner ){` |
|      13 | 1784 | `			if( --iNode < 0 ){` |
|     ! 0 | 1785 | `				break;` |
|       - | 1786 | `			}` |
|      13 | 1787 | `			pNode = ((xmlNodePtr *)SySetBasePtr(&p->sOpen))[iNode];` |
|      13 | 1788 | `			if( pNode == pFmt ){` |
|      13 | 1789 | `				break;` |
|       - | 1790 | `			}` |
|     ! 0 | 1791 | `			if( iInner > 3 ){` |
|     ! 0 | 1792 | `				Html5FmtRemove(p,pNode);` |
|     ! 0 | 1793 | `			}` |
|     ! 0 | 1794 | `			if( Html5FmtIndex(p,pNode) < 0 ){` |
|     ! 0 | 1795 | `				Html5SetRemoveAt(&p->sOpen,iNode);` |
|     ! 0 | 1796 | `				continue;` |
|       - | 1797 | `			}` |
|     ! 0 | 1798 | `			pNew = Html5CloneFmt(p,pNode);` |
|     ! 0 | 1799 | `			if( pNew == 0 ){` |
|     ! 0 | 1800 | `				break;` |
|       - | 1801 | `			}` |
|     ! 0 | 1802 | `			((xmlNodePtr *)SySetBasePtr(&p->sFmt))[Html5FmtIndex(p,pNode)] = pNew;` |
|     ! 0 | 1803 | `			((xmlNodePtr *)SySetBasePtr(&p->sOpen))[iNode] = pNew;` |
|     ! 0 | 1804 | `			if( pLast == pFurthest ){` |
|     ! 0 | 1805 | `				iBookmark = Html5FmtIndex(p,pNew) + 1;` |
|     ! 0 | 1806 | `			}` |
|     ! 0 | 1807 | `			xmlUnlinkNode(pLast);` |
|     ! 0 | 1808 | `			xmlAddChild(pNew,pLast);` |
|     ! 0 | 1809 | `			pLast = pNew;` |
|     ! 0 | 1810 | `		}` |
|      13 | 1811 | `		xmlUnlinkNode(pLast);` |
|      13 | 1812 | `		xmlAddChild(pAncestor ? pAncestor : (xmlNodePtr)p->pDoc,pLast);` |
|      13 | 1813 | `		pNew = Html5CloneFmt(p,pFmt);` |
|      13 | 1814 | `		if( pNew == 0 ){` |
|     ! 0 | 1815 | `			return 1;` |
|       - | 1816 | `		}` |
|      13 | 1817 | `		pChild = pFurthest->children;` |
|      25 | 1818 | `		while( pChild ){` |
|      13 | 1819 | `			xmlNodePtr pNext = pChild->next;` |
|      13 | 1820 | `			xmlUnlinkNode(pChild);` |
|      13 | 1821 | `			xmlAddChild(pNew,pChild);` |
|      13 | 1822 | `			pChild = pNext;` |
|       1 | 1823 | `		}` |
|      13 | 1824 | `		xmlAddChild(pFurthest,pNew);` |
|      13 | 1825 | `		i = Html5FmtIndex(p,pFmt);` |
|      13 | 1826 | `		if( i >= 0 ){` |
|      13 | 1827 | `			Html5SetRemoveAt(&p->sFmt,i);` |
|      13 | 1828 | `			if( iBookmark > i ){` |
|     ! 0 | 1829 | `				iBookmark--;` |
|     ! 0 | 1830 | `			}` |
|       6 | 1831 | `		}` |
|      13 | 1832 | `		Html5SetInsertAt(&p->sFmt,iBookmark,pNew);` |
|      13 | 1833 | `		i = Html5StackIndex(p,pFmt);` |
|      13 | 1834 | `		if( i >= 0 ){` |
|      13 | 1835 | `			Html5SetRemoveAt(&p->sOpen,i);` |
|       6 | 1836 | `		}` |
|      13 | 1837 | `		Html5SetInsertAt(&p->sOpen,Html5StackIndex(p,pFurthest) + 1,pNew);` |
|       7 | 1838 | `	}` |
|     ! 0 | 1839 | `	return 1;` |
|      47 | 1840 | `}` |
|       - | 1841 | `/* Does a start tag of this name reconstruct before it is inserted? */` |
|   13702 | 1842 | `static int Html5Reconstructs(const char *zName)` |
|       1 | 1843 | `{` |
|   20284 | 1844 | `	return !HTML5_IN(azHtml5Special,zName)` |
|   13702 | 1845 | `		\|\| HTML5_IN(azHtml5ReconSpecial,zName);` |
|       1 | 1846 | `}` |
|       - | 1847 | `/*` |
|       - | 1848 | `` * The spec's `any other end tag`, which is where a formatting name lands when`` |
|       - | 1849 | ` * the list does not hold it: walk out from the current node, close the first` |
|       - | 1850 | `` * element of that name, and STOP at the first special one -- a `</b>` spelled`` |
|       - | 1851 | `` * inside a `<td>` whose `<b>` is outside it closes nothing at all.`` |
|       - | 1852 | ` */` |
|       6 | 1853 | `static void Html5EndTagOther(html5_parser *p,const char *zName)` |
|       1 | 1854 | `{` |
|       7 | 1855 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|       7 | 1856 | `	int n = (int)SySetUsed(&p->sOpen);` |
|       7 | 1857 | `	while( n-- > 0 ){` |
|       7 | 1858 | `		if( Html5Eq((const char *)apStack[n]->name,zName) ){` |
|     ! 0 | 1859 | `			while( (int)SySetUsed(&p->sOpen) > n ){` |
|     ! 0 | 1860 | `				Html5Pop(p);` |
|     ! 0 | 1861 | `			}` |
|     ! 0 | 1862 | `			return;` |
|       - | 1863 | `		}` |
|       7 | 1864 | `		if( HTML5_IN(azHtml5Special,(const char *)apStack[n]->name) ){` |
|       7 | 1865 | `			return;` |
|       - | 1866 | `		}` |
|     ! 0 | 1867 | `	}` |
|       4 | 1868 | `}` |
|       - | 1869 | `/*` |
|       - | 1870 | ``  * Is an element of this name open, with no scope marker above it?  `bButton` `` |
|       - | 1871 | ``  * asks the spec's BUTTON scope instead, which is the same walk with `<button>` `` |
|       - | 1872 | `` * added to what stops it.  Only the `<p>` questions ask that one, and the two`` |
|       - | 1873 | `` * answers differ in exactly one shape: a `<p>` with a `<button>` open inside`` |
|       - | 1874 | `` * it is out of reach, so `<p><button><div>` leaves the div in the button`` |
|       - | 1875 | `` * rather than closing the paragraph, and a `</p>` there closes nothing and`` |
|       - | 1876 | ` * mints instead.` |
|       - | 1877 | ` */` |
|   23782 | 1878 | `static int Html5NameInScopeEx(html5_parser *p,const char *zName,int bButton)` |
|       1 | 1879 | `{` |
|   23783 | 1880 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|   23783 | 1881 | `	int n = (int)SySetUsed(&p->sOpen);` |
|   36141 | 1882 | `	while( n-- > 0 ){` |
|   36139 | 1883 | `		const char *zOpen = (const char *)apStack[n]->name;` |
|   36139 | 1884 | `		if( Html5Eq(zOpen,zName) ){` |
|   11517 | 1885 | `			return 1;` |
|       - | 1886 | `		}` |
|   24622 | 1887 | `		if( HTML5_IN(azHtml5Scope,zOpen)` |
|   18497 | 1888 | `		 \|\| (bButton && Html5Eq(zOpen,"button")) ){` |
|   12265 | 1889 | `			return 0;` |
|       - | 1890 | `		}` |
|       1 | 1891 | `	}` |
|       3 | 1892 | `	return 0;` |
|   11892 | 1893 | `}` |
|     252 | 1894 | `static int Html5NameInScope(html5_parser *p,const char *zName)` |
|       1 | 1895 | `{` |
|     253 | 1896 | `	return Html5NameInScopeEx(p,zName,FALSE);` |
|       1 | 1897 | `}` |
|       - | 1898 | ``/* The current start tag's value for `zName`, or a NULL when it has none. */`` |
|      60 | 1899 | `static const char * Html5TokAttr(html5_parser *p,const char *zName)` |
|       1 | 1900 | `{` |
|      61 | 1901 | `	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);` |
|      61 | 1902 | `	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);` |
|      61 | 1903 | `	sxu32 i,n = SySetUsed(&p->sAttr);` |
|      61 | 1904 | `	for( i = 0 ; i < n ; ++i ){` |
|      17 | 1905 | `		if( Html5Eq(zBuf + aAttr[i].nNameOfs,zName) ){` |
|      17 | 1906 | `			return zBuf + aAttr[i].nValOfs;` |
|       - | 1907 | `		}` |
|     ! 0 | 1908 | `	}` |
|      45 | 1909 | `	return 0;` |
|      31 | 1910 | `}` |
|       - | 1911 | `/*` |
|       - | 1912 | ` * Does a table's own insertion mode handle this start tag, so that it stays` |
|       - | 1913 | `` * inside the table?  `<input>` is the one answered by an attribute: a hidden`` |
|       - | 1914 | ` * one is table furniture, any other is content and leaves.` |
|       - | 1915 | ` */` |
|   13732 | 1916 | `static int Html5TableOwns(html5_parser *p,const char *zName)` |
|       1 | 1917 | `{` |
|   13733 | 1918 | `	if( Html5Eq(zName,"input") ){` |
|      61 | 1919 | `		const char *zType = Html5TokAttr(p,"type");` |
|      61 | 1920 | `		return zType && Html5Eq(zType,"hidden");` |
|       - | 1921 | `	}` |
|   13673 | 1922 | `	return Html5Eq(zName,"form") \|\| HTML5_IN(azHtml5TableOwn,zName);` |
|    6867 | 1923 | `}` |
|       - | 1924 | `/* Insert the current start tag, pushing it unless it takes no children. */` |
|   14258 | 1925 | `static xmlNodePtr Html5InsertStart(html5_parser *p)` |
|       1 | 1926 | `{` |
|   14259 | 1927 | `	const char *zName = Html5TokName(p);` |
|   14259 | 1928 | `	xmlNodePtr pEl = Html5NewElem(p,zName);` |
|   14259 | 1929 | `	if( pEl == 0 ){` |
|     ! 0 | 1930 | `		return 0;` |
|       - | 1931 | `	}` |
|   14259 | 1932 | `	Html5AddAttrs(p,pEl);` |
|   14259 | 1933 | `	if( HTML5_IN(azHtml5Void,zName) ){` |
|     421 | 1934 | `		return pEl;` |
|       - | 1935 | `	}` |
|   13839 | 1936 | `	Html5Push(p,pEl);` |
|   13839 | 1937 | `	if( HTML5_IN(azHtml5Rcdata,zName) ){` |
|      55 | 1938 | `		p->iText = HTML5_TEXT_RCDATA;` |
|   13812 | 1939 | `	}else if( HTML5_IN(azHtml5Raw,zName) ){` |
|     121 | 1940 | `		p->iText = HTML5_TEXT_RAW;` |
|   13725 | 1941 | `	}else if( Html5Eq(zName,"plaintext") ){` |
|      29 | 1942 | `		p->iText = HTML5_TEXT_PLAIN;` |
|      14 | 1943 | `	}` |
|   13839 | 1944 | `	return pEl;` |
|    7130 | 1945 | `}` |
|       - | 1946 | `/*` |
|       - | 1947 | `` * A `<template>` is the one element whose content is parsed in a mode of its`` |
|       - | 1948 | ` * own.  Every mode that can meet the start tag hands it here: the element is` |
|       - | 1949 | ` * inserted where the current mode would put it, the mode that was running is` |
|       - | 1950 | `` * PARKED, and the parser switches to `in template` until the matching end tag`` |
|       - | 1951 | ` * hands the parked mode back.  Templates nest, so the parked modes are a` |
|       - | 1952 | `` * stack rather than one slot -- without it a `<template>` written in the head`` |
|       - | 1953 | `` * fell through `in head`'s "anything else", which pops and re-runs the token`` |
|       - | 1954 | `` * in `after head`, minting a second `<body>` INSIDE the head to hold content`` |
|       - | 1955 | ` * that belongs in the template.` |
|       - | 1956 | ` */` |
|      40 | 1957 | `static void Html5TemplateStart(html5_parser *p)` |
|       1 | 1958 | `{` |
|      41 | 1959 | `	int iSave = p->iMode;` |
|      41 | 1960 | `	Html5InsertStart(p);` |
|      41 | 1961 | `	Html5FmtMarker(p);` |
|      41 | 1962 | `	p->bFramesetOk = 0;` |
|      41 | 1963 | `	SySetPut(&p->sTmpl,(const void *)&iSave);` |
|      41 | 1964 | `	p->iMode = HTML5_M_IN_TEMPLATE;` |
|      41 | 1965 | `}` |
|       - | 1966 | `/*` |
|       - | 1967 | ` * The matching end tag, and the only way out short of the end of the document.` |
|       - | 1968 | `` * An end tag with no template open is dropped where a bare `Html5EndTag` would`` |
|       - | 1969 | `` * have popped whatever else was open -- `</template>` written in a head with no`` |
|       - | 1970 | ` * template closed the HEAD and sent the rest of it into the body.` |
|       - | 1971 | ` */` |
|      40 | 1972 | `static void Html5TemplateEnd(html5_parser *p)` |
|       1 | 1973 | `{` |
|       - | 1974 | `	int *aMode;` |
|       - | 1975 | `	sxu32 n;` |
|      41 | 1976 | `	if( Html5OpenDepth(p,"template") < 0 ){` |
|       3 | 1977 | `		return;` |
|       - | 1978 | `	}` |
|      39 | 1979 | `	Html5PopTo(p,"template");` |
|      39 | 1980 | `	Html5FmtClearToMarker(p);` |
|      39 | 1981 | `	n = SySetUsed(&p->sTmpl);` |
|      39 | 1982 | `	aMode = (int *)SySetBasePtr(&p->sTmpl);` |
|      39 | 1983 | `	if( n > 0 ){` |
|      39 | 1984 | `		p->iMode = aMode[n - 1];` |
|      39 | 1985 | `		SySetPop(&p->sTmpl);` |
|      20 | 1986 | `	}else{` |
|     ! 0 | 1987 | `		p->iMode = HTML5_M_IN_BODY;` |
|       - | 1988 | `	}` |
|      21 | 1989 | `}` |
|   14278 | 1990 | `static void Html5InsertText(html5_parser *p)` |
|       1 | 1991 | `{` |
|   14279 | 1992 | `	sxu32 n = SyBlobLength(&p->sBuf);` |
|   14279 | 1993 | `	if( n < 1 ){` |
|      13 | 1994 | `		return;` |
|       - | 1995 | `	}` |
|       - | 1996 | `	/*` |
|       - | 1997 | `	 * Html5Attach merges a text node into a preceding text sibling on BOTH of` |
|       - | 1998 | `	 * its paths, which is exactly the spec's rule, so the tree never carries` |
|       - | 1999 | `	 * two adjacent ones.` |
|       - | 2000 | `	 */` |
|   21400 | 2001 | `	Html5Attach(p,xmlNewDocTextLen(p->pDoc,` |
|   14266 | 2002 | `		(const xmlChar *)SyBlobData(&p->sBuf),(int)n));` |
|    7140 | 2003 | `}` |
|       - | 2004 | `/*` |
|       - | 2005 | ` * The frameset modes keep the whitespace of a character run and drop the rest` |
|       - | 2006 | ` * of it, so a run that is a mixture is inserted with its other bytes removed` |
|       - | 2007 | ` * rather than kept whole or dropped whole.` |
|       - | 2008 | ` */` |
|      28 | 2009 | `static void Html5InsertSpaceOnly(html5_parser *p)` |
|       1 | 2010 | `{` |
|      29 | 2011 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);` |
|      29 | 2012 | `	sxu32 i,n = SyBlobLength(&p->sBuf),nKeep = 0;` |
|      29 | 2013 | `	unsigned char *zKeep = (unsigned char *)SyBlobData(&p->sBuf);` |
|      87 | 2014 | `	for( i = 0 ; i < n ; ++i ){` |
|      59 | 2015 | `		if( Html5IsSpace(z[i]) ){` |
|      35 | 2016 | `			zKeep[nKeep++] = z[i];` |
|      17 | 2017 | `		}` |
|      30 | 2018 | `	}` |
|      29 | 2019 | `	p->sBuf.nByte = nKeep;` |
|      29 | 2020 | `	Html5InsertText(p);` |
|      29 | 2021 | `}` |
|   14004 | 2022 | `static int Html5TextIsSpace(html5_parser *p)` |
|       1 | 2023 | `{` |
|   14005 | 2024 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);` |
|   14005 | 2025 | `	sxu32 i,n = SyBlobLength(&p->sBuf);` |
|   14209 | 2026 | `	for( i = 0 ; i < n ; ++i ){` |
|   14061 | 2027 | `		if( !Html5IsSpace(z[i]) ){` |
|   13857 | 2028 | `			return 0;` |
|       - | 2029 | `		}` |
|     103 | 2030 | `	}` |
|     149 | 2031 | `	return 1;` |
|    7003 | 2032 | `}` |
|       - | 2033 | `/* The same question a mode that DROPS whitespace asks -- a form feed is not. */` |
|     702 | 2034 | `static int Html5TextIsIgnSpace(html5_parser *p)` |
|       1 | 2035 | `{` |
|     703 | 2036 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);` |
|     703 | 2037 | `	sxu32 i,n = SyBlobLength(&p->sBuf);` |
|    4889 | 2038 | `	for( i = 0 ; i < n ; ++i ){` |
|    4793 | 2039 | `		if( !Html5IsIgnSpace(z[i]) ){` |
|     607 | 2040 | `			return 0;` |
|       - | 2041 | `		}` |
|    2094 | 2042 | `	}` |
|      97 | 2043 | `	return 1;` |
|     352 | 2044 | `}` |
|       - | 2045 | `/* Drop the leading whitespace of a text token, keeping the rest. */` |
|     548 | 2046 | `static void Html5TrimLeadingSpace(html5_parser *p)` |
|       1 | 2047 | `{` |
|     549 | 2048 | `	unsigned char *z = (unsigned char *)SyBlobData(&p->sBuf);` |
|     549 | 2049 | `	sxu32 i = 0,n = SyBlobLength(&p->sBuf);` |
|     549 | 2050 | `	if( n < 1 ){` |
|     ! 0 | 2051 | `		return;` |
|       - | 2052 | `	}` |
|     607 | 2053 | `	while( i < n && Html5IsIgnSpace(z[i]) ){` |
|      59 | 2054 | `		i++;` |
|       1 | 2055 | `	}` |
|     549 | 2056 | `	if( i > 0 ){` |
|       - | 2057 | `		sxu32 j;` |
|     165 | 2058 | `		for( j = 0 ; j < n - i ; ++j ){` |
|     109 | 2059 | `			z[j] = z[j + i];` |
|      55 | 2060 | `		}` |
|      57 | 2061 | `		p->sBuf.nByte = n - i;` |
|      28 | 2062 | `	}` |
|     275 | 2063 | `}` |
|       - | 2064 | `/*` |
|       - | 2065 | ` * The head modes KEEP that whitespace rather than dropping it: the leading run` |
|       - | 2066 | ` * is inserted where a run of nothing but whitespace would have gone, and only` |
|       - | 2067 | `` * the rest is handed to the mode this token is about to walk into.  `before`` |
|       - | 2068 | `` * html` and `before head` ignore it instead, so they still trim without`` |
|       - | 2069 | ` * inserting -- which is why this is a second helper and not a flag on the` |
|       - | 2070 | ` * first.` |
|       - | 2071 | ` */` |
|     392 | 2072 | `static void Html5SplitLeadingSpace(html5_parser *p)` |
|       1 | 2073 | `{` |
|     393 | 2074 | `	const unsigned char *z = (const unsigned char *)SyBlobData(&p->sBuf);` |
|     393 | 2075 | `	sxu32 i = 0,n = SyBlobLength(&p->sBuf);` |
|     433 | 2076 | `	while( i < n && Html5IsIgnSpace(z[i]) ){` |
|      41 | 2077 | `		i++;` |
|       1 | 2078 | `	}` |
|     393 | 2079 | `	if( i > 0 ){` |
|      39 | 2080 | `		p->sBuf.nByte = i;` |
|      39 | 2081 | `		Html5InsertText(p);` |
|      39 | 2082 | `		p->sBuf.nByte = n;` |
|      19 | 2083 | `	}` |
|     393 | 2084 | `	Html5TrimLeadingSpace(p);` |
|     393 | 2085 | `}` |
|     306 | 2086 | `static void Html5InsertComment(html5_parser *p,xmlNodePtr pParent)` |
|       1 | 2087 | `{` |
|       - | 2088 | `	xmlNodePtr pC;` |
|     307 | 2089 | `	SyBlobNullAppend(&p->sBuf);` |
|     307 | 2090 | `	pC = xmlNewDocComment(p->pDoc,(const xmlChar *)SyBlobData(&p->sBuf));` |
|     307 | 2091 | `	if( pC ){` |
|     307 | 2092 | `		xmlAddChild(pParent,pC);` |
|     153 | 2093 | `	}` |
|     307 | 2094 | `}` |
|       - | 2095 | `/* ------------------------------------------------------------------ *` |
|       - | 2096 | ` * Foreign content: the SVG and MathML subtrees` |
|       - | 2097 | ` * ------------------------------------------------------------------ */` |
|       - | 2098 |  |
|       - | 2099 | `/*` |
|       - | 2100 | `` * Inside a `<svg>` or a `<math>` the tokenizer still lowercases every name it`` |
|       - | 2101 | ` * reads, because it has no idea what tree it is feeding.  These two tables are` |
|       - | 2102 | ` * the spec's repair: the SVG names whose real spelling is mixed-case, and the` |
|       - | 2103 | ` * SVG attributes likewise.  A name that is in neither keeps the lowercased` |
|       - | 2104 | `` * spelling -- an unknown `<unknownThing>` is an `unknownthing` there too.`` |
|       - | 2105 | ` */` |
|       - | 2106 | `typedef struct html5_fix html5_fix;` |
|       - | 2107 | `struct html5_fix { const char *zLow; const char *zFix; };` |
|       - | 2108 | `static const html5_fix aHtml5SvgTag[] = {` |
|       - | 2109 | `	{"altglyph","altGlyph"},{"altglyphdef","altGlyphDef"},` |
|       - | 2110 | `	{"altglyphitem","altGlyphItem"},{"animatecolor","animateColor"},` |
|       - | 2111 | `	{"animatemotion","animateMotion"},{"animatetransform","animateTransform"},` |
|       - | 2112 | `	{"clippath","clipPath"},{"feblend","feBlend"},` |
|       - | 2113 | `	{"fecolormatrix","feColorMatrix"},` |
|       - | 2114 | `	{"fecomponenttransfer","feComponentTransfer"},` |
|       - | 2115 | `	{"fecomposite","feComposite"},{"feconvolvematrix","feConvolveMatrix"},` |
|       - | 2116 | `	{"fediffuselighting","feDiffuseLighting"},` |
|       - | 2117 | `	{"fedisplacementmap","feDisplacementMap"},` |
|       - | 2118 | `	{"fedistantlight","feDistantLight"},{"fedropshadow","feDropShadow"},` |
|       - | 2119 | `	{"feflood","feFlood"},{"fefunca","feFuncA"},{"fefuncb","feFuncB"},` |
|       - | 2120 | `	{"fefuncg","feFuncG"},{"fefuncr","feFuncR"},` |
|       - | 2121 | `	{"fegaussianblur","feGaussianBlur"},{"feimage","feImage"},` |
|       - | 2122 | `	{"femerge","feMerge"},{"femergenode","feMergeNode"},` |
|       - | 2123 | `	{"femorphology","feMorphology"},{"feoffset","feOffset"},` |
|       - | 2124 | `	{"fepointlight","fePointLight"},` |
|       - | 2125 | `	{"fespecularlighting","feSpecularLighting"},` |
|       - | 2126 | `	{"fespotlight","feSpotLight"},{"fetile","feTile"},` |
|       - | 2127 | `	{"feturbulence","feTurbulence"},{"foreignobject","foreignObject"},` |
|       - | 2128 | `	{"glyphref","glyphRef"},{"lineargradient","linearGradient"},` |
|       - | 2129 | `	{"radialgradient","radialGradient"},{"textpath","textPath"}` |
|       - | 2130 | `};` |
|       - | 2131 | `static const html5_fix aHtml5SvgAttr[] = {` |
|       - | 2132 | `	{"attributename","attributeName"},{"attributetype","attributeType"},` |
|       - | 2133 | `	{"basefrequency","baseFrequency"},{"baseprofile","baseProfile"},` |
|       - | 2134 | `	{"calcmode","calcMode"},{"clippathunits","clipPathUnits"},` |
|       - | 2135 | `	{"diffuseconstant","diffuseConstant"},{"edgemode","edgeMode"},` |
|       - | 2136 | `	{"filterunits","filterUnits"},{"glyphref","glyphRef"},` |
|       - | 2137 | `	{"gradienttransform","gradientTransform"},{"gradientunits","gradientUnits"},` |
|       - | 2138 | `	{"kernelmatrix","kernelMatrix"},{"kernelunitlength","kernelUnitLength"},` |
|       - | 2139 | `	{"keypoints","keyPoints"},{"keysplines","keySplines"},` |
|       - | 2140 | `	{"keytimes","keyTimes"},{"lengthadjust","lengthAdjust"},` |
|       - | 2141 | `	{"limitingconeangle","limitingConeAngle"},{"markerheight","markerHeight"},` |
|       - | 2142 | `	{"markerunits","markerUnits"},{"markerwidth","markerWidth"},` |
|       - | 2143 | `	{"maskcontentunits","maskContentUnits"},{"maskunits","maskUnits"},` |
|       - | 2144 | `	{"numoctaves","numOctaves"},{"pathlength","pathLength"},` |
|       - | 2145 | `	{"patterncontentunits","patternContentUnits"},` |
|       - | 2146 | `	{"patterntransform","patternTransform"},{"patternunits","patternUnits"},` |
|       - | 2147 | `	{"pointsatx","pointsAtX"},{"pointsaty","pointsAtY"},` |
|       - | 2148 | `	{"pointsatz","pointsAtZ"},{"preservealpha","preserveAlpha"},` |
|       - | 2149 | `	{"preserveaspectratio","preserveAspectRatio"},` |
|       - | 2150 | `	{"primitiveunits","primitiveUnits"},{"refx","refX"},{"refy","refY"},` |
|       - | 2151 | `	{"repeatcount","repeatCount"},{"repeatdur","repeatDur"},` |
|       - | 2152 | `	{"requiredextensions","requiredExtensions"},` |
|       - | 2153 | `	{"requiredfeatures","requiredFeatures"},` |
|       - | 2154 | `	{"specularconstant","specularConstant"},` |
|       - | 2155 | `	{"specularexponent","specularExponent"},{"spreadmethod","spreadMethod"},` |
|       - | 2156 | `	{"startoffset","startOffset"},{"stddeviation","stdDeviation"},` |
|       - | 2157 | `	{"stitchtiles","stitchTiles"},{"surfacescale","surfaceScale"},` |
|       - | 2158 | `	{"systemlanguage","systemLanguage"},{"tablevalues","tableValues"},` |
|       - | 2159 | `	{"targetx","targetX"},{"targety","targetY"},{"textlength","textLength"},` |
|       - | 2160 | `	{"viewbox","viewBox"},{"viewtarget","viewTarget"},` |
|       - | 2161 | `	{"xchannelselector","xChannelSelector"},` |
|       - | 2162 | `	{"ychannelselector","yChannelSelector"},{"zoomandpan","zoomAndPan"}` |
|       - | 2163 | `};` |
|       - | 2164 | `/*` |
|       - | 2165 | ` * The HTML start tags that give up on the foreign subtree entirely: they are` |
|       - | 2166 | `` * so much more likely to be a page that forgot to close its `<svg>` than SVG`` |
|       - | 2167 | ` * content that the spec pops back out to HTML and reprocesses the tag there.` |
|       - | 2168 | `` * `<font>` joins them only when it carries one of the three attributes that`` |
|       - | 2169 | ` * make it the HTML one.` |
|       - | 2170 | ` */` |
|       - | 2171 | `static const char * const azHtml5Breakout[] = {` |
|       - | 2172 | `	"b","big","blockquote","body","br","center","code","dd","div","dl","dt",` |
|       - | 2173 | `	"em","embed","h1","h2","h3","h4","h5","h6","head","hr","i","img","li",` |
|       - | 2174 | `	"listing","menu","meta","nobr","ol","p","pre","ruby","s","small","span",` |
|       - | 2175 | `	"strong","strike","sub","sup","table","tt","u","ul","var"` |
|       - | 2176 | `};` |
|       - | 2177 | `/* The local names of the attributes that carry a namespace of their own. */` |
|       - | 2178 | `static const char * const azHtml5Xlink[] = {` |
|       - | 2179 | `	"actuate","arcrole","href","role","show","title","type"` |
|       - | 2180 | `};` |
|       - | 2181 | `static const char * const azHtml5Xml[] = { "lang","space" };` |
|       - | 2182 |  |
|     100 | 2183 | `static int Html5LowerEq(const char *zLeft,const char *zRight)` |
|       1 | 2184 | `{` |
|     451 | 2185 | `	while( *zLeft != 0 && Html5Lower(*zLeft) == Html5Lower(*zRight) ){` |
|     351 | 2186 | `		zLeft++;` |
|     351 | 2187 | `		zRight++;` |
|       1 | 2188 | `	}` |
|     101 | 2189 | `	return *zLeft == 0 && *zRight == 0;` |
|       1 | 2190 | `}` |
|       - | 2191 | `/* The namespace an open element is in.  Everything that is not one of the two` |
|       - | 2192 | ` * foreign ones -- the HTML namespace, no namespace at all -- answers HTML, and` |
|       - | 2193 | ` * that is what every rule below tests for. */` |
|   81218 | 2194 | `static int Html5NsKind(xmlNodePtr pEl)` |
|       1 | 2195 | `{` |
|       - | 2196 | `	const char *zHref;` |
|   81219 | 2197 | `	if( pEl == 0 \|\| pEl->ns == 0 \|\| pEl->ns->href == 0 ){` |
|     607 | 2198 | `		return HTML5_NSK_HTML;` |
|       - | 2199 | `	}` |
|   80613 | 2200 | `	zHref = (const char *)pEl->ns->href;` |
|   80613 | 2201 | `	if( Html5Eq(zHref,HTML5_SVG_NS) ){` |
|     543 | 2202 | `		return HTML5_NSK_SVG;` |
|       - | 2203 | `	}` |
|   80071 | 2204 | `	if( Html5Eq(zHref,HTML5_MATH_NS) ){` |
|     271 | 2205 | `		return HTML5_NSK_MATH;` |
|       - | 2206 | `	}` |
|   79801 | 2207 | `	return HTML5_NSK_HTML;` |
|   40610 | 2208 | `}` |
|     120 | 2209 | `static const char * Html5NsHrefOf(int iKind)` |
|       1 | 2210 | `{` |
|     121 | 2211 | `	if( iKind == HTML5_NSK_SVG ){` |
|      83 | 2212 | `		return HTML5_SVG_NS;` |
|       - | 2213 | `	}` |
|      39 | 2214 | `	return iKind == HTML5_NSK_MATH ? HTML5_MATH_NS : 0;` |
|      61 | 2215 | `}` |
|       - | 2216 | ``/* A MathML `<annotation-xml>` whose `encoding` names an HTML flavour, and the`` |
|       - | 2217 | ` * three SVG elements that hold HTML outright: inside one of these the HTML` |
|       - | 2218 | `` * rules run again, so a `<b>` under a `<foreignObject>` is an HTML `<b>`. */`` |
|     180 | 2219 | `static int Html5IsHtmlIp(xmlNodePtr pEl)` |
|       1 | 2220 | `{` |
|     181 | 2221 | `	int iKind = Html5NsKind(pEl);` |
|       - | 2222 | `	const char *zName;` |
|     181 | 2223 | `	if( iKind == HTML5_NSK_HTML ){` |
|     ! 0 | 2224 | `		return 0;` |
|       - | 2225 | `	}` |
|     181 | 2226 | `	zName = (const char *)pEl->name;` |
|     181 | 2227 | `	if( iKind == HTML5_NSK_SVG ){` |
|     237 | 2228 | `		return Html5Eq(zName,"foreignObject") \|\| Html5Eq(zName,"desc")` |
|     180 | 2229 | `			\|\| Html5Eq(zName,"title");` |
|       - | 2230 | `	}` |
|      57 | 2231 | `	if( Html5Eq(zName,"annotation-xml") ){` |
|       5 | 2232 | `		xmlChar *zEnc = xmlGetNoNsProp(pEl,(const xmlChar *)"encoding");` |
|       7 | 2233 | `		int bHit = zEnc && (Html5LowerEq((const char *)zEnc,"text/html")` |
|       2 | 2234 | `			\|\| Html5LowerEq((const char *)zEnc,"application/xhtml+xml"));` |
|       5 | 2235 | `		if( zEnc ){` |
|       5 | 2236 | `			xmlFree(zEnc);` |
|       2 | 2237 | `		}` |
|       5 | 2238 | `		return bHit;` |
|       - | 2239 | `	}` |
|      53 | 2240 | `	return 0;` |
|      91 | 2241 | `}` |
|       - | 2242 | `/* The five MathML token elements: HTML rules for everything but the two tags` |
|       - | 2243 | ` * that are genuinely MathML's own. */` |
|     190 | 2244 | `static int Html5IsMathIp(xmlNodePtr pEl)` |
|       1 | 2245 | `{` |
|     191 | 2246 | `	const char *zName = pEl ? (const char *)pEl->name : "";` |
|     224 | 2247 | `	return Html5NsKind(pEl) == HTML5_NSK_MATH` |
|     248 | 2248 | `		&& (Html5Eq(zName,"mi") \|\| Html5Eq(zName,"mo") \|\| Html5Eq(zName,"mn")` |
|      50 | 2249 | `		 \|\| Html5Eq(zName,"ms") \|\| Html5Eq(zName,"mtext"));` |
|       1 | 2250 | `}` |
|      96 | 2251 | `static const char * Html5TableFix(const char *zName,const html5_fix *aTab,int nTab)` |
|       1 | 2252 | `{` |
|       - | 2253 | `	int i;` |
|    3619 | 2254 | `	for( i = 0 ; i < nTab ; ++i ){` |
|    3543 | 2255 | `		if( Html5Eq(aTab[i].zLow,zName) ){` |
|      21 | 2256 | `			return aTab[i].zFix;` |
|       - | 2257 | `		}` |
|    1762 | 2258 | `	}` |
|      77 | 2259 | `	return zName;` |
|      49 | 2260 | `}` |
|       - | 2261 | `/*` |
|       - | 2262 | ` * The attributes of a foreign start tag.  Three things happen to a name that` |
|       - | 2263 | ``  * do not happen in HTML: the SVG table restores its case, `definitionurl` `` |
|       - | 2264 | `` * becomes MathML's `definitionURL`, and the handful of `xlink:`/`xml:` names`` |
|       - | 2265 | ` * move into a real namespace instead of staying a name with a colon in it.` |
|       - | 2266 | `` * Anything else -- `foo:bar` included -- is stored verbatim.`` |
|       - | 2267 | ` */` |
|     120 | 2268 | `static void Html5AddAttrsForeign(html5_parser *p,xmlNodePtr pEl,int iKind)` |
|       1 | 2269 | `{` |
|     121 | 2270 | `	html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);` |
|     121 | 2271 | `	const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);` |
|     121 | 2272 | `	sxu32 i,n = SySetUsed(&p->sAttr);` |
|     145 | 2273 | `	for( i = 0 ; i < n ; ++i ){` |
|      25 | 2274 | `		const char *zName = zBuf + aAttr[i].nNameOfs;` |
|      25 | 2275 | `		const char *zVal = zBuf + aAttr[i].nValOfs;` |
|      25 | 2276 | `		const char *zLocal = 0;` |
|      25 | 2277 | `		const char *zHref = 0;` |
|      25 | 2278 | `		const char *zPfx = 0;` |
|      25 | 2279 | `		if( zName[0] == 0 ){` |
|     ! 0 | 2280 | `			continue;` |
|       - | 2281 | `		}` |
|      24 | 2282 | `		if( SyStrncmp(zName,"xlink:",sizeof("xlink:")-1) == 0` |
|      15 | 2283 | `		 && HTML5_IN(azHtml5Xlink,zName + sizeof("xlink:") - 1) ){` |
|       5 | 2284 | `			zLocal = zName + sizeof("xlink:") - 1;` |
|       5 | 2285 | `			zHref = HTML5_XLINK_NS;` |
|       5 | 2286 | `			zPfx = "xlink";` |
|      23 | 2287 | `		}else if( SyStrncmp(zName,"xml:",sizeof("xml:")-1) == 0` |
|      12 | 2288 | `		 && HTML5_IN(azHtml5Xml,zName + sizeof("xml:") - 1) ){` |
|       3 | 2289 | `			zLocal = zName + sizeof("xml:") - 1;` |
|       3 | 2290 | `			zHref = HTML5_XML_NS;` |
|       3 | 2291 | `			zPfx = "xml";` |
|      20 | 2292 | `		}else if( iKind == HTML5_NSK_SVG ){` |
|      15 | 2293 | `			zLocal = Html5TableFix(zName,aHtml5SvgAttr,` |
|       - | 2294 | `				(int)SX_ARRAYSIZE(aHtml5SvgAttr));` |
|      12 | 2295 | `		}else if( Html5Eq(zName,"definitionurl") ){` |
|       3 | 2296 | `			zLocal = "definitionURL";` |
|       2 | 2297 | `		}else{` |
|       3 | 2298 | `			zLocal = zName;` |
|       - | 2299 | `		}` |
|      25 | 2300 | `		if( zHref ){` |
|       7 | 2301 | `			xmlNsPtr pNs = Html5NsGet(p,pEl,zHref,zPfx);` |
|       7 | 2302 | `			if( xmlHasNsProp(pEl,(const xmlChar *)zLocal,(const xmlChar *)zHref) ){` |
|     ! 0 | 2303 | `				continue;` |
|       - | 2304 | `			}` |
|       7 | 2305 | `			xmlNewNsProp(pEl,pNs,(const xmlChar *)zLocal,(const xmlChar *)zVal);` |
|       7 | 2306 | `			continue;` |
|       - | 2307 | `		}` |
|      19 | 2308 | `		if( xmlHasProp(pEl,(const xmlChar *)zLocal) ){` |
|     ! 0 | 2309 | `			continue;` |
|       - | 2310 | `		}` |
|      19 | 2311 | `		xmlNewProp(pEl,(const xmlChar *)zLocal,(const xmlChar *)zVal);` |
|      10 | 2312 | `	}` |
|     121 | 2313 | `}` |
|       - | 2314 | `/*` |
|       - | 2315 | ` * A foreign start tag.  There is no void-element list here and no armed text` |
|       - | 2316 | ` * flavour: the ONLY thing that closes an element without an end tag is the` |
|       - | 2317 | `` * `/>` the source wrote, which HTML ignores and foreign content honours.`` |
|       - | 2318 | ` */` |
|     120 | 2319 | `static xmlNodePtr Html5InsertForeign(html5_parser *p,int iKind)` |
|       1 | 2320 | `{` |
|     121 | 2321 | `	const char *zName = Html5TokName(p);` |
|       - | 2322 | `	xmlNodePtr pEl;` |
|     121 | 2323 | `	if( iKind == HTML5_NSK_SVG ){` |
|      83 | 2324 | `		zName = Html5TableFix(zName,aHtml5SvgTag,(int)SX_ARRAYSIZE(aHtml5SvgTag));` |
|      41 | 2325 | `	}` |
|     121 | 2326 | `	pEl = Html5NewElemNs(p,zName,Html5NsHrefOf(iKind));` |
|     121 | 2327 | `	if( pEl == 0 ){` |
|     ! 0 | 2328 | `		return 0;` |
|       - | 2329 | `	}` |
|     121 | 2330 | `	Html5AddAttrsForeign(p,pEl,iKind);` |
|     121 | 2331 | `	if( !p->bSelfClose ){` |
|      99 | 2332 | `		Html5Push(p,pEl);` |
|      49 | 2333 | `	}` |
|     121 | 2334 | `	return pEl;` |
|      61 | 2335 | `}` |
|       - | 2336 | `/* Does THIS token go through the foreign rules?  The question is asked of the` |
|       - | 2337 | `` * current node for every token, which is what lets one `<b>` inside a`` |
|       - | 2338 | `` * `<foreignObject>` be HTML while its `<circle>` sibling is not. */`` |
|  106240 | 2339 | `static int Html5UseForeign(html5_parser *p)` |
|       1 | 2340 | `{` |
|  106241 | 2341 | `	xmlNodePtr pCur = Html5Top(p);` |
|  106241 | 2342 | `	const char *zTok = Html5TokName(p);` |
|  106240 | 2343 | `	if( pCur == 0 \|\| p->iTok == HTML5_TOK_EOF` |
|   79967 | 2344 | `	 \|\| Html5NsKind(pCur) == HTML5_NSK_HTML ){` |
|  106071 | 2345 | `		return 0;` |
|       - | 2346 | `	}` |
|     171 | 2347 | `	if( Html5IsMathIp(pCur) ){` |
|      23 | 2348 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|       7 | 2349 | `			return 0;` |
|       - | 2350 | `		}` |
|      16 | 2351 | `		if( p->iTok == HTML5_TOK_START && !Html5Eq(zTok,"mglyph")` |
|       5 | 2352 | `		 && !Html5Eq(zTok,"malignmark") ){` |
|       5 | 2353 | `			return 0;` |
|       - | 2354 | `		}` |
|       6 | 2355 | `	}` |
|     160 | 2356 | `	if( Html5NsKind(pCur) == HTML5_NSK_MATH` |
|     107 | 2357 | `	 && Html5Eq((const char *)pCur->name,"annotation-xml")` |
|      30 | 2358 | `	 && p->iTok == HTML5_TOK_START && Html5Eq(zTok,"svg") ){` |
|     ! 0 | 2359 | `		return 0;` |
|       - | 2360 | `	}` |
|     160 | 2361 | `	if( Html5IsHtmlIp(pCur)` |
|      89 | 2362 | `	 && (p->iTok == HTML5_TOK_START \|\| p->iTok == HTML5_TOK_TEXT) ){` |
|       9 | 2363 | `		return 0;` |
|       - | 2364 | `	}` |
|     153 | 2365 | `	return 1;` |
|   53121 | 2366 | `}` |
|       - | 2367 | `/* Is this the HTML tag that gives up on the subtree? */` |
|      72 | 2368 | `static int Html5IsBreakout(html5_parser *p,const char *zName)` |
|       1 | 2369 | `{` |
|      73 | 2370 | `	if( HTML5_IN(azHtml5Breakout,zName) ){` |
|      13 | 2371 | `		return 1;` |
|       - | 2372 | `	}` |
|      61 | 2373 | `	if( Html5Eq(zName,"font") ){` |
|     ! 0 | 2374 | `		html5_attr *aAttr = (html5_attr *)SySetBasePtr(&p->sAttr);` |
|     ! 0 | 2375 | `		const char *zBuf = (const char *)SyBlobData(&p->sAttrBuf);` |
|     ! 0 | 2376 | `		sxu32 i,n = SySetUsed(&p->sAttr);` |
|     ! 0 | 2377 | `		for( i = 0 ; i < n ; ++i ){` |
|     ! 0 | 2378 | `			const char *zAt = zBuf + aAttr[i].nNameOfs;` |
|     ! 0 | 2379 | `			if( Html5Eq(zAt,"color") \|\| Html5Eq(zAt,"face")` |
|     ! 0 | 2380 | `			 \|\| Html5Eq(zAt,"size") ){` |
|     ! 0 | 2381 | `				return 1;` |
|       - | 2382 | `			}` |
|     ! 0 | 2383 | `		}` |
|     ! 0 | 2384 | `	}` |
|      61 | 2385 | `	return 0;` |
|      37 | 2386 | `}` |
|       - | 2387 | `/*` |
|       - | 2388 | ` * The foreign tree constructor.  Answers 1 when the token has to be handed` |
|       - | 2389 | ` * back to the HTML rules -- which is how both ways out of a foreign subtree` |
|       - | 2390 | ` * work: a breakout tag pops to the nearest HTML element and reprocesses, and` |
|       - | 2391 | ` * an end tag that matches nothing foreign walks down to an HTML element and` |
|       - | 2392 | ` * lets that element's own rules answer it.` |
|       - | 2393 | ` */` |
|     152 | 2394 | `static int Html5ForeignDispatch(html5_parser *p)` |
|       1 | 2395 | `{` |
|     153 | 2396 | `	const char *zName = Html5TokName(p);` |
|     153 | 2397 | `	switch( p->iTok ){` |
|       4 | 2398 | `	case HTML5_TOK_TEXT:` |
|       9 | 2399 | `		Html5InsertText(p);` |
|       9 | 2400 | `		return 0;` |
|     ! 0 | 2401 | `	case HTML5_TOK_COMMENT:` |
|     ! 0 | 2402 | `		Html5InsertComment(p,Html5Target(p));` |
|     ! 0 | 2403 | `		return 0;` |
|      36 | 2404 | `	case HTML5_TOK_START:` |
|      73 | 2405 | `		if( Html5IsBreakout(p,zName) ){` |
|      54 | 2406 | `			while( Html5Top(p) && Html5NsKind(Html5Top(p)) != HTML5_NSK_HTML` |
|      43 | 2407 | `			 && !Html5IsHtmlIp(Html5Top(p)) && !Html5IsMathIp(Html5Top(p)) ){` |
|      21 | 2408 | `				Html5Pop(p);` |
|       1 | 2409 | `			}` |
|      13 | 2410 | `			return 1;` |
|       - | 2411 | `		}` |
|      61 | 2412 | `		Html5InsertForeign(p,Html5NsKind(Html5Top(p)));` |
|      61 | 2413 | `		return 0;` |
|      36 | 2414 | `	case HTML5_TOK_END: {` |
|      73 | 2415 | `		xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|      73 | 2416 | `		int i = (int)SySetUsed(&p->sOpen) - 1;` |
|       - | 2417 | `		/* The comparison is case-INSENSITIVE, because the stack holds the` |
|       - | 2418 | `` 		 * repaired spelling and the token holds the lowercased one: `</G>` `` |
|       - | 2419 | ``		 * and `</clippath>` both close what they name. */`` |
|      97 | 2420 | `		for( ; i >= 0 ; --i ){` |
|      97 | 2421 | `			if( Html5LowerEq((const char *)apStack[i]->name,zName) ){` |
|     133 | 2422 | `				while( (int)SySetUsed(&p->sOpen) > i ){` |
|      71 | 2423 | `					Html5Pop(p);` |
|       1 | 2424 | `				}` |
|      63 | 2425 | `				return 0;` |
|       - | 2426 | `			}` |
|      35 | 2427 | `			if( Html5NsKind(apStack[i]) == HTML5_NSK_HTML ){` |
|      11 | 2428 | `				p->bHtmlRules = 1;` |
|      11 | 2429 | `				return 1;` |
|       - | 2430 | `			}` |
|      13 | 2431 | `		}` |
|     ! 0 | 2432 | `		return 0;` |
|       - | 2433 | `	}` |
|     ! 0 | 2434 | `	default:` |
|     ! 0 | 2435 | `		break;` |
|       - | 2436 | `	}` |
|     ! 0 | 2437 | `	return 0;` |
|      77 | 2438 | `}` |
|       - | 2439 | `/*` |
|       - | 2440 | ` * A list item's own start tag closes the one it is a sibling of -- which, with` |
|       - | 2441 | ` * a formatting element left open inside that one, is no longer the innermost` |
|       - | 2442 | ` * open element.  The walk is the spec's: out from the current node, closing` |
|       - | 2443 | `` * the first `zOne`/`zTwo` it reaches, and stopping at a special element that`` |
|       - | 2444 | ` * is not one of the three a list item is allowed to sit inside.` |
|       - | 2445 | ` */` |
|      60 | 2446 | `static void Html5CloseListItem(html5_parser *p,const char *zOne,const char *zTwo)` |
|       1 | 2447 | `{` |
|      61 | 2448 | `	xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|      61 | 2449 | `	int n = (int)SySetUsed(&p->sOpen);` |
|      65 | 2450 | `	while( n-- > 0 ){` |
|      65 | 2451 | `		const char *zTop = (const char *)apStack[n]->name;` |
|      65 | 2452 | `		if( Html5Eq(zTop,zOne) \|\| Html5Eq(zTop,zTwo) ){` |
|      17 | 2453 | `			while( (int)SySetUsed(&p->sOpen) > n ){` |
|      11 | 2454 | `				Html5Pop(p);` |
|       1 | 2455 | `			}` |
|       7 | 2456 | `			return;` |
|       - | 2457 | `		}` |
|      58 | 2458 | `		if( HTML5_IN(azHtml5Special,zTop) && !Html5Eq(zTop,"address")` |
|      55 | 2459 | `		 && !Html5Eq(zTop,"div") && !Html5Eq(zTop,"p") ){` |
|      55 | 2460 | `			return;` |
|       - | 2461 | `		}` |
|       1 | 2462 | `	}` |
|      31 | 2463 | `}` |
|       - | 2464 | `/*` |
|       - | 2465 | `` * The rules an open `<select>` adds to a start tag.  A second `<select>` and`` |
|       - | 2466 | `` * an `<input>` both close it -- the `<select>` is then dropped where the`` |
|       - | 2467 | `` * `<input>` goes on to be inserted beside it -- and an `<hr>` or an`` |
|       - | 2468 | `` * `<optgroup>` closes the `<option>`, and then the `<optgroup>`, the source`` |
|       - | 2469 | ` * left open, so both land on the select rather than inside the option.` |
|       - | 2470 | ` * Answers 1 when the token is spent and nothing is to be inserted for it.` |
|       - | 2471 | ` */` |
|   13796 | 2472 | `static int Html5SelectImplied(html5_parser *p,const char *zName)` |
|       1 | 2473 | `{` |
|   13797 | 2474 | `	int bSelect = Html5Eq(zName,"select");` |
|   13797 | 2475 | `	if( !Html5InSelect(p) ){` |
|   13219 | 2476 | `		return 0;` |
|       - | 2477 | `	}` |
|     579 | 2478 | `	if( bSelect \|\| Html5Eq(zName,"input") ){` |
|      49 | 2479 | `		Html5PopTo(p,"select");` |
|      49 | 2480 | `		return bSelect;` |
|       - | 2481 | `	}` |
|     531 | 2482 | `	if( Html5Eq(zName,"hr") \|\| Html5Eq(zName,"optgroup") ){` |
|     117 | 2483 | `		xmlNodePtr pTop = Html5Top(p);` |
|     117 | 2484 | `		if( pTop && Html5Eq((const char *)pTop->name,"option") ){` |
|      37 | 2485 | `			Html5Pop(p);` |
|      37 | 2486 | `			pTop = Html5Top(p);` |
|      18 | 2487 | `		}` |
|     117 | 2488 | `		if( pTop && Html5Eq((const char *)pTop->name,"optgroup") ){` |
|      13 | 2489 | `			Html5Pop(p);` |
|       6 | 2490 | `		}` |
|      58 | 2491 | `	}` |
|     531 | 2492 | `	return 0;` |
|    6899 | 2493 | `}` |
|       - | 2494 | ``/* The `in body` start-tag rules that are about what is ALREADY open. */`` |
|   13702 | 2495 | `static void Html5BodyImplied(html5_parser *p,const char *zName)` |
|       1 | 2496 | `{` |
|       - | 2497 | `	/*` |
|       - | 2498 | ``	 * An open `<select>` answers for everything inside it, so none of the`` |
|       - | 2499 | ``	 * implied closes below may reach past it: a `<p>` or an `<li>` the source`` |
|       - | 2500 | `	 * opened OUTSIDE the select stays open, and the tag that would have` |
|       - | 2501 | `` 	 * closed it is inserted where it stands instead.  Only the `<option>` `` |
|       - | 2502 | `	 * rule, which never looks further than the current node, still applies.` |
|       - | 2503 | `	 */` |
|   13703 | 2504 | `	int bSelect = Html5InSelect(p);` |
|       - | 2505 | `	/*` |
|       - | 2506 | ``	 * The `<p>` being closed need not be the innermost open element: a`` |
|       - | 2507 | `	 * formatting element left open inside it sits above it now, and the spec` |
|       - | 2508 | `	 * closes the p under it rather than giving up.` |
|       - | 2509 | `	 */` |
|   13702 | 2510 | `	if( !bSelect && HTML5_IN(azHtml5ClosesP,zName)` |
|   12690 | 2511 | `	 && !(p->bQuirks && Html5Eq(zName,"table"))` |
|   12115 | 2512 | `	 && Html5NameInScopeEx(p,"p",TRUE) ){` |
|      25 | 2513 | `		Html5PopTo(p,"p");` |
|      12 | 2514 | `	}` |
|   13441 | 2515 | `	if( !bSelect && Html5Eq(zName,"li") ){` |
|      51 | 2516 | `		Html5CloseListItem(p,"li","li");` |
|      25 | 2517 | `	}` |
|   13441 | 2518 | `	if( !bSelect && (Html5Eq(zName,"dd") \|\| Html5Eq(zName,"dt")) ){` |
|      11 | 2519 | `		Html5CloseListItem(p,"dd","dt");` |
|       5 | 2520 | `	}` |
|   13441 | 2521 | `	if( Html5Eq(zName,"option") && Html5OpenDepth(p,"option") == 0 ){` |
|      13 | 2522 | `		Html5Pop(p);` |
|       6 | 2523 | `	}` |
|   13440 | 2524 | `	if( !bSelect && zName[0] == 'h' && zName[1] >= '1' && zName[1] <= '6'` |
|      47 | 2525 | `	 && zName[2] == 0 ){` |
|      43 | 2526 | `		xmlNodePtr pTop = Html5Top(p);` |
|      42 | 2527 | `		if( pTop && pTop->name[0] == 'h' && pTop->name[1] >= '1'` |
|       1 | 2528 | `		 && pTop->name[1] <= '6' && pTop->name[2] == 0 ){` |
|     ! 0 | 2529 | `			Html5Pop(p);` |
|     ! 0 | 2530 | `		}` |
|      21 | 2531 | `	}` |
|   13441 | 2532 | `}` |
|       - | 2533 | `/*` |
|       - | 2534 | ` * The table implications, which is all the four table insertion modes decide.` |
|       - | 2535 | `` * A `<tr>` needs a row-group above it and a `<td>` needs a row; the stack says`` |
|       - | 2536 | ` * which of them the source already spelled.` |
|       - | 2537 | ` */` |
|   13702 | 2538 | `static void Html5TableImplied(html5_parser *p,const char *zName)` |
|       1 | 2539 | `{` |
|   13703 | 2540 | `	int bCell = Html5Eq(zName,"td") \|\| Html5Eq(zName,"th");` |
|   13703 | 2541 | `	int bRow = Html5Eq(zName,"tr");` |
|   27394 | 2542 | `	int bGroup = Html5Eq(zName,"tbody") \|\| Html5Eq(zName,"thead")` |
|   20549 | 2543 | `		\|\| Html5Eq(zName,"tfoot");` |
|       - | 2544 | `	{` |
|       - | 2545 | `		/*` |
|       - | 2546 | ``		 * The stack is cleared back to a `table`, a `template` or the root, so`` |
|       - | 2547 | `		 * a template written between the tag and the table under it IS the` |
|       - | 2548 | `		 * context: the row belongs to the template, no row group is implied` |
|       - | 2549 | `		 * for it, and the table below never sees it.` |
|       - | 2550 | `		 */` |
|   13703 | 2551 | `		xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|   13703 | 2552 | `		sxu32 n = SySetUsed(&p->sOpen);` |
|   13703 | 2553 | `		int bTable = 0;` |
|   41873 | 2554 | `		while( n-- > 0 ){` |
|   28583 | 2555 | `			if( Html5Eq((const char *)apStack[n]->name,"template") ){` |
|      27 | 2556 | `				return;` |
|       - | 2557 | `			}` |
|   28557 | 2558 | `			if( Html5Eq((const char *)apStack[n]->name,"table") ){` |
|     387 | 2559 | `				bTable = 1;` |
|     387 | 2560 | `				break;` |
|       - | 2561 | `			}` |
|       1 | 2562 | `		}` |
|   13677 | 2563 | `		if( !bTable ){` |
|   13291 | 2564 | `			return;` |
|       - | 2565 | `		}` |
|       - | 2566 | `	}` |
|     387 | 2567 | `	if( Html5Eq(zName,"col") ){` |
|       - | 2568 | ``		/* A `<col>` needs a column group the way a `<td>` needs a row. */`` |
|      27 | 2569 | `		if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,"colgroup") ){` |
|      11 | 2570 | `			return;` |
|       - | 2571 | `		}` |
|      25 | 2572 | `		while( Html5Top(p) && !Html5Eq((const char *)Html5Top(p)->name,"table") ){` |
|       9 | 2573 | `			Html5Pop(p);` |
|       1 | 2574 | `		}` |
|      17 | 2575 | `		if( Html5Top(p) ){` |
|      17 | 2576 | `			xmlNodePtr pGrp = Html5NewElem(p,"colgroup");` |
|      17 | 2577 | `			if( pGrp ){` |
|      17 | 2578 | `				Html5Push(p,pGrp);` |
|       8 | 2579 | `			}` |
|       8 | 2580 | `		}` |
|      17 | 2581 | `		return;` |
|       - | 2582 | `	}` |
|     361 | 2583 | `	if( bGroup \|\| Html5Eq(zName,"caption") \|\| Html5Eq(zName,"colgroup") ){` |
|     111 | 2584 | `		while( Html5Top(p) && !Html5Eq((const char *)Html5Top(p)->name,"table") ){` |
|      41 | 2585 | `			Html5Pop(p);` |
|       1 | 2586 | `		}` |
|      71 | 2587 | `		return;` |
|       - | 2588 | `	}` |
|     291 | 2589 | `	if( bRow \|\| bCell ){` |
|       - | 2590 | `		/*` |
|       - | 2591 | ``		 * `Clear the stack back to a table context`, which is also what`` |
|       - | 2592 | `		 * closes a cell beside an open one.  Whatever was fostered out of the` |
|       - | 2593 | `		 * table is still stacked ABOVE it, and a row or a cell belongs to the` |
|       - | 2594 | `		 * table under all of it rather than to the content that left.` |
|       - | 2595 | `		 */` |
|     179 | 2596 | `		while( Html5Top(p) ){` |
|     179 | 2597 | `			const char *zTop = (const char *)Html5Top(p)->name;` |
|     178 | 2598 | `			if( Html5Eq(zTop,"table") \|\| Html5Eq(zTop,"tbody")` |
|      96 | 2599 | `			 \|\| Html5Eq(zTop,"thead") \|\| Html5Eq(zTop,"tfoot")` |
|      91 | 2600 | `			 \|\| Html5Eq(zTop,"html") ){` |
|      45 | 2601 | `				break;` |
|       - | 2602 | `			}` |
|      91 | 2603 | `			if( bCell && Html5Eq(zTop,"tr") ){` |
|      67 | 2604 | `				break;` |
|       - | 2605 | `			}` |
|      25 | 2606 | `			Html5Pop(p);` |
|       1 | 2607 | `		}` |
|     155 | 2608 | `		if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,"table") ){` |
|      79 | 2609 | `			xmlNodePtr pGrp = Html5NewElem(p,"tbody");` |
|      79 | 2610 | `			if( pGrp ){` |
|      79 | 2611 | `				Html5Push(p,pGrp);` |
|      39 | 2612 | `			}` |
|      39 | 2613 | `		}` |
|     154 | 2614 | `		if( bCell && Html5Top(p)` |
|      85 | 2615 | `		 && !Html5Eq((const char *)Html5Top(p)->name,"tr") ){` |
|      19 | 2616 | `			xmlNodePtr pRow = Html5NewElem(p,"tr");` |
|      19 | 2617 | `			if( pRow ){` |
|      19 | 2618 | `				Html5Push(p,pRow);` |
|       9 | 2619 | `			}` |
|       9 | 2620 | `		}` |
|      77 | 2621 | `	}` |
|    6852 | 2622 | `}` |
|       - | 2623 | `/*` |
|       - | 2624 | `` * A column group holds nothing but `<col>`, so anything else written while one`` |
|       - | 2625 | ` * is open closes it first and is then reconsidered against the table under it` |
|       - | 2626 | ` * -- usually to be fostered straight back out.  Whitespace stays, the way it` |
|       - | 2627 | ` * does in the table itself.` |
|       - | 2628 | ` */` |
|   39710 | 2629 | `static void Html5ColgroupImplied(html5_parser *p,const char *zName)` |
|       1 | 2630 | `{` |
|   39711 | 2631 | `	xmlNodePtr pTop = Html5Top(p);` |
|   39711 | 2632 | `	if( pTop == 0 \|\| !Html5Eq((const char *)pTop->name,"colgroup") ){` |
|   39679 | 2633 | `		return;` |
|       - | 2634 | `	}` |
|      33 | 2635 | `	if( zName && (Html5Eq(zName,"col") \|\| Html5Eq(zName,"template")) ){` |
|      11 | 2636 | `		return;` |
|       - | 2637 | `	}` |
|      23 | 2638 | `	Html5Pop(p);` |
|   19856 | 2639 | `}` |
|       - | 2640 | `/* The generic end tag: pop to it, or ignore it if it is not open. */` |
|   12068 | 2641 | `static void Html5EndTag(html5_parser *p,const char *zName)` |
|       1 | 2642 | `{` |
|   12069 | 2643 | `	if( HTML5_IN(azHtml5Void,zName) ){` |
|      11 | 2644 | `		return;` |
|       - | 2645 | `	}` |
|   12059 | 2646 | `	if( Html5OpenDepth(p,zName) < 0 ){` |
|     137 | 2647 | `		return;` |
|       - | 2648 | `	}` |
|   11923 | 2649 | `	Html5PopTo(p,zName);` |
|    6035 | 2650 | `}` |
|       - | 2651 | ``/* Does `pParent` already hold an element of this name? */`` |
|   13362 | 2652 | `static xmlNodePtr Html5ChildNamed(xmlNodePtr pParent,const char *zName)` |
|       1 | 2653 | `{` |
|       - | 2654 | `	xmlNodePtr pCur;` |
|   26837 | 2655 | `	for( pCur = pParent ? pParent->children : 0 ; pCur ; pCur = pCur->next ){` |
|   26252 | 2656 | `		if( pCur->type == XML_ELEMENT_NODE` |
|   26244 | 2657 | `		 && Html5Eq((const char *)pCur->name,zName) ){` |
|   12779 | 2658 | `			return pCur;` |
|       - | 2659 | `		}` |
|    6738 | 2660 | `	}` |
|     585 | 2661 | `	return 0;` |
|    6682 | 2662 | `}` |
|   13026 | 2663 | `static void Html5OpenHtml(html5_parser *p,int bWithAttrs)` |
|       1 | 2664 | `{` |
|       - | 2665 | `	xmlNodePtr pHtml;` |
|   13027 | 2666 | `	if( p->iFlags & HTML5_NOIMPLIED ){` |
|       3 | 2667 | `		p->iMode = HTML5_M_IN_BODY;` |
|       3 | 2668 | `		return;` |
|       - | 2669 | `	}` |
|   13025 | 2670 | `	pHtml = Html5NewElem(p,"html");` |
|   13025 | 2671 | `	if( pHtml == 0 ){` |
|     ! 0 | 2672 | `		return;` |
|       - | 2673 | `	}` |
|   13025 | 2674 | `	if( bWithAttrs ){` |
|     395 | 2675 | `		Html5AddAttrs(p,pHtml);` |
|     197 | 2676 | `	}` |
|   13025 | 2677 | `	xmlDocSetRootElement(p->pDoc,pHtml);` |
|   13025 | 2678 | `	p->pHtml = pHtml;` |
|   13025 | 2679 | `	Html5Push(p,pHtml);` |
|   13025 | 2680 | `	p->iMode = HTML5_M_BEFORE_HEAD;` |
|    6514 | 2681 | `}` |
|   13024 | 2682 | `static void Html5OpenHead(html5_parser *p,int bWithAttrs)` |
|       1 | 2683 | `{` |
|       - | 2684 | `	xmlNodePtr pHead;` |
|   13025 | 2685 | `	if( p->iFlags & HTML5_NOIMPLIED ){` |
|     ! 0 | 2686 | `		p->iMode = HTML5_M_IN_BODY;` |
|     ! 0 | 2687 | `		return;` |
|       - | 2688 | `	}` |
|   13025 | 2689 | `	pHead = Html5NewElem(p,"head");` |
|   13025 | 2690 | `	if( pHead == 0 ){` |
|     ! 0 | 2691 | `		return;` |
|       - | 2692 | `	}` |
|   13025 | 2693 | `	if( bWithAttrs ){` |
|     179 | 2694 | `		Html5AddAttrs(p,pHead);` |
|      89 | 2695 | `	}` |
|   13025 | 2696 | `	p->pHead = pHead;` |
|   13025 | 2697 | `	Html5Push(p,pHead);` |
|   13025 | 2698 | `	p->iMode = HTML5_M_IN_HEAD;` |
|    6513 | 2699 | `}` |
|   12952 | 2700 | `static void Html5OpenBody(html5_parser *p,int bWithAttrs)` |
|       1 | 2701 | `{` |
|       - | 2702 | `	xmlNodePtr pBody;` |
|   12953 | 2703 | `	if( p->iFlags & HTML5_NOIMPLIED ){` |
|     ! 0 | 2704 | `		p->iMode = HTML5_M_IN_BODY;` |
|     ! 0 | 2705 | `		return;` |
|       - | 2706 | `	}` |
|   12953 | 2707 | `	pBody = Html5NewElem(p,"body");` |
|   12953 | 2708 | `	if( pBody == 0 ){` |
|     ! 0 | 2709 | `		return;` |
|       - | 2710 | `	}` |
|   12953 | 2711 | `	if( bWithAttrs ){` |
|       - | 2712 | `		/*` |
|       - | 2713 | `		 * A body the SOURCE spelled clears the frameset-ok flag; one the` |
|       - | 2714 | `		 * parser minted because content needed somewhere to go does not, and` |
|       - | 2715 | `` 		 * that is the whole difference between a `<frameset>` after `<body>` `` |
|       - | 2716 | ``		 * being dropped and one after `<p>` replacing the tree.`` |
|       - | 2717 | `		 */` |
|     653 | 2718 | `		Html5AddAttrs(p,pBody);` |
|     653 | 2719 | `		p->bFramesetOk = 0;` |
|     326 | 2720 | `	}` |
|   12953 | 2721 | `	Html5Push(p,pBody);` |
|   12953 | 2722 | `	p->iMode = HTML5_M_IN_BODY;` |
|    6477 | 2723 | `}` |
|       - | 2724 | `/*` |
|       - | 2725 | ` * One token through the insertion modes.  A mode that cannot take the token` |
|       - | 2726 | ` * mints what the spec says is missing and asks to be handed it again, which is` |
|       - | 2727 | `` * the `bAgain` answer.`` |
|       - | 2728 | ` */` |
|       - | 2729 | `/*` |
|       - | 2730 | ` * Does this doctype put the document in quirks mode?  The identifiers are` |
|       - | 2731 | ` * compared ASCII case-insensitively, which is why the tables hold them folded` |
|       - | 2732 | `` * and this walks the source spelling through `Html5Lower` rather than folding`` |
|       - | 2733 | ` * a copy.` |
|       - | 2734 | ` */` |
|   32130 | 2735 | `static int Html5LowerPrefix(const char *zStr,const char *zPfx)` |
|       1 | 2736 | `{` |
|       - | 2737 | `	sxu32 i;` |
|   35219 | 2738 | `	for( i = 0 ; zPfx[i] != 0 ; ++i ){` |
|   35211 | 2739 | `		if( zStr[i] == 0 \|\| Html5Lower((unsigned char)zStr[i])` |
|    3782 | 2740 | `		 != (unsigned char)zPfx[i] ){` |
|   32123 | 2741 | `			return 0;` |
|       - | 2742 | `		}` |
|    1545 | 2743 | `	}` |
|       9 | 2744 | `	return 1;` |
|   16066 | 2745 | `}` |
|    1630 | 2746 | `static int Html5LowerSame(const char *zStr,const char *zWord)` |
|       1 | 2747 | `{` |
|    1631 | 2748 | `	return Html5LowerPrefix(zStr,zWord) && zStr[SyStrlen(zWord)] == 0;` |
|       1 | 2749 | `}` |
|     558 | 2750 | `static int Html5DoctypeQuirks(html5_parser *p,const char *zName,` |
|       - | 2751 | `	const char *zPub,const char *zSys)` |
|       1 | 2752 | `{` |
|       - | 2753 | `	int i;` |
|     559 | 2754 | `	if( p->bForceQuirks \|\| !Html5Eq(zName,"html") ){` |
|      19 | 2755 | `		return 1;` |
|       - | 2756 | `	}` |
|     541 | 2757 | `	if( p->bDocSys && Html5LowerSame(zSys,HTML5_QUIRKS_SYS) ){` |
|       3 | 2758 | `		return 1;` |
|       - | 2759 | `	}` |
|    2151 | 2760 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(azHtml5QuirksPubEq) ; ++i ){` |
|    1615 | 2761 | `		if( Html5LowerSame(zPub,azHtml5QuirksPubEq[i]) ){` |
|       3 | 2762 | `			return 1;` |
|       - | 2763 | `		}` |
|     807 | 2764 | `	}` |
|   29995 | 2765 | `	for( i = 0 ; i < (int)SX_ARRAYSIZE(azHtml5QuirksPubPfx) ; ++i ){` |
|   29461 | 2766 | `		if( Html5LowerPrefix(zPub,azHtml5QuirksPubPfx[i]) ){` |
|       3 | 2767 | `			return 1;` |
|       - | 2768 | `		}` |
|   14730 | 2769 | `	}` |
|     535 | 2770 | `	if( !p->bDocSys ){` |
|    1559 | 2771 | `		for( i = 0 ; i < (int)SX_ARRAYSIZE(azHtml5QuirksPubNoSys) ; ++i ){` |
|    1041 | 2772 | `			if( Html5LowerPrefix(zPub,azHtml5QuirksPubNoSys[i]) ){` |
|       3 | 2773 | `				return 1;` |
|       - | 2774 | `			}` |
|     520 | 2775 | `		}` |
|     259 | 2776 | `	}` |
|     533 | 2777 | `	return 0;` |
|     280 | 2778 | `}` |
|  106446 | 2779 | `static int Html5Dispatch(html5_parser *p)` |
|       1 | 2780 | `{` |
|  106447 | 2781 | `	const char *zName = Html5TokName(p);` |
|       - | 2782 | `	/*` |
|       - | 2783 | ``	 * The spec's `text` insertion mode: a `<title>` or a `<script>` armed the`` |
|       - | 2784 | `	 * tokenizer, so what came back is that element's content and goes into it` |
|       - | 2785 | `	 * whatever mode the tree is otherwise in.  Without this the text walks the` |
|       - | 2786 | `	 * modes, pops the element that asked for it and lands in the body.` |
|       - | 2787 | `	 */` |
|  106447 | 2788 | `	if( p->bTextTok ){` |
|     197 | 2789 | `		Html5InsertText(p);` |
|     197 | 2790 | `		return 0;` |
|       - | 2791 | `	}` |
|       - | 2792 | `	/*` |
|       - | 2793 | `	 * Foreign content is decided before the insertion mode, not inside it: the` |
|       - | 2794 | ``	 * mode is still `in body` all the way through an `<svg>` subtree, and it is`` |
|       - | 2795 | `	 * the current node's NAMESPACE that says which set of rules this token` |
|       - | 2796 | `	 * meets.` |
|       - | 2797 | `	 */` |
|       - | 2798 | `	/*` |
|       - | 2799 | `	 * ...unless the foreign end-tag walk has just said so.  Reaching an HTML` |
|       - | 2800 | `	 * element on the way out of a foreign subtree hands THIS token to the` |
|       - | 2801 | `	 * HTML rules without popping anything, so the namespace question has to` |
|       - | 2802 | `	 * be skipped for one dispatch -- asking it again would send the token` |
|       - | 2803 | `	 * straight back to the walk it just came out of, which is what used to` |
|       - | 2804 | `	 * spin the reprocess guard and drop the token.` |
|       - | 2805 | `	 */` |
|  106251 | 2806 | `	if( p->bHtmlRules ){` |
|      11 | 2807 | `		p->bHtmlRules = 0;` |
|  106246 | 2808 | `	}else if( Html5UseForeign(p) ){` |
|     153 | 2809 | `		return Html5ForeignDispatch(p);` |
|       - | 2810 | `	}` |
|  106099 | 2811 | `	switch( p->iMode ){` |
|    6636 | 2812 | `	case HTML5_M_INITIAL:` |
|   13273 | 2813 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     559 | 2814 | `			const char *zPub = (const char *)SyBlobData(&p->sPubId);` |
|     559 | 2815 | `			const char *zSys = (const char *)SyBlobData(&p->sSysId);` |
|       - | 2816 | `			/*` |
|       - | 2817 | `			 * php keeps both identifiers on the node, and an empty NAME stays` |
|       - | 2818 | ``			 * empty there rather than becoming `html`: `<!DOCTYPE>` answers`` |
|       - | 2819 | ``			 * `$doctype->name === ''` and serializes as `<!DOCTYPE >`.`` |
|       - | 2820 | `			 */` |
|    1117 | 2821 | `			xmlCreateIntSubset(p->pDoc,(const xmlChar *)zName,` |
|     558 | 2822 | `				zPub[0] ? (const xmlChar *)zPub : 0,` |
|     558 | 2823 | `				p->bDocSys ? (const xmlChar *)zSys : 0);` |
|     559 | 2824 | `			p->bQuirks = Html5DoctypeQuirks(p,zName,zPub,zSys);` |
|     559 | 2825 | `			p->iMode = HTML5_M_BEFORE_HTML;` |
|     559 | 2826 | `			return 0;` |
|       - | 2827 | `		}` |
|   12715 | 2828 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|     275 | 2829 | `			Html5InsertComment(p,(xmlNodePtr)p->pDoc);` |
|     275 | 2830 | `			return 0;` |
|       - | 2831 | `		}` |
|   12441 | 2832 | `		if( p->iTok == HTML5_TOK_TEXT && Html5TextIsIgnSpace(p) ){` |
|       9 | 2833 | `			return 0;` |
|       - | 2834 | `		}` |
|       - | 2835 | `		/*` |
|       - | 2836 | `		 * A document that states no doctype is a parse error at the first` |
|       - | 2837 | `		 * token that is not one -- but only a TAG is reported: character data` |
|       - | 2838 | `		 * reaching this mode is quietly the anything-else branch.` |
|       - | 2839 | `		 */` |
|   12432 | 2840 | `		if( (p->iTok == HTML5_TOK_START \|\| p->iTok == HTML5_TOK_END)` |
|   12404 | 2841 | `		 && (p->iFlags & HTML5_NOIMPLIED) == 0 ){` |
|   18559 | 2842 | `			Html5Err(p,"tree","unexpected-token-in-initial-mode",` |
|    6186 | 2843 | `				p->iTokLine,p->iTokCol,p->iTokCol2);` |
|    6186 | 2844 | `		}` |
|   12433 | 2845 | `		p->iMode = HTML5_M_BEFORE_HTML;` |
|   12433 | 2846 | `		return 1;` |
|    6500 | 2847 | `	case HTML5_M_BEFORE_HTML:` |
|   13001 | 2848 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 2849 | `			return 0;` |
|       - | 2850 | `		}` |
|   13001 | 2851 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 2852 | `			Html5InsertComment(p,(xmlNodePtr)p->pDoc);` |
|       3 | 2853 | `			return 0;` |
|       - | 2854 | `		}` |
|   12999 | 2855 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|      77 | 2856 | `			if( Html5TextIsIgnSpace(p) ){` |
|       5 | 2857 | `				return 0;` |
|       - | 2858 | `			}` |
|      73 | 2859 | `			Html5TrimLeadingSpace(p);` |
|      36 | 2860 | `		}` |
|   12995 | 2861 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){` |
|     395 | 2862 | `			Html5OpenHtml(p,TRUE);` |
|     395 | 2863 | `			return 0;` |
|       - | 2864 | `		}` |
|   12600 | 2865 | `		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"head")` |
|       8 | 2866 | `		 && !Html5Eq(zName,"body") && !Html5Eq(zName,"html")` |
|       9 | 2867 | `		 && !Html5Eq(zName,"br") && (p->iFlags & HTML5_NOIMPLIED) == 0 ){` |
|      10 | 2868 | `			Html5Err(p,"tree","unexpected-closed-token-in-before-html-mode",` |
|       3 | 2869 | `				p->iTokLine,p->iTokCol,p->iTokCol2);` |
|       7 | 2870 | `			return 0;` |
|       - | 2871 | `		}` |
|   12595 | 2872 | `		Html5OpenHtml(p,FALSE);` |
|   12595 | 2873 | `		return 1;` |
|    6497 | 2874 | `	case HTML5_M_BEFORE_HEAD:` |
|   12995 | 2875 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 2876 | `			return 0;` |
|       - | 2877 | `		}` |
|   12995 | 2878 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 2879 | `			Html5InsertComment(p,Html5Target(p));` |
|       3 | 2880 | `			return 0;` |
|       - | 2881 | `		}` |
|   12993 | 2882 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|      91 | 2883 | `			if( Html5TextIsIgnSpace(p) ){` |
|       7 | 2884 | `				return 0;` |
|       - | 2885 | `			}` |
|      85 | 2886 | `			Html5TrimLeadingSpace(p);` |
|      42 | 2887 | `		}` |
|   12987 | 2888 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"head") ){` |
|     179 | 2889 | `			Html5OpenHead(p,TRUE);` |
|     179 | 2890 | `			return 0;` |
|       - | 2891 | `		}` |
|   12809 | 2892 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){` |
|     ! 0 | 2893 | `			return 0;` |
|       - | 2894 | `		}` |
|       - | 2895 | `		/*` |
|       - | 2896 | ``		 * The same four names `before html` lets through, dropped here for`` |
|       - | 2897 | `		 * the same reason and under php's own spelling of the diagnostic --` |
|       - | 2898 | `		 * which is not the one the neighbouring modes use.  Nothing depended` |
|       - | 2899 | `		 * on this screen while a dropped end tag did nothing anyway; a` |
|       - | 2900 | ``		 * `</p>` now MINTS an element one mode further on, so the tag has to`` |
|       - | 2901 | `		 * stop being handed onward.` |
|       - | 2902 | `		 */` |
|   12808 | 2903 | `		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"head")` |
|       2 | 2904 | `		 && !Html5Eq(zName,"body") && !Html5Eq(zName,"html")` |
|       3 | 2905 | `		 && !Html5Eq(zName,"br") && (p->iFlags & HTML5_NOIMPLIED) == 0 ){` |
|     ! 0 | 2906 | `			Html5Err(p,"tree","unexpected-closed_token-in-before-head-mode",` |
|     ! 0 | 2907 | `				p->iTokLine,p->iTokCol,p->iTokCol2);` |
|     ! 0 | 2908 | `			return 0;` |
|       - | 2909 | `		}` |
|   12809 | 2910 | `		Html5OpenHead(p,FALSE);` |
|   12809 | 2911 | `		return 1;` |
|    6622 | 2912 | `	case HTML5_M_IN_HEAD:` |
|   13245 | 2913 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 2914 | `			return 0;` |
|       - | 2915 | `		}` |
|   13245 | 2916 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 2917 | `			Html5InsertComment(p,Html5Target(p));` |
|       3 | 2918 | `			return 0;` |
|       - | 2919 | `		}` |
|   13243 | 2920 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|     141 | 2921 | `			if( Html5TextIsIgnSpace(p) ){` |
|       9 | 2922 | `				Html5InsertText(p);` |
|       9 | 2923 | `				return 0;` |
|       - | 2924 | `			}` |
|     133 | 2925 | `			Html5SplitLeadingSpace(p);` |
|      66 | 2926 | `		}` |
|   13235 | 2927 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){` |
|     ! 0 | 2928 | `			return 0;` |
|       - | 2929 | `		}` |
|   13235 | 2930 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){` |
|      25 | 2931 | `			Html5TemplateStart(p);` |
|      25 | 2932 | `			return 0;` |
|       - | 2933 | `		}` |
|   13211 | 2934 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){` |
|       3 | 2935 | `			Html5TemplateEnd(p);` |
|       3 | 2936 | `			return 0;` |
|       - | 2937 | `		}` |
|       - | 2938 | `		/*` |
|       - | 2939 | ``		 * With scripting disabled a head `<noscript>` holds MARKUP, so it`` |
|       - | 2940 | `		 * gets a mode of its own rather than the RAWTEXT arming: the head` |
|       - | 2941 | `		 * elements it accepts stay inside it and everything else closes it.` |
|       - | 2942 | `		 */` |
|   13209 | 2943 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noscript") ){` |
|      65 | 2944 | `			Html5InsertStart(p);` |
|      65 | 2945 | `			p->iMode = HTML5_M_IN_HEAD_NOSCRIPT;` |
|      65 | 2946 | `			return 0;` |
|       - | 2947 | `		}` |
|   13145 | 2948 | `		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName) ){` |
|     235 | 2949 | `			Html5InsertStart(p);` |
|     235 | 2950 | `			return 0;` |
|       - | 2951 | `		}` |
|   12911 | 2952 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"head") ){` |
|      83 | 2953 | `			Html5Pop(p);` |
|      83 | 2954 | `			p->iMode = HTML5_M_AFTER_HEAD;` |
|      83 | 2955 | `			return 0;` |
|       - | 2956 | `		}` |
|       - | 2957 | ``		/* `</title>` closes the title, not the head that holds it. */`` |
|   12828 | 2958 | `		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"html")` |
|     138 | 2959 | `		 && Html5OpenDepth(p,zName) >= 0` |
|     132 | 2960 | `		 && (p->pHead == 0 \|\| Html5Top(p) != p->pHead) ){` |
|     125 | 2961 | `			Html5EndTag(p,zName);` |
|     125 | 2962 | `			return 0;` |
|       - | 2963 | `		}` |
|   12705 | 2964 | `		Html5Pop(p);` |
|   12705 | 2965 | `		p->iMode = HTML5_M_AFTER_HEAD;` |
|   12705 | 2966 | `		return 1;` |
|      53 | 2967 | `	case HTML5_M_IN_HEAD_NOSCRIPT:` |
|     107 | 2968 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 2969 | `			return 0;` |
|       - | 2970 | `		}` |
|     107 | 2971 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noscript") ){` |
|      19 | 2972 | `			Html5Pop(p);` |
|      19 | 2973 | `			p->iMode = HTML5_M_IN_HEAD;` |
|      19 | 2974 | `			return 0;` |
|       - | 2975 | `		}` |
|      89 | 2976 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 2977 | `			Html5InsertComment(p,Html5Target(p));` |
|       3 | 2978 | `			return 0;` |
|       - | 2979 | `		}` |
|      87 | 2980 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|      33 | 2981 | `			if( Html5TextIsIgnSpace(p) ){` |
|       9 | 2982 | `				Html5InsertText(p);` |
|       9 | 2983 | `				return 0;` |
|       - | 2984 | `			}` |
|      25 | 2985 | `			Html5SplitLeadingSpace(p);` |
|      12 | 2986 | `		}` |
|      79 | 2987 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){` |
|       - | 2988 | ``			/* `in body` rules, which for an `<html>` already open is the`` |
|       - | 2989 | `			 * attributes it did not have yet. */` |
|       3 | 2990 | `			if( p->pHtml ){` |
|       3 | 2991 | `				Html5AddAttrs(p,p->pHtml);` |
|       1 | 2992 | `			}` |
|       3 | 2993 | `			return 0;` |
|       - | 2994 | `		}` |
|      76 | 2995 | `		if( p->iTok == HTML5_TOK_START` |
|      59 | 2996 | `		 && HTML5_IN(azHtml5HeadNoscript,zName) ){` |
|      25 | 2997 | `			Html5InsertStart(p);` |
|      25 | 2998 | `			return 0;` |
|       - | 2999 | `		}` |
|       - | 3000 | `		/*` |
|       - | 3001 | ``		 * `</style>` closes the style this mode opened, not the noscript`` |
|       - | 3002 | ``		 * holding it -- the same reading `in head` gives its own end tags,`` |
|       - | 3003 | `		 * and without it the close walked out of the head and minted a` |
|       - | 3004 | ``		 * second `<body>` inside it.`` |
|       - | 3005 | `		 */` |
|      52 | 3006 | `		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"html")` |
|      12 | 3007 | `		 && Html5OpenDepth(p,zName) >= 0 && Html5Top(p)` |
|      11 | 3008 | `		 && !Html5Eq((const char *)Html5Top(p)->name,"noscript") ){` |
|       9 | 3009 | `			Html5EndTag(p,zName);` |
|       9 | 3010 | `			return 0;` |
|       - | 3011 | `		}` |
|       - | 3012 | `		/*` |
|       - | 3013 | ``		 * A `<head>` and a nested `<noscript>` are the two start tags this`` |
|       - | 3014 | ``		 * mode DROPS; everything else -- a `<title>`, a `</br>`, any content`` |
|       - | 3015 | `		 * at all -- closes the noscript and is re-run one mode out.` |
|       - | 3016 | `		 */` |
|      44 | 3017 | `		if( p->iTok == HTML5_TOK_START` |
|      31 | 3018 | `		 && (Html5Eq(zName,"head") \|\| Html5Eq(zName,"noscript")) ){` |
|       5 | 3019 | `			return 0;` |
|       - | 3020 | `		}` |
|      41 | 3021 | `		Html5Pop(p);` |
|      41 | 3022 | `		p->iMode = HTML5_M_IN_HEAD;` |
|      41 | 3023 | `		return 1;` |
|    6397 | 3024 | `	case HTML5_M_AFTER_HEAD:` |
|   12795 | 3025 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 3026 | `			return 0;` |
|       - | 3027 | `		}` |
|   12795 | 3028 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|     ! 0 | 3029 | `			Html5InsertComment(p,Html5Target(p));` |
|     ! 0 | 3030 | `			return 0;` |
|       - | 3031 | `		}` |
|   12795 | 3032 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|     161 | 3033 | `			if( Html5TextIsIgnSpace(p) ){` |
|       5 | 3034 | `				Html5InsertText(p);` |
|       5 | 3035 | `				return 0;` |
|       - | 3036 | `			}` |
|     157 | 3037 | `			Html5SplitLeadingSpace(p);` |
|      78 | 3038 | `		}` |
|   12791 | 3039 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"body") ){` |
|     653 | 3040 | `			Html5OpenBody(p,TRUE);` |
|     653 | 3041 | `			return 0;` |
|       - | 3042 | `		}` |
|   12139 | 3043 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"frameset") ){` |
|       - | 3044 | `			/* No body is minted at all: the frameset IS the document's. */` |
|      73 | 3045 | `			Html5InsertStart(p);` |
|      73 | 3046 | `			p->iMode = HTML5_M_IN_FRAMESET;` |
|      73 | 3047 | `			return 0;` |
|       - | 3048 | `		}` |
|   12067 | 3049 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") ){` |
|     ! 0 | 3050 | `			return 0;` |
|       - | 3051 | `		}` |
|       - | 3052 | `		/*` |
|       - | 3053 | `		 * A head element spelled after the head was closed goes back INTO the` |
|       - | 3054 | `		 * head, which is the one place the tree's shape is not the source's` |
|       - | 3055 | `		 * order.` |
|       - | 3056 | `		 */` |
|   12067 | 3057 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){` |
|     ! 0 | 3058 | `			Html5TemplateStart(p);` |
|     ! 0 | 3059 | `			return 0;` |
|       - | 3060 | `		}` |
|   12067 | 3061 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){` |
|     ! 0 | 3062 | `			Html5TemplateEnd(p);` |
|     ! 0 | 3063 | `			return 0;` |
|       - | 3064 | `		}` |
|   12066 | 3065 | `		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName)` |
|    5947 | 3066 | `		 && p->pHead ){` |
|     ! 0 | 3067 | `			Html5Push(p,p->pHead);` |
|     ! 0 | 3068 | `			Html5InsertStart(p);` |
|     ! 0 | 3069 | `			if( Html5Top(p) != p->pHead ){` |
|     ! 0 | 3070 | `				Html5Pop(p);` |
|     ! 0 | 3071 | `			}` |
|     ! 0 | 3072 | `			Html5Pop(p);` |
|     ! 0 | 3073 | `			return 0;` |
|       - | 3074 | `		}` |
|       - | 3075 | `		/*` |
|       - | 3076 | ``		 * The end tags `before head` drops, minus one: the head is closed`` |
|       - | 3077 | ``		 * already, so `</head>` is dropped with the rest and only `</body>`,`` |
|       - | 3078 | ``		 * `</html>` and `</br>` are handed onward.  php spells this`` |
|       - | 3079 | `		 * diagnostic without naming the mode.  Nothing depended on the` |
|       - | 3080 | ``		 * screen while a dropped end tag did nothing anyway; a `</p>` now`` |
|       - | 3081 | ``		 * MINTS an element in `in body`, so the tag has to stop here.`` |
|       - | 3082 | `		 */` |
|   12066 | 3083 | `		if( p->iTok == HTML5_TOK_END && !Html5Eq(zName,"body")` |
|      18 | 3084 | `		 && !Html5Eq(zName,"html") && !Html5Eq(zName,"br")` |
|      15 | 3085 | `		 && (p->iFlags & HTML5_NOIMPLIED) == 0 ){` |
|      19 | 3086 | `			Html5Err(p,"tree","unexpected-closed-token",` |
|       6 | 3087 | `				p->iTokLine,p->iTokCol,p->iTokCol2);` |
|      13 | 3088 | `			return 0;` |
|       - | 3089 | `		}` |
|   12055 | 3090 | `		Html5OpenBody(p,FALSE);` |
|   12055 | 3091 | `		return 1;` |
|      26 | 3092 | `	case HTML5_M_IN_TEMPLATE:` |
|       - | 3093 | `		/*` |
|       - | 3094 | `		 * The content of a template is everything the body would take, so the` |
|       - | 3095 | `		 * spec's rule for anything that is not a head element is to hand the` |
|       - | 3096 | ``		 * mode over to `in body` and reprocess there; `</template>` is then`` |
|       - | 3097 | ``		 * reached from `in body` rather than from here.  The head elements`` |
|       - | 3098 | ``		 * keep `in head`'s treatment, which is what arms `<title>` and`` |
|       - | 3099 | ``		 * `<script>` for their raw text.`` |
|       - | 3100 | `		 */` |
|      53 | 3101 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"template") ){` |
|       3 | 3102 | `			Html5TemplateStart(p);` |
|       3 | 3103 | `			return 0;` |
|       - | 3104 | `		}` |
|      51 | 3105 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"template") ){` |
|      11 | 3106 | `			Html5TemplateEnd(p);` |
|      11 | 3107 | `			return 0;` |
|       - | 3108 | `		}` |
|      41 | 3109 | `		if( p->iTok == HTML5_TOK_START && HTML5_IN(azHtml5Head,zName) ){` |
|       5 | 3110 | `			Html5InsertStart(p);` |
|       5 | 3111 | `			return 0;` |
|       - | 3112 | `		}` |
|      37 | 3113 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 3114 | `			return 0;` |
|       - | 3115 | `		}` |
|       - | 3116 | `		/*` |
|       - | 3117 | `		 * An end tag is where the handover stops: everything but` |
|       - | 3118 | ``		 * `</template>` is DROPPED here rather than re-run `in body`.  Only`` |
|       - | 3119 | `		 * an end tag written before the template holds any content can reach` |
|       - | 3120 | ``		 * this mode at all -- a start tag latches the mode to `in body` and`` |
|       - | 3121 | `		 * that is where the closes of what it opened are read -- and for` |
|       - | 3122 | ``		 * those the two readings differ by exactly one tag: `</br>`, which`` |
|       - | 3123 | ``		 * `in body` answers with an ELEMENT.`` |
|       - | 3124 | `		 */` |
|      37 | 3125 | `		if( p->iTok == HTML5_TOK_END ){` |
|       7 | 3126 | `			return 0;` |
|       - | 3127 | `		}` |
|      31 | 3128 | `		p->iMode = HTML5_M_IN_BODY;` |
|      31 | 3129 | `		return 1;` |
|      47 | 3130 | `	case HTML5_M_AFTER_BODY:` |
|      95 | 3131 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       7 | 3132 | `			Html5InsertComment(p,p->pHtml ? p->pHtml : (xmlNodePtr)p->pDoc);` |
|       7 | 3133 | `			return 0;` |
|       - | 3134 | `		}` |
|      89 | 3135 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 3136 | `			return 0;` |
|       - | 3137 | `		}` |
|      89 | 3138 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"html") ){` |
|      71 | 3139 | `			p->iMode = HTML5_M_AFTER_AFTER_BODY;` |
|      71 | 3140 | `			return 0;` |
|       - | 3141 | `		}` |
|       - | 3142 | `		/*` |
|       - | 3143 | `		 * Whitespace here is the BODY's, not the document's.  It is handed` |
|       - | 3144 | ``		 * to the `in body` rules without latching the mode -- falling out of`` |
|       - | 3145 | `		 * this switch is how a mode borrows another's rules without becoming` |
|       - | 3146 | `		 * it -- so a comment written after it is still the html element's,` |
|       - | 3147 | ``		 * and a run reaching an open `<table>` is still the table's.`` |
|       - | 3148 | `		 */` |
|      19 | 3149 | `		if( p->iTok == HTML5_TOK_TEXT && Html5TextIsSpace(p) ){` |
|       9 | 3150 | `			break;` |
|       - | 3151 | `		}` |
|      11 | 3152 | `		p->iMode = HTML5_M_IN_BODY;` |
|      11 | 3153 | `		return 1;` |
|       4 | 3154 | `	case HTML5_M_AFTER_AFTER_BODY:` |
|       - | 3155 | ``		/* Past `</html>`: a comment is the document's, beside the root. */`` |
|       9 | 3156 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       5 | 3157 | `			Html5InsertComment(p,(xmlNodePtr)p->pDoc);` |
|       5 | 3158 | `			return 0;` |
|       - | 3159 | `		}` |
|       5 | 3160 | `		if( p->iTok == HTML5_TOK_DOCTYPE ){` |
|     ! 0 | 3161 | `			return 0;` |
|       - | 3162 | `		}` |
|       5 | 3163 | `		if( p->iTok == HTML5_TOK_TEXT && Html5TextIsSpace(p) ){` |
|       3 | 3164 | `			break;` |
|       - | 3165 | `		}` |
|       3 | 3166 | `		p->iMode = HTML5_M_IN_BODY;` |
|       3 | 3167 | `		return 1;` |
|     108 | 3168 | `	case HTML5_M_IN_FRAMESET:` |
|     217 | 3169 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|      23 | 3170 | `			Html5InsertSpaceOnly(p);` |
|      23 | 3171 | `			return 0;` |
|       - | 3172 | `		}` |
|     195 | 3173 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 3174 | `			Html5InsertComment(p,Html5Target(p));` |
|       3 | 3175 | `			return 0;` |
|       - | 3176 | `		}` |
|     193 | 3177 | `		if( p->iTok == HTML5_TOK_START ){` |
|     102 | 3178 | `			if( Html5Eq(zName,"frameset") \|\| Html5Eq(zName,"frame")` |
|      55 | 3179 | `			 \|\| Html5Eq(zName,"noframes") ){` |
|       - | 3180 | `				/*` |
|       - | 3181 | ``				 * `<frame>` is void, so `Html5InsertStart` inserts it without`` |
|       - | 3182 | ``				 * pushing it, and `<noframes>` is raw text the same way it is`` |
|       - | 3183 | `				 * in the head.` |
|       - | 3184 | `				 */` |
|      97 | 3185 | `				Html5InsertStart(p);` |
|      97 | 3186 | `				return 0;` |
|       - | 3187 | `			}` |
|       7 | 3188 | `			if( Html5Eq(zName,"html") && p->pHtml ){` |
|     ! 0 | 3189 | `				Html5AddAttrs(p,p->pHtml);` |
|     ! 0 | 3190 | `				return 0;` |
|       - | 3191 | `			}` |
|       7 | 3192 | `			return 0;` |
|       - | 3193 | `		}` |
|      91 | 3194 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes") ){` |
|       - | 3195 | `			/* The one element besides a frameset these modes leave open. */` |
|       3 | 3196 | `			if( Html5Top(p) && Html5Eq((const char *)Html5Top(p)->name,` |
|       - | 3197 | `					"noframes") ){` |
|       3 | 3198 | `				Html5Pop(p);` |
|       1 | 3199 | `			}` |
|       3 | 3200 | `			return 0;` |
|       - | 3201 | `		}` |
|      89 | 3202 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"frameset") ){` |
|       - | 3203 | `			/*` |
|       - | 3204 | ``			 * The root `<html>` is never popped by an end tag, so a stray`` |
|       - | 3205 | ``			 * `</frameset>` past the outermost one is ignored; closing the`` |
|       - | 3206 | `			 * outermost is what leaves the mode.` |
|       - | 3207 | `			 */` |
|      79 | 3208 | `			if( Html5Top(p) == p->pHtml ){` |
|     ! 0 | 3209 | `				return 0;` |
|       - | 3210 | `			}` |
|      79 | 3211 | `			Html5Pop(p);` |
|      78 | 3212 | `			if( Html5Top(p) == 0` |
|      79 | 3213 | `			 \|\| !Html5Eq((const char *)Html5Top(p)->name,"frameset") ){` |
|      77 | 3214 | `				p->iMode = HTML5_M_AFTER_FRAMESET;` |
|      38 | 3215 | `			}` |
|      79 | 3216 | `			return 0;` |
|       - | 3217 | `		}` |
|      11 | 3218 | `		return 0;` |
|      11 | 3219 | `	case HTML5_M_AFTER_FRAMESET:` |
|      23 | 3220 | `		if( p->iTok == HTML5_TOK_TEXT ){` |
|       7 | 3221 | `			Html5InsertSpaceOnly(p);` |
|       7 | 3222 | `			return 0;` |
|       - | 3223 | `		}` |
|      17 | 3224 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 3225 | `			Html5InsertComment(p,Html5Target(p));` |
|       3 | 3226 | `			return 0;` |
|       - | 3227 | `		}` |
|      15 | 3228 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noframes") ){` |
|       3 | 3229 | `			Html5InsertStart(p);` |
|       3 | 3230 | `			return 0;` |
|       - | 3231 | `		}` |
|      13 | 3232 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") && p->pHtml ){` |
|       3 | 3233 | `			Html5AddAttrs(p,p->pHtml);` |
|       3 | 3234 | `			return 0;` |
|       - | 3235 | `		}` |
|      10 | 3236 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes")` |
|       5 | 3237 | `		 && Html5Top(p)` |
|       3 | 3238 | `		 && Html5Eq((const char *)Html5Top(p)->name,"noframes") ){` |
|       3 | 3239 | `			Html5Pop(p);` |
|       3 | 3240 | `			return 0;` |
|       - | 3241 | `		}` |
|       9 | 3242 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"html") ){` |
|       5 | 3243 | `			p->iMode = HTML5_M_AFTER_AFTER_FRAMESET;` |
|       5 | 3244 | `			return 0;` |
|       - | 3245 | `		}` |
|       5 | 3246 | `		return 0;` |
|       3 | 3247 | `	case HTML5_M_AFTER_AFTER_FRAMESET:` |
|       - | 3248 | ``		/* Past `</html>`: a comment is the document's, beside the root. */`` |
|       7 | 3249 | `		if( p->iTok == HTML5_TOK_COMMENT ){` |
|       3 | 3250 | `			Html5InsertComment(p,(xmlNodePtr)p->pDoc);` |
|       3 | 3251 | `			return 0;` |
|       - | 3252 | `		}` |
|       5 | 3253 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"noframes") ){` |
|     ! 0 | 3254 | `			Html5InsertStart(p);` |
|     ! 0 | 3255 | `			return 0;` |
|       - | 3256 | `		}` |
|       5 | 3257 | `		if( p->iTok == HTML5_TOK_START && Html5Eq(zName,"html") && p->pHtml ){` |
|     ! 0 | 3258 | `			Html5AddAttrs(p,p->pHtml);` |
|     ! 0 | 3259 | `			return 0;` |
|       - | 3260 | `		}` |
|       4 | 3261 | `		if( p->iTok == HTML5_TOK_END && Html5Eq(zName,"noframes")` |
|     ! 0 | 3262 | `		 && Html5Top(p)` |
|       1 | 3263 | `		 && Html5Eq((const char *)Html5Top(p)->name,"noframes") ){` |
|     ! 0 | 3264 | `			Html5Pop(p);` |
|     ! 0 | 3265 | `			return 0;` |
|       - | 3266 | `		}` |
|       5 | 3267 | `		return 0;` |
|   20145 | 3268 | `	default:` |
|   40290 | 3269 | `		break;` |
|       - | 3270 | `	}` |
|       - | 3271 | `	/*` |
|       - | 3272 | ``	 * `in body`, and every table mode with it.  Fostering is armed per token`` |
|       - | 3273 | `	 * by the two cases that can be misplaced and is off for everything else,` |
|       - | 3274 | `	 * including the implied row groups and rows the table modes mint.` |
|       - | 3275 | `	 */` |
|   40301 | 3276 | `	p->bFoster = 0;` |
|   40301 | 3277 | `	switch( p->iTok ){` |
|    6994 | 3278 | `	case HTML5_TOK_TEXT:` |
|       - | 3279 | `		/*` |
|       - | 3280 | `		 * A run that is nothing but whitespace stays inside the table; one` |
|       - | 3281 | `		 * with any other byte in it leaves WHOLE, spaces and all.  The` |
|       - | 3282 | `		 * reconstruction runs under the same arming, so a formatting element` |
|       - | 3283 | `		 * reopened to hold the run is fostered with it.` |
|       - | 3284 | `		 */` |
|   13989 | 3285 | `		p->bFoster = !Html5TextIsSpace(p);` |
|   13989 | 3286 | `		if( p->bFoster ){` |
|       - | 3287 | `			/* Any byte that is not whitespace is content a frameset` |
|       - | 3288 | `			 * would have to throw away, so it may no longer replace it. */` |
|   13851 | 3289 | `			p->bFramesetOk = 0;` |
|   13851 | 3290 | `			Html5ColgroupImplied(p,0);` |
|    7064 | 3291 | `		}else if( !Html5TextIsIgnSpace(p) ){` |
|       - | 3292 | `			/*` |
|       - | 3293 | ``			 * A run a table keeps whole can still END an open `<colgroup>`:`` |
|       - | 3294 | `			 * that mode asks the IGNORABLE-space question, so a form feed` |
|       - | 3295 | `			 * closes it where a space does not.  What was written before the` |
|       - | 3296 | `			 * form feed is still the colgroup's, so the run splits.` |
|       - | 3297 | `			 */` |
|      81 | 3298 | `			Html5SplitLeadingSpace(p);` |
|      81 | 3299 | `			Html5ColgroupImplied(p,0);` |
|      40 | 3300 | `		}` |
|   13989 | 3301 | `		Html5Reconstruct(p);` |
|   13989 | 3302 | `		Html5InsertText(p);` |
|   13989 | 3303 | `		break;` |
|       4 | 3304 | `	case HTML5_TOK_COMMENT:` |
|       - | 3305 | `		/* A comment is the one thing a table keeps wherever it is written. */` |
|       9 | 3306 | `		Html5InsertComment(p,Html5Target(p));` |
|       9 | 3307 | `		break;` |
|       4 | 3308 | `	case HTML5_TOK_DOCTYPE:` |
|       8 | 3309 | `		break;` |
|    7052 | 3310 | `	case HTML5_TOK_START: {` |
|       - | 3311 | `		xmlNodePtr pEl;` |
|       - | 3312 | `		int bTableForm;` |
|   14105 | 3313 | `		if( Html5Eq(zName,"template") ){` |
|      15 | 3314 | `			Html5TemplateStart(p);` |
|      15 | 3315 | `			break;` |
|       - | 3316 | `		}` |
|   14091 | 3317 | `		if( Html5Eq(zName,"body") && SySetUsed(&p->sTmpl) > 0 ){` |
|       - | 3318 | ``			/* A `<body>` start tag inside a template has no body to merge`` |
|       - | 3319 | `			 * its attributes into and does not open one; it is dropped. */` |
|       3 | 3320 | `			break;` |
|       - | 3321 | `		}` |
|   14089 | 3322 | `		if( Html5Eq(zName,"frameset") ){` |
|       - | 3323 | `			/*` |
|       - | 3324 | `			 * A frameset written where content already is REPLACES the body` |
|       - | 3325 | `			 * rather than joining it -- but only while nothing has gone into` |
|       - | 3326 | `			 * the document that a frameset cannot hold.  The body is the` |
|       - | 3327 | `			 * second element on the stack; anything the source opened inside` |
|       - | 3328 | `			 * it goes away with it.` |
|       - | 3329 | `			 */` |
|      71 | 3330 | `			xmlNodePtr *apStack = (xmlNodePtr *)SySetBasePtr(&p->sOpen);` |
|       - | 3331 | `			xmlNodePtr pBody;` |
|      70 | 3332 | `			if( SySetUsed(&p->sOpen) < 2 \|\| p->bFramesetOk == 0` |
|      46 | 3333 | `			 \|\| !Html5Eq((const char *)apStack[1]->name,"body") ){` |
|      26 | 3334 | `				break;` |
|       - | 3335 | `			}` |
|      21 | 3336 | `			pBody = apStack[1];` |
|       - | 3337 | `			/*` |
|       - | 3338 | `			 * Pop first, then free: the formatting list may still name an` |
|       - | 3339 | `			 * element inside the body, and popping past its marker is what` |
|       - | 3340 | `			 * clears it before the subtree goes.` |
|       - | 3341 | `			 */` |
|      55 | 3342 | `			while( SySetUsed(&p->sOpen) > 1 ){` |
|      35 | 3343 | `				Html5Pop(p);` |
|       1 | 3344 | `			}` |
|      21 | 3345 | `			SySetTruncate(&p->sFmt,0);` |
|      21 | 3346 | `			xmlUnlinkNode(pBody);` |
|      21 | 3347 | `			xmlFreeNode(pBody);` |
|      21 | 3348 | `			Html5InsertStart(p);` |
|      21 | 3349 | `			p->iMode = HTML5_M_IN_FRAMESET;` |
|      21 | 3350 | `			break;` |
|       - | 3351 | `		}` |
|   14019 | 3352 | `		if( Html5Eq(zName,"html") \|\| Html5Eq(zName,"body") ){` |
|       - | 3353 | ``			/* A second `<body>` merges its attributes and shuts the door. */`` |
|      61 | 3354 | `			if( Html5Eq(zName,"body") ){` |
|      45 | 3355 | `				p->bFramesetOk = 0;` |
|      22 | 3356 | `			}` |
|      61 | 3357 | `			break;` |
|       - | 3358 | `		}` |
|       - | 3359 | `		/*` |
|       - | 3360 | ``		 * A `<frame>` belongs to a frameset and nowhere else: written in the`` |
|       - | 3361 | `` 		 * body -- which is where one written after an IGNORED `<frameset>` `` |
|       - | 3362 | `		 * arrives -- it is dropped rather than inserted.` |
|       - | 3363 | `		 */` |
|   13959 | 3364 | `		if( Html5Eq(zName,"frame") ){` |
|      67 | 3365 | `			break;` |
|       - | 3366 | `		}` |
|       - | 3367 | `		/*` |
|       - | 3368 | `		 * ...and the rest of the table furniture, which is dropped only when` |
|       - | 3369 | `		 * there is no table anywhere for it to belong to.` |
|       - | 3370 | `		 */` |
|   13893 | 3371 | `		if( HTML5_IN(azHtml5NoTable,zName) && !Html5TableOpen(p) ){` |
|      75 | 3372 | `			break;` |
|       - | 3373 | `		}` |
|   13818 | 3374 | `		if( HTML5_IN(azHtml5NoFrameset,zName)` |
|   13360 | 3375 | `		 \|\| (Html5Eq(zName,"input") && !Html5TableOwns(p,zName)) ){` |
|     945 | 3376 | `			p->bFramesetOk = 0;` |
|     472 | 3377 | `		}` |
|   13819 | 3378 | `		if( Html5Eq(zName,"head") ){` |
|      23 | 3379 | `			break;` |
|       - | 3380 | `		}` |
|   13797 | 3381 | `		if( Html5SelectImplied(p,zName) ){` |
|      33 | 3382 | `			break;` |
|       - | 3383 | `		}` |
|       - | 3384 | `		/*` |
|       - | 3385 | `` 		 * Two formatting elements refuse to nest in themselves: a second `<a>` `` |
|       - | 3386 | `` 		 * closes the first wherever it is on the list, and a second `<nobr>` `` |
|       - | 3387 | `		 * closes the one in scope.  Both do it through the adoption, so what` |
|       - | 3388 | `		 * was caught inside the first is carried out of it.` |
|       - | 3389 | `		 */` |
|   13765 | 3390 | `		if( Html5Eq(zName,"a") ){` |
|      13 | 3391 | `			xmlNodePtr pOpenA = Html5FmtLast(p,"a");` |
|      13 | 3392 | `			if( pOpenA ){` |
|       5 | 3393 | `				Html5Adoption(p,"a");` |
|       5 | 3394 | `				Html5FmtRemove(p,pOpenA);` |
|       5 | 3395 | `				Html5SetRemoveAt(&p->sOpen,Html5StackIndex(p,pOpenA));` |
|       3 | 3396 | `			}` |
|   13759 | 3397 | `		}else if( Html5Eq(zName,"nobr") && Html5NameInScope(p,"nobr") ){` |
|       3 | 3398 | `			Html5Adoption(p,"nobr");` |
|       1 | 3399 | `		}` |
|       - | 3400 | `		/*` |
|       - | 3401 | `		 * The two tags that switch namespace.  They reconstruct the formatting` |
|       - | 3402 | `		 * elements like any other body content and then stop being HTML.` |
|       - | 3403 | `		 */` |
|   13765 | 3404 | `		if( Html5Eq(zName,"svg") \|\| Html5Eq(zName,"math") ){` |
|      61 | 3405 | `			Html5ColgroupImplied(p,zName);` |
|      61 | 3406 | `			p->bFoster = 1;` |
|      61 | 3407 | `			Html5Reconstruct(p);` |
|      61 | 3408 | `			Html5InsertForeign(p,Html5Eq(zName,"svg")` |
|       - | 3409 | `				? HTML5_NSK_SVG : HTML5_NSK_MATH);` |
|      61 | 3410 | `			break;` |
|       - | 3411 | `		}` |
|       - | 3412 | `		/*` |
|       - | 3413 | ``		 * A `<table>` written inside one is the source forgetting the close:`` |
|       - | 3414 | `		 * the open table is closed and the new one opens beside it rather` |
|       - | 3415 | `		 * than within it.  The question is asked in SCOPE, so a table inside` |
|       - | 3416 | ``		 * a `<td>` is a real nested table and this does not fire.`` |
|       - | 3417 | `		 */` |
|   13705 | 3418 | `		if( Html5Eq(zName,"table") && Html5NameInScope(p,"table") ){` |
|       3 | 3419 | `			Html5PopTo(p,"table");` |
|       1 | 3420 | `		}` |
|   13705 | 3421 | `		Html5ColgroupImplied(p,zName);` |
|       - | 3422 | `		/*` |
|       - | 3423 | ``		 * `<image>` names no element.  The spec renames the TOKEN to `img`,`` |
|       - | 3424 | ``		 * so what lands is a VOID `<img>` carrying the token's attributes --`` |
|       - | 3425 | `		 * misnaming it also left it OPEN, swallowing the rest of the body.` |
|       - | 3426 | ``		 * The rename is an HTML rule alone: an `<image>` inside `<svg>` is an`` |
|       - | 3427 | `		 * SVG image, and foreign content is dispatched before this switch.` |
|       - | 3428 | `		 * And php reaches the rename by RE-RUNNING the renamed token, which` |
|       - | 3429 | `` 		 * a table's foster path has no re-run to hand it, so an `<image>` `` |
|       - | 3430 | `` 		 * written straight into a table is DROPPED there where the `<img>` `` |
|       - | 3431 | `` 		 * it would have become is fostered out -- the implied `</colgroup>` `` |
|       - | 3432 | `		 * above is why the question is asked here and not at the top.` |
|       - | 3433 | `		 */` |
|   13705 | 3434 | `		if( Html5Eq(zName,"image") ){` |
|      21 | 3435 | `			if( Html5InTableCtx(p) ){` |
|       3 | 3436 | `				break;` |
|       - | 3437 | `			}` |
|      19 | 3438 | `			SyBlobReset(&p->sName);` |
|      19 | 3439 | `			SyBlobAppend(&p->sName,"img",sizeof("img") - 1);` |
|      19 | 3440 | `			Html5EndStr(&p->sName);` |
|      19 | 3441 | `			zName = Html5TokName(p);` |
|       9 | 3442 | `		}` |
|   13703 | 3443 | `		p->bFoster = !Html5TableOwns(p,zName);` |
|   13703 | 3444 | `		bTableForm = Html5Eq(zName,"form") && Html5InTableCtx(p);` |
|   13703 | 3445 | `		Html5TableImplied(p,zName);` |
|   13703 | 3446 | `		Html5BodyImplied(p,zName);` |
|   13703 | 3447 | `		if( Html5Reconstructs(zName) ){` |
|    1041 | 3448 | `			Html5Reconstruct(p);` |
|     520 | 3449 | `		}` |
|   13703 | 3450 | `		pEl = Html5InsertStart(p);` |
|   13703 | 3451 | `		if( pEl && bTableForm ){` |
|       - | 3452 | `			/*` |
|       - | 3453 | ``			 * A `<form>` written straight into a table is inserted and popped`` |
|       - | 3454 | `			 * at once, so the table keeps the empty element and everything` |
|       - | 3455 | `			 * after it is fostered out rather than filling the form.` |
|       - | 3456 | `			 */` |
|       3 | 3457 | `			Html5Pop(p);` |
|       1 | 3458 | `		}` |
|   13703 | 3459 | `		if( pEl ){` |
|   13703 | 3460 | `			if( HTML5_IN(azHtml5Fmt,zName) ){` |
|     133 | 3461 | `				Html5FmtPush(p,pEl);` |
|   13637 | 3462 | `			}else if( HTML5_IN(azHtml5FmtMark,zName) ){` |
|     129 | 3463 | `				Html5FmtMarker(p);` |
|      64 | 3464 | `			}` |
|    6851 | 3465 | `		}` |
|   13703 | 3466 | `		break;` |
|       - | 3467 | `	}` |
|    6096 | 3468 | `	case HTML5_TOK_END:` |
|   12193 | 3469 | `		if( Html5Eq(zName,"template") ){` |
|      29 | 3470 | `			Html5TemplateEnd(p);` |
|      29 | 3471 | `			break;` |
|       - | 3472 | `		}` |
|       - | 3473 | `		/*` |
|       - | 3474 | `		 * Both of these only SWITCH the mode: the body element stays open,` |
|       - | 3475 | `		 * and so does everything the source left open inside it, which is` |
|       - | 3476 | `		 * where the whitespace and the stray content that follow keep` |
|       - | 3477 | `		 * landing.  Popping the body here put a document's final newline` |
|       - | 3478 | `		 * BESIDE the body instead of in it -- the shape every file ending` |
|       - | 3479 | ``		 * `</body>\n</html>` has -- and sent a `<div>` written after the`` |
|       - | 3480 | ``		 * close into the html element.  `</html>` differs by being re-run in`` |
|       - | 3481 | `		 * the mode it switches to, which is what ends the document.` |
|       - | 3482 | `		 */` |
|   12165 | 3483 | `		if( Html5Eq(zName,"body") ){` |
|      87 | 3484 | `			p->iMode = HTML5_M_AFTER_BODY;` |
|      87 | 3485 | `			break;` |
|       - | 3486 | `		}` |
|   12079 | 3487 | `		if( Html5Eq(zName,"html") ){` |
|       3 | 3488 | `			p->iMode = HTML5_M_AFTER_BODY;` |
|       3 | 3489 | `			return 1;` |
|       - | 3490 | `		}` |
|       - | 3491 | `		/*` |
|       - | 3492 | ``		 * `</br>` is the one end tag that OPENS an element.  It is re-run as a`` |
|       - | 3493 | ``		 * `<br>` START tag carrying none of the attributes it was written`` |
|       - | 3494 | ``		 * with, so everything a `<br>` start tag owes -- the reconstruction,`` |
|       - | 3495 | `		 * the frameset flag, and being FOSTERED out of a table -- is owed by` |
|       - | 3496 | `		 * this too, and asking the start tag rather than copying it is the` |
|       - | 3497 | `		 * only way that stays true.` |
|       - | 3498 | `		 */` |
|   12077 | 3499 | `		if( Html5Eq(zName,"br") ){` |
|      33 | 3500 | `			p->iTok = HTML5_TOK_START;` |
|      33 | 3501 | `			SyBlobReset(&p->sAttrBuf);` |
|      33 | 3502 | `			SySetReset(&p->sAttr);` |
|      33 | 3503 | `			return 1;` |
|       - | 3504 | `		}` |
|       - | 3505 | `		/*` |
|       - | 3506 | ``		 * `</p>` is the other one, and it opens an element only when there is`` |
|       - | 3507 | ``		 * no `<p>` in BUTTON scope to close -- which is why `</p>` written`` |
|       - | 3508 | ``		 * alone leaves `<p></p>` behind, and why one written inside a`` |
|       - | 3509 | ``		 * `<button>` mints a second paragraph rather than closing the one`` |
|       - | 3510 | ``		 * outside.  The element is asked for the same way `</br>` asks, as a`` |
|       - | 3511 | `		 * START tag stripped of the attributes the end tag was written with,` |
|       - | 3512 | `		 * so that the implied closes, the reconstruction and being FOSTERED` |
|       - | 3513 | `		 * out of a table are all owed by it too.  Closing it is owed once` |
|       - | 3514 | `		 * that whole token has been processed, not inside this dispatch: the` |
|       - | 3515 | `		 * element is not on the stack until the start tag's own path has` |
|       - | 3516 | `		 * run.` |
|       - | 3517 | `		 */` |
|   12045 | 3518 | `		if( Html5Eq(zName,"p") && !Html5NameInScopeEx(p,"p",TRUE) ){` |
|      23 | 3519 | `			p->iTok = HTML5_TOK_START;` |
|      23 | 3520 | `			SyBlobReset(&p->sAttrBuf);` |
|      23 | 3521 | `			SySetReset(&p->sAttr);` |
|      23 | 3522 | `			p->bCloseP = 1;` |
|      23 | 3523 | `			return 1;` |
|       - | 3524 | `		}` |
|   12023 | 3525 | `		if( !Html5Eq(zName,"colgroup") ){` |
|   12017 | 3526 | `			Html5ColgroupImplied(p,zName);` |
|    6008 | 3527 | `		}` |
|   12023 | 3528 | `		if( HTML5_IN(azHtml5Fmt,zName) ){` |
|      87 | 3529 | `			if( Html5Adoption(p,zName) == 0 ){` |
|       7 | 3530 | `				Html5EndTagOther(p,zName);` |
|       3 | 3531 | `			}` |
|      87 | 3532 | `			break;` |
|       - | 3533 | `		}` |
|   11937 | 3534 | `		Html5EndTag(p,zName);` |
|   11936 | 3535 | `		break;` |
|     ! 0 | 3536 | `	default:` |
|     ! 0 | 3537 | `		break;` |
|       - | 3538 | `	}` |
|   40245 | 3539 | `	return 0;` |
|   53224 | 3540 | `}` |
|       - | 3541 |  |
|       - | 3542 | `/* ------------------------------------------------------------------ *` |
|       - | 3543 | ` * The entry point` |
|       - | 3544 | ` * ------------------------------------------------------------------ */` |
|       - | 3545 |  |
|       - | 3546 | `/*` |
|       - | 3547 | `` * `HTML_NO_DEFAULT_NS` asks for a tree whose ELEMENTS carry no namespace, and`` |
|       - | 3548 | ` * that has to happen after the parse rather than during it: the foreign rules` |
|       - | 3549 | `` * are chosen by the current node's namespace, so a `<svg>` that never got one`` |
|       - | 3550 | ` * would fold its names, miss its breakout tags and never leave the subtree.` |
|       - | 3551 | ` * So the tree is built namespaced and stripped here.  An ATTRIBUTE keeps its` |
|       - | 3552 | `` * namespace -- php answers the xlink URI for an `xlink:href` either way -- and`` |
|       - | 3553 | ` * so does the declaration behind it.` |
|       - | 3554 | ` */` |
|      26 | 3555 | `static void Html5StripForeignNs(xmlNodePtr pNode)` |
|       1 | 3556 | `{` |
|       - | 3557 | `	xmlNodePtr pCur;` |
|      55 | 3558 | `	for( pCur = pNode ; pCur ; pCur = pCur->next ){` |
|       - | 3559 | `		xmlNsPtr *ppNs;` |
|      29 | 3560 | `		if( pCur->type != XML_ELEMENT_NODE ){` |
|       7 | 3561 | `			continue;` |
|       - | 3562 | `		}` |
|       - | 3563 | `		/* Children first: a declaration is only free to go once nothing` |
|       - | 3564 | `		 * underneath it still points at it. */` |
|      23 | 3565 | `		Html5StripForeignNs(pCur->children);` |
|      23 | 3566 | `		if( Html5NsKind(pCur) != HTML5_NSK_HTML ){` |
|       7 | 3567 | `			pCur->ns = 0;` |
|       3 | 3568 | `		}` |
|      23 | 3569 | `		ppNs = &pCur->nsDef;` |
|      25 | 3570 | `		while( *ppNs ){` |
|       3 | 3571 | `			xmlNsPtr pNs = *ppNs;` |
|       2 | 3572 | `			if( pNs->href` |
|       3 | 3573 | `			 && (Html5Eq((const char *)pNs->href,HTML5_SVG_NS)` |
|       1 | 3574 | `			  \|\| Html5Eq((const char *)pNs->href,HTML5_MATH_NS)) ){` |
|       3 | 3575 | `				*ppNs = pNs->next;` |
|       3 | 3576 | `				pNs->next = 0;` |
|       3 | 3577 | `				xmlFreeNs(pNs);` |
|       2 | 3578 | `			}else{` |
|     ! 0 | 3579 | `				ppNs = &pNs->next;` |
|       - | 3580 | `			}` |
|       1 | 3581 | `		}` |
|      12 | 3582 | `	}` |
|      27 | 3583 | `}` |
|       - | 3584 |  |
|       - | 3585 | `/*` |
|       - | 3586 | ` * Normalize the source the way the spec's input stream does: a CRLF pair and a` |
|       - | 3587 | ` * lone CR are both one LF, so nothing downstream -- including the line and` |
|       - | 3588 | ` * column a diagnostic prints -- has to know which spelling a file used.` |
|       - | 3589 | ` */` |
|   13026 | 3590 | `static int Html5Normalize(SyMemBackend *pAlloc,const char *zSrc,int nSrc,SyBlob *pOut)` |
|       1 | 3591 | `{` |
|       - | 3592 | `	int i;` |
|   13027 | 3593 | `	SyBlobInit(pOut,pAlloc);` |
| 1637097 | 3594 | `	for( i = 0 ; i < nSrc ; ++i ){` |
| 1624071 | 3595 | `		if( zSrc[i] == '\r' ){` |
|     521 | 3596 | `			SyBlobAppend(pOut,"\n",1);` |
|     521 | 3597 | `			if( i + 1 < nSrc && zSrc[i + 1] == '\n' ){` |
|     ! 0 | 3598 | `				i++;` |
|     ! 0 | 3599 | `			}` |
|     521 | 3600 | `			continue;` |
|       - | 3601 | `		}` |
| 1623551 | 3602 | `		SyBlobAppend(pOut,&zSrc[i],1);` |
|  811776 | 3603 | `	}` |
|   13027 | 3604 | `	return PH7_OK;` |
|       1 | 3605 | `}` |
|   13026 | 3606 | `PH7_PRIVATE xmlDocPtr PH7_Html5Parse(` |
|       - | 3607 | `	SyMemBackend *pAlloc,` |
|       - | 3608 | `	const char *zSrc,int nSrc,` |
|       - | 3609 | `	int iFlags,` |
|       - | 3610 | `	const char *zEnc,` |
|       - | 3611 | `	void (*xErr)(void *,const char *,const char *,sxu32,sxu32,sxu32),` |
|       - | 3612 | `	void *pErrUser` |
|       - | 3613 | `	)` |
|       1 | 3614 | `{` |
|       - | 3615 | `	html5_parser sParser;` |
|       - | 3616 | `	SyBlob sIn;` |
|       - | 3617 | `	xmlDocPtr pDoc;` |
|   13027 | 3618 | `	pDoc = xmlNewDoc((const xmlChar *)"1.0");` |
|   13027 | 3619 | `	if( pDoc == 0 ){` |
|     ! 0 | 3620 | `		return 0;` |
|       - | 3621 | `	}` |
|   13027 | 3622 | `	pDoc->encoding = xmlStrdup((const xmlChar *)(zEnc && zEnc[0] ? zEnc : "UTF-8"));` |
|   13027 | 3623 | `	pDoc->standalone = 1;` |
|   13027 | 3624 | `	Html5Normalize(pAlloc,zSrc,nSrc,&sIn);` |
|   13027 | 3625 | `	SyZero(&sParser,sizeof(sParser));` |
|   13027 | 3626 | `	sParser.zIn = (const unsigned char *)SyBlobData(&sIn);` |
|   13027 | 3627 | `	sParser.nIn = SyBlobLength(&sIn);` |
|   13027 | 3628 | `	sParser.iLine = 1;` |
|   13027 | 3629 | `	sParser.iCol = 1;` |
|   13027 | 3630 | `	sParser.pAlloc = pAlloc;` |
|   13027 | 3631 | `	sParser.pDoc = pDoc;` |
|   13027 | 3632 | `	sParser.iFlags = iFlags;` |
|   13027 | 3633 | `	sParser.iMode = HTML5_M_INITIAL;` |
|   13027 | 3634 | `	sParser.bFramesetOk = 1;` |
|       - | 3635 | `	/* A document that states no doctype at all is in quirks mode, so the flag` |
|       - | 3636 | `	 * starts SET and only a doctype the initial mode accepts can clear it --` |
|       - | 3637 | `	 * which is also why a second doctype, reaching a later mode, changes` |
|       - | 3638 | `	 * nothing. */` |
|   13027 | 3639 | `	sParser.bQuirks = 1;` |
|   13027 | 3640 | `	sParser.xErr = xErr;` |
|   13027 | 3641 | `	sParser.pErrUser = pErrUser;` |
|   13027 | 3642 | `	SySetInit(&sParser.sOpen,pAlloc,sizeof(xmlNodePtr));` |
|   13027 | 3643 | `	SySetInit(&sParser.sFmt,pAlloc,sizeof(xmlNodePtr));` |
|   13027 | 3644 | `	SySetInit(&sParser.sTmpl,pAlloc,sizeof(int));` |
|   13027 | 3645 | `	SySetInit(&sParser.sAttr,pAlloc,sizeof(html5_attr));` |
|   13027 | 3646 | `	SyBlobInit(&sParser.sName,pAlloc);` |
|   13027 | 3647 | `	SyBlobInit(&sParser.sBuf,pAlloc);` |
|   13027 | 3648 | `	SyBlobInit(&sParser.sAttrBuf,pAlloc);` |
|   13027 | 3649 | `	SyBlobInit(&sParser.sPubId,pAlloc);` |
|   13027 | 3650 | `	SyBlobInit(&sParser.sSysId,pAlloc);` |
|   50207 | 3651 | `	for(;;){` |
|   56721 | 3652 | `		int nGuard = 0;` |
|   56721 | 3653 | `		sParser.bHtmlRules = 0;` |
|   56721 | 3654 | `		Html5NextToken(&sParser);` |
|   56721 | 3655 | `		if( sParser.iTok == HTML5_TOK_EOF ){` |
|   13027 | 3656 | `			break;` |
|       - | 3657 | `		}` |
|       - | 3658 | `		/*` |
|       - | 3659 | `		 * A mode may hand the token back once per mode it walks through, and` |
|       - | 3660 | `		 * there are seven of them; the guard is what turns a rule written` |
|       - | 3661 | `		 * wrong into a refusal rather than a hang.` |
|       - | 3662 | `		 */` |
|  106447 | 3663 | `		while( Html5Dispatch(&sParser) && ++nGuard < 16 ){` |
|       - | 3664 | `			;` |
|       1 | 3665 | `		}` |
|   43695 | 3666 | `		if( sParser.bCloseP ){` |
|      23 | 3667 | `			xmlNodePtr pTop = Html5Top(&sParser);` |
|      23 | 3668 | `			sParser.bCloseP = 0;` |
|       - | 3669 | `			/*` |
|       - | 3670 | ``			 * Only if the minted `<p>` is really open: the start tag it was`` |
|       - | 3671 | ``			 * re-run as can still be dropped -- inside a `<select>`, or by a`` |
|       - | 3672 | `			 * mode that answers end tags for itself -- and there is nothing` |
|       - | 3673 | `			 * to close then.` |
|       - | 3674 | `			 */` |
|      23 | 3675 | `			if( pTop && Html5Eq((const char *)pTop->name,"p") ){` |
|      23 | 3676 | `				Html5Pop(&sParser);` |
|      11 | 3677 | `			}` |
|      11 | 3678 | `		}` |
|       1 | 3679 | `	}` |
|       - | 3680 | `	/*` |
|       - | 3681 | `	 * A document that reached the end without ever leaving the initial modes` |
|       - | 3682 | `` 	 * still owes its skeleton: php answers `<html><head></head><body></body>` `` |
|       - | 3683 | `	 * for an empty string.` |
|       - | 3684 | `	 */` |
|   13027 | 3685 | `	if( (iFlags & HTML5_NOIMPLIED) == 0 ){` |
|   13025 | 3686 | `		if( sParser.pHtml == 0 ){` |
|      39 | 3687 | `			Html5OpenHtml(&sParser,FALSE);` |
|      19 | 3688 | `		}` |
|   13025 | 3689 | `		if( sParser.pHead == 0 ){` |
|      39 | 3690 | `			while( Html5Top(&sParser) && Html5Top(&sParser) != sParser.pHtml ){` |
|     ! 0 | 3691 | `				Html5Pop(&sParser);` |
|     ! 0 | 3692 | `			}` |
|      39 | 3693 | `			Html5OpenHead(&sParser,FALSE);` |
|      19 | 3694 | `		}` |
|       - | 3695 | `		/*` |
|       - | 3696 | `		 * A frameset document owes no body -- it is the one shape where the` |
|       - | 3697 | ``		 * root holds `head` and a sibling that is not `body`.`` |
|       - | 3698 | `		 */` |
|   13024 | 3699 | `		if( sParser.pHtml && Html5ChildNamed(sParser.pHtml,"body") == 0` |
|    6682 | 3700 | `		 && Html5ChildNamed(sParser.pHtml,"frameset") == 0 ){` |
|     495 | 3701 | `			while( Html5Top(&sParser) && Html5Top(&sParser) != sParser.pHtml ){` |
|     249 | 3702 | `				Html5Pop(&sParser);` |
|       1 | 3703 | `			}` |
|     247 | 3704 | `			Html5OpenBody(&sParser,FALSE);` |
|     123 | 3705 | `		}` |
|    6512 | 3706 | `	}` |
|   13027 | 3707 | `	if( iFlags & HTML5_NO_DEF_NS ){` |
|       5 | 3708 | `		Html5StripForeignNs(pDoc->children);` |
|       2 | 3709 | `	}` |
|   13027 | 3710 | `	SyBlobRelease(&sIn);` |
|   13027 | 3711 | `	SyBlobRelease(&sParser.sName);` |
|   13027 | 3712 | `	SyBlobRelease(&sParser.sBuf);` |
|   13027 | 3713 | `	SyBlobRelease(&sParser.sAttrBuf);` |
|   13027 | 3714 | `	SyBlobRelease(&sParser.sPubId);` |
|   13027 | 3715 | `	SyBlobRelease(&sParser.sSysId);` |
|   13027 | 3716 | `	SySetRelease(&sParser.sOpen);` |
|   13027 | 3717 | `	SySetRelease(&sParser.sFmt);` |
|   13027 | 3718 | `	SySetRelease(&sParser.sTmpl);` |
|   13027 | 3719 | `	SySetRelease(&sParser.sAttr);` |
|   13027 | 3720 | `	return pDoc;` |
|    6514 | 3721 | `}` |
|       - | 3722 | `#else` |
|       - | 3723 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|       - | 3724 | `typedef int vm_dom_html5_unused;` |
|       - | 3725 | `#endif /* PH7_ENABLE_LIBXML */` |
|       - | 3726 |  |
