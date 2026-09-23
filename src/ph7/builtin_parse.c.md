# src/ph7/builtin_parse.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1868/2023 lines (92.34%)

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
|   155 |   72 | `static void FvTrim(const char **pz,int *pn){` |
|   155 |   73 | `	const char *z = *pz;` |
|   155 |   74 | `	int n = *pn;` |
|   159 |   75 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|   163 |   76 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|   155 |   77 | `	*pz = z; *pn = n;` |
|   155 |   78 | `}` |
|     - |   79 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|    59 |   80 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|    59 |   81 | `	int neg = 0, i;` |
|    59 |   82 | `	sxu64 u = 0;` |
|    59 |   83 | `	FvTrim(&z,&n);` |
|    59 |   84 | `	if( n==0 ){ return 0; }` |
|    53 |   85 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|    53 |   86 | `	if( n==0 ){ return 0; }` |
|    51 |   87 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|     3 |   88 | `		z += 2; n -= 2;` |
|     3 |   89 | `		if( n==0 ){ return 0; }` |
|     7 |   90 | `		for( i=0; i<n; i++ ){` |
|     5 |   91 | `			int h = SyHexToint((unsigned char)z[i]);` |
|     5 |   92 | `			if( h<0 ){ return 0; }` |
|     5 |   93 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|     5 |   94 | `			u = u*16 + (sxu64)h;` |
|     3 |   95 | `		}` |
|    50 |   96 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|     9 |   97 | `		for( i=0; i<n; i++ ){` |
|     7 |   98 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|     7 |   99 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|     7 |  100 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|     4 |  101 | `		}` |
|     2 |  102 | `	}else{` |
|    47 |  103 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|   205 |  104 | `		for( i=0; i<n; i++ ){` |
|   175 |  105 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|   163 |  106 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|   163 |  107 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|    82 |  108 | `		}` |
|     - |  109 | `	}` |
|    35 |  110 | `	if( neg ){` |
|     5 |  111 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|     5 |  112 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|     3 |  113 | `	}else{` |
|    31 |  114 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|    29 |  115 | `		*pOut = (ph7_int64)u;` |
|     - |  116 | `	}` |
|    33 |  117 | `	return 1;` |
|    30 |  118 | `}` |
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
|   318 |  979 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|     - |  980 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|     - |  981 | `                         ph7_value *pDefault)` |
|     3 |  982 | `{` |
|   321 |  983 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|     - |  984 | `	const char *zVal; int nVal;` |
|     - |  985 | `	/* An array/object input fails every scalar filter. */` |
|   321 |  986 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|   319 |  987 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|   319 |  988 | `	switch( iFilter ){` |
|    29 |  989 | `	case FV_VALIDATE_INT: {` |
|     - |  990 | `		ph7_int64 v;` |
|    60 |  991 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|    33 |  992 | `		if( pOpts ){` |
|     7 |  993 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|     7 |  994 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|     7 |  995 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|     7 |  996 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|     2 |  997 | `		}` |
|    31 |  998 | `		ph7_result_int64(pCtx,v);` |
|    31 |  999 | `		return PH7_OK;` |
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
|   162 | 1060 | `}` |
|     - | 1061 | `/*` |
|     - | 1062 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|     - | 1063 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|     - | 1064 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|     - | 1065 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|     - | 1066 | ` * unset outputs keep the caller-provided defaults.` |
|     - | 1067 | ` */` |
|   330 | 1068 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|     - | 1069 | `                              int *piFilter,int *piFlags,` |
|     - | 1070 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|     3 | 1071 | `{` |
|   333 | 1072 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|   333 | 1073 | `	if( nArg>iBase+1 ){` |
|    90 | 1074 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|    42 | 1075 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|    42 | 1076 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|    42 | 1077 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|    42 | 1078 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|    42 | 1079 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|    22 | 1080 | `		}else{` |
|    50 | 1081 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|     - | 1082 | `		}` |
|    44 | 1083 | `	}` |
|   333 | 1084 | `}` |
|     - | 1085 | `/*` |
|     - | 1086 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|     - | 1087 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|     - | 1088 | ` */` |
|   308 | 1089 | `PH7_PRIVATE int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1090 | `{` |
|   310 | 1091 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|   310 | 1092 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|   310 | 1093 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|   310 | 1094 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|   310 | 1095 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|   156 | 1096 | `}` |
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
|    18 | 1154 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|     - | 1155 | `	const char *zInput, /* Raw input */` |
|     - | 1156 | `	int nByte,  /* Input length */` |
|     - | 1157 | `	int delim,  /* Delimiter */` |
|     - | 1158 | `	int encl,   /* Enclosure */` |
|     - | 1159 | `	int escape,  /* Escape character */` |
|     - | 1160 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|     - | 1161 | `	void *pUserData /* Last argument to xConsumer() */` |
|     - | 1162 | `	)` |
|     1 | 1163 | `{` |
|    19 | 1164 | `	const char *zEnd = &zInput[nByte];` |
|    19 | 1165 | `	const char *zIn = zInput;` |
|     - | 1166 | `	const char *zPtr;` |
|     - | 1167 | `	int isEnc;` |
|     - | 1168 | `	/* Start processing */` |
|    30 | 1169 | `	for(;;){` |
|    61 | 1170 | `		if( zIn >= zEnd ){` |
|     - | 1171 | `			/* No more input to process */` |
|    19 | 1172 | `			break;` |
|     - | 1173 | `		}` |
|    43 | 1174 | `		isEnc = 0;` |
|    43 | 1175 | `		zPtr = zIn;` |
|     - | 1176 | `		/* Find the first delimiter */` |
|    99 | 1177 | `		while( zIn < zEnd ){` |
|    81 | 1178 | `			if( zIn[0] == delim && !isEnc){` |
|     - | 1179 | `				/* Delimiter found,break imediately */` |
|    13 | 1180 | `				break;` |
|    57 | 1181 | `			}else if( zIn[0] == encl ){` |
|     - | 1182 | `				/* Inside enclosure? */` |
|   ! 0 | 1183 | `				isEnc = !isEnc;` |
|    57 | 1184 | `			}else if( zIn[0] == escape ){` |
|     - | 1185 | `				/* Escape sequence */` |
|   ! 0 | 1186 | `				zIn++;` |
|   ! 0 | 1187 | `			}` |
|     - | 1188 | `			/* Advance the cursor */` |
|    57 | 1189 | `			zIn++;` |
|     1 | 1190 | `		}` |
|    43 | 1191 | `		if( zIn > zPtr ){` |
|    43 | 1192 | `			int nByteChunk = (int)(zIn-zPtr);` |
|     - | 1193 | `			sxi32 rc;` |
|     - | 1194 | `			/* Invoke the supllied callback */` |
|    43 | 1195 | `			if( zPtr[0] == encl ){` |
|   ! 0 | 1196 | `				zPtr++;` |
|   ! 0 | 1197 | `				nByteChunk-=2;` |
|   ! 0 | 1198 | `			}` |
|    43 | 1199 | `			if( nByteChunk > 0 ){` |
|    43 | 1200 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|    43 | 1201 | `				if( rc == SXERR_ABORT ){` |
|     - | 1202 | `					/* User callback request an operation abort */` |
|   ! 0 | 1203 | `					break;` |
|     - | 1204 | `				}` |
|    21 | 1205 | `			}` |
|    21 | 1206 | `		}` |
|     - | 1207 | `		/* Ignore trailing delimiter */` |
|    67 | 1208 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|    25 | 1209 | `			zIn++;` |
|     1 | 1210 | `		}` |
|     1 | 1211 | `	}` |
|    19 | 1212 | `	return SXRET_OK;` |
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
|    42 | 1248 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|     1 | 1249 | `{` |
|    43 | 1250 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|     - | 1251 | `	ph7_value sEntry;` |
|     - | 1252 | `	SyString sToken;` |
|     - | 1253 | `	/* Insert the token in the given array */` |
|    43 | 1254 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|     - | 1255 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|    95 | 1256 | `	SyStringFullTrimSafe(&sToken);` |
|    43 | 1257 | `	if( sToken.nByte < 1){` |
|   ! 0 | 1258 | `		return SXRET_OK;` |
|     - | 1259 | `	}` |
|    43 | 1260 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|    43 | 1261 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|    43 | 1262 | `	PH7_MemObjRelease(&sEntry);` |
|    43 | 1263 | `	return SXRET_OK;` |
|    22 | 1264 | `}` |
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
|    18 | 1280 | `PH7_PRIVATE int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1281 | `{` |
|     - | 1282 | `	const char *zInput;` |
|     - | 1283 | `	ph7_value *pArray;` |
|    19 | 1284 | `	int delim  = ',';   /* Delimiter */` |
|    19 | 1285 | `	int encl   = '"' ;  /* Enclosure */` |
|    19 | 1286 | `	int escape = '\\';  /* Escape character */` |
|     - | 1287 | `	int nLen;` |
|    19 | 1288 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1289 | `		/* Missing/Invalid arguments,return NULL */` |
|   ! 0 | 1290 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1291 | `		return PH7_OK;` |
|     - | 1292 | `	}` |
|     - | 1293 | `	/* Extract the raw input */` |
|    19 | 1294 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 1295 | `	if( nArg > 1 ){` |
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
|     9 | 1314 | `	pArray = ph7_context_new_array(pCtx);` |
|     9 | 1315 | `	if( pArray == 0 ){` |
|     - | 1316 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|   ! 0 | 1317 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1318 | `	}` |
|     - | 1319 | `	/* Parse the raw input */` |
|     9 | 1320 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|     - | 1321 | `	/* Return the freshly created array */` |
|     9 | 1322 | `	ph7_result_value(pCtx,pArray);` |
|     9 | 1323 | `	return PH7_OK;` |
|    10 | 1324 | `}` |
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
|     - | 1537 | `/*` |
|     - | 1538 | ` * The ini scanner's ${NAME} expansion: php answers a known ini OPTION first and` |
|     - | 1539 | ` * the process environment second (zend_ini_get_var), the empty string when` |
|     - | 1540 | ` * neither knows the name — a defined CONSTANT deliberately does NOT answer` |
|     - | 1541 | ` * here (that is the bare-identifier rule below). The VFS environment reader` |
|     - | 1542 | ` * answers through the call context's RESULT slot (it was written for` |
|     - | 1543 | ` * getenv()), so the read borrows pCtx->pRet around the call and empties it` |
|     - | 1544 | ` * again; the parse's own result is not written until the very end.` |
|     - | 1545 | ` */` |
|     6 | 1546 | `static void VmIniExpandDollarVar(ph7_context *pCtx,const char *zName,sxu32 nName,SyBlob *pOut)` |
|     1 | 1547 | `{` |
|     - | 1548 | `	char zVar[128];` |
|     - | 1549 | `	SyBlob sVal;` |
|     7 | 1550 | `	if( nName < 1 \|\| nName >= sizeof(zVar) ){` |
|   ! 0 | 1551 | `		return; /* php answers "" for an unknown name; an unreasonable one is unknown */` |
|     - | 1552 | `	}` |
|     7 | 1553 | `	SyMemcpy(zName,zVar,nName);` |
|     7 | 1554 | `	zVar[nName] = 0;` |
|     7 | 1555 | `	SyBlobInit(&sVal,&pCtx->pVm->sAllocator);` |
|     7 | 1556 | `	PH7_VmIniGetStr(pCtx->pVm,zVar,&sVal);` |
|     7 | 1557 | `	if( SyBlobLength(&sVal) > 0 ){` |
|   ! 0 | 1558 | `		SyBlobAppend(pOut,SyBlobData(&sVal),SyBlobLength(&sVal));` |
|   ! 0 | 1559 | `		SyBlobRelease(&sVal);` |
|   ! 0 | 1560 | `		return;` |
|     - | 1561 | `	}` |
|     7 | 1562 | `	SyBlobRelease(&sVal);` |
|     - | 1563 | `	{` |
|     7 | 1564 | `		const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     7 | 1565 | `		ph7_value *pRet = pCtx->pRet;` |
|     7 | 1566 | `		sxu32 nBefore = SyBlobLength(&pRet->sBlob);` |
|     7 | 1567 | `		if( pVfs && pVfs->xGetenv ){` |
|     7 | 1568 | `			if( pVfs->xGetenv(zVar,pCtx) == PH7_OK && SyBlobLength(&pRet->sBlob) > nBefore ){` |
|     7 | 1569 | `				SyBlobAppend(pOut,(const char *)SyBlobData(&pRet->sBlob) + nBefore,` |
|     4 | 1570 | `					SyBlobLength(&pRet->sBlob) - nBefore);` |
|     2 | 1571 | `			}` |
|     7 | 1572 | `			ph7_value_reset_string_cursor(pRet);` |
|     3 | 1573 | `		}` |
|     - | 1574 | `	}` |
|     4 | 1575 | `}` |
|     - | 1576 | `/*` |
|     - | 1577 | ` * Interpret one UNQUOTED ini value the way php's INI_SCANNER_NORMAL and` |
|     - | 1578 | ` * INI_SCANNER_TYPED do (a QUOTED value is always its literal bytes, and RAW` |
|     - | 1579 | ` * never reaches here):` |
|     - | 1580 | ` *` |
|     - | 1581 | ` *  - A whole-value word, case-insensitive: true/on/yes and false/off/no/none` |
|     - | 1582 | ` *    and null. NORMAL renders them "1" / "" / ""; TYPED renders true / false /` |
|     - | 1583 | ` *    NULL.` |
|     - | 1584 | ` *  - TYPED only: -?[0-9]+ is an int — a value int64 cannot hold falls back to` |
|     - | 1585 | ` *    the SOURCE text as a string — and [0-9]*\.[0-9]* with at least one digit` |
|     - | 1586 | ` *    is a float. php's typed grammar attaches '-' only to the INTEGER shape` |
|     - | 1587 | ` *    ("-1.5" stays a string); '+', hex, binary and exponents were never in it.` |
|     - | 1588 | ` *  - Everything else expands: ${NAME} answers an ini option or the` |
|     - | 1589 | ` *    environment, and a bare identifier token that names a DEFINED constant is` |
|     - | 1590 | ` *    replaced by that constant's value ("MYC and more" -> "someval and more").` |
|     - | 1591 | ` *` |
|     - | 1592 | ` * pValue arrives as an empty string.` |
|     - | 1593 | ` */` |
|   128 | 1594 | `static void VmIniInterpretValue(ph7_context *pCtx,const SyString *pRaw,int iMode,ph7_value *pValue)` |
|     1 | 1595 | `{` |
|   129 | 1596 | `	const char *zIn = pRaw->zString;` |
|   129 | 1597 | `	const char *zEnd = &zIn[pRaw->nByte];` |
|   129 | 1598 | `	sxu32 n = pRaw->nByte;` |
|     - | 1599 | `	SyBlob sOut;` |
|   129 | 1600 | `	if( n == 0 ){` |
|   ! 0 | 1601 | `		return; /* the empty string, both modes */` |
|     - | 1602 | `	}` |
|   128 | 1603 | `	if( (n == 4 && SyStrnicmp(zIn,"true",4) == 0)` |
|   125 | 1604 | `	 \|\| (n == 2 && SyStrnicmp(zIn,"on",2) == 0)` |
|   118 | 1605 | `	 \|\| (n == 3 && SyStrnicmp(zIn,"yes",3) == 0) ){` |
|    15 | 1606 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|    11 | 1607 | `			ph7_value_bool(pValue,1);` |
|     6 | 1608 | `		}else{` |
|     5 | 1609 | `			ph7_value_string(pValue,"1",1);` |
|     - | 1610 | `		}` |
|    15 | 1611 | `		return;` |
|     - | 1612 | `	}` |
|   114 | 1613 | `	if( (n == 5 && SyStrnicmp(zIn,"false",5) == 0)` |
|   113 | 1614 | `	 \|\| (n == 3 && SyStrnicmp(zIn,"off",3) == 0)` |
|   111 | 1615 | `	 \|\| (n == 2 && SyStrnicmp(zIn,"no",2) == 0)` |
|   111 | 1616 | `	 \|\| (n == 4 && SyStrnicmp(zIn,"none",4) == 0) ){` |
|    13 | 1617 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|     9 | 1618 | `			ph7_value_bool(pValue,0);` |
|     4 | 1619 | `		}` |
|     - | 1620 | `		/* NORMAL: the empty string pValue already holds */` |
|    13 | 1621 | `		return;` |
|     - | 1622 | `	}` |
|   109 | 1623 | `	if( n == 4 && SyStrnicmp(zIn,"null",4) == 0 ){` |
|     5 | 1624 | `		if( iMode == PH7_INI_SCANNER_TYPED ){` |
|     3 | 1625 | `			ph7_value_null(pValue);` |
|     1 | 1626 | `		}` |
|     5 | 1627 | `		return;` |
|     - | 1628 | `	}` |
|   105 | 1629 | `	if( iMode == PH7_INI_SCANNER_TYPED ){` |
|    33 | 1630 | `		sxu32 i = 0;` |
|    33 | 1631 | `		sxu32 nDig = 0,nDot = 0;` |
|    33 | 1632 | `		int bNeg = 0,bNum = 1;` |
|    33 | 1633 | `		if( zIn[0] == '-' ){` |
|     5 | 1634 | `			bNeg = 1;` |
|     5 | 1635 | `			i = 1;` |
|     2 | 1636 | `		}` |
|   153 | 1637 | `		for( ; i < n ; i++ ){` |
|   131 | 1638 | `			if( zIn[i] >= '0' && zIn[i] <= '9' ){` |
|   113 | 1639 | `				nDig++;` |
|    75 | 1640 | `			}else if( zIn[i] == '.' ){` |
|     9 | 1641 | `				nDot++;` |
|     5 | 1642 | `			}else{` |
|    11 | 1643 | `				bNum = 0;` |
|    11 | 1644 | `				break;` |
|     - | 1645 | `			}` |
|    61 | 1646 | `		}` |
|    33 | 1647 | `		if( bNum && nDig > 0 && nDot == 0 ){` |
|    15 | 1648 | `			sxi64 iVal = 0;` |
|    15 | 1649 | `			int iOverflow = 0;` |
|    15 | 1650 | `			SyStrToInt64Ex(zIn,n,(void *)&iVal,0,&iOverflow);` |
|    15 | 1651 | `			if( !iOverflow ){` |
|    13 | 1652 | `				ph7_value_int64(pValue,iVal);` |
|    13 | 1653 | `				return;` |
|     - | 1654 | `			}` |
|     3 | 1655 | `			ph7_value_string(pValue,zIn,(int)n);` |
|     3 | 1656 | `			return;` |
|     - | 1657 | `		}` |
|    19 | 1658 | `		if( bNum && nDig > 0 && nDot == 1 && !bNeg ){` |
|     7 | 1659 | `			double rVal = 0;` |
|     7 | 1660 | `			SyStrToReal(zIn,n,(void *)&rVal,0);` |
|     7 | 1661 | `			ph7_value_double(pValue,rVal);` |
|     7 | 1662 | `			return;` |
|     - | 1663 | `		}` |
|     6 | 1664 | `	}` |
|    85 | 1665 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   169 | 1666 | `	while( zIn < zEnd ){` |
|   119 | 1667 | `		if( zIn[0] == '$' && &zIn[1] < zEnd && zIn[1] == '{' ){` |
|     7 | 1668 | `			const char *p = &zIn[2];` |
|    97 | 1669 | `			while( p < zEnd && p[0] != '}' ){` |
|    91 | 1670 | `				p++;` |
|     1 | 1671 | `			}` |
|     7 | 1672 | `			if( p < zEnd ){` |
|     7 | 1673 | `				VmIniExpandDollarVar(pCtx,&zIn[2],(sxu32)(p - &zIn[2]),&sOut);` |
|     7 | 1674 | `				zIn = &p[1];` |
|     7 | 1675 | `				continue;` |
|     - | 1676 | `			}` |
|     - | 1677 | `			/* No closing brace: the bytes stand as written */` |
|   ! 0 | 1678 | `		}` |
|   113 | 1679 | `		if( ((unsigned char)zIn[0] < 0xc0 && SyisAlpha(zIn[0])) \|\| zIn[0] == '_' ){` |
|    79 | 1680 | `			const char *pTok = zIn;` |
|     - | 1681 | `			ph7_value sCons;` |
|   218 | 1682 | `			while( zIn < zEnd` |
|   281 | 1683 | `			 && (((unsigned char)zIn[0] < 0xc0 && SyisAlphaNum(zIn[0])) \|\| zIn[0] == '_') ){` |
|   237 | 1684 | `				zIn++;` |
|     1 | 1685 | `			}` |
|    45 | 1686 | `			PH7_MemObjInit(pCtx->pVm,&sCons);` |
|    45 | 1687 | `			if( PH7_VmQueryConstant(pCtx->pVm,pTok,(sxu32)(zIn - pTok),&sCons) ){` |
|     - | 1688 | `				int nCons;` |
|     5 | 1689 | `				const char *zCons = ph7_value_to_string(&sCons,&nCons);` |
|     5 | 1690 | `				SyBlobAppend(&sOut,zCons,(sxu32)nCons);` |
|     3 | 1691 | `			}else{` |
|    41 | 1692 | `				SyBlobAppend(&sOut,pTok,(sxu32)(zIn - pTok));` |
|     - | 1693 | `			}` |
|    45 | 1694 | `			PH7_MemObjRelease(&sCons);` |
|    45 | 1695 | `			continue;` |
|     - | 1696 | `		}` |
|    35 | 1697 | `		SyBlobAppend(&sOut,zIn,(sxu32)sizeof(char));` |
|    35 | 1698 | `		zIn++;` |
|     1 | 1699 | `	}` |
|    51 | 1700 | `	if( SyBlobLength(&sOut) > 0 ){` |
|    49 | 1701 | `		ph7_value_string(pValue,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    24 | 1702 | `	}` |
|    51 | 1703 | `	SyBlobRelease(&sOut);` |
|    51 | 1704 | `}` |
|    26 | 1705 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection,int iScannerMode)` |
|     1 | 1706 | `{` |
|     - | 1707 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|    27 | 1708 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|     - | 1709 | `	SyHashEntry *pEntry;` |
|     - | 1710 | `	SyString sEntry;` |
|     - | 1711 | `	SyHash sHash;` |
|     - | 1712 | `	int c;` |
|     - | 1713 | `	/* Create an empty array and worker variables */` |
|    27 | 1714 | `	pArray = ph7_context_new_array(pCtx);` |
|    27 | 1715 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|    27 | 1716 | `	pValue = ph7_context_new_scalar(pCtx);` |
|    27 | 1717 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|     - | 1718 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|   ! 0 | 1719 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1720 | `	}` |
|    27 | 1721 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|    27 | 1722 | `	pCur = pArray;` |
|     - | 1723 | `	/* Start the parse process */` |
|    96 | 1724 | `	for(;;){` |
|     - | 1725 | `		/* Ignore leading white spaces */` |
|   355 | 1726 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|   159 | 1727 | `			zIn++;` |
|     1 | 1728 | `		}` |
|   197 | 1729 | `		if( zIn >= zEnd ){` |
|     - | 1730 | `			/* No more input to process */` |
|    27 | 1731 | `			break;` |
|     - | 1732 | `		}` |
|   171 | 1733 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1734 | `			/* Comment til the end of line */` |
|     5 | 1735 | `			zIn++;` |
|    73 | 1736 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    69 | 1737 | `				zIn++;` |
|     1 | 1738 | `			}` |
|     5 | 1739 | `			continue;` |
|     - | 1740 | `		}` |
|     - | 1741 | `		/* Reset the string cursor of the working variable */` |
|   167 | 1742 | `		ph7_value_reset_string_cursor(pWorker);` |
|   167 | 1743 | `		if( zIn[0] == '[' ){` |
|     - | 1744 | `			/* Section: Extract the section name */` |
|    11 | 1745 | `			zIn++;` |
|    11 | 1746 | `			zCur = zIn;` |
|    77 | 1747 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|    67 | 1748 | `				zIn++;` |
|     1 | 1749 | `			}` |
|    11 | 1750 | `			if( zIn > zCur && bProcessSection ){` |
|     - | 1751 | `				/* Save the section name */` |
|     7 | 1752 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     7 | 1753 | `				SyStringFullTrim(&sEntry);` |
|     7 | 1754 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     7 | 1755 | `				if( sEntry.nByte > 0 ){` |
|     - | 1756 | `					/* Associate an array with the section */` |
|     7 | 1757 | `					pSection = ph7_context_new_array(pCtx);` |
|     7 | 1758 | `					if( pSection ){` |
|     7 | 1759 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|     7 | 1760 | `						pCur = pSection;` |
|     3 | 1761 | `					}` |
|     3 | 1762 | `				}` |
|     3 | 1763 | `			}` |
|    11 | 1764 | `			zIn++; /* Trailing square brackets ']' */` |
|     6 | 1765 | `		}else{` |
|     - | 1766 | `			ph7_value *pOldCur;` |
|     - | 1767 | `			int is_array;` |
|     - | 1768 | `			int iLen;` |
|     - | 1769 | `			/* Properties */` |
|   157 | 1770 | `			is_array = 0;` |
|   157 | 1771 | `			zCur = zIn;` |
|   157 | 1772 | `			iLen = 0; /* cc warning */` |
|   157 | 1773 | `			pOldCur = pCur;` |
|   681 | 1774 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|   525 | 1775 | `				if( zIn[0] == '[' && !is_array ){` |
|     - | 1776 | `					/* Array */` |
|   ! 0 | 1777 | `					iLen = (int)(zIn-zCur);` |
|   ! 0 | 1778 | `					is_array = 1;` |
|   ! 0 | 1779 | `					if( iLen > 0 ){` |
|   ! 0 | 1780 | `						ph7_value *pvArr = 0; /* cc warning */` |
|     - | 1781 | `						/* Query the hashtable */` |
|   ! 0 | 1782 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   ! 0 | 1783 | `						SyStringFullTrim(&sEntry);` |
|   ! 0 | 1784 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|   ! 0 | 1785 | `						if( pEntry ){` |
|   ! 0 | 1786 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|   ! 0 | 1787 | `						}else{` |
|     - | 1788 | `							/* Create an empty array */` |
|   ! 0 | 1789 | `							pvArr = ph7_context_new_array(pCtx);` |
|   ! 0 | 1790 | `							if( pvArr ){` |
|     - | 1791 | `								/* Save the entry */` |
|   ! 0 | 1792 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|     - | 1793 | `								/* Insert the entry */` |
|   ! 0 | 1794 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1795 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|   ! 0 | 1796 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|   ! 0 | 1797 | `								ph7_value_reset_string_cursor(pWorker);` |
|   ! 0 | 1798 | `							}` |
|     - | 1799 | `						}` |
|   ! 0 | 1800 | `						if( pvArr ){` |
|   ! 0 | 1801 | `							pCur = pvArr;` |
|   ! 0 | 1802 | `						}` |
|   ! 0 | 1803 | `					}` |
|   ! 0 | 1804 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|   ! 0 | 1805 | `						zIn++;` |
|   ! 0 | 1806 | `					}` |
|   ! 0 | 1807 | `				}` |
|   525 | 1808 | `				zIn++;` |
|     1 | 1809 | `			}` |
|   157 | 1810 | `			if( !is_array ){` |
|   157 | 1811 | `				iLen = (int)(zIn-zCur);` |
|    78 | 1812 | `			}` |
|     - | 1813 | `			/* Trim the key */` |
|   157 | 1814 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|   307 | 1815 | `			SyStringFullTrim(&sEntry);` |
|   157 | 1816 | `			if( sEntry.nByte > 0 ){` |
|   157 | 1817 | `				if( !is_array ){` |
|     - | 1818 | `					/* Save the key name */` |
|   157 | 1819 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    78 | 1820 | `				}` |
|     - | 1821 | `				/* extract key value. pValue must come back to an EMPTY STRING` |
|     - | 1822 | `				 * whatever the last entry typed it as (INI_SCANNER_TYPED sets` |
|     - | 1823 | `				 * bool/int/float/null): ph7_value_string() re-types it, the` |
|     - | 1824 | `				 * cursor reset then empties it. */` |
|   157 | 1825 | `				ph7_value_string(pValue,"",0);` |
|   157 | 1826 | `				ph7_value_reset_string_cursor(pValue);` |
|   157 | 1827 | `				zIn++; /* '=' */` |
|     - | 1828 | `				/* Skip the spaces BEFORE the value but never the newline that` |
|     - | 1829 | ``				 * ENDS it: `key =` at end of line is php's empty-string entry,`` |
|     - | 1830 | `				 * and the old skip ran onto the next line and swallowed it` |
|     - | 1831 | ``				 * whole — `e1 =` followed by `c1 = 10K` answered`` |
|     - | 1832 | `				 * ["e1" => "c1 = 10K"] with c1 GONE. */` |
|   297 | 1833 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && zIn[0] != '\n' && SyisSpace(zIn[0]) ){` |
|   141 | 1834 | `					zIn++;` |
|     1 | 1835 | `				}` |
|   157 | 1836 | `				if( zIn < zEnd && zIn[0] != '\n' ){` |
|     - | 1837 | `					int bQuoted;` |
|   145 | 1838 | `					zCur = zIn;` |
|   145 | 1839 | `					c = zIn[0];` |
|   145 | 1840 | `					bQuoted = (c == '"' \|\| c == '\'');` |
|   145 | 1841 | `					if( bQuoted ){` |
|    13 | 1842 | `						zIn++;` |
|     - | 1843 | `						/* Delimit the value */` |
|   101 | 1844 | `						while( zIn < zEnd ){` |
|   101 | 1845 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    13 | 1846 | `								break;` |
|     - | 1847 | `							}` |
|    89 | 1848 | `							zIn++;` |
|     1 | 1849 | `						}` |
|    13 | 1850 | `						if( zIn < zEnd ){` |
|    13 | 1851 | `							zIn++;` |
|     6 | 1852 | `						}` |
|     7 | 1853 | `					}else{` |
|  1003 | 1854 | `						while( zIn < zEnd ){` |
|   997 | 1855 | `							if( zIn[0] == '\n' ){` |
|   123 | 1856 | `								if( zIn[-1] != '\\' ){` |
|   123 | 1857 | `									break;` |
|   ! 0 | 1858 | `								}` |
|   875 | 1859 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|     - | 1860 | `								/* Inline comments */` |
|     3 | 1861 | `								break;` |
|     - | 1862 | `							}` |
|   871 | 1863 | `							zIn++;` |
|     1 | 1864 | `						}` |
|     - | 1865 | `					}` |
|     - | 1866 | `					/* Trim the value */` |
|   145 | 1867 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|   149 | 1868 | `					SyStringFullTrim(&sEntry);` |
|   145 | 1869 | `					if( bQuoted ){` |
|    25 | 1870 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    25 | 1871 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|     6 | 1872 | `					}` |
|   145 | 1873 | `					if( bQuoted \|\| iScannerMode == PH7_INI_SCANNER_RAW ){` |
|     - | 1874 | `						/* A quoted value is its literal bytes in EVERY mode` |
|     - | 1875 | `						 * (php runs no expansion inside quotes), and RAW keeps` |
|     - | 1876 | `						 * even a bare word uninterpreted. */` |
|    45 | 1877 | `						if( sEntry.nByte > 0 ){` |
|    45 | 1878 | `							ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|    22 | 1879 | `						}` |
|    23 | 1880 | `					}else{` |
|   101 | 1881 | `						VmIniInterpretValue(pCtx,&sEntry,iScannerMode,pValue);` |
|     - | 1882 | `					}` |
|    72 | 1883 | `				}` |
|     - | 1884 | `				/* Insert the key and it's value (an empty value included) */` |
|   157 | 1885 | `				ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|    79 | 1886 | `			}else{` |
|   ! 0 | 1887 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|   ! 0 | 1888 | `					zIn++;` |
|   ! 0 | 1889 | `				}` |
|     - | 1890 | `			}` |
|   157 | 1891 | `			pCur = pOldCur;` |
|     - | 1892 | `		}` |
|     1 | 1893 | `	}` |
|    27 | 1894 | `	SyHashRelease(&sHash);` |
|     - | 1895 | `	/* Return the parse of the INI string */` |
|    27 | 1896 | `	ph7_result_value(pCtx,pArray);` |
|    27 | 1897 | `	return SXRET_OK;` |
|    14 | 1898 | `}` |
|     - | 1899 | `/*` |
|     - | 1900 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|     - | 1901 | ` *  Parse a configuration string.` |
|     - | 1902 | ` * Parameters` |
|     - | 1903 | ` *  $ini` |
|     - | 1904 | ` *   The contents of the ini file being parsed.` |
|     - | 1905 | ` *  $process_sections` |
|     - | 1906 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|     - | 1907 | ` *   and settings included. The default for process_sections is FALSE.` |
|     - | 1908 | ` *  $scanner_mode` |
|     - | 1909 | ` *   INI_SCANNER_NORMAL (default: values interpreted — booleans, constants,` |
|     - | 1910 | ` *   ${var}), INI_SCANNER_RAW (values kept verbatim) or INI_SCANNER_TYPED` |
|     - | 1911 | ` *   (booleans, null and numbers come back as their own types). Any other` |
|     - | 1912 | ` *   value is php's "Invalid scanner mode" warning and FALSE.` |
|     - | 1913 | ` * Return` |
|     - | 1914 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|     - | 1915 | ` */` |
|    22 | 1916 | `PH7_PRIVATE int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1917 | `{` |
|     - | 1918 | `	const char *zIni;` |
|     - | 1919 | `	int nByte;` |
|    23 | 1920 | `	int iMode = PH7_INI_SCANNER_NORMAL;` |
|    23 | 1921 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|     - | 1922 | `		/* Missing/Invalid arguments,return FALSE*/` |
|   ! 0 | 1923 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1924 | `		return PH7_OK;` |
|     - | 1925 | `	}` |
|    23 | 1926 | `	if( nArg > 2 && ph7_value_is_int(apArg[2]) ){` |
|    11 | 1927 | `		iMode = ph7_value_to_int(apArg[2]);` |
|    10 | 1928 | `		if( iMode != PH7_INI_SCANNER_NORMAL && iMode != PH7_INI_SCANNER_RAW` |
|     8 | 1929 | `		 && iMode != PH7_INI_SCANNER_TYPED ){` |
|     - | 1930 | ``			/* php's bare message: no `func(): ` qualifier on this one */`` |
|     3 | 1931 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"Invalid scanner mode");` |
|     3 | 1932 | `			ph7_result_bool(pCtx,0);` |
|     3 | 1933 | `			return PH7_OK;` |
|     - | 1934 | `		}` |
|     4 | 1935 | `	}` |
|     - | 1936 | `	/* Extract the raw INI buffer */` |
|    21 | 1937 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|     - | 1938 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|    21 | 1939 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0,iMode);` |
|    12 | 1940 | `}` |
|     - | 1941 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|     - | 1942 | `#ifdef PH7_NEED_BUILTIN_REG` |
|     - | 1943 |  |
|     - | 1944 | `/*` |
|     - | 1945 | ` * Ctype Functions.` |
|     - | 1946 | ` * Status:` |
|     - | 1947 | ` *    Stable.` |
|     - | 1948 | ` */` |
|     - | 1949 | `/*` |
|     - | 1950 | ` * bool ctype_alnum(string $text)` |
|     - | 1951 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|     - | 1952 | ` * Parameters` |
|     - | 1953 | ` *  $text` |
|     - | 1954 | ` *   The tested string.` |
|     - | 1955 | ` * Return` |
|     - | 1956 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|     - | 1957 | ` */` |
|    72 | 1958 | `PH7_PRIVATE int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1959 | `{` |
|     - | 1960 | `	const unsigned char *zIn,*zEnd;` |
|     - | 1961 | `	int nLen;` |
|    73 | 1962 | `	if( nArg < 1 ){` |
|     - | 1963 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 1964 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1965 | `		return PH7_OK;` |
|     - | 1966 | `	}` |
|     - | 1967 | `	/* Extract the target string */` |
|    73 | 1968 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    73 | 1969 | `	zEnd = &zIn[nLen];` |
|    73 | 1970 | `	if( nLen < 1 ){` |
|     - | 1971 | `		/* Empty string,return FALSE */` |
|     3 | 1972 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1973 | `		return PH7_OK;` |
|     - | 1974 | `	}` |
|     - | 1975 | `	/* Perform the requested operation */` |
|   110 | 1976 | `	for(;;){` |
|   221 | 1977 | `		if( zIn >= zEnd ){` |
|     - | 1978 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    65 | 1979 | `			ph7_result_bool(pCtx,1);` |
|    65 | 1980 | `			return PH7_OK;` |
|     - | 1981 | `		}` |
|   157 | 1982 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|     7 | 1983 | `			break;` |
|     - | 1984 | `		}` |
|     - | 1985 | `		/* Point to the next character */` |
|   151 | 1986 | `		zIn++;` |
|     1 | 1987 | `	}` |
|     - | 1988 | `	/* The test failed,return FALSE */` |
|     7 | 1989 | `	ph7_result_bool(pCtx,0);` |
|     7 | 1990 | `	return PH7_OK;` |
|    37 | 1991 | `}` |
|     - | 1992 | `/*` |
|     - | 1993 | ` * bool ctype_alpha(string $text)` |
|     - | 1994 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|     - | 1995 | ` * Parameters` |
|     - | 1996 | ` *  $text` |
|     - | 1997 | ` *   The tested string.` |
|     - | 1998 | ` * Return` |
|     - | 1999 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|     - | 2000 | ` */` |
|    16 | 2001 | `PH7_PRIVATE int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2002 | `{` |
|     - | 2003 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2004 | `	int nLen;` |
|    17 | 2005 | `	if( nArg < 1 ){` |
|     - | 2006 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2007 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2008 | `		return PH7_OK;` |
|     - | 2009 | `	}` |
|     - | 2010 | `	/* Extract the target string */` |
|    17 | 2011 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2012 | `	zEnd = &zIn[nLen];` |
|    17 | 2013 | `	if( nLen < 1 ){` |
|     - | 2014 | `		/* Empty string,return FALSE */` |
|     3 | 2015 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2016 | `		return PH7_OK;` |
|     - | 2017 | `	}` |
|     - | 2018 | `	/* Perform the requested operation */` |
|    42 | 2019 | `	for(;;){` |
|    85 | 2020 | `		if( zIn >= zEnd ){` |
|     - | 2021 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2022 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2023 | `			return PH7_OK;` |
|     - | 2024 | `		}` |
|    77 | 2025 | `		if( !SyisAlpha(zIn[0]) ){` |
|     7 | 2026 | `			break;` |
|     - | 2027 | `		}` |
|     - | 2028 | `		/* Point to the next character */` |
|    71 | 2029 | `		zIn++;` |
|     1 | 2030 | `	}` |
|     - | 2031 | `	/* The test failed,return FALSE */` |
|     7 | 2032 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2033 | `	return PH7_OK;` |
|     9 | 2034 | `}` |
|     - | 2035 | `/*` |
|     - | 2036 | ` * bool ctype_cntrl(string $text)` |
|     - | 2037 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|     - | 2038 | ` * Parameters` |
|     - | 2039 | ` *  $text` |
|     - | 2040 | ` *   The tested string.` |
|     - | 2041 | ` * Return` |
|     - | 2042 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|     - | 2043 | ` */` |
|    16 | 2044 | `PH7_PRIVATE int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2045 | `{` |
|     - | 2046 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2047 | `	int nLen;` |
|    17 | 2048 | `	if( nArg < 1 ){` |
|     - | 2049 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2050 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2051 | `		return PH7_OK;` |
|     - | 2052 | `	}` |
|     - | 2053 | `	/* Extract the target string */` |
|    17 | 2054 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2055 | `	zEnd = &zIn[nLen];` |
|    17 | 2056 | `	if( nLen < 1 ){` |
|     - | 2057 | `		/* Empty string,return FALSE */` |
|     3 | 2058 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2059 | `		return PH7_OK;` |
|     - | 2060 | `	}` |
|     - | 2061 | `	/* Perform the requested operation */` |
|    14 | 2062 | `	for(;;){` |
|    29 | 2063 | `		if( zIn >= zEnd ){` |
|     - | 2064 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2065 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2066 | `			return PH7_OK;` |
|     - | 2067 | `		}` |
|    21 | 2068 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2069 | `			/* UTF-8 stream  */` |
|   ! 0 | 2070 | `			break;` |
|     - | 2071 | `		}` |
|    21 | 2072 | `		if( !SyisCtrl(zIn[0]) ){` |
|     7 | 2073 | `			break;` |
|     - | 2074 | `		}` |
|     - | 2075 | `		/* Point to the next character */` |
|    15 | 2076 | `		zIn++;` |
|     1 | 2077 | `	}` |
|     - | 2078 | `	/* The test failed,return FALSE */` |
|     7 | 2079 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2080 | `	return PH7_OK;` |
|     9 | 2081 | `}` |
|     - | 2082 | `/*` |
|     - | 2083 | ` * bool ctype_digit(string $text)` |
|     - | 2084 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|     - | 2085 | ` * Parameters` |
|     - | 2086 | ` *  $text` |
|     - | 2087 | ` *   The tested string.` |
|     - | 2088 | ` * Return` |
|     - | 2089 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|     - | 2090 | ` */` |
|  1635 | 2091 | `PH7_PRIVATE int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 2092 | `{` |
|     - | 2093 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2094 | `	int nLen;` |
|  1640 | 2095 | `	if( nArg < 1 ){` |
|     - | 2096 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2097 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2098 | `		return PH7_OK;` |
|     - | 2099 | `	}` |
|     - | 2100 | `	/* Extract the target string */` |
|  1640 | 2101 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  1640 | 2102 | `	zEnd = &zIn[nLen];` |
|  1640 | 2103 | `	if( nLen < 1 ){` |
|     - | 2104 | `		/* Empty string,return FALSE */` |
|     3 | 2105 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2106 | `		return PH7_OK;` |
|     - | 2107 | `	}` |
|     - | 2108 | `	/* Perform the requested operation */` |
|  1479 | 2109 | `	for(;;){` |
|  2961 | 2110 | `		if( zIn >= zEnd ){` |
|     - | 2111 | `			/* If we reach the end of the string,then the test succeeded. */` |
|  1262 | 2112 | `			ph7_result_bool(pCtx,1);` |
|  1262 | 2113 | `			return PH7_OK;` |
|     - | 2114 | `		}` |
|  1704 | 2115 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2116 | `			/* UTF-8 stream  */` |
|   ! 0 | 2117 | `			break;` |
|     - | 2118 | `		}` |
|  1704 | 2119 | `		if( !SyisDigit(zIn[0]) ){` |
|   381 | 2120 | `			break;` |
|     - | 2121 | `		}` |
|     - | 2122 | `		/* Point to the next character */` |
|  1328 | 2123 | `		zIn++;` |
|     5 | 2124 | `	}` |
|     - | 2125 | `	/* The test failed,return FALSE */` |
|   381 | 2126 | `	ph7_result_bool(pCtx,0);` |
|   381 | 2127 | `	return PH7_OK;` |
|   823 | 2128 | `}` |
|     - | 2129 | `/*` |
|     - | 2130 | ` * bool ctype_xdigit(string $text)` |
|     - | 2131 | ` *  Check for character(s) representing a hexadecimal digit.` |
|     - | 2132 | ` * Parameters` |
|     - | 2133 | ` *  $text` |
|     - | 2134 | ` *   The tested string.` |
|     - | 2135 | ` * Return` |
|     - | 2136 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|     - | 2137 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|     - | 2138 | ` */` |
|    44 | 2139 | `PH7_PRIVATE int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2140 | `{` |
|     - | 2141 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2142 | `	int nLen;` |
|    46 | 2143 | `	if( nArg < 1 ){` |
|     - | 2144 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2145 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2146 | `		return PH7_OK;` |
|     - | 2147 | `	}` |
|     - | 2148 | `	/* Extract the target string */` |
|    46 | 2149 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    46 | 2150 | `	zEnd = &zIn[nLen];` |
|    46 | 2151 | `	if( nLen < 1 ){` |
|     - | 2152 | `		/* Empty string,return FALSE */` |
|     3 | 2153 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2154 | `		return PH7_OK;` |
|     - | 2155 | `	}` |
|     - | 2156 | `	/* Perform the requested operation */` |
|    85 | 2157 | `	for(;;){` |
|   172 | 2158 | `		if( zIn >= zEnd ){` |
|     - | 2159 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    38 | 2160 | `			ph7_result_bool(pCtx,1);` |
|    38 | 2161 | `			return PH7_OK;` |
|     - | 2162 | `		}` |
|   136 | 2163 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2164 | `			/* UTF-8 stream  */` |
|   ! 0 | 2165 | `			break;` |
|     - | 2166 | `		}` |
|   136 | 2167 | `		if( !SyisHex(zIn[0]) ){` |
|     7 | 2168 | `			break;` |
|     - | 2169 | `		}` |
|     - | 2170 | `		/* Point to the next character */` |
|   130 | 2171 | `		zIn++;` |
|     2 | 2172 | `	}` |
|     - | 2173 | `	/* The test failed,return FALSE */` |
|     7 | 2174 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2175 | `	return PH7_OK;` |
|    24 | 2176 | `}` |
|     - | 2177 | `/*` |
|     - | 2178 | ` * bool ctype_graph(string $text)` |
|     - | 2179 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|     - | 2180 | ` * Parameters` |
|     - | 2181 | ` *  $text` |
|     - | 2182 | ` *   The tested string.` |
|     - | 2183 | ` * Return` |
|     - | 2184 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|     - | 2185 | ` * (no white space), FALSE otherwise.` |
|     - | 2186 | ` */` |
|    16 | 2187 | `PH7_PRIVATE int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2188 | `{` |
|     - | 2189 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2190 | `	int nLen;` |
|    17 | 2191 | `	if( nArg < 1 ){` |
|     - | 2192 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2193 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2194 | `		return PH7_OK;` |
|     - | 2195 | `	}` |
|     - | 2196 | `	/* Extract the target string */` |
|    17 | 2197 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2198 | `	zEnd = &zIn[nLen];` |
|    17 | 2199 | `	if( nLen < 1 ){` |
|     - | 2200 | `		/* Empty string,return FALSE */` |
|     3 | 2201 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2202 | `		return PH7_OK;` |
|     - | 2203 | `	}` |
|     - | 2204 | `	/* Perform the requested operation */` |
|    57 | 2205 | `	for(;;){` |
|   115 | 2206 | `		if( zIn >= zEnd ){` |
|     - | 2207 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2208 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2209 | `			return PH7_OK;` |
|     - | 2210 | `		}` |
|   107 | 2211 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2212 | `			/* UTF-8 stream  */` |
|   ! 0 | 2213 | `			break;` |
|     - | 2214 | `		}` |
|   107 | 2215 | `		if( !SyisGraph(zIn[0]) ){` |
|     7 | 2216 | `			break;` |
|     - | 2217 | `		}` |
|     - | 2218 | `		/* Point to the next character */` |
|   101 | 2219 | `		zIn++;` |
|     1 | 2220 | `	}` |
|     - | 2221 | `	/* The test failed,return FALSE */` |
|     7 | 2222 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2223 | `	return PH7_OK;` |
|     9 | 2224 | `}` |
|     - | 2225 | `/*` |
|     - | 2226 | ` * bool ctype_print(string $text)` |
|     - | 2227 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|     - | 2228 | ` * Parameters` |
|     - | 2229 | ` *  $text` |
|     - | 2230 | ` *   The tested string.` |
|     - | 2231 | ` * Return` |
|     - | 2232 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|     - | 2233 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|     - | 2234 | ` *  or control function at all.` |
|     - | 2235 | ` */` |
|    16 | 2236 | `PH7_PRIVATE int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2237 | `{` |
|     - | 2238 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2239 | `	int nLen;` |
|    17 | 2240 | `	if( nArg < 1 ){` |
|     - | 2241 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2242 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2243 | `		return PH7_OK;` |
|     - | 2244 | `	}` |
|     - | 2245 | `	/* Extract the target string */` |
|    17 | 2246 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2247 | `	zEnd = &zIn[nLen];` |
|    17 | 2248 | `	if( nLen < 1 ){` |
|     - | 2249 | `		/* Empty string,return FALSE */` |
|     3 | 2250 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2251 | `		return PH7_OK;` |
|     - | 2252 | `	}` |
|     - | 2253 | `	/* Perform the requested operation */` |
|    63 | 2254 | `	for(;;){` |
|   127 | 2255 | `		if( zIn >= zEnd ){` |
|     - | 2256 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2257 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2258 | `			return PH7_OK;` |
|     - | 2259 | `		}` |
|   119 | 2260 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2261 | `			/* UTF-8 stream  */` |
|   ! 0 | 2262 | `			break;` |
|     - | 2263 | `		}` |
|   119 | 2264 | `		if( !SyisPrint(zIn[0]) ){` |
|     7 | 2265 | `			break;` |
|     - | 2266 | `		}` |
|     - | 2267 | `		/* Point to the next character */` |
|   113 | 2268 | `		zIn++;` |
|     1 | 2269 | `	}` |
|     - | 2270 | `	/* The test failed,return FALSE */` |
|     7 | 2271 | `	ph7_result_bool(pCtx,0);` |
|     7 | 2272 | `	return PH7_OK;` |
|     9 | 2273 | `}` |
|     - | 2274 | `/*` |
|     - | 2275 | ` * bool ctype_punct(string $text)` |
|     - | 2276 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|     - | 2277 | ` * Parameters` |
|     - | 2278 | ` *  $text` |
|     - | 2279 | ` *   The tested string.` |
|     - | 2280 | ` * Return` |
|     - | 2281 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|     - | 2282 | ` *  digit or blank, FALSE otherwise.` |
|     - | 2283 | ` */` |
|    18 | 2284 | `PH7_PRIVATE int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2285 | `{` |
|     - | 2286 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2287 | `	int nLen;` |
|    19 | 2288 | `	if( nArg < 1 ){` |
|     - | 2289 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2290 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2291 | `		return PH7_OK;` |
|     - | 2292 | `	}` |
|     - | 2293 | `	/* Extract the target string */` |
|    19 | 2294 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 2295 | `	zEnd = &zIn[nLen];` |
|    19 | 2296 | `	if( nLen < 1 ){` |
|     - | 2297 | `		/* Empty string,return FALSE */` |
|     3 | 2298 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2299 | `		return PH7_OK;` |
|     - | 2300 | `	}` |
|     - | 2301 | `	/* Perform the requested operation */` |
|    38 | 2302 | `	for(;;){` |
|    77 | 2303 | `		if( zIn >= zEnd ){` |
|     - | 2304 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     9 | 2305 | `			ph7_result_bool(pCtx,1);` |
|     9 | 2306 | `			return PH7_OK;` |
|     - | 2307 | `		}` |
|    69 | 2308 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2309 | `			/* UTF-8 stream  */` |
|   ! 0 | 2310 | `			break;` |
|     - | 2311 | `		}` |
|    69 | 2312 | `		if( !SyisPunct(zIn[0]) ){` |
|     9 | 2313 | `			break;` |
|     - | 2314 | `		}` |
|     - | 2315 | `		/* Point to the next character */` |
|    61 | 2316 | `		zIn++;` |
|     1 | 2317 | `	}` |
|     - | 2318 | `	/* The test failed,return FALSE */` |
|     9 | 2319 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2320 | `	return PH7_OK;` |
|    10 | 2321 | `}` |
|     - | 2322 | `/*` |
|     - | 2323 | ` * bool ctype_space(string $text)` |
|     - | 2324 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|     - | 2325 | ` * Parameters` |
|     - | 2326 | ` *  $text` |
|     - | 2327 | ` *   The tested string.` |
|     - | 2328 | ` * Return` |
|     - | 2329 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|     - | 2330 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|     - | 2331 | ` *  and form feed characters.` |
|     - | 2332 | ` */` |
| 40771 | 2333 | `PH7_PRIVATE int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     5 | 2334 | `{` |
|     - | 2335 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2336 | `	int nLen;` |
| 40776 | 2337 | `	if( nArg < 1 ){` |
|     - | 2338 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2339 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2340 | `		return PH7_OK;` |
|     - | 2341 | `	}` |
|     - | 2342 | `	/* Extract the target string */` |
| 40776 | 2343 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
| 40776 | 2344 | `	zEnd = &zIn[nLen];` |
| 40776 | 2345 | `	if( nLen < 1 ){` |
|     - | 2346 | `		/* Empty string,return FALSE */` |
|     3 | 2347 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2348 | `		return PH7_OK;` |
|     - | 2349 | `	}` |
|     - | 2350 | `	/* Perform the requested operation */` |
| 20566 | 2351 | `	for(;;){` |
| 40808 | 2352 | `		if( zIn >= zEnd ){` |
|     - | 2353 | `			/* If we reach the end of the string,then the test succeeded. */` |
|    11 | 2354 | `			ph7_result_bool(pCtx,1);` |
|    11 | 2355 | `			return PH7_OK;` |
|     - | 2356 | `		}` |
| 40798 | 2357 | `		if( zIn[0] >= 0xc0 ){` |
|     - | 2358 | `			/* UTF-8 stream  */` |
|   ! 0 | 2359 | `			break;` |
|     - | 2360 | `		}` |
| 40798 | 2361 | `		if( !SyisSpace(zIn[0]) ){` |
| 40764 | 2362 | `			break;` |
|     - | 2363 | `		}` |
|     - | 2364 | `		/* Point to the next character */` |
|    35 | 2365 | `		zIn++;` |
|     1 | 2366 | `	}` |
|     - | 2367 | `	/* The test failed,return FALSE */` |
| 40764 | 2368 | `	ph7_result_bool(pCtx,0);` |
| 40764 | 2369 | `	return PH7_OK;` |
| 20555 | 2370 | `}` |
|     - | 2371 | `/*` |
|     - | 2372 | ` * bool ctype_lower(string $text)` |
|     - | 2373 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|     - | 2374 | ` * Parameters` |
|     - | 2375 | ` *  $text` |
|     - | 2376 | ` *   The tested string.` |
|     - | 2377 | ` * Return` |
|     - | 2378 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|     - | 2379 | ` */` |
|    16 | 2380 | `PH7_PRIVATE int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2381 | `{` |
|     - | 2382 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2383 | `	int nLen;` |
|    17 | 2384 | `	if( nArg < 1 ){` |
|     - | 2385 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2386 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2387 | `		return PH7_OK;` |
|     - | 2388 | `	}` |
|     - | 2389 | `	/* Extract the target string */` |
|    17 | 2390 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2391 | `	zEnd = &zIn[nLen];` |
|    17 | 2392 | `	if( nLen < 1 ){` |
|     - | 2393 | `		/* Empty string,return FALSE */` |
|     3 | 2394 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2395 | `		return PH7_OK;` |
|     - | 2396 | `	}` |
|     - | 2397 | `	/* Perform the requested operation */` |
|    27 | 2398 | `	for(;;){` |
|    55 | 2399 | `		if( zIn >= zEnd ){` |
|     - | 2400 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2401 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2402 | `			return PH7_OK;` |
|     - | 2403 | `		}` |
|    51 | 2404 | `		if( !SyisLower(zIn[0]) ){` |
|    11 | 2405 | `			break;` |
|     - | 2406 | `		}` |
|     - | 2407 | `		/* Point to the next character */` |
|    41 | 2408 | `		zIn++;` |
|     1 | 2409 | `	}` |
|     - | 2410 | `	/* The test failed,return FALSE */` |
|    11 | 2411 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2412 | `	return PH7_OK;` |
|     9 | 2413 | `}` |
|     - | 2414 | `/*` |
|     - | 2415 | ` * bool ctype_upper(string $text)` |
|     - | 2416 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|     - | 2417 | ` * Parameters` |
|     - | 2418 | ` *  $text` |
|     - | 2419 | ` *   The tested string.` |
|     - | 2420 | ` * Return` |
|     - | 2421 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|     - | 2422 | ` */` |
|    16 | 2423 | `PH7_PRIVATE int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2424 | `{` |
|     - | 2425 | `	const unsigned char *zIn,*zEnd;` |
|     - | 2426 | `	int nLen;` |
|    17 | 2427 | `	if( nArg < 1 ){` |
|     - | 2428 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2429 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2430 | `		return PH7_OK;` |
|     - | 2431 | `	}` |
|     - | 2432 | `	/* Extract the target string */` |
|    17 | 2433 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2434 | `	zEnd = &zIn[nLen];` |
|    17 | 2435 | `	if( nLen < 1 ){` |
|     - | 2436 | `		/* Empty string,return FALSE */` |
|     3 | 2437 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2438 | `		return PH7_OK;` |
|     - | 2439 | `	}` |
|     - | 2440 | `	/* Perform the requested operation */` |
|    28 | 2441 | `	for(;;){` |
|    57 | 2442 | `		if( zIn >= zEnd ){` |
|     - | 2443 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     5 | 2444 | `			ph7_result_bool(pCtx,1);` |
|     5 | 2445 | `			return PH7_OK;` |
|     - | 2446 | `		}` |
|    53 | 2447 | `		if( !SyisUpper(zIn[0]) ){` |
|    11 | 2448 | `			break;` |
|     - | 2449 | `		}` |
|     - | 2450 | `		/* Point to the next character */` |
|    43 | 2451 | `		zIn++;` |
|     1 | 2452 | `	}` |
|     - | 2453 | `	/* The test failed,return FALSE */` |
|    11 | 2454 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2455 | `	return PH7_OK;` |
|     9 | 2456 | `}` |
|     - | 2457 | `/* Date/Time functions moved to builtin_date.c */` |
|     - | 2458 | `/*` |
|     - | 2459 | ` * Section:` |
|     - | 2460 | ` *    URL handling Functions.` |
|     - | 2461 | ` * Status:` |
|     - | 2462 | ` *    Stable.` |
|     - | 2463 | ` */` |
|     - | 2464 | `/*` |
|     - | 2465 | ` * Output consumer callback for the standard Symisc routines.` |
|     - | 2466 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|     - | 2467 | ` */` |
|  1824 | 2468 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     3 | 2469 | `{` |
|     - | 2470 | `	/* Store in the call context result buffer */` |
|  1827 | 2471 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|  1827 | 2472 | `	return SXRET_OK;` |
|     3 | 2473 | `}` |
|     - | 2474 | `/*` |
|     - | 2475 | ` * string base64_encode(string $data)` |
|     - | 2476 | ` *  Encodes data with MIME base64` |
|     - | 2477 | ` * Parameter` |
|     - | 2478 | ` *  $data` |
|     - | 2479 | ` *    Data to encode` |
|     - | 2480 | ` * Return` |
|     - | 2481 | ` *  Encoded data or FALSE on failure.` |
|     - | 2482 | ` */` |
|    10 | 2483 | `PH7_PRIVATE int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2484 | `{` |
|     - | 2485 | `	const char *zIn;` |
|     - | 2486 | `	int nLen;` |
|    12 | 2487 | `	if( nArg < 1 ){` |
|     - | 2488 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2489 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2490 | `		return PH7_OK;` |
|     - | 2491 | `	}` |
|     - | 2492 | `	/* Extract the input string */` |
|    12 | 2493 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    12 | 2494 | `	if( nLen < 1 ){` |
|     - | 2495 | `		/* php encodes the empty string to the EMPTY STRING; base64_encode() cannot` |
|     - | 2496 | `		 * fail at all, so FALSE was never one of its answers. */` |
|     3 | 2497 | `		ph7_result_string(pCtx,"",0);` |
|     3 | 2498 | `		return PH7_OK;` |
|     - | 2499 | `	}` |
|     - | 2500 | `	/* Perform the BASE64 encoding */` |
|    10 | 2501 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    10 | 2502 | `	return PH7_OK;` |
|     7 | 2503 | `}` |
|     - | 2504 | `/*` |
|     - | 2505 | ` * php's base64 reverse table: -1 is skippable whitespace (\t \n \r and space,` |
|     - | 2506 | ` * exactly php's set -- \v/\f are NOT skipped), -2 is an invalid byte, 0..63 the` |
|     - | 2507 | ` * decoded 6-bit value. The pad byte '=' is handled before the lookup, so its` |
|     - | 2508 | ` * table slot is never consulted.` |
|     - | 2509 | ` */` |
|     - | 2510 | `static const signed char aB64Rev[256] = {` |
|     - | 2511 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-1,-1,-2,-2,-1,-2,-2,` |
|     - | 2512 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2513 | `	-1,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,62,-2,-2,-2,63,` |
|     - | 2514 | `	52,53,54,55,56,57,58,59,60,61,-2,-2,-2,-2,-2,-2,` |
|     - | 2515 | `	-2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 2516 | `	15,16,17,18,19,20,21,22,23,24,25,-2,-2,-2,-2,-2,` |
|     - | 2517 | `	-2,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 2518 | `	41,42,43,44,45,46,47,48,49,50,51,-2,-2,-2,-2,-2,` |
|     - | 2519 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2520 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2521 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2522 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2523 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2524 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2525 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,` |
|     - | 2526 | `	-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2,-2` |
|     - | 2527 | `};` |
|     - | 2528 | `/*` |
|     - | 2529 | ` * string base64_decode(string $data, bool $strict = false)` |
|     - | 2530 | ` *  Decodes data encoded with MIME base64` |
|     - | 2531 | ` * Parameters` |
|     - | 2532 | ` *  $data` |
|     - | 2533 | ` *    Encoded data.` |
|     - | 2534 | ` *  $strict` |
|     - | 2535 | ` *    When true, return FALSE if the input contains a character outside the` |
|     - | 2536 | ` *    base64 alphabet (whitespace is still skipped) or the padding/length is` |
|     - | 2537 | ` *    malformed. When false, such bytes are silently skipped (best effort).` |
|     - | 2538 | ` * Return` |
|     - | 2539 | ` *  Returns the original data or FALSE on failure.` |
|     - | 2540 | ` * Implementation note: a faithful port of php's php_base64_decode_ex(). The old` |
|     - | 2541 | ` * code ignored $strict entirely and ran the shared SyBase64Decode(), which maps` |
|     - | 2542 | ` * every non-alphabet byte (whitespace included) to 0 rather than skipping it --` |
|     - | 2543 | ` * a silent wrong answer on padded/whitespace input in BOTH modes.` |
|     - | 2544 | ` */` |
|    40 | 2545 | `PH7_PRIVATE int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2546 | `{` |
|     - | 2547 | `	const unsigned char *zIn;` |
|     - | 2548 | `	unsigned char *zOut;` |
|    42 | 2549 | `	int nLen,strict = 0;` |
|    42 | 2550 | `	int i = 0,j = 0,padding = 0,k;` |
|    42 | 2551 | `	if( nArg < 1 ){` |
|     - | 2552 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2553 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2554 | `		return PH7_OK;` |
|     - | 2555 | `	}` |
|     - | 2556 | `	/* Extract the input string */` |
|    42 | 2557 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    42 | 2558 | `	if( nLen < 1 ){` |
|     - | 2559 | `		/* php decodes the empty string to the EMPTY STRING, not FALSE (FALSE is reserved` |
|     - | 2560 | `		 * for input that cannot be decoded at all). */` |
|     6 | 2561 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2562 | `		return PH7_OK;` |
|     - | 2563 | `	}` |
|    37 | 2564 | `	if( nArg > 1 ){` |
|    31 | 2565 | `		strict = ph7_value_to_bool(apArg[1]);` |
|    15 | 2566 | `	}` |
|     - | 2567 | `	/* Output is at most 3/4 of the input; nLen bytes is a safe upper bound. */` |
|    37 | 2568 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    37 | 2569 | `	if( zOut == 0 ){` |
|   ! 0 | 2570 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2571 | `	}` |
|   187 | 2572 | `	for( k = 0 ; k < nLen ; ++k ){` |
|   155 | 2573 | `		int ch = zIn[k];` |
|     - | 2574 | `		int val;` |
|   155 | 2575 | `		if( ch == '=' ){` |
|     - | 2576 | `			/* Pad byte: count it, decode nothing. */` |
|    23 | 2577 | `			padding++;` |
|    23 | 2578 | `			continue;` |
|     - | 2579 | `		}` |
|   133 | 2580 | `		val = aB64Rev[ch];` |
|   133 | 2581 | `		if( !strict ){` |
|     - | 2582 | `			/* Lenient: skip whitespace AND any invalid byte. */` |
|    61 | 2583 | `			if( val < 0 ){` |
|    15 | 2584 | `				continue;` |
|     - | 2585 | `			}` |
|    24 | 2586 | `		}else{` |
|    73 | 2587 | `			if( val == -1 ){` |
|     - | 2588 | `				/* Skippable whitespace. */` |
|     7 | 2589 | `				continue;` |
|     - | 2590 | `			}` |
|    67 | 2591 | `			if( val == -2 ){` |
|     - | 2592 | `				/* A byte outside the base64 alphabet. */` |
|     5 | 2593 | `				goto fail;` |
|     - | 2594 | `			}` |
|    63 | 2595 | `			if( padding ){` |
|     - | 2596 | `				/* Data must not follow the padding. */` |
|   ! 0 | 2597 | `				goto fail;` |
|     - | 2598 | `			}` |
|     - | 2599 | `		}` |
|   109 | 2600 | `		switch( i & 3 ){` |
|    20 | 2601 | `			case 0:` |
|    41 | 2602 | `				zOut[j] = (unsigned char)(val << 2);` |
|    41 | 2603 | `				break;` |
|    18 | 2604 | `			case 1:` |
|    37 | 2605 | `				zOut[j++] \|= (unsigned char)(val >> 4);` |
|    37 | 2606 | `				zOut[j] = (unsigned char)((val & 0x0F) << 4);` |
|    37 | 2607 | `				break;` |
|    11 | 2608 | `			case 2:` |
|    23 | 2609 | `				zOut[j++] \|= (unsigned char)(val >> 2);` |
|    23 | 2610 | `				zOut[j] = (unsigned char)((val & 0x03) << 6);` |
|    23 | 2611 | `				break;` |
|     5 | 2612 | `			case 3:` |
|    11 | 2613 | `				zOut[j++] \|= (unsigned char)val;` |
|    10 | 2614 | `				break;` |
|     - | 2615 | `		}` |
|   109 | 2616 | `		i++;` |
|    55 | 2617 | `	}` |
|    33 | 2618 | `	if( strict ){` |
|     - | 2619 | `		/* A lone trailing 6-bit group (one leftover char) cannot form a byte. */` |
|    21 | 2620 | `		if( (i & 3) == 1 ){` |
|     3 | 2621 | `			goto fail;` |
|     - | 2622 | `		}` |
|     - | 2623 | `		/* Padding must be 1 or 2 bytes and complete the 4-char group. */` |
|    19 | 2624 | `		if( padding && (padding > 2 \|\| ((i + padding) & 3) != 0) ){` |
|     3 | 2625 | `			goto fail;` |
|     - | 2626 | `		}` |
|     8 | 2627 | `	}` |
|    29 | 2628 | `	ph7_result_string(pCtx,(const char *)zOut,j);` |
|    29 | 2629 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    29 | 2630 | `	return PH7_OK;` |
|     4 | 2631 | `fail:` |
|     9 | 2632 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     9 | 2633 | `	ph7_result_bool(pCtx,0);` |
|     9 | 2634 | `	return PH7_OK;` |
|    22 | 2635 | `}` |
|     - | 2636 | `/*` |
|     - | 2637 | ` * uuencode's six-bit alphabet: a value of 0 is written as the backtick php uses` |
|     - | 2638 | ` * instead of the historical space, every other value as ' ' + value. The three` |
|     - | 2639 | ` * PH7_UU_ENC_C* helpers pack the 6-bit groups exactly like php's macros: each` |
|     - | 2640 | ` * contribution is masked to its own bit window, so the result never depends on` |
|     - | 2641 | ` * whether the platform's char is signed.` |
|     - | 2642 | ` */` |
|     - | 2643 | ``#define PH7_UU_ENC(c)      ((char)((c) ? (((c) & 077) + ' ') : '`'))`` |
|     - | 2644 | `#define PH7_UU_ENC_C1(a)   PH7_UU_ENC((a) >> 2)` |
|     - | 2645 | `#define PH7_UU_ENC_C2(a,b) PH7_UU_ENC((((a) << 4) & 060) \| (((b) >> 4) & 017))` |
|     - | 2646 | `#define PH7_UU_ENC_C3(b,c) PH7_UU_ENC((((b) << 2) & 074) \| (((c) >> 6) & 003))` |
|     - | 2647 | `#define PH7_UU_ENC_C4(c)   PH7_UU_ENC((c) & 077)` |
|     - | 2648 | `#define PH7_UU_DEC(c)      ((((int)(c)) - ' ') & 077)` |
|     - | 2649 | `/*` |
|     - | 2650 | ` * string convert_uuencode(string $data)` |
|     - | 2651 | ` *  Uuencode a string.` |
|     - | 2652 | ` * Parameter` |
|     - | 2653 | ` *  $data` |
|     - | 2654 | ` *   Data to encode.` |
|     - | 2655 | ` * Return` |
|     - | 2656 | ` *  The uuencoded data: 45-byte lines, each prefixed with its encoded length and` |
|     - | 2657 | `` *  terminated by a newline, followed by php's "`\n" end marker. An empty input`` |
|     - | 2658 | ` *  answers just that marker.` |
|     - | 2659 | ` * Implementation note: a faithful port of php's php_uuencode(). This used to be` |
|     - | 2660 | ` * registered as an ALIAS of base64_encode() -- a wrong ALGORITHM, so every answer` |
|     - | 2661 | ` * was silently a base64 string (convert_uuencode("abc") gave "YWJj" where php` |
|     - | 2662 | `` * gives "#86)C\n`\n").`` |
|     - | 2663 | ` */` |
|    36 | 2664 | `PH7_PRIVATE int PH7_builtin_convert_uuencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2665 | `{` |
|     - | 2666 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 2667 | `	char zLine[64]; /* one full line is 1 length byte + 60 data bytes + '\n' */` |
|    38 | 2668 | `	int nLen,iLen = 45,n;` |
|    38 | 2669 | `	if( nArg < 1 ){` |
|     - | 2670 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2671 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2672 | `		return PH7_OK;` |
|     - | 2673 | `	}` |
|     - | 2674 | `	/* Extract the input string */` |
|    38 | 2675 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    38 | 2676 | `	if( nLen < 0 ){` |
|   ! 0 | 2677 | `		nLen = 0;` |
|   ! 0 | 2678 | `	}` |
|    38 | 2679 | `	zEnd = &zIn[nLen];` |
|     - | 2680 | `	/* Emit whole groups while at least four bytes remain: the last line is closed by` |
|     - | 2681 | ``	 * the tail block below so a group of one or two bytes gets php's '`' filler. */`` |
|    70 | 2682 | `	while( &zIn[3] < zEnd ){` |
|    34 | 2683 | `		zStop = &zIn[iLen];` |
|    34 | 2684 | `		if( zStop > zEnd ){` |
|     - | 2685 | `			/* A short final line: its length byte counts every remaining byte, but only` |
|     - | 2686 | `			 * whole three-byte groups are encoded here -- the leftovers ride the tail` |
|     - | 2687 | `			 * block, which then adds no length byte of its own. */` |
|    10 | 2688 | `			iLen = (int)(zEnd - zIn);` |
|    10 | 2689 | `			zStop = &zIn[(iLen/3)*3];` |
|     4 | 2690 | `		}` |
|    34 | 2691 | `		n = 0;` |
|    34 | 2692 | `		zLine[n++] = PH7_UU_ENC(iLen);` |
|   476 | 2693 | `		while( zIn < zStop ){` |
|   444 | 2694 | `			zLine[n++] = PH7_UU_ENC_C1(zIn[0]);` |
|   444 | 2695 | `			zLine[n++] = PH7_UU_ENC_C2(zIn[0],zIn[1]);` |
|   444 | 2696 | `			zLine[n++] = PH7_UU_ENC_C3(zIn[1],zIn[2]);` |
|   444 | 2697 | `			zLine[n++] = PH7_UU_ENC_C4(zIn[2]);` |
|   444 | 2698 | `			zIn += 3;` |
|     2 | 2699 | `		}` |
|    34 | 2700 | `		if( iLen == 45 ){` |
|    25 | 2701 | `			zLine[n++] = '\n';` |
|    12 | 2702 | `		}` |
|    34 | 2703 | `		ph7_result_string(pCtx,zLine,n);` |
|     2 | 2704 | `	}` |
|    38 | 2705 | `	if( zIn < zEnd ){` |
|     - | 2706 | `		/* One to three trailing bytes. php reads the bytes past the end of the string` |
|     - | 2707 | `		 * (its buffers are NUL terminated); the missing ones are zero here. */` |
|    30 | 2708 | `		unsigned char c0 = zIn[0];` |
|    30 | 2709 | `		unsigned char c1 = (&zIn[1] < zEnd) ? zIn[1] : 0;` |
|    30 | 2710 | `		unsigned char c2 = (&zIn[2] < zEnd) ? zIn[2] : 0;` |
|    30 | 2711 | `		n = 0;` |
|    30 | 2712 | `		if( iLen == 45 ){` |
|     - | 2713 | `			/* No short line was opened above: this group is a line of its own. */` |
|    22 | 2714 | `			zLine[n++] = PH7_UU_ENC((int)(zEnd - zIn));` |
|    22 | 2715 | `			iLen = 0;` |
|    10 | 2716 | `		}` |
|    30 | 2717 | `		zLine[n++] = PH7_UU_ENC_C1(c0);` |
|    30 | 2718 | `		zLine[n++] = PH7_UU_ENC_C2(c0,c1);` |
|    30 | 2719 | ``		zLine[n++] = ((zEnd - zIn) > 1) ? PH7_UU_ENC_C3(c1,c2) : '`';`` |
|    30 | 2720 | ``		zLine[n++] = ((zEnd - zIn) > 2) ? PH7_UU_ENC_C4(c2)     : '`';`` |
|    30 | 2721 | `		ph7_result_string(pCtx,zLine,n);` |
|    14 | 2722 | `	}` |
|    38 | 2723 | `	if( iLen != 45 ){` |
|     - | 2724 | `		/* A short (or tail) line is still open; a run of whole 45-byte lines -- and the` |
|     - | 2725 | `		 * empty input, which opens no line at all -- is already newline-terminated. */` |
|    30 | 2726 | `		ph7_result_string(pCtx,"\n",1);` |
|    14 | 2727 | `	}` |
|     - | 2728 | `	/* php's end marker: a zero-length line. */` |
|    38 | 2729 | ``	ph7_result_string(pCtx,"`\n",2);`` |
|    38 | 2730 | `	return PH7_OK;` |
|    20 | 2731 | `}` |
|     - | 2732 | `/*` |
|     - | 2733 | ` * string\|false convert_uudecode(string $data)` |
|     - | 2734 | ` *  Decode a uuencoded string.` |
|     - | 2735 | ` * Parameter` |
|     - | 2736 | ` *  $data` |
|     - | 2737 | ` *   Uuencoded data.` |
|     - | 2738 | ` * Return` |
|     - | 2739 | ` *  The decoded data, or FALSE (with a warning) when $data is not a valid uuencoded` |
|     - | 2740 | ` *  string: an empty input, a line claiming more bytes than the whole input holds, or` |
|     - | 2741 | ` *  a line whose data is truncated. Trailing garbage after the first short line is` |
|     - | 2742 | ` *  ignored, exactly like php.` |
|     - | 2743 | ` * Implementation note: a faithful port of php's php_uudecode(); see the encoder above` |
|     - | 2744 | ` * for why this was not a decoder at all before.` |
|     - | 2745 | ` */` |
|    48 | 2746 | `PH7_PRIVATE int PH7_builtin_convert_uudecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2747 | `{` |
|     - | 2748 | `	const unsigned char *zIn,*zEnd,*zStop;` |
|     - | 2749 | `	unsigned char *zOut;` |
|     - | 2750 | `	int nLen,iLen;` |
|    50 | 2751 | `	sxu32 nOut = 0,nTotal = 0;` |
|    50 | 2752 | `	if( nArg < 1 ){` |
|     - | 2753 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2754 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2755 | `		return PH7_OK;` |
|     - | 2756 | `	}` |
|     - | 2757 | `	/* Extract the input string */` |
|    50 | 2758 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|    50 | 2759 | `	if( nLen < 1 ){` |
|     - | 2760 | `		/* php refuses the empty string rather than decoding it to "". */` |
|     3 | 2761 | `		goto fail;` |
|     - | 2762 | `	}` |
|    48 | 2763 | `	zEnd = &zIn[nLen];` |
|     - | 2764 | `	/* Every four input characters yield three bytes and each line spends one more` |
|     - | 2765 | `	 * character on its length, so the input length is a safe upper bound. */` |
|    48 | 2766 | `	zOut = (unsigned char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen + 1);` |
|    48 | 2767 | `	if( zOut == 0 ){` |
|   ! 0 | 2768 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2769 | `	}` |
|    72 | 2770 | `	while( zIn < zEnd ){` |
|    72 | 2771 | `		iLen = PH7_UU_DEC(*zIn++);` |
|    72 | 2772 | `		if( iLen == 0 ){` |
|     - | 2773 | `			/* The end marker (or any line claiming zero bytes) stops the decoding. */` |
|    12 | 2774 | `			break;` |
|     - | 2775 | `		}` |
|    62 | 2776 | `		if( iLen > nLen ){` |
|     5 | 2777 | `			goto err;` |
|     - | 2778 | `		}` |
|    58 | 2779 | `		nTotal += (sxu32)iLen;` |
|     - | 2780 | `		/* A line carries four characters per three-byte group, whole groups only. */` |
|    58 | 2781 | `		zStop = zIn + ((iLen + 2)/3)*4;` |
|    58 | 2782 | `		if( zStop > zEnd ){` |
|     5 | 2783 | `			goto err;` |
|     - | 2784 | `		}` |
|   544 | 2785 | `		while( zIn < zStop ){` |
|   492 | 2786 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[0]) << 2) \| (PH7_UU_DEC(zIn[1]) >> 4));` |
|   492 | 2787 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[1]) << 4) \| (PH7_UU_DEC(zIn[2]) >> 2));` |
|   492 | 2788 | `			zOut[nOut++] = (unsigned char)((PH7_UU_DEC(zIn[2]) << 6) \|  PH7_UU_DEC(zIn[3]));` |
|   492 | 2789 | `			zIn += 4;` |
|     2 | 2790 | `		}` |
|    54 | 2791 | `		if( iLen < 45 ){` |
|     - | 2792 | `			/* A short line ends the payload; whatever follows is ignored. */` |
|    30 | 2793 | `			break;` |
|     - | 2794 | `		}` |
|    25 | 2795 | `		zIn++; /* Skip the line separator */` |
|     1 | 2796 | `	}` |
|     - | 2797 | `	/* Drop the padding the last group carried: php keeps only as many bytes as the` |
|     - | 2798 | `	 * length bytes declared, counted over the WHOLE input rather than per line. */` |
|    40 | 2799 | `	if( nOut > nTotal ){` |
|    20 | 2800 | `		nOut = nTotal;` |
|     9 | 2801 | `	}` |
|    40 | 2802 | `	ph7_result_string(pCtx,(const char *)zOut,(int)nOut);` |
|    40 | 2803 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|    40 | 2804 | `	return PH7_OK;` |
|     4 | 2805 | `err:` |
|     9 | 2806 | `	SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|     5 | 2807 | `fail:` |
|    11 | 2808 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 2809 | `		"Argument #1 ($data) is not a valid uuencoded string"); /* the "convert_uudecode(): " prefix is added by the handler */` |
|    11 | 2810 | `	ph7_result_bool(pCtx,0);` |
|    11 | 2811 | `	return PH7_OK;` |
|    26 | 2812 | `}` |
|     - | 2813 | `/*` |
|     - | 2814 | ` * string urlencode(string $str)` |
|     - | 2815 | ` *  URL encoding` |
|     - | 2816 | ` * Parameter` |
|     - | 2817 | ` *  $data` |
|     - | 2818 | ` *   Input string.` |
|     - | 2819 | ` * Return` |
|     - | 2820 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|     - | 2821 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|     - | 2822 | ` *  encoded as plus (+) signs.` |
|     - | 2823 | ` */` |
|    16 | 2824 | `PH7_PRIVATE int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2825 | `{` |
|     - | 2826 | `	const char *zIn;` |
|     - | 2827 | `	int nLen;` |
|    19 | 2828 | `	if( nArg < 1 ){` |
|     - | 2829 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2830 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2831 | `		return PH7_OK;` |
|     - | 2832 | `	}` |
|     - | 2833 | `	/* Extract the input string */` |
|    19 | 2834 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    19 | 2835 | `	if( nLen < 1 ){` |
|     - | 2836 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 2837 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2838 | `		return PH7_OK;` |
|     - | 2839 | `	}` |
|     - | 2840 | `	/* Perform the URL encoding */` |
|    14 | 2841 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    14 | 2842 | `	return PH7_OK;` |
|    11 | 2843 | `}` |
|     - | 2844 | `/*` |
|     - | 2845 | ` * string rawurlencode(string $str)` |
|     - | 2846 | ` *  RFC 3986 URL encoding: spaces become %20 (not '+') and '~' is left intact.` |
|     - | 2847 | ` */` |
|    14 | 2848 | `PH7_PRIVATE int PH7_builtin_rawurlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 2849 | `{` |
|     - | 2850 | `	const char *zIn;` |
|     - | 2851 | `	int nLen;` |
|    17 | 2852 | `	if( nArg < 1 ){` |
|     - | 2853 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 2854 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2855 | `		return PH7_OK;` |
|     - | 2856 | `	}` |
|     - | 2857 | `	/* Extract the input string */` |
|    17 | 2858 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|    17 | 2859 | `	if( nLen < 1 ){` |
|     - | 2860 | `		/* php returns an empty string for empty input, not FALSE */` |
|     6 | 2861 | `		ph7_result_string(pCtx,"",0);` |
|     6 | 2862 | `		return PH7_OK;` |
|     - | 2863 | `	}` |
|     - | 2864 | `	/* Perform the RFC 3986 URL encoding */` |
|    12 | 2865 | `	SyUriEncodeRaw(zIn,(sxu32)nLen,Consumer,pCtx);` |
|    12 | 2866 | `	return PH7_OK;` |
|    10 | 2867 | `}` |
|     - | 2868 | `/* SyUriEncode/SyUriDecode write through a consumer; both query-string builtins` |
|     - | 2869 | ` * below want the bytes in a blob. */` |
|  5130 | 2870 | `static int UriBlobConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|     2 | 2871 | `{` |
|  5132 | 2872 | `	return (int)SyBlobAppend((SyBlob *)pUserData,pData,(sxu32)nLen);` |
|     2 | 2873 | `}` |
|     - | 2874 | `/* --- parse_str (php's main/php_variables.c) ---------------------------- */` |
|     - | 2875 |  |
|     - | 2876 | `/*` |
|     - | 2877 | ` * php_register_variable_ex(): register ONE decoded "name[idx][idx]" against a` |
|     - | 2878 | ` * target array. The name arrives ALREADY url-decoded, which is the rule the` |
|     - | 2879 | ` * chunk did not have -- php decodes the whole key first and only then looks for` |
|     - | 2880 | ` * brackets, so "a%5Bb%5D=1" is the NESTED a[b], not a flat key spelled "a[b]".` |
|     - | 2881 | ` *` |
|     - | 2882 | ` * The walk is destructive on its own copy of the name (php writes NULs over the` |
|     - | 2883 | ` * brackets), so zVar must be a writable NUL-terminated buffer.` |
|     - | 2884 | ` */` |
|   346 | 2885 | `static ph7_hashmap * ParseStrDescend(ph7_context *pCtx,ph7_hashmap *pMap,` |
|     - | 2886 | `	const char *zKey,ph7_value *pKey)` |
|     1 | 2887 | `{` |
|   347 | 2888 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 2889 | `	ph7_value *pSlot,*pEmpty;` |
|   347 | 2890 | `	if( zKey ){` |
|   341 | 2891 | `		ph7_value_reset_string_cursor(pKey);` |
|   341 | 2892 | `		ph7_value_string(pKey,zKey,(int)SyStrlen(zKey));` |
|   341 | 2893 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) == SXRET_OK ){` |
|    27 | 2894 | `			pSlot = HashmapExtractNodeValue(pNode);` |
|    27 | 2895 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP) ){` |
|    23 | 2896 | `				return (ph7_hashmap *)pSlot->x.pOther;` |
|     - | 2897 | `			}` |
|     2 | 2898 | `		}` |
|   159 | 2899 | `	}` |
|     - | 2900 | `	/* Nothing usable there: php OVERWRITES whatever scalar is in the way with a` |
|     - | 2901 | `	 * fresh array ("a=1&a[b]=2" ends as a['b']). */` |
|   325 | 2902 | `	pEmpty = ph7_context_new_array(pCtx);` |
|   325 | 2903 | `	if( pEmpty == 0 \|\| PH7_HashmapInsert(pMap,zKey ? pKey : 0,pEmpty) != SXRET_OK ){` |
|   ! 0 | 2904 | `		return 0;` |
|     - | 2905 | `	}` |
|   325 | 2906 | `	if( zKey ){` |
|   319 | 2907 | `		if( PH7_HashmapLookup(pMap,pKey,&pNode) != SXRET_OK ){` |
|   ! 0 | 2908 | `			return 0;` |
|     - | 2909 | `		}` |
|   160 | 2910 | `	}else{` |
|     7 | 2911 | `		pNode = pMap->pLast;   /* the append just made */` |
|     - | 2912 | `	}` |
|   325 | 2913 | `	pSlot = pNode ? HashmapExtractNodeValue(pNode) : 0;` |
|   325 | 2914 | `	return (pSlot && (pSlot->iFlags & MEMOBJ_HASHMAP)) ? (ph7_hashmap *)pSlot->x.pOther : 0;` |
|   174 | 2915 | `}` |
|  2156 | 2916 | `static void ParseStrRegister(ph7_context *pCtx,ph7_value *pTarget,char *zVar,` |
|     - | 2917 | `	ph7_value *pVal,int nMaxNest)` |
|     2 | 2918 | `{` |
|  2158 | 2919 | `	ph7_hashmap *pCur = (ph7_hashmap *)pTarget->x.pOther;` |
|     - | 2920 | `	ph7_value *pIdxKey;` |
|  2158 | 2921 | `	char *p,*ip = 0,*index;` |
|  2158 | 2922 | `	int bIsArray = 0,nNest = 0;` |
|     - | 2923 | `	/* php ignores leading SPACES in the name outright -- they are not mangled to` |
|     - | 2924 | `	 * '_' the way an interior space is. */` |
|  2164 | 2925 | `	while( zVar[0] == ' ' ){` |
|     7 | 2926 | `		zVar++;` |
|     1 | 2927 | `	}` |
|     - | 2928 | `	/* Neither a space nor a dot may live in a php variable name; both become '_'.` |
|     - | 2929 | `	 * The scan stops at the first '[', so only the BASE name is mangled. */` |
| 10210 | 2930 | `	for( p = zVar ; p[0] ; p++ ){` |
|  8134 | 2931 | `		if( p[0] == ' ' \|\| p[0] == '.' ){` |
|    20 | 2932 | `			p[0] = '_';` |
|  8125 | 2933 | `		}else if( p[0] == '[' ){` |
|    81 | 2934 | `			bIsArray = 1;` |
|    81 | 2935 | `			ip = p;` |
|    81 | 2936 | `			p[0] = 0;` |
|    81 | 2937 | `			break;` |
|     - | 2938 | `		}` |
|  4028 | 2939 | `	}` |
|  2158 | 2940 | `	if( p == zVar ){` |
|     5 | 2941 | `		return; /* empty name (or a name that was nothing but a space) */` |
|     - | 2942 | `	}` |
|  2154 | 2943 | `	index = zVar;` |
|  2154 | 2944 | `	pIdxKey = ph7_context_new_scalar(pCtx);` |
|  2154 | 2945 | `	if( pIdxKey == 0 ){` |
|   ! 0 | 2946 | `		return;` |
|     - | 2947 | `	}` |
|  2428 | 2948 | `	while( bIsArray ){` |
|     - | 2949 | `		char *zSeg;` |
|     - | 2950 | `		ph7_hashmap *pNext;` |
|   353 | 2951 | `		if( ++nNest > nMaxNest ){` |
|     - | 2952 | `			/* php drops the whole top-level variable it was building and warns.` |
|     - | 2953 | `			 * The message is deliberately vague about the input -- php calls` |
|     - | 2954 | `			 * saying more "information disclosure". */` |
|     3 | 2955 | `			ph7_hashmap_node *pNode = 0;` |
|     3 | 2956 | `			ph7_hashmap *pRoot = (ph7_hashmap *)pTarget->x.pOther;` |
|     3 | 2957 | `			ph7_value_reset_string_cursor(pIdxKey);` |
|     3 | 2958 | `			ph7_value_string(pIdxKey,zVar,(int)SyStrlen(zVar));` |
|     3 | 2959 | `			if( PH7_HashmapLookup(pRoot,pIdxKey,&pNode) == SXRET_OK ){` |
|     3 | 2960 | `				PH7_HashmapUnlinkNode(pNode,TRUE);` |
|     1 | 2961 | `			}` |
|     4 | 2962 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 2963 | `				"Input variable nesting level exceeded %d. To increase the limit "` |
|     1 | 2964 | `				"change max_input_nesting_level in php.ini.",nMaxNest);` |
|     3 | 2965 | `			return;` |
|     - | 2966 | `		}` |
|   351 | 2967 | `		ip++;` |
|   351 | 2968 | `		zSeg = ip;` |
|   351 | 2969 | `		if( ip[0] == ' ' \|\| ip[0] == '\t' \|\| ip[0] == '\n' \|\| ip[0] == '\r' ){` |
|     5 | 2970 | `			ip++;   /* php skips ONE leading space before testing for ']' */` |
|     2 | 2971 | `		}` |
|   351 | 2972 | `		if( ip[0] == ']' ){` |
|    39 | 2973 | `			zSeg = 0;   /* "[]" (and "[ ]") appends */` |
|    20 | 2974 | `		}else{` |
|   651 | 2975 | `			while( ip[0] && ip[0] != ']' ){ ip++; }` |
|   313 | 2976 | `			if( ip[0] == 0 ){` |
|     - | 2977 | `				/* An unterminated '[': php un-terminates the name -- the bracket` |
|     - | 2978 | `				 * itself becomes '_' -- and the rest is mangled and used as a` |
|     - | 2979 | `				 * PLAIN key, so "a[b=1" registers "a_b". */` |
|     5 | 2980 | `				zSeg[-1] = '_';` |
|     7 | 2981 | `				for( p = zSeg ; p[0] ; p++ ){` |
|     3 | 2982 | `					if( p[0] == ' ' \|\| p[0] == '.' \|\| p[0] == '[' ){` |
|   ! 0 | 2983 | `						p[0] = '_';` |
|   ! 0 | 2984 | `					}` |
|     2 | 2985 | `				}` |
|     5 | 2986 | `				break;` |
|     - | 2987 | `			}` |
|   309 | 2988 | `			ip[0] = 0;` |
|     - | 2989 | `		}` |
|   347 | 2990 | `		pNext = ParseStrDescend(pCtx,pCur,index,pIdxKey);` |
|   347 | 2991 | `		if( pNext == 0 ){` |
|   ! 0 | 2992 | `			return;` |
|     - | 2993 | `		}` |
|   347 | 2994 | `		pCur = pNext;` |
|   347 | 2995 | `		index = zSeg;` |
|   347 | 2996 | `		ip++;` |
|   347 | 2997 | `		if( ip[0] == '[' ){` |
|   275 | 2998 | `			ip[0] = 0;   /* another level follows */` |
|   138 | 2999 | `		}else{` |
|    73 | 3000 | `			break;       /* whatever trails the last ']' is ignored */` |
|     - | 3001 | `		}` |
|     1 | 3002 | `	}` |
|  2152 | 3003 | `	if( index == 0 ){` |
|    33 | 3004 | `		PH7_HashmapInsert(pCur,0,pVal);` |
|    17 | 3005 | `	}else{` |
|  2120 | 3006 | `		ph7_value_reset_string_cursor(pIdxKey);` |
|  2120 | 3007 | `		ph7_value_string(pIdxKey,index,(int)SyStrlen(index));` |
|  2120 | 3008 | `		PH7_HashmapInsert(pCur,pIdxKey,pVal);` |
|     - | 3009 | `	}` |
|  1080 | 3010 | `}` |
|     - | 3011 | `/*` |
|     - | 3012 | ` * void parse_str(string $string, array &$result)` |
|     - | 3013 | ` *  Parse a query string into $result the way php's own GET/POST parser does.` |
|     - | 3014 | ` */` |
|   106 | 3015 | `PH7_PRIVATE int PH7_builtin_parse_str(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3016 | `{` |
|     - | 3017 | `	ph7_value *pArray,*pVal;` |
|     - | 3018 | `	SyBlob sSep,sName,sValue;` |
|     - | 3019 | `	const char *zIn,*zSep;` |
|     - | 3020 | `	int nByte,nSep;` |
|   108 | 3021 | `	sxu32 i = 0;` |
|   108 | 3022 | `	sxi64 nCount = 0,nMaxVars,nMaxNest;` |
|   108 | 3023 | `	if( nArg < 2 ){` |
|     - | 3024 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|   ! 0 | 3025 | `		return PH7_OK;` |
|     - | 3026 | `	}` |
|   108 | 3027 | `	pArray = ph7_context_new_array(pCtx);` |
|   108 | 3028 | `	pVal = ph7_context_new_scalar(pCtx);` |
|   108 | 3029 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 3030 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3031 | `	}` |
|   108 | 3032 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   108 | 3033 | `	nMaxVars = PH7_VmIniGetInt(pCtx->pVm,"max_input_vars",1000);` |
|   108 | 3034 | `	nMaxNest = PH7_VmIniGetInt(pCtx->pVm,"max_input_nesting_level",64);` |
|   108 | 3035 | `	SyBlobInit(&sSep,&pCtx->pVm->sAllocator);` |
|   108 | 3036 | `	SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|   108 | 3037 | `	SyBlobInit(&sValue,&pCtx->pVm->sAllocator);` |
|   108 | 3038 | `	PH7_VmIniGetStr(pCtx->pVm,"arg_separator.input",&sSep);` |
|   108 | 3039 | `	if( SyBlobLength(&sSep) < 1 ){` |
|   ! 0 | 3040 | `		SyBlobAppend(&sSep,"&",sizeof(char));` |
|   ! 0 | 3041 | `	}` |
|   108 | 3042 | `	zSep = (const char *)SyBlobData(&sSep);` |
|   108 | 3043 | `	nSep = (int)SyBlobLength(&sSep);` |
|     - | 3044 | `	/* php tokenizes with strtok(), so the separator is a SET of bytes and a run` |
|     - | 3045 | `	 * of them yields no empty field -- and an embedded NUL ends the input. */` |
|  2264 | 3046 | `	while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 3047 | `		sxu32 iStart,iEq;` |
|     - | 3048 | `		int bFound;` |
|  4224 | 3049 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 3050 | `			int s;` |
|  6380 | 3051 | `			for( s = 0 ; s < nSep ; ++s ){` |
|  4222 | 3052 | `				if( zIn[i] == zSep[s] ){ break; }` |
|  1081 | 3053 | `			}` |
|  4222 | 3054 | `			if( s == nSep ){ break; }` |
|  2064 | 3055 | `			i++;` |
|     2 | 3056 | `		}` |
|  2162 | 3057 | `		if( i >= (sxu32)nByte \|\| zIn[i] == 0 ){` |
|     2 | 3058 | `			break;` |
|     - | 3059 | `		}` |
|  2160 | 3060 | `		iStart = i;` |
|  2160 | 3061 | `		iEq = 0;` |
|  2160 | 3062 | `		bFound = 0;` |
| 19532 | 3063 | `		while( i < (sxu32)nByte && zIn[i] ){` |
|     - | 3064 | `			int s;` |
| 36806 | 3065 | `			for( s = 0 ; s < nSep ; ++s ){` |
| 19434 | 3066 | `				if( zIn[i] == zSep[s] ){ break; }` |
|  8688 | 3067 | `			}` |
| 19434 | 3068 | `			if( s < nSep ){ break; }` |
| 17374 | 3069 | `			if( zIn[i] == '=' && !bFound ){` |
|  2152 | 3070 | `				iEq = i;` |
|  2152 | 3071 | `				bFound = 1;` |
|  1075 | 3072 | `			}` |
| 17374 | 3073 | `			i++;` |
|     2 | 3074 | `		}` |
|  2160 | 3075 | `		if( ++nCount > nMaxVars ){` |
|     4 | 3076 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 3077 | `				"Input variables exceeded %qd. To increase the limit change "` |
|     1 | 3078 | `				"max_input_vars in php.ini.",nMaxVars);` |
|     3 | 3079 | `			break;` |
|     - | 3080 | `		}` |
|     - | 3081 | `		/* Both halves are url-decoded BEFORE the name is parsed for brackets. */` |
|  2158 | 3082 | `		SyBlobReset(&sName);` |
|  2158 | 3083 | `		SyBlobReset(&sValue);` |
|  2158 | 3084 | `		if( bFound ){` |
|  2150 | 3085 | `			if( iEq > iStart ){` |
|  2148 | 3086 | `				SyUriDecode(&zIn[iStart],iEq - iStart,UriBlobConsumer,&sName,TRUE);` |
|  1073 | 3087 | `			}` |
|  2150 | 3088 | `			if( i > iEq + 1 ){` |
|  2148 | 3089 | `				SyUriDecode(&zIn[iEq + 1],i - (iEq + 1),UriBlobConsumer,&sValue,TRUE);` |
|  1073 | 3090 | `			}` |
|  1076 | 3091 | `		}else{` |
|     9 | 3092 | `			SyUriDecode(&zIn[iStart],i - iStart,UriBlobConsumer,&sName,TRUE);` |
|     - | 3093 | `		}` |
|  2158 | 3094 | `		SyBlobAppend(&sName,"\0",sizeof(char));   /* the walk is C-string based */` |
|  2158 | 3095 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sValue),(int)SyBlobLength(&sValue));` |
|  2158 | 3096 | `		ParseStrRegister(pCtx,pArray,(char *)SyBlobData(&sName),pVal,(int)nMaxNest);` |
|  2158 | 3097 | `		ph7_value_reset_string_cursor(pVal);` |
|     2 | 3098 | `	}` |
|   108 | 3099 | `	SyBlobRelease(&sSep);` |
|   108 | 3100 | `	SyBlobRelease(&sName);` |
|   108 | 3101 | `	SyBlobRelease(&sValue);` |
|     - | 3102 | `	/* $result is by REFERENCE and php REPLACES it, empty array included. */` |
|   108 | 3103 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[1],pArray);` |
|   108 | 3104 | `	return PH7_OK;` |
|    55 | 3105 | `}` |
|     - | 3106 | `/* --- http_build_query (php's ext/standard/http.c) ---------------------- */` |
|     - | 3107 |  |
|     - | 3108 | `/*` |
|     - | 3109 | ` * The chain of hashmaps and instances the walk is currently INSIDE. This is` |
|     - | 3110 | ` * php's GC_TRY_PROTECT_RECURSION without a mark bit: a container that is its own` |
|     - | 3111 | `` * ancestor contributes nothing, so `$a['self'] = &$a` builds "a=1" rather than`` |
|     - | 3112 | ` * recursing forever. PHL had no guard here at all and ran the allocator out of` |
|     - | 3113 | ` * memory on exactly that input.` |
|     - | 3114 | ` */` |
|     - | 3115 | `typedef struct http_query_frame http_query_frame;` |
|     - | 3116 | `struct http_query_frame {` |
|     - | 3117 | `	const void *pWalked;                  /* the ph7_hashmap / ph7_class_instance */` |
|     - | 3118 | `	const http_query_frame *pParent;` |
|     - | 3119 | `};` |
|     - | 3120 | `typedef struct http_query_state http_query_state;` |
|     - | 3121 | `struct http_query_state {` |
|     - | 3122 | `	ph7_context *pCtx;` |
|     - | 3123 | `	SyBlob *pOut;      /* the form string built so far */` |
|     - | 3124 | `	const char *zSep;  /* argument separator */` |
|     - | 3125 | `	sxu32 nSep;` |
|     - | 3126 | `	int bRaw;          /* PHP_QUERY_RFC3986 rather than RFC1738 */` |
|     - | 3127 | `	int nDepth;` |
|     - | 3128 | `	int rc;            /* PH7_OK, or the status of a throw in flight */` |
|     - | 3129 | `};` |
|     - | 3130 | `/*` |
|     - | 3131 | ` * php has no fixed nesting limit here -- it asks the platform whether the C` |
|     - | 3132 | ` * stack is nearly gone and throws "Maximum call stack size reached." when it is.` |
|     - | 3133 | ` * PHL walks the same tree on the same C stack, so it needs a bound; this one is` |
|     - | 3134 | ` * far above any query string anyone builds and reports php's own error.` |
|     - | 3135 | ` */` |
|     - | 3136 | `#define HTTP_QUERY_MAX_DEPTH 512` |
|     - | 3137 |  |
|   696 | 3138 | `static int HttpQueryIsAncestor(const http_query_frame *pFrame,const void *pWalked)` |
|     1 | 3139 | `{` |
| 90431 | 3140 | `	while( pFrame ){` |
| 89739 | 3141 | `		if( pFrame->pWalked == pWalked ){` |
|     5 | 3142 | `			return 1;` |
|     - | 3143 | `		}` |
| 89735 | 3144 | `		pFrame = pFrame->pParent;` |
|     1 | 3145 | `	}` |
|   693 | 3146 | `	return 0;` |
|   349 | 3147 | `}` |
|   780 | 3148 | `static void HttpQueryEncodeTo(SyBlob *pOut,int bRaw,const char *zIn,sxu32 nByte)` |
|     1 | 3149 | `{` |
|   781 | 3150 | `	if( nByte < 1 ){` |
|   ! 0 | 3151 | `		return;` |
|     - | 3152 | `	}` |
|   781 | 3153 | `	if( bRaw ){` |
|     5 | 3154 | `		SyUriEncodeRaw(zIn,nByte,UriBlobConsumer,pOut);` |
|     3 | 3155 | `	}else{` |
|   777 | 3156 | `		SyUriEncode(zIn,nByte,UriBlobConsumer,pOut);` |
|     - | 3157 | `	}` |
|   391 | 3158 | `}` |
|     - | 3159 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|     - | 3160 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 3161 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 3162 | `	const http_query_frame *pParent);` |
|     - | 3163 |  |
|     - | 3164 | `/*` |
|     - | 3165 | ` * php_url_encode_scalar(): one "<key_prefix><key>[%5D]=<value>" leaf, preceded` |
|     - | 3166 | ` * by the separator once anything has been written.` |
|     - | 3167 | ` */` |
|   106 | 3168 | `static void HttpQueryScalar(http_query_state *p,` |
|     - | 3169 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 3170 | `	ph7_value *pVal,` |
|     - | 3171 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 3172 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|     1 | 3173 | `{` |
|   107 | 3174 | `	if( SyBlobLength(p->pOut) > 0 ){` |
|    49 | 3175 | `		SyBlobAppend(p->pOut,p->zSep,p->nSep);` |
|    24 | 3176 | `	}` |
|   107 | 3177 | `	if( nKeyPrefix > 0 ){` |
|    45 | 3178 | `		SyBlobAppend(p->pOut,zKeyPrefix,nKeyPrefix);` |
|    22 | 3179 | `	}` |
|   107 | 3180 | `	if( bIntKey ){` |
|     - | 3181 | `		/* The numeric prefix is appended RAW -- php never url-encodes it, which` |
|     - | 3182 | `		 * is why http_build_query([1,2], "a b") answers "a b0=1&a b1=2". The` |
|     - | 3183 | `		 * chunk encoded it and answered "a+b0=1". */` |
|    53 | 3184 | `		if( nNumPrefix > 0 ){` |
|    17 | 3185 | `			SyBlobAppend(p->pOut,zNumPrefix,nNumPrefix);` |
|     8 | 3186 | `		}` |
|    53 | 3187 | `		SyBlobFormat(p->pOut,"%qd",iKey);` |
|    27 | 3188 | `	}else{` |
|    55 | 3189 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zKey,nKey);` |
|     - | 3190 | `	}` |
|   107 | 3191 | `	if( nKeyPrefix > 0 ){` |
|    45 | 3192 | `		SyBlobAppend(p->pOut,"%5D",sizeof("%5D")-1);` |
|    22 | 3193 | `	}` |
|   107 | 3194 | `	SyBlobAppend(p->pOut,"=",sizeof(char));` |
|   107 | 3195 | `	if( ph7_value_is_bool(pVal) ){` |
|     - | 3196 | `		/* php writes the digit itself: to_string() would give "" for false. */` |
|     5 | 3197 | `		SyBlobAppend(p->pOut,ph7_value_to_bool(pVal) ? "1" : "0",sizeof(char));` |
|     3 | 3198 | `	}else{` |
|     - | 3199 | `		int nVal;` |
|   103 | 3200 | `		const char *zVal = ph7_value_to_string(pVal,&nVal);` |
|   103 | 3201 | `		HttpQueryEncodeTo(p->pOut,p->bRaw,zVal,(sxu32)nVal);` |
|     - | 3202 | `	}` |
|   107 | 3203 | `}` |
|     - | 3204 | `/*` |
|     - | 3205 | ` * Build the key prefix a nested container's members carry: php closes the` |
|     - | 3206 | ` * PREVIOUS bracket and opens the next one in the same step, so a second level` |
|     - | 3207 | ` * appends "%5D%5B" where the first opened with "%5B".` |
|     - | 3208 | ` */` |
|   634 | 3209 | `static void HttpQueryNestPrefix(http_query_state *p,SyBlob *pPrefix,` |
|     - | 3210 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 3211 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 3212 | `	const char *zKeyPrefix,sxu32 nKeyPrefix)` |
|     1 | 3213 | `{` |
|   635 | 3214 | `	if( nKeyPrefix > 0 ){` |
|   601 | 3215 | `		SyBlobAppend(pPrefix,zKeyPrefix,nKeyPrefix);` |
|   335 | 3216 | `	}else if( bIntKey && nNumPrefix > 0 ){` |
|     9 | 3217 | `		SyBlobAppend(pPrefix,zNumPrefix,nNumPrefix);` |
|     4 | 3218 | `	}` |
|   635 | 3219 | `	if( bIntKey ){` |
|    11 | 3220 | `		SyBlobFormat(pPrefix,"%qd",iKey);` |
|     6 | 3221 | `	}else{` |
|   625 | 3222 | `		HttpQueryEncodeTo(pPrefix,p->bRaw,zKey,nKey);` |
|     - | 3223 | `	}` |
|   952 | 3224 | `	SyBlobAppend(pPrefix,nKeyPrefix > 0 ? "%5D%5B" : "%5B",` |
|   317 | 3225 | `		nKeyPrefix > 0 ? sizeof("%5D%5B")-1 : sizeof("%5B")-1);` |
|   635 | 3226 | `}` |
|     - | 3227 | `/*` |
|     - | 3228 | ` * One (key, value) pair, whichever container it came from. php skips NULL and` |
|     - | 3229 | ` * RESOURCE outright, descends into an array or a non-enum object, and treats` |
|     - | 3230 | ` * everything else -- a backed enum case included -- as a scalar.` |
|     - | 3231 | ` */` |
|   748 | 3232 | `static void HttpQueryPair(http_query_state *p,` |
|     - | 3233 | `	int bIntKey,sxi64 iKey,const char *zKey,sxu32 nKey,` |
|     - | 3234 | `	ph7_value *pVal,` |
|     - | 3235 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 3236 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 3237 | `	const http_query_frame *pParent)` |
|     1 | 3238 | `{` |
|     - | 3239 | `	int bDescend;` |
|   749 | 3240 | `	if( p->rc != PH7_OK ){` |
|   ! 0 | 3241 | `		return;` |
|     - | 3242 | `	}` |
|   749 | 3243 | `	if( ph7_value_is_null(pVal) \|\| ph7_value_is_resource(pVal) ){` |
|     7 | 3244 | `		return;` |
|     - | 3245 | `	}` |
|   743 | 3246 | `	bDescend = ph7_value_is_array(pVal);` |
|   743 | 3247 | `	if( ph7_value_is_object(pVal) ){` |
|    11 | 3248 | `		ph7_class_instance *pInst = (ph7_class_instance *)pVal->x.pOther;` |
|    11 | 3249 | `		if( (pInst->pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     5 | 3250 | `			bDescend = 1;` |
|     3 | 3251 | `		}else{` |
|     - | 3252 | `			/* php compares an enum case by its BACKING value here; the chunk` |
|     - | 3253 | `			 * descended into it and emitted its name/value properties. */` |
|     7 | 3254 | `			ph7_value *pBacking = PH7_EnumCaseBackingValueOf(pInst);` |
|     7 | 3255 | `			if( pBacking == 0 ){` |
|     5 | 3256 | `				p->rc = PH7_VmThrowException(p->pCtx,"ValueError",` |
|     - | 3257 | `					"Unbacked enum %z cannot be converted to a string",` |
|     2 | 3258 | `					&pInst->pClass->sName);` |
|     3 | 3259 | `				return;` |
|     - | 3260 | `			}` |
|     7 | 3261 | `			HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pBacking,` |
|     2 | 3262 | `				zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|     5 | 3263 | `			return;` |
|     - | 3264 | `		}` |
|     2 | 3265 | `	}` |
|   737 | 3266 | `	if( bDescend ){` |
|     - | 3267 | `		SyBlob sPrefix;` |
|   635 | 3268 | `		SyBlobInit(&sPrefix,&p->pCtx->pVm->sAllocator);` |
|   952 | 3269 | `		HttpQueryNestPrefix(p,&sPrefix,bIntKey,iKey,zKey,nKey,` |
|   317 | 3270 | `			zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|     - | 3271 | `		/* php passes no numeric prefix down: it only ever prefixes a TOP-LEVEL` |
|     - | 3272 | `		 * integer key. */` |
|   952 | 3273 | `		HttpQueryWalk(p,pVal,0,0,` |
|   634 | 3274 | `			(const char *)SyBlobData(&sPrefix),SyBlobLength(&sPrefix),pParent);` |
|   635 | 3275 | `		SyBlobRelease(&sPrefix);` |
|   635 | 3276 | `		return;` |
|     - | 3277 | `	}` |
|   154 | 3278 | `	HttpQueryScalar(p,bIntKey,iKey,zKey,nKey,pVal,` |
|    51 | 3279 | `		zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix);` |
|   375 | 3280 | `}` |
|     - | 3281 | `/* Every visible, non-static, materialized property of an instance, in` |
|     - | 3282 | ` * declaration order. php asks the CALLER's scope, so http_build_query($this)` |
|     - | 3283 | ` * from inside the class sees its private members -- the chunk reached them` |
|     - | 3284 | ` * through a global-scope get_object_vars() and never did. */` |
|     8 | 3285 | `static void HttpQueryWalkObject(http_query_state *p,ph7_class_instance *pThis,` |
|     - | 3286 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,const http_query_frame *pFrame)` |
|     1 | 3287 | `{` |
|     - | 3288 | `	SyHashEntry *pEntry;` |
|     - | 3289 | `	ph7_value sValue;` |
|     9 | 3290 | `	PH7_MemObjInit(pThis->pVm,&sValue);` |
|     9 | 3291 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|    39 | 3292 | `	while( p->rc == PH7_OK && (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|    31 | 3293 | `		VmClassAttr *pAttr = (VmClassAttr *)pEntry->pUserData;` |
|    31 | 3294 | `		SyString *pName = &pAttr->pAttr->sName;` |
|     - | 3295 | `		ph7_value *pValue;` |
|    31 | 3296 | `		if( pAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_HIDDEN) ){` |
|   ! 0 | 3297 | `			continue;` |
|     - | 3298 | `		}` |
|    31 | 3299 | `		if( pAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - | 3300 | `			/* A virtual hooked property has no backing store, and php reads the` |
|     - | 3301 | `			 * raw property table here rather than dispatching the get hook. */` |
|     5 | 3302 | `			continue;` |
|     - | 3303 | `		}` |
|    40 | 3304 | `		if( !PH7_VmClassMemberAccess(pThis->pVm,pThis->pClass,pName,` |
|    26 | 3305 | `				pAttr->pAttr->iProtection,FALSE) ){` |
|     7 | 3306 | `			continue;` |
|     - | 3307 | `		}` |
|    21 | 3308 | `		pValue = PH7_ClassInstanceExtractAttrValue(pThis,pAttr);` |
|    21 | 3309 | `		if( pValue == 0 ){` |
|   ! 0 | 3310 | `			continue;` |
|     - | 3311 | `		}` |
|    21 | 3312 | `		PH7_MemObjLoad(pValue,&sValue);` |
|    31 | 3313 | `		HttpQueryPair(p,0,0,SyStringData(pName),SyStringLength(pName),&sValue,` |
|    10 | 3314 | `			0,0,zKeyPrefix,nKeyPrefix,pFrame);` |
|    21 | 3315 | `		PH7_MemObjRelease(&sValue);` |
|     1 | 3316 | `	}` |
|     9 | 3317 | `	PH7_MemObjRelease(&sValue);` |
|     9 | 3318 | `}` |
|     - | 3319 | `/* php_url_encode_hash_ex() over one array or object. */` |
|   696 | 3320 | `static int HttpQueryWalk(http_query_state *p,ph7_value *pData,` |
|     - | 3321 | `	const char *zNumPrefix,sxu32 nNumPrefix,` |
|     - | 3322 | `	const char *zKeyPrefix,sxu32 nKeyPrefix,` |
|     - | 3323 | `	const http_query_frame *pParent)` |
|     1 | 3324 | `{` |
|     - | 3325 | `	http_query_frame sFrame;` |
|   697 | 3326 | `	const void *pWalked = pData->x.pOther;` |
|   697 | 3327 | `	if( HttpQueryIsAncestor(pParent,pWalked) ){` |
|     5 | 3328 | `		return PH7_OK;` |
|     - | 3329 | `	}` |
|   693 | 3330 | `	if( p->nDepth >= HTTP_QUERY_MAX_DEPTH ){` |
|   ! 0 | 3331 | `		p->rc = PH7_VmThrowException(p->pCtx,"Error","Maximum call stack size reached.");` |
|   ! 0 | 3332 | `		return p->rc;` |
|     - | 3333 | `	}` |
|   693 | 3334 | `	sFrame.pWalked = pWalked;` |
|   693 | 3335 | `	sFrame.pParent = pParent;` |
|   693 | 3336 | `	p->nDepth++;` |
|   693 | 3337 | `	if( ph7_value_is_object(pData) ){` |
|     9 | 3338 | `		HttpQueryWalkObject(p,(ph7_class_instance *)pWalked,zKeyPrefix,nKeyPrefix,&sFrame);` |
|     5 | 3339 | `	}else{` |
|   685 | 3340 | `		ph7_hashmap *pMap = (ph7_hashmap *)pWalked;` |
|   685 | 3341 | `		ph7_hashmap_node *pNode = pMap->pFirst;` |
|     - | 3342 | `		ph7_value sValue;` |
|   685 | 3343 | `		sxu32 n = pMap->nEntry;` |
|   685 | 3344 | `		PH7_MemObjInit(pMap->pVm,&sValue);` |
|     - | 3345 | `		/* Insertion order runs pFirst then the pPrev chain (MACRO_LD_PUSH links` |
|     - | 3346 | `		 * a new node in through pNext, so pNext is the OLDER neighbour). */` |
|  1413 | 3347 | `		while( n > 0 && p->rc == PH7_OK ){` |
|   729 | 3348 | `			int bIntKey = (pNode->iType == HASHMAP_INT_NODE);` |
|   729 | 3349 | `			PH7_HashmapExtractNodeValue(pNode,&sValue,FALSE);` |
|  1093 | 3350 | `			HttpQueryPair(p,bIntKey,bIntKey ? pNode->xKey.iKey : 0,` |
|   364 | 3351 | `				bIntKey ? 0 : (const char *)SyBlobData(&pNode->xKey.sKey),` |
|   364 | 3352 | `				bIntKey ? 0 : SyBlobLength(&pNode->xKey.sKey),` |
|   364 | 3353 | `				&sValue,zNumPrefix,nNumPrefix,zKeyPrefix,nKeyPrefix,&sFrame);` |
|   729 | 3354 | `			PH7_MemObjRelease(&sValue);` |
|   729 | 3355 | `			pNode = pNode->pPrev;` |
|   729 | 3356 | `			n--;` |
|     1 | 3357 | `		}` |
|   685 | 3358 | `		PH7_MemObjRelease(&sValue);` |
|     - | 3359 | `	}` |
|   693 | 3360 | `	p->nDepth--;` |
|   693 | 3361 | `	return p->rc;` |
|   349 | 3362 | `}` |
|     - | 3363 | `/*` |
|     - | 3364 | ` * string http_build_query(object\|array $data, string $numeric_prefix = "",` |
|     - | 3365 | ` *                         ?string $arg_separator = null,` |
|     - | 3366 | ` *                         int $encoding_type = PHP_QUERY_RFC1738)` |
|     - | 3367 | ` *  Generate a URL-encoded query string from an array or an object.` |
|     - | 3368 | ` */` |
|    76 | 3369 | `PH7_PRIVATE int PH7_builtin_http_build_query(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 3370 | `{` |
|     - | 3371 | `	http_query_state sState;` |
|     - | 3372 | `	SyBlob sOut;` |
|     - | 3373 | `	char zName[64];` |
|    78 | 3374 | `	const char *zNumPrefix = 0,*zSep = "&";` |
|    78 | 3375 | `	int nNumPrefix = 0,nSep = 1;` |
|    78 | 3376 | `	if( nArg < 1 ){` |
|     - | 3377 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|   ! 0 | 3378 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 | 3379 | `		return PH7_OK;` |
|     - | 3380 | `	}` |
|     - | 3381 | ``	/* php DECLARES `object\|array $data` and REPORTS "must be of type array" --`` |
|     - | 3382 | ``	 * the shared ZPP screen leaves a union arm holding `array` alone for exactly`` |
|     - | 3383 | `	 * this reason, so the wording is the builtin's own. */` |
|    78 | 3384 | `	if( !ph7_value_is_array(apArg[0]) && !ph7_value_is_object(apArg[0]) ){` |
|    16 | 3385 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 3386 | `			"http_build_query(): Argument #1 ($data) must be of type array, %s given",` |
|     5 | 3387 | `			VmValueGivenName(apArg[0],zName,sizeof(zName)));` |
|     - | 3388 | `	}` |
|    67 | 3389 | `	if( ph7_value_is_object(apArg[0]) ){` |
|    11 | 3390 | `		ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    11 | 3391 | `		if( pInst->pClass->iFlags & PH7_CLASS_ENUM ){` |
|     7 | 3392 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - | 3393 | `				"http_build_query(): Argument #1 ($data) must not be an enum, %z given",` |
|     4 | 3394 | `				&pInst->pClass->sName);` |
|     - | 3395 | `		}` |
|     3 | 3396 | `	}` |
|    63 | 3397 | `	if( nArg > 1 ){` |
|    33 | 3398 | `		zNumPrefix = ph7_value_to_string(apArg[1],&nNumPrefix);` |
|    16 | 3399 | `	}` |
|    63 | 3400 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     3 | 3401 | `		zSep = ph7_value_to_string(apArg[2],&nSep);` |
|     1 | 3402 | `	}` |
|    63 | 3403 | `	sState.pCtx = pCtx;` |
|    63 | 3404 | `	sState.zSep = zSep;` |
|    63 | 3405 | `	sState.nSep = (sxu32)nSep;` |
|    63 | 3406 | `	sState.bRaw = (nArg > 3) && (ph7_value_to_int(apArg[3]) == 2 /* PHP_QUERY_RFC3986 */);` |
|    63 | 3407 | `	sState.nDepth = 0;` |
|    63 | 3408 | `	sState.rc = PH7_OK;` |
|    63 | 3409 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    63 | 3410 | `	sState.pOut = &sOut;` |
|    63 | 3411 | `	HttpQueryWalk(&sState,apArg[0],zNumPrefix,(sxu32)nNumPrefix,0,0,0);` |
|    63 | 3412 | `	if( sState.rc == PH7_OK ){` |
|    61 | 3413 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    30 | 3414 | `	}` |
|    63 | 3415 | `	SyBlobRelease(&sOut);` |
|    63 | 3416 | `	return sState.rc;` |
|    40 | 3417 | `}` |
|     - | 3418 | `/*` |
|     - | 3419 | ` * string urldecode(string $str)` |
|     - | 3420 | ` *  Decodes any %## encoding in the given string.` |
|     - | 3421 | ` *  Plus symbols ('+') are decoded to a space character.` |
|     - | 3422 | ` * string rawurldecode(string $str)` |
|     - | 3423 | ` *  The same, except that '+' is NOT a space: RFC 3986 has no plus convention, so` |
|     - | 3424 | ` *  php leaves it alone. rawurldecode() used to be registered as an ALIAS of` |
|     - | 3425 | ` *  urldecode(), which turned every literal '+' into a space.` |
|     - | 3426 | ` * Parameter` |
|     - | 3427 | ` *  $data` |
|     - | 3428 | ` *    Input string.` |
|     - | 3429 | ` * Return` |
|     - | 3430 | ` *  Decoded URL or FALSE on failure.` |
|     - | 3431 | ` */` |
|   120 | 3432 | `static int UrlDecodeCommon(ph7_context *pCtx,int nArg,ph7_value **apArg,int bPlus)` |
|     3 | 3433 | `{` |
|     - | 3434 | `	const char *zIn;` |
|     - | 3435 | `	int nLen;` |
|   123 | 3436 | `	if( nArg < 1 ){` |
|     - | 3437 | `		/* Missing arguments,return FALSE */` |
|   ! 0 | 3438 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 3439 | `		return PH7_OK;` |
|     - | 3440 | `	}` |
|     - | 3441 | `	/* Extract the input string */` |
|   123 | 3442 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|   123 | 3443 | `	if( nLen < 1 ){` |
|     - | 3444 | `		/* php returns an empty string for empty input, not FALSE */` |
|    13 | 3445 | `		ph7_result_string(pCtx,"",0);` |
|    13 | 3446 | `		return PH7_OK;` |
|     - | 3447 | `	}` |
|     - | 3448 | `	/* Perform the URL decoding */` |
|   112 | 3449 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,bPlus);` |
|   112 | 3450 | `	return PH7_OK;` |
|    63 | 3451 | `}` |
|    64 | 3452 | `PH7_PRIVATE int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 3453 | `{` |
|    67 | 3454 | `	return UrlDecodeCommon(pCtx,nArg,apArg,TRUE);` |
|     3 | 3455 | `}` |
|    56 | 3456 | `PH7_PRIVATE int PH7_builtin_rawurldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 3457 | `{` |
|    59 | 3458 | `	return UrlDecodeCommon(pCtx,nArg,apArg,FALSE);` |
|     3 | 3459 | `}` |
|     - | 3460 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|     - | 3461 |  |
