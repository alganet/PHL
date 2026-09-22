# src/ph7/builtin_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1388/1538 lines (90.25%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `#include <stdlib.h>  /* strtod */` |
|     - |    8 | `#include <math.h>    /* HUGE_VAL */` |
|     - |    9 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|     - |   10 | `/*` |
|     - |   11 | ` * Section:` |
|     - |   12 | ` *    Parsing/classification functions: filter_var, CSV, strip_tags,` |
|     - |   13 | ` *    parse_ini_string, the ctype_* family and URL/base64 coding.` |
|     - |   14 | ` * Status:` |
|     - |   15 | ` *    Stable.` |
|     - |   16 | ` */` |
|     - |   17 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   18 | `#define PH7_NEED_BUILTIN_REG 1` |
|     - |   19 | `#endif` |
|     - |   20 | `#ifndef PH7_DISABLE_DISK_IO` |
|     - |   21 | `#define PH7_NEED_FMT_AND_INI 1` |
|     - |   22 | `#endif` |
|     - |   23 | `#ifdef PH7_NEED_BUILTIN_REG` |
|     - |   24 | `/*` |
|     - |   25 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|     - |   26 | ` *` |
|     - |   27 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|     - |   28 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|     - |   29 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|     - |   30 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|     - |   31 | ` * unconditionally — neither matches PHP's filter semantics.` |
|     - |   32 | ` */` |
|     - |   33 | `#define FV_VALIDATE_INT     257` |
|     - |   34 | `#define FV_VALIDATE_BOOLEAN 258` |
|     - |   35 | `#define FV_VALIDATE_FLOAT   259` |
|     - |   36 | `#define FV_VALIDATE_REGEXP  272` |
|     - |   37 | `#define FV_VALIDATE_URL     273` |
|     - |   38 | `#define FV_VALIDATE_EMAIL   274` |
|     - |   39 | `#define FV_VALIDATE_IP      275` |
|     - |   40 | `#define FV_VALIDATE_MAC     276` |
|     - |   41 | `#define FV_VALIDATE_DOMAIN  277` |
|     - |   42 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|     - |   43 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|     - |   44 | `#define FV_SANITIZE_EMAIL   517` |
|     - |   45 | `#define FV_SANITIZE_URL     518` |
|     - |   46 | `#define FV_SANITIZE_NUMBER_INT   519` |
|     - |   47 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|     - |   48 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|     - |   49 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|     - |   50 | `#define FV_FLAG_ALLOW_HEX    2` |
|     - |   51 | `#define FV_FLAG_STRIP_LOW    4` |
|     - |   52 | `#define FV_FLAG_STRIP_HIGH   8` |
|     - |   53 | `#define FV_FLAG_ENCODE_LOW   16` |
|     - |   54 | `#define FV_FLAG_ENCODE_HIGH  32` |
|     - |   55 | `#define FV_FLAG_ENCODE_AMP   64` |
|     - |   56 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|     - |   57 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|     - |   58 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|     - |   59 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|     - |   60 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|     - |   61 | `#define FV_FLAG_IPV4  1048576` |
|     - |   62 | `#define FV_FLAG_IPV6  2097152` |
|     - |   63 | `#define FV_NULL_ON_FAILURE 134217728` |
|     - |   64 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|     - |   65 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|     - |   66 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|     - |   67 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|     - |   68 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|     - |   69 |  |
|     - |   70 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|     - |   71 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|   153 |   72 | `static void FvTrim(const char **pz,int *pn){` |
|   153 |   73 | `	const char *z = *pz;` |
|   153 |   74 | `	int n = *pn;` |
|   157 |   75 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|   161 |   76 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|   153 |   77 | `	*pz = z; *pn = n;` |
|   153 |   78 | `}` |
|     - |   79 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|    57 |   80 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|    57 |   81 | `	int neg = 0, i;` |
|    57 |   82 | `	sxu64 u = 0;` |
|    57 |   83 | `	FvTrim(&z,&n);` |
|    57 |   84 | `	if( n==0 ){ return 0; }` |
|    51 |   85 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|    51 |   86 | `	if( n==0 ){ return 0; }` |
|    49 |   87 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|     3 |   88 | `		z += 2; n -= 2;` |
|     3 |   89 | `		if( n==0 ){ return 0; }` |
|     7 |   90 | `		for( i=0; i<n; i++ ){` |
|     5 |   91 | `			int h = SyHexToint((unsigned char)z[i]);` |
|     5 |   92 | `			if( h<0 ){ return 0; }` |
|     5 |   93 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|     5 |   94 | `			u = u*16 + (sxu64)h;` |
|     3 |   95 | `		}` |
|    48 |   96 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|     9 |   97 | `		for( i=0; i<n; i++ ){` |
|     7 |   98 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|     7 |   99 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|     7 |  100 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|     4 |  101 | `		}` |
|     2 |  102 | `	}else{` |
|    45 |  103 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|   201 |  104 | `		for( i=0; i<n; i++ ){` |
|   173 |  105 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|   161 |  106 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|   161 |  107 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|    81 |  108 | `		}` |
|     - |  109 | `	}` |
|    33 |  110 | `	if( neg ){` |
|     5 |  111 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|     5 |  112 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|     3 |  113 | `	}else{` |
|    29 |  114 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|    27 |  115 | `		*pOut = (ph7_int64)u;` |
|     - |  116 | `	}` |
|    31 |  117 | `	return 1;` |
|    29 |  118 | `}` |
|     - |  119 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|    69 |  120 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|     - |  121 | `	char zBuf[512];` |
|    69 |  122 | `	int i, m = 0, seenDigit = 0;` |
|    69 |  123 | `	const char *zv; int nv; double d = 0;` |
|    69 |  124 | `	FvTrim(&z,&n);` |
|     - |  125 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|     - |  126 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|    69 |  127 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|    69 |  128 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|     - |  129 | `		/* Commas are optional, but when present they must group the integer part` |
|     - |  130 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|     - |  131 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|     - |  132 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|    25 |  133 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|    25 |  134 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|    25 |  135 | `		intEnd = s;` |
|   167 |  136 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|   143 |  137 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|   143 |  138 | `			intEnd++;` |
|     1 |  139 | `		}` |
|    25 |  140 | `		if( hasComma ){` |
|    25 |  141 | `			segStart = s; segIdx = 0;` |
|   165 |  142 | `			for( i=s; i<=intEnd; i++ ){` |
|   151 |  143 | `				if( i==intEnd \|\| z[i]==',' ){` |
|    49 |  144 | `					int segLen = i - segStart, k;` |
|    49 |  145 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|    25 |  146 | `					else if( segLen!=3 ){ return 0; }` |
|   119 |  147 | `					for( k=segStart; k<i; k++ ){` |
|    81 |  148 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|    81 |  149 | `						zBuf[m++] = z[k];` |
|    41 |  150 | `					}` |
|    39 |  151 | `					segStart = i+1; segIdx++;` |
|    19 |  152 | `				}` |
|    71 |  153 | `			}` |
|     8 |  154 | `		}else{` |
|   ! 0 |  155 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|     - |  156 | `		}` |
|    27 |  157 | `		for( i=intEnd; i<n; i++ ){` |
|    13 |  158 | `			if( z[i]==',' ){ return 0; }` |
|    13 |  159 | `			zBuf[m++] = z[i];` |
|     7 |  160 | `		}` |
|    15 |  161 | `		zv = zBuf; nv = m;` |
|     8 |  162 | `	}else{` |
|    45 |  163 | `		zv = z; nv = n;` |
|     - |  164 | `	}` |
|    59 |  165 | `	i = 0;` |
|    59 |  166 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|   167 |  167 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|    59 |  168 | `	if( i<nv && zv[i]=='.' ){` |
|    21 |  169 | `		i++;` |
|    39 |  170 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|    10 |  171 | `	}` |
|    59 |  172 | `	if( !seenDigit ){ return 0; }` |
|    57 |  173 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|    29 |  174 | `		i++;` |
|    29 |  175 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    29 |  176 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|   105 |  177 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|    14 |  178 | `	}` |
|    57 |  179 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|     - |  180 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|     - |  181 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|     - |  182 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|     - |  183 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|     - |  184 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|     - |  185 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|     - |  186 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|     - |  187 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|     - |  188 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|    53 |  189 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|    53 |  190 | `	zBuf[nv] = 0;` |
|    53 |  191 | `	errno = 0;` |
|    53 |  192 | `	d = strtod(zBuf,0);` |
|    53 |  193 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|    15 |  194 | `		return 0;` |
|     - |  195 | `	}` |
|    39 |  196 | `	*pOut = d;` |
|    39 |  197 | `	return 1;` |
|    35 |  198 | `}` |
|     - |  199 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|     - |  200 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|     - |  201 | ` * false, NOT failures. */` |
|    33 |  202 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|    33 |  203 | `	FvTrim(&z,&n);` |
|    32 |  204 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|    25 |  205 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|    11 |  206 | `		*pBool = 1; return 1;` |
|     - |  207 | `	}` |
|    22 |  208 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|    11 |  209 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|    11 |  210 | `		*pBool = 0; return 1;` |
|     - |  211 | `	}` |
|     9 |  212 | `	return 0;` |
|    15 |  213 | `}` |
|     - |  214 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|    33 |  215 | `static int FvValidateIp4(const char *z,int n){` |
|    33 |  216 | `	int i = 0, parts = 0;` |
|    77 |  217 | `	while( i<n ){` |
|    65 |  218 | `		int val = 0, digits = 0, start = i;` |
|   143 |  219 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|    85 |  220 | `			val = val*10 + (z[i]-'0');` |
|    85 |  221 | `			if( val>255 ){ return 0; }` |
|    79 |  222 | `			digits++; i++;` |
|     1 |  223 | `		}` |
|    59 |  224 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|    49 |  225 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|    45 |  226 | `		parts++;` |
|    45 |  227 | `		if( parts>4 ){ return 0; }` |
|    45 |  228 | `		if( i<n ){` |
|    33 |  229 | `			if( z[i]!='.' ){ return 0; }` |
|    33 |  230 | `			i++;` |
|    33 |  231 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|    16 |  232 | `		}` |
|     1 |  233 | `	}` |
|    13 |  234 | `	return parts==4;` |
|    17 |  235 | `}` |
|     - |  236 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|     - |  237 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|    19 |  238 | `static int FvIp6Hextets(const char *z,int n){` |
|    19 |  239 | `	int i = 0, segStart = 0, groups = 0;` |
|    19 |  240 | `	if( n==0 ){ return 0; }` |
|   145 |  241 | `	while( i<=n ){` |
|   133 |  242 | `		if( i==n \|\| z[i]==':' ){` |
|    23 |  243 | `			int segLen = i - segStart, j, isV4 = 0;` |
|    23 |  244 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|    77 |  245 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|    23 |  246 | `			if( isV4 ){` |
|    11 |  247 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|    11 |  248 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|     5 |  249 | `				groups += 2;` |
|     3 |  250 | `			}else{` |
|    13 |  251 | `				if( segLen>4 ){ return -1; }` |
|    47 |  252 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|    13 |  253 | `				groups++;` |
|     - |  254 | `			}` |
|    17 |  255 | `			segStart = i+1;` |
|     8 |  256 | `		}` |
|   127 |  257 | `		i++;` |
|     1 |  258 | `	}` |
|    13 |  259 | `	return groups;` |
|    10 |  260 | `}` |
|     - |  261 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|    19 |  262 | `static int FvValidateIp6(const char *z,int n){` |
|    19 |  263 | `	const char *zDbl = 0;` |
|     - |  264 | `	int i, ga, gb;` |
|   139 |  265 | `	for( i=0; i+1<n; i++ ){` |
|   123 |  266 | `		if( z[i]==':' && z[i+1]==':' ){` |
|    13 |  267 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|    11 |  268 | `			zDbl = z+i;` |
|     5 |  269 | `		}` |
|    61 |  270 | `	}` |
|    17 |  271 | `	if( zDbl==0 ){` |
|     9 |  272 | `		return FvIp6Hextets(z,n)==8;` |
|   ! 0 |  273 | `	}else{` |
|     9 |  274 | `		int lenA = (int)(zDbl - z);` |
|     9 |  275 | `		int lenB = n - lenA - 2;` |
|     9 |  276 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|     9 |  277 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|     9 |  278 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|     9 |  279 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|     - |  280 | `	}` |
|    10 |  281 | `}` |
|    25 |  282 | `static int FvValidateIp(const char *z,int n,int flags){` |
|    25 |  283 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|    25 |  284 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|    25 |  285 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|    21 |  286 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|    13 |  287 | `	return 0;` |
|    13 |  288 | `}` |
|     - |  289 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|    11 |  290 | `static int FvValidateMac(const char *z,int n){` |
|     - |  291 | `	char sep;` |
|     - |  292 | `	int i;` |
|    11 |  293 | `	if( n!=17 ){ return 0; }` |
|     7 |  294 | `	sep = z[2];` |
|     7 |  295 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|   105 |  296 | `	for( i=0; i<17; i++ ){` |
|   101 |  297 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|    71 |  298 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|    50 |  299 | `	}` |
|     5 |  300 | `	return 1;` |
|     6 |  301 | `}` |
|     - |  302 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|     - |  303 | ` * parts or IP-literal domains). */` |
|    28 |  304 | `static int FvValidateEmail(const char *z,int n){` |
|    28 |  305 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|     - |  306 | `	const char *zDom;` |
|    28 |  307 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|   201 |  308 | `	for( i=0; i<n; i++ ){` |
|   181 |  309 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|    91 |  310 | `	}` |
|    21 |  311 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|    21 |  312 | `	localLen = at;` |
|    21 |  313 | `	zDom = z + at + 1;` |
|    21 |  314 | `	domLen = n - at - 1;` |
|    21 |  315 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|    57 |  316 | `	for( i=0; i<localLen; i++ ){` |
|    43 |  317 | `		unsigned char c = (unsigned char)z[i];` |
|    43 |  318 | `		if( c<=' ' ){ return 0; }` |
|    41 |  319 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|    20 |  320 | `	}` |
|    15 |  321 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|    13 |  322 | `	labelStart = 0;` |
|    85 |  323 | `	for( i=0; i<=domLen; i++ ){` |
|    75 |  324 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|    25 |  325 | `			int ll = i - labelStart;` |
|    25 |  326 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|    23 |  327 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|    23 |  328 | `			if( i<domLen ){ dotCount++; }` |
|    23 |  329 | `			labelStart = i+1;` |
|    12 |  330 | `		}else{` |
|    51 |  331 | `			unsigned char c = (unsigned char)zDom[i];` |
|    51 |  332 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|     - |  333 | `		}` |
|    37 |  334 | `	}` |
|    11 |  335 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|     9 |  336 | `	return 1;` |
|    15 |  337 | `}` |
|     - |  338 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|    11 |  339 | `static int FvValidateDomain(const char *z,int n){` |
|     - |  340 | `	int i;` |
|    11 |  341 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|    81 |  342 | `	for( i=0; i<n; i++ ){` |
|    75 |  343 | `		unsigned char c = (unsigned char)z[i];` |
|    75 |  344 | `		if( c<=' ' ){ return 0; }` |
|    75 |  345 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|    37 |  346 | `	}` |
|     7 |  347 | `	return 1;` |
|     6 |  348 | `}` |
|     - |  349 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|     - |  350 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|    15 |  351 | `static int FvValidateUrl(const char *z,int n){` |
|     - |  352 | `	SyhttpUri sUri;` |
|    15 |  353 | `	if( n==0 ){ return 0; }` |
|    15 |  354 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|    15 |  355 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|    15 |  356 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|     8 |  357 | `}` |
|     - |  358 | `/* The Fv sanitizers build their result by appending directly to the call` |
|     - |  359 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|     - |  360 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|     - |  361 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|    37 |  362 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|    37 |  363 | `	int i, runStart = 0;` |
|    37 |  364 | `	ph7_result_string(pCtx,"",0);` |
|    97 |  365 | `	for( i=0; i<n; i++ ){` |
|    91 |  366 | `		char c = z[i];` |
|    91 |  367 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|    91 |  368 | `		if( !keep && isFloat ){` |
|    38 |  369 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|    23 |  370 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|    36 |  371 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|    12 |  372 | `		}` |
|    61 |  373 | `		if( !keep ){` |
|    33 |  374 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    33 |  375 | `			runStart = i+1;` |
|    16 |  376 | `		}` |
|    31 |  377 | `	}` |
|     7 |  378 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     7 |  379 | `}` |
|     - |  380 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|     - |  381 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|     - |  382 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|     - |  383 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|   287 |  384 | `static int FvStripByte(unsigned char c,int flags){` |
|   287 |  385 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|   281 |  386 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|   269 |  387 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|   267 |  388 | `	return 0;` |
|   144 |  389 | `}` |
|     - |  390 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|     - |  391 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|     - |  392 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|     - |  393 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|     - |  394 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|     - |  395 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|     - |  396 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|    25 |  397 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|    25 |  398 | `	int i, runStart = 0;` |
|    25 |  399 | `	ph7_result_string(pCtx,"",0);` |
|   193 |  400 | `	for( i=0; i<n; i++ ){` |
|   179 |  401 | `		unsigned char c = (unsigned char)z[i];` |
|   179 |  402 | `		if( FvStripByte(c,flags) ){` |
|    13 |  403 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    13 |  404 | `			runStart = i+1;` |
|    13 |  405 | `			continue;` |
|     - |  406 | `		}` |
|   167 |  407 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|     3 |  408 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     3 |  409 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|     3 |  410 | `			runStart = i+1;` |
|   166 |  411 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|   164 |  412 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|    37 |  413 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     9 |  414 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     9 |  415 | `			runStart = i+1;` |
|     4 |  416 | `		}` |
|    79 |  417 | `	}` |
|    15 |  418 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|    15 |  419 | `}` |
|     - |  420 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|     - |  421 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|     - |  422 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|     - |  423 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|     - |  424 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|    13 |  425 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|    13 |  426 | `	int i, runStart = 0;` |
|     - |  427 | `	const char *zEnt;` |
|    13 |  428 | `	ph7_result_string(pCtx,"",0);` |
|   131 |  429 | `	for( i=0; i<n; i++ ){` |
|   119 |  430 | `		unsigned char c = (unsigned char)z[i];` |
|   119 |  431 | `		if( FvStripByte(c,flags) ){` |
|     9 |  432 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     9 |  433 | `			runStart = i+1;` |
|     9 |  434 | `			continue;` |
|     - |  435 | `		}` |
|   111 |  436 | `		switch( c ){` |
|     3 |  437 | `		case '<':  zEnt = "&#60;"; break;` |
|     3 |  438 | `		case '>':  zEnt = "&#62;"; break;` |
|    11 |  439 | `		case '&':  zEnt = "&#38;"; break;` |
|     3 |  440 | `		case '"':  zEnt = "&#34;"; break;` |
|     3 |  441 | `		case '\'': zEnt = "&#39;"; break;` |
|    46 |  442 | `		default:` |
|     - |  443 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|     - |  444 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|    93 |  445 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|    17 |  446 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    17 |  447 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|    17 |  448 | `				runStart = i+1;` |
|     8 |  449 | `			}` |
|    93 |  450 | `			continue; /* keep in the current run */` |
|     - |  451 | `		}` |
|    19 |  452 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    19 |  453 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|    19 |  454 | `		runStart = i+1;` |
|    10 |  455 | `	}` |
|    13 |  456 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|    13 |  457 | `}` |
|     - |  458 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|     - |  459 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|     - |  460 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|     - |  461 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|     - |  462 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|     - |  463 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|     - |  464 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|     - |  465 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|     - |  466 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|     - |  467 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|     - |  468 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|     - |  469 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|     - |  470 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|     - |  471 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|     - |  472 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|     - |  473 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|     - |  474 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|     - |  475 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|     - |  476 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|     - |  477 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|     - |  478 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|     - |  479 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|     - |  480 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|     - |  481 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|     - |  482 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|     - |  483 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|     - |  484 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|     - |  485 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|     - |  486 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|     - |  487 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|     - |  488 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|     - |  489 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|     - |  490 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|     - |  491 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|     - |  492 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|     - |  493 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|     - |  494 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|     - |  495 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|     - |  496 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|     - |  497 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|     - |  498 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|     - |  499 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|     - |  500 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|     - |  501 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|     - |  502 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|     - |  503 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|     - |  504 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|     - |  505 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|     - |  506 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|     - |  507 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|     - |  508 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|     - |  509 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|     - |  510 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|     - |  511 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|     - |  512 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|     - |  513 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|     - |  514 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|     - |  515 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|     - |  516 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|     - |  517 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|     - |  518 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|     - |  519 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|     - |  520 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|     - |  521 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|     - |  522 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|     - |  523 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|     - |  524 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|     - |  525 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|     - |  526 | `};` |
|     - |  527 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|    41 |  528 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|    41 |  529 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|   323 |  530 | `	while( lo <= hi ){` |
|   309 |  531 | `		int mid = (lo + hi) / 2;` |
|   309 |  532 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|   309 |  533 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|   283 |  534 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|     1 |  535 | `	}` |
|    15 |  536 | `	return 0;` |
|    21 |  537 | `}` |
|     - |  538 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|     - |  539 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|     - |  540 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|     - |  541 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|   101 |  542 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|   101 |  543 | `	unsigned char c = p[0];` |
|   101 |  544 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|   101 |  545 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|    99 |  546 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|    47 |  547 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|    45 |  548 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|    45 |  549 | `		return 2;` |
|     - |  550 | `	}` |
|    53 |  551 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|     - |  552 | `		sxu32 cp;` |
|    47 |  553 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|    33 |  554 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|    33 |  555 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|    29 |  556 | `		*pCp = cp;` |
|    29 |  557 | `		return 3;` |
|     - |  558 | `	}` |
|     7 |  559 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|     - |  560 | `		sxu32 cp;` |
|     5 |  561 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|     5 |  562 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|     5 |  563 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|     5 |  564 | `		*pCp = cp;` |
|     5 |  565 | `		return 4;` |
|     - |  566 | `	}` |
|     3 |  567 | `	return 0;                                /* 0xF5-0xFF */` |
|    51 |  568 | `}` |
|     - |  569 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|     - |  570 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|     - |  571 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|     - |  572 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|     - |  573 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|     - |  574 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|     - |  575 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|     - |  576 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|     - |  577 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|     - |  578 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|    25 |  579 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|    25 |  580 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|    25 |  581 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|    25 |  582 | `}` |
|     - |  583 | `/* ---------------------------------------------------------------------------` |
|     - |  584 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|     - |  585 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|     - |  586 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|     - |  587 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|     - |  588 | ` * ------------------------------------------------------------------------ */` |
|     - |  589 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|     - |  590 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|   585 |  591 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|   585 |  592 | `	sxu8 *z = (sxu8 *)zBuf;` |
|   585 |  593 | `	SX_WRITE_UTF8(z,cp);` |
|   585 |  594 | `	return (int)(z - (sxu8 *)zBuf);` |
|     1 |  595 | `}` |
|     - |  596 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|     - |  597 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|     - |  598 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|     - |  599 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|     - |  600 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|     - |  601 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|    91 |  602 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|    91 |  603 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|    91 |  604 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|    87 |  605 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|    85 |  606 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|    85 |  607 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|    79 |  608 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|    77 |  609 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|    71 |  610 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|   ! 0 |  611 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|     - |  612 | `	}` |
|    71 |  613 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|     9 |  614 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|     9 |  615 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|     4 |  616 | `	}` |
|    71 |  617 | `	return 1;` |
|    46 |  618 | `}` |
|     - |  619 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|     - |  620 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|     - |  621 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|     - |  622 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|     9 |  623 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|     9 |  624 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|     9 |  625 | `	return HtmlCpAllowed(cp,iFlags);` |
|     5 |  626 | `}` |
|     - |  627 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|     - |  628 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|     - |  629 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|     - |  630 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|     - |  631 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|     - |  632 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|     - |  633 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|     - |  634 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|     9 |  635 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|     9 |  636 | `	if( cp > 0x10FFFF ){ return 0; }` |
|     7 |  637 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|   ! 0 |  638 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|   ! 0 |  639 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|   ! 0 |  640 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|   ! 0 |  641 | `	return 1;` |
|     5 |  642 | `}` |
|     - |  643 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|     - |  644 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|     - |  645 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|     - |  646 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|     - |  647 | ` * start a new sequence is left for the next round. */` |
|     5 |  648 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|    11 |  649 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|    15 |  650 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|    15 |  651 | `	unsigned char c = p[0];` |
|    15 |  652 | `	int nAvail = (int)(zEnd - p);` |
|    15 |  653 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|    13 |  654 | `	if( c < 0xE0 ){` |
|     3 |  655 | `		if( nAvail < 2 ){ return 1; }` |
|     3 |  656 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|     - |  657 | `	}` |
|    11 |  658 | `	if( c < 0xF0 ){` |
|    11 |  659 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|     3 |  660 | `			return 3; /* complete but overlong/surrogate */` |
|     - |  661 | `		}` |
|     9 |  662 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|   ! 0 |  663 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|   ! 0 |  664 | `		return 3;` |
|     - |  665 | `	}` |
|   ! 0 |  666 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|   ! 0 |  667 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|     - |  668 | `	}` |
|   ! 0 |  669 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|   ! 0 |  670 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|   ! 0 |  671 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|   ! 0 |  672 | `	return 4;` |
|     8 |  673 | `}` |
|     - |  674 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|     - |  675 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|     - |  676 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|     - |  677 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|     - |  678 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|     - |  679 | `};` |
|     - |  680 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|     - |  681 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|     - |  682 | ` * HTML 4.01 table (documented divergence). */` |
|    63 |  683 | `static int HtmlDocHasNamedTable(int iDoc){` |
|    63 |  684 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|     1 |  685 | `}` |
|     - |  686 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|     - |  687 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|     - |  688 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|     - |  689 | ` * whichever function the requested table belongs to. */` |
|    29 |  690 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|    29 |  691 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|    21 |  692 | `		return "&#039;";` |
|     - |  693 | `	}` |
|     9 |  694 | `	return "&apos;";` |
|    15 |  695 | `}` |
|     - |  696 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|     - |  697 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|     - |  698 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|     - |  699 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|     - |  700 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|     - |  701 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|     - |  702 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|     - |  703 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|     - |  704 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|     - |  705 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|     - |  706 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|     - |  707 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|   172 |  708 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|     1 |  709 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|   173 |  710 | `	int nAvail = (int)(zEnd - z);` |
|   173 |  711 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - |  712 | `	sxu32 n;` |
|   173 |  713 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|   169 |  714 | `	if( z[1] == '#' ){` |
|     - |  715 | `		/* Numeric reference */` |
|    89 |  716 | `		sxu32 cp = 0;` |
|    89 |  717 | `		int i = 2, bHex = 0, nDig = 0;` |
|    89 |  718 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|   317 |  719 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|     - |  720 | `			int v;` |
|   221 |  721 | `			unsigned char c = z[i];` |
|   221 |  722 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|    17 |  723 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|    17 |  724 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|   ! 0 |  725 | `			else { return 0; }` |
|     - |  726 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|     - |  727 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|   221 |  728 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|   221 |  729 | `			nDig++;` |
|   111 |  730 | `		}` |
|    97 |  731 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|    97 |  732 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    83 |  733 | `		if( !bFull ){` |
|     - |  734 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|    99 |  735 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|    25 |  736 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|    11 |  737 | `		}` |
|    75 |  738 | `		*pCp = cp;` |
|    75 |  739 | `		*pnConsumed = i + 1;` |
|    75 |  740 | `		return 1;` |
|     - |  741 | `	}` |
|     - |  742 | `	/* Named reference — every entity name starts with a letter, so anything` |
|     - |  743 | `	 * else can bail out before touching the tables. */` |
|    81 |  744 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|   287 |  745 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|   265 |  746 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|   243 |  747 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|    53 |  748 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|    53 |  749 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|    53 |  750 | `			return 1;` |
|     - |  751 | `		}` |
|    96 |  752 | `	}` |
|    23 |  753 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|     - |  754 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|     - |  755 | `		 * positions and guarantees the decode set can never drift from the` |
|     - |  756 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|     - |  757 | `		 * for ~96% of rows. */` |
|  3369 |  758 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|     - |  759 | `			sxu32 nEnt;` |
|  3357 |  760 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|   121 |  761 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|   121 |  762 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|     7 |  763 | `				*pCp = aHtml401Ent[n].cp;` |
|     7 |  764 | `				*pnConsumed = (int)nEnt;` |
|     7 |  765 | `				return 1;` |
|     - |  766 | `			}` |
|    58 |  767 | `		}` |
|     6 |  768 | `	}` |
|    17 |  769 | `	return 0;` |
|    88 |  770 | `}` |
|     - |  771 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|     - |  772 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|     - |  773 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|     - |  774 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|     - |  775 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|    96 |  776 | `PH7_PRIVATE void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|     1 |  777 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|    97 |  778 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|    97 |  779 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     - |  780 | `	const unsigned char *runStart;` |
|    97 |  781 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - |  782 | `	sxu32 cp;` |
|    97 |  783 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|     - |  784 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|     - |  785 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|   381 |  786 | `		while( p < zEnd ){` |
|     - |  787 | `			int len;` |
|   323 |  788 | `			if( *p < 0x80 ){ p++; continue; }` |
|    37 |  789 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|    37 |  790 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|    27 |  791 | `			p += len;` |
|     1 |  792 | `		}` |
|    59 |  793 | `		p = (const unsigned char *)zIn;` |
|    29 |  794 | `	}` |
|    87 |  795 | `	runStart = p;` |
|    87 |  796 | `	ph7_result_string(pCtx,"",0);` |
|   463 |  797 | `	while( p < zEnd ){` |
|   377 |  798 | `		const char *zEnt = 0;` |
|     - |  799 | `		int len;` |
|   377 |  800 | `		if( *p < 0x80 ){` |
|   313 |  801 | `			len = 1;` |
|   313 |  802 | `			switch( *p ){` |
|    25 |  803 | `			case '<': zEnt = "&lt;"; break;` |
|    25 |  804 | `			case '>': zEnt = "&gt;"; break;` |
|    18 |  805 | `			case '&':` |
|    37 |  806 | `				zEnt = "&amp;";` |
|    37 |  807 | `				if( !bDoubleEncode ){` |
|     - |  808 | `					sxu32 eCp; int nEat;` |
|    25 |  809 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|     - |  810 | `						/* A valid existing entity: keep it verbatim. */` |
|    13 |  811 | `						zEnt = 0;` |
|    13 |  812 | `						len = nEat;` |
|     6 |  813 | `					}` |
|    12 |  814 | `				}` |
|    37 |  815 | `				break;` |
|    10 |  816 | `			case '"':` |
|    21 |  817 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|    21 |  818 | `				break;` |
|    12 |  819 | `			case '\'':` |
|    25 |  820 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|    23 |  821 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|    11 |  822 | `				}` |
|    25 |  823 | `				break;` |
|    92 |  824 | `			default:` |
|   185 |  825 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|   ! 0 |  826 | `					zEnt = "\xEF\xBF\xBD";` |
|   ! 0 |  827 | `				}` |
|   184 |  828 | `				break;` |
|     - |  829 | `			}` |
|   157 |  830 | `		}else{` |
|    65 |  831 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|    65 |  832 | `			if( len == 0 ){` |
|     - |  833 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|     - |  834 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|     - |  835 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|    15 |  836 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    15 |  837 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|    15 |  838 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|    15 |  839 | `				runStart = p;` |
|    15 |  840 | `				continue;` |
|     - |  841 | `			}` |
|    51 |  842 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|    41 |  843 | `				zEnt = FvHtml401Lookup(cp);` |
|    20 |  844 | `			}` |
|    51 |  845 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|   ! 0 |  846 | `				zEnt = "\xEF\xBF\xBD";` |
|   ! 0 |  847 | `			}` |
|     - |  848 | `		}` |
|   363 |  849 | `		if( zEnt ){` |
|   135 |  850 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|   135 |  851 | `			ph7_result_string(pCtx,zEnt,-1);` |
|   135 |  852 | `			runStart = p + len;` |
|    67 |  853 | `		}` |
|   363 |  854 | `		p += len;` |
|     1 |  855 | `	}` |
|    87 |  856 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|    49 |  857 | `}` |
|     - |  858 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|     - |  859 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|     - |  860 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|     - |  861 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|     - |  862 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|    84 |  863 | `PH7_PRIVATE void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|     1 |  864 | `                         int iFlags,int bFull){` |
|    85 |  865 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|    85 |  866 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|    85 |  867 | `	const unsigned char *runStart = p;` |
|    85 |  868 | `	ph7_result_string(pCtx,"",0);` |
|   565 |  869 | `	while( p < zEnd ){` |
|     - |  870 | `		sxu32 cp;` |
|     - |  871 | `		int nEat;` |
|   516 |  872 | `		if( *p != '&' ){ p++; continue; }` |
|   155 |  873 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|   124 |  874 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|   117 |  875 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|     - |  876 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|    37 |  877 | `			p += nEat;` |
|    37 |  878 | `			continue;` |
|     - |  879 | `		}` |
|    89 |  880 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     - |  881 | `		{` |
|     - |  882 | `			char zBuf[4];` |
|    89 |  883 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|    89 |  884 | `			ph7_result_string(pCtx,zBuf,n);` |
|     - |  885 | `		}` |
|    89 |  886 | `		p += nEat;` |
|    89 |  887 | `		runStart = p;` |
|     1 |  888 | `	}` |
|    81 |  889 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|    81 |  890 | `}` |
|     - |  891 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|     - |  892 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|     - |  893 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|     - |  894 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|     - |  895 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|   143 |  896 | `PH7_PRIVATE void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|     - |  897 | `	const char *zCs;` |
|     - |  898 | `	int nCs;` |
|   150 |  899 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|    15 |  900 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|    15 |  901 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|    13 |  902 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|    13 |  903 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|     - |  904 | `	}` |
|   ! 0 |  905 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|   ! 0 |  906 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|    72 |  907 | `}` |
|     - |  908 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|     - |  909 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|     - |  910 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|     - |  911 | ` * ordering; 253 entries under the defaults). */` |
|   549 |  912 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|   549 |  913 | `	ph7_value_string(pValue,zEnt,-1);` |
|   549 |  914 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|   549 |  915 | `	ph7_value_reset_string_cursor(pValue);` |
|   549 |  916 | `}` |
|    13 |  917 | `PH7_PRIVATE void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|     - |  918 | `	ph7_value *pArray,*pValue;` |
|    13 |  919 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     - |  920 | `	sxu32 n;` |
|    13 |  921 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    13 |  922 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 |  923 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|   ! 0 |  924 | `		ph7_result_null(pCtx);` |
|   ! 0 |  925 | `		return;` |
|     - |  926 | `	}` |
|    13 |  927 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|    11 |  928 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|     5 |  929 | `	}` |
|    13 |  930 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|    13 |  931 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     - |  932 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|     - |  933 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|     - |  934 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|     7 |  935 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|     3 |  936 | `	}` |
|    13 |  937 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|    13 |  938 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|    13 |  939 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|     - |  940 | `		char zKey[8];` |
|   499 |  941 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|   497 |  942 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|   497 |  943 | `			zKey[nK] = 0;` |
|   497 |  944 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|   249 |  945 | `		}` |
|     1 |  946 | `	}` |
|    13 |  947 | `	ph7_result_value(pCtx,pArray);` |
|     7 |  948 | `}` |
|    25 |  949 | `static int FvEmailAllowed(unsigned char c){` |
|    25 |  950 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|    16 |  951 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|    10 |  952 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|    15 |  953 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|    13 |  954 | `}` |
|    23 |  955 | `static int FvUrlAllowed(unsigned char c){` |
|    23 |  956 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|     1 |  957 | `}` |
|     - |  958 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|     5 |  959 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|     5 |  960 | `	int i, runStart = 0;` |
|     5 |  961 | `	ph7_result_string(pCtx,"",0);` |
|    51 |  962 | `	for( i=0; i<n; i++ ){` |
|    47 |  963 | `		unsigned char c = (unsigned char)z[i];` |
|    47 |  964 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|    11 |  965 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|    11 |  966 | `			runStart = i+1;` |
|     5 |  967 | `		}` |
|    24 |  968 | `	}` |
|     5 |  969 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     5 |  970 | `}` |
|     - |  971 | `/*` |
|     - |  972 | ` * Apply the selected filter to one already-resolved input value and write the` |
|     - |  973 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|     - |  974 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|     - |  975 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|     - |  976 | ` * else false. A validating filter that passes returns the (string) input` |
|     - |  977 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|     - |  978 | ` */` |
|   316 |  979 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|     - |  980 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|     - |  981 | `                         ph7_value *pDefault)` |
|     3 |  982 | `{` |
|   319 |  983 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|     - |  984 | `	const char *zVal; int nVal;` |
|     - |  985 | `	/* An array/object input fails every scalar filter. */` |
|   319 |  986 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|   317 |  987 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|   317 |  988 | `	switch( iFilter ){` |
|    28 |  989 | `	case FV_VALIDATE_INT: {` |
|     - |  990 | `		ph7_int64 v;` |
|    58 |  991 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|    31 |  992 | `		if( pOpts ){` |
|     7 |  993 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|     7 |  994 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|     7 |  995 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|     7 |  996 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|     2 |  997 | `		}` |
|    29 |  998 | `		ph7_result_int64(pCtx,v);` |
|    29 |  999 | `		return PH7_OK;` |
|     - | 1000 | `	}` |
|    34 | 1001 | `	case FV_VALIDATE_FLOAT: {` |
|     - | 1002 | `		double d;` |
|    69 | 1003 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|    39 | 1004 | `		ph7_result_double(pCtx,d);` |
|    39 | 1005 | `		return PH7_OK;` |
|     - | 1006 | `	}` |
|    14 | 1007 | `	case FV_VALIDATE_BOOLEAN: {` |
|     - | 1008 | `		int b;` |
|    29 | 1009 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|    21 | 1010 | `		ph7_result_bool(pCtx,b);` |
|    21 | 1011 | `		return PH7_OK;` |
|     - | 1012 | `	}` |
|    25 | 1013 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|    11 | 1014 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|    28 | 1015 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|    11 | 1016 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|    15 | 1017 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|     3 | 1018 | `	case FV_VALIDATE_REGEXP: {` |
|     - | 1019 | `#ifdef PH7_ENABLE_PCRE` |
|     8 | 1020 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|     8 | 1021 | `		const char *zRe; int nRe, matched = 0;` |
|     8 | 1022 | `		if( pRe==0 ){` |
|     3 | 1023 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1024 | `				"filter_var(): \"regexp\" option is missing");` |
|     - | 1025 | `		}` |
|     5 | 1026 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|     5 | 1027 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|     3 | 1028 | `		goto pass;` |
|     - | 1029 | `#else` |
|     - | 1030 | `		goto fail;` |
|     - | 1031 | `#endif` |
|     - | 1032 | `	}` |
|     3 | 1033 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|     5 | 1034 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|    13 | 1035 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|    25 | 1036 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|     3 | 1037 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|     3 | 1038 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|    13 | 1039 | `	case FV_DEFAULT:` |
|     - | 1040 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|     - | 1041 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|    28 | 1042 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|    15 | 1043 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|    15 | 1044 | `			return PH7_OK;` |
|     - | 1045 | `		}` |
|    14 | 1046 | `		goto pass;` |
|   ! 0 | 1047 | `	default:` |
|   ! 0 | 1048 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|   ! 0 | 1049 | `			"Unknown filter with ID %d",iFilter);` |
|   ! 0 | 1050 | `		break; /* unknown filter id -> fail */` |
|   ! 0 | 1051 | `	}` |
|    58 | 1052 | `fail:` |
|   118 | 1053 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|   114 | 1054 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|   108 | 1055 | `	else { ph7_result_bool(pCtx,0); }` |
|   118 | 1056 | `	return PH7_OK;` |
|    26 | 1057 | `pass: /* validation passed: return the (string) input unchanged */` |
|    54 | 1058 | `	ph7_result_string(pCtx,zVal,nVal);` |
|    54 | 1059 | `	return PH7_OK;` |
|   161 | 1060 | `}` |
|     - | 1061 | `/*` |
|     - | 1062 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|     - | 1063 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|     - | 1064 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|     - | 1065 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|     - | 1066 | ` * unset outputs keep the caller-provided defaults.` |
|     - | 1067 | ` */` |
|   328 | 1068 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|     - | 1069 | `                              int *piFilter,int *piFlags,` |
|     - | 1070 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|     3 | 1071 | `{` |
|   331 | 1072 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|   331 | 1073 | `	if( nArg>iBase+1 ){` |
|    88 | 1074 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|    42 | 1075 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|    42 | 1076 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|    42 | 1077 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|    42 | 1078 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|    42 | 1079 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|    22 | 1080 | `		}else{` |
|    48 | 1081 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|     - | 1082 | `		}` |
|    43 | 1083 | `	}` |
|   331 | 1084 | `}` |
|     - | 1085 | `/*` |
|     - | 1086 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|     - | 1087 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|     - | 1088 | ` */` |
|   306 | 1089 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1090 | `{` |
|   308 | 1091 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|   308 | 1092 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|   308 | 1093 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|   308 | 1094 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|   308 | 1095 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|   155 | 1096 | `}` |
|     - | 1097 | `/*` |
|     - | 1098 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|     - | 1099 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|     - | 1100 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|     - | 1101 | ` *   - variable NOT set: 'default' option wins, else false when` |
|     - | 1102 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|     - | 1103 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|     - | 1104 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|     - | 1105 | ` *   - variable present: delegate to FvApplyFilter.` |
|     - | 1106 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|     - | 1107 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|     - | 1108 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|     - | 1109 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|     - | 1110 | ` *  php's snapshot.` |
|     - | 1111 | ` */` |
|    24 | 1112 | `PH7_PRIVATE int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1113 | `{` |
|    26 | 1114 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|    26 | 1115 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|     - | 1116 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|    26 | 1117 | `	if( nArg<2 ){` |
|   ! 0 | 1118 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|   ! 0 | 1119 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|     - | 1120 | `	}` |
|    26 | 1121 | `	iType = ph7_value_to_int(apArg[0]);` |
|    26 | 1122 | `	switch( iType ){` |
|     3 | 1123 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|     3 | 1124 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|   ! 0 | 1125 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|   ! 0 | 1126 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|    19 | 1127 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|     1 | 1128 | `	default:` |
|     3 | 1129 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1130 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|     - | 1131 | `	}` |
|    23 | 1132 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|    23 | 1133 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|     - | 1134 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|    23 | 1135 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|    23 | 1136 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|    33 | 1137 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|    23 | 1138 | `	if( pElem==0 ){` |
|     - | 1139 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|     - | 1140 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|    13 | 1141 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|     9 | 1142 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|     7 | 1143 | `		else { ph7_result_null(pCtx); }` |
|    13 | 1144 | `		return PH7_OK;` |
|     - | 1145 | `	}` |
|    11 | 1146 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|    14 | 1147 | `}` |
|     - | 1148 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 1149 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - | 1150 | `/*` |
|     - | 1151 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|     - | 1152 |  |
|     - | 1153 | ` */` |
|    14 | 1154 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|     - | 1155 | `	const char *zInput, /* Raw input */` |
|     - | 1156 | `	int nByte,  /* Input length */` |
|     - | 1157 | `	int delim,  /* Delimiter */` |
|     - | 1158 | `	int encl,   /* Enclosure */` |
|     - | 1159 | `	int escape,  /* Escape character */` |
|     - | 1160 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|     - | 1161 | `	void *pUserData /* Last argument to xConsumer() */` |
|     - | 1162 | `	)` |
|     1 | 1163 | `{` |
|    15 | 1164 | `	const char *zEnd = &zInput[nByte];` |
|    15 | 1165 | `	const char *zIn = zInput;` |
|     - | 1166 | `	const char *zPtr;` |
|     - | 1167 | `	int isEnc;` |
|     - | 1168 | `	/* Start processing */` |
|    24 | 1169 | `	for(;;){` |
|    49 | 1170 | `		if( zIn >= zEnd ){` |
|     - | 1171 | `			/* No more input to process */` |
|    15 | 1172 | `			break;` |
|     - | 1173 | `		}` |
|    35 | 1174 | `		isEnc = 0;` |
|    35 | 1175 | `		zPtr = zIn;` |
|     - | 1176 | `		/* Find the first delimiter */` |
|    81 | 1177 | `		while( zIn < zEnd ){` |
|    67 | 1178 | `			if( zIn[0] == delim && !isEnc){` |
|     - | 1179 | `				/* Delimiter found,break imediately */` |
|    11 | 1180 | `				break;` |
|    47 | 1181 | `			}else if( zIn[0] == encl ){` |
|     - | 1182 | `				/* Inside enclosure? */` |
|   ! 0 | 1183 | `				isEnc = !isEnc;` |
|    47 | 1184 | `			}else if( zIn[0] == escape ){` |
|     - | 1185 | `				/* Escape sequence */` |
|   ! 0 | 1186 | `				zIn++;` |
|   ! 0 | 1187 | `			}` |
|     - | 1188 | `			/* Advance the cursor */` |
|    47 | 1189 | `			zIn++;` |
|     1 | 1190 | `		}` |
|    35 | 1191 | `		if( zIn > zPtr ){` |
|    35 | 1192 | `			int nByteChunk = (int)(zIn-zPtr);` |
|     - | 1193 | `			sxi32 rc;` |
|     - | 1194 | `			/* Invoke the supllied callback */` |
|    35 | 1195 | `			if( zPtr[0] == encl ){` |
|   ! 0 | 1196 | `				zPtr++;` |
|   ! 0 | 1197 | `				nByteChunk-=2;` |
|   ! 0 | 1198 | `			}` |
|    35 | 1199 | `			if( nByteChunk > 0 ){` |
|    35 | 1200 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|    35 | 1201 | `				if( rc == SXERR_ABORT ){` |
|     - | 1202 | `					/* User callback request an operation abort */` |
|   ! 0 | 1203 | `					break;` |
|     - | 1204 | `				}` |
|    17 | 1205 | `			}` |
|    17 | 1206 | `		}` |
|     - | 1207 | `		/* Ignore trailing delimiter */` |
|    55 | 1208 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|    21 | 1209 | `			zIn++;` |
|     1 | 1210 | `		}` |
|     1 | 1211 | `	}` |
|    15 | 1212 | `	return SXRET_OK;` |
|     1 | 1213 | `}` |
|     - | 1214 | `/*` |
|     - | 1215 | ` * Validate a CSV $separator/$enclosure/$escape argument like php 8 and` |
|     - | 1216 | ` * extract its character. $separator/$enclosure must be exactly one` |
|     - | 1217 | ` * character; $escape may also be empty, which disables escape processing` |
|     - | 1218 | ` * (the caller gets PH7_CSV_NO_ESCAPE). The argument is coerced to string` |
|     - | 1219 | ` * first like php's ZPP, so an int 5 separates on "5"; null and array` |
|     - | 1220 | ` * arguments never reach here — the central type screen rejects them.` |
|     - | 1221 | ` * Returns PH7_OK on success; otherwise the ValueError has been thrown and` |
|     - | 1222 | ` * the caller must return the propagated status.` |
|     - | 1223 | ` */` |
|   112 | 1224 | `PH7_PRIVATE sxi32 PH7_CsvCharArg(ph7_context *pCtx,ph7_value *pArg,int iArg,` |
|     - | 1225 | `	const char *zName,int bAllowEmpty,int *pChar)` |
|     1 | 1226 | `{` |
|     - | 1227 | `	const char *zPtr;` |
|     - | 1228 | `	int n;` |
|   113 | 1229 | `	zPtr = ph7_value_to_string(pArg,&n);` |
|   113 | 1230 | `	if( n == 1 ){` |
|    81 | 1231 | `		*pChar = zPtr[0];` |
|    81 | 1232 | `		return PH7_OK;` |
|     - | 1233 | `	}` |
|    33 | 1234 | `	if( n < 1 && bAllowEmpty ){` |
|     7 | 1235 | `		*pChar = PH7_CSV_NO_ESCAPE;` |
|     7 | 1236 | `		return PH7_OK;` |
|     - | 1237 | `	}` |
|    40 | 1238 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1239 | `		"%s(): Argument #%d ($%s) must be %sa single character",` |
|    13 | 1240 | `		ph7_function_name(pCtx),iArg,zName,bAllowEmpty ? "empty or " : ""` |
|     - | 1241 | `		);` |
|    57 | 1242 | `}` |
|     - | 1243 | `/*` |
|     - | 1244 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|     - | 1245 | ` * All the processed input is insereted into an array passed as the last` |
|     - | 1246 | ` * argument to this callback.` |
|     - | 1247 | ` */` |
|    34 | 1248 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|     1 | 1249 | `{` |
|    35 | 1250 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - | 1251 | `	ph7_value sEntry;` |
|     - | 1252 | `	SyString sToken;` |
|     - | 1253 | `	/* Insert the token in the given array */` |
|    35 | 1254 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|     - | 1255 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|    77 | 1256 | `	SyStringFullTrimSafe(&sToken);` |
|    35 | 1257 | `	if( sToken.nByte < 1){` |
|   ! 0 | 1258 | `		return SXRET_OK;` |
|     - | 1259 | `	}` |
|    35 | 1260 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|    35 | 1261 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|    35 | 1262 | `	PH7_MemObjRelease(&sEntry);` |
|    35 | 1263 | `	return SXRET_OK;` |
|    18 | 1264 | `}` |
|     - | 1265 | `/*` |
|     - | 1266 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|     - | 1267 | ` *  Parse a CSV string into an array.` |
|     - | 1268 | ` * Parameters` |
|     - | 1269 | ` *  $input` |
|     - | 1270 | ` *   The string to parse.` |
|     - | 1271 | ` *  $delimiter` |
|     - | 1272 | ` *   Set the field delimiter (one character only).` |
|     - | 1273 | ` *  $enclosure` |
|     - | 1274 | ` *   Set the field enclosure character (one character only).` |
|     - | 1275 | ` *  $escape` |
|     - | 1276 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|     - | 1277 | ` * Return` |
|     - | 1278 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|     - | 1279 | ` */` |
|    16 | 1280 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1281 | `{` |
|     - | 1282 | `	const char *zInput;` |
|     - | 1283 | `	ph7_value *pArray;` |
|    17 | 1284 | `	int delim  = ',';   /* Delimiter */` |
|    17 | 1285 | `	int encl   = '"' ;  /* Enclosure */` |
|    17 | 1286 | `	int escape = '\\';  /* Escape character */` |
|     - | 1287 | `	int nLen;` |
|    17 | 1288 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1289 | `		/* Missing/Invalid arguments,return NULL */` |
|   ! 0 | 1290 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1291 | `		return PH7_OK;` |
|     - | 1292 | `	}` |
|     - | 1293 | `	/* Extract the raw input */` |
|    17 | 1294 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1295 | `	if( nArg > 1 ){` |
|    17 | 1296 | `		sxi32 rc = PH7_CsvCharArg(pCtx,apArg[1],2,"separator",0,&delim);` |
|    17 | 1297 | `		if( rc != PH7_OK ){` |
|     5 | 1298 | `			return rc;` |
|     - | 1299 | `		}` |
|    13 | 1300 | `		if( nArg > 2 ){` |
|    13 | 1301 | `			rc = PH7_CsvCharArg(pCtx,apArg[2],3,"enclosure",0,&encl);` |
|    13 | 1302 | `			if( rc != PH7_OK ){` |
|     5 | 1303 | `				return rc;` |
|     - | 1304 | `			}` |
|     9 | 1305 | `			if( nArg > 3 ){` |
|     9 | 1306 | `				rc = PH7_CsvCharArg(pCtx,apArg[3],4,"escape",1,&escape);` |
|     9 | 1307 | `				if( rc != PH7_OK ){` |
|     3 | 1308 | `					return rc;` |
|     - | 1309 | `				}` |
|     3 | 1310 | `			}` |
|     3 | 1311 | `		}` |
|     3 | 1312 | `	}` |
|     - | 1313 | `	/* Create our array */` |
|     7 | 1314 | `	pArray = ph7_context_new_array(pCtx);` |
|     7 | 1315 | `	if( pArray == 0 ){` |
|     - | 1316 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|   ! 0 | 1317 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1318 | `	}` |
|     - | 1319 | `	/* Parse the raw input */` |
|     7 | 1320 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|     - | 1321 | `	/* Return the freshly created array */` |
|     7 | 1322 | `	ph7_result_value(pCtx,pArray);` |
|     7 | 1323 | `	return PH7_OK;` |
|     9 | 1324 | `}` |
|     - | 1325 | `/*` |
|     - | 1326 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|     - | 1327 | ` * container.` |
|     - | 1328 | ` * Refer to [strip_tags()].` |
|     - | 1329 | ` */` |
|    10 | 1330 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|     1 | 1331 | `{` |
|    11 | 1332 | `	const char *zEnd = &zTag[nByte];` |
|     - | 1333 | `	const char *zPtr;` |
|     - | 1334 | `	SyString sEntry;` |
|     - | 1335 | `	/* Strip tags */` |
|    10 | 1336 | `	for(;;){` |
|    45 | 1337 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|    14 | 1338 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|    15 | 1339 | `				zTag++;` |
|     1 | 1340 | `		}` |
|    21 | 1341 | `		if( zTag >= zEnd ){` |
|    11 | 1342 | `			break;` |
|     - | 1343 | `		}` |
|    11 | 1344 | `		zPtr = zTag;` |
|     - | 1345 | `		/* Delimit the tag */` |
|    25 | 1346 | `		while(zTag < zEnd ){` |
|    25 | 1347 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|     - | 1348 | `				/* UTF-8 stream */` |
|     3 | 1349 | `				zTag++;` |
|     5 | 1350 | `				SX_JMP_UTF8(zTag,zEnd);` |
|    24 | 1351 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|    11 | 1352 | `				break;` |
|   ! 0 | 1353 | `			}else{` |
|    13 | 1354 | `				zTag++;` |
|     - | 1355 | `			}` |
|     1 | 1356 | `		}` |
|    11 | 1357 | `		if( zTag > zPtr ){` |
|     - | 1358 | `			/* Perform the insertion */` |
|    11 | 1359 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|    11 | 1360 | `			SyStringFullTrim(&sEntry);` |
|    11 | 1361 | `			SySetPut(pSet,(const void *)&sEntry);` |
|     5 | 1362 | `		}` |
|     - | 1363 | `		/* Jump the trailing '>' */` |
|    11 | 1364 | `		zTag++;` |
|     1 | 1365 | `	}` |
|    11 | 1366 | `	return SXRET_OK;` |
|     1 | 1367 | `}` |
|     - | 1368 | `/*` |
|     - | 1369 | ` * Check if the given HTML tag name is present in the given container.` |
|     - | 1370 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|     - | 1371 | ` * Refer to [strip_tags()].` |
|     - | 1372 | ` */` |
|    36 | 1373 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|     1 | 1374 | `{` |
|    37 | 1375 | `	if( SySetUsed(pSet) > 0 ){` |
|    25 | 1376 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|     - | 1377 | `		SyString sTag;` |
|    85 | 1378 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|    24 | 1379 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|    37 | 1380 | `			zTag++;` |
|     1 | 1381 | `		}` |
|     - | 1382 | `		/* Delimit the tag */` |
|    25 | 1383 | `		zCur = zTag;` |
|    77 | 1384 | `		while(zTag < zEnd ){` |
|    77 | 1385 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|     - | 1386 | `				/* UTF-8 stream */` |
|     5 | 1387 | `				zTag++;` |
|     9 | 1388 | `				SX_JMP_UTF8(zTag,zEnd);` |
|    75 | 1389 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|    25 | 1390 | `				break;` |
|   ! 0 | 1391 | `			}else{` |
|    49 | 1392 | `				zTag++;` |
|     - | 1393 | `			}` |
|     1 | 1394 | `		}` |
|    25 | 1395 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|     - | 1396 | `		/* Trim leading white spaces and null bytes */` |
|    35 | 1397 | `		SyStringLeftTrimSafe(&sTag);` |
|    25 | 1398 | `		if( sTag.nByte > 0 ){` |
|     - | 1399 | `			SyString *aEntry,*pEntry;` |
|     - | 1400 | `			sxi32 rc;` |
|     - | 1401 | `			sxu32 n;` |
|     - | 1402 | `			/* Perform the lookup */` |
|    25 | 1403 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|    29 | 1404 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|    25 | 1405 | `				pEntry = &aEntry[n];` |
|     - | 1406 | `				/* Do the comparison */` |
|    25 | 1407 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|    25 | 1408 | `				if( !rc ){` |
|    21 | 1409 | `					return SXRET_OK;` |
|     - | 1410 | `				}` |
|     3 | 1411 | `			}` |
|     2 | 1412 | `		}` |
|     2 | 1413 | `	}` |
|     - | 1414 | `	/* No such tag */` |
|    17 | 1415 | `	return SXERR_NOTFOUND;` |
|    19 | 1416 | `}` |
|     - | 1417 | `/*` |
|     - | 1418 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|     - | 1419 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|     - | 1420 | ` * Refer to [strip_tags()].` |
|     - | 1421 | ` */` |
|    18 | 1422 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|     2 | 1423 | `{` |
|    20 | 1424 | `	const char *zEnd = &zIn[nByte];` |
|     - | 1425 | `	const char *zPtr,*zTag;` |
|     - | 1426 | `	SySet sSet;` |
|     - | 1427 | `	/* initialize the set of allowed tags */` |
|    20 | 1428 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    20 | 1429 | `	if( nTaglen > 0 ){` |
|     - | 1430 | `		/* Set of allowed tags */` |
|    11 | 1431 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|     5 | 1432 | `	}` |
|     - | 1433 | `	/* Set the empty string */` |
|    20 | 1434 | `	ph7_result_string(pCtx,"",0);` |
|     - | 1435 | `	/* Start processing */` |
|    27 | 1436 | `	for(;;){` |
|    56 | 1437 | `		if(zIn >= zEnd){` |
|     - | 1438 | `			/* No more input to process */` |
|    18 | 1439 | `			break;` |
|     - | 1440 | `		}` |
|    39 | 1441 | `		zPtr = zIn;` |
|     - | 1442 | `		/* Find a tag */` |
|   133 | 1443 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|    95 | 1444 | `			zIn++;` |
|     1 | 1445 | `		}` |
|    39 | 1446 | `		if( zIn > zPtr ){` |
|     - | 1447 | `			/* Consume raw input */` |
|    21 | 1448 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|    10 | 1449 | `		}` |
|     - | 1450 | `		/* Ignore trailing null bytes */` |
|    39 | 1451 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|   ! 0 | 1452 | `			zIn++;` |
|   ! 0 | 1453 | `		}` |
|    39 | 1454 | `		if(zIn >= zEnd){` |
|     - | 1455 | `			/* No more input to process */` |
|     3 | 1456 | `			break;` |
|     - | 1457 | `		}` |
|    37 | 1458 | `		if( zIn[0] == '<' ){` |
|     - | 1459 | `			sxi32 rc;` |
|    37 | 1460 | `			zTag = zIn++;` |
|     - | 1461 | `			/* Delimit the tag */` |
|   127 | 1462 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|    91 | 1463 | `				zIn++;` |
|     1 | 1464 | `			}` |
|    37 | 1465 | `			if( zIn < zEnd ){` |
|    37 | 1466 | `				zIn++; /* Ignore the trailing closing tag */` |
|    18 | 1467 | `			}` |
|     - | 1468 | `			/* Query the set */` |
|    37 | 1469 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|    37 | 1470 | `			if( rc == SXRET_OK ){` |
|     - | 1471 | `				/* Keep the tag */` |
|    21 | 1472 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|    10 | 1473 | `			}` |
|    18 | 1474 | `		}` |
|     1 | 1475 | `	}` |
|     - | 1476 | `	/* Cleanup */` |
|    20 | 1477 | `	SySetRelease(&sSet);` |
|    20 | 1478 | `	return SXRET_OK;` |
|     2 | 1479 | `}` |
|     - | 1480 | `/*` |
|     - | 1481 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|     - | 1482 | ` *   Strip HTML and PHP tags from a string.` |
|     - | 1483 | ` * Parameters` |
|     - | 1484 | ` *  $str` |
|     - | 1485 | ` *  The input string.` |
|     - | 1486 | ` * $allowable_tags` |
|     - | 1487 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|     - | 1488 | ` * Return` |
|     - | 1489 | ` *  Returns the stripped string.` |
|     - | 1490 | ` */` |
|    16 | 1491 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1492 | `{` |
|    18 | 1493 | `	const char *zTaglist = 0;` |
|     - | 1494 | `	const char *zString;` |
|    18 | 1495 | `	int nTaglen = 0;` |
|     - | 1496 | `	int nLen;` |
|    18 | 1497 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1498 | `		/* Missing/Invalid arguments,return the empty string */` |
|   ! 0 | 1499 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1500 | `		return PH7_OK;` |
|     - | 1501 | `	}` |
|     - | 1502 | `	/* Point to the raw string */` |
|    18 | 1503 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    18 | 1504 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|     - | 1505 | `		/* Allowed tag */` |
|    11 | 1506 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|     5 | 1507 | `	}` |
|     - | 1508 | `	/* Process input */` |
|    18 | 1509 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|    18 | 1510 | `	return PH7_OK;` |
|    10 | 1511 | `}` |
|     - | 1512 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1513 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - | 1514 | `/*` |
|     - | 1515 | ` * Parse an INI string.` |
|     - | 1516 |  |
|     - | 1517 | ` * According to wikipedia` |
|     - | 1518 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|     - | 1519 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|     - | 1520 | ` *  Format` |
|     - | 1521 | `*    Properties` |
|     - | 1522 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|     - | 1523 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|     - | 1524 | `*     Example:` |
|     - | 1525 | `*      name=value` |
|     - | 1526 | `*    Sections` |
|     - | 1527 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|     - | 1528 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|     - | 1529 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|     - | 1530 | `*     or the end of the file. Sections may not be nested.` |
|     - | 1531 | `*     Example:` |
|     - | 1532 | `*      [section]` |
|     - | 1533 | `*   Comments` |
|     - | 1534 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|     - | 1535 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|     - | 1536 | `*/` |
|    12 | 1537 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|     1 | 1538 | `{` |
|     - | 1539 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|    13 | 1540 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - | 1541 | `	SyHashEntry *pEntry;` |
|     - | 1542 | `	SyString sEntry;` |
|     - | 1543 | `	SyHash sHash;` |
|     - | 1544 | `	int c;` |
|     - | 1545 | `	/* Create an empty array and worker variables */` |
|    13 | 1546 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 | 1547 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    13 | 1548 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    13 | 1549 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|     - | 1550 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|   ! 0 | 1551 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1552 | `	}` |
|    13 | 1553 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|    13 | 1554 | `	pCur = pArray;` |
|     - | 1555 | `	/* Start the parse process */` |
|    21 | 1556 | `	for(;;){` |
|     - | 1557 | `		/* Ignore leading white spaces */` |
|    69 | 1558 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|    27 | 1559 | `			zIn++;` |
|     1 | 1560 | `		}` |
|    43 | 1561 | `		if( zIn >= zEnd ){` |
|     - | 1562 | `			/* No more input to process */` |
|    13 | 1563 | `			break;` |
|     - | 1564 | `		}` |
|    31 | 1565 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1566 | `			/* Comment til the end of line */` |
|   ! 0 | 1567 | `			zIn++;` |
|   ! 0 | 1568 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|   ! 0 | 1569 | `				zIn++;` |
|   ! 0 | 1570 | `			}` |
|   ! 0 | 1571 | `			continue;` |
|     - | 1572 | `		}` |
|     - | 1573 | `		/* Reset the string cursor of the working variable */` |
|    31 | 1574 | `		ph7_value_reset_string_cursor(pWorker);` |
|    31 | 1575 | `		if( zIn[0] == '[' ){` |
|     - | 1576 | `			/* Section: Extract the section name */` |
|     9 | 1577 | `			zIn++;` |
|     9 | 1578 | `			zCur = zIn;` |
|    73 | 1579 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|    65 | 1580 | `				zIn++;` |
|     1 | 1581 | `			}` |
|     9 | 1582 | `			if( zIn > zCur && bProcessSection ){` |
|     - | 1583 | `				/* Save the section name */` |
|     5 | 1584 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     5 | 1585 | `				SyStringFullTrim(&sEntry);` |
|     5 | 1586 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     5 | 1587 | `				if( sEntry.nByte > 0 ){` |
|     - | 1588 | `					/* Associate an array with the section */` |
|     5 | 1589 | `					pSection = ph7_context_new_array(pCtx);` |
|     5 | 1590 | `					if( pSection ){` |
|     5 | 1591 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|     5 | 1592 | `						pCur = pSection;` |
|     2 | 1593 | `					}` |
|     2 | 1594 | `				}` |
|     2 | 1595 | `			}` |
|     9 | 1596 | `			zIn++; /* Trailing square brackets ']' */` |
|     5 | 1597 | `		}else{` |
|     - | 1598 | `			ph7_value *pOldCur;` |
|     - | 1599 | `			int is_array;` |
|     - | 1600 | `			int iLen;` |
|     - | 1601 | `			/* Properties */` |
|    23 | 1602 | `			is_array = 0;` |
|    23 | 1603 | `			zCur = zIn;` |
|    23 | 1604 | `			iLen = 0; /* cc warning */` |
|    23 | 1605 | `			pOldCur = pCur;` |
|   155 | 1606 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|   133 | 1607 | `				if( zIn[0] == '[' && !is_array ){` |
|     - | 1608 | `					/* Array */` |
|   ! 0 | 1609 | `					iLen = (int)(zIn-zCur);` |
|   ! 0 | 1610 | `					is_array = 1;` |
|   ! 0 | 1611 | `					if( iLen > 0 ){` |
|   ! 0 | 1612 | `						ph7_value *pvArr = 0; /* cc warning */` |
|     - | 1613 | `						/* Query the hashtable */` |
|   ! 0 | 1614 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   ! 0 | 1615 | `						SyStringFullTrim(&sEntry);` |
|   ! 0 | 1616 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|   ! 0 | 1617 | `						if( pEntry ){` |
|   ! 0 | 1618 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|   ! 0 | 1619 | `						}else{` |
|     - | 1620 | `							/* Create an empty array */` |
|   ! 0 | 1621 | `							pvArr = ph7_context_new_array(pCtx);` |
|   ! 0 | 1622 | `							if( pvArr ){` |
|     - | 1623 | `								/* Save the entry */` |
|   ! 0 | 1624 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|     - | 1625 | `								/* Insert the entry */` |
|   ! 0 | 1626 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1627 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|   ! 0 | 1628 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|   ! 0 | 1629 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1630 | `							}` |
|     - | 1631 | `						}` |
|   ! 0 | 1632 | `						if( pvArr ){` |
|   ! 0 | 1633 | `							pCur = pvArr;` |
|   ! 0 | 1634 | `						}` |
|   ! 0 | 1635 | `					}` |
|   ! 0 | 1636 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|   ! 0 | 1637 | `						zIn++;` |
|   ! 0 | 1638 | `					}` |
|   ! 0 | 1639 | `				}` |
|   133 | 1640 | `				zIn++;` |
|     1 | 1641 | `			}` |
|    23 | 1642 | `			if( !is_array ){` |
|    23 | 1643 | `				iLen = (int)(zIn-zCur);` |
|    11 | 1644 | `			}` |
|     - | 1645 | `			/* Trim the key */` |
|    23 | 1646 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    39 | 1647 | `			SyStringFullTrim(&sEntry);` |
|    23 | 1648 | `			if( sEntry.nByte > 0 ){` |
|    23 | 1649 | `				if( !is_array ){` |
|     - | 1650 | `					/* Save the key name */` |
|    23 | 1651 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    11 | 1652 | `				}` |
|     - | 1653 | `				/* extract key value */` |
|    23 | 1654 | `				ph7_value_reset_string_cursor(pValue);` |
|    23 | 1655 | `				zIn++; /* '=' */` |
|    39 | 1656 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    17 | 1657 | `					zIn++;` |
|     1 | 1658 | `				}` |
|    23 | 1659 | `				if( zIn < zEnd ){` |
|    21 | 1660 | `					zCur = zIn;` |
|    21 | 1661 | `					c = zIn[0];` |
|    21 | 1662 | `					if( c == '"' \|\| c == '\'' ){` |
|   ! 0 | 1663 | `						zIn++;` |
|     - | 1664 | `						/* Delimit the value */` |
|   ! 0 | 1665 | `						while( zIn < zEnd ){` |
|   ! 0 | 1666 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|   ! 0 | 1667 | `								break;` |
|     - | 1668 | `							}` |
|   ! 0 | 1669 | `							zIn++;` |
|   ! 0 | 1670 | `						}` |
|   ! 0 | 1671 | `						if( zIn < zEnd ){` |
|   ! 0 | 1672 | `							zIn++;` |
|   ! 0 | 1673 | `						}` |
|   ! 0 | 1674 | `					}else{` |
|   125 | 1675 | `						while( zIn < zEnd ){` |
|   123 | 1676 | `							if( zIn[0] == '\n' ){` |
|    19 | 1677 | `								if( zIn[-1] != '\\' ){` |
|    19 | 1678 | `									break;` |
|   ! 0 | 1679 | `								}` |
|   105 | 1680 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1681 | `								/* Inline comments */` |
|   ! 0 | 1682 | `								break;` |
|     - | 1683 | `							}` |
|   105 | 1684 | `							zIn++;` |
|     1 | 1685 | `						}` |
|     - | 1686 | `					}` |
|     - | 1687 | `					/* Trim the value */` |
|    21 | 1688 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|    21 | 1689 | `					SyStringFullTrim(&sEntry);` |
|    21 | 1690 | `					if( c == '"' \|\| c == '\'' ){` |
|   ! 0 | 1691 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|   ! 0 | 1692 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|   ! 0 | 1693 | `					}` |
|    21 | 1694 | `					if( sEntry.nByte > 0 ){` |
|    21 | 1695 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|    10 | 1696 | `					}` |
|     - | 1697 | `					/* Insert the key and it's value */` |
|    21 | 1698 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|    10 | 1699 | `				}` |
|    12 | 1700 | `			}else{` |
|   ! 0 | 1701 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|   ! 0 | 1702 | `					zIn++;` |
|   ! 0 | 1703 | `				}` |
|     - | 1704 | `			}` |
|    23 | 1705 | `			pCur = pOldCur;` |
|     - | 1706 | `		}` |
|     1 | 1707 | `	}` |
|    13 | 1708 | `	SyHashRelease(&sHash);` |
|     - | 1709 | `	/* Return the parse of the INI string */` |
|    13 | 1710 | `	ph7_result_value(pCtx,pArray);` |
|    13 | 1711 | `	return SXRET_OK;` |
|     7 | 1712 | `}` |
|     - | 1713 | `/*` |
|     - | 1714 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|     - | 1715 | ` *  Parse a configuration string.` |
|     - | 1716 | ` * Parameters` |
|     - | 1717 | ` *  $ini` |
|     - | 1718 | ` *   The contents of the ini file being parsed.` |
|     - | 1719 | ` *  $process_sections` |
|     - | 1720 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|     - | 1721 | ` *   and settings included. The default for process_sections is FALSE.` |
|     - | 1722 | ` *  $scanner_mode (Not used)` |
|     - | 1723 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|     - | 1724 | ` *   then option values will not be parsed.` |
|     - | 1725 | ` * Return` |
|     - | 1726 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|     - | 1727 | ` */` |
|    10 | 1728 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1729 | `{` |
|     - | 1730 | `	const char *zIni;` |
|     - | 1731 | `	int nByte;` |
|    11 | 1732 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1733 | `		/* Missing/Invalid arguments,return FALSE*/` |
|   ! 0 | 1734 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1735 | `		return PH7_OK;` |
|     - | 1736 | `	}` |
|     - | 1737 | `	/* Extract the raw INI buffer */` |
|    11 | 1738 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|     - | 1739 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|    11 | 1740 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     6 | 1741 | `}` |
|     - | 1742 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1743 | `#ifdef PH7_NEED_BUILTIN_REG` |
|     - | 1744 |  |
|     - | 1745 | `/*` |
|     - | 1746 | ` * Ctype Functions.` |
|     - | 1747 | ` * Status:` |
|     - | 1748 | ` *    Stable.` |
|     - | 1749 | ` */` |
|     - | 1750 | `/*` |
|     - | 1751 | ` * bool ctype_alnum(string $text)` |
|     - | 1752 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|     - | 1753 | ` * Parameters` |
|     - | 1754 | ` *  $text` |
|     - | 1755 | ` *   The tested string.` |
|     - | 1756 | ` * Return` |
|     - | 1757 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|     - | 1758 | ` */` |
|    72 | 1759 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1760 | `{` |
|     - | 1761 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1762 | `	int nLen;` |
|    73 | 1763 | `	if( nArg < 1 ){` |
|     - | 1764 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1765 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1766 | `		return PH7_OK;` |
|     - | 1767 | `	}` |
|     - | 1768 | `	/* Extract the target string */` |
|    73 | 1769 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    73 | 1770 | `	zEnd = &zIn[nLen];` |
|    73 | 1771 | `	if( nLen < 1 ){` |
|     - | 1772 | `		/* Empty string,return FALSE */` |
|     3 | 1773 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1774 | `		return PH7_OK;` |
|     - | 1775 | `	}` |
|     - | 1776 | `	/* Perform the requested operation */` |
|   110 | 1777 | `	for(;;){` |
|   221 | 1778 | `		if( zIn >= zEnd ){` |
|     - | 1779 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    65 | 1780 | `			ph7_result_bool(pCtx,1);` |
|    65 | 1781 | `			return PH7_OK;` |
|     - | 1782 | `		}` |
|   157 | 1783 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|     7 | 1784 | `			break;` |
|     - | 1785 | `		}` |
|     - | 1786 | `		/* Point to the next character */` |
|   151 | 1787 | `		zIn++;` |
|     1 | 1788 | `	}` |
|     - | 1789 | `	/* The test failed,return FALSE */` |
|     7 | 1790 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1791 | `	return PH7_OK;` |
|    37 | 1792 | `}` |
|     - | 1793 | `/*` |
|     - | 1794 | ` * bool ctype_alpha(string $text)` |
|     - | 1795 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|     - | 1796 | ` * Parameters` |
|     - | 1797 | ` *  $text` |
|     - | 1798 | ` *   The tested string.` |
|     - | 1799 | ` * Return` |
|     - | 1800 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|     - | 1801 | ` */` |
|    16 | 1802 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1803 | `{` |
|     - | 1804 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1805 | `	int nLen;` |
|    17 | 1806 | `	if( nArg < 1 ){` |
|     - | 1807 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1808 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1809 | `		return PH7_OK;` |
|     - | 1810 | `	}` |
|     - | 1811 | `	/* Extract the target string */` |
|    17 | 1812 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1813 | `	zEnd = &zIn[nLen];` |
|    17 | 1814 | `	if( nLen < 1 ){` |
|     - | 1815 | `		/* Empty string,return FALSE */` |
|     3 | 1816 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1817 | `		return PH7_OK;` |
|     - | 1818 | `	}` |
|     - | 1819 | `	/* Perform the requested operation */` |
|    42 | 1820 | `	for(;;){` |
|    85 | 1821 | `		if( zIn >= zEnd ){` |
|     - | 1822 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 1823 | `			ph7_result_bool(pCtx,1);` |
|     9 | 1824 | `			return PH7_OK;` |
|     - | 1825 | `		}` |
|    77 | 1826 | `		if( !SyisAlpha(zIn[0]) ){` |
|     7 | 1827 | `			break;` |
|     - | 1828 | `		}` |
|     - | 1829 | `		/* Point to the next character */` |
|    71 | 1830 | `		zIn++;` |
|     1 | 1831 | `	}` |
|     - | 1832 | `	/* The test failed,return FALSE */` |
|     7 | 1833 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1834 | `	return PH7_OK;` |
|     9 | 1835 | `}` |
|     - | 1836 | `/*` |
|     - | 1837 | ` * bool ctype_cntrl(string $text)` |
|     - | 1838 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|     - | 1839 | ` * Parameters` |
|     - | 1840 | ` *  $text` |
|     - | 1841 | ` *   The tested string.` |
|     - | 1842 | ` * Return` |
|     - | 1843 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|     - | 1844 | ` */` |
|    16 | 1845 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1846 | `{` |
|     - | 1847 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1848 | `	int nLen;` |
|    17 | 1849 | `	if( nArg < 1 ){` |
|     - | 1850 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1851 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1852 | `		return PH7_OK;` |
|     - | 1853 | `	}` |
|     - | 1854 | `	/* Extract the target string */` |
|    17 | 1855 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1856 | `	zEnd = &zIn[nLen];` |
|    17 | 1857 | `	if( nLen < 1 ){` |
|     - | 1858 | `		/* Empty string,return FALSE */` |
|     3 | 1859 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1860 | `		return PH7_OK;` |
|     - | 1861 | `	}` |
|     - | 1862 | `	/* Perform the requested operation */` |
|    14 | 1863 | `	for(;;){` |
|    29 | 1864 | `		if( zIn >= zEnd ){` |
|     - | 1865 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 1866 | `			ph7_result_bool(pCtx,1);` |
|     9 | 1867 | `			return PH7_OK;` |
|     - | 1868 | `		}` |
|    21 | 1869 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1870 | `			/* UTF-8 stream  */` |
|   ! 0 | 1871 | `			break;` |
|     - | 1872 | `		}` |
|    21 | 1873 | `		if( !SyisCtrl(zIn[0]) ){` |
|     7 | 1874 | `			break;` |
|     - | 1875 | `		}` |
|     - | 1876 | `		/* Point to the next character */` |
|    15 | 1877 | `		zIn++;` |
|     1 | 1878 | `	}` |
|     - | 1879 | `	/* The test failed,return FALSE */` |
|     7 | 1880 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1881 | `	return PH7_OK;` |
|     9 | 1882 | `}` |
|     - | 1883 | `/*` |
|     - | 1884 | ` * bool ctype_digit(string $text)` |
|     - | 1885 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|     - | 1886 | ` * Parameters` |
|     - | 1887 | ` *  $text` |
|     - | 1888 | ` *   The tested string.` |
|     - | 1889 | ` * Return` |
|     - | 1890 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|     - | 1891 | ` */` |
|  2060 | 1892 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1893 | `{` |
|     - | 1894 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1895 | `	int nLen;` |
|  2065 | 1896 | `	if( nArg < 1 ){` |
|     - | 1897 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1898 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1899 | `		return PH7_OK;` |
|     - | 1900 | `	}` |
|     - | 1901 | `	/* Extract the target string */` |
|  2065 | 1902 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  2065 | 1903 | `	zEnd = &zIn[nLen];` |
|  2065 | 1904 | `	if( nLen < 1 ){` |
|     - | 1905 | `		/* Empty string,return FALSE */` |
|     9 | 1906 | `		ph7_result_bool(pCtx,0);` |
|     9 | 1907 | `		return PH7_OK;` |
|     - | 1908 | `	}` |
|     - | 1909 | `	/* Perform the requested operation */` |
|  1859 | 1910 | `	for(;;){` |
|  3723 | 1911 | `		if( zIn >= zEnd ){` |
|     - | 1912 | `			/* If we reach the end of the string,then the test succeeded. */` |
|  1599 | 1913 | `			ph7_result_bool(pCtx,1);` |
|  1599 | 1914 | `			return PH7_OK;` |
|     - | 1915 | `		}` |
|  2129 | 1916 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1917 | `			/* UTF-8 stream  */` |
|   ! 0 | 1918 | `			break;` |
|     - | 1919 | `		}` |
|  2129 | 1920 | `		if( !SyisDigit(zIn[0]) ){` |
|   463 | 1921 | `			break;` |
|     - | 1922 | `		}` |
|     - | 1923 | `		/* Point to the next character */` |
|  1671 | 1924 | `		zIn++;` |
|     5 | 1925 | `	}` |
|     - | 1926 | `	/* The test failed,return FALSE */` |
|   463 | 1927 | `	ph7_result_bool(pCtx,0);` |
|   463 | 1928 | `	return PH7_OK;` |
|  1035 | 1929 | `}` |
|     - | 1930 | `/*` |
|     - | 1931 | ` * bool ctype_xdigit(string $text)` |
|     - | 1932 | ` *  Check for character(s) representing a hexadecimal digit.` |
|     - | 1933 | ` * Parameters` |
|     - | 1934 | ` *  $text` |
|     - | 1935 | ` *   The tested string.` |
|     - | 1936 | ` * Return` |
|     - | 1937 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|     - | 1938 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|     - | 1939 | ` */` |
|    44 | 1940 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1941 | `{` |
|     - | 1942 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1943 | `	int nLen;` |
|    46 | 1944 | `	if( nArg < 1 ){` |
|     - | 1945 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1946 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1947 | `		return PH7_OK;` |
|     - | 1948 | `	}` |
|     - | 1949 | `	/* Extract the target string */` |
|    46 | 1950 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    46 | 1951 | `	zEnd = &zIn[nLen];` |
|    46 | 1952 | `	if( nLen < 1 ){` |
|     - | 1953 | `		/* Empty string,return FALSE */` |
|     3 | 1954 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1955 | `		return PH7_OK;` |
|     - | 1956 | `	}` |
|     - | 1957 | `	/* Perform the requested operation */` |
|    85 | 1958 | `	for(;;){` |
|   172 | 1959 | `		if( zIn >= zEnd ){` |
|     - | 1960 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    38 | 1961 | `			ph7_result_bool(pCtx,1);` |
|    38 | 1962 | `			return PH7_OK;` |
|     - | 1963 | `		}` |
|   136 | 1964 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1965 | `			/* UTF-8 stream  */` |
|   ! 0 | 1966 | `			break;` |
|     - | 1967 | `		}` |
|   136 | 1968 | `		if( !SyisHex(zIn[0]) ){` |
|     7 | 1969 | `			break;` |
|     - | 1970 | `		}` |
|     - | 1971 | `		/* Point to the next character */` |
|   130 | 1972 | `		zIn++;` |
|     2 | 1973 | `	}` |
|     - | 1974 | `	/* The test failed,return FALSE */` |
|     7 | 1975 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1976 | `	return PH7_OK;` |
|    24 | 1977 | `}` |
|     - | 1978 | `/*` |
|     - | 1979 | ` * bool ctype_graph(string $text)` |
|     - | 1980 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|     - | 1981 | ` * Parameters` |
|     - | 1982 | ` *  $text` |
|     - | 1983 | ` *   The tested string.` |
|     - | 1984 | ` * Return` |
|     - | 1985 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|     - | 1986 | ` * (no white space), FALSE otherwise.` |
|     - | 1987 | ` */` |
|    16 | 1988 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1989 | `{` |
|     - | 1990 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1991 | `	int nLen;` |
|    17 | 1992 | `	if( nArg < 1 ){` |
|     - | 1993 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1994 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1995 | `		return PH7_OK;` |
|     - | 1996 | `	}` |
|     - | 1997 | `	/* Extract the target string */` |
|    17 | 1998 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1999 | `	zEnd = &zIn[nLen];` |
|    17 | 2000 | `	if( nLen < 1 ){` |
|     - | 2001 | `		/* Empty string,return FALSE */` |
|     3 | 2002 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2003 | `		return PH7_OK;` |
|     - | 2004 | `	}` |
|     - | 2005 | `	/* Perform the requested operation */` |
|    57 | 2006 | `	for(;;){` |
|   115 | 2007 | `		if( zIn >= zEnd ){` |
|     - | 2008 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2009 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2010 | `			return PH7_OK;` |
|     - | 2011 | `		}` |
|   107 | 2012 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2013 | `			/* UTF-8 stream  */` |
|   ! 0 | 2014 | `			break;` |
|     - | 2015 | `		}` |
|   107 | 2016 | `		if( !SyisGraph(zIn[0]) ){` |
|     7 | 2017 | `			break;` |
|     - | 2018 | `		}` |
|     - | 2019 | `		/* Point to the next character */` |
|   101 | 2020 | `		zIn++;` |
|     1 | 2021 | `	}` |
|     - | 2022 | `	/* The test failed,return FALSE */` |
|     7 | 2023 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2024 | `	return PH7_OK;` |
|     9 | 2025 | `}` |
|     - | 2026 | `/*` |
|     - | 2027 | ` * bool ctype_print(string $text)` |
|     - | 2028 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|     - | 2029 | ` * Parameters` |
|     - | 2030 | ` *  $text` |
|     - | 2031 | ` *   The tested string.` |
|     - | 2032 | ` * Return` |
|     - | 2033 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|     - | 2034 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|     - | 2035 | ` *  or control function at all.` |
|     - | 2036 | ` */` |
|    16 | 2037 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2038 | `{` |
|     - | 2039 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2040 | `	int nLen;` |
|    17 | 2041 | `	if( nArg < 1 ){` |
|     - | 2042 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2043 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2044 | `		return PH7_OK;` |
|     - | 2045 | `	}` |
|     - | 2046 | `	/* Extract the target string */` |
|    17 | 2047 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2048 | `	zEnd = &zIn[nLen];` |
|    17 | 2049 | `	if( nLen < 1 ){` |
|     - | 2050 | `		/* Empty string,return FALSE */` |
|     3 | 2051 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2052 | `		return PH7_OK;` |
|     - | 2053 | `	}` |
|     - | 2054 | `	/* Perform the requested operation */` |
|    63 | 2055 | `	for(;;){` |
|   127 | 2056 | `		if( zIn >= zEnd ){` |
|     - | 2057 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2058 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2059 | `			return PH7_OK;` |
|     - | 2060 | `		}` |
|   119 | 2061 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2062 | `			/* UTF-8 stream  */` |
|   ! 0 | 2063 | `			break;` |
|     - | 2064 | `		}` |
|   119 | 2065 | `		if( !SyisPrint(zIn[0]) ){` |
|     7 | 2066 | `			break;` |
|     - | 2067 | `		}` |
|     - | 2068 | `		/* Point to the next character */` |
|   113 | 2069 | `		zIn++;` |
|     1 | 2070 | `	}` |
|     - | 2071 | `	/* The test failed,return FALSE */` |
|     7 | 2072 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2073 | `	return PH7_OK;` |
|     9 | 2074 | `}` |
|     - | 2075 | `/*` |
|     - | 2076 | ` * bool ctype_punct(string $text)` |
|     - | 2077 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|     - | 2078 | ` * Parameters` |
|     - | 2079 | ` *  $text` |
|     - | 2080 | ` *   The tested string.` |
|     - | 2081 | ` * Return` |
|     - | 2082 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|     - | 2083 | ` *  digit or blank, FALSE otherwise.` |
|     - | 2084 | ` */` |
|    18 | 2085 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2086 | `{` |
|     - | 2087 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2088 | `	int nLen;` |
|    19 | 2089 | `	if( nArg < 1 ){` |
|     - | 2090 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2091 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2092 | `		return PH7_OK;` |
|     - | 2093 | `	}` |
|     - | 2094 | `	/* Extract the target string */` |
|    19 | 2095 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 2096 | `	zEnd = &zIn[nLen];` |
|    19 | 2097 | `	if( nLen < 1 ){` |
|     - | 2098 | `		/* Empty string,return FALSE */` |
|     3 | 2099 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2100 | `		return PH7_OK;` |
|     - | 2101 | `	}` |
|     - | 2102 | `	/* Perform the requested operation */` |
|    38 | 2103 | `	for(;;){` |
|    77 | 2104 | `		if( zIn >= zEnd ){` |
|     - | 2105 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2106 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2107 | `			return PH7_OK;` |
|     - | 2108 | `		}` |
|    69 | 2109 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2110 | `			/* UTF-8 stream  */` |
|   ! 0 | 2111 | `			break;` |
|     - | 2112 | `		}` |
|    69 | 2113 | `		if( !SyisPunct(zIn[0]) ){` |
|     9 | 2114 | `			break;` |
|     - | 2115 | `		}` |
|     - | 2116 | `		/* Point to the next character */` |
|    61 | 2117 | `		zIn++;` |
|     1 | 2118 | `	}` |
|     - | 2119 | `	/* The test failed,return FALSE */` |
|     9 | 2120 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2121 | `	return PH7_OK;` |
|    10 | 2122 | `}` |
|     - | 2123 | `/*` |
|     - | 2124 | ` * bool ctype_space(string $text)` |
|     - | 2125 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|     - | 2126 | ` * Parameters` |
|     - | 2127 | ` *  $text` |
|     - | 2128 | ` *   The tested string.` |
|     - | 2129 | ` * Return` |
|     - | 2130 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|     - | 2131 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|     - | 2132 | ` *  and form feed characters.` |
|     - | 2133 | ` */` |
| 36675 | 2134 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 2135 | `{` |
|     - | 2136 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2137 | `	int nLen;` |
| 36680 | 2138 | `	if( nArg < 1 ){` |
|     - | 2139 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2140 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2141 | `		return PH7_OK;` |
|     - | 2142 | `	}` |
|     - | 2143 | `	/* Extract the target string */` |
| 36680 | 2144 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
| 36680 | 2145 | `	zEnd = &zIn[nLen];` |
| 36680 | 2146 | `	if( nLen < 1 ){` |
|     - | 2147 | `		/* Empty string,return FALSE */` |
|     3 | 2148 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2149 | `		return PH7_OK;` |
|     - | 2150 | `	}` |
|     - | 2151 | `	/* Perform the requested operation */` |
| 18504 | 2152 | `	for(;;){` |
| 36712 | 2153 | `		if( zIn >= zEnd ){` |
|     - | 2154 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    11 | 2155 | `			ph7_result_bool(pCtx,1);` |
|    11 | 2156 | `			return PH7_OK;` |
|     - | 2157 | `		}` |
| 36702 | 2158 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2159 | `			/* UTF-8 stream  */` |
|   ! 0 | 2160 | `			break;` |
|     - | 2161 | `		}` |
| 36702 | 2162 | `		if( !SyisSpace(zIn[0]) ){` |
| 36668 | 2163 | `			break;` |
|     - | 2164 | `		}` |
|     - | 2165 | `		/* Point to the next character */` |
|    35 | 2166 | `		zIn++;` |
|     1 | 2167 | `	}` |
|     - | 2168 | `	/* The test failed,return FALSE */` |
| 36668 | 2169 | `	ph7_result_bool(pCtx,0);` |
| 36668 | 2170 | `	return PH7_OK;` |
| 18493 | 2171 | `}` |
|     - | 2172 | `/*` |
|     - | 2173 | ` * bool ctype_lower(string $text)` |
|     - | 2174 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|     - | 2175 | ` * Parameters` |
|     - | 2176 | ` *  $text` |
|     - | 2177 | ` *   The tested string.` |
|     - | 2178 | ` * Return` |
|     - | 2179 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|     - | 2180 | ` */` |
|    16 | 2181 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2182 | `{` |
|     - | 2183 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2184 | `	int nLen;` |
|    17 | 2185 | `	if( nArg < 1 ){` |
|     - | 2186 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2187 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2188 | `		return PH7_OK;` |
|     - | 2189 | `	}` |
|     - | 2190 | `	/* Extract the target string */` |
|    17 | 2191 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2192 | `	zEnd = &zIn[nLen];` |
|    17 | 2193 | `	if( nLen < 1 ){` |
|     - | 2194 | `		/* Empty string,return FALSE */` |
|     3 | 2195 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2196 | `		return PH7_OK;` |
|     - | 2197 | `	}` |
|     - | 2198 | `	/* Perform the requested operation */` |
|    27 | 2199 | `	for(;;){` |
|    55 | 2200 | `		if( zIn >= zEnd ){` |
|     - | 2201 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2202 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2203 | `			return PH7_OK;` |
|     - | 2204 | `		}` |
|    51 | 2205 | `		if( !SyisLower(zIn[0]) ){` |
|    11 | 2206 | `			break;` |
|     - | 2207 | `		}` |
|     - | 2208 | `		/* Point to the next character */` |
|    41 | 2209 | `		zIn++;` |
|     1 | 2210 | `	}` |
|     - | 2211 | `	/* The test failed,return FALSE */` |
|    11 | 2212 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2213 | `	return PH7_OK;` |
|     9 | 2214 | `}` |
|     - | 2215 | `/*` |
|     - | 2216 | ` * bool ctype_upper(string $text)` |
|     - | 2217 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|     - | 2218 | ` * Parameters` |
|     - | 2219 | ` *  $text` |
|     - | 2220 | ` *   The tested string.` |
|     - | 2221 | ` * Return` |
|     - | 2222 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|     - | 2223 | ` */` |
|    16 | 2224 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2225 | `{` |
|     - | 2226 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2227 | `	int nLen;` |
|    17 | 2228 | `	if( nArg < 1 ){` |
|     - | 2229 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2230 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2231 | `		return PH7_OK;` |
|     - | 2232 | `	}` |
|     - | 2233 | `	/* Extract the target string */` |
|    17 | 2234 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2235 | `	zEnd = &zIn[nLen];` |
|    17 | 2236 | `	if( nLen < 1 ){` |
|     - | 2237 | `		/* Empty string,return FALSE */` |
|     3 | 2238 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2239 | `		return PH7_OK;` |
|     - | 2240 | `	}` |
|     - | 2241 | `	/* Perform the requested operation */` |
|    28 | 2242 | `	for(;;){` |
|    57 | 2243 | `		if( zIn >= zEnd ){` |
|     - | 2244 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2245 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2246 | `			return PH7_OK;` |
|     - | 2247 | `		}` |
|    53 | 2248 | `		if( !SyisUpper(zIn[0]) ){` |
|    11 | 2249 | `			break;` |
|     - | 2250 | `		}` |
|     - | 2251 | `		/* Point to the next character */` |
|    43 | 2252 | `		zIn++;` |
|     1 | 2253 | `	}` |
|     - | 2254 | `	/* The test failed,return FALSE */` |
|    11 | 2255 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2256 | `	return PH7_OK;` |
|     9 | 2257 | `}` |
|     - | 2258 | `/* Date/Time functions moved to builtin_date.c */` |
|     - | 2259 | `/*` |
|     - | 2260 | ` * Section:` |
|     - | 2261 | ` *    URL handling Functions.` |
|     - | 2262 | ` * Status:` |
|     - | 2263 | ` *    Stable.` |
|     - | 2264 | ` */` |
|     - | 2265 | `/*` |
|     - | 2266 | ` * Output consumer callback for the standard Symisc routines.` |
|     - | 2267 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|     - | 2268 | ` */` |
|  2052 | 2269 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     3 | 2270 | `{` |
|     - | 2271 | `	/* Store in the call context result buffer */` |
|  2055 | 2272 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|  2055 | 2273 | `	return SXRET_OK;` |
|     3 | 2274 | `}` |
|     - | 2275 | `/*` |
|     - | 2276 | ` * string base64_encode(string $data)` |
|     - | 2277 | ` *  Encodes data with MIME base64` |
|     - | 2278 | ` * Parameter` |
|     - | 2279 | ` *  $data` |
|     - | 2280 | ` *    Data to encode` |
|     - | 2281 | ` * Return` |
|     - | 2282 | ` *  Encoded data or FALSE on failure.` |
|     - | 2283 | ` */` |
|    10 | 2284 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2285 | `{` |
|     - | 2286 | `	const char *zIn;` |
|     - | 2287 | `	int nLen;` |
|    12 | 2288 | `	if( nArg < 1 ){` |
|     - | 2289 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2290 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2291 | `		return PH7_OK;` |
|     - | 2292 | `	}` |
|     - | 2293 | `	/* Extract the input string */` |
|    12 | 2294 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    12 | 2295 | `	if( nLen < 1 ){` |
|     - | 2296 | `		/* php encodes the empty string to the EMPTY STRING; base64_encode() cannot` |
|     - | 2297 | `		 * fail at all, so FALSE was never one of its answers. */` |
|     3 | 2298 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 2299 | `		return PH7_OK;` |
|     - | 2300 | `	}` |
|     - | 2301 | `	/* Perform the BASE64 encoding */` |
|    10 | 2302 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    10 | 2303 | `	return PH7_OK;` |
|     7 | 2304 | `}` |
|     - | 2305 | `/*` |
|     - | 2306 | ` * php's base64 reverse table: -1 is skippable whitespace (\t \n \r and space,` |
|     - | 2307 | ` * exactly php's set -- \v/\f are NOT skipped), -2 is an invalid byte, 0..63 the` |
|     - | 2308 | ` * decoded 6-bit value. The pad byte '=' is handled before the lookup, so its` |
|     - | 2309 | ` * table slot is never consulted.` |
|     - | 2310 | ` */` |
|     - | 2311 | `static const signed char aB64Rev[256] = {` |
|     - | 2312 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-2,-2,-1,-2,-2,` |
|     - | 2313 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2314 | `	-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,62,-2,-2,-2,63,` |
|     - | 2315 | `	52,53,54,55,56,57,58,59,60,61,-2,-2,-2,-2,-2,-2,` |
|     - | 2316 | `	-2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 2317 | `	15,16,17,18,19,20,21,22,23,24,25,-2,-2,-2,-2,-2,` |
|     - | 2318 | `	-2,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 2319 | `	41,42,43,44,45,46,47,48,49,50,51,-2,-2,-2,-2,-2,` |
|     - | 2320 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2321 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2322 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2323 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2324 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2325 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2326 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2327 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2` |
|     - | 2328 | `};` |
|     - | 2329 | `/*` |
|     - | 2330 | ` * string base64_decode(string $data, bool $strict = false)` |
|     - | 2331 | ` *  Decodes data encoded with MIME base64` |
|     - | 2332 | ` * Parameters` |
|     - | 2333 | ` *  $data` |
|     - | 2334 | ` *    Encoded data.` |
|     - | 2335 | ` *  $strict` |
|     - | 2336 | ` *    When true, return FALSE if the input contains a character outside the` |
|     - | 2337 | ` *    base64 alphabet (whitespace is still skipped) or the padding/length is` |
|     - | 2338 | ` *    malformed. When false, such bytes are silently skipped (best effort).` |
|     - | 2339 | ` * Return` |
|     - | 2340 | ` *  Returns the original data or FALSE on failure.` |
|     - | 2341 | ` * Implementation note: a faithful port of php's php_base64_decode_ex(). The old` |
|     - | 2342 | ` * code ignored $strict entirely and ran the shared SyBase64Decode(), which maps` |
|     - | 2343 | ` * every non-alphabet byte (whitespace included) to 0 rather than skipping it --` |
|     - | 2344 | ` * a silent wrong answer on padded/whitespace input in BOTH modes.` |
|     - | 2345 | ` */` |
|    40 | 2346 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2347 | `{` |
|     - | 2348 | `	const unsigned char *zIn;` |
|     - | 2349 | `	unsigned char *zOut;` |
|    42 | 2350 | `	int nLen,strict = 0;` |
|    42 | 2351 | `	int i = 0,j = 0,padding = 0,k;` |
|    42 | 2352 | `	if( nArg < 1 ){` |
|     - | 2353 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2354 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2355 | `		return PH7_OK;` |
|     - | 2356 | `	}` |
|     - | 2357 | `	/* Extract the input string */` |
|    42 | 2358 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    42 | 2359 | `	if( nLen < 1 ){` |
|     - | 2360 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|     - | 2361 | `		 * for input that cannot be decoded at all). */` |
|     6 | 2362 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2363 | `		return PH7_OK;` |
|     - | 2364 | `	}` |
|    37 | 2365 | `	if( nArg > 1 ){` |
|    31 | 2366 | `		strict = ph7_value_to_bool(apArg[1]);` |
|    15 | 2367 | `	}` |
|     - | 2368 | `	/* Output is at most 3/4 of the input; nLen bytes is a safe upper bound. */` |
|    37 | 2369 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    37 | 2370 | `	if( zOut == 0 ){` |
|   ! 0 | 2371 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2372 | `	}` |
|   187 | 2373 | `	for( k = 0 ; k < nLen ; ++k ){` |
|   155 | 2374 | `		int ch = zIn[k];` |
|     - | 2375 | `		int val;` |
|   155 | 2376 | `		if( ch == '=' ){` |
|     - | 2377 | `			/* Pad byte: count it, decode nothing. */` |
|    23 | 2378 | `			padding++;` |
|    23 | 2379 | `			continue;` |
|     - | 2380 | `		}` |
|   133 | 2381 | `		val = aB64Rev[ch];` |
|   133 | 2382 | `		if( !strict ){` |
|     - | 2383 | `			/* Lenient: skip whitespace AND any invalid byte. */` |
|    61 | 2384 | `			if( val < 0 ){` |
|    15 | 2385 | `				continue;` |
|     - | 2386 | `			}` |
|    24 | 2387 | `		}else{` |
|    73 | 2388 | `			if( val == -1 ){` |
|     - | 2389 | `				/* Skippable whitespace. */` |
|     7 | 2390 | `				continue;` |
|     - | 2391 | `			}` |
|    67 | 2392 | `			if( val == -2 ){` |
|     - | 2393 | `				/* A byte outside the base64 alphabet. */` |
|     5 | 2394 | `				goto fail;` |
|     - | 2395 | `			}` |
|    63 | 2396 | `			if( padding ){` |
|     - | 2397 | `				/* Data must not follow the padding. */` |
|   ! 0 | 2398 | `				goto fail;` |
|     - | 2399 | `			}` |
|     - | 2400 | `		}` |
|   109 | 2401 | `		switch( i & 3 ){` |
|    20 | 2402 | `			case 0:` |
|    41 | 2403 | `				zOut[j] = (unsigned char)(val << 2);` |
|    41 | 2404 | `				break;` |
|    18 | 2405 | `			case 1:` |
|    37 | 2406 | `				zOut[j++] \|= (unsigned char)(val >> 4);` |
|    37 | 2407 | `				zOut[j] = (unsigned char)((val & 0x0F) << 4);` |
|    37 | 2408 | `				break;` |
|    11 | 2409 | `			case 2:` |
|    23 | 2410 | `				zOut[j++] \|= (unsigned char)(val >> 2);` |
|    23 | 2411 | `				zOut[j] = (unsigned char)((val & 0x03) << 6);` |
|    23 | 2412 | `				break;` |
|     5 | 2413 | `			case 3:` |
|    11 | 2414 | `				zOut[j++] \|= (unsigned char)val;` |
|    10 | 2415 | `				break;` |
|     - | 2416 | `		}` |
|   109 | 2417 | `		i++;` |
|    55 | 2418 | `	}` |
|    33 | 2419 | `	if( strict ){` |
|     - | 2420 | `		/* A lone trailing 6-bit group (one leftover char) cannot form a byte. */` |
|    21 | 2421 | `		if( (i & 3) == 1 ){` |
|     3 | 2422 | `			goto fail;` |
|     - | 2423 | `		}` |
|     - | 2424 | `		/* Padding must be 1 or 2 bytes and complete the 4-char group. */` |
|    19 | 2425 | `		if( padding && (padding > 2 \|\| ((i + padding) & 3) != 0) ){` |
|     3 | 2426 | `			goto fail;` |
|     - | 2427 | `		}` |
|     8 | 2428 | `	}` |
|    29 | 2429 | `	ph7_result_string(pCtx,(const char *)zOut,j);` |
|    29 | 2430 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    29 | 2431 | `	return PH7_OK;` |
|     4 | 2432 | `fail:` |
|     9 | 2433 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     9 | 2434 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2435 | `	return PH7_OK;` |
|    22 | 2436 | `}` |
|     - | 2437 | `/*` |
|     - | 2438 | ` * uuencode's six-bit alphabet: a value of 0 is written as the backtick php uses` |
|     - | 2439 | ` * instead of the historical space, every other value as ' ' + value. The three` |
|     - | 2440 | ` * PH7_UU_ENC_C* helpers pack the 6-bit groups exactly like php's macros: each` |
|     - | 2441 | ` * contribution is masked to its own bit window, so the result never depends on` |
|     - | 2442 | ` * whether the platform's char is signed.` |
|     - | 2443 | ` */` |
|     - | 2444 | ``#define PH7_UU_ENC(c)      ((char)((c) ? (((c) & 077) + ' ') : '`'))`` |
|     - | 2445 | `#define PH7_UU_ENC_C1(a)   PH7_UU_ENC((a) >> 2)` |
|     - | 2446 | `#define PH7_UU_ENC_C2(a,b) PH7_UU_ENC((((a) << 4) & 060) \| (((b) >> 4) & 017))` |
|     - | 2447 | `#define PH7_UU_ENC_C3(b,c) PH7_UU_ENC((((b) << 2) & 074) \| (((c) >> 6) & 003))` |
|     - | 2448 | `#define PH7_UU_ENC_C4(c)   PH7_UU_ENC((c) & 077)` |
|     - | 2449 | `#define PH7_UU_DEC(c)      ((((int)(c)) - ' ') & 077)` |
|     - | 2450 | `/*` |
|     - | 2451 | ` * string convert_uuencode(string $data)` |
|     - | 2452 | ` *  Uuencode a string.` |
|     - | 2453 | ` * Parameter` |
|     - | 2454 | ` *  $data` |
|     - | 2455 | ` *   Data to encode.` |
|     - | 2456 | ` * Return` |
|     - | 2457 | ` *  The uuencoded data: 45-byte lines, each prefixed with its encoded length and` |
|     - | 2458 | `` *  terminated by a newline, followed by php's "`\n" end marker. An empty input`` |
|     - | 2459 | ` *  answers just that marker.` |
|     - | 2460 | ` * Implementation note: a faithful port of php's php_uuencode(). This used to be` |
|     - | 2461 | ` * registered as an ALIAS of base64_encode() -- a wrong ALGORITHM, so every answer` |
|     - | 2462 | ` * was silently a base64 string (convert_uuencode("abc") gave "YWJj" where php` |
|     - | 2463 | `` * gives "#86)C\n`\n").`` |
|     - | 2464 | ` */` |
|    36 | 2465 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2466 | `{` |
|     - | 2467 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 2468 | `	char zLine[64]; /* one full line is 1 length byte + 60 data bytes + '\n' */` |
|    38 | 2469 | `	int nLen,iLen = 45,n;` |
|    38 | 2470 | `	if( nArg < 1 ){` |
|     - | 2471 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2472 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2473 | `		return PH7_OK;` |
|     - | 2474 | `	}` |
|     - | 2475 | `	/* Extract the input string */` |
|    38 | 2476 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    38 | 2477 | `	if( nLen < 0 ){` |
|   ! 0 | 2478 | `		nLen = 0;` |
|   ! 0 | 2479 | `	}` |
|    38 | 2480 | `	zEnd = &zIn[nLen];` |
|     - | 2481 | `	/* Emit whole groups while at least four bytes remain: the last line is closed by` |
|     - | 2482 | ``	 * the tail block below so a group of one or two bytes gets php's '`' filler. */`` |
|    70 | 2483 | `	while( &zIn[3] < zEnd ){` |
|    34 | 2484 | `		zStop = &zIn[iLen];` |
|    34 | 2485 | `		if( zStop > zEnd ){` |
|     - | 2486 | `			/* A short final line: its length byte counts every remaining byte, but only` |
|     - | 2487 | `			 * whole three-byte groups are encoded here -- the leftovers ride the tail` |
|     - | 2488 | `			 * block, which then adds no length byte of its own. */` |
|    10 | 2489 | `			iLen = (int)(zEnd - zIn);` |
|    10 | 2490 | `			zStop = &zIn[(iLen/3)*3];` |
|     4 | 2491 | `		}` |
|    34 | 2492 | `		n = 0;` |
|    34 | 2493 | `		zLine[n++] = PH7_UU_ENC(iLen);` |
|   476 | 2494 | `		while( zIn < zStop ){` |
|   444 | 2495 | `			zLine[n++] = PH7_UU_ENC_C1(zIn[0]);` |
|   444 | 2496 | `			zLine[n++] = PH7_UU_ENC_C2(zIn[0],zIn[1]);` |
|   444 | 2497 | `			zLine[n++] = PH7_UU_ENC_C3(zIn[1],zIn[2]);` |
|   444 | 2498 | `			zLine[n++] = PH7_UU_ENC_C4(zIn[2]);` |
|   444 | 2499 | `			zIn += 3;` |
|     2 | 2500 | `		}` |
|    34 | 2501 | `		if( iLen == 45 ){` |
|    25 | 2502 | `			zLine[n++] = '\n';` |
|    12 | 2503 | `		}` |
|    34 | 2504 | `		ph7_result_string(pCtx,zLine,n);` |
|     2 | 2505 | `	}` |
|    38 | 2506 | `	if( zIn < zEnd ){` |
|     - | 2507 | `		/* One to three trailing bytes. php reads the bytes past the end of the string` |
|     - | 2508 | `		 * (its buffers are NUL terminated); the missing ones are zero here. */` |
|    30 | 2509 | `		unsigned char c0 = zIn[0];` |
|    30 | 2510 | `		unsigned char c1 = (&zIn[1] < zEnd) ? zIn[1] : 0;` |
|    30 | 2511 | `		unsigned char c2 = (&zIn[2] < zEnd) ? zIn[2] : 0;` |
|    30 | 2512 | `		n = 0;` |
|    30 | 2513 | `		if( iLen == 45 ){` |
|     - | 2514 | `			/* No short line was opened above: this group is a line of its own. */` |
|    22 | 2515 | `			zLine[n++] = PH7_UU_ENC((int)(zEnd - zIn));` |
|    22 | 2516 | `			iLen = 0;` |
|    10 | 2517 | `		}` |
|    30 | 2518 | `		zLine[n++] = PH7_UU_ENC_C1(c0);` |
|    30 | 2519 | `		zLine[n++] = PH7_UU_ENC_C2(c0,c1);` |
|    30 | 2520 | ``		zLine[n++] = ((zEnd - zIn) > 1) ? PH7_UU_ENC_C3(c1,c2) : '`';`` |
|    30 | 2521 | ``		zLine[n++] = ((zEnd - zIn) > 2) ? PH7_UU_ENC_C4(c2)     : '`';`` |
|    30 | 2522 | `		ph7_result_string(pCtx,zLine,n);` |
|    14 | 2523 | `	}` |
|    38 | 2524 | `	if( iLen != 45 ){` |
|     - | 2525 | `		/* A short (or tail) line is still open; a run of whole 45-byte lines -- and the` |
|     - | 2526 | `		 * empty input, which opens no line at all -- is already newline-terminated. */` |
|    30 | 2527 | `		ph7_result_string(pCtx,"\n",1);` |
|    14 | 2528 | `	}` |
|     - | 2529 | `	/* php's end marker: a zero-length line. */` |
|    38 | 2530 | ``	ph7_result_string(pCtx,"`\n",2);`` |
|    38 | 2531 | `	return PH7_OK;` |
|    20 | 2532 | `}` |
|     - | 2533 | `/*` |
|     - | 2534 | ` * string\|false convert_uudecode(string $data)` |
|     - | 2535 | ` *  Decode a uuencoded string.` |
|     - | 2536 | ` * Parameter` |
|     - | 2537 | ` *  $data` |
|     - | 2538 | ` *   Uuencoded data.` |
|     - | 2539 | ` * Return` |
|     - | 2540 | ` *  The decoded data, or FALSE (with a warning) when $data is not a valid uuencoded` |
|     - | 2541 | ` *  string: an empty input, a line claiming more bytes than the whole input holds, or` |
|     - | 2542 | ` *  a line whose data is truncated. Trailing garbage after the first short line is` |
|     - | 2543 | ` *  ignored, exactly like php.` |
|     - | 2544 | ` * Implementation note: a faithful port of php's php_uudecode(); see the encoder above` |
|     - | 2545 | ` * for why this was not a decoder at all before.` |
|     - | 2546 | ` */` |
|    48 | 2547 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2548 | `{` |
|     - | 2549 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 2550 | `	unsigned char *zOut;` |
|     - | 2551 | `	int nLen,iLen;` |
|    50 | 2552 | `	sxu32 nOut = 0,nTotal = 0;` |
|    50 | 2553 | `	if( nArg < 1 ){` |
|     - | 2554 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2555 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2556 | `		return PH7_OK;` |
|     - | 2557 | `	}` |
|     - | 2558 | `	/* Extract the input string */` |
|    50 | 2559 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    50 | 2560 | `	if( nLen < 1 ){` |
|     - | 2561 | `		/* php refuses the empty string rather than decoding it to "". */` |
|     3 | 2562 | `		goto fail;` |
|     - | 2563 | `	}` |
|    48 | 2564 | `	zEnd = &zIn[nLen];` |
|     - | 2565 | `	/* Every four input characters yield three bytes and each line spends one more` |
|     - | 2566 | `	 * character on its length, so the input length is a safe upper bound. */` |
|    48 | 2567 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    48 | 2568 | `	if( zOut == 0 ){` |
|   ! 0 | 2569 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2570 | `	}` |
|    72 | 2571 | `	while( zIn < zEnd ){` |
|    72 | 2572 | `		iLen = PH7_UU_DEC(*zIn++);` |
|    72 | 2573 | `		if( iLen == 0 ){` |
|     - | 2574 | `			/* The end marker (or any line claiming zero bytes) stops the decoding. */` |
|    12 | 2575 | `			break;` |
|     - | 2576 | `		}` |
|    62 | 2577 | `		if( iLen > nLen ){` |
|     5 | 2578 | `			goto err;` |
|     - | 2579 | `		}` |
|    58 | 2580 | `		nTotal += (sxu32)iLen;` |
|     - | 2581 | `		/* A line carries four characters per three-byte group, whole groups only. */` |
|    58 | 2582 | `		zStop = zIn + ((iLen + 2)/3)*4;` |
|    58 | 2583 | `		if( zStop > zEnd ){` |
|     5 | 2584 | `			goto err;` |
|     - | 2585 | `		}` |
|   544 | 2586 | `		while( zIn < zStop ){` |
|   492 | 2587 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[0]) << 2) \| (PH7_UU_DEC(zIn[1]) >> 4));` |
|   492 | 2588 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[1]) << 4) \| (PH7_UU_DEC(zIn[2]) >> 2));` |
|   492 | 2589 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[2]) << 6) \|  PH7_UU_DEC(zIn[3]));` |
|   492 | 2590 | `			zIn += 4;` |
|     2 | 2591 | `		}` |
|    54 | 2592 | `		if( iLen < 45 ){` |
|     - | 2593 | `			/* A short line ends the payload; whatever follows is ignored. */` |
|    30 | 2594 | `			break;` |
|     - | 2595 | `		}` |
|    25 | 2596 | `		zIn++; /* Skip the line separator */` |
|     1 | 2597 | `	}` |
|     - | 2598 | `	/* Drop the padding the last group carried: php keeps only as many bytes as the` |
|     - | 2599 | `	 * length bytes declared, counted over the WHOLE input rather than per line. */` |
|    40 | 2600 | `	if( nOut > nTotal ){` |
|    20 | 2601 | `		nOut = nTotal;` |
|     9 | 2602 | `	}` |
|    40 | 2603 | `	ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|    40 | 2604 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    40 | 2605 | `	return PH7_OK;` |
|     4 | 2606 | `err:` |
|     9 | 2607 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     5 | 2608 | `fail:` |
|    11 | 2609 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 2610 | `		"Argument #1 ($data) is not a valid uuencoded string"); /* the "convert_uudecode(): " prefix is added by the handler */` |
|    11 | 2611 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2612 | `	return PH7_OK;` |
|    26 | 2613 | `}` |
|     - | 2614 | `/*` |
|     - | 2615 | ` * string urlencode(string $str)` |
|     - | 2616 | ` *  URL encoding` |
|     - | 2617 | ` * Parameter` |
|     - | 2618 | ` *  $data` |
|     - | 2619 | ` *   Input string.` |
|     - | 2620 | ` * Return` |
|     - | 2621 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|     - | 2622 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|     - | 2623 | ` *  encoded as plus (+) signs.` |
|     - | 2624 | ` */` |
|   104 | 2625 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2626 | `{` |
|     - | 2627 | `	const char *zIn;` |
|     - | 2628 | `	int nLen;` |
|   107 | 2629 | `	if( nArg < 1 ){` |
|     - | 2630 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2631 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2632 | `		return PH7_OK;` |
|     - | 2633 | `	}` |
|     - | 2634 | `	/* Extract the input string */` |
|   107 | 2635 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   107 | 2636 | `	if( nLen < 1 ){` |
|     - | 2637 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 2638 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2639 | `		return PH7_OK;` |
|     - | 2640 | `	}` |
|     - | 2641 | `	/* Perform the URL encoding */` |
|   102 | 2642 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|   102 | 2643 | `	return PH7_OK;` |
|    55 | 2644 | `}` |
|     - | 2645 | `/*` |
|     - | 2646 | ` * string rawurlencode(string $str)` |
|     - | 2647 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|     - | 2648 | ` */` |
|    18 | 2649 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2650 | `{` |
|     - | 2651 | `	const char *zIn;` |
|     - | 2652 | `	int nLen;` |
|    21 | 2653 | `	if( nArg < 1 ){` |
|     - | 2654 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2655 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2656 | `		return PH7_OK;` |
|     - | 2657 | `	}` |
|     - | 2658 | `	/* Extract the input string */` |
|    21 | 2659 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    21 | 2660 | `	if( nLen < 1 ){` |
|     - | 2661 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 2662 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2663 | `		return PH7_OK;` |
|     - | 2664 | `	}` |
|     - | 2665 | `	/* Perform the RFC 3986 URL encoding */` |
|    16 | 2666 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    16 | 2667 | `	return PH7_OK;` |
|    12 | 2668 | `}` |
|     - | 2669 | `/*` |
|     - | 2670 | ` * string urldecode(string $str)` |
|     - | 2671 | ` *  Decodes any %## encoding in the given string.` |
|     - | 2672 | ` *  Plus symbols ('+') are decoded to a space character.` |
|     - | 2673 | ` * string rawurldecode(string $str)` |
|     - | 2674 | ` *  The same, except that '+' is NOT a space: RFC 3986 has no plus convention, so` |
|     - | 2675 | ` *  php leaves it alone. rawurldecode() used to be registered as an ALIAS of` |
|     - | 2676 | ` *  urldecode(), which turned every literal '+' into a space.` |
|     - | 2677 | ` * Parameter` |
|     - | 2678 | ` *  $data` |
|     - | 2679 | ` *    Input string.` |
|     - | 2680 | ` * Return` |
|     - | 2681 | ` *  Decoded URL or FALSE on failure.` |
|     - | 2682 | ` */` |
|   242 | 2683 | `static int UrlDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPlus)` |
|     3 | 2684 | `{` |
|     - | 2685 | `	const char *zIn;` |
|     - | 2686 | `	int nLen;` |
|   245 | 2687 | `	if( nArg < 1 ){` |
|     - | 2688 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2689 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2690 | `		return PH7_OK;` |
|     - | 2691 | `	}` |
|     - | 2692 | `	/* Extract the input string */` |
|   245 | 2693 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   245 | 2694 | `	if( nLen < 1 ){` |
|     - | 2695 | `		/* php returns an empty string for empty input, not FALSE */` |
|    27 | 2696 | `		ph7_result_string(pCtx,"",0);` |
|    27 | 2697 | `		return PH7_OK;` |
|     - | 2698 | `	}` |
|     - | 2699 | `	/* Perform the URL decoding */` |
|   220 | 2700 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,bPlus);` |
|   220 | 2701 | `	return PH7_OK;` |
|   124 | 2702 | `}` |
|   186 | 2703 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2704 | `{` |
|   189 | 2705 | `	return UrlDecodeCommon(pCtx,nArg,apArg,TRUE);` |
|     3 | 2706 | `}` |
|    56 | 2707 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2708 | `{` |
|    59 | 2709 | `	return UrlDecodeCommon(pCtx,nArg,apArg,FALSE);` |
|     3 | 2710 | `}` |
|     - | 2711 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 2712 |  |
