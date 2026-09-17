# src/ph7/builtin_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1234/1377 lines (89.62%)

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
|     4 | 1154 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|     - | 1155 | `	const char *zInput, /* Raw input */` |
|     - | 1156 | `	int nByte,  /* Input length */` |
|     - | 1157 | `	int delim,  /* Delimiter */` |
|     - | 1158 | `	int encl,   /* Enclosure */` |
|     - | 1159 | `	int escape,  /* Escape character */` |
|     - | 1160 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|     - | 1161 | `	void *pUserData /* Last argument to xConsumer() */` |
|     - | 1162 | `	)` |
|     1 | 1163 | `{` |
|     5 | 1164 | `	const char *zEnd = &zInput[nByte];` |
|     5 | 1165 | `	const char *zIn = zInput;` |
|     - | 1166 | `	const char *zPtr;` |
|     - | 1167 | `	int isEnc;` |
|     - | 1168 | `	/* Start processing */` |
|     8 | 1169 | `	for(;;){` |
|    17 | 1170 | `		if( zIn >= zEnd ){` |
|     - | 1171 | `			/* No more input to process */` |
|     5 | 1172 | `			break;` |
|     - | 1173 | `		}` |
|    13 | 1174 | `		isEnc = 0;` |
|    13 | 1175 | `		zPtr = zIn;` |
|     - | 1176 | `		/* Find the first delimiter */` |
|    27 | 1177 | `		while( zIn < zEnd ){` |
|    23 | 1178 | `			if( zIn[0] == delim && !isEnc){` |
|     - | 1179 | `				/* Delimiter found,break imediately */` |
|     5 | 1180 | `				break;` |
|    15 | 1181 | `			}else if( zIn[0] == encl ){` |
|     - | 1182 | `				/* Inside enclosure? */` |
|   ! 0 | 1183 | `				isEnc = !isEnc;` |
|    15 | 1184 | `			}else if( zIn[0] == escape ){` |
|     - | 1185 | `				/* Escape sequence */` |
|   ! 0 | 1186 | `				zIn++;` |
|   ! 0 | 1187 | `			}` |
|     - | 1188 | `			/* Advance the cursor */` |
|    15 | 1189 | `			zIn++;` |
|     1 | 1190 | `		}` |
|    13 | 1191 | `		if( zIn > zPtr ){` |
|    13 | 1192 | `			int nByteChunk = (int)(zIn-zPtr);` |
|     - | 1193 | `			sxi32 rc;` |
|     - | 1194 | `			/* Invoke the supllied callback */` |
|    13 | 1195 | `			if( zPtr[0] == encl ){` |
|   ! 0 | 1196 | `				zPtr++;` |
|   ! 0 | 1197 | `				nByteChunk-=2;` |
|   ! 0 | 1198 | `			}` |
|    13 | 1199 | `			if( nByteChunk > 0 ){` |
|    13 | 1200 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|    13 | 1201 | `				if( rc == SXERR_ABORT ){` |
|     - | 1202 | `					/* User callback request an operation abort */` |
|   ! 0 | 1203 | `					break;` |
|     - | 1204 | `				}` |
|     6 | 1205 | `			}` |
|     6 | 1206 | `		}` |
|     - | 1207 | `		/* Ignore trailing delimiter */` |
|    21 | 1208 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|     9 | 1209 | `			zIn++;` |
|     1 | 1210 | `		}` |
|     1 | 1211 | `	}` |
|     5 | 1212 | `	return SXRET_OK;` |
|     1 | 1213 | `}` |
|     - | 1214 | `/*` |
|     - | 1215 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|     - | 1216 | ` * All the processed input is insereted into an array passed as the last` |
|     - | 1217 | ` * argument to this callback.` |
|     - | 1218 | ` */` |
|    12 | 1219 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|     1 | 1220 | `{` |
|    13 | 1221 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - | 1222 | `	ph7_value sEntry;` |
|     - | 1223 | `	SyString sToken;` |
|     - | 1224 | `	/* Insert the token in the given array */` |
|    13 | 1225 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|     - | 1226 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|    27 | 1227 | `	SyStringFullTrimSafe(&sToken);` |
|    13 | 1228 | `	if( sToken.nByte < 1){` |
|   ! 0 | 1229 | `		return SXRET_OK;` |
|     - | 1230 | `	}` |
|    13 | 1231 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|    13 | 1232 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|    13 | 1233 | `	PH7_MemObjRelease(&sEntry);` |
|    13 | 1234 | `	return SXRET_OK;` |
|     7 | 1235 | `}` |
|     - | 1236 | `/*` |
|     - | 1237 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|     - | 1238 | ` *  Parse a CSV string into an array.` |
|     - | 1239 | ` * Parameters` |
|     - | 1240 | ` *  $input` |
|     - | 1241 | ` *   The string to parse.` |
|     - | 1242 | ` *  $delimiter` |
|     - | 1243 | ` *   Set the field delimiter (one character only).` |
|     - | 1244 | ` *  $enclosure` |
|     - | 1245 | ` *   Set the field enclosure character (one character only).` |
|     - | 1246 | ` *  $escape` |
|     - | 1247 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|     - | 1248 | ` * Return` |
|     - | 1249 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|     - | 1250 | ` */` |
|     2 | 1251 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1252 | `{` |
|     - | 1253 | `	const char *zInput,*zPtr;` |
|     - | 1254 | `	ph7_value *pArray;` |
|     3 | 1255 | `	int delim  = ',';   /* Delimiter */` |
|     3 | 1256 | `	int encl   = '"' ;  /* Enclosure */` |
|     3 | 1257 | `	int escape = '\\';  /* Escape character */` |
|     - | 1258 | `	int nLen;` |
|     3 | 1259 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1260 | `		/* Missing/Invalid arguments,return NULL */` |
|   ! 0 | 1261 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1262 | `		return PH7_OK;` |
|     - | 1263 | `	}` |
|     - | 1264 | `	/* Extract the raw input */` |
|     3 | 1265 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|     3 | 1266 | `	if( nArg > 1 ){` |
|     - | 1267 | `		int i;` |
|     3 | 1268 | `		if( ph7_value_is_string(apArg[1]) ){` |
|     - | 1269 | `			/* Extract the delimiter */` |
|     3 | 1270 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|     3 | 1271 | `			if( i > 0 ){` |
|     3 | 1272 | `				delim = zPtr[0];` |
|     1 | 1273 | `			}` |
|     1 | 1274 | `		}` |
|     3 | 1275 | `		if( nArg > 2 ){` |
|     3 | 1276 | `			if( ph7_value_is_string(apArg[2]) ){` |
|     - | 1277 | `				/* Extract the enclosure */` |
|     3 | 1278 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|     3 | 1279 | `				if( i > 0 ){` |
|     3 | 1280 | `					encl = zPtr[0];` |
|     1 | 1281 | `				}` |
|     1 | 1282 | `			}` |
|     3 | 1283 | `			if( nArg > 3 ){` |
|     3 | 1284 | `				if( ph7_value_is_string(apArg[3]) ){` |
|     - | 1285 | `					/* Extract the escape character */` |
|     3 | 1286 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|     3 | 1287 | `					if( i > 0 ){` |
|     3 | 1288 | `						escape = zPtr[0];` |
|     1 | 1289 | `					}` |
|     1 | 1290 | `				}` |
|     1 | 1291 | `			}` |
|     1 | 1292 | `		}` |
|     1 | 1293 | `	}` |
|     - | 1294 | `	/* Create our array */` |
|     3 | 1295 | `	pArray = ph7_context_new_array(pCtx);` |
|     3 | 1296 | `	if( pArray == 0 ){` |
|     - | 1297 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|   ! 0 | 1298 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1299 | `	}` |
|     - | 1300 | `	/* Parse the raw input */` |
|     3 | 1301 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|     - | 1302 | `	/* Return the freshly created array */` |
|     3 | 1303 | `	ph7_result_value(pCtx,pArray);` |
|     3 | 1304 | `	return PH7_OK;` |
|     2 | 1305 | `}` |
|     - | 1306 | `/*` |
|     - | 1307 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|     - | 1308 | ` * container.` |
|     - | 1309 | ` * Refer to [strip_tags()].` |
|     - | 1310 | ` */` |
|    10 | 1311 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|     1 | 1312 | `{` |
|    11 | 1313 | `	const char *zEnd = &zTag[nByte];` |
|     - | 1314 | `	const char *zPtr;` |
|     - | 1315 | `	SyString sEntry;` |
|     - | 1316 | `	/* Strip tags */` |
|    10 | 1317 | `	for(;;){` |
|    45 | 1318 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|    14 | 1319 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|    15 | 1320 | `				zTag++;` |
|     1 | 1321 | `		}` |
|    21 | 1322 | `		if( zTag >= zEnd ){` |
|    11 | 1323 | `			break;` |
|     - | 1324 | `		}` |
|    11 | 1325 | `		zPtr = zTag;` |
|     - | 1326 | `		/* Delimit the tag */` |
|    25 | 1327 | `		while(zTag < zEnd ){` |
|    25 | 1328 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|     - | 1329 | `				/* UTF-8 stream */` |
|     3 | 1330 | `				zTag++;` |
|     5 | 1331 | `				SX_JMP_UTF8(zTag,zEnd);` |
|    24 | 1332 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|    11 | 1333 | `				break;` |
|   ! 0 | 1334 | `			}else{` |
|    13 | 1335 | `				zTag++;` |
|     - | 1336 | `			}` |
|     1 | 1337 | `		}` |
|    11 | 1338 | `		if( zTag > zPtr ){` |
|     - | 1339 | `			/* Perform the insertion */` |
|    11 | 1340 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|    11 | 1341 | `			SyStringFullTrim(&sEntry);` |
|    11 | 1342 | `			SySetPut(pSet,(const void *)&sEntry);` |
|     5 | 1343 | `		}` |
|     - | 1344 | `		/* Jump the trailing '>' */` |
|    11 | 1345 | `		zTag++;` |
|     1 | 1346 | `	}` |
|    11 | 1347 | `	return SXRET_OK;` |
|     1 | 1348 | `}` |
|     - | 1349 | `/*` |
|     - | 1350 | ` * Check if the given HTML tag name is present in the given container.` |
|     - | 1351 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|     - | 1352 | ` * Refer to [strip_tags()].` |
|     - | 1353 | ` */` |
|    36 | 1354 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|     1 | 1355 | `{` |
|    37 | 1356 | `	if( SySetUsed(pSet) > 0 ){` |
|    25 | 1357 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|     - | 1358 | `		SyString sTag;` |
|    85 | 1359 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|    24 | 1360 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|    37 | 1361 | `			zTag++;` |
|     1 | 1362 | `		}` |
|     - | 1363 | `		/* Delimit the tag */` |
|    25 | 1364 | `		zCur = zTag;` |
|    77 | 1365 | `		while(zTag < zEnd ){` |
|    77 | 1366 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|     - | 1367 | `				/* UTF-8 stream */` |
|     5 | 1368 | `				zTag++;` |
|     9 | 1369 | `				SX_JMP_UTF8(zTag,zEnd);` |
|    75 | 1370 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|    25 | 1371 | `				break;` |
|   ! 0 | 1372 | `			}else{` |
|    49 | 1373 | `				zTag++;` |
|     - | 1374 | `			}` |
|     1 | 1375 | `		}` |
|    25 | 1376 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|     - | 1377 | `		/* Trim leading white spaces and null bytes */` |
|    35 | 1378 | `		SyStringLeftTrimSafe(&sTag);` |
|    25 | 1379 | `		if( sTag.nByte > 0 ){` |
|     - | 1380 | `			SyString *aEntry,*pEntry;` |
|     - | 1381 | `			sxi32 rc;` |
|     - | 1382 | `			sxu32 n;` |
|     - | 1383 | `			/* Perform the lookup */` |
|    25 | 1384 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|    29 | 1385 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|    25 | 1386 | `				pEntry = &aEntry[n];` |
|     - | 1387 | `				/* Do the comparison */` |
|    25 | 1388 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|    25 | 1389 | `				if( !rc ){` |
|    21 | 1390 | `					return SXRET_OK;` |
|     - | 1391 | `				}` |
|     3 | 1392 | `			}` |
|     2 | 1393 | `		}` |
|     2 | 1394 | `	}` |
|     - | 1395 | `	/* No such tag */` |
|    17 | 1396 | `	return SXERR_NOTFOUND;` |
|    19 | 1397 | `}` |
|     - | 1398 | `/*` |
|     - | 1399 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|     - | 1400 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|     - | 1401 | ` * Refer to [strip_tags()].` |
|     - | 1402 | ` */` |
|    16 | 1403 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|     1 | 1404 | `{` |
|    17 | 1405 | `	const char *zEnd = &zIn[nByte];` |
|     - | 1406 | `	const char *zPtr,*zTag;` |
|     - | 1407 | `	SySet sSet;` |
|     - | 1408 | `	/* initialize the set of allowed tags */` |
|    17 | 1409 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|    17 | 1410 | `	if( nTaglen > 0 ){` |
|     - | 1411 | `		/* Set of allowed tags */` |
|    11 | 1412 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|     5 | 1413 | `	}` |
|     - | 1414 | `	/* Set the empty string */` |
|    17 | 1415 | `	ph7_result_string(pCtx,"",0);` |
|     - | 1416 | `	/* Start processing */` |
|    26 | 1417 | `	for(;;){` |
|    53 | 1418 | `		if(zIn >= zEnd){` |
|     - | 1419 | `			/* No more input to process */` |
|    15 | 1420 | `			break;` |
|     - | 1421 | `		}` |
|    39 | 1422 | `		zPtr = zIn;` |
|     - | 1423 | `		/* Find a tag */` |
|   133 | 1424 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|    95 | 1425 | `			zIn++;` |
|     1 | 1426 | `		}` |
|    39 | 1427 | `		if( zIn > zPtr ){` |
|     - | 1428 | `			/* Consume raw input */` |
|    21 | 1429 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|    10 | 1430 | `		}` |
|     - | 1431 | `		/* Ignore trailing null bytes */` |
|    39 | 1432 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|   ! 0 | 1433 | `			zIn++;` |
|   ! 0 | 1434 | `		}` |
|    39 | 1435 | `		if(zIn >= zEnd){` |
|     - | 1436 | `			/* No more input to process */` |
|     3 | 1437 | `			break;` |
|     - | 1438 | `		}` |
|    37 | 1439 | `		if( zIn[0] == '<' ){` |
|     - | 1440 | `			sxi32 rc;` |
|    37 | 1441 | `			zTag = zIn++;` |
|     - | 1442 | `			/* Delimit the tag */` |
|   127 | 1443 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|    91 | 1444 | `				zIn++;` |
|     1 | 1445 | `			}` |
|    37 | 1446 | `			if( zIn < zEnd ){` |
|    37 | 1447 | `				zIn++; /* Ignore the trailing closing tag */` |
|    18 | 1448 | `			}` |
|     - | 1449 | `			/* Query the set */` |
|    37 | 1450 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|    37 | 1451 | `			if( rc == SXRET_OK ){` |
|     - | 1452 | `				/* Keep the tag */` |
|    21 | 1453 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|    10 | 1454 | `			}` |
|    18 | 1455 | `		}` |
|     1 | 1456 | `	}` |
|     - | 1457 | `	/* Cleanup */` |
|    17 | 1458 | `	SySetRelease(&sSet);` |
|    17 | 1459 | `	return SXRET_OK;` |
|     1 | 1460 | `}` |
|     - | 1461 | `/*` |
|     - | 1462 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|     - | 1463 | ` *   Strip HTML and PHP tags from a string.` |
|     - | 1464 | ` * Parameters` |
|     - | 1465 | ` *  $str` |
|     - | 1466 | ` *  The input string.` |
|     - | 1467 | ` * $allowable_tags` |
|     - | 1468 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|     - | 1469 | ` * Return` |
|     - | 1470 | ` *  Returns the stripped string.` |
|     - | 1471 | ` */` |
|    14 | 1472 | `PH7_PRIVATE int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1473 | `{` |
|    15 | 1474 | `	const char *zTaglist = 0;` |
|     - | 1475 | `	const char *zString;` |
|    15 | 1476 | `	int nTaglen = 0;` |
|     - | 1477 | `	int nLen;` |
|    15 | 1478 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1479 | `		/* Missing/Invalid arguments,return the empty string */` |
|   ! 0 | 1480 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 1481 | `		return PH7_OK;` |
|     - | 1482 | `	}` |
|     - | 1483 | `	/* Point to the raw string */` |
|    15 | 1484 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    15 | 1485 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|     - | 1486 | `		/* Allowed tag */` |
|    11 | 1487 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|     5 | 1488 | `	}` |
|     - | 1489 | `	/* Process input */` |
|    15 | 1490 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|    15 | 1491 | `	return PH7_OK;` |
|     8 | 1492 | `}` |
|     - | 1493 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1494 | `#ifdef PH7_NEED_FMT_AND_INI` |
|     - | 1495 | `/*` |
|     - | 1496 | ` * Parse an INI string.` |
|     - | 1497 |  |
|     - | 1498 | ` * According to wikipedia` |
|     - | 1499 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|     - | 1500 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|     - | 1501 | ` *  Format` |
|     - | 1502 | `*    Properties` |
|     - | 1503 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|     - | 1504 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|     - | 1505 | `*     Example:` |
|     - | 1506 | `*      name=value` |
|     - | 1507 | `*    Sections` |
|     - | 1508 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|     - | 1509 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|     - | 1510 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|     - | 1511 | `*     or the end of the file. Sections may not be nested.` |
|     - | 1512 | `*     Example:` |
|     - | 1513 | `*      [section]` |
|     - | 1514 | `*   Comments` |
|     - | 1515 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|     - | 1516 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|     - | 1517 | `*/` |
|    12 | 1518 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|     1 | 1519 | `{` |
|     - | 1520 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|    13 | 1521 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - | 1522 | `	SyHashEntry *pEntry;` |
|     - | 1523 | `	SyString sEntry;` |
|     - | 1524 | `	SyHash sHash;` |
|     - | 1525 | `	int c;` |
|     - | 1526 | `	/* Create an empty array and worker variables */` |
|    13 | 1527 | `	pArray = ph7_context_new_array(pCtx);` |
|    13 | 1528 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    13 | 1529 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    13 | 1530 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|     - | 1531 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|   ! 0 | 1532 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1533 | `	}` |
|    13 | 1534 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|    13 | 1535 | `	pCur = pArray;` |
|     - | 1536 | `	/* Start the parse process */` |
|    21 | 1537 | `	for(;;){` |
|     - | 1538 | `		/* Ignore leading white spaces */` |
|    69 | 1539 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|    27 | 1540 | `			zIn++;` |
|     1 | 1541 | `		}` |
|    43 | 1542 | `		if( zIn >= zEnd ){` |
|     - | 1543 | `			/* No more input to process */` |
|    13 | 1544 | `			break;` |
|     - | 1545 | `		}` |
|    31 | 1546 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1547 | `			/* Comment til the end of line */` |
|   ! 0 | 1548 | `			zIn++;` |
|   ! 0 | 1549 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|   ! 0 | 1550 | `				zIn++;` |
|   ! 0 | 1551 | `			}` |
|   ! 0 | 1552 | `			continue;` |
|     - | 1553 | `		}` |
|     - | 1554 | `		/* Reset the string cursor of the working variable */` |
|    31 | 1555 | `		ph7_value_reset_string_cursor(pWorker);` |
|    31 | 1556 | `		if( zIn[0] == '[' ){` |
|     - | 1557 | `			/* Section: Extract the section name */` |
|     9 | 1558 | `			zIn++;` |
|     9 | 1559 | `			zCur = zIn;` |
|    73 | 1560 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|    65 | 1561 | `				zIn++;` |
|     1 | 1562 | `			}` |
|     9 | 1563 | `			if( zIn > zCur && bProcessSection ){` |
|     - | 1564 | `				/* Save the section name */` |
|     5 | 1565 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     5 | 1566 | `				SyStringFullTrim(&sEntry);` |
|     5 | 1567 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     5 | 1568 | `				if( sEntry.nByte > 0 ){` |
|     - | 1569 | `					/* Associate an array with the section */` |
|     5 | 1570 | `					pSection = ph7_context_new_array(pCtx);` |
|     5 | 1571 | `					if( pSection ){` |
|     5 | 1572 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|     5 | 1573 | `						pCur = pSection;` |
|     2 | 1574 | `					}` |
|     2 | 1575 | `				}` |
|     2 | 1576 | `			}` |
|     9 | 1577 | `			zIn++; /* Trailing square brackets ']' */` |
|     5 | 1578 | `		}else{` |
|     - | 1579 | `			ph7_value *pOldCur;` |
|     - | 1580 | `			int is_array;` |
|     - | 1581 | `			int iLen;` |
|     - | 1582 | `			/* Properties */` |
|    23 | 1583 | `			is_array = 0;` |
|    23 | 1584 | `			zCur = zIn;` |
|    23 | 1585 | `			iLen = 0; /* cc warning */` |
|    23 | 1586 | `			pOldCur = pCur;` |
|   155 | 1587 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|   133 | 1588 | `				if( zIn[0] == '[' && !is_array ){` |
|     - | 1589 | `					/* Array */` |
|   ! 0 | 1590 | `					iLen = (int)(zIn-zCur);` |
|   ! 0 | 1591 | `					is_array = 1;` |
|   ! 0 | 1592 | `					if( iLen > 0 ){` |
|   ! 0 | 1593 | `						ph7_value *pvArr = 0; /* cc warning */` |
|     - | 1594 | `						/* Query the hashtable */` |
|   ! 0 | 1595 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   ! 0 | 1596 | `						SyStringFullTrim(&sEntry);` |
|   ! 0 | 1597 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|   ! 0 | 1598 | `						if( pEntry ){` |
|   ! 0 | 1599 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|   ! 0 | 1600 | `						}else{` |
|     - | 1601 | `							/* Create an empty array */` |
|   ! 0 | 1602 | `							pvArr = ph7_context_new_array(pCtx);` |
|   ! 0 | 1603 | `							if( pvArr ){` |
|     - | 1604 | `								/* Save the entry */` |
|   ! 0 | 1605 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|     - | 1606 | `								/* Insert the entry */` |
|   ! 0 | 1607 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1608 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|   ! 0 | 1609 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|   ! 0 | 1610 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1611 | `							}` |
|     - | 1612 | `						}` |
|   ! 0 | 1613 | `						if( pvArr ){` |
|   ! 0 | 1614 | `							pCur = pvArr;` |
|   ! 0 | 1615 | `						}` |
|   ! 0 | 1616 | `					}` |
|   ! 0 | 1617 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|   ! 0 | 1618 | `						zIn++;` |
|   ! 0 | 1619 | `					}` |
|   ! 0 | 1620 | `				}` |
|   133 | 1621 | `				zIn++;` |
|     1 | 1622 | `			}` |
|    23 | 1623 | `			if( !is_array ){` |
|    23 | 1624 | `				iLen = (int)(zIn-zCur);` |
|    11 | 1625 | `			}` |
|     - | 1626 | `			/* Trim the key */` |
|    23 | 1627 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    39 | 1628 | `			SyStringFullTrim(&sEntry);` |
|    23 | 1629 | `			if( sEntry.nByte > 0 ){` |
|    23 | 1630 | `				if( !is_array ){` |
|     - | 1631 | `					/* Save the key name */` |
|    23 | 1632 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    11 | 1633 | `				}` |
|     - | 1634 | `				/* extract key value */` |
|    23 | 1635 | `				ph7_value_reset_string_cursor(pValue);` |
|    23 | 1636 | `				zIn++; /* '=' */` |
|    39 | 1637 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    17 | 1638 | `					zIn++;` |
|     1 | 1639 | `				}` |
|    23 | 1640 | `				if( zIn < zEnd ){` |
|    21 | 1641 | `					zCur = zIn;` |
|    21 | 1642 | `					c = zIn[0];` |
|    21 | 1643 | `					if( c == '"' \|\| c == '\'' ){` |
|   ! 0 | 1644 | `						zIn++;` |
|     - | 1645 | `						/* Delimit the value */` |
|   ! 0 | 1646 | `						while( zIn < zEnd ){` |
|   ! 0 | 1647 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|   ! 0 | 1648 | `								break;` |
|     - | 1649 | `							}` |
|   ! 0 | 1650 | `							zIn++;` |
|   ! 0 | 1651 | `						}` |
|   ! 0 | 1652 | `						if( zIn < zEnd ){` |
|   ! 0 | 1653 | `							zIn++;` |
|   ! 0 | 1654 | `						}` |
|   ! 0 | 1655 | `					}else{` |
|   125 | 1656 | `						while( zIn < zEnd ){` |
|   123 | 1657 | `							if( zIn[0] == '\n' ){` |
|    19 | 1658 | `								if( zIn[-1] != '\\' ){` |
|    19 | 1659 | `									break;` |
|   ! 0 | 1660 | `								}` |
|   105 | 1661 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1662 | `								/* Inline comments */` |
|   ! 0 | 1663 | `								break;` |
|     - | 1664 | `							}` |
|   105 | 1665 | `							zIn++;` |
|     1 | 1666 | `						}` |
|     - | 1667 | `					}` |
|     - | 1668 | `					/* Trim the value */` |
|    21 | 1669 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|    21 | 1670 | `					SyStringFullTrim(&sEntry);` |
|    21 | 1671 | `					if( c == '"' \|\| c == '\'' ){` |
|   ! 0 | 1672 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|   ! 0 | 1673 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|   ! 0 | 1674 | `					}` |
|    21 | 1675 | `					if( sEntry.nByte > 0 ){` |
|    21 | 1676 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|    10 | 1677 | `					}` |
|     - | 1678 | `					/* Insert the key and it's value */` |
|    21 | 1679 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|    10 | 1680 | `				}` |
|    12 | 1681 | `			}else{` |
|   ! 0 | 1682 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|   ! 0 | 1683 | `					zIn++;` |
|   ! 0 | 1684 | `				}` |
|     - | 1685 | `			}` |
|    23 | 1686 | `			pCur = pOldCur;` |
|     - | 1687 | `		}` |
|     1 | 1688 | `	}` |
|    13 | 1689 | `	SyHashRelease(&sHash);` |
|     - | 1690 | `	/* Return the parse of the INI string */` |
|    13 | 1691 | `	ph7_result_value(pCtx,pArray);` |
|    13 | 1692 | `	return SXRET_OK;` |
|     7 | 1693 | `}` |
|     - | 1694 | `/*` |
|     - | 1695 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|     - | 1696 | ` *  Parse a configuration string.` |
|     - | 1697 | ` * Parameters` |
|     - | 1698 | ` *  $ini` |
|     - | 1699 | ` *   The contents of the ini file being parsed.` |
|     - | 1700 | ` *  $process_sections` |
|     - | 1701 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|     - | 1702 | ` *   and settings included. The default for process_sections is FALSE.` |
|     - | 1703 | ` *  $scanner_mode (Not used)` |
|     - | 1704 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|     - | 1705 | ` *   then option values will not be parsed.` |
|     - | 1706 | ` * Return` |
|     - | 1707 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|     - | 1708 | ` */` |
|    10 | 1709 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1710 | `{` |
|     - | 1711 | `	const char *zIni;` |
|     - | 1712 | `	int nByte;` |
|    11 | 1713 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1714 | `		/* Missing/Invalid arguments,return FALSE*/` |
|   ! 0 | 1715 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1716 | `		return PH7_OK;` |
|     - | 1717 | `	}` |
|     - | 1718 | `	/* Extract the raw INI buffer */` |
|    11 | 1719 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|     - | 1720 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|    11 | 1721 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|     6 | 1722 | `}` |
|     - | 1723 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1724 | `#ifdef PH7_NEED_BUILTIN_REG` |
|     - | 1725 |  |
|     - | 1726 | `/*` |
|     - | 1727 | ` * Ctype Functions.` |
|     - | 1728 | ` * Status:` |
|     - | 1729 | ` *    Stable.` |
|     - | 1730 | ` */` |
|     - | 1731 | `/*` |
|     - | 1732 | ` * bool ctype_alnum(string $text)` |
|     - | 1733 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|     - | 1734 | ` * Parameters` |
|     - | 1735 | ` *  $text` |
|     - | 1736 | ` *   The tested string.` |
|     - | 1737 | ` * Return` |
|     - | 1738 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|     - | 1739 | ` */` |
|    72 | 1740 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1741 | `{` |
|     - | 1742 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1743 | `	int nLen;` |
|    73 | 1744 | `	if( nArg < 1 ){` |
|     - | 1745 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1746 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1747 | `		return PH7_OK;` |
|     - | 1748 | `	}` |
|     - | 1749 | `	/* Extract the target string */` |
|    73 | 1750 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    73 | 1751 | `	zEnd = &zIn[nLen];` |
|    73 | 1752 | `	if( nLen < 1 ){` |
|     - | 1753 | `		/* Empty string,return FALSE */` |
|     3 | 1754 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1755 | `		return PH7_OK;` |
|     - | 1756 | `	}` |
|     - | 1757 | `	/* Perform the requested operation */` |
|   110 | 1758 | `	for(;;){` |
|   221 | 1759 | `		if( zIn >= zEnd ){` |
|     - | 1760 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    65 | 1761 | `			ph7_result_bool(pCtx,1);` |
|    65 | 1762 | `			return PH7_OK;` |
|     - | 1763 | `		}` |
|   157 | 1764 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|     7 | 1765 | `			break;` |
|     - | 1766 | `		}` |
|     - | 1767 | `		/* Point to the next character */` |
|   151 | 1768 | `		zIn++;` |
|     1 | 1769 | `	}` |
|     - | 1770 | `	/* The test failed,return FALSE */` |
|     7 | 1771 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1772 | `	return PH7_OK;` |
|    37 | 1773 | `}` |
|     - | 1774 | `/*` |
|     - | 1775 | ` * bool ctype_alpha(string $text)` |
|     - | 1776 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|     - | 1777 | ` * Parameters` |
|     - | 1778 | ` *  $text` |
|     - | 1779 | ` *   The tested string.` |
|     - | 1780 | ` * Return` |
|     - | 1781 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|     - | 1782 | ` */` |
|    16 | 1783 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1784 | `{` |
|     - | 1785 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1786 | `	int nLen;` |
|    17 | 1787 | `	if( nArg < 1 ){` |
|     - | 1788 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1789 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1790 | `		return PH7_OK;` |
|     - | 1791 | `	}` |
|     - | 1792 | `	/* Extract the target string */` |
|    17 | 1793 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1794 | `	zEnd = &zIn[nLen];` |
|    17 | 1795 | `	if( nLen < 1 ){` |
|     - | 1796 | `		/* Empty string,return FALSE */` |
|     3 | 1797 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1798 | `		return PH7_OK;` |
|     - | 1799 | `	}` |
|     - | 1800 | `	/* Perform the requested operation */` |
|    42 | 1801 | `	for(;;){` |
|    85 | 1802 | `		if( zIn >= zEnd ){` |
|     - | 1803 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 1804 | `			ph7_result_bool(pCtx,1);` |
|     9 | 1805 | `			return PH7_OK;` |
|     - | 1806 | `		}` |
|    77 | 1807 | `		if( !SyisAlpha(zIn[0]) ){` |
|     7 | 1808 | `			break;` |
|     - | 1809 | `		}` |
|     - | 1810 | `		/* Point to the next character */` |
|    71 | 1811 | `		zIn++;` |
|     1 | 1812 | `	}` |
|     - | 1813 | `	/* The test failed,return FALSE */` |
|     7 | 1814 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1815 | `	return PH7_OK;` |
|     9 | 1816 | `}` |
|     - | 1817 | `/*` |
|     - | 1818 | ` * bool ctype_cntrl(string $text)` |
|     - | 1819 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|     - | 1820 | ` * Parameters` |
|     - | 1821 | ` *  $text` |
|     - | 1822 | ` *   The tested string.` |
|     - | 1823 | ` * Return` |
|     - | 1824 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|     - | 1825 | ` */` |
|    16 | 1826 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1827 | `{` |
|     - | 1828 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1829 | `	int nLen;` |
|    17 | 1830 | `	if( nArg < 1 ){` |
|     - | 1831 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1832 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1833 | `		return PH7_OK;` |
|     - | 1834 | `	}` |
|     - | 1835 | `	/* Extract the target string */` |
|    17 | 1836 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1837 | `	zEnd = &zIn[nLen];` |
|    17 | 1838 | `	if( nLen < 1 ){` |
|     - | 1839 | `		/* Empty string,return FALSE */` |
|     3 | 1840 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1841 | `		return PH7_OK;` |
|     - | 1842 | `	}` |
|     - | 1843 | `	/* Perform the requested operation */` |
|    14 | 1844 | `	for(;;){` |
|    29 | 1845 | `		if( zIn >= zEnd ){` |
|     - | 1846 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 1847 | `			ph7_result_bool(pCtx,1);` |
|     9 | 1848 | `			return PH7_OK;` |
|     - | 1849 | `		}` |
|    21 | 1850 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1851 | `			/* UTF-8 stream  */` |
|   ! 0 | 1852 | `			break;` |
|     - | 1853 | `		}` |
|    21 | 1854 | `		if( !SyisCtrl(zIn[0]) ){` |
|     7 | 1855 | `			break;` |
|     - | 1856 | `		}` |
|     - | 1857 | `		/* Point to the next character */` |
|    15 | 1858 | `		zIn++;` |
|     1 | 1859 | `	}` |
|     - | 1860 | `	/* The test failed,return FALSE */` |
|     7 | 1861 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1862 | `	return PH7_OK;` |
|     9 | 1863 | `}` |
|     - | 1864 | `/*` |
|     - | 1865 | ` * bool ctype_digit(string $text)` |
|     - | 1866 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|     - | 1867 | ` * Parameters` |
|     - | 1868 | ` *  $text` |
|     - | 1869 | ` *   The tested string.` |
|     - | 1870 | ` * Return` |
|     - | 1871 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|     - | 1872 | ` */` |
|  1806 | 1873 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 1874 | `{` |
|     - | 1875 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1876 | `	int nLen;` |
|  1811 | 1877 | `	if( nArg < 1 ){` |
|     - | 1878 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1879 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1880 | `		return PH7_OK;` |
|     - | 1881 | `	}` |
|     - | 1882 | `	/* Extract the target string */` |
|  1811 | 1883 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  1811 | 1884 | `	zEnd = &zIn[nLen];` |
|  1811 | 1885 | `	if( nLen < 1 ){` |
|     - | 1886 | `		/* Empty string,return FALSE */` |
|     9 | 1887 | `		ph7_result_bool(pCtx,0);` |
|     9 | 1888 | `		return PH7_OK;` |
|     - | 1889 | `	}` |
|     - | 1890 | `	/* Perform the requested operation */` |
|  1679 | 1891 | `	for(;;){` |
|  3363 | 1892 | `		if( zIn >= zEnd ){` |
|     - | 1893 | `			/* If we reach the end of the string,then the test succeeded. */` |
|  1497 | 1894 | `			ph7_result_bool(pCtx,1);` |
|  1497 | 1895 | `			return PH7_OK;` |
|     - | 1896 | `		}` |
|  1871 | 1897 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1898 | `			/* UTF-8 stream  */` |
|   ! 0 | 1899 | `			break;` |
|     - | 1900 | `		}` |
|  1871 | 1901 | `		if( !SyisDigit(zIn[0]) ){` |
|   311 | 1902 | `			break;` |
|     - | 1903 | `		}` |
|     - | 1904 | `		/* Point to the next character */` |
|  1565 | 1905 | `		zIn++;` |
|     5 | 1906 | `	}` |
|     - | 1907 | `	/* The test failed,return FALSE */` |
|   311 | 1908 | `	ph7_result_bool(pCtx,0);` |
|   311 | 1909 | `	return PH7_OK;` |
|   908 | 1910 | `}` |
|     - | 1911 | `/*` |
|     - | 1912 | ` * bool ctype_xdigit(string $text)` |
|     - | 1913 | ` *  Check for character(s) representing a hexadecimal digit.` |
|     - | 1914 | ` * Parameters` |
|     - | 1915 | ` *  $text` |
|     - | 1916 | ` *   The tested string.` |
|     - | 1917 | ` * Return` |
|     - | 1918 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|     - | 1919 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|     - | 1920 | ` */` |
|    38 | 1921 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1922 | `{` |
|     - | 1923 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1924 | `	int nLen;` |
|    40 | 1925 | `	if( nArg < 1 ){` |
|     - | 1926 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1927 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1928 | `		return PH7_OK;` |
|     - | 1929 | `	}` |
|     - | 1930 | `	/* Extract the target string */` |
|    40 | 1931 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    40 | 1932 | `	zEnd = &zIn[nLen];` |
|    40 | 1933 | `	if( nLen < 1 ){` |
|     - | 1934 | `		/* Empty string,return FALSE */` |
|     3 | 1935 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1936 | `		return PH7_OK;` |
|     - | 1937 | `	}` |
|     - | 1938 | `	/* Perform the requested operation */` |
|    76 | 1939 | `	for(;;){` |
|   154 | 1940 | `		if( zIn >= zEnd ){` |
|     - | 1941 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    32 | 1942 | `			ph7_result_bool(pCtx,1);` |
|    32 | 1943 | `			return PH7_OK;` |
|     - | 1944 | `		}` |
|   124 | 1945 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1946 | `			/* UTF-8 stream  */` |
|   ! 0 | 1947 | `			break;` |
|     - | 1948 | `		}` |
|   124 | 1949 | `		if( !SyisHex(zIn[0]) ){` |
|     7 | 1950 | `			break;` |
|     - | 1951 | `		}` |
|     - | 1952 | `		/* Point to the next character */` |
|   118 | 1953 | `		zIn++;` |
|     2 | 1954 | `	}` |
|     - | 1955 | `	/* The test failed,return FALSE */` |
|     7 | 1956 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1957 | `	return PH7_OK;` |
|    21 | 1958 | `}` |
|     - | 1959 | `/*` |
|     - | 1960 | ` * bool ctype_graph(string $text)` |
|     - | 1961 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|     - | 1962 | ` * Parameters` |
|     - | 1963 | ` *  $text` |
|     - | 1964 | ` *   The tested string.` |
|     - | 1965 | ` * Return` |
|     - | 1966 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|     - | 1967 | ` * (no white space), FALSE otherwise.` |
|     - | 1968 | ` */` |
|    16 | 1969 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1970 | `{` |
|     - | 1971 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1972 | `	int nLen;` |
|    17 | 1973 | `	if( nArg < 1 ){` |
|     - | 1974 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1975 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1976 | `		return PH7_OK;` |
|     - | 1977 | `	}` |
|     - | 1978 | `	/* Extract the target string */` |
|    17 | 1979 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 1980 | `	zEnd = &zIn[nLen];` |
|    17 | 1981 | `	if( nLen < 1 ){` |
|     - | 1982 | `		/* Empty string,return FALSE */` |
|     3 | 1983 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1984 | `		return PH7_OK;` |
|     - | 1985 | `	}` |
|     - | 1986 | `	/* Perform the requested operation */` |
|    57 | 1987 | `	for(;;){` |
|   115 | 1988 | `		if( zIn >= zEnd ){` |
|     - | 1989 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 1990 | `			ph7_result_bool(pCtx,1);` |
|     9 | 1991 | `			return PH7_OK;` |
|     - | 1992 | `		}` |
|   107 | 1993 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 1994 | `			/* UTF-8 stream  */` |
|   ! 0 | 1995 | `			break;` |
|     - | 1996 | `		}` |
|   107 | 1997 | `		if( !SyisGraph(zIn[0]) ){` |
|     7 | 1998 | `			break;` |
|     - | 1999 | `		}` |
|     - | 2000 | `		/* Point to the next character */` |
|   101 | 2001 | `		zIn++;` |
|     1 | 2002 | `	}` |
|     - | 2003 | `	/* The test failed,return FALSE */` |
|     7 | 2004 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2005 | `	return PH7_OK;` |
|     9 | 2006 | `}` |
|     - | 2007 | `/*` |
|     - | 2008 | ` * bool ctype_print(string $text)` |
|     - | 2009 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|     - | 2010 | ` * Parameters` |
|     - | 2011 | ` *  $text` |
|     - | 2012 | ` *   The tested string.` |
|     - | 2013 | ` * Return` |
|     - | 2014 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|     - | 2015 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|     - | 2016 | ` *  or control function at all.` |
|     - | 2017 | ` */` |
|    16 | 2018 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2019 | `{` |
|     - | 2020 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2021 | `	int nLen;` |
|    17 | 2022 | `	if( nArg < 1 ){` |
|     - | 2023 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2024 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2025 | `		return PH7_OK;` |
|     - | 2026 | `	}` |
|     - | 2027 | `	/* Extract the target string */` |
|    17 | 2028 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2029 | `	zEnd = &zIn[nLen];` |
|    17 | 2030 | `	if( nLen < 1 ){` |
|     - | 2031 | `		/* Empty string,return FALSE */` |
|     3 | 2032 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2033 | `		return PH7_OK;` |
|     - | 2034 | `	}` |
|     - | 2035 | `	/* Perform the requested operation */` |
|    63 | 2036 | `	for(;;){` |
|   127 | 2037 | `		if( zIn >= zEnd ){` |
|     - | 2038 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2039 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2040 | `			return PH7_OK;` |
|     - | 2041 | `		}` |
|   119 | 2042 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2043 | `			/* UTF-8 stream  */` |
|   ! 0 | 2044 | `			break;` |
|     - | 2045 | `		}` |
|   119 | 2046 | `		if( !SyisPrint(zIn[0]) ){` |
|     7 | 2047 | `			break;` |
|     - | 2048 | `		}` |
|     - | 2049 | `		/* Point to the next character */` |
|   113 | 2050 | `		zIn++;` |
|     1 | 2051 | `	}` |
|     - | 2052 | `	/* The test failed,return FALSE */` |
|     7 | 2053 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2054 | `	return PH7_OK;` |
|     9 | 2055 | `}` |
|     - | 2056 | `/*` |
|     - | 2057 | ` * bool ctype_punct(string $text)` |
|     - | 2058 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|     - | 2059 | ` * Parameters` |
|     - | 2060 | ` *  $text` |
|     - | 2061 | ` *   The tested string.` |
|     - | 2062 | ` * Return` |
|     - | 2063 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|     - | 2064 | ` *  digit or blank, FALSE otherwise.` |
|     - | 2065 | ` */` |
|    18 | 2066 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2067 | `{` |
|     - | 2068 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2069 | `	int nLen;` |
|    19 | 2070 | `	if( nArg < 1 ){` |
|     - | 2071 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2072 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2073 | `		return PH7_OK;` |
|     - | 2074 | `	}` |
|     - | 2075 | `	/* Extract the target string */` |
|    19 | 2076 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 2077 | `	zEnd = &zIn[nLen];` |
|    19 | 2078 | `	if( nLen < 1 ){` |
|     - | 2079 | `		/* Empty string,return FALSE */` |
|     3 | 2080 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2081 | `		return PH7_OK;` |
|     - | 2082 | `	}` |
|     - | 2083 | `	/* Perform the requested operation */` |
|    38 | 2084 | `	for(;;){` |
|    77 | 2085 | `		if( zIn >= zEnd ){` |
|     - | 2086 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2087 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2088 | `			return PH7_OK;` |
|     - | 2089 | `		}` |
|    69 | 2090 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2091 | `			/* UTF-8 stream  */` |
|   ! 0 | 2092 | `			break;` |
|     - | 2093 | `		}` |
|    69 | 2094 | `		if( !SyisPunct(zIn[0]) ){` |
|     9 | 2095 | `			break;` |
|     - | 2096 | `		}` |
|     - | 2097 | `		/* Point to the next character */` |
|    61 | 2098 | `		zIn++;` |
|     1 | 2099 | `	}` |
|     - | 2100 | `	/* The test failed,return FALSE */` |
|     9 | 2101 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2102 | `	return PH7_OK;` |
|    10 | 2103 | `}` |
|     - | 2104 | `/*` |
|     - | 2105 | ` * bool ctype_space(string $text)` |
|     - | 2106 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|     - | 2107 | ` * Parameters` |
|     - | 2108 | ` *  $text` |
|     - | 2109 | ` *   The tested string.` |
|     - | 2110 | ` * Return` |
|     - | 2111 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|     - | 2112 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|     - | 2113 | ` *  and form feed characters.` |
|     - | 2114 | ` */` |
| 16013 | 2115 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 2116 | `{` |
|     - | 2117 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2118 | `	int nLen;` |
| 16018 | 2119 | `	if( nArg < 1 ){` |
|     - | 2120 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2121 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2122 | `		return PH7_OK;` |
|     - | 2123 | `	}` |
|     - | 2124 | `	/* Extract the target string */` |
| 16018 | 2125 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
| 16018 | 2126 | `	zEnd = &zIn[nLen];` |
| 16018 | 2127 | `	if( nLen < 1 ){` |
|     - | 2128 | `		/* Empty string,return FALSE */` |
|     3 | 2129 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2130 | `		return PH7_OK;` |
|     - | 2131 | `	}` |
|     - | 2132 | `	/* Perform the requested operation */` |
|  8078 | 2133 | `	for(;;){` |
| 16050 | 2134 | `		if( zIn >= zEnd ){` |
|     - | 2135 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    11 | 2136 | `			ph7_result_bool(pCtx,1);` |
|    11 | 2137 | `			return PH7_OK;` |
|     - | 2138 | `		}` |
| 16040 | 2139 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2140 | `			/* UTF-8 stream  */` |
|   ! 0 | 2141 | `			break;` |
|     - | 2142 | `		}` |
| 16040 | 2143 | `		if( !SyisSpace(zIn[0]) ){` |
| 16006 | 2144 | `			break;` |
|     - | 2145 | `		}` |
|     - | 2146 | `		/* Point to the next character */` |
|    35 | 2147 | `		zIn++;` |
|     1 | 2148 | `	}` |
|     - | 2149 | `	/* The test failed,return FALSE */` |
| 16006 | 2150 | `	ph7_result_bool(pCtx,0);` |
| 16006 | 2151 | `	return PH7_OK;` |
|  8067 | 2152 | `}` |
|     - | 2153 | `/*` |
|     - | 2154 | ` * bool ctype_lower(string $text)` |
|     - | 2155 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|     - | 2156 | ` * Parameters` |
|     - | 2157 | ` *  $text` |
|     - | 2158 | ` *   The tested string.` |
|     - | 2159 | ` * Return` |
|     - | 2160 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|     - | 2161 | ` */` |
|    16 | 2162 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2163 | `{` |
|     - | 2164 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2165 | `	int nLen;` |
|    17 | 2166 | `	if( nArg < 1 ){` |
|     - | 2167 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2168 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2169 | `		return PH7_OK;` |
|     - | 2170 | `	}` |
|     - | 2171 | `	/* Extract the target string */` |
|    17 | 2172 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2173 | `	zEnd = &zIn[nLen];` |
|    17 | 2174 | `	if( nLen < 1 ){` |
|     - | 2175 | `		/* Empty string,return FALSE */` |
|     3 | 2176 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2177 | `		return PH7_OK;` |
|     - | 2178 | `	}` |
|     - | 2179 | `	/* Perform the requested operation */` |
|    27 | 2180 | `	for(;;){` |
|    55 | 2181 | `		if( zIn >= zEnd ){` |
|     - | 2182 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2183 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2184 | `			return PH7_OK;` |
|     - | 2185 | `		}` |
|    51 | 2186 | `		if( !SyisLower(zIn[0]) ){` |
|    11 | 2187 | `			break;` |
|     - | 2188 | `		}` |
|     - | 2189 | `		/* Point to the next character */` |
|    41 | 2190 | `		zIn++;` |
|     1 | 2191 | `	}` |
|     - | 2192 | `	/* The test failed,return FALSE */` |
|    11 | 2193 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2194 | `	return PH7_OK;` |
|     9 | 2195 | `}` |
|     - | 2196 | `/*` |
|     - | 2197 | ` * bool ctype_upper(string $text)` |
|     - | 2198 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|     - | 2199 | ` * Parameters` |
|     - | 2200 | ` *  $text` |
|     - | 2201 | ` *   The tested string.` |
|     - | 2202 | ` * Return` |
|     - | 2203 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|     - | 2204 | ` */` |
|    16 | 2205 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2206 | `{` |
|     - | 2207 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2208 | `	int nLen;` |
|    17 | 2209 | `	if( nArg < 1 ){` |
|     - | 2210 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2211 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2212 | `		return PH7_OK;` |
|     - | 2213 | `	}` |
|     - | 2214 | `	/* Extract the target string */` |
|    17 | 2215 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2216 | `	zEnd = &zIn[nLen];` |
|    17 | 2217 | `	if( nLen < 1 ){` |
|     - | 2218 | `		/* Empty string,return FALSE */` |
|     3 | 2219 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2220 | `		return PH7_OK;` |
|     - | 2221 | `	}` |
|     - | 2222 | `	/* Perform the requested operation */` |
|    28 | 2223 | `	for(;;){` |
|    57 | 2224 | `		if( zIn >= zEnd ){` |
|     - | 2225 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2226 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2227 | `			return PH7_OK;` |
|     - | 2228 | `		}` |
|    53 | 2229 | `		if( !SyisUpper(zIn[0]) ){` |
|    11 | 2230 | `			break;` |
|     - | 2231 | `		}` |
|     - | 2232 | `		/* Point to the next character */` |
|    43 | 2233 | `		zIn++;` |
|     1 | 2234 | `	}` |
|     - | 2235 | `	/* The test failed,return FALSE */` |
|    11 | 2236 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2237 | `	return PH7_OK;` |
|     9 | 2238 | `}` |
|     - | 2239 | `/* Date/Time functions moved to builtin_date.c */` |
|     - | 2240 | `/*` |
|     - | 2241 | ` * Section:` |
|     - | 2242 | ` *    URL handling Functions.` |
|     - | 2243 | ` * Status:` |
|     - | 2244 | ` *    Stable.` |
|     - | 2245 | ` */` |
|     - | 2246 | `/*` |
|     - | 2247 | ` * Output consumer callback for the standard Symisc routines.` |
|     - | 2248 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|     - | 2249 | ` */` |
|   282 | 2250 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     1 | 2251 | `{` |
|     - | 2252 | `	/* Store in the call context result buffer */` |
|   283 | 2253 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   283 | 2254 | `	return SXRET_OK;` |
|     1 | 2255 | `}` |
|     - | 2256 | `/*` |
|     - | 2257 | ` * string base64_encode(string $data)` |
|     - | 2258 | ` * string convert_uuencode(string $data)` |
|     - | 2259 | ` *  Encodes data with MIME base64` |
|     - | 2260 | ` * Parameter` |
|     - | 2261 | ` *  $data` |
|     - | 2262 | ` *    Data to encode` |
|     - | 2263 | ` * Return` |
|     - | 2264 | ` *  Encoded data or FALSE on failure.` |
|     - | 2265 | ` */` |
|     6 | 2266 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2267 | `{` |
|     - | 2268 | `	const char *zIn;` |
|     - | 2269 | `	int nLen;` |
|     7 | 2270 | `	if( nArg < 1 ){` |
|     - | 2271 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2272 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2273 | `		return PH7_OK;` |
|     - | 2274 | `	}` |
|     - | 2275 | `	/* Extract the input string */` |
|     7 | 2276 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     7 | 2277 | `	if( nLen < 1 ){` |
|     - | 2278 | `		/* Nothing to process,return FALSE */` |
|   ! 0 | 2279 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2280 | `		return PH7_OK;` |
|     - | 2281 | `	}` |
|     - | 2282 | `	/* Perform the BASE64 encoding */` |
|     7 | 2283 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     7 | 2284 | `	return PH7_OK;` |
|     4 | 2285 | `}` |
|     - | 2286 | `/*` |
|     - | 2287 | ` * string base64_decode(string $data)` |
|     - | 2288 | ` * string convert_uudecode(string $data)` |
|     - | 2289 | ` *  Decodes data encoded with MIME base64` |
|     - | 2290 | ` * Parameter` |
|     - | 2291 | ` *  $data` |
|     - | 2292 | ` *    Encoded data.` |
|     - | 2293 | ` * Return` |
|     - | 2294 | ` *  Returns the original data or FALSE on failure.` |
|     - | 2295 | ` */` |
|     8 | 2296 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2297 | `{` |
|     - | 2298 | `	const char *zIn;` |
|     - | 2299 | `	int nLen;` |
|     9 | 2300 | `	if( nArg < 1 ){` |
|     - | 2301 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2302 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2303 | `		return PH7_OK;` |
|     - | 2304 | `	}` |
|     - | 2305 | `	/* Extract the input string */` |
|     9 | 2306 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     9 | 2307 | `	if( nLen < 1 ){` |
|     - | 2308 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|     - | 2309 | `		 * for input that cannot be decoded at all). */` |
|     3 | 2310 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 2311 | `		return PH7_OK;` |
|     - | 2312 | `	}` |
|     - | 2313 | `	/* Perform the BASE64 decoding */` |
|     7 | 2314 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     7 | 2315 | `	return PH7_OK;` |
|     5 | 2316 | `}` |
|     - | 2317 | `/*` |
|     - | 2318 | ` * string urlencode(string $str)` |
|     - | 2319 | ` *  URL encoding` |
|     - | 2320 | ` * Parameter` |
|     - | 2321 | ` *  $data` |
|     - | 2322 | ` *   Input string.` |
|     - | 2323 | ` * Return` |
|     - | 2324 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|     - | 2325 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|     - | 2326 | ` *  encoded as plus (+) signs.` |
|     - | 2327 | ` */` |
|   100 | 2328 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2329 | `{` |
|     - | 2330 | `	const char *zIn;` |
|     - | 2331 | `	int nLen;` |
|   101 | 2332 | `	if( nArg < 1 ){` |
|     - | 2333 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2334 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2335 | `		return PH7_OK;` |
|     - | 2336 | `	}` |
|     - | 2337 | `	/* Extract the input string */` |
|   101 | 2338 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   101 | 2339 | `	if( nLen < 1 ){` |
|     - | 2340 | `		/* php returns an empty string for empty input, not FALSE */` |
|     3 | 2341 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 2342 | `		return PH7_OK;` |
|     - | 2343 | `	}` |
|     - | 2344 | `	/* Perform the URL encoding */` |
|    99 | 2345 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    99 | 2346 | `	return PH7_OK;` |
|    51 | 2347 | `}` |
|     - | 2348 | `/*` |
|     - | 2349 | ` * string rawurlencode(string $str)` |
|     - | 2350 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|     - | 2351 | ` */` |
|    14 | 2352 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2353 | `{` |
|     - | 2354 | `	const char *zIn;` |
|     - | 2355 | `	int nLen;` |
|    15 | 2356 | `	if( nArg < 1 ){` |
|     - | 2357 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2358 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2359 | `		return PH7_OK;` |
|     - | 2360 | `	}` |
|     - | 2361 | `	/* Extract the input string */` |
|    15 | 2362 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    15 | 2363 | `	if( nLen < 1 ){` |
|     - | 2364 | `		/* php returns an empty string for empty input, not FALSE */` |
|     3 | 2365 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 2366 | `		return PH7_OK;` |
|     - | 2367 | `	}` |
|     - | 2368 | `	/* Perform the RFC 3986 URL encoding */` |
|    13 | 2369 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    13 | 2370 | `	return PH7_OK;` |
|     8 | 2371 | `}` |
|     - | 2372 | `/*` |
|     - | 2373 | ` * string urldecode(string $str)` |
|     - | 2374 | ` *  Decodes any %## encoding in the given string.` |
|     - | 2375 | ` *  Plus symbols ('+') are decoded to a space character.` |
|     - | 2376 | ` * Parameter` |
|     - | 2377 | ` *  $data` |
|     - | 2378 | ` *    Input string.` |
|     - | 2379 | ` * Return` |
|     - | 2380 | ` *  Decoded URL or FALSE on failure.` |
|     - | 2381 | ` */` |
|   110 | 2382 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2383 | `{` |
|     - | 2384 | `	const char *zIn;` |
|     - | 2385 | `	int nLen;` |
|   111 | 2386 | `	if( nArg < 1 ){` |
|     - | 2387 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2388 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2389 | `		return PH7_OK;` |
|     - | 2390 | `	}` |
|     - | 2391 | `	/* Extract the input string */` |
|   111 | 2392 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   111 | 2393 | `	if( nLen < 1 ){` |
|     - | 2394 | `		/* php returns an empty string for empty input, not FALSE */` |
|    17 | 2395 | `		ph7_result_string(pCtx,"",0);` |
|    17 | 2396 | `		return PH7_OK;` |
|     - | 2397 | `	}` |
|     - | 2398 | `	/* Perform the URL decoding */` |
|    95 | 2399 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|    95 | 2400 | `	return PH7_OK;` |
|    56 | 2401 | `}` |
|     - | 2402 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 2403 |  |
